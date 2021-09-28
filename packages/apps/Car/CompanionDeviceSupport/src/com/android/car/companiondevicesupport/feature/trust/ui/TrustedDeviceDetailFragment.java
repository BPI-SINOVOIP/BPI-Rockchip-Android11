/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.companiondevicesupport.feature.trust.ui;

import static com.android.car.connecteddevice.util.SafeLog.loge;

import android.annotation.NonNull;
import android.os.Bundle;
import android.text.Html;
import android.text.Spanned;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Switch;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.companiondevicesupport.api.internal.trust.TrustedDevice;

import java.util.List;

/** Fragment that shows the details of an trusted device. */
public class TrustedDeviceDetailFragment extends Fragment {
    private final static String TAG = "TrustedDeviceDetailFragment";
    private final static String ASSOCIATED_DEVICE_KEY = "AssociatedDeviceKey";

    private AssociatedDevice mAssociatedDevice;
    private TrustedDeviceViewModel mModel;
    private TrustedDevice mTrustedDevice;
    private Switch mSwitch;
    private TextView mTrustedDeviceTitle;

    static TrustedDeviceDetailFragment newInstance(@NonNull AssociatedDevice device) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(ASSOCIATED_DEVICE_KEY, device);
        TrustedDeviceDetailFragment fragment = new TrustedDeviceDetailFragment();
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.trusted_device_detail_fragment, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Bundle bundle = getArguments();
        if (bundle == null) {
            loge(TAG, "No valid arguments for TrustedDeviceDetailFragment.");
            return;
        }
        mAssociatedDevice = bundle.getParcelable(ASSOCIATED_DEVICE_KEY);
        mTrustedDeviceTitle = view.findViewById(R.id.trusted_device_item_title);
        setTrustedDeviceTitle();
        mModel = ViewModelProviders.of(getActivity()).get(TrustedDeviceViewModel.class);
        mSwitch = view.findViewById(R.id.trusted_device_switch);
        mSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked && mTrustedDevice == null) {
                // When the current device has not been enrolled as trusted device, turning on the
                // switch is for enrolling the current device.
                mModel.setDeviceToEnable(mAssociatedDevice);
                mSwitch.setChecked(false);
            } else if (!isChecked && mTrustedDevice != null) {
                // When the current device has been enrolled as trusted device, turning off the
                // switch is for disable trusted device feature for the current device.
                mModel.setDeviceToDisable(mTrustedDevice);
                mSwitch.setChecked(true);
            }
            // Ignore other conditions as {@link Switch#setChecked(boolean)} will always trigger
            // this listener.
        });
        observeViewModel();
    }

    private void setTrustedDeviceTitle() {
        String deviceName = mAssociatedDevice.getDeviceName();
        if (deviceName == null) {
            deviceName = getString(R.string.unknown);
        }
        String deviceTitle = getString(R.string.trusted_device_item_title, deviceName);
        Spanned styledDeviceTitle = Html.fromHtml(deviceTitle, Html.FROM_HTML_MODE_LEGACY);
        mTrustedDeviceTitle.setText(styledDeviceTitle);
    }

    private void setTrustedDevices(List<TrustedDevice> devices) {
        if (devices == null) {
            mSwitch.setChecked(false);
            mTrustedDevice = null;
            return;
        }
        if (devices.isEmpty()) {
            mSwitch.setChecked(false);
            mTrustedDevice = null;
            return;
        }
        if (devices.size() > 1) {
            loge(TAG, "More than one devices have been associated.");
            return;
        }
        // Currently, we only support single trusted device.
        TrustedDevice device = devices.get(0);
        if (!device.getDeviceId().equals(mAssociatedDevice.getDeviceId())) {
            loge(TAG, "Trusted device id doesn't match associated device id.");
            return;
        }
        mTrustedDevice = device;
        mSwitch.setChecked(true);
    }

    private void observeViewModel() {
        mModel.getAssociatedDevice().observe(this, device -> {
            if (device == null) {
                return;
            }
            if (device.getDeviceId().equals(mAssociatedDevice.getDeviceId())) {
                mAssociatedDevice = device;
                setTrustedDeviceTitle();
            }
        });

        mModel.getTrustedDevices().observe(this, this::setTrustedDevices);

        mModel.getDisabledDevice().observe(this, device -> {
            if (device == null) {
                return;
            }
            mModel.setDisabledDevice(null);
            if (mTrustedDevice.equals(device)) {
                mTrustedDevice = null;
                mSwitch.setChecked(false);
            }
        });

        mModel.getEnabledDevice().observe(this, device -> {
            if (device == null) {
                return;
            }
            mModel.setEnabledDevice(null);
            if (device.getDeviceId().equals(mAssociatedDevice.getDeviceId())) {
                mTrustedDevice = device;
                mSwitch.setChecked(true);
            }
        });
    }
}
