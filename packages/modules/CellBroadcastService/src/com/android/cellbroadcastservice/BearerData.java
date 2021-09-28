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

package com.android.cellbroadcastservice;

import android.content.Context;
import android.telephony.SmsCbCmasInfo;
import android.telephony.cdma.CdmaSmsCbProgramData;
import android.util.Log;

/**
 * An object to decode CDMA SMS bearer data.
 */
public final class BearerData {
    private final static String LOG_TAG = "BearerData";

    /**
     * Bearer Data Subparameter Identifiers
     * (See 3GPP2 C.S0015-B, v2.0, table 4.5-1)
     * NOTE: Unneeded subparameter types are not included
     */
    private static final byte SUBPARAM_MESSAGE_IDENTIFIER = 0x00;
    private static final byte SUBPARAM_USER_DATA = 0x01;
    private static final byte SUBPARAM_PRIORITY_INDICATOR = 0x08;
    private static final byte SUBPARAM_LANGUAGE_INDICATOR = 0x0D;

    // All other values after this are reserved.
    private static final byte SUBPARAM_ID_LAST_DEFINED = 0x17;

    /**
     * Supported priority modes for CDMA SMS messages
     * (See 3GPP2 C.S0015-B, v2.0, table 4.5.9-1)
     */
    public static final int PRIORITY_NORMAL = 0x0;
    public static final int PRIORITY_INTERACTIVE = 0x1;
    public static final int PRIORITY_URGENT = 0x2;
    public static final int PRIORITY_EMERGENCY = 0x3;

    /**
     * Language Indicator values.  NOTE: the spec (3GPP2 C.S0015-B,
     * v2, 4.5.14) is ambiguous as to the meaning of this field, as it
     * refers to C.R1001-D but that reference has been crossed out.
     * It would seem reasonable to assume the values from C.R1001-F
     * (table 9.2-1) are to be used instead.
     */
    public static final int LANGUAGE_UNKNOWN = 0x00;
    public static final int LANGUAGE_ENGLISH = 0x01;
    public static final int LANGUAGE_FRENCH = 0x02;
    public static final int LANGUAGE_SPANISH = 0x03;
    public static final int LANGUAGE_JAPANESE = 0x04;
    public static final int LANGUAGE_KOREAN = 0x05;
    public static final int LANGUAGE_CHINESE = 0x06;
    public static final int LANGUAGE_HEBREW = 0x07;

    /**
     * Supported message types for CDMA SMS messages
     * (See 3GPP2 C.S0015-B, v2.0, table 4.5.1-1)
     * Used for CdmaSmsCbTest.
     */
    public static final int MESSAGE_TYPE_DELIVER        = 0x01;

    /**
     * 16-bit value indicating the message ID, which increments modulo 65536.
     * (Special rules apply for WAP-messages.)
     * (See 3GPP2 C.S0015-B, v2, 4.5.1)
     */
    public int messageId;

    /**
     * Priority modes for CDMA SMS message (See 3GPP2 C.S0015-B, v2.0, table 4.5.9-1)
     */
    public int priority = PRIORITY_NORMAL;

    /**
     * Language indicator for CDMA SMS message.
     */
    public int language = LANGUAGE_UNKNOWN;

    /**
     * 1-bit value that indicates whether a User Data Header (UDH) is present.
     * (See 3GPP2 C.S0015-B, v2, 4.5.1)
     *
     * NOTE: during encoding, this value will be set based on the
     * presence of a UDH in the structured data, any existing setting
     * will be overwritten.
     */
    public boolean hasUserDataHeader;

    /**
     * Information on the user data
     * (e.g. padding bits, user data, user data header, etc)
     * (See 3GPP2 C.S.0015-B, v2, 4.5.2)
     */
    public UserData userData;

    /**
     * CMAS warning notification information.
     *
     * @see #decodeCmasUserData(BearerData, int)
     */
    public SmsCbCmasInfo cmasWarningInfo;

    /**
     * Construct an empty BearerData.
     */
    private BearerData() {
    }

    private static class CodingException extends Exception {
        public CodingException(String s) {
            super(s);
        }
    }

