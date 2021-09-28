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

package android.security.cts;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;

import java.lang.Integer;
import java.lang.String;
import java.util.Arrays;
import java.util.List;
import java.util.ArrayList;

/**
 * Host-side tests for values in /proc/net.
 *
 * These tests analyze /proc/net to verify that certain networking properties are correct.
 */
public class ProcNetTest extends DeviceTestCase implements IBuildReceiver, IDeviceTest {
    private static final String SPI_TIMEOUT_SYSCTL = "/proc/sys/net/core/xfrm_acq_expires";
    private static final int MIN_ACQ_EXPIRES = 3600;
    // Global sysctls. Must be present and set to 1.
    private static final String[] GLOBAL_SYSCTLS = {
        "/proc/sys/net/ipv4/fwmark_reflect",
        "/proc/sys/net/ipv6/fwmark_reflect",
        "/proc/sys/net/ipv4/tcp_fwmark_accept",
    };

    // Per-interface IPv6 autoconf sysctls.
    private static final String IPV6_SYSCTL_DIR = "/proc/sys/net/ipv6/conf";
    private static final String AUTOCONF_SYSCTL = "accept_ra_rt_table";

    // Expected values for MIN|MAX_PLEN.
    private static final String ACCEPT_RA_RT_INFO_MIN_PLEN_STRING = "accept_ra_rt_info_min_plen";
    private static final int ACCEPT_RA_RT_INFO_MIN_PLEN_VALUE = 48;
    private static final String ACCEPT_RA_RT_INFO_MAX_PLEN_STRING = "accept_ra_rt_info_max_plen";
    private static final int ACCEPT_RA_RT_INFO_MAX_PLEN_VALUE = 64;
    // Expected values for RFC 7559 router soliciations.
    // Maximum number of router solicitations to send. -1 means no limit.
    private static final int IPV6_WIFI_ROUTER_SOLICITATIONS = -1;
    private ITestDevice mDevice;
    private IBuildInfo mBuild;
    private String[] mSysctlDirs;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuild(IBuildInfo build) {
        mBuild = build;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setDevice(ITestDevice device) {
        super.setDevice(device);
        mDevice = device;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mSysctlDirs = getSysctlDirs();
    }

    private String[] getSysctlDirs() throws Exception {
        String interfaceDirs[] = mDevice.executeAdbCommand("shell", "ls", "-1",
                IPV6_SYSCTL_DIR).split("\n");
        List<String> interfaceDirsList = new ArrayList<String>(Arrays.asList(interfaceDirs));
        interfaceDirsList.remove("all");
        interfaceDirsList.remove("lo");
        return interfaceDirsList.toArray(new String[interfaceDirsList.size()]);
    }


    protected void assertLess(String sysctl, int a, int b) {
        assertTrue("value of " + sysctl + ": expected < " + b + " but was: " + a, a < b);
    }

    protected void assertAtLeast(String sysctl, int a, int b) {
        assertTrue("value of " + sysctl + ": expected >= " + b + " but was: " + a, a >= b);
    }

    public int readIntFromPath(String path) throws Exception {
        String mode = mDevice.executeAdbCommand("shell", "stat", "-c", "%a", path).trim();
        String user = mDevice.executeAdbCommand("shell", "stat", "-c", "%u", path).trim();
        String group = mDevice.executeAdbCommand("shell", "stat", "-c", "%g", path).trim();
        assertEquals(mode, "644");
        assertEquals(user, "0");
        assertEquals(group, "0");
        return Integer.parseInt(mDevice.executeAdbCommand("shell", "cat", path).trim());
    }

    /**
     * Checks that SPI default timeouts are overridden, and set to a reasonable length of time
     */
    public void testMinAcqExpires() throws Exception {
        int value = readIntFromPath(SPI_TIMEOUT_SYSCTL);
        assertAtLeast(SPI_TIMEOUT_SYSCTL, value, MIN_ACQ_EXPIRES);
    }

    /**
     * Checks that the sysctls for multinetwork kernel features are present and
     * enabled.
     */
    public void testProcSysctls() throws Exception {
        for (String sysctl : GLOBAL_SYSCTLS) {
            int value = readIntFromPath(sysctl);
            assertEquals(sysctl, 1, value);
        }

        for (String interfaceDir : mSysctlDirs) {
            String path = IPV6_SYSCTL_DIR + "/" + interfaceDir + "/" + AUTOCONF_SYSCTL;
            int value = readIntFromPath(path);
            assertLess(path, value, 0);
        }
    }

    /**
     * Verify that accept_ra_rt_info_{min,max}_plen exists and is set to the expected value
     */
    public void testAcceptRaRtInfoMinMaxPlen() throws Exception {
        for (String interfaceDir : mSysctlDirs) {
            String path = IPV6_SYSCTL_DIR + "/" + interfaceDir + "/" + "accept_ra_rt_info_min_plen";
            int value = readIntFromPath(path);
            assertEquals(path, value, ACCEPT_RA_RT_INFO_MIN_PLEN_VALUE);
            path = IPV6_SYSCTL_DIR + "/" + interfaceDir + "/" + "accept_ra_rt_info_max_plen";
            value = readIntFromPath(path);
            assertEquals(path, value, ACCEPT_RA_RT_INFO_MAX_PLEN_VALUE);
        }
    }

    /**
     * Verify that router_solicitations exists and is set to the expected value
     * and verify that router_solicitation_max_interval exists and is in an acceptable interval.
     */
    public void testRouterSolicitations() throws Exception {
        for (String interfaceDir : mSysctlDirs) {
            String path = IPV6_SYSCTL_DIR + "/" + interfaceDir + "/" + "router_solicitations";
            int value = readIntFromPath(path);
            assertEquals(IPV6_WIFI_ROUTER_SOLICITATIONS, value);
            path = IPV6_SYSCTL_DIR + "/" + interfaceDir + "/" + "router_solicitation_max_interval";
            int interval = readIntFromPath(path);
            final int lowerBoundSec = 15 * 60;
            final int upperBoundSec = 60 * 60;
            assertTrue(lowerBoundSec <= interval);
            assertTrue(interval <= upperBoundSec);
        }
    }
}
