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

import static android.hardware.biometrics.BiometricManager.Authenticators;

import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.hardware.biometrics.BiometricPrompt.AuthenticationCallback;
import android.hardware.biometrics.BiometricPrompt.AuthenticationResult;
import android.hardware.biometrics.BiometricPrompt.CryptoObject;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.provider.Settings;
import android.security.keystore.KeyPermanentlyInvalidatedException;
import android.util.Log;
import android.view.View;
import android.widget.Button;

import com.android.cts.verifier.R;

import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;

/**
 * On devices without a strong biometric, ensure that the
 * {@link BiometricManager#canAuthenticate(int)} returns
 * {@link BiometricManager#BIOMETRIC_ERROR_NO_HARDWARE}
 *
 * Ensure that this result is consistent with the configuration in core/res/res/values/config.xml
 *
 * Ensure that invoking {@link Settings.ACTION_BIOMETRIC_ENROLL} with its corresponding
 * {@link Settings.EXTRA_BIOMETRIC_AUTHENTICATORS_ALLOWED} enrolls a
 * {@link BiometricManager.Authenticators.BIOMETRIC_STRONG} authenticator. This can be done by
 * authenticating a {@link BiometricPrompt.CryptoObject}.
 *
 * Ensure that authentication with a strong biometric unlocks the appropriate keys.
 *
 * Ensure that the BiometricPrompt UI displays all fields in the public API surface.
 */
public class BiometricStrongTests extends AbstractBaseTest {
    private static final String TAG = "BiometricStrongTests";

    private static final String KEY_NAME_STRONGBOX = "key_using_strongbox";
    private static final String KEY_NAME_NO_STRONGBOX = "key_without_strongbox";
    private static final byte[] PAYLOAD = new byte[] {1, 2, 3, 4, 5, 6};

    // TODO: Build these lists in a smarter way. For now, when adding a test to this list, please
    // double check the logic in isOnPauseAllowed()
    private boolean mHasStrongBox;
    private Button mCheckAndEnrollButton;
    private Button mAuthenticateWithoutStrongBoxButton;
    private Button mAuthenticateWithStrongBoxButton;
    private Button mAuthenticateUIButton;
    private Button mAuthenticateCredential1Button; // setDeviceCredentialAllowed(true), biometric
    private Button mAuthenticateCredential2Button; // setDeviceCredentialAllowed(true), credential
    private Button mAuthenticateCredential3Button; // setAllowedAuthenticators(CREDENTIAL|BIOMETRIC)
    private Button mCheckInvalidInputsButton;
    private Button mRejectThenAuthenticateButton;
    private Button mNegativeButtonButton;
    private Button mCancellationButton;
    private Button mKeyInvalidatedButton;

