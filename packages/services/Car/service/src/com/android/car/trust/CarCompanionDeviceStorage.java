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
 * limitations under the License.
 */

package com.android.car.trust;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import android.util.Log;

import com.android.car.R;

import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.UnrecoverableKeyException;
import java.security.cert.CertificateException;
import java.util.UUID;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.KeyGenerator;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.GCMParameterSpec;

/**
 * Storage for Trusted Devices in a car.
 */
class CarCompanionDeviceStorage {
    private static final String TAG = CarCompanionDeviceStorage.class.getSimpleName();

    private static final String UNIQUE_ID_KEY = "CTABM_unique_id";
    private static final String PREF_ENCRYPTION_KEY_PREFIX = "CTABM_encryption_key";
    private static final String KEY_ALIAS = "Ukey2Key";
    private static final String CIPHER_TRANSFORMATION = "AES/GCM/NoPadding";
    private static final String KEYSTORE_PROVIDER = "AndroidKeyStore";
    private static final String IV_SPEC_SEPARATOR = ";";

    // The length of the authentication tag for a cipher in GCM mode. The GCM specification states
    // that this length can only have the values {128, 120, 112, 104, 96}. Using the highest
    // possible value.
    private static final int GCM_AUTHENTICATION_TAG_LENGTH = 128;

    private Context mContext;
    private SharedPreferences mSharedPreferences;
    private UUID mUniqueId;

    CarCompanionDeviceStorage(@NonNull Context context) {
        mContext = context;
    }

    /** Return the car TrustedAgent {@link SharedPreferences}. */
    @NonNull
    SharedPreferences getSharedPrefs() {
        // This should be called only after user 0 is unlocked.
        if (mSharedPreferences != null) {
            return mSharedPreferences;
        }
        mSharedPreferences = mContext.getSharedPreferences(
                mContext.getString(R.string.token_handle_shared_preferences), Context.MODE_PRIVATE);
        return mSharedPreferences;
    }

    /**
     * Returns User Id for the given token handle
     *
     * @param handle The handle corresponding to the escrow token
     * @return User id corresponding to the handle
     */
    int getUserHandleByTokenHandle(long handle) {
        return getSharedPrefs().getInt(String.valueOf(handle), -1);
    }

    /**
     * Get communication encryption key for the given device
     *
     * @param deviceId id of trusted device
     * @return encryption key, null if device id is not recognized
     */
    @Nullable
    byte[] getEncryptionKey(@NonNull String deviceId) {
        SharedPreferences prefs = getSharedPrefs();
        String key = createSharedPrefKey(deviceId);
        if (!prefs.contains(key)) {
            return null;
        }

        // This value will not be "null" because we already checked via a call to contains().
        String[] values = prefs.getString(key, null).split(IV_SPEC_SEPARATOR);

        if (values.length != 2) {
            return null;
        }

        byte[] encryptedKey = Base64.decode(values[0], Base64.DEFAULT);
        byte[] ivSpec = Base64.decode(values[1], Base64.DEFAULT);
        return decryptWithKeyStore(KEY_ALIAS, encryptedKey, ivSpec);
    }

    /**
     * Save encryption key for the given device
     *
     * @param deviceId did of trusted device
     * @param encryptionKey encryption key
     * @return {@code true} if the operation succeeded
     */
    boolean saveEncryptionKey(@NonNull String deviceId, @NonNull byte[] encryptionKey) {
        String encryptedKey = encryptWithKeyStore(KEY_ALIAS, encryptionKey);
        if (encryptedKey == null) {
            return false;
        }
        if (getSharedPrefs().contains(createSharedPrefKey(deviceId))) {
            clearEncryptionKey(deviceId);
        }

        return getSharedPrefs()
                .edit()
                .putString(createSharedPrefKey(deviceId), encryptedKey)
                .commit();
    }

    /**
     * Clear the encryption key for the given device
     *
     * @param deviceId id of the peer device
     */
    void clearEncryptionKey(@Nullable String deviceId) {
        if (deviceId == null) {
            return;
        }
        getSharedPrefs()
                .edit()
                .remove(createSharedPrefKey(deviceId))
                .commit();
    }

