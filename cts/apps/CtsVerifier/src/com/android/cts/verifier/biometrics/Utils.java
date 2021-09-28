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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.hardware.biometrics.BiometricManager;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.text.InputType;
import android.util.Log;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Toast;

import java.security.KeyPair;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.util.Random;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import javax.crypto.SecretKey;

public class Utils {
    private static final String TAG = "BiometricTestUtils";

    static void createBiometricBoundKey(String keyName, boolean useStrongBox) throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        KeyGenerator keyGenerator = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");

        // Set the alias of the entry in Android KeyStore where the key will appear
        // and the constrains (purposes) in the constructor of the Builder
        keyGenerator.init(new KeyGenParameterSpec.Builder(keyName,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_CBC)
                .setUserAuthenticationRequired(true)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_PKCS7)
                .setIsStrongBoxBacked(useStrongBox)
                .setInvalidatedByBiometricEnrollment(true)
                .build());
        keyGenerator.generateKey();
    }

    static void createTimeBoundSecretKey_deprecated(String keyName, boolean useStrongBox)
            throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        KeyGenerator keyGenerator = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");

        // Set the alias of the entry in Android KeyStore where the key will appear
        // and the constrains (purposes) in the constructor of the Builder
        keyGenerator.init(new KeyGenParameterSpec.Builder(keyName,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_CBC)
                .setUserAuthenticationRequired(true)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_PKCS7)
                .setIsStrongBoxBacked(useStrongBox)
                .setUserAuthenticationValidityDurationSeconds(5 /* seconds */)
                .build());
        keyGenerator.generateKey();
    }

    static void createTimeBoundSecretKey(String keyName, int authTypes, boolean useStrongBox)
            throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        KeyGenerator keyGenerator = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");

        // Set the alias of the entry in Android KeyStore where the key will appear
        // and the constrains (purposes) in the constructor of the Builder
        keyGenerator.init(new KeyGenParameterSpec.Builder(keyName,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_CBC)
                .setUserAuthenticationRequired(true)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_PKCS7)
                .setIsStrongBoxBacked(useStrongBox)
                .setUserAuthenticationParameters(1 /* seconds */, authTypes)
                .build());
        keyGenerator.generateKey();
    }

    static Cipher initCipher(String keyName) throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        SecretKey secretKey = (SecretKey) keyStore.getKey(keyName, null);

        Cipher cipher = Cipher.getInstance(KeyProperties.KEY_ALGORITHM_AES + "/"
                + KeyProperties.BLOCK_MODE_CBC + "/"
                + KeyProperties.ENCRYPTION_PADDING_PKCS7);
        cipher.init(Cipher.ENCRYPT_MODE, secretKey);
        return cipher;
    }

    static Signature initSignature(String keyName) throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);

        KeyStore.Entry entry = keyStore.getEntry(keyName, null);

        PrivateKey privateKey = ((KeyStore.PrivateKeyEntry) entry).getPrivateKey();

        // TODO: This can be used to verify signature
        // PublicKey publicKey = keyStore.getCertificate(keyName).getPublicKey();

        Signature signature = Signature.getInstance("SHA256withECDSA");
        signature.initSign(privateKey);
        return signature;
    }

    static Mac initMac(String keyName) throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);

        SecretKey secretKey = (SecretKey) keyStore.getKey(keyName, null);

        Mac mac = Mac.getInstance("HmacSHA256");
        mac.init(secretKey);
        return mac;
    }

    static byte[] doEncrypt(Cipher cipher, byte[] data) throws Exception {
        return cipher.doFinal(data);
    }

    static byte[] doSign(Signature signature, byte[] data) throws Exception {
        signature.update(data);
        return signature.sign();
    }

    static void showInstructionDialog(Context context, int titleRes, int messageRes,
            int positiveButtonRes, DialogInterface.OnClickListener listener) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(titleRes);
        builder.setMessage(messageRes);
        builder.setPositiveButton(positiveButtonRes, listener);
        AlertDialog dialog = builder.create();
        dialog.show();
    }

    static abstract class VerifyRandomContents {
        final Context mContext;
        final String mRandomTitle;
        final String mRandomSubtitle;
        final String mRandomDescription;
        final String mRandomNegativeButtonText;

        abstract void onVerificationSucceeded();

        VerifyRandomContents(Context context) {
            mContext = context;

            final Random random = new Random();
            mRandomTitle = String.valueOf(random.nextInt(10000));
            mRandomSubtitle = String.valueOf(random.nextInt(10000));
            mRandomDescription = String.valueOf(random.nextInt(10000));
            mRandomNegativeButtonText = String.valueOf(random.nextInt(10000));
        }

        void verifyContents(String titleEntered, String subtitleEntered, String descriptionEntered,
                String negativeEntered) {
            if (!titleEntered.contentEquals(mRandomTitle)) {
                showToastAndLog(mContext, "Title incorrect, "
                        + titleEntered + " " + mRandomTitle);
            } else if (!subtitleEntered.contentEquals(mRandomSubtitle)) {
                showToastAndLog(mContext, "Subtitle incorrect, "
                        + subtitleEntered + " " + mRandomSubtitle);
            } else if (!descriptionEntered.contentEquals(mRandomDescription)) {
                showToastAndLog(mContext, "Description incorrect, "
                        + descriptionEntered + " " + mRandomDescription);
            } else if (!negativeEntered.contentEquals(mRandomNegativeButtonText)) {
                showToastAndLog(mContext, "Negative text incorrect, "
                        + negativeEntered + " " + mRandomNegativeButtonText);
            } else {
                showToastAndLog(mContext, "Contents matched!");
                onVerificationSucceeded();
            }
        }

    }

    static void showUIVerificationDialog(Context context, int titleRes,
            int positiveButtonRes, VerifyRandomContents contents) {
        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);

        final EditText titleBox = new EditText(context);
        titleBox.setHint("Title");
        titleBox.setInputType(InputType.TYPE_CLASS_NUMBER);
        layout.addView(titleBox);

        final EditText subtitleBox = new EditText(context);
        subtitleBox.setHint("Subtitle");
        subtitleBox.setInputType(InputType.TYPE_CLASS_NUMBER);
        layout.addView(subtitleBox);

        final EditText descriptionBox = new EditText(context);
        descriptionBox.setHint("Description");
        descriptionBox.setInputType(InputType.TYPE_CLASS_NUMBER);
        layout.addView(descriptionBox);

        final EditText negativeBox = new EditText(context);
        negativeBox.setHint("Negative Button");
        negativeBox.setInputType(InputType.TYPE_CLASS_NUMBER);
        layout.addView(negativeBox);

        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle(titleRes);
        builder.setPositiveButton(positiveButtonRes, (dialog, which) -> {
            if (which == DialogInterface.BUTTON_POSITIVE) {
                final String titleEntered = titleBox.getText().toString();
                final String subtitleEntered = subtitleBox.getText().toString();
                final String descriptionEntered = descriptionBox.getText().toString();
                final String negativeEntered = negativeBox.getText().toString();

                contents.verifyContents(titleEntered, subtitleEntered, descriptionEntered,
                        negativeEntered);
            }
        });

        AlertDialog dialog = builder.create();
        dialog.setView(layout);
        dialog.show();
    }

    static boolean deviceConfigContains(Context context, int authenticator) {
        final String config[] = context.getResources()
                .getStringArray(com.android.internal.R.array.config_biometric_sensors);
        for (String s : config) {
            Log.d(TAG, s);
            final String[] elems = s.split(":");
            final int strength = Integer.parseInt(elems[2]);
            if (strength == authenticator) {
                return true;
            }
        }
        return false;
    }

    static void showToastAndLog(Context context, String s) {
        Log.d(TAG, s);
        Toast.makeText(context, s, Toast.LENGTH_SHORT).show();
    }
}