    /**
     * Returns the language indicator as a two-character ISO 639 string.
     *
     * @return a two character ISO 639 language code
     */
    public String getLanguage() {
        return getLanguageCodeForValue(language);
    }

    /**
     * Converts a CDMA language indicator value to an ISO 639 two character language code.
     *
     * @param languageValue the CDMA language value to convert
     * @return the two character ISO 639 language code for the specified value, or null if unknown
     */
    private static String getLanguageCodeForValue(int languageValue) {
        switch (languageValue) {
            case LANGUAGE_ENGLISH:
                return "en";

            case LANGUAGE_FRENCH:
                return "fr";

            case LANGUAGE_SPANISH:
                return "es";

            case LANGUAGE_JAPANESE:
                return "ja";

            case LANGUAGE_KOREAN:
                return "ko";

            case LANGUAGE_CHINESE:
                return "zh";

            case LANGUAGE_HEBREW:
                return "he";

            default:
                return null;
        }
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("BearerData ");
        builder.append(", messageId=" + messageId);
        builder.append(", hasUserDataHeader=" + hasUserDataHeader);
        builder.append(", userData=" + userData);
        builder.append(" }");
        return builder.toString();
    }

    private static boolean decodeMessageId(BearerData bData, BitwiseInputStream inStream)
            throws BitwiseInputStream.AccessException {
        final int EXPECTED_PARAM_SIZE = 3 * 8;
        boolean decodeSuccess = false;
        int paramBits = inStream.read(8) * 8;
        if (paramBits >= EXPECTED_PARAM_SIZE) {
            paramBits -= EXPECTED_PARAM_SIZE;
            decodeSuccess = true;
            inStream.skip(4); // skip messageType
            bData.messageId = inStream.read(8) << 8;
            bData.messageId |= inStream.read(8);
            bData.hasUserDataHeader = (inStream.read(1) == 1);
            inStream.skip(3);
        }
        if ((!decodeSuccess) || (paramBits > 0)) {
            Log.d(LOG_TAG, "MESSAGE_IDENTIFIER decode "
                    + (decodeSuccess ? "succeeded" : "failed")
                    + " (extra bits = " + paramBits + ")");
        }
        inStream.skip(paramBits);
        return decodeSuccess;
    }

    private static boolean decodeReserved(BitwiseInputStream inStream, int subparamId)
            throws BitwiseInputStream.AccessException, CodingException {
        boolean decodeSuccess = false;
        int subparamLen = inStream.read(8); // SUBPARAM_LEN
        int paramBits = subparamLen * 8;
        if (paramBits <= inStream.available()) {
            decodeSuccess = true;
            inStream.skip(paramBits);
        }
        Log.d(LOG_TAG, "RESERVED bearer data subparameter " + subparamId + " decode "
                + (decodeSuccess ? "succeeded" : "failed") + " (param bits = " + paramBits + ")");
        if (!decodeSuccess) {
            throw new CodingException("RESERVED bearer data subparameter " + subparamId
                    + " had invalid SUBPARAM_LEN " + subparamLen);
        }

        return decodeSuccess;
    }

    private static boolean decodeUserData(BearerData bData, BitwiseInputStream inStream)
            throws BitwiseInputStream.AccessException {
        int paramBits = inStream.read(8) * 8;
        bData.userData = new UserData();
        bData.userData.msgEncoding = inStream.read(5);
        bData.userData.msgEncodingSet = true;
        bData.userData.msgType = 0;
        int consumedBits = 5;
        if ((bData.userData.msgEncoding == UserData.ENCODING_IS91_EXTENDED_PROTOCOL) ||
                (bData.userData.msgEncoding == UserData.ENCODING_GSM_DCS)) {
            bData.userData.msgType = inStream.read(8);
            consumedBits += 8;
        }
        bData.userData.numFields = inStream.read(8);
        consumedBits += 8;
        int dataBits = paramBits - consumedBits;
        bData.userData.payload = inStream.readByteArray(dataBits);
        return true;
    }

