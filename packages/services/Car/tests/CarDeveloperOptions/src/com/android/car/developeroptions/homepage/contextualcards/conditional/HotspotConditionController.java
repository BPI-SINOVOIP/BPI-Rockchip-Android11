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

package com.android.car.developeroptions.homepage.contextualcards.conditional;

import android.app.settings.SettingsEnums;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.TetherSettings;
import com.android.car.developeroptions.core.SubSettingLauncher;
import com.android.car.developeroptions.homepage.contextualcards.ContextualCard;
import com.android.settingslib.RestrictedLockUtils;
import com.android.settingslib.RestrictedLockUtilsInternal;

import java.util.Objects;

public class HotspotConditionController implements ConditionalCardController {
    static final String TAG = "HotspotConditionController";
    static final int ID = Objects.hash("HotspotConditionController");

    private static final IntentFilter WIFI_AP_STATE_FILTER =
            new IntentFilter(WifiManager.WIFI_AP_STATE_CHANGED_ACTION);

    private final Context mAppContext;
    private final ConditionManager mConditionManager;
    private final WifiManager mWifiManager;
    private final Receiver mReceiver;


    public HotspotConditionController(Context appContext, ConditionManager conditionManager) {
        mAppContext = appContext;
        mConditionManager = conditionManager;
        mWifiManager = appContext.getSystemService(WifiManager.class);
        mReceiver = new Receiver();
    }

    @Override
    public long getId() {
        return ID;
    }

    @Override
    public boolean isDisplayable() {
        return mWifiManager.isWifiApEnabled();
    }

    @Override
    public void onPrimaryClick(Context context) {
        new SubSettingLauncher(context)
                .setDestination(TetherSettings.class.getName())
                .setSourceMetricsCategory(SettingsEnums.DASHBOARD_SUMMARY)
                .setTitleRes(R.string.tether_settings_title_all)
                .launch();
    }

    @Override
    public void onActionClick() {
        final RestrictedLockUtils.EnforcedAdmin admin =
                RestrictedLockUtilsInternal.checkIfRestrictionEnforced(
                        mAppContext, UserManager.DISALLOW_CONFIG_TETHERING, UserHandle.myUserId());
        if (admin != null) {
            RestrictedLockUtils.sendShowAdminSupportDetailsIntent(mAppContext, admin);
        } else {
            ConnectivityManager cm = (ConnectivityManager) mAppContext.getSystemService(
                    Context.CONNECTIVITY_SERVICE);
            cm.stopTethering(ConnectivityManager.TETHERING_WIFI);
        }
    }

    @Override
    public ContextualCard buildContextualCard() {
        return new ConditionalContextualCard.Builder()
                .setConditionId(ID)
                .setMetricsConstant(SettingsEnums.SETTINGS_CONDITION_HOTSPOT)
                .setActionText(mAppContext.getText(R.string.condition_turn_off))
                .setName(mAppContext.getPackageName() + "/"
                        + mAppContext.getText(R.string.condition_hotspot_title))
                .setTitleText(mAppContext.getText(R.string.condition_hotspot_title).toString())
                .setSummaryText(getSsid().toString())
                .setIconDrawable(mAppContext.getDrawable(R.drawable.ic_hotspot))
                .setViewType(ConditionContextualCardRenderer.VIEW_TYPE_HALF_WIDTH)
                .build();
    }

    @Override
    public void startMonitoringStateChange() {
        mAppContext.registerReceiver(mReceiver, WIFI_AP_STATE_FILTER);
    }

    @Override
    public void stopMonitoringStateChange() {
        mAppContext.unregisterReceiver(mReceiver);
    }

    private CharSequence getSsid() {
        WifiConfiguration wifiConfig = mWifiManager.getWifiApConfiguration();
        if (wifiConfig == null) {
            Log.e(TAG, "Ap config null unexpectedly");
            // should never happen.
            return "";
        } else {
            return wifiConfig.SSID;
        }
    }

    public class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (WifiManager.WIFI_AP_STATE_CHANGED_ACTION.equals(intent.getAction())) {
                mConditionManager.onConditionChanged();
            }
        }
    }
}
