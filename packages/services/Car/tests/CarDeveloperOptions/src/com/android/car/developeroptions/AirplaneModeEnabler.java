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

package com.android.car.developeroptions;

import android.app.settings.SettingsEnums;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.os.Handler;
import android.os.Looper;
import android.os.UserHandle;
import android.provider.Settings;
import android.sysprop.TelephonyProperties;

import com.android.settingslib.WirelessUtils;
import com.android.settingslib.core.instrumentation.MetricsFeatureProvider;

public class AirplaneModeEnabler  extends BroadcastReceiver {

    private static final int EVENT_SERVICE_STATE_CHANGED = 3;

    private final Context mContext;
    private final MetricsFeatureProvider mMetricsFeatureProvider;
    private IntentFilter mFilter;

    private OnAirplaneModeChangedListener mOnAirplaneModeChangedListener;

    public interface OnAirplaneModeChangedListener {
        /**
         * Called when airplane mode status is changed.
         *
         * @param isAirplaneModeOn the airplane mode is on
         */
        void onAirplaneModeChanged(boolean isAirplaneModeOn);
    }

    private ContentObserver mAirplaneModeObserver = new ContentObserver(
            new Handler(Looper.getMainLooper())) {
        @Override
        public void onChange(boolean selfChange) {
            onAirplaneModeChanged();
        }
    };

    public AirplaneModeEnabler(Context context, MetricsFeatureProvider metricsFeatureProvider,
            OnAirplaneModeChangedListener listener) {

        mContext = context;
        mMetricsFeatureProvider = metricsFeatureProvider;
        mOnAirplaneModeChangedListener = listener;

        mFilter = new IntentFilter(Intent.ACTION_SERVICE_STATE);
    }

    public void resume() {
        mContext.registerReceiver(this, mFilter);
        mContext.getContentResolver().registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.AIRPLANE_MODE_ON), true,
                mAirplaneModeObserver);
    }

    public void pause() {
        mContext.unregisterReceiver(this);
        mContext.getContentResolver().unregisterContentObserver(mAirplaneModeObserver);
    }

    private void setAirplaneModeOn(boolean enabling) {
        // Change the system setting
        Settings.Global.putInt(mContext.getContentResolver(), Settings.Global.AIRPLANE_MODE_ON,
                enabling ? 1 : 0);

        // Notify listener the system setting is changed.
        if (mOnAirplaneModeChangedListener != null) {
            mOnAirplaneModeChangedListener.onAirplaneModeChanged(enabling);
        }

        // Post the intent
        Intent intent = new Intent(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        intent.putExtra("state", enabling);
        mContext.sendBroadcastAsUser(intent, UserHandle.ALL);
    }

    /**
     * Called when we've received confirmation that the airplane mode was set.
     * TODO: We update the checkbox summary when we get notified
     * that mobile radio is powered up/down. We should not have dependency
     * on one radio alone. We need to do the following:
     * - handle the case of wifi/bluetooth failures
     * - mobile does not send failure notification, fail on timeout.
     */
    private void onAirplaneModeChanged() {
        if (mOnAirplaneModeChangedListener != null) {
            mOnAirplaneModeChangedListener.onAirplaneModeChanged(isAirplaneModeOn());
        }
    }

    public void setAirplaneMode(boolean isAirplaneModeOn) {
        if (TelephonyProperties.in_ecm_mode().orElse(false)) {
            // In ECM mode, do not update database at this point
        } else {
            mMetricsFeatureProvider.action(mContext, SettingsEnums.ACTION_AIRPLANE_TOGGLE,
                    isAirplaneModeOn);
            setAirplaneModeOn(isAirplaneModeOn);
        }
    }

    public void setAirplaneModeInECM(boolean isECMExit, boolean isAirplaneModeOn) {
        if (isECMExit) {
            // update database based on the current checkbox state
            setAirplaneModeOn(isAirplaneModeOn);
        } else {
            // update summary
            onAirplaneModeChanged();
        }
    }

    public boolean isAirplaneModeOn() {
        return WirelessUtils.isAirplaneModeOn(mContext);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();

        if (Intent.ACTION_SERVICE_STATE.equals(action)) {
            onAirplaneModeChanged();
        }
    }
}
