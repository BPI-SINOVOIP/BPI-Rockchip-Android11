/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.internal.net.ipsec.ike.message;

import android.annotation.Nullable;
import android.net.ipsec.ike.SaProposal;
import android.net.ipsec.ike.exceptions.IkeProtocolException;

import com.android.internal.net.ipsec.ike.IkeDhParams;
import com.android.internal.net.ipsec.ike.exceptions.InvalidSyntaxException;
import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;
import com.android.internal.net.utils.BigIntegerUtils;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.security.GeneralSecurityException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.ProviderException;
import java.security.SecureRandom;
import java.security.spec.InvalidKeySpecException;

import javax.crypto.KeyAgreement;
import javax.crypto.interfaces.DHPrivateKey;
import javax.crypto.interfaces.DHPublicKey;
import javax.crypto.spec.DHParameterSpec;
import javax.crypto.spec.DHPrivateKeySpec;
import javax.crypto.spec.DHPublicKeySpec;

/**
 * IkeKePayload represents a Key Exchange payload
 *
 * <p>This class provides methods for generating Diffie-Hellman value and doing Diffie-Hellman
 * exhchange. Upper layer should ignore IkeKePayload with unsupported DH group type.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#page-89">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class IkeKePayload extends IkePayload {
    private static final int KE_HEADER_LEN = 4;
    private static final int KE_HEADER_RESERVED = 0;

    // Key exchange data length in octets
    private static final int DH_GROUP_1024_BIT_MODP_DATA_LEN = 128;
    private static final int DH_GROUP_2048_BIT_MODP_DATA_LEN = 256;
    private static final int DH_GROUP_3072_BIT_MODP_DATA_LEN = 384;
    private static final int DH_GROUP_4096_BIT_MODP_DATA_LEN = 512;

    // Algorithm name of Diffie-Hellman
    private static final String KEY_EXCHANGE_ALGORITHM = "DH";

    // TODO: Create a library initializer that checks if Provider supports DH algorithm.

    /** Supported dhGroup falls into {@link DhGroup} */
    public final int dhGroup;

    /** Public DH key for the recipient to calculate shared key. */
    public final byte[] keyExchangeData;

    /** Flag indicates if this is an outbound payload. */
    public final boolean isOutbound;

    /**
     * localPrivateKey caches the locally generated private key when building an outbound KE
     * payload. It will not be sent out. It is only used to calculate DH shared key when IKE library
     * receives a public key from the remote server.
     *
     * <p>localPrivateKey of a inbound payload will be set to null. Caller MUST ensure its an
     * outbound payload before using localPrivateKey.
     */
    @Nullable public final DHPrivateKeySpec localPrivateKey;

    /**
     * Construct an instance of IkeKePayload in the context of IkePayloadFactory
     *
     * @param critical indicates if this payload is critical. Ignored in supported payload as
     *     instructed by the RFC 7296.
     * @param payloadBody payload body in byte array
     * @throws IkeProtocolException if there is any error
     * @see <a href="https://tools.ietf.org/html/rfc7296#page-76">RFC 7296, Internet Key Exchange
     *     Protocol Version 2 (IKEv2), Critical.
     */
    IkeKePayload(boolean critical, byte[] payloadBody) throws IkeProtocolException {
        super(PAYLOAD_TYPE_KE, critical);

        isOutbound = false;
        localPrivateKey = null;

        ByteBuffer inputBuffer = ByteBuffer.wrap(payloadBody);

        dhGroup = Short.toUnsignedInt(inputBuffer.getShort());
        // Skip reserved field
        inputBuffer.getShort();

        int dataSize = payloadBody.length - KE_HEADER_LEN;
        // Check if dataSize matches the DH group type
        boolean isValidSyntax = true;
        switch (dhGroup) {
            case SaProposal.DH_GROUP_1024_BIT_MODP:
                isValidSyntax = DH_GROUP_1024_BIT_MODP_DATA_LEN == dataSize;
                break;
            case SaProposal.DH_GROUP_2048_BIT_MODP:
                isValidSyntax = DH_GROUP_2048_BIT_MODP_DATA_LEN == dataSize;
                break;
            case SaProposal.DH_GROUP_3072_BIT_MODP:
                isValidSyntax = DH_GROUP_3072_BIT_MODP_DATA_LEN == dataSize;
                break;
            case SaProposal.DH_GROUP_4096_BIT_MODP:
                isValidSyntax = DH_GROUP_4096_BIT_MODP_DATA_LEN == dataSize;
                break;
            default:
                // For unsupported DH group, we cannot check its syntax. Upper layer will ingore
                // this payload.
        }
        if (!isValidSyntax) {
            throw new InvalidSyntaxException("Invalid KE payload length for provided DH group.");
        }

        keyExchangeData = new byte[dataSize];
        inputBuffer.get(keyExchangeData);
    }

    /**
     * Construct an instance of IkeKePayload for building an outbound packet.
     *
     * <p>Generate a DH key pair. Cache the private key and send out the public key as
     * keyExchangeData.
     *
     * <p>Critical bit in this payload must not be set as instructed in RFC 7296.
     *
     * @param dh DH group for this KE payload
     * @param randomnessFactory the randomness factory
     * @see <a href="https://tools.ietf.org/html/rfc7296#page-76">RFC 7296, Internet Key Exchange
     *     Protocol Version 2 (IKEv2), Critical.
     */
    public IkeKePayload(@SaProposal.DhGroup int dh, RandomnessFactory randomnessFactory) {
        super(PAYLOAD_TYPE_KE, false);

        dhGroup = dh;
        isOutbound = true;

        BigInteger prime = BigInteger.ZERO;
        int keySize = 0;
        switch (dhGroup) {
            case SaProposal.DH_GROUP_1024_BIT_MODP:
                prime =
                        BigIntegerUtils.unsignedHexStringToBigInteger(
                                IkeDhParams.PRIME_1024_BIT_MODP);
                keySize = DH_GROUP_1024_BIT_MODP_DATA_LEN;
                break;
            case SaProposal.DH_GROUP_2048_BIT_MODP:
                prime =
                        BigIntegerUtils.unsignedHexStringToBigInteger(
                                IkeDhParams.PRIME_2048_BIT_MODP);
                keySize = DH_GROUP_2048_BIT_MODP_DATA_LEN;
                break;
            case SaProposal.DH_GROUP_3072_BIT_MODP:
                prime =
                        BigIntegerUtils.unsignedHexStringToBigInteger(
                                IkeDhParams.PRIME_3072_BIT_MODP);
                keySize = DH_GROUP_3072_BIT_MODP_DATA_LEN;
                break;
            case SaProposal.DH_GROUP_4096_BIT_MODP:
                prime =
                        BigIntegerUtils.unsignedHexStringToBigInteger(
                                IkeDhParams.PRIME_4096_BIT_MODP);
                keySize = DH_GROUP_4096_BIT_MODP_DATA_LEN;
                break;
            default:
                throw new IllegalArgumentException("DH group not supported: " + dh);
        }

        try {
            BigInteger baseGen = BigInteger.valueOf(IkeDhParams.BASE_GENERATOR_MODP);
            DHParameterSpec dhParams = new DHParameterSpec(prime, baseGen);

            KeyPairGenerator dhKeyPairGen = KeyPairGenerator.getInstance(KEY_EXCHANGE_ALGORITHM);

            SecureRandom random = randomnessFactory.getRandom();
            random = random == null ? new SecureRandom() : random;
            dhKeyPairGen.initialize(dhParams, random);

            KeyPair keyPair = dhKeyPairGen.generateKeyPair();

            DHPrivateKey privateKey = (DHPrivateKey) keyPair.getPrivate();
            DHPrivateKeySpec dhPrivateKeyspec =
                    new DHPrivateKeySpec(privateKey.getX(), prime, baseGen);
            DHPublicKey publicKey = (DHPublicKey) keyPair.getPublic();

            // Zero-pad the public key without the sign bit
            keyExchangeData =
                    BigIntegerUtils.bigIntegerToUnsignedByteArray(publicKey.getY(), keySize);
            localPrivateKey = dhPrivateKeyspec;
        } catch (NoSuchAlgorithmException e) {
            throw new ProviderException("Failed to obtain " + KEY_EXCHANGE_ALGORITHM, e);
        } catch (InvalidAlgorithmParameterException e) {
            throw new IllegalArgumentException("Failed to initialize key generator", e);
        }
    }

    /**
     * Encode KE payload to ByteBuffer.
     *
     * @param nextPayload type of payload that follows this payload.
     * @param byteBuffer destination ByteBuffer that stores encoded payload.
     */
    @Override
    protected void encodeToByteBuffer(@PayloadType int nextPayload, ByteBuffer byteBuffer) {
        encodePayloadHeaderToByteBuffer(nextPayload, getPayloadLength(), byteBuffer);
        byteBuffer
                .putShort((short) dhGroup)
                .putShort((short) KE_HEADER_RESERVED)
                .put(keyExchangeData);
    }

    /**
     * Get entire payload length.
     *
     * @return entire payload length.
     */
    @Override
    protected int getPayloadLength() {
        return GENERIC_HEADER_LENGTH + KE_HEADER_LEN + keyExchangeData.length;
    }

    /**
     * Calculate the shared secret.
     *
     * @param privateKeySpec contains the local private key, DH prime and DH base generator.
     * @param remotePublicKey the public key from remote server.
     * @throws GeneralSecurityException if the remote public key is invalid.
     */
    public static byte[] getSharedKey(DHPrivateKeySpec privateKeySpec, byte[] remotePublicKey)
            throws GeneralSecurityException {
        KeyAgreement dhKeyAgreement;
        KeyFactory dhKeyFactory;
        try {
            // Apply local private key.
            dhKeyAgreement = KeyAgreement.getInstance(KEY_EXCHANGE_ALGORITHM);
            dhKeyFactory = KeyFactory.getInstance(KEY_EXCHANGE_ALGORITHM);
            DHPrivateKey privateKey = (DHPrivateKey) dhKeyFactory.generatePrivate(privateKeySpec);
            dhKeyAgreement.init(privateKey);
        } catch (NoSuchAlgorithmException | InvalidKeySpecException | InvalidKeyException e) {
            throw new IllegalArgumentException("Failed to generate DH private key", e);
        }

        // Build public key.
        BigInteger publicKeyValue = BigIntegerUtils.unsignedByteArrayToBigInteger(remotePublicKey);
        BigInteger primeValue = privateKeySpec.getP();
        BigInteger baseGenValue = privateKeySpec.getG();
        DHPublicKeySpec publicKeySpec =
                new DHPublicKeySpec(publicKeyValue, primeValue, baseGenValue);

        // Validate and apply public key. Validation includes range check as instructed by RFC6989
        // section 2.1
        DHPublicKey publicKey = (DHPublicKey) dhKeyFactory.generatePublic(publicKeySpec);

        dhKeyAgreement.doPhase(publicKey, true /* Last phase */);
        return dhKeyAgreement.generateSecret();
    }

    /**
     * Return the payload type as a String.
     *
     * @return the payload type as a String.
     */
    @Override
    public String getTypeString() {
        return "KE";
    }
}
