/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.ext.services.notification;

import static android.app.Notification.CATEGORY_MESSAGE;
import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.app.NotificationManager.IMPORTANCE_HIGH;
import static android.app.NotificationManager.IMPORTANCE_LOW;
import static android.app.NotificationManager.IMPORTANCE_MIN;
import static android.app.NotificationManager.IMPORTANCE_UNSPECIFIED;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.Person;
import android.app.RemoteInput;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.Icon;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.Parcelable;
import android.service.notification.StatusBarNotification;
import android.util.Log;
import android.util.SparseArray;

import java.util.ArrayList;
import java.util.Objects;
import java.util.Set;

/**
 * Holds data about notifications.
 */
public class NotificationEntry {
    static final String TAG = "NotificationEntry";

    // Copied from hidden definitions in Notification.TvExtender
    private static final String EXTRA_TV_EXTENDER = "android.tv.EXTENSIONS";

    private final Context mContext;
    private final StatusBarNotification mSbn;
    private final PackageManager mPackageManager;
    private int mTargetSdkVersion = Build.VERSION_CODES.N_MR1;
    private final boolean mPreChannelsNotification;
    private final AudioAttributes mAttributes;
    private final NotificationChannel mChannel;
    private final int mImportance;
    private boolean mSeen;
    private boolean mIsShowActionEventLogged;
    private final SmsHelper mSmsHelper;

    private final Object mLock = new Object();

    public NotificationEntry(Context applicationContext, PackageManager packageManager,
            StatusBarNotification sbn, NotificationChannel channel, SmsHelper smsHelper) {
        mContext = applicationContext;
        mSbn = cloneStatusBarNotificationLight(sbn);
        mChannel = channel;
        mPackageManager = packageManager;
        mPreChannelsNotification = isPreChannelsNotification();
        mAttributes = calculateAudioAttributes();
        mImportance = calculateInitialImportance();
        mSmsHelper = smsHelper;
    }

    /** Adapted from {@code Notification.lightenPayload}. */
    @SuppressWarnings("nullness")
    private static void lightenNotificationPayload(Notification notification) {
        notification.tickerView = null;
        notification.contentView = null;
        notification.bigContentView = null;
        notification.headsUpContentView = null;
        notification.largeIcon = null;
        if (notification.extras != null && !notification.extras.isEmpty()) {
            final Set<String> keyset = notification.extras.keySet();
            final int keysetSize = keyset.size();
            final String[] keys = keyset.toArray(new String[keysetSize]);
            for (int i = 0; i < keysetSize; i++) {
                final String key = keys[i];
                if (EXTRA_TV_EXTENDER.equals(key)
                        || Notification.EXTRA_MESSAGES.equals(key)
                        || Notification.EXTRA_MESSAGING_PERSON.equals(key)
                        || Notification.EXTRA_PEOPLE_LIST.equals(key)) {
                    continue;
                }
                final Object obj = notification.extras.get(key);
                if (obj != null
                        && (obj instanceof Parcelable
                        || obj instanceof Parcelable[]
                        || obj instanceof SparseArray
                        || obj instanceof ArrayList)) {
                    notification.extras.remove(key);
                }
            }
        }
    }

    /** An interpretation of {@code Notification.cloneInto} with heavy=false. */
    private Notification cloneNotificationLight(Notification notification) {
        // We can't just use clone() here because the only way to remove the icons is with the
        // builder, which we can only create with a Context.
        Notification lightNotification =
                Notification.Builder.recoverBuilder(mContext, notification)
                        .setSmallIcon(0)
                        .setLargeIcon((Icon) null)
                        .build();
        lightenNotificationPayload(lightNotification);
        return lightNotification;
    }

    /** Adapted from {@code StatusBarNotification.cloneLight}. */
    public StatusBarNotification cloneStatusBarNotificationLight(StatusBarNotification sbn) {
        return new StatusBarNotification(
                sbn.getPackageName(),
                sbn.getOpPkg(),
                sbn.getId(),
                sbn.getTag(),
                sbn.getUid(),
                /*initialPid=*/ 0,
                /*score=*/ 0,
                cloneNotificationLight(sbn.getNotification()),
                sbn.getUser(),
                sbn.getPostTime());
    }

