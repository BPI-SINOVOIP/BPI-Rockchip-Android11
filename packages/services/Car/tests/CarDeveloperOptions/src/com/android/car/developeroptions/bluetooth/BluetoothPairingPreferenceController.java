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

package com.android.car.developeroptions.bluetooth;

import android.content.Context;

import androidx.preference.Preference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.PreferenceControllerMixin;
import com.android.car.developeroptions.core.SubSettingLauncher;
import com.android.car.developeroptions.dashboard.DashboardFragment;
import com.android.settingslib.core.AbstractPreferenceController;


/**
 * Controller that shows and updates the bluetooth device name
 */
public class BluetoothPairingPreferenceController extends AbstractPreferenceController
        implements PreferenceControllerMixin {
    private static final String TAG = "BluetoothPairingPrefCtrl";

    public static final String KEY_PAIRING = "pref_bt_pairing";
    private DashboardFragment mFragment;
    private Preference mPreference;

    public BluetoothPairingPreferenceController(Context context, DashboardFragment fragment) {
        super(context);
        mFragment = fragment;
    }

    @Override
    public boolean isAvailable() {
        return true;
    }

    @Override
    public String getPreferenceKey() {
        return KEY_PAIRING;
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        if (KEY_PAIRING.equals(preference.getKey())) {
            new SubSettingLauncher(mContext)
                    .setDestination(BluetoothPairingDetail.class.getName())
                    .setTitleRes(R.string.bluetooth_pairing_page_title)
                    .setSourceMetricsCategory(mFragment.getMetricsCategory())
                    .launch();

            return true;
        }

        return false;
    }

    /**
     * Create pairing preference to jump to pairing page
     *
     * @return bluetooth preference that created in this method
     */
    public Preference createBluetoothPairingPreference(int order) {
        mPreference = new Preference(mFragment.getPreferenceScreen().getContext());
        mPreference.setKey(KEY_PAIRING);
        mPreference.setIcon(R.drawable.ic_menu_add);
        mPreference.setOrder(order);
        mPreference.setTitle(R.string.bluetooth_pairing_pref_title);

        return mPreference;
    }

}
