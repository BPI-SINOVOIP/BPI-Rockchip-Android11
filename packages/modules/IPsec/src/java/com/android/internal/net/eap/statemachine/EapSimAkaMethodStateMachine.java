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

package com.android.internal.net.eap.statemachine;

import static com.android.internal.net.eap.EapAuthenticator.LOG;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_RESPONSE;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_MAC;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_NOTIFICATION;

import android.net.eap.EapSessionConfig.EapUiccConfig;
import android.telephony.TelephonyManager;
import android.util.Base64;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.crypto.Fips186_2Prf;
import com.android.internal.net.eap.exceptions.EapInvalidRequestException;
import com.android.internal.net.eap.exceptions.EapSilentException;
import com.android.internal.net.eap.exceptions.simaka.EapSimAkaAuthenticationFailureException;
import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.EapData;
import com.android.internal.net.eap.message.EapMessage;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtClientErrorCode;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData;
import com.android.internal.net.utils.Log;

import java.nio.ByteBuffer;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

/**
 * EapSimAkaMethodStateMachine represents an abstract state machine for managing EAP-SIM and EAP-AKA
 * sessions.
 *
 * @see <a href="https://tools.ietf.org/html/rfc4186">RFC 4186, Extensible Authentication
 * Protocol for Subscriber Identity Modules (EAP-SIM)</a>
 * @see <a href="https://tools.ietf.org/html/rfc4187">RFC 4187, Extensible Authentication
 * Protocol for Authentication and Key Agreement (EAP-AKA)</a>
 */
public abstract class EapSimAkaMethodStateMachine extends EapMethodStateMachine {
    public static final String MASTER_KEY_GENERATION_ALG = "SHA-1";
    public static final String MAC_ALGORITHM_STRING = "HmacSHA1";

    // K_encr and K_aut lengths are 16 bytes (RFC 4186#7, RFC 4187#7)
    public static final int KEY_LEN = 16;

    // Session Key lengths are 64 bytes (RFC 4186#7, RFC 4187#7)
    public static final int SESSION_KEY_LENGTH = 64;

    public final byte[] mKEncr = new byte[getKEncrLength()];
    public final byte[] mKAut = new byte[getKAutLength()];
    public final byte[] mMsk = new byte[getMskLength()];
    public final byte[] mEmsk = new byte[getEmskLength()];

    @VisibleForTesting boolean mHasReceivedSimAkaNotification = false;

    final TelephonyManager mTelephonyManager;
    final byte[] mEapIdentity;
    final EapUiccConfig mEapUiccConfig;

    @VisibleForTesting Mac mMacAlgorithm;

    EapSimAkaMethodStateMachine(
            TelephonyManager telephonyManager, byte[] eapIdentity, EapUiccConfig eapUiccConfig) {
        if (telephonyManager == null) {
            throw new IllegalArgumentException("TelephonyManager must be non-null");
        } else if (eapIdentity == null) {
            throw new IllegalArgumentException("EapIdentity must be non-null");
        } else if (eapUiccConfig == null) {
            throw new IllegalArgumentException("EapUiccConfig must be non-null");
        }
        this.mTelephonyManager = telephonyManager;
        this.mEapIdentity = eapIdentity;
        this.mEapUiccConfig = eapUiccConfig;

        LOG.d(
                this.getClass().getSimpleName(),
                mEapUiccConfig.getClass().getSimpleName() + ":"
                        + " subId=" + mEapUiccConfig.subId
                        + " apptype=" + mEapUiccConfig.apptype);
    }

    protected int getKEncrLength() {
        return KEY_LEN;
    }

    protected int getKAutLength() {
        return KEY_LEN;
    }

    protected int getMskLength() {
        return SESSION_KEY_LENGTH;
    }

    protected int getEmskLength() {
        return SESSION_KEY_LENGTH;
    }

    @Override
    EapResult handleEapNotification(String tag, EapMessage message) {
        return EapStateMachine.handleNotification(tag, message);
    }

    protected String getMacAlgorithm() {
        return MAC_ALGORITHM_STRING;
    }

    @VisibleForTesting
    EapResult buildClientErrorResponse(
            int eapIdentifier,
            int eapMethodType,
            AtClientErrorCode clientErrorCode) {
        mIsExpectingEapFailure = true;
        EapSimAkaTypeData eapSimAkaTypeData = getEapSimAkaTypeData(clientErrorCode);
        byte[] encodedTypeData = eapSimAkaTypeData.encode();

        EapData eapData = new EapData(eapMethodType, encodedTypeData);
        try {
            EapMessage response = new EapMessage(EAP_CODE_RESPONSE, eapIdentifier, eapData);
            return EapResult.EapResponse.getEapResponse(response);
        } catch (EapSilentException ex) {
            return new EapResult.EapError(ex);
        }
    }

