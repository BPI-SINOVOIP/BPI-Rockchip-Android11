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

package com.android.internal.net.ipsec.ike.crypto;

import android.net.IpSecAlgorithm;
import android.net.ipsec.ike.SaProposal;

import java.nio.ByteBuffer;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * IkeCipher represents a negotiated normal mode encryption algorithm.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-3.3.2">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class IkeNormalModeCipher extends IkeCipher {
    /** Package private */
    IkeNormalModeCipher(int algorithmId, int keyLength, int ivLength, String algorithmName) {
        super(algorithmId, keyLength, ivLength, algorithmName, false /*isAead*/);
    }

    private byte[] doCipherAction(byte[] data, byte[] keyBytes, byte[] ivBytes, int opmode)
            throws IllegalBlockSizeException {
        if (getKeyLength() != keyBytes.length) {
            throw new IllegalArgumentException(
                    "Expected key length: "
                            + getKeyLength()
                            + " Received key length: "
                            + keyBytes.length);
        }
        try {
            SecretKeySpec key = new SecretKeySpec(keyBytes, getAlgorithmName());
            IvParameterSpec iv = new IvParameterSpec(ivBytes);
            mCipher.init(opmode, key, iv);

            ByteBuffer inputBuffer = ByteBuffer.wrap(data);
            ByteBuffer outputBuffer = ByteBuffer.allocate(data.length);

            mCipher.doFinal(inputBuffer, outputBuffer);
            return outputBuffer.array();
        } catch (InvalidKeyException
                | InvalidAlgorithmParameterException
                | BadPaddingException
                | ShortBufferException e) {
            String errorMessage =
                    Cipher.ENCRYPT_MODE == opmode
                            ? "Failed to encrypt data: "
                            : "Failed to decrypt data: ";
            throw new IllegalArgumentException(errorMessage, e);
        }
    }

    /**
     * Encrypt padded data.
     *
     * @param paddedData the padded data to encrypt.
     * @param keyBytes the encryption key.
     * @param ivBytes the initialization vector (IV).
     * @return the encrypted and padded data.
     */
    public byte[] encrypt(byte[] paddedData, byte[] keyBytes, byte[] ivBytes) {
        try {
            return doCipherAction(paddedData, keyBytes, ivBytes, Cipher.ENCRYPT_MODE);
        } catch (IllegalBlockSizeException e) {
            throw new IllegalArgumentException("Failed to encrypt data: ", e);
        }
    }

    /**
     * Decrypt the encrypted and padded data.
     *
     * @param encryptedData the encrypted and padded data.
     * @param keyBytes the decryption key.
     * @param ivBytes the initialization vector (IV).
     * @return the decrypted and padded data.
     * @throws IllegalBlockSizeException if the total encryptedData length is not a multiple of
     *     block size.
     */
    public byte[] decrypt(byte[] encryptedData, byte[] keyBytes, byte[] ivBytes)
            throws IllegalBlockSizeException {
        return doCipherAction(encryptedData, keyBytes, ivBytes, Cipher.DECRYPT_MODE);
    }

    @Override
    public IpSecAlgorithm buildIpSecAlgorithmWithKey(byte[] key) {
        validateKeyLenOrThrow(key);

        switch (getAlgorithmId()) {
            case SaProposal.ENCRYPTION_ALGORITHM_3DES:
                // TODO: Consider supporting 3DES in IpSecTransform.
                throw new UnsupportedOperationException("Do not support 3Des encryption.");
            case SaProposal.ENCRYPTION_ALGORITHM_AES_CBC:
                return new IpSecAlgorithm(IpSecAlgorithm.CRYPT_AES_CBC, key);
            default:
                throw new IllegalArgumentException(
                        "Unrecognized Encryption Algorithm ID: " + getAlgorithmId());
        }
    }
}
