/**
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

package com.android.car.dialer.ui.settings;

import android.os.Bundle;

import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProvider;

import com.android.car.dialer.log.L;

/**
 * Activity for settings in Dialer
 */
public class DialerSettingsActivity extends FragmentActivity {
    private static final String TAG = "CD.SettingsActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        L.d(TAG, "onCreate");

        LiveData<Boolean> hasHfpDeviceConnectedLiveData = new ViewModelProvider(this)
                .get(DialerSettingsViewModel.class)
                .hasHfpDeviceConnected();
        hasHfpDeviceConnectedLiveData.observe(this, hasHfpDeviceConnected -> {
            if (!Boolean.TRUE.equals(hasHfpDeviceConnected)) {
                // Simply finishes settings activity and returns to main activity to show error.
                finish();
            }
        });

        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                    .replace(android.R.id.content, new DialerSettingsFragment())
                    .commit();
        }
    }
}
