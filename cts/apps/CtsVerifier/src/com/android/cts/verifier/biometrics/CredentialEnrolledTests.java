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

import android.content.Intent;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricManager.Authenticators;
import android.hardware.biometrics.BiometricPrompt;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.widget.Button;

import com.android.cts.verifier.R;

import java.util.concurrent.Executor;

/**
 * This test checks that when a credential is enrolled, and biometrics are not enrolled,
 * BiometricManager and BiometricPrompt receive the correct results.
 */
public class CredentialEnrolledTests extends AbstractBaseTest {
    private static final String TAG = "CredentialEnrolledTests";

    private static final int REQUEST_ENROLL = 1;

    private Button mEnrollButton;
    private Button mBiometricManagerButton;
    private Button mBPSetAllowedAuthenticatorsButton;
    private Button mBPSetDeviceCredentialAllowedButton;
    private Button mCancellationButton;

    private boolean mEnrollPass;
    private boolean mBiometricManagerPass;
    private boolean mBiometricPromptSetAllowedAuthenticatorsPass;
    private boolean mBiometricPromptSetDeviceCredentialAllowedPass;
    private boolean mCancellationPass;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Executor mExecutor = mHandler::post;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.biometric_test_credential_enrolled_tests);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        final BiometricManager bm = getSystemService(BiometricManager.class);

        mEnrollButton = findViewById(R.id.enroll_credential_button);
        mEnrollButton.setOnClickListener((view) -> {
            final int biometricResult = bm.canAuthenticate(Authenticators.DEVICE_CREDENTIAL);
            if (biometricResult != BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED) {
                showToastAndLog("Please ensure you do not have a PIN/Pattern/Password set");
                return;
            }

            requestCredentialEnrollment(REQUEST_ENROLL);
        });

        // Test BiometricManager#canAuthenticate(DEVICE_CREDENTIAL)
        mBiometricManagerButton = findViewById(R.id.bm_button);
        mBiometricManagerButton.setOnClickListener((view) -> {

            final int biometricResult = bm.canAuthenticate(Authenticators.BIOMETRIC_WEAK);
            switch (biometricResult) {
                case BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE:
                case BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED:
                    // OK
                    break;
                case BiometricManager.BIOMETRIC_SUCCESS:
                    showToastAndLog("Unexpected result: " + biometricResult +
                            ". Please make sure the device does not have a biometric enrolled");
                    return;
                default:
                    showToastAndLog("Unexpected result: " + biometricResult);
                    return;
            }

            final int credentialResult = bm.canAuthenticate(Authenticators.DEVICE_CREDENTIAL);
            if (credentialResult == BiometricManager.BIOMETRIC_SUCCESS) {
                mBiometricManagerButton.setEnabled(false);
                mBiometricManagerPass = true;
                updatePassButton();
            } else {
                showToastAndLog("Unexpected result: " + credentialResult
                        + ". Please make sure the device"
                        + " has a PIN/Pattern/Password set");
            }
        });

        // Test setAllowedAuthenticators(DEVICE_CREDENTIAL)
        mBPSetAllowedAuthenticatorsButton = findViewById(R.id.setAllowedAuthenticators_button);
        mBPSetAllowedAuthenticatorsButton.setOnClickListener((view) -> {
            BiometricPrompt.Builder builder = new BiometricPrompt.Builder(this);
            builder.setTitle("Title");
            builder.setSubtitle("Subtitle");
            builder.setDescription("Description");
            builder.setAllowedAuthenticators(Authenticators.DEVICE_CREDENTIAL);
            BiometricPrompt bp = builder.build();
            bp.authenticate(new CancellationSignal(), mExecutor,
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(
                                BiometricPrompt.AuthenticationResult result) {
                            final int authenticator = result.getAuthenticationType();
                            if (authenticator ==
                                    BiometricPrompt.AUTHENTICATION_RESULT_TYPE_DEVICE_CREDENTIAL) {
                                mBPSetAllowedAuthenticatorsButton.setEnabled(false);
                                mBiometricPromptSetAllowedAuthenticatorsPass = true;
                                updatePassButton();
                            } else {
                                showToastAndLog("Unexpected authenticator: " + authenticator);
                            }
                        }

                        @Override
                        public void onAuthenticationError(int errorCode, CharSequence errString) {
                            showToastAndLog("Unexpected error: " + errorCode + ", " + errString);
                        }
                    });
        });

        // Test setDeviceCredentialAllowed(true)
        mBPSetDeviceCredentialAllowedButton = findViewById(R.id.setDeviceCredentialAllowed_button);
        mBPSetDeviceCredentialAllowedButton.setOnClickListener((view) -> {
            BiometricPrompt.Builder builder = new BiometricPrompt.Builder(this);
            builder.setTitle("Title");
            builder.setSubtitle("Subtitle");
            builder.setDescription("Description");
            builder.setDeviceCredentialAllowed(true);
            BiometricPrompt bp = builder.build();
            bp.authenticate(new CancellationSignal(), mExecutor,
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(
                                BiometricPrompt.AuthenticationResult result) {
                            final int authenticator = result.getAuthenticationType();
                            if (authenticator ==
                                    BiometricPrompt.AUTHENTICATION_RESULT_TYPE_DEVICE_CREDENTIAL) {
                                mBPSetDeviceCredentialAllowedButton.setEnabled(false);
                                mBiometricPromptSetDeviceCredentialAllowedPass = true;
                                updatePassButton();
                            } else {
                                showToastAndLog("Unexpected authenticator: " + authenticator
                                        + ". Please ensure the device does not have a biometric"
                                        + " enrolled.");
                            }
                        }

                        @Override
                        public void onAuthenticationError(int errorCode, CharSequence errString) {
                            showToastAndLog("Unexpected error: " + errorCode + ", " + errString);
                        }
                    });
        });

        mCancellationButton = findViewById(R.id.authenticate_cancellation_button);
        mCancellationButton.setOnClickListener((view) -> {
           testCancellationSignal(Authenticators.DEVICE_CREDENTIAL, () -> {
               mCancellationButton.setEnabled(false);
               mCancellationPass = true;
               updatePassButton();
           });
        });
    }

    @Override
    protected boolean isOnPauseAllowed() {
        // Allow user to go to Settings, etc to figure out why this test isn't passing.
        return !mBiometricManagerPass;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_ENROLL) {
            final BiometricManager bm = getSystemService(BiometricManager.class);
            final int result = bm.canAuthenticate(Authenticators.DEVICE_CREDENTIAL);
            if (result == BiometricManager.BIOMETRIC_SUCCESS) {
                mEnrollPass = true;
                mEnrollButton.setEnabled(false);
                mBiometricManagerButton.setEnabled(true);
                mBPSetAllowedAuthenticatorsButton.setEnabled(true);
                mBPSetDeviceCredentialAllowedButton.setEnabled(true);
                mCancellationButton.setEnabled(true);
            } else {
                showToastAndLog("Unexpected result: " + result + ". Please ensure that tapping"
                        + " the button sends you to credential enrollment, and that you have"
                        + " enrolled a credential.");
            }
        }
    }

    private void requestCredentialEnrollment(int requestCode) {
        final Intent enrollIntent = new Intent(Settings.ACTION_BIOMETRIC_ENROLL);
        enrollIntent.putExtra(Settings.EXTRA_BIOMETRIC_AUTHENTICATORS_ALLOWED,
                Authenticators.DEVICE_CREDENTIAL);

        startActivityForResult(enrollIntent, requestCode);
    }

    private void updatePassButton() {
        if (mEnrollPass && mBiometricManagerPass && mBiometricPromptSetAllowedAuthenticatorsPass
                && mBiometricPromptSetDeviceCredentialAllowedPass && mCancellationPass) {
            showToastAndLog("All tests passed");
            getPassButton().setEnabled(true);
        }
    }
}
