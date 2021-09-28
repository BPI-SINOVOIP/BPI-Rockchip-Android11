/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.car.settings.bluetooth;

import static android.os.UserManager.DISALLOW_CONFIG_BLUETOOTH;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;

import androidx.preference.PreferenceGroup;

import com.android.car.settings.R;
import com.android.car.settings.common.CarUxRestrictionsHelper;
import com.android.car.settings.common.FragmentController;
import com.android.settingslib.bluetooth.BluetoothDeviceFilter;
import com.android.settingslib.bluetooth.CachedBluetoothDevice;

/**
 * Displays a list of bonded (paired) Bluetooth devices. Clicking on a device will attempt a
 * connection with that device. If a device is already connected, a click will prompt the user to
 * disconnect. Devices are shown with an addition affordance which launches the device details page.
 */
public class BluetoothBondedDevicesPreferenceController extends
        BluetoothDevicesGroupPreferenceController {

    public BluetoothBondedDevicesPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected BluetoothDeviceFilter.Filter getDeviceFilter() {
        return BluetoothDeviceFilter.BONDED_DEVICE_FILTER;
    }

    @Override
    protected BluetoothDevicePreference createDevicePreference(CachedBluetoothDevice cachedDevice) {
        BluetoothDevicePreference pref = super.createDevicePreference(cachedDevice);
        pref.setSecondaryActionIcon(R.drawable.ic_settings_gear);
        pref.setOnSecondaryActionClickListener(() -> getFragmentController().launchFragment(
                BluetoothDeviceDetailsFragment.newInstance(cachedDevice)));
        return pref;
    }

    @Override
    protected void onDeviceClicked(CachedBluetoothDevice cachedDevice) {
        if (cachedDevice.isConnected()) {
            getFragmentController().showDialog(
                    BluetoothDisconnectConfirmDialogFragment.newInstance(cachedDevice),
                    /* tag= */ null);
        } else {
            cachedDevice.connect(/* connectAllProfiles= */ true);
        }
    }

    @Override
    public void onDeviceBondStateChanged(CachedBluetoothDevice cachedDevice, int bondState) {
        refreshUi();
    }

    @Override
    protected void updateState(PreferenceGroup preferenceGroup) {
        super.updateState(preferenceGroup);

        boolean hasUserRestriction = getUserManager().hasUserRestriction(DISALLOW_CONFIG_BLUETOOTH);
        updateActionVisibility(preferenceGroup, !hasUserRestriction);
    }

    @Override
    protected void onApplyUxRestrictions(CarUxRestrictions uxRestrictions) {
        super.onApplyUxRestrictions(uxRestrictions);

        if (CarUxRestrictionsHelper.isNoSetup(uxRestrictions)) {
            updateActionVisibility(getPreference(), /* isActionVisible= */ false);
        }
    }

    private void updateActionVisibility(PreferenceGroup group, boolean isActionVisible) {
        for (int i = 0; i < group.getPreferenceCount(); i++) {
            ((BluetoothDevicePreference) group.getPreference(i)).setSecondaryActionVisible(
                    isActionVisible);
        }
    }
}