    private static String decodeUtf8(byte[] data, int offset, int numFields)
            throws CodingException {
        return decodeCharset(data, offset, numFields, 1, "UTF-8");
    }

    private static String decodeUtf16(byte[] data, int offset, int numFields)
            throws CodingException {
        // Subtract header and possible padding byte (at end) from num fields.
        int padding = offset % 2;
        numFields -= (offset + padding) / 2;
        return decodeCharset(data, offset, numFields, 2, "utf-16be");
    }

    private static String decodeCharset(byte[] data, int offset, int numFields, int width,
            String charset) throws CodingException {
        if (numFields < 0 || (numFields * width + offset) > data.length) {
            // Try to decode the max number of characters in payload
            int padding = offset % width;
            int maxNumFields = (data.length - offset - padding) / width;
            if (maxNumFields < 0) {
                throw new CodingException(charset + " decode failed: offset out of range");
            }
            Log.e(LOG_TAG, charset + " decode error: offset = " + offset + " numFields = "
                    + numFields + " data.length = " + data.length + " maxNumFields = "
                    + maxNumFields);
            numFields = maxNumFields;
        }
        try {
            return new String(data, offset, numFields * width, charset);
        } catch (java.io.UnsupportedEncodingException ex) {
            throw new CodingException(charset + " decode failed: " + ex);
        }
    }

    private static String decode7bitAscii(byte[] data, int offset, int numFields)
            throws CodingException {
        try {
            int offsetBits = offset * 8;
            int offsetSeptets = (offsetBits + 6) / 7;
            numFields -= offsetSeptets;

            StringBuffer strBuf = new StringBuffer(numFields);
            BitwiseInputStream inStream = new BitwiseInputStream(data);
            int wantedBits = (offsetSeptets * 7) + (numFields * 7);
            if (inStream.available() < wantedBits) {
                throw new CodingException("insufficient data (wanted " + wantedBits +
                        " bits, but only have " + inStream.available() + ")");
            }
            inStream.skip(offsetSeptets * 7);
            for (int i = 0; i < numFields; i++) {
                int charCode = inStream.read(7);
                if ((charCode >= UserData.ASCII_MAP_BASE_INDEX) &&
                        (charCode <= UserData.ASCII_MAP_MAX_INDEX)) {
                    strBuf.append(UserData.ASCII_MAP[charCode - UserData.ASCII_MAP_BASE_INDEX]);
                } else if (charCode == UserData.ASCII_NL_INDEX) {
                    strBuf.append('\n');
                } else if (charCode == UserData.ASCII_CR_INDEX) {
                    strBuf.append('\r');
                } else {
                    /* For other charCodes, they are unprintable, and so simply use SPACE. */
                    strBuf.append(' ');
                }
            }
            return strBuf.toString();
        } catch (BitwiseInputStream.AccessException ex) {
            throw new CodingException("7bit ASCII decode failed: " + ex);
        }
    }

    private static String decode7bitGsm(byte[] data, int offset, int numFields)
            throws CodingException {
        // Start reading from the next 7-bit aligned boundary after offset.
        int offsetBits = offset * 8;
        int offsetSeptets = (offsetBits + 6) / 7;
        numFields -= offsetSeptets;
        int paddingBits = (offsetSeptets * 7) - offsetBits;
        String result = GsmAlphabet.gsm7BitPackedToString(data, offset, numFields,
                paddingBits, 0, 0);
        if (result == null) {
            throw new CodingException("7bit GSM decoding failed");
        }
        return result;
    }

    private static String decodeLatin(byte[] data, int offset, int numFields)
            throws CodingException {
        return decodeCharset(data, offset, numFields, 1, "ISO-8859-1");
    }

    private static String decodeShiftJis(byte[] data, int offset, int numFields)
            throws CodingException {
        return decodeCharset(data, offset, numFields, 1, "Shift_JIS");
    }

