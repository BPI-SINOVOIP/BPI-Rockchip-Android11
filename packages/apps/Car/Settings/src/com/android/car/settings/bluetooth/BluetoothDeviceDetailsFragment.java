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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.os.Bundle;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;
import com.android.settingslib.bluetooth.CachedBluetoothDevice;
import com.android.settingslib.bluetooth.LocalBluetoothManager;

import java.util.Arrays;
import java.util.List;

/**
 * Page which displays information for a remote Bluetooth device including the name, icon,
 * connection status, supported profiles, and MAC. Buttons are shown to enable
 * connecting/disconnecting and forgetting the device.
 */
public class BluetoothDeviceDetailsFragment extends SettingsFragment {

    private static final String KEY_DEVICE_ADDRESS = "device_address";

    private final CachedBluetoothDevice.Callback mDeviceCallback =
            this::updateConnectionButtonState;

    private CachedBluetoothDevice mCachedDevice;
    private MenuItem mConnectionButton;
    private MenuItem mForgetButton;

    /**
     * Returns a new {@link BluetoothDeviceDetailsFragment} for the given {@code device}.
     */
    public static BluetoothDeviceDetailsFragment newInstance(CachedBluetoothDevice device) {
        Bundle args = new Bundle();
        args.putString(BluetoothDeviceDetailsFragment.KEY_DEVICE_ADDRESS, device.getAddress());
        BluetoothDeviceDetailsFragment fragment = new BluetoothDeviceDetailsFragment();
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Arrays.asList(mConnectionButton, mForgetButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mConnectionButton = new MenuItem.Builder(getContext()).build();
        updateConnectionButtonState();
        mForgetButton = new MenuItem.Builder(getContext())
                .setTitle(R.string.forget)
                .setOnClickListener(i -> {
                    mCachedDevice.unpair();
                    goBack();
                })
                .build();
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.bluetooth_device_details_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        LocalBluetoothManager manager = BluetoothUtils.getLocalBtManager(context);
        if (manager == null) {
            goBack();
            return;
        }
        String deviceAddress = getArguments().getString(KEY_DEVICE_ADDRESS);
        BluetoothDevice remoteDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(
                deviceAddress);
        mCachedDevice = manager.getCachedDeviceManager().findDevice(remoteDevice);
        if (mCachedDevice == null) {
            goBack();
            return;
        }

        use(BluetoothDeviceNamePreferenceController.class,
                R.string.pk_bluetooth_device_name).setCachedDevice(mCachedDevice);
        use(BluetoothDeviceProfilesPreferenceController.class,
                R.string.pk_bluetooth_device_profiles).setCachedDevice(mCachedDevice);
        use(BluetoothDeviceAddressPreferenceController.class,
                R.string.pk_bluetooth_device_address).setCachedDevice(mCachedDevice);
    }

    @Override
    public void onStart() {
        super.onStart();
        mCachedDevice.registerCallback(mDeviceCallback);
        updateConnectionButtonState();
    }

    @Override
    public void onStop() {
        super.onStop();
        mCachedDevice.unregisterCallback(mDeviceCallback);
    }

    private void updateConnectionButtonState() {
        mConnectionButton.setEnabled(!mCachedDevice.isBusy());
        if (mCachedDevice.isConnected()) {
            mConnectionButton.setTitle(R.string.disconnect);
            mConnectionButton.setOnClickListener(view -> mCachedDevice.disconnect());
        } else {
            mConnectionButton.setTitle(R.string.connect);
            mConnectionButton.setOnClickListener(
                    view -> mCachedDevice.connect(/* connectAllProfiles= */ true));
        }
    }
}
