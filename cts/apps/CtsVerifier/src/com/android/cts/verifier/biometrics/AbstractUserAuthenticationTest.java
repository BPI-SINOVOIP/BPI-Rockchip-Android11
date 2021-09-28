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
import android.hardware.biometrics.BiometricPrompt.CryptoObject;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.security.keystore.KeyProperties;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.util.concurrent.Executor;

/**
 * This is the abstract base class for testing/checking that keys generated via
 * setUserAuthenticationParameters(timeout, CREDENTIAL) can be unlocked (or not) depending on the
 * type of authenticator used. This tests various combinations of
 * {timeout, authenticator, strongbox}. Extending classes currently consist of:
 * {@link UserAuthenticationCredentialCipherTest} for testing {@link javax.crypto.Cipher}.
 */
public abstract class AbstractUserAuthenticationTest extends PassFailButtons.Activity {

    private static final String TAG = "AbstractUserAuthenticationCredentialTest";

    private static final int TIMED_KEY_DURATION = 3;
    private static final byte[] PAYLOAD = new byte[] {1, 2, 3, 4, 5, 6};

    abstract class ExpectedResults {
        abstract boolean shouldCredentialUnlockPerUseKey();
        abstract boolean shouldCredentialUnlockTimedKey();
        abstract boolean shouldBiometricUnlockPerUseKey();
        abstract boolean shouldBiometricUnlockTimedKey();
    }

    /**
     * @return Log tag.
     */
    abstract String getTag();

    abstract int getInstructionsResourceId();

    abstract void createUserAuthenticationKey(String keyName, int timeout, int authType,
            boolean useStrongBox) throws Exception;

    abstract ExpectedResults getExpectedResults();

    /**
     * @return The authenticators allowed to unlock the cryptographic operation. See
     * {@link KeyProperties#AUTH_DEVICE_CREDENTIAL} and {@link KeyProperties#AUTH_BIOMETRIC_STRONG}
     */
    abstract int getKeyAuthenticators();

    /**
     * Due to the differences between auth-per-use operations and time-based operations, the
     * initialization of the keystore operation may be before or after authentication. Initializing
     * the operation will require the extending class to store it somewhere for later use. This
     * cached operation should be cleared after {@link #doKeystoreOperation(byte[])} is invoked.
     */
    abstract void initializeKeystoreOperation(String keyName) throws Exception;

    /**
     * This method is used only for auth-per-use keys. This requires the keystore operation to
     * already be initialized and cached within the extending class.
     */
    abstract CryptoObject getCryptoObject();

    /**
     * Attempts to perform the initialized/cached keystore operation. This method must guarantee
     * that the cached operation is null after it's run (both passing and failing cases).
     */
    abstract void doKeystoreOperation(byte[] payload) throws Exception;

    protected final Handler mHandler = new Handler(Looper.getMainLooper());
    protected final Executor mExecutor = mHandler::post;

    private BiometricManager mBiometricManager;

    private Button mCredentialPerUseButton;
    private Button mCredentialTimedButton;
    private Button mBiometricPerUseButton;
    private Button mBiometricTimedButton;
    private Button mCredentialPerUseButton_strongbox;
    private Button mCredentialTimedButton_strongbox;
    private Button mBiometricPerUseButton_strongbox;
    private Button mBiometricTimedButton_strongbox;