    private static String decodeGsmDcs(byte[] data, int offset, int numFields,
            int msgType)
            throws CodingException {
        if ((msgType & 0xC0) != 0) {
            throw new CodingException("unsupported coding group ("
                    + msgType + ")");
        }

        switch ((msgType >> 2) & 0x3) {
            case UserData.ENCODING_GSM_DCS_7BIT:
                return decode7bitGsm(data, offset, numFields);
            case UserData.ENCODING_GSM_DCS_8BIT:
                return decodeUtf8(data, offset, numFields);
            case UserData.ENCODING_GSM_DCS_16BIT:
                return decodeUtf16(data, offset, numFields);
            default:
                throw new CodingException("unsupported user msgType encoding ("
                        + msgType + ")");
        }
    }

    private static void decodeUserDataPayload(Context context, UserData userData,
            boolean hasUserDataHeader) throws CodingException {
        int offset = 0;
        if (hasUserDataHeader) {
            int udhLen = userData.payload[0] & 0x00FF;
            offset += udhLen + 1;
            byte[] headerData = new byte[udhLen];
            System.arraycopy(userData.payload, 1, headerData, 0, udhLen);
            userData.userDataHeader = SmsHeader.fromByteArray(headerData);
        }
        switch (userData.msgEncoding) {
            case UserData.ENCODING_OCTET:
                /*
                 *  Octet decoding depends on the carrier service.
                 */
                boolean decodingtypeUTF8 = context.getResources()
                        .getBoolean(R.bool.config_sms_utf8_support);

                // Strip off any padding bytes, meaning any differences between the length of the
                // array and the target length specified by numFields.  This is to avoid any
                // confusion by code elsewhere that only considers the payload array length.
                byte[] payload = new byte[userData.numFields];
                int copyLen = userData.numFields < userData.payload.length
                        ? userData.numFields : userData.payload.length;

                System.arraycopy(userData.payload, 0, payload, 0, copyLen);
                userData.payload = payload;

                if (!decodingtypeUTF8) {
                    // There are many devices in the market that send 8bit text sms (latin
                    // encoded) as
                    // octet encoded.
                    userData.payloadStr = decodeLatin(userData.payload, offset, userData.numFields);
                } else {
                    userData.payloadStr = decodeUtf8(userData.payload, offset, userData.numFields);
                }
                break;

            case UserData.ENCODING_IA5:
            case UserData.ENCODING_7BIT_ASCII:
                userData.payloadStr = decode7bitAscii(userData.payload, offset, userData.numFields);
                break;
            case UserData.ENCODING_UNICODE_16:
                userData.payloadStr = decodeUtf16(userData.payload, offset, userData.numFields);
                break;
            case UserData.ENCODING_GSM_7BIT_ALPHABET:
                userData.payloadStr = decode7bitGsm(userData.payload, offset,
                        userData.numFields);
                break;
            case UserData.ENCODING_LATIN:
                userData.payloadStr = decodeLatin(userData.payload, offset, userData.numFields);
                break;
            case UserData.ENCODING_SHIFT_JIS:
                userData.payloadStr = decodeShiftJis(userData.payload, offset, userData.numFields);
                break;
            case UserData.ENCODING_GSM_DCS:
                userData.payloadStr = decodeGsmDcs(userData.payload, offset,
                        userData.numFields, userData.msgType);
                break;
            default:
                throw new CodingException("unsupported user data encoding ("
                        + userData.msgEncoding + ")");
        }
    }

    private static boolean decodeLanguageIndicator(BearerData bData, BitwiseInputStream inStream)
            throws BitwiseInputStream.AccessException {
        final int EXPECTED_PARAM_SIZE = 1 * 8;
        boolean decodeSuccess = false;
        int paramBits = inStream.read(8) * 8;
        if (paramBits >= EXPECTED_PARAM_SIZE) {
            paramBits -= EXPECTED_PARAM_SIZE;
            decodeSuccess = true;
            bData.language = inStream.read(8);
        }
        if ((!decodeSuccess) || (paramBits > 0)) {
            Log.d(LOG_TAG, "LANGUAGE_INDICATOR decode "
                    + (decodeSuccess ? "succeeded" : "failed")
                    + " (extra bits = " + paramBits + ")");
        }
        inStream.skip(paramBits);
        return decodeSuccess;
    }

