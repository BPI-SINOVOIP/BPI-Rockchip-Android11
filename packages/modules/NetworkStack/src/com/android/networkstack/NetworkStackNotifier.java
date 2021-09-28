/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.networkstack;

import static android.app.NotificationManager.IMPORTANCE_NONE;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Handler;
import android.os.Looper;
import android.os.UserHandle;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import com.android.networkstack.apishim.NetworkInformationShimImpl;
import com.android.networkstack.apishim.common.CaptivePortalDataShim;
import com.android.networkstack.apishim.common.NetworkInformationShim;

import java.util.Hashtable;
import java.util.function.Consumer;

/**
 * Displays notification related to connected networks.
 */
public class NetworkStackNotifier {
    private final Context mContext;
    private final Handler mHandler;
    private final NotificationManager mNotificationManager;
    private final Dependencies mDependencies;

    @NonNull
    private final Hashtable<Network, TrackedNetworkStatus> mNetworkStatus = new Hashtable<>();
    @Nullable
    private Network mDefaultNetwork;
    @NonNull
    private final NetworkInformationShim mInfoShim = NetworkInformationShimImpl.newInstance();

    /**
     * The TrackedNetworkStatus object is a data class that keeps track of the relevant state of the
     * various networks on the device. For efficiency the members are mutable, which means any
     * instance of this object should only ever be accessed on the looper thread passed in the
     * constructor. Any access (read or write) from any other thread would be incorrect.
     */
    private static class TrackedNetworkStatus {
        private boolean mValidatedNotificationPending;
        private int mShownNotification = NOTE_NONE;
        private LinkProperties mLinkProperties;
        private NetworkCapabilities mNetworkCapabilities;

        private boolean isValidated() {
            if (mNetworkCapabilities == null) return false;
            return mNetworkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
        }
    }

    @VisibleForTesting
    protected static final String CHANNEL_CONNECTED = "connected_note_loud";
    @VisibleForTesting
    protected static final String CHANNEL_VENUE_INFO = "connected_note";

    private static final int NOTE_NONE = 0;
    private static final int NOTE_CONNECTED = 1;
    private static final int NOTE_VENUE_INFO = 2;

    private static final int NOTE_ID_NETWORK_INFO = 1;

    @VisibleForTesting
    protected static final long CONNECTED_NOTIFICATION_TIMEOUT_MS = 20_000L;

    protected static class Dependencies {
        public PendingIntent getActivityPendingIntent(Context context, Intent intent, int flags) {
            return PendingIntent.getActivity(context, 0 /* requestCode */, intent, flags);
        }
    }

    public NetworkStackNotifier(@NonNull Context context, @NonNull Looper looper) {
        this(context, looper, new Dependencies());
    }

    protected NetworkStackNotifier(@NonNull Context context, @NonNull Looper looper,
            @NonNull Dependencies dependencies) {
        mContext = context;
        mHandler = new Handler(looper);
        mDependencies = dependencies;
        mNotificationManager = getContextAsUser(mContext, UserHandle.ALL)
                .getSystemService(NotificationManager.class);
        final ConnectivityManager cm = context.getSystemService(ConnectivityManager.class);
        cm.registerDefaultNetworkCallback(new DefaultNetworkCallback(), mHandler);
        cm.registerNetworkCallback(
                new NetworkRequest.Builder()
                        .addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(),
                new AllNetworksCallback(),
                mHandler);

        createNotificationChannel(CHANNEL_CONNECTED,
                R.string.notification_channel_name_connected,
                R.string.notification_channel_description_connected,
                NotificationManager.IMPORTANCE_HIGH);
        createNotificationChannel(CHANNEL_VENUE_INFO,
                R.string.notification_channel_name_network_venue_info,
                R.string.notification_channel_description_network_venue_info,
                NotificationManager.IMPORTANCE_DEFAULT);
    }

    @VisibleForTesting
    protected Handler getHandler() {
        return mHandler;
    }

    private void createNotificationChannel(@NonNull String id, @StringRes int title,
            @StringRes int description, int importance) {
        final Resources resources = mContext.getResources();
        NotificationChannel channel = new NotificationChannel(id,
                resources.getString(title),
                importance);
        channel.setDescription(resources.getString(description));
        getNotificationManagerForChannels().createNotificationChannel(channel);
    }