    private boolean isPreChannelsNotification() {
        try {
            ApplicationInfo info = mPackageManager.getApplicationInfoAsUser(
                    mSbn.getPackageName(), PackageManager.MATCH_ALL,
                    mSbn.getUser());
            if (info != null) {
                mTargetSdkVersion = info.targetSdkVersion;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Couldn't look up " + mSbn.getPackageName());
        }
        if (NotificationChannel.DEFAULT_CHANNEL_ID.equals(getChannel().getId())) {
            if (mTargetSdkVersion < Build.VERSION_CODES.O) {
                return true;
            }
        }
        return false;
    }

    private AudioAttributes calculateAudioAttributes() {
        final Notification n = getNotification();
        AudioAttributes attributes = getChannel().getAudioAttributes();
        if (attributes == null) {
            attributes = Notification.AUDIO_ATTRIBUTES_DEFAULT;
        }

        if (mPreChannelsNotification && !getChannel().hasUserSetSound()) {
            if (n.audioAttributes != null) {
                // prefer audio attributes to stream type
                attributes = n.audioAttributes;
            } else if (n.audioStreamType >= 0) {
                // the stream type is valid, use it
                attributes = new AudioAttributes.Builder()
                        .setLegacyStreamType(n.audioStreamType)
                        .build();
            } else {
                Log.w(TAG, String.format("Invalid stream type: %d", n.audioStreamType));
            }
        }
        return attributes;
    }

    private int calculateInitialImportance() {
        final Notification n = getNotification();
        int importance = getChannel().getImportance();
        int requestedImportance = IMPORTANCE_DEFAULT;

        // Migrate notification flags to scores
        if ((n.flags & Notification.FLAG_HIGH_PRIORITY) != 0) {
            n.priority = Notification.PRIORITY_MAX;
        }

        n.priority = clamp(n.priority, Notification.PRIORITY_MIN,
                Notification.PRIORITY_MAX);
        switch (n.priority) {
            case Notification.PRIORITY_MIN:
                requestedImportance = IMPORTANCE_MIN;
                break;
            case Notification.PRIORITY_LOW:
                requestedImportance = IMPORTANCE_LOW;
                break;
            case Notification.PRIORITY_DEFAULT:
                requestedImportance = IMPORTANCE_DEFAULT;
                break;
            case Notification.PRIORITY_HIGH:
            case Notification.PRIORITY_MAX:
                requestedImportance = IMPORTANCE_HIGH;
                break;
        }

        if (mPreChannelsNotification
                && (importance == IMPORTANCE_UNSPECIFIED
                || (getChannel().hasUserSetImportance()))) {
            if (n.fullScreenIntent != null) {
                requestedImportance = IMPORTANCE_HIGH;
            }
            importance = requestedImportance;
        }

        return importance;
    }

    public boolean isCategory(String category) {
        return Objects.equals(getNotification().category, category);
    }

    /**
     * Similar to {@link #isCategory(String)}, but checking the public version of the notification,
     * if available.
     */
    public boolean isPublicVersionCategory(String category) {
        Notification publicVersion = getNotification().publicVersion;
        if (publicVersion == null) {
            return false;
        }
        return Objects.equals(publicVersion.category, category);
    }

    public boolean isAudioAttributesUsage(int usage) {
        return mAttributes != null && mAttributes.getUsage() == usage;
    }

    private boolean hasPerson() {
        // TODO: cache favorite and recent contacts to check contact affinity
        ArrayList<Person> people = getNotification().extras.getParcelableArrayList(
                Notification.EXTRA_PEOPLE_LIST);
        return people != null && !people.isEmpty();
    }

    protected boolean hasStyle(Class targetStyle) {
        String templateClass = getNotification().extras.getString(Notification.EXTRA_TEMPLATE);
        return targetStyle.getName().equals(templateClass);
    }

    protected boolean isOngoing() {
        return (getNotification().flags & Notification.FLAG_FOREGROUND_SERVICE) != 0;
    }

    protected boolean involvesPeople() {
        return isMessaging()
                || hasStyle(Notification.InboxStyle.class)
                || hasPerson()
                || isDefaultSmsApp();
    }

    private boolean isDefaultSmsApp() {
        String defaultSmsApp = mSmsHelper.getDefaultSmsPackage();
        if (defaultSmsApp == null) {
            return false;
        }
        return mSbn.getPackageName().equals(defaultSmsApp);
    }

    protected boolean isMessaging() {
        return isCategory(CATEGORY_MESSAGE)
                || isPublicVersionCategory(CATEGORY_MESSAGE)
                || hasStyle(Notification.MessagingStyle.class);
    }

    public boolean hasInlineReply() {
        Notification.Action[] actions = getNotification().actions;
        if (actions == null) {
            return false;
        }
        for (Notification.Action action : actions) {
            RemoteInput[] remoteInputs = action.getRemoteInputs();
            if (remoteInputs == null) {
                continue;
            }
            for (RemoteInput remoteInput : remoteInputs) {
                if (remoteInput.getAllowFreeFormInput()) {
                    return true;
                }
            }
        }
        return false;
    }

    public void setSeen() {
        synchronized (mLock) {
            mSeen = true;
        }
    }

    public void setShowActionEventLogged() {
        synchronized (mLock) {
            mIsShowActionEventLogged = true;
        }
    }

    public boolean hasSeen() {
        synchronized (mLock) {
            return mSeen;
        }
    }

    public boolean isShowActionEventLogged() {
        synchronized (mLock) {
            return mIsShowActionEventLogged;
        }
    }

    public StatusBarNotification getSbn() {
        return mSbn;
    }

    public Notification getNotification() {
        return getSbn().getNotification();
    }

    public NotificationChannel getChannel() {
        return mChannel;
    }

    public int getImportance() {
        return mImportance;
    }

    private int clamp(int x, int low, int high) {
        return (x < low) ? low : ((x > high) ? high : x);
    }
}
