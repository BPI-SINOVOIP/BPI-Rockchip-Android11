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

package com.android.internal.net.eap.message.simaka.attributes;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

/**
 * EapTestAttributeDefinitions provides byte[] encodings of commonly used EAP Messages.
 *
 * @ee <a href="https://tools.ietf.org/html/rfc4186#section-10">RFC 4186, EAP-SIM Authentication,
 * Section 10</a>
 * @see <a href="https://tools.ietf.org/html/rfc4187#section-10">RFC 4187, EAP-AKA Authentication,
 * Section 10</a>
 */
public class EapTestAttributeDefinitions {
    public static final String VERSION = "0001";
    public static final String AT_VERSION_LIST_DATA = "0002" + VERSION + "0000";
    public static final byte[] AT_VERSION_LIST =
            hexStringToByteArray("0F02" + AT_VERSION_LIST_DATA);
    public static final byte[] AT_SELECTED_VERSION = hexStringToByteArray("10010001");
    public static final String NONCE_MT_STRING = "0123456789ABCDEFFEDCBA9876543210";
    public static final byte[] NONCE_MT = hexStringToByteArray(NONCE_MT_STRING);
    public static final byte[] AT_NONCE_MT = hexStringToByteArray("07050000" + NONCE_MT_STRING);
    public static final byte[] AT_PERMANENT_ID_REQ = hexStringToByteArray("0A010000");
    public static final byte[] AT_ANY_ID_REQ = hexStringToByteArray("0D010000");
    public static final byte[] AT_FULL_AUTH_ID_REQ = hexStringToByteArray("11010000");

    // Identity = "test1@android.net"
    public static final String IDENTITY_STRING = "746573743140616E64726F69642E6E6574";
    public static final byte[] IDENTITY = hexStringToByteArray(IDENTITY_STRING);
    public static final byte[] AT_IDENTITY =
            hexStringToByteArray("0E060011" + IDENTITY_STRING + "000000");
    public static final String RAND_1 = "00112233445566778899AABBCCDDEEFF";
    public static final byte[] RAND_1_BYTES = hexStringToByteArray(RAND_1);
    public static final String RAND_2 = "FFEEDDCCBBAA99887766554433221100";
    public static final byte[] RAND_2_BYTES = hexStringToByteArray(RAND_2);
    public static final byte[] AT_RAND_SIM = hexStringToByteArray("01090000" + RAND_1 + RAND_2);
    public static final byte[] AT_RAND_AKA = hexStringToByteArray("01050000" + RAND_1);
    public static final byte[] AT_PADDING = hexStringToByteArray("0602000000000000");
    public static final String MAC = "112233445566778899AABBCCDDEEFF11";
    public static final byte[] MAC_BYTES = hexStringToByteArray(MAC);
    public static final byte[] AT_MAC = hexStringToByteArray("0B050000" + MAC);
    public static final String COUNTER = "000A";
    public static final int COUNTER_INT = Integer.parseInt(COUNTER, 16 /* radix */);
    public static final byte[] AT_COUNTER = hexStringToByteArray("1301" + COUNTER);
    public static final byte[] AT_COUNTER_TOO_SMALL = hexStringToByteArray("14010000");
    public static final String NONCE_S = "0123456789ABCDEFFEDCBA9876543210";
    public static final byte[] AT_NONCE_S = hexStringToByteArray("15050000" + NONCE_S);
    public static final String NOTIFICATION_CODE = "8000";
    public static final byte[] AT_NOTIFICATION = hexStringToByteArray("0C01" + NOTIFICATION_CODE);
    public static final String ERROR_CODE = "0001";
    public static final byte[] AT_CLIENT_ERROR_CODE = hexStringToByteArray("1601" + ERROR_CODE);
    public static final String AUTN = "0123456789ABCDEFFEDCBA9876543210";
    public static final byte[] AUTN_BYTES = hexStringToByteArray(AUTN);
    public static final byte[] AT_AUTN = hexStringToByteArray("02050000" + AUTN);
    public static final String RES = "1122334455";
    public static final byte[] RES_BYTES = hexStringToByteArray(RES);
    public static final byte[] AT_RES = hexStringToByteArray("03030028" + RES + "000000");
    public static final String AUTS = "112233445566778899AABBCCDDEE";
    public static final byte[] AUTS_BYTES = hexStringToByteArray(AUTS);
    public static final byte[] AT_AUTS = hexStringToByteArray("0404" + AUTS);
    public static final byte[] AT_BIDDING_SUPPORTS_AKA_PRIME = hexStringToByteArray("88018000");
    public static final byte[] AT_BIDDING_DOES_NOT_SUPPORT_AKA_PRIME =
            hexStringToByteArray("88010000");

