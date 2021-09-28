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

package android.net.wifi.cts;

import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_METERED;
import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_NONE;
import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_NOT_METERED;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.app.UiAutomation;
import android.content.Context;
import android.net.IpConfiguration;
import android.net.LinkAddress;
import android.net.ProxyInfo;
import android.net.StaticIpConfiguration;
import android.net.Uri;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.SystemUtil;
import com.android.compatibility.common.util.ThrowingRunnable;


import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Tests for wifi backup/restore functionality.
 */
@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
@SmallTest
@RunWith(AndroidJUnit4.class)
public class WifiBackupRestoreTest extends WifiJUnit4TestBase {
    private static final String TAG = "WifiBackupRestoreTest";
    private static final String LEGACY_SUPP_CONF_FILE =
            "assets/BackupLegacyFormatSupplicantConf.txt";
    private static final String LEGACY_IP_CONF_FILE =
            "assets/BackupLegacyFormatIpConf.txt";
    private static final String V1_0_FILE = "assets/BackupV1.0Format.xml";
    private static final String V1_1_FILE = "assets/BackupV1.1Format.xml";
    private static final String V1_2_FILE = "assets/BackupV1.2Format.xml";

    public static final String EXPECTED_LEGACY_STATIC_IP_LINK_ADDRESS = "192.168.48.2";
    public static final int EXPECTED_LEGACY_STATIC_IP_LINK_PREFIX_LENGTH = 8;
    public static final String EXPECTED_LEGACY_STATIC_IP_GATEWAY_ADDRESS = "192.168.48.1";
    public static final String[] EXPECTED_LEGACY_STATIC_IP_DNS_SERVER_ADDRESSES =
            new String[]{"192.168.48.1", "192.168.48.10"};
    public static final String EXPECTED_LEGACY_STATIC_PROXY_HOST = "192.168.48.1";
    public static final int EXPECTED_LEGACY_STATIC_PROXY_PORT = 8000;
    public static final String EXPECTED_LEGACY_STATIC_PROXY_EXCLUSION_LIST = "";
    public static final String EXPECTED_LEGACY_PAC_PROXY_LOCATION = "http://";

    private Context mContext;
    private WifiManager mWifiManager;
    private UiDevice mUiDevice;
    private boolean mWasVerboseLoggingEnabled;

    private static final int DURATION = 10_000;
    private static final int DURATION_SCREEN_TOGGLE = 2000;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        // skip the test if WiFi is not supported
        assumeTrue(WifiFeature.isWifiSupported(mContext));

        mWifiManager = mContext.getSystemService(WifiManager.class);
        assertThat(mWifiManager).isNotNull();

