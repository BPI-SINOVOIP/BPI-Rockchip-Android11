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

package com.android.cts.verifier.battery;

import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.View;

import com.android.cts.verifier.OrderedTestActivity;
import com.android.cts.verifier.R;

public class BatterySaverTestActivity extends OrderedTestActivity {
    private PowerManager mPowerManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setInfoResources(R.string.battery_saver_test, R.string.battery_saver_test_info, -1);

        mPowerManager = getSystemService(PowerManager.class);
    }

    @Override
    protected OrderedTestActivity.Test[] getTests() {
        return new Test[]{
                // Confirm a battery exists before proceeding with the rest of the tests,
                mConfirmBatteryExists,
                // Verify device is unplugged.
                mConfirmUnplugged,
                // Verify battery saver is off.
                mConfirmBsOff,
                mEnableBs,
                mDisableBs
        };
    }

    private final Test mConfirmBatteryExists = new Test(
            R.string.battery_saver_test_no_battery_detected) {
        @Override
        protected void run() {
            super.run();

            final Intent batteryInfo = registerReceiver(null, new
                    IntentFilter(Intent.ACTION_BATTERY_CHANGED));

            if (batteryInfo.getBooleanExtra(BatteryManager.EXTRA_PRESENT, true)) {
                succeed();
            } else {
                findViewById(R.id.btn_next).setVisibility(View.GONE);
                // Set both pass and fail to visible in case the device is meant to have a battery.
                findViewById(R.id.pass_button).setVisibility(View.VISIBLE);
                findViewById(R.id.fail_button).setVisibility(View.VISIBLE);
            }
        }
    };

    private final Test mConfirmUnplugged = new Test(R.string.battery_saver_test_unplug) {
        @Override
        protected void run() {
            super.run();

            if (!isPluggedIn()) {
                succeed();
            }
        }

        @Override
        protected void onNextClick() {
            if (isPluggedIn()) {
                error(R.string.battery_saver_test_device_plugged_in);
            } else {
                succeed();
            }
        }

        private boolean isPluggedIn() {
            Intent intent = registerReceiver(null,
                    new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
            return plugged == BatteryManager.BATTERY_PLUGGED_AC
                    || plugged == BatteryManager.BATTERY_PLUGGED_USB
                    || plugged == BatteryManager.BATTERY_PLUGGED_WIRELESS;
        }
    };

    private final Test mConfirmBsOff = new Test(R.string.battery_saver_test_start_turn_bs_off) {
        @Override
        protected void run() {
            super.run();

            if (!mPowerManager.isPowerSaveMode()) {
                succeed();
            }
        }

        @Override
        protected void onNextClick() {
            if (mPowerManager.isPowerSaveMode()) {
                error(R.string.battery_saver_test_bs_on);
            } else {
                succeed();
            }
        }
    };

    private final Test mEnableBs = new Test(R.string.battery_saver_test_enable_bs) {
        @Override
        protected void onNextClick() {
            if (mPowerManager.isPowerSaveMode()) {
                succeed();
            } else {
                error(R.string.battery_saver_test_bs_off);
            }
        }
    };

    private final Test mDisableBs = new Test(R.string.battery_saver_test_disable_bs) {
        @Override
        protected void onNextClick() {
            if (!mPowerManager.isPowerSaveMode()) {
                succeed();
            } else {
                error(R.string.battery_saver_test_bs_on);
            }
        }
    };
}
