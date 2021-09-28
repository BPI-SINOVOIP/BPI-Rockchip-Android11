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

package com.android.cts.verifier.car;

import android.car.Car;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;
import android.car.VehicleAreaType;
import android.car.VehiclePropertyIds;
import android.os.Bundle;
import android.widget.TextView;
import android.util.ArraySet;
import android.util.Log;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.util.Arrays;
import java.util.List;

/** A CTS Verifier test case to verify NIGHT_MODE is implemented correctly.*/
public class NightModeTestActivity extends PassFailButtons.Activity {
    private static final String TAG = NightModeTestActivity.class.getSimpleName();
    private static final int TOTAL_MATCHES_NEEDED_TO_FINISH = 2;
    private static final String TOTAL_TIMES_NEW_VALUE_MATCHED_INSTRUCTION =
        "TotalTimesNewValueMatchedInstruction";
    private static final String CURRENT_NIGHT_MODE_VALUE = "CurrentNightModeValue";
    private Boolean mCurrentNightModeValue;
    private TextView mInstructionTextView;
    private TextView mCurrentNightModeValueTextView;
    private int mTotalTimesNewValueMatchedInstruction = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Setup the UI.
        setContentView(R.layout.night_mode_test);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.night_mode_test, R.string.night_mode_test_desc, -1);
        getPassButton().setEnabled(false);

        mInstructionTextView = (TextView) findViewById(R.id.night_mode_instruction);
        mInstructionTextView.setText("Waiting to get first NIGHT_MODE callback from Vehicle HAL");
        mCurrentNightModeValueTextView = (TextView) findViewById(R.id.current_night_mode_value);


        CarPropertyManager carPropertyManager =
            (CarPropertyManager) Car.createCar(this).getCarManager(Car.PROPERTY_SERVICE);

        if(!carPropertyManager.registerCallback(mCarPropertyEventCallback,
            VehiclePropertyIds.NIGHT_MODE, CarPropertyManager.SENSOR_RATE_ONCHANGE)) {
            mInstructionTextView.setText("ERROR: Unable to register for NIGHT_MODE callback");
            Log.e(TAG, "Failed to register callback for NIGHT_MODE with CarPropertyManager");
        }
    }

    // Need to save the state because of the UI Mode switch with the change in the NIGHT_MODE
    // property value.
    @Override
    protected void onSaveInstanceState(final Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(CURRENT_NIGHT_MODE_VALUE, mCurrentNightModeValue);
        outState.putInt(TOTAL_TIMES_NEW_VALUE_MATCHED_INSTRUCTION,
            mTotalTimesNewValueMatchedInstruction);
    }

    @Override
    protected void onRestoreInstanceState(final Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        mCurrentNightModeValue = savedInstanceState.getBoolean(CURRENT_NIGHT_MODE_VALUE);
        mTotalTimesNewValueMatchedInstruction =
            savedInstanceState.getInt(TOTAL_TIMES_NEW_VALUE_MATCHED_INSTRUCTION);
    }

    private final CarPropertyManager.CarPropertyEventCallback mCarPropertyEventCallback =
      new CarPropertyManager.CarPropertyEventCallback() {
        @Override
        public void onChangeEvent(CarPropertyValue value) {
            if(value.getStatus() != CarPropertyValue.STATUS_AVAILABLE) {
                Log.e(TAG, "New CarPropertyValue's status is not available - propId: " +
                    value.getPropertyId() + " status: " + value.getStatus());
                return;
            }

            Boolean newValue = (Boolean) value.getValue();
            Log.i(TAG, "New NIGHT_MODE value: " + newValue);

            // On the first callback, mCurrentNightModeValue will be null, so just save the
            // current value. All other callbacks, check if the NIGHT_MODE value has switched.
            // If switched, update the count.
            if (mCurrentNightModeValue != null && !mCurrentNightModeValue.equals(newValue)) {
                mTotalTimesNewValueMatchedInstruction++;
            }

            mCurrentNightModeValue = newValue;
            mCurrentNightModeValueTextView.setText(mCurrentNightModeValue.toString());

            // Check if the test is finished. If not finished, update the instructions.
            if(mTotalTimesNewValueMatchedInstruction >= TOTAL_MATCHES_NEEDED_TO_FINISH) {
                mInstructionTextView.setText("Test Finished!");
                getPassButton().setEnabled(true);
            } else if(mCurrentNightModeValue) {
                mInstructionTextView.setText("Toggle off NIGHT_MODE through Vehicle HAL");
            } else {
                mInstructionTextView.setText("Toggle on NIGHT_MODE through Vehicle HAL");
            }
        }

        @Override
        public void onErrorEvent(int propId, int zone) {
            Log.e(TAG, "propId: " + propId + " zone: " + zone);
        }
      };
}
