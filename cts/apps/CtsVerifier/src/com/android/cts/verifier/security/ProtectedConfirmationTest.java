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
 * limitations under the License
 */

package com.android.cts.verifier.security;

import android.content.pm.PackageManager;
import android.icu.util.Calendar;
import android.os.Bundle;
import android.security.ConfirmationAlreadyPresentingException;
import android.security.ConfirmationCallback;
import android.security.ConfirmationNotAvailableException;
import android.security.ConfirmationPrompt;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.Toast;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.Signature;
import java.security.SignatureException;
import java.security.UnrecoverableEntryException;
import java.security.cert.CertificateException;
import java.util.Date;

public class ProtectedConfirmationTest extends PassFailButtons.Activity {

    /**
     * Alias for our key in the Android Key Store.
     */
    private static final String KEY_NAME = "my_confirmation_key";
    private boolean teeTestSuccess = false;
    private boolean strongboxTestSuccess = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.sec_protected_confirmation_main);
        setPassFailButtonClickListeners();

        boolean protectedConfirmationSupported =
                ConfirmationPrompt.isSupported(getApplicationContext());
        if (protectedConfirmationSupported) {
            setInfoResources(R.string.sec_protected_confirmation_test,
                    R.string.sec_protected_confirmation_test_info, -1);
            getPassButton().setEnabled(false);
        } else {
            setInfoResources(R.string.sec_protected_confirmation_not_supported_title,
                    R.string.sec_protected_confirmation_not_supported_info, -1);
            getPassButton().setEnabled(true);
            return;
        }
        boolean hasStrongbox = this.getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);

        findViewById(R.id.sec_protected_confirmation_tee_test_success)
                .setVisibility(View.INVISIBLE);
        findViewById(R.id.sec_protected_confirmation_strongbox_test_success)
                .setVisibility(View.INVISIBLE);
        Button startTestButton = (Button) findViewById(R.id.sec_start_test_button);
        startTestButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                showToast("Test running...");
                v.post(new Runnable() {
                    @Override
                    public void run() {
                        runTest(false /* useStrongbox */);
                    }
                });
            }

        });

        Button startStrongboxTestButton =
                (Button) findViewById(R.id.sec_start_test_strongbox_button);
        if (hasStrongbox) {
            startStrongboxTestButton.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    showToast("Test running...");
                    v.post(new Runnable() {
                        @Override
                        public void run() {
                            runTest(true /* useStrongbox */);
                        }
                    });
                }

            });
        } else {
            startStrongboxTestButton.setVisibility(View.GONE);
            // since strongbox is unavailable we mark the strongbox test as passed so that the tee
            // test alone can make the test pass.
            strongboxTestSuccess = true;
        }

    }

    /**
     * Creates an asymmetric signing key in AndroidKeyStore which can only be used for signing
     * user confirmed messages.
     */
    private void createKey(boolean useStrongbox) {
        Calendar calendar = Calendar.getInstance();
        Date validityStart = calendar.getTime();
        calendar.add(Calendar.YEAR, 1);
        Date validityEnd = calendar.getTime();
        try {
            KeyPairGenerator kpg = KeyPairGenerator.getInstance(
                    KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
            KeyGenParameterSpec.Builder builder = new KeyGenParameterSpec.Builder(
                    KEY_NAME,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY);
            builder.setDigests(KeyProperties.DIGEST_SHA256, KeyProperties.DIGEST_SHA512);
            builder.setAttestationChallenge("CtsVerifierTest".getBytes());
            builder.setUserConfirmationRequired(true);
            builder.setIsStrongBoxBacked(useStrongbox);
            builder.setKeyValidityStart(validityStart);
            builder.setKeyValidityEnd(validityEnd);
            kpg.initialize(builder.build());
            kpg.generateKeyPair();
        } catch (NoSuchAlgorithmException | NoSuchProviderException |
                 InvalidAlgorithmParameterException e) {
            throw new RuntimeException("Failed to create confirmation key", e);
        }
    }

    private boolean trySign(byte[] dataThatWasConfirmed) {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            keyStore.load(null);
            KeyStore.Entry key = keyStore.getEntry(KEY_NAME, null);
            Signature s = Signature.getInstance("SHA256withECDSA");
            s.initSign(((KeyStore.PrivateKeyEntry) key).getPrivateKey());
            s.update(dataThatWasConfirmed);
            s.sign();
        } catch (CertificateException | KeyStoreException | IOException | NoSuchAlgorithmException |
                UnrecoverableEntryException | InvalidKeyException e) {
            throw new RuntimeException("Failed to load confirmation key", e);
        } catch (SignatureException e) {
            return false;
        }
        return true;
    }

    private void runTest(boolean useStrongbox) {
        createKey(useStrongbox);
        if (trySign(getString(R.string.sec_protected_confirmation_message)
                .getBytes())) {
            showToast("Test failed. Key could sign without confirmation.");
        } else {
            showConfirmationPrompt(
                    getString(R.string.sec_protected_confirmation_message),
                    useStrongbox);
        }
    }

    private void showConfirmationPrompt(String confirmationMessage, boolean useStrongbox) {
        ConfirmationPrompt.Builder builder = new ConfirmationPrompt.Builder(this);
        builder.setPromptText(confirmationMessage);
        builder.setExtraData(new byte[]{0x1, 0x02, 0x03});
        ConfirmationPrompt prompt = builder.build();
        try {
            prompt.presentPrompt(getMainExecutor(),
                    new ConfirmationCallback() {
                        @Override
                        public void onConfirmed(byte[] dataThatWasConfirmed) {
                            super.onConfirmed(dataThatWasConfirmed);
                            if (trySign(dataThatWasConfirmed)) {
                                markTestSuccess(useStrongbox);
                            } else {
                                showToast("Failed to sign confirmed message");
                            }
                        }

                        @Override
                        public void onDismissed() {
                            super.onDismissed();
                            showToast("User dismissed the dialog.");
                        }

                        @Override
                        public void onCanceled() {
                            super.onCanceled();
                            showToast("Confirmation dialog was canceled.");
                        }

                        @Override
                        public void onError(Throwable e) {
                            super.onError(e);
                            throw new RuntimeException("Confirmation Callback encountered an error",
                                                       e);
                        }
                    });
        } catch (ConfirmationAlreadyPresentingException | ConfirmationNotAvailableException e) {
            throw new RuntimeException("Error trying to present the confirmation prompt", e);
        }
    }

    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG)
                .show();
    }

    private void markTestSuccess(boolean strongbox) {
        if (strongbox) {
            if (!strongboxTestSuccess) {
                findViewById(R.id.sec_protected_confirmation_strongbox_test_success)
                        .setVisibility(View.VISIBLE);
            }
            strongboxTestSuccess = true;
        } else {
            if (!teeTestSuccess) {
                findViewById(R.id.sec_protected_confirmation_tee_test_success)
                        .setVisibility(View.VISIBLE);
            }
            teeTestSuccess = true;
        }
        if (strongboxTestSuccess && teeTestSuccess) {
            showToast("Test passed.");
            getPassButton().setEnabled(true);
        }
    }
}

