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

import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;

import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.Mac;

/**
 * IkeMacIntegrity represents a negotiated integrity algorithm.
 *
 * <p>For integrity algorithms based on encryption algorithm, all operations will be done by a
 * {@link Cipher}. Otherwise, all operations will be done by a {@link Mac}.
 *
 * <p>@see <a href="https://tools.ietf.org/html/rfc7296#section-3.3.2">RFC 7296, Internet Key
 * Exchange Protocol Version 2 (IKEv2)</a>
 */
public class IkeMacIntegrity extends IkeMac {
    // STOPSHIP: b/130190639 Catch unchecked exceptions, notify users and close the IKE session.
    private final int mChecksumLength;

    private IkeMacIntegrity(
            @SaProposal.IntegrityAlgorithm int algorithmId,
            int keyLength,
            String algorithmName,
            boolean isEncryptAlgo,
            int checksumLength) {
        super(algorithmId, keyLength, algorithmName, isEncryptAlgo);
        mChecksumLength = checksumLength;
    }

    /**
     * Construct an instance of IkeMacIntegrity.
     *
     * @param integrityTransform the valid negotiated IntegrityTransform.
     * @return an instance of IkeMacIntegrity.
     */
    public static IkeMacIntegrity create(IntegrityTransform integrityTransform) {
        int algorithmId = integrityTransform.id;

        int keyLength = 0;
        String algorithmName = "";
        boolean isEncryptAlgo = false;
        int checksumLength = 0;

        switch (algorithmId) {
            case SaProposal.INTEGRITY_ALGORITHM_NONE:
                throw new IllegalArgumentException("Integrity algorithm is not found.");
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96:
                keyLength = 20;
                algorithmName = "HmacSHA1";
                checksumLength = 12;
                break;
            case SaProposal.INTEGRITY_ALGORITHM_AES_XCBC_96:
                keyLength = 16;
                isEncryptAlgo = true;
                algorithmName = "AES/CBC/NoPadding";
                checksumLength = 12;
                break;
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_256_128:
                keyLength = 32;
                algorithmName = "HmacSHA256";
                checksumLength = 16;
                break;
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_384_192:
                keyLength = 48;
                algorithmName = "HmacSHA384";
                checksumLength = 24;
                break;
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_512_256:
                keyLength = 64;
                algorithmName = "HmacSHA512";
                checksumLength = 32;
                break;
            default:
                throw new IllegalArgumentException(
                        "Unrecognized Integrity Algorithm ID: " + algorithmId);
        }

        return new IkeMacIntegrity(
                algorithmId, keyLength, algorithmName, isEncryptAlgo, checksumLength);
    }

    /**
     * Gets integrity checksum length (in bytes).
     *
     * <p>IKE defines a fixed truncation length for each integirty algorithm as its checksum length.
     *
     * @return the integrity checksum length (in bytes).
     */
    public int getChecksumLen() {
        return mChecksumLength;
    }

    /**
     * Signs the bytes to generate an integrity checksum.
     *
     * @param keyBytes the negotiated integrity key.
     * @param dataToAuthenticate the data to authenticate.
     * @return the integrity checksum.
     */
    public byte[] generateChecksum(byte[] keyBytes, byte[] dataToAuthenticate) {
        if (getKeyLength() != keyBytes.length) {
            throw new IllegalArgumentException(
                    "Expected key length: "
                            + getKeyLength()
                            + " Received key length: "
                            + keyBytes.length);
        }

        byte[] signedBytes = signBytes(keyBytes, dataToAuthenticate);
        return Arrays.copyOfRange(signedBytes, 0, mChecksumLength);
    }

    /**
     * Build IpSecAlgorithm from this IkeMacIntegrity.
     *
     * <p>Build IpSecAlgorithm that represents the same integrity algorithm with this
     * IkeMacIntegrity instance with provided integrity key.
     *
     * @param key the integrity key in byte array.
     * @return the IpSecAlgorithm.
     */
    public IpSecAlgorithm buildIpSecAlgorithmWithKey(byte[] key) {
        if (key.length != getKeyLength()) {
            throw new IllegalArgumentException(
                    "Expected key with length of : "
                            + getKeyLength()
                            + " Received key with length of : "
                            + key.length);
        }

        switch (getAlgorithmId()) {
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96:
                return new IpSecAlgorithm(IpSecAlgorithm.AUTH_HMAC_SHA1, key, mChecksumLength * 8);
            case SaProposal.INTEGRITY_ALGORITHM_AES_XCBC_96:
                // TODO:Consider supporting AES128_XCBC in IpSecTransform.
                throw new IllegalArgumentException(
                        "Do not support IpSecAlgorithm with AES128_XCBC.");
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_256_128:
                return new IpSecAlgorithm(
                        IpSecAlgorithm.AUTH_HMAC_SHA256, key, mChecksumLength * 8);
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_384_192:
                return new IpSecAlgorithm(
                        IpSecAlgorithm.AUTH_HMAC_SHA384, key, mChecksumLength * 8);
            case SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_512_256:
                return new IpSecAlgorithm(
                        IpSecAlgorithm.AUTH_HMAC_SHA512, key, mChecksumLength * 8);
            default:
                throw new IllegalArgumentException(
                        "Unrecognized Integrity Algorithm ID: " + getAlgorithmId());
        }
    }

    /**
     * Returns algorithm type as a String.
     *
     * @return the algorithm type as a String.
     */
    @Override
    public String getTypeString() {
        return "Integrity Algorithm.";
    }
}
