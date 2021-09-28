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

package android.net.ipsec.ike.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.net.InetAddresses;
import android.net.ipsec.ike.IkeTrafficSelector;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Shared parameters and util methods for testing different components of IKE */
abstract class IkeTestBase {
    static final int MIN_PORT = 0;
    static final int MAX_PORT = 65535;
    private static final int INBOUND_TS_START_PORT = MIN_PORT;
    private static final int INBOUND_TS_END_PORT = 65520;
    private static final int OUTBOUND_TS_START_PORT = 16;
    private static final int OUTBOUND_TS_END_PORT = MAX_PORT;

    static final int IP4_ADDRESS_LEN = 4;
    static final int IP6_ADDRESS_LEN = 16;
    static final int IP4_PREFIX_LEN = 32;
    static final int IP6_PREFIX_LEN = 64;

    static final byte[] IKE_PSK = "ikeAndroidPsk".getBytes();

    static final String LOCAL_HOSTNAME = "client.test.ike.android.net";
    static final String REMOTE_HOSTNAME = "server.test.ike.android.net";
    static final String LOCAL_ASN1_DN_STRING = "CN=client.test.ike.android.net, O=Android, C=US";
    static final String LOCAL_RFC822_NAME = "client.test.ike@example.com";
    static final byte[] LOCAL_KEY_ID = "Local Key ID".getBytes();

    static final int SUB_ID = 1;
    static final byte[] EAP_IDENTITY = "test@android.net".getBytes();
    static final String NETWORK_NAME = "android.net";
    static final String EAP_MSCHAPV2_USERNAME = "mschapv2user";
    static final String EAP_MSCHAPV2_PASSWORD = "password";

    static final Inet4Address IPV4_ADDRESS_LOCAL =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.100"));
    static final Inet4Address IPV4_ADDRESS_REMOTE =
            (Inet4Address) (InetAddresses.parseNumericAddress("198.51.100.100"));
    static final Inet6Address IPV6_ADDRESS_LOCAL =
            (Inet6Address) (InetAddresses.parseNumericAddress("2001:db8::100"));
    static final Inet6Address IPV6_ADDRESS_REMOTE =
            (Inet6Address) (InetAddresses.parseNumericAddress("2001:db8:255::100"));

    static final InetAddress PCSCF_IPV4_ADDRESS_1 = InetAddresses.parseNumericAddress("192.0.2.1");
    static final InetAddress PCSCF_IPV4_ADDRESS_2 = InetAddresses.parseNumericAddress("192.0.2.2");
    static final InetAddress PCSCF_IPV6_ADDRESS_1 =
            InetAddresses.parseNumericAddress("2001:DB8::1");
    static final InetAddress PCSCF_IPV6_ADDRESS_2 =
            InetAddresses.parseNumericAddress("2001:DB8::2");

    static final IkeTrafficSelector DEFAULT_V4_TS =
            new IkeTrafficSelector(
                    MIN_PORT,
                    MAX_PORT,
                    InetAddresses.parseNumericAddress("0.0.0.0"),
                    InetAddresses.parseNumericAddress("255.255.255.255"));
    static final IkeTrafficSelector DEFAULT_V6_TS =
            new IkeTrafficSelector(
                    MIN_PORT,
                    MAX_PORT,
                    InetAddresses.parseNumericAddress("::"),
                    InetAddresses.parseNumericAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
    static final IkeTrafficSelector INBOUND_V4_TS =
            new IkeTrafficSelector(
                    INBOUND_TS_START_PORT,
                    INBOUND_TS_END_PORT,
                    InetAddresses.parseNumericAddress("192.0.2.10"),
                    InetAddresses.parseNumericAddress("192.0.2.20"));
    static final IkeTrafficSelector OUTBOUND_V4_TS =
            new IkeTrafficSelector(
                    OUTBOUND_TS_START_PORT,
                    OUTBOUND_TS_END_PORT,
                    InetAddresses.parseNumericAddress("198.51.100.0"),
                    InetAddresses.parseNumericAddress("198.51.100.255"));

    static final IkeTrafficSelector INBOUND_V6_TS =
            new IkeTrafficSelector(
                    INBOUND_TS_START_PORT,
                    INBOUND_TS_END_PORT,
                    InetAddresses.parseNumericAddress("2001:db8::10"),
                    InetAddresses.parseNumericAddress("2001:db8::128"));
    static final IkeTrafficSelector OUTBOUND_V6_TS =
            new IkeTrafficSelector(
                    OUTBOUND_TS_START_PORT,
                    OUTBOUND_TS_END_PORT,
                    InetAddresses.parseNumericAddress("2001:db8:255::64"),
                    InetAddresses.parseNumericAddress("2001:db8:255::255"));

    // Verify Config requests in TunnelModeChildSessionParams and IkeSessionParams
    <T> void verifyConfigRequestTypes(
            Map<Class<? extends T>, Integer> expectedReqCntMap, List<? extends T> resultReqList) {
        Map<Class<? extends T>, Integer> resultReqCntMap = new HashMap<>();

        // Verify that every config request type in resultReqList is expected, and build
        // resultReqCntMap at the same time
        for (T resultReq : resultReqList) {
            boolean isResultReqExpected = false;

            for (Class<? extends T> expectedReqInterface : expectedReqCntMap.keySet()) {
                if (expectedReqInterface.isInstance(resultReq)) {
                    isResultReqExpected = true;

                    resultReqCntMap.put(
                            expectedReqInterface,
                            resultReqCntMap.getOrDefault(expectedReqInterface, 0) + 1);
                }
            }

            if (!isResultReqExpected) {
                fail("Failed due to unexpected config request " + resultReq);
            }
        }

        assertEquals(expectedReqCntMap, resultReqCntMap);

        // TODO: Think of a neat way to validate both counts and values in this method. Probably can
        // build Runnables as validators for count and values.
    }
}