    private static boolean decodePriorityIndicator(BearerData bData, BitwiseInputStream inStream)
            throws BitwiseInputStream.AccessException {
        final int EXPECTED_PARAM_SIZE = 1 * 8;
        boolean decodeSuccess = false;
        int paramBits = inStream.read(8) * 8;
        if (paramBits >= EXPECTED_PARAM_SIZE) {
            paramBits -= EXPECTED_PARAM_SIZE;
            decodeSuccess = true;
            bData.priority = inStream.read(2);
            inStream.skip(6);
        }
        if ((!decodeSuccess) || (paramBits > 0)) {
            Log.d(LOG_TAG, "PRIORITY_INDICATOR decode "
                    + (decodeSuccess ? "succeeded" : "failed")
                    + " (extra bits = " + paramBits + ")");
        }
        inStream.skip(paramBits);
        return decodeSuccess;
    }

    private static int serviceCategoryToCmasMessageClass(int serviceCategory) {
        switch (serviceCategory) {
            case CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT:
                return SmsCbCmasInfo.CMAS_CLASS_PRESIDENTIAL_LEVEL_ALERT;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT:
                return SmsCbCmasInfo.CMAS_CLASS_EXTREME_THREAT;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT:
                return SmsCbCmasInfo.CMAS_CLASS_SEVERE_THREAT;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY:
                return SmsCbCmasInfo.CMAS_CLASS_CHILD_ABDUCTION_EMERGENCY;

            case CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE:
                return SmsCbCmasInfo.CMAS_CLASS_REQUIRED_MONTHLY_TEST;

            default:
                return SmsCbCmasInfo.CMAS_CLASS_UNKNOWN;
        }
    }

    /**
     * CMAS message decoding.
     * (See TIA-1149-0-1, CMAS over CDMA)
     *
     * @param serviceCategory is the service category from the SMS envelope
     */
    private static void decodeCmasUserData(Context context, BearerData bData, int serviceCategory)
            throws BitwiseInputStream.AccessException, CodingException {
        BitwiseInputStream inStream = new BitwiseInputStream(bData.userData.payload);
        if (inStream.available() < 8) {
            throw new CodingException("emergency CB with no CMAE_protocol_version");
        }
        int protocolVersion = inStream.read(8);
        if (protocolVersion != 0) {
            throw new CodingException("unsupported CMAE_protocol_version " + protocolVersion);
        }

        int messageClass = serviceCategoryToCmasMessageClass(serviceCategory);
        int category = SmsCbCmasInfo.CMAS_CATEGORY_UNKNOWN;
        int responseType = SmsCbCmasInfo.CMAS_RESPONSE_TYPE_UNKNOWN;
        int severity = SmsCbCmasInfo.CMAS_SEVERITY_UNKNOWN;
        int urgency = SmsCbCmasInfo.CMAS_URGENCY_UNKNOWN;
        int certainty = SmsCbCmasInfo.CMAS_CERTAINTY_UNKNOWN;

        while (inStream.available() >= 16) {
            int recordType = inStream.read(8);
            int recordLen = inStream.read(8);
            switch (recordType) {
                case 0:     // Type 0 elements (Alert text)
                    UserData alertUserData = new UserData();
                    alertUserData.msgEncoding = inStream.read(5);
                    alertUserData.msgEncodingSet = true;
                    alertUserData.msgType = 0;

                    int numFields;                          // number of chars to decode
                    switch (alertUserData.msgEncoding) {
                        case UserData.ENCODING_OCTET:
                        case UserData.ENCODING_LATIN:
                            numFields = recordLen - 1;      // subtract 1 byte for encoding
                            break;

                        case UserData.ENCODING_IA5:
                        case UserData.ENCODING_7BIT_ASCII:
                        case UserData.ENCODING_GSM_7BIT_ALPHABET:
                            numFields = ((recordLen * 8) - 5) / 7;  // subtract 5 bits for encoding
                            break;

                        case UserData.ENCODING_UNICODE_16:
                            numFields = (recordLen - 1) / 2;
                            break;

                        default:
                            numFields = 0;      // unsupported encoding
                    }

                    alertUserData.numFields = numFields;
                    alertUserData.payload = inStream.readByteArray(recordLen * 8 - 5);
                    decodeUserDataPayload(context, alertUserData, false);
                    bData.userData = alertUserData;
                    break;

                case 1:     // Type 1 elements
                    category = inStream.read(8);
                    responseType = inStream.read(8);
                    severity = inStream.read(4);
                    urgency = inStream.read(4);
                    certainty = inStream.read(4);
                    inStream.skip(recordLen * 8 - 28);
                    break;

                default:
                    Log.w(LOG_TAG, "skipping unsupported CMAS record type " + recordType);
                    inStream.skip(recordLen * 8);
                    break;
            }
        }

        bData.cmasWarningInfo = new SmsCbCmasInfo(messageClass, category, responseType, severity,
                urgency, certainty);
    }

