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

package com.android.internal.net.eap.message.mschapv2;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

public class EapMsChapV2PacketDefinitions {
    public static final String ID = "1F";
    public static final int ID_INT = Integer.parseInt(ID, 16 /* radix */);

    public static final String CHALLENGE = "000102030405060708090A0B0C0D0E0F";
    public static final byte[] CHALLENGE_BYTES = hexStringToByteArray(CHALLENGE);

    // server name is the ASCII hex for "authenticator@android.net"
    public static final String SERVER_NAME = "61757468656E74696361746F7240616E64726F69642E6E6574";
    public static final byte[] SERVER_NAME_BYTES = hexStringToByteArray(SERVER_NAME);
    public static final byte[] EAP_MSCHAP_V2_CHALLENGE_REQUEST =
            hexStringToByteArray("01" + ID + "002E10" + CHALLENGE + SERVER_NAME);

    public static final byte[] CHALLENGE_REQUEST_WRONG_OP_CODE = hexStringToByteArray("02");
    public static final String SHORT_CHALLENGE = "001122334455";
    public static final byte[] SHORT_CHALLENGE_BYTES = hexStringToByteArray(SHORT_CHALLENGE);
    public static final byte[] CHALLENGE_REQUEST_SHORT_CHALLENGE =
            hexStringToByteArray("01" + ID + "002406" + SHORT_CHALLENGE + SERVER_NAME);
    public static final byte[] CHALLENGE_REQUEST_SHORT_MS_LENGTH =
            hexStringToByteArray("01" + ID + "000110" + CHALLENGE + SERVER_NAME);
    public static final byte[] CHALLENGE_REQUEST_LONG_MS_LENGTH =
            hexStringToByteArray("01" + ID + "00FF10" + CHALLENGE + SERVER_NAME);

    public static final String PEER_CHALLENGE = "00112233445566778899AABBCCDDEEFF";
    public static final byte[] PEER_CHALLENGE_BYTES = hexStringToByteArray(PEER_CHALLENGE);
    public static final String NT_RESPONSE = "FFEEDDCCBBAA998877665544332211000011223344556677";
    public static final byte[] NT_RESPONSE_BYTES = hexStringToByteArray(NT_RESPONSE);

    // peer name is the ASCII hex for "peer@android.net"
    public static final String PEER_NAME = "7065657240616E64726F69642E6E6574";
    public static final byte[] PEER_NAME_BYTES = hexStringToByteArray(PEER_NAME);
    public static final byte[] EAP_MSCHAP_V2_CHALLENGE_RESPONSE =
            hexStringToByteArray(
                    "02"
                            + ID
                            + "004631"
                            + PEER_CHALLENGE
                            + "0000000000000000"
                            + NT_RESPONSE
                            + "00"
                            + PEER_NAME);

    public static final byte[] SHORT_NT_RESPONSE = hexStringToByteArray("0011223344");

    public static final String AUTH_STRING = "00112233445566778899AABBCCDDEEFF00112233";

    // ASCII hex for AUTH_STRING
    public static final String AUTH_STRING_HEX =
            "30303131323233333434353536363737383839394141424243434444454546463030313132323333";
    public static final byte[] AUTH_BYTES = hexStringToByteArray(AUTH_STRING);

    // hex("S=") + AUTH_STRING_HEX
    public static final String FORMATTED_AUTH_STRING = "533D" + AUTH_STRING_HEX;

    public static final String SPACE_HEX = "20";

    // ASCII hex for: "test Android 1234"
    public static final String MESSAGE = "test Android 1234";
    public static final String MESSAGE_HEX = "7465737420416E64726F69642031323334";

    // hex("M=") + MESSAGE_HEX
    public static final String FORMATTED_MESSAGE = "4D3D" + MESSAGE_HEX;

    public static final byte[] EAP_MSCHAP_V2_SUCCESS_REQUEST =
            hexStringToByteArray(
                    "03" + ID + "0042" + FORMATTED_AUTH_STRING + SPACE_HEX + FORMATTED_MESSAGE);
    public static final byte[] EAP_MSCHAP_V2_SUCCESS_REQUEST_EMPTY_MESSAGE =
            hexStringToByteArray("03" + ID + "0031" + FORMATTED_AUTH_STRING + SPACE_HEX + "4D3D");
    public static final byte[] EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE =
            hexStringToByteArray("03" + ID + "002E" + FORMATTED_AUTH_STRING);
    public static final byte[] EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE_WITH_SPACE =
            hexStringToByteArray("03" + ID + "002F" + FORMATTED_AUTH_STRING + SPACE_HEX);
    public static final String MESSAGE_MISSING_TEXT = "<omitted by authenticator>";

    public static final String SHORT_AUTH_STRING = "001122334455";

    public static final byte[] SUCCESS_REQUEST_WRONG_OP_CODE = hexStringToByteArray("02");

    // message format: hex("M=") + AUTH_STRING_HEX + hex("M=") + MESSAGE_HEX
    public static final byte[] SUCCESS_REQUEST_WRONG_PREFIX =
            hexStringToByteArray("03" + ID + "00314D3D" + AUTH_STRING_HEX + SPACE_HEX + "4D3D");

