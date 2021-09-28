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

package android.app.notification.legacy29.cts;

import android.app.NotificationManager;
import android.content.ComponentName;
import android.os.Bundle;
import android.service.notification.Adjustment;
import android.service.notification.NotificationAssistantService;
import android.service.notification.StatusBarNotification;

import java.util.List;

public class TestNotificationAssistant extends NotificationAssistantService {
    public static final String TAG = "TestNotificationAssistant";
    public static final String PKG = "android.app.notification.legacy29.cts";

    private static TestNotificationAssistant sNotificationAssistantInstance = null;
    boolean isConnected;
    boolean isPanelOpen = false;
    public List<String> currentCapabilities;
    int notificationVisibleCount = 0;
    int notificationSeenCount = 0;
    int notificationHiddenCount = 0;
    String snoozedKey;
    String snoozedUntilContext;
    private NotificationManager mNotificationManager;

    public static String getId() {
        return String.format("%s/%s", TestNotificationAssistant.class.getPackage().getName(),
                TestNotificationAssistant.class.getName());
    }

    public static ComponentName getComponentName() {
        return new ComponentName(TestNotificationAssistant.class.getPackage().getName(),
                TestNotificationAssistant.class.getName());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mNotificationManager = getSystemService(NotificationManager.class);
    }

    @Override
    public void onListenerConnected() {
        super.onListenerConnected();
        sNotificationAssistantInstance = this;
        isConnected = true;
    }

    @Override
    public void onListenerDisconnected() {
        isConnected = false;
    }

    public static TestNotificationAssistant getInstance() {
        return sNotificationAssistantInstance;
    }

    @Override
    public void onNotificationSnoozedUntilContext(StatusBarNotification statusBarNotification,
            String s) {
        snoozedKey = statusBarNotification.getKey();
        snoozedUntilContext = s;
    }

    @Override
    public Adjustment onNotificationEnqueued(StatusBarNotification sbn) {
        Bundle signals = new Bundle();
        signals.putInt(Adjustment.KEY_USER_SENTIMENT, Ranking.USER_SENTIMENT_POSITIVE);
        return new Adjustment(sbn.getPackageName(), sbn.getKey(), signals, "",
                sbn.getUser());
    }

    @Override
    public void onAllowedAdjustmentsChanged() {
        currentCapabilities = mNotificationManager.getAllowedAssistantAdjustments();
    }

    void resetNotificationVisibilityCounts() {
        notificationHiddenCount = 0;
        notificationVisibleCount = 0;
        notificationSeenCount = 0;
    }

    @Override
    public void onNotificationVisibilityChanged(String key, boolean isVisible) {
        if (isVisible) {
            notificationVisibleCount++;
        } else {
            notificationHiddenCount++;
        }
    }

    @Override
    public void onNotificationsSeen(List<String> keys) {
        notificationSeenCount += keys.size();
    }

    @Override
    public void onPanelHidden() {
        isPanelOpen = false;
    }

    @Override
    public void onPanelRevealed(int items) {
        isPanelOpen = true;
    }
}