    @VisibleForTesting
    EapResult buildResponseMessage(
            int eapType,
            int eapSubtype,
            int identifier,
            List<EapSimAkaAttribute> attributes) {
        EapSimAkaTypeData eapSimTypeData = getEapSimAkaTypeData(eapSubtype, attributes);
        EapData eapData = new EapData(eapType, eapSimTypeData.encode());

        try {
            EapMessage eapMessage = new EapMessage(EAP_CODE_RESPONSE, identifier, eapData);
            return EapResult.EapResponse.getEapResponse(eapMessage);
        } catch (EapSilentException ex) {
            return new EapResult.EapError(ex);
        }
    }

    @VisibleForTesting
    protected void generateAndPersistKeys(
            String tag, MessageDigest sha1, Fips186_2Prf prf, byte[] mkInput) {
        byte[] mk = sha1.digest(mkInput);

        // run mk through FIPS 186-2
        int outputBytes = mKEncr.length + mKAut.length + mMsk.length + mEmsk.length;
        byte[] prfResult = prf.getRandom(mk, outputBytes);

        ByteBuffer prfResultBuffer = ByteBuffer.wrap(prfResult);
        prfResultBuffer.get(mKEncr);
        prfResultBuffer.get(mKAut);
        prfResultBuffer.get(mMsk);
        prfResultBuffer.get(mEmsk);

        // Log as hash unless PII debug mode enabled
        LOG.d(tag, "MK input=" + LOG.pii(mkInput));
        LOG.d(tag, "MK=" + LOG.pii(mk));
        LOG.d(tag, "K_encr=" + LOG.pii(mKEncr));
        LOG.d(tag, "K_aut=" + LOG.pii(mKAut));
        LOG.d(tag, "MSK=" + LOG.pii(mMsk));
        LOG.d(tag, "EMSK=" + LOG.pii(mEmsk));
    }

    @VisibleForTesting
    byte[] processUiccAuthentication(String tag, int authType, byte[] formattedChallenge) throws
            EapSimAkaAuthenticationFailureException {
        String base64Challenge = Base64.encodeToString(formattedChallenge, Base64.NO_WRAP);
        String base64Response =
                mTelephonyManager.getIccAuthentication(
                        mEapUiccConfig.apptype,
                        authType,
                        base64Challenge);

        if (base64Response == null) {
            String msg = "UICC authentication failed. Input: " + LOG.pii(formattedChallenge);
            LOG.e(tag, msg);
            throw new EapSimAkaAuthenticationFailureException(msg);
        }

        return Base64.decode(base64Response, Base64.DEFAULT);
    }

    @VisibleForTesting
    boolean isValidMac(String tag, EapMessage message, EapSimAkaTypeData typeData, byte[] extraData)
            throws GeneralSecurityException, EapSimAkaInvalidAttributeException,
                    EapSilentException {
        mMacAlgorithm = Mac.getInstance(getMacAlgorithm());
        mMacAlgorithm.init(new SecretKeySpec(mKAut, getMacAlgorithm()));

        LOG.d(tag, "Computing MAC (raw msg): " + LOG.pii(message.encode()));

        byte[] mac = getMac(message.eapCode, message.eapIdentifier, typeData, extraData);
        // attributes are 'valid', so must have AtMac
        AtMac atMac = (AtMac) typeData.attributeMap.get(EAP_AT_MAC);

        boolean isValidMac = Arrays.equals(mac, atMac.mac);
        if (!isValidMac) {
            // MAC in message != calculated mac
            LOG.e(
                    tag,
                    "Received message with invalid Mac."
                            + " received=" + Log.byteArrayToHexString(atMac.mac)
                            + ", computed=" + Log.byteArrayToHexString(mac));
        }

        return isValidMac;
    }

    @VisibleForTesting
    byte[] getMac(int eapCode, int eapIdentifier, EapSimAkaTypeData typeData, byte[] extraData)
            throws EapSimAkaInvalidAttributeException, EapSilentException {
        if (mMacAlgorithm == null) {
            throw new IllegalStateException(
                    "Can't calculate MAC before mMacAlgorithm is set in ChallengeState");
        }

        // cache original Mac so it can be restored after calculating the Mac
        AtMac originalMac = (AtMac) typeData.attributeMap.get(EAP_AT_MAC);
        typeData.attributeMap.put(EAP_AT_MAC, new AtMac());

        byte[] typeDataWithEmptyMac = typeData.encode();
        EapData eapData = new EapData(getEapMethod(), typeDataWithEmptyMac);
        EapMessage messageForMac = new EapMessage(eapCode, eapIdentifier, eapData);

        LOG.d(this.getClass().getSimpleName(),
                "Computing MAC (mac cleared): " + LOG.pii(messageForMac.encode()));

        ByteBuffer buffer = ByteBuffer.allocate(messageForMac.eapLength + extraData.length);
        buffer.put(messageForMac.encode());
        buffer.put(extraData);
        byte[] mac = mMacAlgorithm.doFinal(buffer.array());

        typeData.attributeMap.put(EAP_AT_MAC, originalMac);

        // need HMAC-SHA1-128 - first 16 bytes of SHA1 (RFC 4186#10.14, RFC 4187#10.15)
        return Arrays.copyOfRange(mac, 0, AtMac.MAC_LENGTH);
    }