    private static boolean isCmasAlertCategory(int category) {
        return category >= CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT
                && category <= CdmaSmsCbProgramData.CATEGORY_CMAS_LAST_RESERVED_VALUE;
    }

    /**
     * Create BearerData object from serialized representation.
     * (See 3GPP2 C.R1001-F, v1.0, section 4.5 for layout details)
     *
     * @param smsData         byte array of raw encoded SMS bearer data.
     * @param serviceCategory the envelope service category (for CMAS alert handling)
     * @return an instance of BearerData.
     */
    public static BearerData decode(Context context, byte[] smsData, int serviceCategory)
            throws CodingException, BitwiseInputStream.AccessException {
        BitwiseInputStream inStream = new BitwiseInputStream(smsData);
        BearerData bData = new BearerData();
        int foundSubparamMask = 0;
        while (inStream.available() > 0) {
            int subparamId = inStream.read(8);
            int subparamIdBit = 1 << subparamId;
            // int is 4 bytes. This duplicate check has a limit to Id number up to 32 (4*8)
            // as 32th bit is the max bit in int.
            // Per 3GPP2 C.S0015-B Table 4.5-1 Bearer Data Subparameter Identifiers:
            // last defined subparam ID is 23 (00010111 = 0x17 = 23).
            // Only do duplicate subparam ID check if subparam is within defined value as
            // reserved subparams are just skipped.
            if ((foundSubparamMask & subparamIdBit) != 0 && (
                    subparamId >= SUBPARAM_MESSAGE_IDENTIFIER
                            && subparamId <= SUBPARAM_ID_LAST_DEFINED)) {
                throw new CodingException("illegal duplicate subparameter (" + subparamId + ")");
            }
            boolean decodeSuccess;
            switch (subparamId) {
                case SUBPARAM_MESSAGE_IDENTIFIER:
                    decodeSuccess = decodeMessageId(bData, inStream);
                    break;
                case SUBPARAM_USER_DATA:
                    decodeSuccess = decodeUserData(bData, inStream);
                    break;
                case SUBPARAM_LANGUAGE_INDICATOR:
                    decodeSuccess = decodeLanguageIndicator(bData, inStream);
                    break;
                case SUBPARAM_PRIORITY_INDICATOR:
                    decodeSuccess = decodePriorityIndicator(bData, inStream);
                    break;
                default:
                    decodeSuccess = decodeReserved(inStream, subparamId);
            }
            if (decodeSuccess && (subparamId >= SUBPARAM_MESSAGE_IDENTIFIER
                    && subparamId <= SUBPARAM_ID_LAST_DEFINED)) {
                foundSubparamMask |= subparamIdBit;
            }
        }
        if ((foundSubparamMask & (1 << SUBPARAM_MESSAGE_IDENTIFIER)) == 0) {
            throw new CodingException("missing MESSAGE_IDENTIFIER subparam");
        }
        if (bData.userData != null) {
            if (isCmasAlertCategory(serviceCategory)) {
                decodeCmasUserData(context, bData, serviceCategory);
            } else {
                decodeUserDataPayload(context, bData.userData, bData.hasUserDataHeader);
            }
        }
        return bData;
    }
}
