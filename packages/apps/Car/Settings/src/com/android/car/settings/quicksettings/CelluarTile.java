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

package com.android.car.settings.quicksettings;

import android.annotation.Nullable;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.telephony.TelephonyManager;
import android.view.View;

import com.android.car.settings.R;
import com.android.car.settings.common.CarSettingActivities;
import com.android.car.settings.common.FragmentHost;
import com.android.settingslib.net.DataUsageController;

/**
 * Controls cellular on quick setting page.
 */
public class CelluarTile implements QuickSettingGridAdapter.Tile, DataUsageController.Callback {
    private final Context mContext;
    private final StateChangedListener mStateChangedListener;
    private final DataUsageController mDataUsageController;
    @Nullable
    private final String mCarrierName;
    private final boolean mAvailable;
    private final View.OnLongClickListener mLaunchDisplaySettings;

    private State mState = State.ON;

    CelluarTile(Context context, StateChangedListener stateChangedListener,
            FragmentHost fragmentHost) {
        mStateChangedListener = stateChangedListener;
        mContext = context;
        TelephonyManager manager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        mDataUsageController = new DataUsageController(mContext);
        mDataUsageController.setCallback(this);
        mAvailable = mDataUsageController.isMobileDataSupported();
        mState = mAvailable && mDataUsageController.isMobileDataEnabled() ? State.ON : State.OFF;
        mCarrierName = mAvailable ? manager.getNetworkOperatorName() : null;

        mLaunchDisplaySettings = v -> {
            context.startActivity(new Intent(context,
                    CarSettingActivities.MobileNetworkActivity.class));
            return true;
        };
    }

    @Override
    public void onMobileDataEnabled(boolean enabled) {
        mState = enabled ? State.ON : State.OFF;
    }

    @Override
    public View.OnLongClickListener getOnLongClickListener() {
        return mLaunchDisplaySettings;
    }

    @Override
    public boolean isAvailable() {
        return mAvailable;
    }

    @Override
    public Drawable getIcon() {
        return mContext.getDrawable(R.drawable.ic_cellular_data);
    }

    @Override
    @Nullable
    public String getText() {
        return mCarrierName;
    }

    @Override
    public State getState() {
        return mState;
    }

    @Override
    public void start() {
    }

    @Override
    public void stop() {
    }

    @Override
    public void onClick(View v) {
        mDataUsageController.setMobileDataEnabled(!mDataUsageController.isMobileDataEnabled());
    }
}
