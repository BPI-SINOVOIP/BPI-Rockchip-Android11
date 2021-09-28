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

import android.content.pm.PackageManager;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricManager.Authenticators;
import android.hardware.biometrics.BiometricPrompt;
import android.hardware.biometrics.BiometricPrompt.AuthenticationCallback;
import android.hardware.biometrics.BiometricPrompt.AuthenticationResult;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.security.keystore.UserNotAuthenticatedException;
import android.util.Log;
import android.view.View;
import android.widget.Button;

import com.android.cts.verifier.R;

import java.util.Arrays;
import java.util.concurrent.Executor;

import javax.crypto.Cipher;

public class CredentialCryptoTests extends AbstractBaseTest {
    private static final String TAG = "CredentialCryptoTests";

    private static final String KEY_NAME_STRONGBOX = "credential_timed_key_strongbox";
    private static final String KEY_NAME_NO_STRONGBOX = "credential_timed_key_no_strongbox";
    private static final byte[] SECRET_BYTE_ARRAY = new byte[] {1, 2, 3, 4, 5, 6};

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Executor mExecutor = mHandler::post;

    private Button mCredentialTimedKeyStrongBoxButton;
    private Button mCredentialTimedKeyNoStrongBoxButton;

    private boolean mCredentialTimedKeyStrongBoxPassed;
    private boolean mCredentialTimedKeyNoStrongBoxPassed;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    protected boolean isOnPauseAllowed() {
        // Tests haven't started yet
        return !mCredentialTimedKeyStrongBoxPassed && !mCredentialTimedKeyNoStrongBoxPassed;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.biometric_test_credential_crypto_tests);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        final BiometricManager biometricManager = getSystemService(BiometricManager.class);

        mCredentialTimedKeyStrongBoxButton =
                findViewById(R.id.credential_timed_key_strongbox_button);
        mCredentialTimedKeyNoStrongBoxButton =
                findViewById(R.id.credential_timed_key_no_strongbox_button);

        final boolean hasStrongBox = getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
        if (!hasStrongBox) {
            Log.d(TAG, "No strongbox");
            mCredentialTimedKeyStrongBoxButton.setVisibility(View.GONE);
            mCredentialTimedKeyStrongBoxPassed = true;
        }

        mCredentialTimedKeyStrongBoxButton.setOnClickListener((view) -> {
            // Ensure credential is set up
            final int result = biometricManager.canAuthenticate(Authenticators.DEVICE_CREDENTIAL);
            if (result != BiometricManager.BIOMETRIC_SUCCESS) {
                showToastAndLog("Unexpected result: " + result + ", please ensure that you have"
                        + " set up a PIN/Pattern/Password");
                return;
            }

            testTimedKey(KEY_NAME_STRONGBOX, true /* useStrongBox */);
        });

        mCredentialTimedKeyNoStrongBoxButton.setOnClickListener((view) -> {
            // Ensure credential is set up
            final int result = biometricManager.canAuthenticate(Authenticators.DEVICE_CREDENTIAL);
            if (result != BiometricManager.BIOMETRIC_SUCCESS) {
                showToastAndLog("Unexpected result: " + result + ", please ensure that you have"
                        + " set up a PIN/Pattern/Password");
                return;
            }

            testTimedKey(KEY_NAME_NO_STRONGBOX, false /* useStrongBox */);
        });
    }

    private void testTimedKey(String keyName, boolean useStrongBox) {
        // Create a key that's usable for 5s after credential has been authenticated
        try {
            Utils.createTimeBoundSecretKey_deprecated(keyName, useStrongBox /* useStrongBox */);
        } catch (Exception e) {
            showToastAndLog("Unable to create key: " + e);
            return;
        }

        // Try to use the key before it's unlocked. Should receive UserNotAuthenticatedException
        try {
            final Cipher cipher = Utils.initCipher(keyName);
        } catch (UserNotAuthenticatedException e) {
            // Expected
            Log.d(TAG, "UserNotAuthenticated (expected)");
        } catch (Exception e) {
            showToastAndLog("Unexpected exception: " + e);
        }

        // Authenticate with credential
        final BiometricPrompt.Builder builder = new BiometricPrompt.Builder(this);
        builder.setTitle("Please authenticate");
        builder.setAllowedAuthenticators(Authenticators.DEVICE_CREDENTIAL);
        final BiometricPrompt prompt = builder.build();
        prompt.authenticate(new CancellationSignal(), mExecutor, new AuthenticationCallback() {
            @Override
            public void onAuthenticationSucceeded(AuthenticationResult result) {
                final int authenticationType = result.getAuthenticationType();
                if (authenticationType
                        != BiometricPrompt.AUTHENTICATION_RESULT_TYPE_DEVICE_CREDENTIAL) {
                    showToastAndLog("Unexpected authentication type: " + authenticationType);
                    return;
                }
                // Try to encrypt something with the key
                try {
                    final Cipher cipher = Utils.initCipher(keyName);
                    byte[] encrypted = Utils.doEncrypt(cipher, SECRET_BYTE_ARRAY);
                    showToastAndLog("Encrypted: " + Arrays.toString(encrypted));

                    if (useStrongBox) {
                        mCredentialTimedKeyStrongBoxPassed = true;
                        mCredentialTimedKeyStrongBoxButton.setEnabled(false);
                    } else {
                        mCredentialTimedKeyNoStrongBoxPassed = true;
                        mCredentialTimedKeyNoStrongBoxButton.setEnabled(false);
                    }
                    updateButton();
                } catch (Exception e) {
                    showToastAndLog("Unable to encrypt: " + e);
                }
            }
        });
    }

    private void updateButton() {
        if (mCredentialTimedKeyStrongBoxPassed && mCredentialTimedKeyNoStrongBoxPassed) {
            showToastAndLog("All tests passed");
            getPassButton().setEnabled(true);
        }
    }
}