    private Button[] mButtons;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.biometric_test_user_authentication_credential_tests);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        mBiometricManager = getSystemService(BiometricManager.class);

        TextView instructionsText = findViewById(R.id.instructions);
        instructionsText.setText(getInstructionsResourceId());

        mCredentialPerUseButton = findViewById(R.id.per_use_auth_with_credential);
        mCredentialTimedButton = findViewById(R.id.duration_auth_with_credential);
        mBiometricPerUseButton = findViewById(R.id.per_use_auth_with_biometric);
        mBiometricTimedButton = findViewById(R.id.duration_auth_with_biometric);
        mCredentialPerUseButton_strongbox
                = findViewById(R.id.per_use_auth_with_credential_strongbox);
        mCredentialTimedButton_strongbox
                = findViewById(R.id.duration_auth_with_credential_strongbox);
        mBiometricPerUseButton_strongbox
                = findViewById(R.id.per_use_auth_with_biometric_strongbox);
        mBiometricTimedButton_strongbox
                = findViewById(R.id.duration_auth_with_biometric_strongbox);

        mButtons = new Button[] {
                mCredentialPerUseButton,
                mCredentialTimedButton,
                mBiometricPerUseButton,
                mBiometricTimedButton,
                mCredentialPerUseButton_strongbox,
                mCredentialTimedButton_strongbox,
                mBiometricPerUseButton_strongbox,
                mBiometricTimedButton_strongbox
        };

        final boolean hasStrongBox = getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_STRONGBOX_KEYSTORE);
        final boolean noStrongBiometricHardware = !hasStrongBiometrics();

        if (!hasStrongBox) {
            mCredentialPerUseButton_strongbox.setVisibility(View.GONE);
            mCredentialTimedButton_strongbox.setVisibility(View.GONE);
            mBiometricPerUseButton_strongbox.setVisibility(View.GONE);
            mBiometricTimedButton_strongbox.setVisibility(View.GONE);
        }

        if (noStrongBiometricHardware) {
            mBiometricPerUseButton.setVisibility(View.GONE);
            mBiometricTimedButton.setVisibility(View.GONE);
            mBiometricPerUseButton_strongbox.setVisibility(View.GONE);
            mBiometricTimedButton_strongbox.setVisibility(View.GONE);
        }

        // No strongbox

        mCredentialPerUseButton.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key1",
                    0 /* timeout */,
                    false /* useStrongBox */,
                    Authenticators.DEVICE_CREDENTIAL,
                    getExpectedResults().shouldCredentialUnlockPerUseKey(),
                    PAYLOAD,
                    mCredentialPerUseButton);
        });

        mCredentialTimedButton.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key2",
                    TIMED_KEY_DURATION /* timeout */,
                    false /* useStrongBox */,
                    Authenticators.DEVICE_CREDENTIAL,
                    getExpectedResults().shouldCredentialUnlockTimedKey(),
                    PAYLOAD,
                    mCredentialTimedButton);
        });

        mBiometricPerUseButton.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key3",
                    0 /* timeout */,
                    false /* useStrongBox */,
                    Authenticators.BIOMETRIC_STRONG,
                    getExpectedResults().shouldBiometricUnlockPerUseKey(),
                    PAYLOAD,
                    mBiometricPerUseButton);
        });

        mBiometricTimedButton.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key4",
                    TIMED_KEY_DURATION /* timeout */,
                    false /* useStrongBox */,
                    Authenticators.BIOMETRIC_STRONG,
                    getExpectedResults().shouldBiometricUnlockTimedKey(),
                    PAYLOAD,
                    mBiometricTimedButton);
        });

        // Strongbox

        mCredentialPerUseButton_strongbox.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key5",
                    0 /* timeout */,
                    true /* useStrongBox */,
                    Authenticators.DEVICE_CREDENTIAL,
                    getExpectedResults().shouldCredentialUnlockPerUseKey(),
                    PAYLOAD,
                    mCredentialPerUseButton_strongbox);
        });

        mCredentialTimedButton_strongbox.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key6",
                    TIMED_KEY_DURATION /* timeout */,
                    true /* useStrongBox */,
                    Authenticators.DEVICE_CREDENTIAL,
                    getExpectedResults().shouldCredentialUnlockTimedKey(),
                    PAYLOAD,
                    mCredentialTimedButton_strongbox);
        });

        mBiometricPerUseButton_strongbox.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key7",
                    0 /* timeout */,
                    true /* useStrongBox */,
                    Authenticators.BIOMETRIC_STRONG,
                    getExpectedResults().shouldBiometricUnlockPerUseKey(),
                    PAYLOAD,
                    mBiometricPerUseButton_strongbox);
        });

        mBiometricTimedButton_strongbox.setOnClickListener((view) -> {
            testCredentialBoundEncryption("key8",
                    TIMED_KEY_DURATION /* timeout */,
                    true /* useStrongBox */,
                    Authenticators.BIOMETRIC_STRONG,
                    getExpectedResults().shouldBiometricUnlockTimedKey(),
                    PAYLOAD,
                    mBiometricTimedButton_strongbox);
        });
    }

    @Override
    protected void onPause() {
        super.onPause();

        if (!getPassButton().isEnabled()) {
            // This test is affected by PIN/Pattern/Password authentication. So, do not allow
            // the test to complete if the user leaves the app (lockscreen, etc will affect this
            // test).
            showToastAndLog("This test must be completed without pausing the app");
            finish();
        }
    }

    private void testCredentialBoundEncryption(String keyName, int timeout, boolean useStrongBox,
            int allowedAuthenticators, boolean shouldKeyBeUsable, byte[] payload,
            Button testButton) {

        final boolean requiresCryptoObject = timeout == 0;

        final int canAuthenticate = mBiometricManager.canAuthenticate(allowedAuthenticators);
        if (canAuthenticate != BiometricManager.BIOMETRIC_SUCCESS) {
            showToastAndLog("Please ensure you can authenticate with the following authenticators: "
                    + allowedAuthenticators + " Result: " + canAuthenticate);
            return;
        }

        try {
            if (mBiometricManager.canAuthenticate(allowedAuthenticators)
                    != BiometricManager.BIOMETRIC_SUCCESS) {
                showToastAndLog("Please ensure you have the authenticator combination set up: "
                        + allowedAuthenticators);
                return;
            }

            try {
                createUserAuthenticationKey(keyName, timeout, getKeyAuthenticators(), useStrongBox);
            } catch (Exception e) {
                final boolean shouldKeyBeGeneratable;
                if (getKeyAuthenticators() == KeyProperties.AUTH_BIOMETRIC_STRONG &&
                        !hasStrongBiometrics()) {
                    shouldKeyBeGeneratable = false;
                } else {
                    shouldKeyBeGeneratable = true;
                }

                if (!shouldKeyBeGeneratable) {
                    Log.d(TAG, "Key not generatable (expected). Exception: " + e);
                    testButton.setVisibility(View.INVISIBLE);
                    updatePassButton();
                    return;
                } else {
                    throw e;
                }
            }

            CryptoObject crypto;

            // For auth-per-use keys, the keystore operation needs to be initialized before
            // authenticating, so we can wrap it into a CryptoObject. For time-based keys, the
            // keystore operation can only be initialized after authentication has occurred.
            if (requiresCryptoObject) {
                initializeKeystoreOperation(keyName);
                crypto = getCryptoObject();
            } else {
                crypto = null;
            }

            final BiometricPrompt.Builder builder = new BiometricPrompt.Builder(this);
            builder.setTitle("Please authenticate");
            builder.setAllowedAuthenticators(allowedAuthenticators);

            // The BiometricPrompt API requires a negative button if credential is not allowed.
            if ((allowedAuthenticators & Authenticators.DEVICE_CREDENTIAL) == 0) {
                builder.setNegativeButton("Cancel", mExecutor, (dialog, which) -> {
                    // Do nothing
                });
            }

            final AuthenticationCallback callback = new AuthenticationCallback() {
                @Override
                public void onAuthenticationSucceeded(AuthenticationResult result) {
                    // Key generation / initialization can depend on past authentication. Ensure
                    // that the user has not authenticated within n+1 seconds before allowing the
                    // next test to start.
                    disableTestsForFewSeconds();

                    Exception exception = null;
                    boolean keyUsed;
                    try {
                        if (!requiresCryptoObject) {
                            initializeKeystoreOperation(keyName);
                        }

                        doKeystoreOperation(payload);

                        keyUsed = true;
                    } catch (Exception e) {
                        keyUsed = false;
                        exception = e;
                    }

                    if (keyUsed != shouldKeyBeUsable) {
                        showToastAndLog("Test failed. shouldKeyBeUsable: " + shouldKeyBeUsable
                                + " keyUsed: " + keyUsed + " Exception: " + exception);
                        if (exception != null) {
                            exception.printStackTrace();
                        }
                    } else {
                        // Set them to invisible, because for this test, disabled actually means
                        // something else. For the initialization of some keys, its success/failure
                        // can depend on if the user has entered their credential within the last
                        // "n" seconds. Those tests need to be disabled until "n" has passed.
                        testButton.setVisibility(View.INVISIBLE);
                    }
                    updatePassButton();
                }
            };


            final BiometricPrompt prompt = builder.build();

            if (requiresCryptoObject) {
                prompt.authenticate(crypto, new CancellationSignal(), mExecutor, callback);
            } else {
                prompt.authenticate(new CancellationSignal(), mExecutor, callback);
            }

        } catch (Exception e) {
            showToastAndLog("Failed during Crypto test: " + e);
            e.printStackTrace();
        }
    }

    private boolean hasStrongBiometrics() {
        return mBiometricManager.canAuthenticate(Authenticators.BIOMETRIC_STRONG)
                != BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE;
    }

    private void disableTestsForFewSeconds() {
        for (Button b : mButtons) {
            b.setEnabled(false);
        }

        mHandler.postDelayed(() -> {
            for (Button b : mButtons) {
                b.setEnabled(true);
            }
        }, TIMED_KEY_DURATION * 1000 + 1000);
    }

    private void updatePassButton() {
        for (Button b : mButtons) {
            if (b.getVisibility() == View.VISIBLE) {
                return;
            }
        }

        showToastAndLog("All tests passed");
        getPassButton().setEnabled(true);
    }

    private void showToastAndLog(String s) {
        Log.d(getTag(), s);
        Toast.makeText(this, s, Toast.LENGTH_SHORT).show();
    }
}
