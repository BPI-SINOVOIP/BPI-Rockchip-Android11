/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.car.developeroptions.notification;

import static android.app.NotificationManager.IMPORTANCE_NONE;
import static android.app.NotificationManager.IMPORTANCE_UNSPECIFIED;

import android.app.INotificationManager;
import android.app.NotificationManager;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.usage.IUsageStatsManager;
import android.app.usage.UsageEvents;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ParceledListSlice;
import android.graphics.drawable.Drawable;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.UserHandle;
import android.text.format.DateUtils;
import android.util.IconDrawableFactory;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.settingslib.R;
import com.android.settingslib.Utils;
import com.android.settingslib.utils.StringUtil;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class NotificationBackend {
    private static final String TAG = "NotificationBackend";

    static IUsageStatsManager sUsageStatsManager = IUsageStatsManager.Stub.asInterface(
            ServiceManager.getService(Context.USAGE_STATS_SERVICE));
    private static final int DAYS_TO_CHECK = 7;
    static INotificationManager sINM = INotificationManager.Stub.asInterface(
            ServiceManager.getService(Context.NOTIFICATION_SERVICE));

    public AppRow loadAppRow(Context context, PackageManager pm, ApplicationInfo app) {
        final AppRow row = new AppRow();
        row.pkg = app.packageName;
        row.uid = app.uid;
        try {
            row.label = app.loadLabel(pm);
        } catch (Throwable t) {
            Log.e(TAG, "Error loading application label for " + row.pkg, t);
            row.label = row.pkg;
        }
        row.icon = IconDrawableFactory.newInstance(context).getBadgedIcon(app);
        row.banned = getNotificationsBanned(row.pkg, row.uid);
        row.showBadge = canShowBadge(row.pkg, row.uid);
        row.bubblePreference = getBubblePreference(row.pkg, row.uid);
        row.userId = UserHandle.getUserId(row.uid);
        row.blockedChannelCount = getBlockedChannelCount(row.pkg, row.uid);
        row.channelCount = getChannelCount(row.pkg, row.uid);
        recordAggregatedUsageEvents(context, row);
        return row;
    }

    public boolean isBlockable(Context context, ApplicationInfo info) {
        final boolean blocked = getNotificationsBanned(info.packageName, info.uid);
        final boolean systemApp = isSystemApp(context, info);
        return !systemApp || (systemApp && blocked);
    }

    public AppRow loadAppRow(Context context, PackageManager pm, PackageInfo app) {
        final AppRow row = loadAppRow(context, pm, app.applicationInfo);
        recordCanBeBlocked(context, pm, app, row);
        return row;
    }

    void recordCanBeBlocked(Context context, PackageManager pm, PackageInfo app, AppRow row) {
        row.systemApp = Utils.isSystemPackage(context.getResources(), pm, app);
        final String[] nonBlockablePkgs = context.getResources().getStringArray(
                com.android.internal.R.array.config_nonBlockableNotificationPackages);
        markAppRowWithBlockables(nonBlockablePkgs, row, app.packageName);
    }

    @VisibleForTesting static void markAppRowWithBlockables(String[] nonBlockablePkgs, AppRow row,
            String packageName) {
        if (nonBlockablePkgs != null) {
            int N = nonBlockablePkgs.length;
            for (int i = 0; i < N; i++) {
                String pkg = nonBlockablePkgs[i];
                if (pkg == null) {
                    continue;
                } else if (pkg.contains(":")) {
                    // Interpret as channel; lock only this channel for this app.
                    if (packageName.equals(pkg.split(":", 2)[0])) {
                        row.lockedChannelId = pkg.split(":", 2 )[1];
                    }
                } else if (packageName.equals(nonBlockablePkgs[i])) {
                    row.systemApp = row.lockedImportance = true;
                }
            }
        }
    }

    public boolean isSystemApp(Context context, ApplicationInfo app) {
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(
                    app.packageName, PackageManager.GET_SIGNATURES);
            final AppRow row = new AppRow();
            recordCanBeBlocked(context,  context.getPackageManager(), info, row);
            return row.systemApp;
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        return false;
    }

    public boolean getNotificationsBanned(String pkg, int uid) {
        try {
            final boolean enabled = sINM.areNotificationsEnabledForPackage(pkg, uid);
            return !enabled;
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public boolean setNotificationsEnabledForPackage(String pkg, int uid, boolean enabled) {
        try {
            if (onlyHasDefaultChannel(pkg, uid)) {
                NotificationChannel defaultChannel =
                        getChannel(pkg, uid, NotificationChannel.DEFAULT_CHANNEL_ID, null);
                defaultChannel.setImportance(enabled ? IMPORTANCE_UNSPECIFIED : IMPORTANCE_NONE);
                updateChannel(pkg, uid, defaultChannel);
            }
            sINM.setNotificationsEnabledForPackage(pkg, uid, enabled);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public boolean canShowBadge(String pkg, int uid) {
        try {
            return sINM.canShowBadge(pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public boolean setShowBadge(String pkg, int uid, boolean showBadge) {
        try {
            sINM.setShowBadge(pkg, uid, showBadge);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public int getBubblePreference(String pkg, int uid) {
        try {
            return sINM.getBubblePreferenceForPackage(pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return -1;
        }
    }

    public boolean setAllowBubbles(String pkg, int uid, int pref) {
        try {
            sINM.setBubblesAllowed(pkg, uid, pref);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public NotificationChannel getChannel(String pkg, int uid, String channelId) {
        if (channelId == null) {
            return null;
        }
        try {
            return sINM.getNotificationChannelForPackage(pkg, uid, channelId, null, true);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return null;
        }
    }


    public NotificationChannel getChannel(String pkg, int uid, String channelId,
            String conversationId) {
        if (channelId == null) {
            return null;
        }
        try {
            return sINM.getNotificationChannelForPackage(pkg, uid, channelId, conversationId, true);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return null;
        }
    }

    public NotificationChannelGroup getGroup(String pkg, int uid, String groupId) {
        if (groupId == null) {
            return null;
        }
        try {
            return sINM.getNotificationChannelGroupForPackage(groupId, pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return null;
        }
    }

    public ParceledListSlice<NotificationChannelGroup> getGroups(String pkg, int uid) {
        try {
            return sINM.getNotificationChannelGroupsForPackage(pkg, uid, false);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return ParceledListSlice.emptyList();
        }
    }

    /**
     * Returns all notification channels associated with the package and uid that will bypass DND
     */
    public ParceledListSlice<NotificationChannel> getNotificationChannelsBypassingDnd(String pkg,
            int uid) {
        try {
            return sINM.getNotificationChannelsBypassingDnd(pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return ParceledListSlice.emptyList();
        }
    }

    public void updateChannel(String pkg, int uid, NotificationChannel channel) {
        try {
            sINM.updateNotificationChannelForPackage(pkg, uid, channel);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
        }
    }

    public void updateChannelGroup(String pkg, int uid, NotificationChannelGroup group) {
        try {
            sINM.updateNotificationChannelGroupForPackage(pkg, uid, group);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
        }
    }

    public int getDeletedChannelCount(String pkg, int uid) {
        try {
            return sINM.getDeletedChannelCount(pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return 0;
        }
    }

    public int getBlockedChannelCount(String pkg, int uid) {
        try {
            return sINM.getBlockedChannelCount(pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return 0;
        }
    }

    public boolean onlyHasDefaultChannel(String pkg, int uid) {
        try {
            return sINM.onlyHasDefaultChannel(pkg, uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public int getChannelCount(String pkg, int uid) {
        try {
            return sINM.getNumNotificationChannelsForPackage(pkg, uid, false);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return 0;
        }
    }

    public int getNumAppsBypassingDnd(int uid) {
        try {
            return sINM.getAppsBypassingDndCount(uid);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return 0;
        }
    }

    public int getBlockedAppCount() {
        try {
            return sINM.getBlockedAppCount(UserHandle.myUserId());
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return 0;
        }
    }

    public boolean shouldHideSilentStatusBarIcons(Context context) {
        try {
            return sINM.shouldHideSilentStatusIcons(context.getPackageName());
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    public void setHideSilentStatusIcons(boolean hide) {
        try {
            sINM.setHideSilentStatusIcons(hide);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
        }
    }

    public void allowAssistantAdjustment(String capability, boolean allowed) {
        try {
            if (allowed) {
                sINM.allowAssistantAdjustment(capability);
            } else {
                sINM.disallowAssistantAdjustment(capability);
            }
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
        }
    }

    public List<String> getAssistantAdjustments(String pkg) {
        try {
            return sINM.getAllowedAssistantAdjustments(pkg);
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
        }
        return new ArrayList<>();
    }

    protected void recordAggregatedUsageEvents(Context context, AppRow appRow) {
        long now = System.currentTimeMillis();
        long startTime = now - (DateUtils.DAY_IN_MILLIS * DAYS_TO_CHECK);
        UsageEvents events = null;
        try {
            events = sUsageStatsManager.queryEventsForPackageForUser(
                    startTime, now, appRow.userId, appRow.pkg, context.getPackageName());
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        recordAggregatedUsageEvents(events, appRow);
    }

    protected void recordAggregatedUsageEvents(UsageEvents events, AppRow appRow) {
        appRow.sentByChannel = new HashMap<>();
        appRow.sentByApp = new NotificationsSentState();
        if (events != null) {
            UsageEvents.Event event = new UsageEvents.Event();
            while (events.hasNextEvent()) {
                events.getNextEvent(event);

                if (event.getEventType() == UsageEvents.Event.NOTIFICATION_INTERRUPTION) {
                    String channelId = event.mNotificationChannelId;
                    if (channelId != null) {
                        NotificationsSentState stats = appRow.sentByChannel.get(channelId);
                        if (stats == null) {
                            stats = new NotificationsSentState();
                            appRow.sentByChannel.put(channelId, stats);
                        }
                        if (event.getTimeStamp() > stats.lastSent) {
                            stats.lastSent = event.getTimeStamp();
                            appRow.sentByApp.lastSent = event.getTimeStamp();
                        }
                        stats.sentCount++;
                        appRow.sentByApp.sentCount++;
                        calculateAvgSentCounts(stats);
                    }
                }

            }
            calculateAvgSentCounts(appRow.sentByApp);
        }
    }

    public static CharSequence getSentSummary(Context context, NotificationsSentState state,
            boolean sortByRecency) {
        if (state == null) {
            return null;
        }
        if (sortByRecency) {
            if (state.lastSent == 0) {
                return context.getString(R.string.notifications_sent_never);
            }
            return StringUtil.formatRelativeTime(
                    context, System.currentTimeMillis() - state.lastSent, true);
        } else {
            if (state.avgSentWeekly > 0) {
                return context.getString(R.string.notifications_sent_weekly, state.avgSentWeekly);
            }
            return context.getString(R.string.notifications_sent_daily, state.avgSentDaily);
        }
    }

    private void calculateAvgSentCounts(NotificationsSentState stats) {
        if (stats != null) {
            stats.avgSentDaily = Math.round((float) stats.sentCount / DAYS_TO_CHECK);
            if (stats.sentCount < DAYS_TO_CHECK) {
                stats.avgSentWeekly = stats.sentCount;
            }
        }
    }

    public ComponentName getAllowedNotificationAssistant() {
        try {
            return sINM.getAllowedNotificationAssistant();
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return null;
        }
    }

    public boolean setNotificationAssistantGranted(ComponentName cn) {
        try {
            sINM.setNotificationAssistantAccessGranted(cn, true);
            if (cn == null) {
                return sINM.getAllowedNotificationAssistant() == null;
            } else {
                return cn.equals(sINM.getAllowedNotificationAssistant());
            }
        } catch (Exception e) {
            Log.w(TAG, "Error calling NoMan", e);
            return false;
        }
    }

    /**
     * NotificationsSentState contains how often an app sends notifications and how recently it sent
     * one.
     */
    public static class NotificationsSentState {
        public int avgSentDaily = 0;
        public int avgSentWeekly = 0;
        public long lastSent = 0;
        public int sentCount = 0;
    }

    static class Row {
        public String section;
    }

    public static class AppRow extends Row {
        public String pkg;
        public int uid;
        public Drawable icon;
        public CharSequence label;
        public Intent settingsIntent;
        public boolean banned;
        public boolean first;  // first app in section
        public boolean systemApp;
        public boolean lockedImportance;
        public String lockedChannelId;
        public boolean showBadge;
        public int bubblePreference = NotificationManager.BUBBLE_PREFERENCE_NONE;
        public int userId;
        public int blockedChannelCount;
        public int channelCount;
        public Map<String, NotificationsSentState> sentByChannel;
        public NotificationsSentState sentByApp;
    }
}
