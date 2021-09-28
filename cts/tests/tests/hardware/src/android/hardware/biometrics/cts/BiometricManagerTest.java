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

package android.hardware.biometrics.cts;

import static org.junit.Assert.assertNotEquals;

import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricManager.Authenticators;
import android.platform.test.annotations.Presubmit;
import android.test.AndroidTestCase;

/**
 * Basic test cases for BiometricManager. See the manual biometric tests in CtsVerifier for a more
 * comprehensive test suite.
 */
public class BiometricManagerTest extends AndroidTestCase {

    private BiometricManager mBiometricManager;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mBiometricManager = getContext().getSystemService(BiometricManager.class);
    }

    @Presubmit
    public void test_canAuthenticate() {

        assertNotEquals("Device should not have any biometrics enrolled",
                mBiometricManager.canAuthenticate(), BiometricManager.BIOMETRIC_SUCCESS);

        assertNotEquals("Device should not have any biometrics enrolled",
                mBiometricManager.canAuthenticate(
                        Authenticators.DEVICE_CREDENTIAL | Authenticators.BIOMETRIC_WEAK),
                BiometricManager.BIOMETRIC_SUCCESS);
    }
}