    // message format: hex("S=") + SHORT_AUTH_STRING + hex("M=") + MESSAGE_HEX
    public static final byte[] SUCCESS_REQUEST_SHORT_AUTH_STRING =
            hexStringToByteArray("03" + ID + "0031533D" + SHORT_AUTH_STRING + SPACE_HEX + "4D3D");

    public static final String INVALID_AUTH_HEX =
            "3030313132323333343435353636373738383939414142424343444445454646303031317A7A7979";
    public static final byte[] SUCCESS_REQUEST_INVALID_AUTH_STRING =
            hexStringToByteArray("03" + ID + "0031533D" + INVALID_AUTH_HEX + SPACE_HEX + "4D3D");

    // extra key-value: hex("N=12")
    public static final String EXTRA_KEY = "4E3D3132";
    public static final byte[] SUCCESS_REQUEST_EXTRA_ATTRIBUTE =
            hexStringToByteArray(
                    "03"
                            + ID
                            + "0042"
                            + FORMATTED_AUTH_STRING
                            + SPACE_HEX
                            + EXTRA_KEY
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);

    public static final String SUCCESS_REQUEST = "S=" + AUTH_STRING + " M=" + MESSAGE;
    public static final String EXTRA_M_MESSAGE = "M=" + MESSAGE;
    public static final String SUCCESS_REQUEST_EXTRA_M =
            "S=" + AUTH_STRING + " M=" + EXTRA_M_MESSAGE;
    public static final String SUCCESS_REQUEST_MISSING_M = "S=" + AUTH_STRING;
    public static final String SUCCESS_REQUEST_INVALID_FORMAT =
            "S==" + AUTH_STRING + "M=" + MESSAGE;
    public static final String SUCCESS_REQUEST_DUPLICATE_KEY =
            "S=" + AUTH_STRING + " S=" + AUTH_STRING + " M=" + MESSAGE;

    public static final byte[] EAP_MSCHAP_V2_SUCCESS_RESPONSE = hexStringToByteArray("03");

    public static final int ERROR_CODE = 647; // account disabled

    // formatted error code: hex("E=" + ERROR_CODE)
    public static final String FORMATTED_ERROR_CODE = "453D363437";
    public static final boolean RETRY_BIT = true;

    // formatted retry bit: hex("R=1")
    public static final String FORMATTED_RETRY_BIT = "523D31";

    // challenge hex: hex(CHALLENGE)
    public static final String CHALLENGE_HEX =
            "3030303130323033303430353036303730383039304130423043304430453046";

    // formatted challenge: hex("C=") + CHALLENGE_HEX
    public static final String FORMATTED_CHALLENGE = "433D" + CHALLENGE_HEX;

    public static final int PASSWORD_CHANGE_PROTOCOL = 3;

    // formatted password change protocol: hex("V=3")
    public static final String FORMATTED_PASSWORD_CHANGE_PROTOCOL = "563D33";

    public static final byte[] EAP_MSCHAP_V2_FAILURE_REQUEST =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0048"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);
    public static final byte[] EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0034"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL);
    public static final byte[] EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE_WITH_SPACE =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0035"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX);

    // invalid error code: hex("E=abc")
    public static final String INVALID_ERROR_CODE = "453D616263";
    public static final byte[] FAILURE_REQUEST_INVALID_ERROR_CODE =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0048"
                            + INVALID_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);

    // invalid challenge: hex("C=zyxd")
    public static final String INVALID_CHALLENGE = "433D7A797864";
    public static final byte[] FAILURE_REQUEST_INVALID_CHALLENGE =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0032"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + INVALID_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);

    // short challenge: hex("C=" + SHORT_CHALLENGE)
    public static final String FORMATTED_SHORT_CHALLENGE = "433D303031313232333334343535";
    public static final byte[] FAILURE_REQUEST_SHORT_CHALLENGE =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0034"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_SHORT_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);

    // invalid password change protocol: hex("V=d")
    public static final String INVALID_PASSWORD_CHANGE_PROTOCOL = "563D64";
    public static final byte[] FAILURE_REQUEST_INVALID_PASSWORD_CHANGE_PROTOCOL =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0048"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_CHALLENGE
                            + SPACE_HEX
                            + INVALID_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);

    public static final byte[] FAILURE_REQUEST_EXTRA_ATTRIBUTE =
            hexStringToByteArray(
                    "04"
                            + ID
                            + "0048"
                            + FORMATTED_ERROR_CODE
                            + SPACE_HEX
                            + FORMATTED_RETRY_BIT
                            + SPACE_HEX
                            + FORMATTED_CHALLENGE
                            + SPACE_HEX
                            + FORMATTED_PASSWORD_CHANGE_PROTOCOL
                            + SPACE_HEX
                            + EXTRA_KEY
                            + SPACE_HEX
                            + FORMATTED_MESSAGE);

    public static final byte[] EAP_MSCHAP_V2_FAILURE_RESPONSE = hexStringToByteArray("04");
}