    /**
     * Get the NotificationManager to use to query channels, as opposed to posting notifications.
     *
     * Although notifications are posted as USER_ALL, notification channels are always created
     * based on the UID calling NotificationManager, regardless of the context UserHandle.
     * When querying notification channels, using a USER_ALL context would return no channel: the
     * default context (as UserHandle 0 for NetworkStack) must be used.
     */
    private NotificationManager getNotificationManagerForChannels() {
        return mContext.getSystemService(NotificationManager.class);
    }

    /**
     * Notify the NetworkStackNotifier that the captive portal app was opened to show a login UI to
     * the user, but the network has not validated yet. The notifier uses this information to show
     * proper notifications once the network validates.
     */
    public void notifyCaptivePortalValidationPending(@NonNull Network network) {
        mHandler.post(() -> setCaptivePortalValidationPending(network));
    }

    private void setCaptivePortalValidationPending(@NonNull Network network) {
        updateNetworkStatus(network, status -> {
            status.mValidatedNotificationPending = true;
            status.mShownNotification = NOTE_NONE;
        });
    }

    @Nullable
    private CaptivePortalDataShim getCaptivePortalData(@NonNull TrackedNetworkStatus status) {
        return mInfoShim.getCaptivePortalData(status.mLinkProperties);
    }

    private String getSsid(@NonNull TrackedNetworkStatus status) {
        return mInfoShim.getSsid(status.mNetworkCapabilities);
    }

    private void updateNetworkStatus(@NonNull Network network,
            @NonNull Consumer<TrackedNetworkStatus> mutator) {
        final TrackedNetworkStatus status =
                mNetworkStatus.computeIfAbsent(network, n -> new TrackedNetworkStatus());
        mutator.accept(status);
    }

    private void updateNotifications(@NonNull Network network) {
        final TrackedNetworkStatus networkStatus = mNetworkStatus.get(network);
        // The required network attributes callbacks were not fired yet for this network
        if (networkStatus == null) return;
        // Don't show the notification when SSID is unknown to prevent sending something vague to
        // the user.
        final boolean hasSsid = !TextUtils.isEmpty(getSsid(networkStatus));

        final CaptivePortalDataShim capportData = getCaptivePortalData(networkStatus);
        final boolean showVenueInfo = capportData != null && capportData.getVenueInfoUrl() != null
                // Only show venue info on validated networks, to prevent misuse of the notification
                // as an alternate login flow that uses the default browser (which would be broken
                // if the device has mobile data).
                && networkStatus.isValidated()
                && isVenueInfoNotificationEnabled()
                // Most browsers do not yet support opening a page on a non-default network, so the
                // venue info link should not be shown if the network is not the default one.
                && network.equals(mDefaultNetwork)
                && hasSsid;
        final boolean showValidated =
                networkStatus.mValidatedNotificationPending && networkStatus.isValidated()
                && hasSsid;
        final String notificationTag = getNotificationTag(network);

        final Resources res = mContext.getResources();
        final Notification.Builder builder;
        if (showVenueInfo) {
            // Do not re-show the venue info notification even if the previous one had a different
            // URL, to avoid potential abuse where APs could spam the notification with different
            // URLs.
            if (networkStatus.mShownNotification == NOTE_VENUE_INFO) return;

            final Intent infoIntent = new Intent(Intent.ACTION_VIEW)
                    .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .setData(capportData.getVenueInfoUrl())
                    .putExtra(ConnectivityManager.EXTRA_NETWORK, network)
                    // Use the network handle as identifier, as there should be only one ACTION_VIEW
                    // pending intent per network.
                    .setIdentifier(Long.toString(network.getNetworkHandle()));

            // If the validated notification should be shown, use the high priority "connected"
            // channel even if the notification contains venue info: the "venue info" notification
            // then doubles as a "connected" notification.
            final String channel = showValidated ? CHANNEL_CONNECTED : CHANNEL_VENUE_INFO;
            builder = getNotificationBuilder(channel, networkStatus, res, getSsid(networkStatus))
                    .setContentText(res.getString(R.string.tap_for_info))
                    .setContentIntent(mDependencies.getActivityPendingIntent(
                            getContextAsUser(mContext, UserHandle.CURRENT),
                            infoIntent, PendingIntent.FLAG_IMMUTABLE));

            networkStatus.mShownNotification = NOTE_VENUE_INFO;
        } else if (showValidated) {
            if (networkStatus.mShownNotification == NOTE_CONNECTED) return;

            builder = getNotificationBuilder(CHANNEL_CONNECTED, networkStatus, res,
                    getSsid(networkStatus))
                    .setTimeoutAfter(CONNECTED_NOTIFICATION_TIMEOUT_MS)
                    .setContentText(res.getString(R.string.connected))
                    .setContentIntent(mDependencies.getActivityPendingIntent(
                            getContextAsUser(mContext, UserHandle.CURRENT),
                            new Intent(Settings.ACTION_WIFI_SETTINGS),
                            PendingIntent.FLAG_IMMUTABLE));

            networkStatus.mShownNotification = NOTE_CONNECTED;
        } else {
            if (networkStatus.mShownNotification != NOTE_NONE
                    // Don't dismiss the connected notification: it's generated as one-off and will
                    // be dismissed after a timeout or if the network disconnects.
                    && networkStatus.mShownNotification != NOTE_CONNECTED) {
                dismissNotification(notificationTag, networkStatus);
            }
            return;
        }

        if (showValidated) {
            networkStatus.mValidatedNotificationPending = false;
        }
        mNotificationManager.notify(notificationTag, NOTE_ID_NETWORK_INFO, builder.build());
    }

