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

import android.security.keystore.KeyProperties;

import com.android.cts.verifier.R;

public class UserAuthenticationCredentialSignatureTest
        extends AbstractUserAuthenticationSignatureTest {
    private static final String TAG = "UserAuthenticationCredentialSignatureTest";

    @Override
    String getTag() {
        return TAG;
    }

    @Override
    int getInstructionsResourceId() {
        return R.string.biometric_test_set_user_authentication_credential_instructions;
    }

    @Override
    ExpectedResults getExpectedResults() {
        return new ExpectedResults() {
            @Override
            boolean shouldCredentialUnlockPerUseKey() {
                return true;
            }

            @Override
            boolean shouldCredentialUnlockTimedKey() {
                return true;
            }

            @Override
            boolean shouldBiometricUnlockPerUseKey() {
                return false;
            }

            @Override
            boolean shouldBiometricUnlockTimedKey() {
                return false;
            }
        };
    }

    @Override
    int getKeyAuthenticators() {
        return KeyProperties.AUTH_DEVICE_CREDENTIAL;
    }
}
