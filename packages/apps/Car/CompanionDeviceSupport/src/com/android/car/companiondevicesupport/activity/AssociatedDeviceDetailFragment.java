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

package com.android.car.companiondevicesupport.activity;

import static com.android.car.connecteddevice.util.SafeLog.loge;

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.feature.trust.TrustedDeviceConstants;


/** Fragment that shows the details of an associated device. */
public class AssociatedDeviceDetailFragment extends Fragment {
    private final static String TAG = "AssociatedDeviceDetailFragment";
    private TextView mDeviceName;
    private TextView mConnectionStatusText;
    private ImageView mConnectionStatusIndicator;
    private TextView mConnectionText;
    private ImageView mConnectionIcon;
    private AssociatedDeviceViewModel mModel;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.associated_device_detail_fragment, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mDeviceName = view.findViewById(R.id.device_name);
        mConnectionIcon = view.findViewById(R.id.connection_icon);
        mConnectionText = view.findViewById(R.id.connection_text);
        mConnectionStatusText = view.findViewById(R.id.connection_status_text);
        mConnectionStatusIndicator = view.findViewById(R.id.connection_status_indicator);

        mModel = ViewModelProviders.of(getActivity()).get(AssociatedDeviceViewModel.class);
        mModel.getDeviceDetails().observe(this, this::setDeviceDetails);

        view.findViewById(R.id.connection_button).setOnClickListener(l -> {
            mModel.toggleConnectionStatusForCurrentDevice();
        });
        view.findViewById(R.id.remove_button).setOnClickListener(v ->
                mModel.selectCurrentDeviceToRemove());
        view.findViewById(R.id.trusted_device_feature_button).setOnClickListener(v ->
                mModel.startFeatureActivityForCurrentDevice(
                        TrustedDeviceConstants.INTENT_ACTION_TRUSTED_DEVICE_SETTING));
    }

    private void setDeviceDetails(AssociatedDeviceDetails deviceDetails) {
        if (deviceDetails == null) {
            return;
        }
        mDeviceName.setText(deviceDetails.getDeviceName());

        if (!deviceDetails.isConnectionEnabled()) {
            setConnectionStatus(
                    ContextCompat.getColor(getContext(), R.color.connection_color_disconnected),
                    getString(R.string.disconnected),
                    ContextCompat.getDrawable(getContext(), R.drawable.ic_phonelink_ring),
                    getString(R.string.enable_device_connection_text));
        } else if (deviceDetails.isConnected()) {
            setConnectionStatus(
                    ContextCompat.getColor(getContext(), R.color.connection_color_connected),
                    getString(R.string.connected),
                    ContextCompat.getDrawable(getContext(), R.drawable.ic_phonelink_erase),
                    getString(R.string.disable_device_connection_text));
        } else {
            setConnectionStatus(
                    ContextCompat.getColor(getContext(), R.color.connection_color_not_detected),
                    getString(R.string.notDetected),
                    ContextCompat.getDrawable(getContext(), R.drawable.ic_phonelink_erase),
                    getString(R.string.disable_device_connection_text));
        }
    }

    private void setConnectionStatus(int connectionStatusColor, String connectionStatusText,
            Drawable connectionIcon, String connectionText) {
        mConnectionStatusText.setText(connectionStatusText);
        mConnectionStatusIndicator.setColorFilter(connectionStatusColor);
        mConnectionText.setText(connectionText);
        mConnectionIcon.setImageDrawable(connectionIcon);
    }
}
