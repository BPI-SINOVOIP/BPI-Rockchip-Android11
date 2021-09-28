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

package android.net.cts;

import static com.android.testutils.ParcelUtilsKt.assertParcelSane;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.net.IpConfiguration;
import android.net.LinkAddress;
import android.net.ProxyInfo;
import android.net.StaticIpConfiguration;

import androidx.test.runner.AndroidJUnit4;

import libcore.net.InetAddressUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.util.ArrayList;

@RunWith(AndroidJUnit4.class)
public final class IpConfigurationTest {
    private static final LinkAddress LINKADDR = new LinkAddress("192.0.2.2/25");
    private static final InetAddress GATEWAY = InetAddressUtils.parseNumericAddress("192.0.2.1");
    private static final InetAddress DNS1 = InetAddressUtils.parseNumericAddress("8.8.8.8");
    private static final InetAddress DNS2 = InetAddressUtils.parseNumericAddress("8.8.4.4");
    private static final String DOMAINS = "example.com";

    private static final ArrayList<InetAddress> dnsServers = new ArrayList<>();

    private StaticIpConfiguration mStaticIpConfig;
    private ProxyInfo mProxy;

    @Before
    public void setUp() {
        dnsServers.add(DNS1);
        dnsServers.add(DNS2);
        mStaticIpConfig = new StaticIpConfiguration.Builder()
                .setIpAddress(LINKADDR)
                .setGateway(GATEWAY)
                .setDnsServers(dnsServers)
                .setDomains(DOMAINS)
                .build();

        mProxy = ProxyInfo.buildDirectProxy("test", 8888);
    }

    @Test
    public void testConstructor() {
        IpConfiguration ipConfig = new IpConfiguration();
        checkEmpty(ipConfig);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration());
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));

        ipConfig.setStaticIpConfiguration(mStaticIpConfig);
        ipConfig.setHttpProxy(mProxy);

        ipConfig.setIpAssignment(IpConfiguration.IpAssignment.STATIC);
        ipConfig.setProxySettings(IpConfiguration.ProxySettings.PAC);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));

        ipConfig.setIpAssignment(IpConfiguration.IpAssignment.STATIC);
        ipConfig.setProxySettings(IpConfiguration.ProxySettings.STATIC);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));

        ipConfig.setIpAssignment(IpConfiguration.IpAssignment.DHCP);
        ipConfig.setProxySettings(IpConfiguration.ProxySettings.PAC);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));

        ipConfig.setIpAssignment(IpConfiguration.IpAssignment.DHCP);
        ipConfig.setProxySettings(IpConfiguration.ProxySettings.PAC);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));

        ipConfig.setIpAssignment(IpConfiguration.IpAssignment.DHCP);
        ipConfig.setProxySettings(IpConfiguration.ProxySettings.STATIC);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));

        ipConfig.setIpAssignment(IpConfiguration.IpAssignment.DHCP);
        ipConfig.setProxySettings(IpConfiguration.ProxySettings.NONE);
        assertIpConfigurationEqual(ipConfig, new IpConfiguration(ipConfig));
    }

    private void checkEmpty(IpConfiguration config) {
        assertEquals(IpConfiguration.IpAssignment.UNASSIGNED,
                config.getIpAssignment().UNASSIGNED);
        assertEquals(IpConfiguration.ProxySettings.UNASSIGNED,
                config.getProxySettings().UNASSIGNED);
        assertNull(config.getStaticIpConfiguration());
        assertNull(config.getHttpProxy());
    }

    private void assertIpConfigurationEqual(IpConfiguration source, IpConfiguration target) {
        assertEquals(source.getIpAssignment(), target.getIpAssignment());
        assertEquals(source.getProxySettings(), target.getProxySettings());
        assertEquals(source.getHttpProxy(), target.getHttpProxy());
        assertEquals(source.getStaticIpConfiguration(), target.getStaticIpConfiguration());
    }

    @Test
    public void testParcel() {
        final IpConfiguration config = new IpConfiguration();
        assertParcelSane(config, 4);
    }
}