    private void dismissNotification(@NonNull String tag, @NonNull TrackedNetworkStatus status) {
        mNotificationManager.cancel(tag, NOTE_ID_NETWORK_INFO);
        status.mShownNotification = NOTE_NONE;
    }

    private Notification.Builder getNotificationBuilder(@NonNull String channelId,
            @NonNull TrackedNetworkStatus networkStatus, @NonNull Resources res,
            @NonNull String ssid) {
        return new Notification.Builder(mContext, channelId)
                .setContentTitle(ssid)
                .setSmallIcon(R.drawable.icon_wifi);
    }

    /**
     * Replacement for {@link Context#createContextAsUser(UserHandle, int)}, which is not available
     * in API 29.
     */
    private static Context getContextAsUser(Context baseContext, UserHandle user) {
        try {
            return baseContext.createPackageContextAsUser(
                    baseContext.getPackageName(), 0 /* flags */, user);
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException("NetworkStack own package not found", e);
        }
    }

    private boolean isVenueInfoNotificationEnabled() {
        final NotificationChannel channel = getNotificationManagerForChannels()
                .getNotificationChannel(CHANNEL_VENUE_INFO);
        if (channel == null) return false;

        return channel.getImportance() != IMPORTANCE_NONE;
    }

    private static String getNotificationTag(@NonNull Network network) {
        return Long.toString(network.getNetworkHandle());
    }

    private class DefaultNetworkCallback extends ConnectivityManager.NetworkCallback {
        @Override
        public void onAvailable(Network network) {
            updateDefaultNetwork(network);
        }

        @Override
        public void onLost(Network network) {
            updateDefaultNetwork(null);
        }

        private void updateDefaultNetwork(@Nullable Network newNetwork) {
            final Network oldDefault = mDefaultNetwork;
            mDefaultNetwork = newNetwork;
            if (oldDefault != null) updateNotifications(oldDefault);
            if (newNetwork != null) updateNotifications(newNetwork);
        }
    }

    private class AllNetworksCallback extends ConnectivityManager.NetworkCallback {
        @Override
        public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {
            updateNetworkStatus(network, status -> status.mLinkProperties = linkProperties);
            updateNotifications(network);
        }

        @Override
        public void onCapabilitiesChanged(@NonNull Network network,
                @NonNull NetworkCapabilities networkCapabilities) {
            updateNetworkStatus(network, s -> s.mNetworkCapabilities = networkCapabilities);
            updateNotifications(network);
        }

        @Override
        public void onLost(Network network) {
            final TrackedNetworkStatus status = mNetworkStatus.remove(network);
            if (status == null) return;
            dismissNotification(getNotificationTag(network), status);
        }
    }
}