    /**
     * Encrypt value with designated key
     *
     * <p>The encrypted value is of the form:
     *
     * <p>key + IV_SPEC_SEPARATOR + ivSpec
     *
     * <p>The {@code ivSpec} is needed to decrypt this key later on.
     *
     * @param keyAlias KeyStore alias for key to use
     * @param value a value to encrypt
     * @return encrypted value, null if unable to encrypt
     */
    @Nullable
    String encryptWithKeyStore(@NonNull String keyAlias, @Nullable byte[] value) {
        if (value == null) {
            return null;
        }

        Key key = getKeyStoreKey(keyAlias);
        try {
            Cipher cipher = Cipher.getInstance(CIPHER_TRANSFORMATION);
            cipher.init(Cipher.ENCRYPT_MODE, key);
            return new StringBuffer(Base64.encodeToString(cipher.doFinal(value), Base64.DEFAULT))
                    .append(IV_SPEC_SEPARATOR)
                    .append(Base64.encodeToString(cipher.getIV(), Base64.DEFAULT))
                    .toString();
        } catch (IllegalBlockSizeException
                | BadPaddingException
                | NoSuchAlgorithmException
                | NoSuchPaddingException
                | IllegalStateException
                | InvalidKeyException e) {
            Log.e(TAG, "Unable to encrypt value with key " + keyAlias, e);
            return null;
        }
    }

    /**
     * Decrypt value with designated key
     *
     * @param keyAlias KeyStore alias for key to use
     * @param value encrypted value
     * @return decrypted value, null if unable to decrypt
     */
    @Nullable
    byte[] decryptWithKeyStore(@NonNull String keyAlias, @Nullable byte[] value,
            @NonNull byte[] ivSpec) {
        if (value == null) {
            return null;
        }

        try {
            Key key = getKeyStoreKey(keyAlias);
            Cipher cipher = Cipher.getInstance(CIPHER_TRANSFORMATION);
            cipher.init(Cipher.DECRYPT_MODE, key,
                    new GCMParameterSpec(GCM_AUTHENTICATION_TAG_LENGTH, ivSpec));
            return cipher.doFinal(value);
        } catch (IllegalBlockSizeException
                | BadPaddingException
                | NoSuchAlgorithmException
                | NoSuchPaddingException
                | IllegalStateException
                | InvalidKeyException
                | InvalidAlgorithmParameterException e) {
            Log.e(TAG, "Unable to decrypt value with key " + keyAlias, e);
            return null;
        }
    }

    private Key getKeyStoreKey(@NonNull String keyAlias) {
        KeyStore keyStore;
        try {
            keyStore = KeyStore.getInstance(KEYSTORE_PROVIDER);
            keyStore.load(null);
            if (!keyStore.containsAlias(keyAlias)) {
                KeyGenerator keyGenerator = KeyGenerator.getInstance(
                        KeyProperties.KEY_ALGORITHM_AES, KEYSTORE_PROVIDER);
                keyGenerator.init(
                        new KeyGenParameterSpec.Builder(keyAlias,
                                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                                .build());
                keyGenerator.generateKey();
            }
            return keyStore.getKey(keyAlias, null);

        } catch (KeyStoreException
                | NoSuchAlgorithmException
                | UnrecoverableKeyException
                | NoSuchProviderException
                | CertificateException
                | IOException
                | InvalidAlgorithmParameterException e) {
            Log.e(TAG, "Unable to retrieve key " + keyAlias + " from KeyStore.", e);
            throw new IllegalStateException(e);
        }
    }

    /**
     * Get the unique id for head unit. Persists on device until factory reset.
     * This should be called only after user 0 is unlocked.
     *
     * @return unique id, or null if unable to retrieve generated id (this should never happen)
     */
    @Nullable
    UUID getUniqueId() {
        if (mUniqueId != null) {
            return mUniqueId;
        }

        SharedPreferences prefs = getSharedPrefs();
        if (prefs.contains(UNIQUE_ID_KEY)) {
            mUniqueId = UUID.fromString(
                    prefs.getString(UNIQUE_ID_KEY, null));
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Found existing trusted unique id: "
                        + prefs.getString(UNIQUE_ID_KEY, ""));
            }
        } else {
            mUniqueId = UUID.randomUUID();
            if (!prefs.edit().putString(UNIQUE_ID_KEY, mUniqueId.toString()).commit()) {
                mUniqueId = null;
            } else if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Generated new trusted unique id: "
                        + prefs.getString(UNIQUE_ID_KEY, ""));
            }
        }

        return mUniqueId;
    }

    private String createSharedPrefKey(@NonNull String deviceId) {
        return PREF_ENCRYPTION_KEY_PREFIX + deviceId;
    }
}
