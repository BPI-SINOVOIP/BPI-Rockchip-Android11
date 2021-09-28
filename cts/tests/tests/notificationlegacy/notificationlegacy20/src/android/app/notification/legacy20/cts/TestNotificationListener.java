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
package android.app.notification.legacy20.cts;

import android.content.ComponentName;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import java.util.ArrayList;

public class TestNotificationListener extends NotificationListenerService {
    public static final String TAG = "TestNotificationListener";
    public static final String PKG = "android.app.notification.legacy.cts";

    private ArrayList<String> mTestPackages = new ArrayList<>();

    public ArrayList<StatusBarNotification> mPosted = new ArrayList<>();
    public ArrayList<StatusBarNotification> mRemoved = new ArrayList<>();
    public RankingMap mRankingMap;

    private static TestNotificationListener sNotificationListenerInstance = null;
    boolean isConnected;

    public static String getId() {
        return String.format("%s/%s", TestNotificationListener.class.getPackage().getName(),
                TestNotificationListener.class.getName());
    }

    public static ComponentName getComponentName() {
        return new ComponentName(TestNotificationListener.class.getPackage().getName(),
                TestNotificationListener.class.getName());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mTestPackages.add(PKG);
    }

    @Override
    public void onListenerConnected() {
        super.onListenerConnected();
        sNotificationListenerInstance = this;
        isConnected = true;
    }

    @Override
    public void onListenerDisconnected() {
        isConnected = false;
    }

    public static TestNotificationListener getInstance() {
        return sNotificationListenerInstance;
    }

    public void resetData() {
        mPosted.clear();
        mRemoved.clear();
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn, RankingMap rankingMap) {
        if (!mTestPackages.contains(sbn.getPackageName())) { return; }
        mRankingMap = rankingMap;
        mPosted.add(sbn);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn, RankingMap rankingMap) {
        if (!mTestPackages.contains(sbn.getPackageName())) { return; }
        mRankingMap = rankingMap;
        mRemoved.add(sbn);
    }

    @Override
    public void onNotificationRankingUpdate(RankingMap rankingMap) {
        mRankingMap = rankingMap;
    }
}