    private boolean mAuthenticateWithoutStrongBoxPassed;
    private boolean mAuthenticateWithStrongBoxPassed;
    private boolean mAuthenticateUIPassed;
    private boolean mAuthenticateCredential1Passed;
    private boolean mAuthenticateCredential2Passed;
    private boolean mAuthenticateCredential3Passed;
    private boolean mCheckInvalidInputsPassed;
    private boolean mRejectThenAuthenticatePassed;
    private boolean mNegativeButtonPassed;
    private boolean mCancellationButtonPassed;
    private boolean mKeyInvalidatedStrongboxPassed;
    private boolean mKeyInvalidatedNoStrongboxPassed;

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    protected void onBiometricEnrollFinished() {
        final int biometricStatus =
                mBiometricManager.canAuthenticate(Authenticators.BIOMETRIC_STRONG);
        if (biometricStatus == BiometricManager.BIOMETRIC_SUCCESS) {
            showToastAndLog("Successfully enrolled, please continue the test");
            mCheckAndEnrollButton.setEnabled(false);
            mAuthenticateWithoutStrongBoxButton.setEnabled(true);
            mAuthenticateWithStrongBoxButton.setEnabled(true);
            mAuthenticateUIButton.setEnabled(true);
            mAuthenticateCredential1Button.setEnabled(true);
            mAuthenticateCredential2Button.setEnabled(true);
            mAuthenticateCredential3Button.setEnabled(true);
            mCheckInvalidInputsButton.setEnabled(true);
            mRejectThenAuthenticateButton.setEnabled(true);
            mNegativeButtonButton.setEnabled(true);
            mCancellationButton.setEnabled(true);
        } else {
            showToastAndLog("Unexpected result after enrollment: " + biometricStatus);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.biometric_test_strong_tests);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        mCheckAndEnrollButton = findViewById(R.id.check_and_enroll_button);
        mAuthenticateWithoutStrongBoxButton = findViewById(R.id.authenticate_no_strongbox_button);
        mAuthenticateWithStrongBoxButton = findViewById(R.id.authenticate_strongbox_button);
        mAuthenticateUIButton = findViewById(R.id.authenticate_ui_button);
        mAuthenticateCredential1Button = findViewById(
                R.id.authenticate_credential_setDeviceCredentialAllowed_biometric_button);
        mAuthenticateCredential2Button = findViewById(
                R.id.authenticate_credential_setDeviceCredentialAllowed_credential_button);
        mAuthenticateCredential3Button = findViewById(
                R.id.authenticate_credential_setAllowedAuthenticators_credential_button);
        mCheckInvalidInputsButton = findViewById(R.id.authenticate_invalid_inputs);
        mRejectThenAuthenticateButton = findViewById(R.id.authenticate_reject_first);
        mNegativeButtonButton = findViewById(R.id.authenticate_negative_button_button);
        mCancellationButton = findViewById(R.id.authenticate_cancellation_button);
        mKeyInvalidatedButton = findViewById(R.id.authenticate_key_invalidated_button);

        mHasStrongBox = getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
        if (!mHasStrongBox) {
            Log.d(TAG, "Device does not support StrongBox");
            mAuthenticateWithStrongBoxButton.setVisibility(View.GONE);
            mAuthenticateWithStrongBoxPassed = true;
            mKeyInvalidatedStrongboxPassed = true;
        }

        mCheckAndEnrollButton.setOnClickListener((view) -> {
            checkAndEnroll(mCheckAndEnrollButton, Authenticators.BIOMETRIC_STRONG,
                    new int[] {Authenticators.BIOMETRIC_STRONG});
        });

        mAuthenticateWithoutStrongBoxButton.setOnClickListener((view) -> {
            testBiometricBoundEncryption(KEY_NAME_NO_STRONGBOX, PAYLOAD,
                    false /* useStrongBox */);
        });

        mAuthenticateWithStrongBoxButton.setOnClickListener((view) -> {
            testBiometricBoundEncryption(KEY_NAME_STRONGBOX, PAYLOAD,
                    true /* useStrongBox */);
        });

        mAuthenticateUIButton.setOnClickListener((view) -> {
            final Utils.VerifyRandomContents contents = new Utils.VerifyRandomContents(this) {
                @Override
                void onVerificationSucceeded() {
                    mAuthenticateUIPassed = true;
                    mAuthenticateUIButton.setEnabled(false);
                    updatePassButton();
                }
            };
            testBiometricUI(contents, Authenticators.BIOMETRIC_STRONG);
        });

        mAuthenticateCredential1Button.setOnClickListener((view) -> {
            testSetDeviceCredentialAllowed_biometricAuth(() -> {
                mAuthenticateCredential1Passed = true;
                mAuthenticateCredential1Button.setEnabled(false);
                updatePassButton();
            });
        });

        mAuthenticateCredential2Button.setOnClickListener((view) -> {
            testSetDeviceCredentialAllowed_credentialAuth(() -> {
                mAuthenticateCredential2Passed = true;
                mAuthenticateCredential2Button.setEnabled(false);
                updatePassButton();
            });
        });

        mAuthenticateCredential3Button.setOnClickListener((view) -> {
            testSetAllowedAuthenticators_credentialAndBiometricEnrolled_credentialAuth(() -> {
                mAuthenticateCredential3Passed = true;
                mAuthenticateCredential3Button.setEnabled(false);
                updatePassButton();
            });
        });

        mCheckInvalidInputsButton.setOnClickListener((view) -> {
            testInvalidInputs(() -> {
                mCheckInvalidInputsPassed = true;
                mCheckInvalidInputsButton.setEnabled(false);
                updatePassButton();
            });
        });

        mRejectThenAuthenticateButton.setOnClickListener((view) -> {
            testBiometricRejectDoesNotEndAuthentication(() -> {
                mRejectThenAuthenticatePassed = true;
                mRejectThenAuthenticateButton.setEnabled(false);
                updatePassButton();
            });
        });

        mNegativeButtonButton.setOnClickListener((view) -> {
            testNegativeButtonCallback(Authenticators.BIOMETRIC_STRONG, () -> {
                mNegativeButtonPassed = true;
                mNegativeButtonButton.setEnabled(false);
                updatePassButton();
            });
        });

        mCancellationButton.setOnClickListener((view) -> {
            testCancellationSignal(Authenticators.BIOMETRIC_STRONG, () -> {
                mCancellationButtonPassed = true;
                mCancellationButton.setEnabled(false);
                updatePassButton();
            });
        });

        mKeyInvalidatedButton.setOnClickListener((view) -> {
            Utils.showInstructionDialog(this,
                    R.string.biometric_test_strong_authenticate_invalidated_instruction_title,
                    R.string.biometric_test_strong_authenticate_invalidated_instruction_contents,
                    R.string.biometric_test_strong_authenticate_invalidated_instruction_continue,
                    (dialog, which) -> {
                if (which == DialogInterface.BUTTON_POSITIVE) {
                    // If the device supports StrongBox, check that this key is invalidated.
                    if (mHasStrongBox)
                        if (isKeyInvalidated(KEY_NAME_STRONGBOX)) {
                            mKeyInvalidatedStrongboxPassed = true;
                        } else {
                            showToastAndLog("StrongBox key not invalidated");
                            return;
                        }
                    }

                // Always check that non-StrongBox keys are invalidated.
                if (isKeyInvalidated(KEY_NAME_NO_STRONGBOX)) {
                    mKeyInvalidatedNoStrongboxPassed = true;
                } else {
                    showToastAndLog("Key not invalidated");
                    return;
                }

                mKeyInvalidatedButton.setEnabled(false);
                updatePassButton();
            });
        });
    }

