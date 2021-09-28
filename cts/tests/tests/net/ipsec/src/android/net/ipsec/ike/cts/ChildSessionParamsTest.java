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

import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.LinkAddress;
import android.net.ipsec.ike.ChildSaProposal;
import android.net.ipsec.ike.ChildSessionParams;
import android.net.ipsec.ike.TransportModeChildSessionParams;
import android.net.ipsec.ike.TunnelModeChildSessionParams;
import android.net.ipsec.ike.TunnelModeChildSessionParams.ConfigRequestIpv4Address;
import android.net.ipsec.ike.TunnelModeChildSessionParams.ConfigRequestIpv4DhcpServer;
import android.net.ipsec.ike.TunnelModeChildSessionParams.ConfigRequestIpv4DnsServer;
import android.net.ipsec.ike.TunnelModeChildSessionParams.ConfigRequestIpv4Netmask;
import android.net.ipsec.ike.TunnelModeChildSessionParams.ConfigRequestIpv6Address;
import android.net.ipsec.ike.TunnelModeChildSessionParams.ConfigRequestIpv6DnsServer;
import android.net.ipsec.ike.TunnelModeChildSessionParams.TunnelModeChildConfigRequest;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.Inet4Address;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class ChildSessionParamsTest extends IkeTestBase {
    private static final int HARD_LIFETIME_SECONDS = (int) TimeUnit.HOURS.toSeconds(3L);
    private static final int SOFT_LIFETIME_SECONDS = (int) TimeUnit.HOURS.toSeconds(1L);

    // Random proposal. Content doesn't matter
    private final ChildSaProposal mSaProposal =
            SaProposalTest.buildChildSaProposalWithCombinedModeCipher();

    private void verifyTunnelModeChildParamsWithDefaultValues(ChildSessionParams childParams) {
        assertTrue(childParams instanceof TunnelModeChildSessionParams);
        verifyChildParamsWithDefaultValues(childParams);
    }

    private void verifyTunnelModeChildParamsWithCustomizedValues(ChildSessionParams childParams) {
        assertTrue(childParams instanceof TunnelModeChildSessionParams);
        verifyChildParamsWithCustomizedValues(childParams);
    }

    private void verifyTransportModeChildParamsWithDefaultValues(ChildSessionParams childParams) {
        assertTrue(childParams instanceof TransportModeChildSessionParams);
        verifyChildParamsWithDefaultValues(childParams);
    }

    private void verifyTransportModeChildParamsWithCustomizedValues(
            ChildSessionParams childParams) {
        assertTrue(childParams instanceof TransportModeChildSessionParams);
        verifyChildParamsWithCustomizedValues(childParams);
    }

    private void verifyChildParamsWithDefaultValues(ChildSessionParams childParams) {
        assertEquals(Arrays.asList(mSaProposal), childParams.getSaProposals());

        // Do not do assertEquals to the default values to be avoid being a change-detector test
        assertTrue(childParams.getHardLifetimeSeconds() > childParams.getSoftLifetimeSeconds());
        assertTrue(childParams.getSoftLifetimeSeconds() > 0);

        assertEquals(
                Arrays.asList(DEFAULT_V4_TS, DEFAULT_V6_TS),
                childParams.getInboundTrafficSelectors());
        assertEquals(
                Arrays.asList(DEFAULT_V4_TS, DEFAULT_V6_TS),
                childParams.getOutboundTrafficSelectors());
    }

    private void verifyChildParamsWithCustomizedValues(ChildSessionParams childParams) {
        assertEquals(Arrays.asList(mSaProposal), childParams.getSaProposals());

        assertEquals(HARD_LIFETIME_SECONDS, childParams.getHardLifetimeSeconds());
        assertEquals(SOFT_LIFETIME_SECONDS, childParams.getSoftLifetimeSeconds());

        assertEquals(
                Arrays.asList(INBOUND_V4_TS, INBOUND_V6_TS),
                childParams.getInboundTrafficSelectors());
        assertEquals(
                Arrays.asList(OUTBOUND_V4_TS, OUTBOUND_V6_TS),
                childParams.getOutboundTrafficSelectors());
    }

    @Test
    public void testBuildTransportModeParamsWithDefaultValues() {
        TransportModeChildSessionParams childParams =
                new TransportModeChildSessionParams.Builder().addSaProposal(mSaProposal).build();

        verifyTransportModeChildParamsWithDefaultValues(childParams);
    }

    @Test
    public void testBuildTunnelModeParamsWithDefaultValues() {
        TunnelModeChildSessionParams childParams =
                new TunnelModeChildSessionParams.Builder().addSaProposal(mSaProposal).build();

        verifyTunnelModeChildParamsWithDefaultValues(childParams);
        assertTrue(childParams.getConfigurationRequests().isEmpty());
    }

    @Test
    public void testBuildTransportModeParamsWithCustomizedValues() {
        TransportModeChildSessionParams childParams =
                new TransportModeChildSessionParams.Builder()
                        .addSaProposal(mSaProposal)
                        .setLifetimeSeconds(HARD_LIFETIME_SECONDS, SOFT_LIFETIME_SECONDS)
                        .addInboundTrafficSelectors(INBOUND_V4_TS)
                        .addInboundTrafficSelectors(INBOUND_V6_TS)
                        .addOutboundTrafficSelectors(OUTBOUND_V4_TS)
                        .addOutboundTrafficSelectors(OUTBOUND_V6_TS)
                        .build();

        verifyTransportModeChildParamsWithCustomizedValues(childParams);
    }

    @Test
    public void testBuildTunnelModeParamsWithCustomizedValues() {
        TunnelModeChildSessionParams childParams =
                new TunnelModeChildSessionParams.Builder()
                        .addSaProposal(mSaProposal)
                        .setLifetimeSeconds(HARD_LIFETIME_SECONDS, SOFT_LIFETIME_SECONDS)
                        .addInboundTrafficSelectors(INBOUND_V4_TS)
                        .addInboundTrafficSelectors(INBOUND_V6_TS)
                        .addOutboundTrafficSelectors(OUTBOUND_V4_TS)
                        .addOutboundTrafficSelectors(OUTBOUND_V6_TS)
                        .build();

        verifyTunnelModeChildParamsWithCustomizedValues(childParams);
    }

    @Test
    public void testBuildChildSessionParamsWithConfigReq() {
        TunnelModeChildSessionParams childParams =
                new TunnelModeChildSessionParams.Builder()
                        .addSaProposal(mSaProposal)
                        .addInternalAddressRequest(AF_INET)
                        .addInternalAddressRequest(AF_INET6)
                        .addInternalAddressRequest(AF_INET6)
                        .addInternalAddressRequest(IPV4_ADDRESS_REMOTE)
                        .addInternalAddressRequest(IPV6_ADDRESS_REMOTE, IP6_PREFIX_LEN)
                        .addInternalDnsServerRequest(AF_INET)
                        .addInternalDnsServerRequest(AF_INET6)
                        .addInternalDhcpServerRequest(AF_INET)
                        .addInternalDhcpServerRequest(AF_INET)
                        .build();

        verifyTunnelModeChildParamsWithDefaultValues(childParams);

        // Verify config request types and number of requests for each type
        Map<Class<? extends TunnelModeChildConfigRequest>, Integer> expectedAttributeCounts =
                new HashMap<>();
        expectedAttributeCounts.put(ConfigRequestIpv4Address.class, 2);
        expectedAttributeCounts.put(ConfigRequestIpv6Address.class, 3);
        expectedAttributeCounts.put(ConfigRequestIpv4Netmask.class, 1);
        expectedAttributeCounts.put(ConfigRequestIpv4DnsServer.class, 1);
        expectedAttributeCounts.put(ConfigRequestIpv6DnsServer.class, 1);
        expectedAttributeCounts.put(ConfigRequestIpv4DhcpServer.class, 2);
        verifyConfigRequestTypes(expectedAttributeCounts, childParams.getConfigurationRequests());

        // Verify specific IPv4 address request
        Set<Inet4Address> expectedV4Addresses = new HashSet<>();
        expectedV4Addresses.add(IPV4_ADDRESS_REMOTE);
        verifySpecificV4AddrConfigReq(expectedV4Addresses, childParams);

        // Verify specific IPv6 address request
        Set<LinkAddress> expectedV6Addresses = new HashSet<>();
        expectedV6Addresses.add(new LinkAddress(IPV6_ADDRESS_REMOTE, IP6_PREFIX_LEN));
        verifySpecificV6AddrConfigReq(expectedV6Addresses, childParams);
    }

    protected void verifySpecificV4AddrConfigReq(
            Set<Inet4Address> expectedAddresses, TunnelModeChildSessionParams childParams) {
        for (TunnelModeChildConfigRequest req : childParams.getConfigurationRequests()) {
            if (req instanceof ConfigRequestIpv4Address
                    && ((ConfigRequestIpv4Address) req).getAddress() != null) {
                Inet4Address address = ((ConfigRequestIpv4Address) req).getAddress();

                // Fail if expectedAddresses does not contain this address
                assertTrue(expectedAddresses.remove(address));
            }
        }

        // Fail if any expected address is not found in result
        assertTrue(expectedAddresses.isEmpty());
    }

    protected void verifySpecificV6AddrConfigReq(
            Set<LinkAddress> expectedAddresses, TunnelModeChildSessionParams childParams) {
        for (TunnelModeChildConfigRequest req : childParams.getConfigurationRequests()) {
            if (req instanceof ConfigRequestIpv6Address
                    && ((ConfigRequestIpv6Address) req).getAddress() != null) {
                ConfigRequestIpv6Address ipv6AddrReq = (ConfigRequestIpv6Address) req;

                // Fail if expectedAddresses does not contain this address
                LinkAddress address =
                        new LinkAddress(ipv6AddrReq.getAddress(), ipv6AddrReq.getPrefixLength());
                assertTrue(expectedAddresses.remove(address));
            }
        }

        // Fail if any expected address is not found in result
        assertTrue(expectedAddresses.isEmpty());
    }
}