    // Network Name = "android.net"
    public static final String NETWORK_NAME_HEX = "616E64726F69642E6E6574";
    public static final byte[] NETWORK_NAME_BYTES = hexStringToByteArray(NETWORK_NAME_HEX);
    public static final byte[] AT_KDF_INPUT =
            hexStringToByteArray("1704000B" + NETWORK_NAME_HEX + "00");
    public static final byte[] AT_KDF_INPUT_EMPTY_NETWORK_NAME = hexStringToByteArray("17010000");
    public static final int KDF_VERSION = 1;
    public static final byte[] AT_KDF = hexStringToByteArray("18010001");

    public static final byte[] AT_VERSION_LIST_INVALID_LENGTH = hexStringToByteArray("0F020003");
    public static final byte[] AT_SELECTED_VERSION_INVALID_LENGTH =
            hexStringToByteArray("10020001");
    public static final byte[] AT_NONCE_INVALID_LENGTH =
            hexStringToByteArray("07060000" + NONCE_MT_STRING);
    public static final byte[] PERMANENT_ID_INVALID_LENGTH = hexStringToByteArray("0A020000");
    public static final byte[] ANY_ID_INVALID_LENGTH = hexStringToByteArray("0D020000");
    public static final byte[] FULL_AUTH_ID_INVALID_LENGTH = hexStringToByteArray("11020000");
    public static final byte[] AT_RAND_SIM_INVALID_NUM_RANDS =
            hexStringToByteArray("01050000" + RAND_1);
    public static final byte[] AT_RAND_SIM_DUPLICATE_RANDS =
            hexStringToByteArray("01090000" + RAND_1 + RAND_1);
    public static final byte[] AT_RAND_AKA_INVALID_LENGTH = hexStringToByteArray("01010000");
    public static final byte[] AT_PADDING_INVALID_PADDING = hexStringToByteArray("0601FFFF");
    public static final byte[] AT_MAC_INVALID_LENGTH = hexStringToByteArray("0B06");
    public static final byte[] AT_COUNTER_INVALID_LENGTH = hexStringToByteArray("1302");
    public static final byte[] AT_COUNTER_TOO_SMALL_INVALID_LENGTH = hexStringToByteArray("1402");
    public static final byte[] AT_NONCE_S_INVALID_LENGTH = hexStringToByteArray("1506");
    public static final byte[] AT_NOTIFICATION_INVALID_LENGTH = hexStringToByteArray("0C02");
    public static final byte[] AT_NOTIFICATION_INVALID_STATE = hexStringToByteArray("0C01C000");
    public static final byte[] AT_CLIENT_ERROR_CODE_INVALID_LENGTH = hexStringToByteArray("1602");
    public static final byte[] AT_AUTN_INVALID_LENGTH = hexStringToByteArray("02010000");
    public static final byte[] AT_RES_INVALID_RES_LENGTH =
            hexStringToByteArray("030300241122334450000000");
    public static final byte[] AT_RES_SHORT_RES =
            hexStringToByteArray("0302000811000000");
    public static final byte[] AT_RES_LONG_RES =
            hexStringToByteArray("0306008800112233445566778899AABBCCDDEEFF11000000");
    public static final byte[] AT_AUTS_INVALID_LENGTH = hexStringToByteArray("03010000");
    public static final byte[] AT_KDF_INVALID_LENGTH = hexStringToByteArray("18020001");
    public static final byte[] AT_BIDDING_INVALID_LENGTH = hexStringToByteArray("88020000");
}
