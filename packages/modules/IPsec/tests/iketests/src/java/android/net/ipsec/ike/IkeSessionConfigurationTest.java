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

package android.net.ipsec.ike;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;

import android.net.InetAddresses;

import com.android.internal.net.ipsec.ike.message.IkeConfigPayload;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttribute;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeAppVersion;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv4Pcscf;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv6Pcscf;

import org.junit.Test;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

public final class IkeSessionConfigurationTest {
    private static final String REMOTE_APP_VERSION = "Test IKE Server 1.0";

    private static final Inet4Address PCSCF_IPV4_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.100"));
    private static final Inet6Address PCSCF_IPV6_ADDRESS =
            (Inet6Address) (InetAddresses.parseNumericAddress("2001:db8::1"));

    private static final IkeSessionConnectionInfo IKE_CONNECT_INFO =
            mock(IkeSessionConnectionInfo.class);

    private static final List<byte[]> REMOTE_VENDOR_IDS;
    private static final List<Integer> ENABLED_EXTENSIONS;

    static {
        REMOTE_VENDOR_IDS = new ArrayList<>();
        REMOTE_VENDOR_IDS.add("vendorIdA".getBytes());
        REMOTE_VENDOR_IDS.add("vendorIdB".getBytes());

        ENABLED_EXTENSIONS = new ArrayList<>();
        ENABLED_EXTENSIONS.add(IkeSessionConfiguration.EXTENSION_TYPE_FRAGMENTATION);
    }

    private void verifyBuildCommon(IkeSessionConfiguration config) {
        // Verify Vendor IDs
        List<byte[]> remoteVendorIds = config.getRemoteVendorIds();
        assertEquals(REMOTE_VENDOR_IDS.size(), remoteVendorIds.size());
        for (int i = 0; i < REMOTE_VENDOR_IDS.size(); i++) {
            assertArrayEquals(REMOTE_VENDOR_IDS.get(i), remoteVendorIds.get(i));
        }

        // Verify enabled extensions
        assertTrue(
                config.isIkeExtensionEnabled(IkeSessionConfiguration.EXTENSION_TYPE_FRAGMENTATION));
        assertFalse(config.isIkeExtensionEnabled(IkeSessionConfiguration.EXTENSION_TYPE_MOBIKE));

        // Verify IkeSessionConnectionInfo
        assertEquals(IKE_CONNECT_INFO, config.getIkeSessionConnectionInfo());
    }

    @Test
    public void testBuildWithoutConfigPayload() {
        IkeSessionConfiguration config =
                new IkeSessionConfiguration(
                        IKE_CONNECT_INFO,
                        null /*configPayload*/,
                        REMOTE_VENDOR_IDS,
                        ENABLED_EXTENSIONS);
        verifyBuildCommon(config);
    }

    @Test
    public void testBuildWithConfigPayload() {
        List<ConfigAttribute> attributeList = new LinkedList<>();
        attributeList.add(new ConfigAttributeIpv4Pcscf(PCSCF_IPV4_ADDRESS));
        attributeList.add(new ConfigAttributeIpv6Pcscf(PCSCF_IPV6_ADDRESS));
        attributeList.add(new ConfigAttributeAppVersion(REMOTE_APP_VERSION));

        IkeConfigPayload configPayload = new IkeConfigPayload(true /*isReply*/, attributeList);

        IkeSessionConfiguration config =
                new IkeSessionConfiguration(
                        IKE_CONNECT_INFO, configPayload, REMOTE_VENDOR_IDS, ENABLED_EXTENSIONS);

        verifyBuildCommon(config);
        assertEquals(
                Arrays.asList(PCSCF_IPV4_ADDRESS, PCSCF_IPV6_ADDRESS), config.getPcscfServers());
        assertEquals(REMOTE_APP_VERSION, config.getRemoteApplicationVersion());
    }

    @Test
    public void testBuildWithNullValueConnectionInfo() {
        try {
            new IkeSessionConfiguration(
                    null /*ikeConnInfo*/,
                    null /*configPayload*/,
                    REMOTE_VENDOR_IDS,
                    ENABLED_EXTENSIONS);
            fail("Expected to fail due to null value ikeConnInfo");
        } catch (NullPointerException expected) {
        }
    }
}
