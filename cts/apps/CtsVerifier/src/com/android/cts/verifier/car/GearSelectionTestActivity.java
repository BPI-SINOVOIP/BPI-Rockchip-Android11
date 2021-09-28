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
import android.car.VehicleGear;
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

/** A CTS Verifier test case to verify GEAR_SELECTION is implemented correctly.*/
public class GearSelectionTestActivity extends PassFailButtons.Activity {
    private static final String TAG = GearSelectionTestActivity.class.getSimpleName();
    private List<Integer> mSupportedGears;
    private int mGearsAchievedCount = 0;
    private TextView mExpectedGearSelectionTextView;
    private TextView mCurrentGearSelectionTextView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Setup the UI.
        setContentView(R.layout.gear_selection_test);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.gear_selection_test, R.string.gear_selection_test_desc, -1);
        getPassButton().setEnabled(false);

        mExpectedGearSelectionTextView = (TextView) findViewById(R.id.expected_gear_selection);
        mCurrentGearSelectionTextView = (TextView) findViewById(R.id.current_gear_selection);

        CarPropertyManager carPropertyManager =
            (CarPropertyManager) Car.createCar(this).getCarManager(Car.PROPERTY_SERVICE);

        // TODO(b/138961351): Verify test works on manual transmission.
        mSupportedGears = carPropertyManager.getPropertyList(new ArraySet<>(Arrays.asList(new
                Integer[]{VehiclePropertyIds.GEAR_SELECTION}))).get(0).getConfigArray();

        if(mSupportedGears.size() != 0){
          Log.i(TAG, "New Expected Gear: " + VehicleGear.toString(mSupportedGears.get(0)));
          mExpectedGearSelectionTextView.setText(VehicleGear.toString(mSupportedGears.get(0)));
        } else {
          Log.e(TAG, "No gears specified in the config array of GEAR_SELECTION property");
          mExpectedGearSelectionTextView.setText("ERROR");
        }

        if(!carPropertyManager.registerCallback(mCarPropertyEventCallback,
            VehiclePropertyIds.GEAR_SELECTION, CarPropertyManager.SENSOR_RATE_ONCHANGE)) {
          Log.e(TAG, "Failed to register callback for GEAR_SELECTION with CarPropertyManager");
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
            Integer newGearSelection = (Integer) value.getValue();
            mCurrentGearSelectionTextView.setText(VehicleGear.toString(newGearSelection));
            Log.i(TAG, "New Gear Selection: " + VehicleGear.toString(newGearSelection));

            if (mSupportedGears.size() == 0) {
                Log.e(TAG, "No gears specified in the config array of GEAR_SELECTION property");
                return;
            }

            // Check to see if new gear matches the expected gear.
            if(newGearSelection == mSupportedGears.get(mGearsAchievedCount)) {
                mGearsAchievedCount++;
                Log.i(TAG, "Matched gear: " + VehicleGear.toString(newGearSelection));
                // Check to see if the test is finished.
                if (mGearsAchievedCount >= mSupportedGears.size()) {
                    mExpectedGearSelectionTextView.setText("Finished");
                    getPassButton().setEnabled(true);
                    Log.i(TAG, "Finished Test");
                } else {
                    // Test is not finished so update the expected gear.
                    mExpectedGearSelectionTextView.setText(
                        VehicleGear.toString(mSupportedGears.get(mGearsAchievedCount)));
                    Log.i(TAG, "New Expected Gear: " + 
                        VehicleGear.toString(mSupportedGears.get(mGearsAchievedCount)));
                }
            }
        }

        @Override
        public void onErrorEvent(int propId, int zone) {
            Log.e(TAG, "propId: " + propId + " zone: " + zone);
        }
      };
}