    @VisibleForTesting
    EapResult buildResponseMessageWithMac(int identifier, int eapSubtype, byte[] extraData) {
        // capacity of 1 for AtMac to be added
        return buildResponseMessageWithMac(identifier, eapSubtype, extraData, new ArrayList<>(1));
    }

    @VisibleForTesting
    EapResult buildResponseMessageWithMac(
            int identifier, int eapSubtype, byte[] extraData, List<EapSimAkaAttribute> attributes) {
        try {
            attributes = new ArrayList<>(attributes);
            attributes.add(new AtMac());
            EapSimAkaTypeData eapSimAkaTypeData = getEapSimAkaTypeData(eapSubtype, attributes);

            byte[] mac = getMac(EAP_CODE_RESPONSE, identifier, eapSimAkaTypeData, extraData);

            eapSimAkaTypeData.attributeMap.put(EAP_AT_MAC, new AtMac(mac));
            EapData eapData = new EapData(getEapMethod(), eapSimAkaTypeData.encode());
            EapMessage eapMessage = new EapMessage(EAP_CODE_RESPONSE, identifier, eapData);
            return EapResponse.getEapResponse(eapMessage);
        } catch (EapSimAkaInvalidAttributeException | EapSilentException ex) {
            // this should never happen
            return new EapError(ex);
        }
    }

    @VisibleForTesting
    EapResult handleEapSimAkaNotification(
            String tag,
            boolean isPreChallengeState,
            int identifier,
            EapSimAkaTypeData eapSimAkaTypeData) {
        // EAP-SIM exchanges must not include more than one EAP-SIM notification round
        // (RFC 4186#6.1, RFC 4187#6.1)
        if (mHasReceivedSimAkaNotification) {
            return new EapError(
                    new EapInvalidRequestException("Received multiple EAP-SIM notifications"));
        }

        mHasReceivedSimAkaNotification = true;
        AtNotification atNotification =
                (AtNotification) eapSimAkaTypeData.attributeMap.get(EAP_AT_NOTIFICATION);

        LOG.d(
                tag,
                "Received AtNotification:"
                        + " S=" + (atNotification.isSuccessCode ? "1" : "0")
                        + " P=" + (atNotification.isPreSuccessfulChallenge ? "1" : "0")
                        + " Code=" + atNotification.notificationCode);

        // P bit of notification code is only allowed after a successful challenge round. This is
        // only possible in the ChallengeState (RFC 4186#6.1, RFC 4187#6.1)
        if (isPreChallengeState && !atNotification.isPreSuccessfulChallenge) {
            return buildClientErrorResponse(
                    identifier, getEapMethod(), AtClientErrorCode.UNABLE_TO_PROCESS);
        }

        if (atNotification.isPreSuccessfulChallenge) {
            // AT_MAC attribute must not be included when the P bit is set (RFC 4186#9.8,
            // RFC 4187#9.10)
            if (eapSimAkaTypeData.attributeMap.containsKey(EAP_AT_MAC)) {
                return buildClientErrorResponse(
                        identifier, getEapMethod(), AtClientErrorCode.UNABLE_TO_PROCESS);
            }

            return buildResponseMessage(
                    getEapMethod(), eapSimAkaTypeData.eapSubtype, identifier, Arrays.asList());
        } else if (!eapSimAkaTypeData.attributeMap.containsKey(EAP_AT_MAC)) {
            // MAC must be included for messages with their P bit not set (RFC 4186#9.8,
            // RFC 4187#9.10)
            return buildClientErrorResponse(
                    identifier, getEapMethod(), AtClientErrorCode.UNABLE_TO_PROCESS);
        }

        try {
            byte[] mac = getMac(EAP_CODE_REQUEST, identifier, eapSimAkaTypeData, new byte[0]);

            AtMac atMac = (AtMac) eapSimAkaTypeData.attributeMap.get(EAP_AT_MAC);
            if (!Arrays.equals(mac, atMac.mac)) {
                // MAC in message != calculated mac
                return buildClientErrorResponse(
                        identifier, getEapMethod(), AtClientErrorCode.UNABLE_TO_PROCESS);
            }
        } catch (EapSilentException | EapSimAkaInvalidAttributeException ex) {
            // We can't continue if the MAC can't be generated
            return new EapError(ex);
        }

        // server has been authenticated, so we can send a response
        return buildResponseMessageWithMac(identifier, eapSimAkaTypeData.eapSubtype, new byte[0]);
    }

    abstract EapSimAkaTypeData getEapSimAkaTypeData(AtClientErrorCode clientErrorCode);
    abstract EapSimAkaTypeData getEapSimAkaTypeData(
            int eapSubtype, List<EapSimAkaAttribute> attributes);
}
