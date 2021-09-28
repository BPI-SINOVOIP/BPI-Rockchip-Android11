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

package com.android.cts.verifier.biometrics;

import android.hardware.biometrics.BiometricManager.Authenticators;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;

import com.android.cts.verifier.R;

public class SensorConfigurationTest extends AbstractBaseTest {

    private static final String TAG = "SensorConfigurationTest";

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    protected boolean isOnPauseAllowed() {
        return true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.biometric_test_sensor_configuration);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        final Button button = findViewById(R.id.biometric_test_sensor_configuration_button);
        button.setOnClickListener((view) -> {
            if (isSensorConfigurationValid()) {
                button.setEnabled(false);
                getPassButton().setEnabled(true);
                showToastAndLog("Test passed");
            }
        });
    }

    private boolean isSensorConfigurationValid() {
        final String config[] = getResources()
                .getStringArray(com.android.internal.R.array.config_biometric_sensors);

        // Device configuration should be formatted as "ID:Modality:Strength", where
        // 1) IDs are unique, starting at 0 and increasing by 1 for each new authenticator.
        // 2) Modality as defined in BiometricAuthenticator.java.
        // 3) Strength as defined in BiometricManager.Authenticators.
        // 4) Sensors must be listed in order of decreasing strength.
        // We currently enforce only 1, 3, and 4, since 2 is not a public API.

        int lastId = -1;
        int lastStrength = Authenticators.BIOMETRIC_STRONG;

        for (int i = 0; i < config.length; i++) {
            String[] elems = config[i].split(":");
            final int id = Integer.parseInt(elems[0]);
            final int modality = Integer.parseInt(elems[1]);
            final int strength = Integer.parseInt(elems[2]);

            Log.d(TAG, "Config(" + i + "): " + config[i]);

            if (id != lastId + 1) {
                showToastAndLog("IDs must be monotonically increasing from 0. "
                        + " Id: " + id + " lastId: " + lastId);
                return false;
            }
            lastId++;

            if (strength != Authenticators.BIOMETRIC_STRONG
                    && strength != Authenticators.BIOMETRIC_WEAK
                    && strength != Authenticators.BIOMETRIC_CONVENIENCE) {
                showToastAndLog("Unknown strength: " + strength);
                return false;
            }

            if (strength < lastStrength) {
                // Strengths are already validated above. The bitfield values
                // decrease as strength increases.
                showToastAndLog("Sensors must be listed in decreasing strength");
                return false;
            }
            lastStrength = strength;
        }

        return true;
    }
}
