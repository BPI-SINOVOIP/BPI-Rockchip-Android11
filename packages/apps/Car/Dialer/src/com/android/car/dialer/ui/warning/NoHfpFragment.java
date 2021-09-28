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

package com.android.car.dialer.ui.warning;

import android.car.Car;
import android.car.content.pm.CarPackageManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProvider;

import com.android.car.apps.common.UxrButton;
import com.android.car.apps.common.util.CarPackageManagerUtils;
import com.android.car.apps.common.util.ViewUtils;
import com.android.car.dialer.R;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.ui.dialpad.DialpadFragment;

/**
 * A fragment that informs the user that there is no bluetooth device attached that can make
 * phone calls.
 */
public class NoHfpFragment extends Fragment {
    private static final String Bluetooth_Setting_ACTION = "android.settings.BLUETOOTH_SETTINGS";
    private static final String Bluetooth_Setting_CATEGORY = "android.intent.category.DEFAULT";

    private TextView mErrorMessageView;

    private Car mCar;
    private CarPackageManager mCarPackageManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mCar = Car.createCar(getActivity());
        mCarPackageManager = (CarPackageManager) mCar.getCarManager(Car.PACKAGE_SERVICE);
    }

    @Override
    public void onDestroy() {
        mCar.disconnect();
        super.onDestroy();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.no_hfp, container, false);
        mErrorMessageView = view.findViewById(R.id.error_string);

        LiveData<String> errorStringLiveData = new ViewModelProvider(getActivity())
                .get(NoHfpViewModel.class)
                .getBluetoothErrorStringLiveData();

        errorStringLiveData.observe(this, error -> {
            // Do not set the NO_BT_ERROR message to avoid UI jankiness
            if (!BluetoothErrorStringLiveData.NO_BT_ERROR.equals(error)) {
                mErrorMessageView.setText(error);
            }
        });

        View emergencyButton = view.findViewById(R.id.emergency_call_button);
        ViewUtils.setVisible(emergencyButton, UiCallManager.get().isEmergencyCallSupported());
        emergencyButton.setOnClickListener(v -> getParentFragmentManager()
                .beginTransaction()
                .replace(android.R.id.content, DialpadFragment.newEmergencyDialpad())
                .addToBackStack(null)
                .commit());

        Intent launchIntent = new Intent();
        launchIntent.setAction(Bluetooth_Setting_ACTION);
        launchIntent.addCategory(Bluetooth_Setting_CATEGORY);

        UxrButton bluetoothButton = view.findViewById(R.id.connect_bluetooth_button);
        boolean isDistractionOptimized = CarPackageManagerUtils.isDistractionOptimized(
                mCarPackageManager, getActivity().getPackageManager(), launchIntent);
        bluetoothButton.setUxRestrictions(isDistractionOptimized
                ? CarUxRestrictions.UX_RESTRICTIONS_BASELINE
                : CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP);
        bluetoothButton.setOnClickListener(v -> startActivity(launchIntent));

        return view;
    }
}
