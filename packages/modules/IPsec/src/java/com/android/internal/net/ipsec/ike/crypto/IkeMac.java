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

import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_AES_XCBC_96;
import static android.net.ipsec.ike.SaProposal.PSEUDORANDOM_FUNCTION_AES128_XCBC;

import com.android.internal.net.crypto.KeyGenerationUtils.ByteSigner;

import java.nio.ByteBuffer;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.Mac;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.SecretKeySpec;

/**
 * IkeMac is an abstract class that represents common information for all negotiated algorithms that
 * generates Message Authentication Code (MAC), e.g. PRF and integrity algorithm.
 */
abstract class IkeMac extends IkeCrypto implements ByteSigner {
    // STOPSHIP: b/130190639 Catch unchecked exceptions, notify users and close the IKE session.
    private final boolean mIsEncryptAlgo;
    private final Mac mMac;
    private final Cipher mCipher;

    protected IkeMac(int algorithmId, int keyLength, String algorithmName, boolean isEncryptAlgo) {
        super(algorithmId, keyLength, algorithmName);

        mIsEncryptAlgo = isEncryptAlgo;

        try {
            if (mIsEncryptAlgo) {
                mMac = null;
                mCipher = Cipher.getInstance(getAlgorithmName());
            } else {
                mMac = Mac.getInstance(getAlgorithmName());
                mCipher = null;
            }
        } catch (NoSuchAlgorithmException | NoSuchPaddingException e) {
            throw new IllegalArgumentException("Failed to construct " + getTypeString(), e);
        }
    }

    /**
     * Signs the bytes to generate a Message Authentication Code (MAC).
     *
     * <p>Caller is responsible for providing valid key according to their use cases (e.g. PSK,
     * SK_p, SK_d ...).
     *
     * @param keyBytes the key to sign data.
     * @param dataToSign the data to be signed.
     * @return the calculated MAC.
     */
    @Override
    public byte[] signBytes(byte[] keyBytes, byte[] dataToSign) {
        try {
            if (mIsEncryptAlgo) {
                int algoId = getAlgorithmId();
                switch (algoId) {
                    case INTEGRITY_ALGORITHM_AES_XCBC_96:
                        return new AesXCbcImpl(mCipher)
                                .signBytes(keyBytes, dataToSign, true /*needTruncation*/);
                    case PSEUDORANDOM_FUNCTION_AES128_XCBC:
                        keyBytes = modifyKeyIfNeeded(keyBytes);
                        return new AesXCbcImpl(mCipher)
                                .signBytes(keyBytes, dataToSign, false /*needTruncation*/);
                    default:
                        throw new IllegalStateException("Invalid algorithm: " + algoId);
                }
            } else {
                SecretKeySpec secretKey = new SecretKeySpec(keyBytes, getAlgorithmName());
                ByteBuffer inputBuffer = ByteBuffer.wrap(dataToSign);
                mMac.init(secretKey);
                mMac.update(inputBuffer);

                return mMac.doFinal();
            }
        } catch (InvalidKeyException | IllegalStateException e) {
            throw new IllegalArgumentException("Failed to generate MAC: ", e);
        }
    }

    private byte[] modifyKeyIfNeeded(byte[] keyBytes) {
        // As per RFC 4434:
        // The key for AES-XCBC-PRF-128 is created as follows:
        //
        // 1. If the key is exactly 128 bits long, use it as-is.
        //
        // 2. If the key has fewer than 128 bits, lengthen it to exactly 128 bits by padding it on
        // the right with zero bits.
        //
        // 3. If the key is 129 bits or longer, shorten it to exactly 128 bits by performing the
        // steps in AES-XCBC-PRF-128 (that is, the algorithm described in this document). In that
        // re-application of this algorithm, the key is 128 zero bits; the message is the too-long
        // current key.
        if (keyBytes.length < 16) {
            keyBytes = Arrays.copyOf(keyBytes, 16);
        } else if (keyBytes.length > 16) {
            keyBytes =
                    new AesXCbcImpl(mCipher)
                            .signBytes(new byte[16], keyBytes, false /*needTruncation*/);
        }

        return keyBytes;
    }
}
