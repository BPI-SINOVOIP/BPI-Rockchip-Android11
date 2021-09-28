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

/** A CTS Verifier test case to verify PARKING_BRAKE_ON is implemented correctly.*/
public class ParkingBrakeOnTestActivity extends PassFailButtons.Activity {
    private static final String TAG = ParkingBrakeOnTestActivity.class.getSimpleName();
    private static final int TOTAL_MATCHES_NEEDED_TO_FINISH = 2;
    private Boolean mCurrentParkingBrakeOnValue;
    private TextView mInstructionTextView;
    private TextView mCurrentParkingBrakeOnValueTextView;
    private int mTotalTimesNewValueMatchInstruction = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Setup the UI.
        setContentView(R.layout.parking_brake_on_test);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.parking_brake_on_test, R.string.parking_brake_on_test_desc, -1);
        getPassButton().setEnabled(false);

        mInstructionTextView = (TextView) findViewById(R.id.instruction);
        mInstructionTextView.setText("Waiting to get first PARKING_BRAKE_ON callback");
        mCurrentParkingBrakeOnValueTextView =
            (TextView) findViewById(R.id.current_parking_brake_on_value);


        CarPropertyManager carPropertyManager =
            (CarPropertyManager) Car.createCar(this).getCarManager(Car.PROPERTY_SERVICE);

        if(!carPropertyManager.registerCallback(mCarPropertyEventCallback,
            VehiclePropertyIds.PARKING_BRAKE_ON, CarPropertyManager.SENSOR_RATE_ONCHANGE)) {
            mInstructionTextView.setText("ERROR: Unable to register for PARKING_BRAKE_ON callback");
            Log.e(TAG, "Failed to register callback for PARKING_BRAKE_ON with CarPropertyManager");
        }
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
            Log.i(TAG, "New PARKING_BRAKE_ON value: " + newValue);

            // On the first callback, mCurrentParkingBrakeOnValue will be null, so just save the
            // current value. All other callbacks, check if the PARKING_BRAKE_ON value has switched.
            // If switched, update the count.
            if (mCurrentParkingBrakeOnValue != null &&
                !mCurrentParkingBrakeOnValue.equals(newValue)) {
                mTotalTimesNewValueMatchInstruction++;
            }

            mCurrentParkingBrakeOnValue = newValue;
            mCurrentParkingBrakeOnValueTextView.setText(mCurrentParkingBrakeOnValue.toString());

            // Check if the test is finished. If not finished, update the instructions.
            if(mTotalTimesNewValueMatchInstruction >= TOTAL_MATCHES_NEEDED_TO_FINISH) {
                mInstructionTextView.setText("Test Finished!");
                getPassButton().setEnabled(true);
            } else if(mCurrentParkingBrakeOnValue) {
                mInstructionTextView.setText("Disengage the Parking Brake");
            } else {
                mInstructionTextView.setText("Engage the Parking Brake");
            }
        }

        @Override
        public void onErrorEvent(int propId, int zone) {
            Log.e(TAG, "propId: " + propId + " zone: " + zone);
        }
      };
}