        // turn on verbose logging for tests
        mWasVerboseLoggingEnabled = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.isVerboseLoggingEnabled());
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(true));
        // Disable scan throttling for tests.
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setScanThrottleEnabled(false));

        if (!mWifiManager.isWifiEnabled()) setWifiEnabled(true);
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        turnScreenOn();
        PollingCheck.check("Wifi not enabled", DURATION, () -> mWifiManager.isWifiEnabled());

        List<WifiConfiguration> savedNetworks = ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.getPrivilegedConfiguredNetworks());
        assertWithMessage("Need at least one saved network").that(savedNetworks).isNotEmpty();
    }

    @After
    public void tearDown() throws Exception {
        if (!WifiFeature.isWifiSupported(mContext)) return;
        if (!mWifiManager.isWifiEnabled()) setWifiEnabled(true);
        turnScreenOff();
        ShellIdentityUtils.invokeWithShellPermissions(
                () -> mWifiManager.setVerboseLoggingEnabled(mWasVerboseLoggingEnabled));
    }

    private void setWifiEnabled(boolean enable) throws Exception {
        // now trigger the change using shell commands.
        SystemUtil.runShellCommand("svc wifi " + (enable ? "enable" : "disable"));
    }

    private void turnScreenOn() throws Exception {
        mUiDevice.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        mUiDevice.executeShellCommand("wm dismiss-keyguard");
        // Since the screen on/off intent is ordered, they will not be sent right now.
        Thread.sleep(DURATION_SCREEN_TOGGLE);
    }

    private void turnScreenOff() throws Exception {
        mUiDevice.executeShellCommand("input keyevent KEYCODE_SLEEP");
        // Since the screen on/off intent is ordered, they will not be sent right now.
        Thread.sleep(DURATION_SCREEN_TOGGLE);
    }

    private void flipMeteredOverride(WifiConfiguration network) {
        if (network.meteredOverride == METERED_OVERRIDE_NONE) {
            network.meteredOverride = METERED_OVERRIDE_METERED;
        } else if (network.meteredOverride == METERED_OVERRIDE_METERED) {
            network.meteredOverride = METERED_OVERRIDE_NOT_METERED;
        } else if (network.meteredOverride == METERED_OVERRIDE_NOT_METERED) {
            network.meteredOverride = METERED_OVERRIDE_NONE;
        }
    }

    /**
     * Tests for {@link WifiManager#retrieveBackupData()} &
     * {@link WifiManager#restoreBackupData(byte[])}
     * Note: If the network was not created by an app with OVERRIDE_WIFI_CONFIG permission (held
     * by AOSP settings app for example), then the backup data will not contain that network. If
     * the device does not contain any such pre-existing saved network, then this test will be
     * a no-op, will only ensure that the device does not crash when invoking the API's.
     */
    @Test
    public void testCanRestoreBackupData() {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        WifiConfiguration origNetwork = null;
        try {
            uiAutomation.adoptShellPermissionIdentity();

            // Pick any saved network to modify;
            origNetwork = mWifiManager.getConfiguredNetworks().stream()
                    .filter(n -> mContext.checkPermission(
                            android.Manifest.permission.OVERRIDE_WIFI_CONFIG, -1, n.creatorUid)
                            == PERMISSION_GRANTED)
                    .findAny()
                    .orElse(null);
            if (origNetwork == null) {
                Log.e(TAG, "Need a network created by an app holding OVERRIDE_WIFI_CONFIG "
                        + "permission to fully evaluate the functionality");
            }

            // Retrieve backup data.
            byte[] backupData = mWifiManager.retrieveBackupData();

            if (origNetwork != null) {
                // Modify the metered bit.
                final String origNetworkSsid = origNetwork.SSID;
                WifiConfiguration modNetwork = new WifiConfiguration(origNetwork);
                flipMeteredOverride(modNetwork);
                int networkId = mWifiManager.updateNetwork(modNetwork);
                assertThat(networkId).isEqualTo(origNetwork.networkId);
                assertThat(mWifiManager.getConfiguredNetworks()
                        .stream()
                        .filter(n -> n.SSID.equals(origNetworkSsid))
                        .findAny()
                        .get().meteredOverride)
                        .isNotEqualTo(origNetwork.meteredOverride);
            }

            // Restore the original backup data & ensure that the metered bit is back to orig.
            mWifiManager.restoreBackupData(backupData);

            if (origNetwork != null) {
                final String origNetworkSsid = origNetwork.SSID;
                assertThat(mWifiManager.getConfiguredNetworks()
                        .stream()
                        .filter(n -> n.SSID.equals(origNetworkSsid))
                        .findAny()
                        .get().meteredOverride)
                        .isEqualTo(origNetwork.meteredOverride);
            }
        } finally {
            // Restore the orig network
            if (origNetwork != null) {
                mWifiManager.updateNetwork(origNetwork);
            }
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Tests for {@link WifiManager#retrieveSoftApBackupData()} &
     * {@link WifiManager#restoreSoftApBackupData(byte[])}
     */
    @Test
    public void testCanRestoreSoftApBackupData() {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        SoftApConfiguration origSoftApConfig = null;
        try {
            uiAutomation.adoptShellPermissionIdentity();

            // Retrieve original soft ap config.
            origSoftApConfig = mWifiManager.getSoftApConfiguration();

            // Retrieve backup data.
            byte[] backupData = mWifiManager.retrieveSoftApBackupData();

            // Modify softap config and set it.
            SoftApConfiguration modSoftApConfig = new SoftApConfiguration.Builder(origSoftApConfig)
                    .setSsid(origSoftApConfig.getSsid() + "b")
                    .build();
            mWifiManager.setSoftApConfiguration(modSoftApConfig);
            // Ensure that it does not match the orig softap config.
            assertThat(mWifiManager.getSoftApConfiguration()).isNotEqualTo(origSoftApConfig);

            // Restore the original backup data & ensure that the orig softap config is restored.
            mWifiManager.restoreSoftApBackupData(backupData);
            assertThat(mWifiManager.getSoftApConfiguration()).isEqualTo(origSoftApConfig);
        } finally {
            if (origSoftApConfig != null) {
                mWifiManager.setSoftApConfiguration(origSoftApConfig);
            }
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Read the content of the given resource file into a String.
     *
     * @param filename String name of the file
     * @return Byte array of the contents of the file.
     * @throws IOException
     */
    private byte[] loadResourceFile(String filename) throws IOException {
        InputStream in = getClass().getClassLoader().getResourceAsStream(filename);
        DataInputStream dis = new DataInputStream(in);
        byte[] data = new byte[dis.available()];
        dis.readFully(data);
        return data;
    }

    private WifiConfiguration createExpectedLegacyWepWifiConfiguration() {
        WifiConfiguration configuration = new WifiConfiguration();
        configuration.SSID = "\"TestSsid1\"";
        configuration.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        configuration.wepKeys = new String[4];
        configuration.wepKeys[0] = "\"WepAscii1\"";
        configuration.wepKeys[1] = "\"WepAscii2\"";
        configuration.wepKeys[2] = "45342312ab";
        configuration.wepKeys[3] = "45342312ab45342312ab34ac12";
        configuration.wepTxKeyIndex = 1;
        return configuration;
    }

    private WifiConfiguration createExpectedLegacyPskWifiConfiguration() {
        WifiConfiguration configuration = new WifiConfiguration();
        configuration.SSID = "\"TestSsid2\"";
        configuration.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
        configuration.preSharedKey = "\"TestPsk123\"";
        return configuration;
    }

    private WifiConfiguration createExpectedLegacyOpenWifiConfiguration() {
        WifiConfiguration configuration = new WifiConfiguration();
        configuration.SSID = "\"TestSsid3\"";
        configuration.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        return configuration;
    }

    private IpConfiguration createExpectedLegacyDHCPIpConfigurationWithPacProxy() throws Exception {
        IpConfiguration ipConfiguration = new IpConfiguration();
        ipConfiguration.setIpAssignment(IpConfiguration.IpAssignment.DHCP);
        ipConfiguration.setProxySettings(IpConfiguration.ProxySettings.PAC);
        ipConfiguration.setHttpProxy(ProxyInfo.buildPacProxy(
                Uri.parse(EXPECTED_LEGACY_PAC_PROXY_LOCATION)));
        return ipConfiguration;
    }

    private StaticIpConfiguration createExpectedLegacyStaticIpconfiguration() throws Exception {
        return new StaticIpConfiguration.Builder()
                .setIpAddress(
                        new LinkAddress(
                                InetAddress.getByName(EXPECTED_LEGACY_STATIC_IP_LINK_ADDRESS),
                                EXPECTED_LEGACY_STATIC_IP_LINK_PREFIX_LENGTH))
                .setGateway(InetAddress.getByName(EXPECTED_LEGACY_STATIC_IP_GATEWAY_ADDRESS))
                .setDnsServers(Arrays.asList(EXPECTED_LEGACY_STATIC_IP_DNS_SERVER_ADDRESSES)
                        .stream()
                        .map(s -> {
                            try {
                                return InetAddress.getByName(s);
                            } catch (UnknownHostException e) {
                                return null;
                            }
                        })
                        .collect(Collectors.toList()))
                .build();
    }

    private IpConfiguration createExpectedLegacyStaticIpConfigurationWithPacProxy()
            throws Exception {
        IpConfiguration ipConfiguration = new IpConfiguration();
        ipConfiguration.setIpAssignment(IpConfiguration.IpAssignment.STATIC);
        ipConfiguration.setStaticIpConfiguration(createExpectedLegacyStaticIpconfiguration());
        ipConfiguration.setProxySettings(IpConfiguration.ProxySettings.PAC);
        ipConfiguration.setHttpProxy(ProxyInfo.buildPacProxy(
                Uri.parse(EXPECTED_LEGACY_PAC_PROXY_LOCATION)));
        return ipConfiguration;
    }

    private IpConfiguration createExpectedLegacyStaticIpConfigurationWithStaticProxy()
            throws Exception {
        IpConfiguration ipConfiguration = new IpConfiguration();
        ipConfiguration.setIpAssignment(IpConfiguration.IpAssignment.STATIC);
        ipConfiguration.setStaticIpConfiguration(createExpectedLegacyStaticIpconfiguration());
        ipConfiguration.setProxySettings(IpConfiguration.ProxySettings.STATIC);
        ipConfiguration.setHttpProxy(ProxyInfo.buildDirectProxy(
                EXPECTED_LEGACY_STATIC_PROXY_HOST, EXPECTED_LEGACY_STATIC_PROXY_PORT,
                Arrays.asList(EXPECTED_LEGACY_STATIC_PROXY_EXCLUSION_LIST)));
        return ipConfiguration;
    }

    /**
     * Asserts that the 2 lists of WifiConfigurations are equal. This compares all the elements
     * saved for backup/restore.
     */
    public static void assertConfigurationsEqual(
            List<WifiConfiguration> expected, List<WifiConfiguration> actual) {
        assertThat(actual.size()).isEqualTo(expected.size());
        for (WifiConfiguration expectedConfiguration : expected) {
            String expectedConfigKey = expectedConfiguration.getKey();
            boolean didCompare = false;
            for (WifiConfiguration actualConfiguration : actual) {
                String actualConfigKey = actualConfiguration.getKey();
                if (actualConfigKey.equals(expectedConfigKey)) {
                    assertConfigurationEqual(
                            expectedConfiguration, actualConfiguration);
                    didCompare = true;
                }
            }
            assertWithMessage("Didn't find matching config for key = "
                    + expectedConfigKey).that(didCompare).isTrue();
        }
    }

    /**
     * Asserts that the 2 WifiConfigurations are equal.
     */
    private static void assertConfigurationEqual(
            WifiConfiguration expected, WifiConfiguration actual) {
        assertThat(actual).isNotNull();
        assertThat(expected).isNotNull();
        assertWithMessage("Network: " + actual.toString())
                .that(actual.SSID).isEqualTo(expected.SSID);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.preSharedKey).isEqualTo(expected.preSharedKey);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.wepKeys).isEqualTo(expected.wepKeys);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.wepTxKeyIndex).isEqualTo(expected.wepTxKeyIndex);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.hiddenSSID).isEqualTo(expected.hiddenSSID);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.requirePmf).isEqualTo(expected.requirePmf);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.allowedKeyManagement).isEqualTo(expected.allowedKeyManagement);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.shared).isEqualTo(expected.shared);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.allowAutojoin).isEqualTo(expected.allowAutojoin);
        assertWithMessage("Network: " + actual.toString())
                .that(actual.getIpConfiguration()).isEqualTo(expected.getIpConfiguration());
        assertWithMessage("Network: " + actual.toString())
                .that(actual.meteredOverride).isEqualTo(expected.meteredOverride);
    }

    private void testRestoreFromBackupData(
            List<WifiConfiguration> expectedConfigurations, ThrowingRunnable restoreMethod)
        throws Exception {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        List<WifiConfiguration> restoredSavedNetworks = null;
        try {
            uiAutomation.adoptShellPermissionIdentity();
            Set<String> origSavedSsids = mWifiManager.getConfiguredNetworks().stream()
                    .map(n -> n.SSID)
                    .collect(Collectors.toSet());

            restoreMethod.run();

            restoredSavedNetworks = mWifiManager.getPrivilegedConfiguredNetworks().stream()
                    .filter(n -> !origSavedSsids.contains(n.SSID))
                    .collect(Collectors.toList());
            assertConfigurationsEqual(
                    expectedConfigurations, restoredSavedNetworks);
        } finally {
            // clean up all restored networks.
            if (restoredSavedNetworks != null) {
                for (WifiConfiguration network : restoredSavedNetworks) {
                    mWifiManager.removeNetwork(network.networkId);
                }
            }
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    private List<WifiConfiguration> createExpectedLegacyConfigurations() throws Exception {
        List<WifiConfiguration> expectedConfigurations = new ArrayList<>();
        WifiConfiguration wepNetwork = createExpectedLegacyWepWifiConfiguration();
        wepNetwork.setIpConfiguration(createExpectedLegacyDHCPIpConfigurationWithPacProxy());
        expectedConfigurations.add(wepNetwork);

        WifiConfiguration pskNetwork = createExpectedLegacyPskWifiConfiguration();
        pskNetwork.setIpConfiguration(createExpectedLegacyStaticIpConfigurationWithPacProxy());
        expectedConfigurations.add(pskNetwork);

        WifiConfiguration openNetwork = createExpectedLegacyOpenWifiConfiguration();
        openNetwork.setIpConfiguration(
                createExpectedLegacyStaticIpConfigurationWithStaticProxy());
        expectedConfigurations.add(openNetwork);
        return expectedConfigurations;
    }

    /**
     * Verify that 3 network configuration is deserialized correctly from AOSP
     * legacy supplicant/ipconf backup data format.
     */
    @Test
    public void testRestoreFromLegacyBackupFormat() throws Exception {
        testRestoreFromBackupData(createExpectedLegacyConfigurations(),
                () -> mWifiManager.restoreSupplicantBackupData(
                        loadResourceFile(LEGACY_SUPP_CONF_FILE),
                        loadResourceFile(LEGACY_IP_CONF_FILE)));

    }

    private List<WifiConfiguration> createExpectedV1_0Configurations() throws Exception {
        List<WifiConfiguration> expectedConfigurations = new ArrayList<>();
        WifiConfiguration wepNetwork = createExpectedLegacyWepWifiConfiguration();
        wepNetwork.setIpConfiguration(createExpectedLegacyDHCPIpConfigurationWithPacProxy());
        expectedConfigurations.add(wepNetwork);

        WifiConfiguration pskNetwork = createExpectedLegacyPskWifiConfiguration();
        pskNetwork.setIpConfiguration(createExpectedLegacyStaticIpConfigurationWithPacProxy());
        expectedConfigurations.add(pskNetwork);

        WifiConfiguration openNetwork = createExpectedLegacyOpenWifiConfiguration();
        openNetwork.setIpConfiguration(
                createExpectedLegacyStaticIpConfigurationWithStaticProxy());
        expectedConfigurations.add(openNetwork);
        return expectedConfigurations;
    }

    /**
     * Verify that 3 network configuration is deserialized correctly from AOSP 1.0 format.
     */
    @Test
    public void testRestoreFromV1_0BackupFormat() throws Exception {
        testRestoreFromBackupData(createExpectedV1_0Configurations(),
                () -> mWifiManager.restoreBackupData(loadResourceFile(V1_0_FILE)));
    }

    private List<WifiConfiguration> createExpectedV1_1Configurations() throws Exception {
        List<WifiConfiguration> expectedConfigurations = new ArrayList<>();
        WifiConfiguration wepNetwork = createExpectedLegacyWepWifiConfiguration();
        wepNetwork.setIpConfiguration(createExpectedLegacyDHCPIpConfigurationWithPacProxy());
        wepNetwork.meteredOverride = METERED_OVERRIDE_METERED;
        expectedConfigurations.add(wepNetwork);

        WifiConfiguration pskNetwork = createExpectedLegacyPskWifiConfiguration();
        pskNetwork.setIpConfiguration(createExpectedLegacyStaticIpConfigurationWithPacProxy());
        pskNetwork.meteredOverride = METERED_OVERRIDE_NONE;
        expectedConfigurations.add(pskNetwork);

        WifiConfiguration openNetwork = createExpectedLegacyOpenWifiConfiguration();
        openNetwork.setIpConfiguration(
                createExpectedLegacyStaticIpConfigurationWithStaticProxy());
        openNetwork.meteredOverride = METERED_OVERRIDE_NOT_METERED;
        expectedConfigurations.add(openNetwork);
        return expectedConfigurations;
    }

    /**
     * Verify that 3 network configuration is deserialized correctly from AOSP 1.1 format.
     */
    @Test
    public void testRestoreFromV1_1BackupFormat() throws Exception {
        testRestoreFromBackupData(createExpectedV1_1Configurations(),
                () -> mWifiManager.restoreBackupData(loadResourceFile(V1_1_FILE)));
    }

    private List<WifiConfiguration> createExpectedV1_2Configurations() throws Exception {
        List<WifiConfiguration> expectedConfigurations = new ArrayList<>();
        WifiConfiguration wepNetwork = createExpectedLegacyWepWifiConfiguration();
        wepNetwork.setIpConfiguration(createExpectedLegacyDHCPIpConfigurationWithPacProxy());
        wepNetwork.meteredOverride = METERED_OVERRIDE_METERED;
        wepNetwork.allowAutojoin = true;
        expectedConfigurations.add(wepNetwork);

        WifiConfiguration pskNetwork = createExpectedLegacyPskWifiConfiguration();
        pskNetwork.setIpConfiguration(createExpectedLegacyStaticIpConfigurationWithPacProxy());
        pskNetwork.meteredOverride = METERED_OVERRIDE_NONE;
        pskNetwork.allowAutojoin = false;
        expectedConfigurations.add(pskNetwork);

        WifiConfiguration openNetwork = createExpectedLegacyOpenWifiConfiguration();
        openNetwork.setIpConfiguration(
                createExpectedLegacyStaticIpConfigurationWithStaticProxy());
        openNetwork.meteredOverride = METERED_OVERRIDE_NOT_METERED;
        openNetwork.allowAutojoin = false;
        expectedConfigurations.add(openNetwork);
        return expectedConfigurations;
    }

    /**
     * Verify that 3 network configuration is deserialized correctly from AOSP 1.2 format.
     */
    @Test
    public void testRestoreFromV1_2BackupFormat() throws Exception {
        testRestoreFromBackupData(createExpectedV1_2Configurations(),
                () -> mWifiManager.restoreBackupData(loadResourceFile(V1_2_FILE)));
    }
}