    @Override
    protected boolean isOnPauseAllowed() {
        // Test hasn't started yet, user may need to go to Settings to remove enrollments
        if (mCheckAndEnrollButton.isEnabled()) {
            return true;
        }

        // Key invalidation test is currently the last test. Thus, if every other test is currently
        // completed, let's allow onPause (allow tester to go into settings multiple times if
        // needed).
        if (mAuthenticateWithoutStrongBoxPassed && mAuthenticateWithStrongBoxPassed
                && mAuthenticateUIPassed && mAuthenticateCredential1Passed
                && mAuthenticateCredential2Passed && mAuthenticateCredential3Passed
                && mCheckInvalidInputsPassed && mRejectThenAuthenticatePassed
                && mNegativeButtonPassed && mCancellationButtonPassed) {
            return true;
        }

        if (mCurrentlyEnrolling) {
            return true;
        }

        return false;
    }

    private boolean isKeyInvalidated(String keyName) {
        try {
            Utils.initCipher(keyName);
        } catch (KeyPermanentlyInvalidatedException e) {
            return true;
        } catch (Exception e) {
            showToastAndLog("Unexpected exception: " + e);
        }
        return false;
    }

    private void testBiometricBoundEncryption(String keyName, byte[] secret, boolean useStrongBox) {
        try {
            // Create the biometric-bound key
            Utils.createBiometricBoundKey(keyName, useStrongBox);

            // Initialize a cipher and try to use it before a biometric has been authenticated
            Cipher tryUseBeforeAuthCipher = Utils.initCipher(keyName);

            try {
                byte[] encrypted = Utils.doEncrypt(tryUseBeforeAuthCipher, secret);
                showToastAndLog("Should not be able to encrypt prior to authenticating: "
                        + Arrays.toString(encrypted));
                return;
            } catch (IllegalBlockSizeException e) {
                // Normal, user has not authenticated yet
                Log.d(TAG, "Exception before authentication has occurred: " + e);
            }

            // Initialize a cipher and try to use it after a biometric has been authenticated
            final Cipher tryUseAfterAuthCipher = Utils.initCipher(keyName);
            CryptoObject crypto = new CryptoObject(tryUseAfterAuthCipher);

            final BiometricPrompt.Builder builder = new BiometricPrompt.Builder(this);
            builder.setTitle("Please authenticate");
            builder.setAllowedAuthenticators(Authenticators.BIOMETRIC_STRONG);
            builder.setNegativeButton("Cancel", mExecutor, (dialog, which) -> {
                // Do nothing
            });
            final BiometricPrompt prompt = builder.build();
            prompt.authenticate(crypto, new CancellationSignal(), mExecutor,
                    new AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(AuthenticationResult result) {
                            try {
                                final int authenticationType = result.getAuthenticationType();
                                if (authenticationType
                                        != BiometricPrompt.AUTHENTICATION_RESULT_TYPE_BIOMETRIC) {
                                    showToastAndLog("Unexpected authenticationType: "
                                            + authenticationType);
                                    return;
                                }

                                byte[] encrypted = Utils.doEncrypt(tryUseAfterAuthCipher,
                                        secret);
                                showToastAndLog("Encrypted payload: " + Arrays.toString(encrypted)
                                        + ", please run the next test");
                                if (useStrongBox) {
                                    mAuthenticateWithStrongBoxPassed = true;
                                    mAuthenticateWithStrongBoxButton.setEnabled(false);
                                } else {
                                    mAuthenticateWithoutStrongBoxPassed = true;
                                    mAuthenticateWithoutStrongBoxButton.setEnabled(false);
                                }
                                updatePassButton();
                            } catch (Exception e) {
                                showToastAndLog("Failed to encrypt after biometric was"
                                        + "authenticated: " + e);
                            }
                        }
                    });
        } catch (Exception e) {
            showToastAndLog("Failed during Crypto test: " + e);
        }
    }

    private void updatePassButton() {
        if (mAuthenticateWithoutStrongBoxPassed && mAuthenticateWithStrongBoxPassed
                && mAuthenticateUIPassed && mAuthenticateCredential1Passed
                && mAuthenticateCredential2Passed && mAuthenticateCredential3Passed
                && mCheckInvalidInputsPassed && mRejectThenAuthenticatePassed
                && mNegativeButtonPassed && mCancellationButtonPassed) {

            if (!mKeyInvalidatedStrongboxPassed || !mKeyInvalidatedNoStrongboxPassed) {
                mKeyInvalidatedButton.setEnabled(true);
            }

            if (mKeyInvalidatedStrongboxPassed && mKeyInvalidatedNoStrongboxPassed) {
                showToastAndLog("All tests passed");
                getPassButton().setEnabled(true);
            }
        }
    }
}
