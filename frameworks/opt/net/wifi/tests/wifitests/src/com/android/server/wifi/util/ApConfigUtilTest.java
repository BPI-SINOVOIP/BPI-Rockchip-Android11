/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.wifi.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.net.MacAddress;
import android.net.wifi.ScanResult;
import android.net.wifi.SoftApCapability;
import android.net.wifi.SoftApConfiguration;
import android.net.wifi.SoftApConfiguration.Builder;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiScanner;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiBaseTest;
import com.android.server.wifi.WifiNative;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Unit tests for {@link com.android.server.wifi.util.ApConfigUtil}.
 */
@SmallTest
public class ApConfigUtilTest extends WifiBaseTest {

    private static final String TEST_COUNTRY_CODE = "TestCountry";

    /**
     * Frequency to channel map. This include some frequencies used outside the US.
     * Representing it using a vector (instead of map) for simplification.  Values at
     * even indices are frequencies and odd indices are channels.
     */
    private static final int[] FREQUENCY_TO_CHANNEL_MAP = {
            2412, SoftApConfiguration.BAND_2GHZ, 1,
            2417, SoftApConfiguration.BAND_2GHZ, 2,
            2422, SoftApConfiguration.BAND_2GHZ, 3,
            2427, SoftApConfiguration.BAND_2GHZ, 4,
            2432, SoftApConfiguration.BAND_2GHZ, 5,
            2437, SoftApConfiguration.BAND_2GHZ, 6,
            2442, SoftApConfiguration.BAND_2GHZ, 7,
            2447, SoftApConfiguration.BAND_2GHZ, 8,
            2452, SoftApConfiguration.BAND_2GHZ, 9,
            2457, SoftApConfiguration.BAND_2GHZ, 10,
            2462, SoftApConfiguration.BAND_2GHZ, 11,
            /* 12, 13 are only legitimate outside the US. */
            2467, SoftApConfiguration.BAND_2GHZ, 12,
            2472, SoftApConfiguration.BAND_2GHZ, 13,
            /* 14 is for Japan, DSSS and CCK only. */
            2484, SoftApConfiguration.BAND_2GHZ, 14,
            /* 34 valid in Japan. */
            5170, SoftApConfiguration.BAND_5GHZ, 34,
            5180, SoftApConfiguration.BAND_5GHZ, 36,
            5190, SoftApConfiguration.BAND_5GHZ, 38,
            5200, SoftApConfiguration.BAND_5GHZ, 40,
            5210, SoftApConfiguration.BAND_5GHZ, 42,
            5220, SoftApConfiguration.BAND_5GHZ, 44,
            5230, SoftApConfiguration.BAND_5GHZ, 46,
            5240, SoftApConfiguration.BAND_5GHZ, 48,
            5260, SoftApConfiguration.BAND_5GHZ, 52,
            5280, SoftApConfiguration.BAND_5GHZ, 56,
            5300, SoftApConfiguration.BAND_5GHZ, 60,
            5320, SoftApConfiguration.BAND_5GHZ, 64,
            5500, SoftApConfiguration.BAND_5GHZ, 100,
            5520, SoftApConfiguration.BAND_5GHZ, 104,
            5540, SoftApConfiguration.BAND_5GHZ, 108,
            5560, SoftApConfiguration.BAND_5GHZ, 112,
            5580, SoftApConfiguration.BAND_5GHZ, 116,
            /* 120, 124, 128 valid in Europe/Japan. */
            5600, SoftApConfiguration.BAND_5GHZ, 120,
            5620, SoftApConfiguration.BAND_5GHZ, 124,
            5640, SoftApConfiguration.BAND_5GHZ, 128,
            /* 132+ valid in US. */
            5660, SoftApConfiguration.BAND_5GHZ, 132,
            5680, SoftApConfiguration.BAND_5GHZ, 136,
            5700, SoftApConfiguration.BAND_5GHZ, 140,
            /* 144 is supported by a subset of WiFi chips. */
            5720, SoftApConfiguration.BAND_5GHZ, 144,
            5745, SoftApConfiguration.BAND_5GHZ, 149,
            5765, SoftApConfiguration.BAND_5GHZ, 153,
            5785, SoftApConfiguration.BAND_5GHZ, 157,
            5805, SoftApConfiguration.BAND_5GHZ, 161,
            5825, SoftApConfiguration.BAND_5GHZ, 165,
            5845, SoftApConfiguration.BAND_5GHZ, 169,
            5865, SoftApConfiguration.BAND_5GHZ, 173,
            /* Now some 6GHz channels */
            5955, SoftApConfiguration.BAND_6GHZ, 1,
            5970, SoftApConfiguration.BAND_6GHZ, 4,
            6110, SoftApConfiguration.BAND_6GHZ, 32
    };

    private static final int[] EMPTY_CHANNEL_LIST = {};
    private static final int[] ALLOWED_2G_FREQS = {2462}; //ch# 11
    private static final int[] ALLOWED_5G_FREQS = {5745, 5765}; //ch# 149, 153
    private static final int[] ALLOWED_6G_FREQS = {5945, 5965};

    @Mock Context mContext;
    @Mock Resources mResources;
    @Mock WifiNative mWifiNative;

    /**
     * Setup test.
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    /**
     * verify convert WifiConfiguration Band to SoftApConfigurationBand.
     */
    @Test
    public void convertWifiConfigBandToSoftapConfigBandTest() throws Exception {
        assertEquals(SoftApConfiguration.BAND_2GHZ, ApConfigUtil
                .convertWifiConfigBandToSoftApConfigBand(WifiConfiguration.AP_BAND_2GHZ));
        assertEquals(SoftApConfiguration.BAND_5GHZ, ApConfigUtil
                .convertWifiConfigBandToSoftApConfigBand(WifiConfiguration.AP_BAND_5GHZ));
        assertEquals(SoftApConfiguration.BAND_2GHZ | SoftApConfiguration.BAND_5GHZ, ApConfigUtil
                .convertWifiConfigBandToSoftApConfigBand(WifiConfiguration.AP_BAND_ANY));
    }



    /**
     * Verify isMultiband success
     */
    @Test
    public void isMultibandSuccess() throws Exception {
        assertTrue(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_6GHZ));
        assertTrue(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_5GHZ
                  | SoftApConfiguration.BAND_6GHZ));
        assertTrue(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_6GHZ));
        assertTrue(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_6GHZ));
    }

    /**
     * Verify isMultiband failure
     */
    @Test
    public void isMultibandFailure() throws Exception {
        assertFalse(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_2GHZ));
        assertFalse(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_5GHZ));
        assertFalse(ApConfigUtil.isMultiband(SoftApConfiguration.BAND_6GHZ));
    }

    /**
     * Verify containsBand success
     */
    @Test
    public void containsBandSuccess() throws Exception {
        assertTrue(ApConfigUtil.containsBand(SoftApConfiguration.BAND_2GHZ,
                SoftApConfiguration.BAND_2GHZ));
        assertTrue(ApConfigUtil.containsBand(SoftApConfiguration.BAND_2GHZ
                | SoftApConfiguration.BAND_6GHZ, SoftApConfiguration.BAND_2GHZ));
        assertTrue(ApConfigUtil.containsBand(SoftApConfiguration.BAND_2GHZ
                | SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_6GHZ,
                SoftApConfiguration.BAND_6GHZ));
    }

    /**
     * Verify containsBand failure
     */
    @Test
    public void containsBandFailure() throws Exception {
        assertFalse(ApConfigUtil.containsBand(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_5GHZ, SoftApConfiguration.BAND_6GHZ));
        assertFalse(ApConfigUtil.containsBand(SoftApConfiguration.BAND_5GHZ,
                  SoftApConfiguration.BAND_6GHZ));
    }

    /**
     * Verify isBandValidSuccess
     */
    @Test
    public void isBandValidSuccess() throws Exception {
        assertTrue(ApConfigUtil.isBandValid(SoftApConfiguration.BAND_2GHZ));
        assertTrue(ApConfigUtil.isBandValid(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_6GHZ));
        assertTrue(ApConfigUtil.isBandValid(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_6GHZ));
    }

    /**
     * Verify isBandValidFailure
     */
    @Test
    public void isBandValidFailure() throws Exception {
        assertFalse(ApConfigUtil.isBandValid(0));
        assertFalse(ApConfigUtil.isBandValid(SoftApConfiguration.BAND_2GHZ
                  | SoftApConfiguration.BAND_6GHZ | 0x0F));
    }

    /**
     * verify frequency to band conversion for all possible frequencies.
     */
    @Test
    public void convertFrequencytoBand() throws Exception {
        for (int i = 0; i < FREQUENCY_TO_CHANNEL_MAP.length; i += 3) {
            assertEquals(FREQUENCY_TO_CHANNEL_MAP[i + 1],
                    ApConfigUtil.convertFrequencyToBand(
                            FREQUENCY_TO_CHANNEL_MAP[i]));
        }
    }

    /**
     * verify channel/band to frequency conversion for all possible channels.
     */
    @Test
    public void convertChannelToFrequency() throws Exception {
        for (int i = 0; i < FREQUENCY_TO_CHANNEL_MAP.length; i += 3) {
            assertEquals(FREQUENCY_TO_CHANNEL_MAP[i],
                    ApConfigUtil.convertChannelToFrequency(
                            FREQUENCY_TO_CHANNEL_MAP[i + 2], FREQUENCY_TO_CHANNEL_MAP[i + 1]));
        }
    }

    /**
     * Test convert string to channel list
     */
    @Test
    public void testConvertStringToChannelList() throws Exception {
        assertEquals(Arrays.asList(1, 6, 11), ApConfigUtil.convertStringToChannelList("1, 6, 11"));
        assertEquals(Arrays.asList(1, 6, 11), ApConfigUtil.convertStringToChannelList("1,6,11"));
        assertEquals(Arrays.asList(1, 9, 10, 11),
                ApConfigUtil.convertStringToChannelList("1, 9-11"));
        assertEquals(Arrays.asList(1, 6, 7, 10, 11),
                ApConfigUtil.convertStringToChannelList("1,6-7, 10-11"));
        assertEquals(Arrays.asList(1, 11),
                ApConfigUtil.convertStringToChannelList("1,6a,11"));
        assertEquals(Arrays.asList(1, 11), ApConfigUtil.convertStringToChannelList("1,6-3,11"));
        assertEquals(Arrays.asList(1),
                ApConfigUtil.convertStringToChannelList("1, abc , def - rsv"));
    }

    /**
     * Test get available channel freq for band
     */
    /**
     * Verify default channel is used when picking a 2G channel without
     * any allowed 2G channels.
     */
    @Test
    public void chooseApChannel2GBandWithNoAllowedChannel() throws Exception {
        int[] allowed2gChannels = {};
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_24_GHZ))
                .thenReturn(allowed2gChannels);
        int freq = ApConfigUtil.chooseApChannel(SoftApConfiguration.BAND_2GHZ, mWifiNative,
                mResources);
        assertEquals(ApConfigUtil.DEFAULT_AP_CHANNEL,
                ScanResult.convertFrequencyMhzToChannel(freq));
    }

    /**
     * Verify a 2G channel is selected from the list of allowed channels.
     */
    @Test
    public void chooseApChannel2GBandWithAllowedChannels() throws Exception {
        when(mResources.getString(R.string.config_wifiSoftap2gChannelList))
                .thenReturn("1, 6, 11");
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_24_GHZ))
                .thenReturn(ALLOWED_2G_FREQS); // ch#11

        int freq = ApConfigUtil.chooseApChannel(SoftApConfiguration.BAND_2GHZ, mWifiNative,
                mResources);
        assertEquals(2462, freq);
    }

    /**
     * Verify a 5G channel is selected from the list of allowed channels.
     */
    @Test
    public void chooseApChannel5GBandWithAllowedChannels() throws Exception {
        when(mResources.getString(R.string.config_wifiSoftap5gChannelList))
                .thenReturn("149, 36-100");
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ))
                .thenReturn(ALLOWED_5G_FREQS); //ch# 149, 153

        int freq = ApConfigUtil.chooseApChannel(
                SoftApConfiguration.BAND_5GHZ, mWifiNative, mResources);
        assertTrue(ArrayUtils.contains(ALLOWED_5G_FREQS, freq));
    }

    /**
     * Verify chooseApChannel failed when selecting a channel in 5GHz band
     * with no channels allowed.
     */
    @Test
    public void chooseApChannel5GBandWithNoAllowedChannels() throws Exception {
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ))
                .thenReturn(EMPTY_CHANNEL_LIST);
        assertEquals(-1, ApConfigUtil.chooseApChannel(SoftApConfiguration.BAND_5GHZ, mWifiNative,
                mResources));
    }

    /**
     * Verify chooseApChannel will select high band channel.
     */
    @Test
    public void chooseApChannelWillHighBandPrefer() throws Exception {
        when(mResources.getString(R.string.config_wifiSoftap2gChannelList))
                .thenReturn("1, 6, 11");
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_24_GHZ))
                .thenReturn(ALLOWED_2G_FREQS); // ch#11
        when(mResources.getString(R.string.config_wifiSoftap5gChannelList))
                .thenReturn("149, 153");
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ))
                .thenReturn(ALLOWED_5G_FREQS); //ch# 149, 153

        int freq = ApConfigUtil.chooseApChannel(
                SoftApConfiguration.BAND_2GHZ | SoftApConfiguration.BAND_5GHZ,
                mWifiNative, mResources);
        assertTrue(ArrayUtils.contains(ALLOWED_5G_FREQS, freq));
    }


    /**
     * Verify default band and channel is used when HAL support is
     * not available.
     */
    @Test
    public void updateApChannelConfigWithoutHal() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setChannel(36, SoftApConfiguration.BAND_5GHZ);

        when(mWifiNative.isHalStarted()).thenReturn(false);
        assertEquals(ApConfigUtil.SUCCESS,
                ApConfigUtil.updateApChannelConfig(mWifiNative, mResources, TEST_COUNTRY_CODE,
                configBuilder, configBuilder.build(), false));
        /* Verify default band and channel is used. */
        assertEquals(ApConfigUtil.DEFAULT_AP_BAND, configBuilder.build().getBand());
        assertEquals(ApConfigUtil.DEFAULT_AP_CHANNEL, configBuilder.build().getChannel());
    }

    /**
     * Verify updateApChannelConfig will return an error when selecting channel
     * for 5GHz band without country code.
     */
    @Test
    public void updateApChannelConfig5GBandNoCountryCode() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        when(mWifiNative.isHalStarted()).thenReturn(true);
        assertEquals(ApConfigUtil.ERROR_GENERIC,
                ApConfigUtil.updateApChannelConfig(mWifiNative, mResources, null,
                configBuilder, configBuilder.build(), false));
    }

    /**
     * Verify the AP band and channel is not updated if specified.
     */
    @Test
    public void updateApChannelConfigWithChannelSpecified() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setChannel(36, SoftApConfiguration.BAND_5GHZ);
        when(mWifiNative.isHalStarted()).thenReturn(true);
        assertEquals(ApConfigUtil.SUCCESS,
                ApConfigUtil.updateApChannelConfig(mWifiNative, mResources, TEST_COUNTRY_CODE,
                configBuilder, configBuilder.build(), false));
        assertEquals(SoftApConfiguration.BAND_5GHZ, configBuilder.build().getBand());
        assertEquals(36, configBuilder.build().getChannel());
    }

    /**
     * Verify updateApChannelConfig will return an error when selecting 5GHz channel
     * without any allowed channels.
     */
    @Test
    public void updateApChannelConfigWith5GBandNoChannelAllowed() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ);
        when(mWifiNative.isHalStarted()).thenReturn(true);
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ))
                .thenReturn(EMPTY_CHANNEL_LIST);
        assertEquals(ApConfigUtil.ERROR_NO_CHANNEL,
                ApConfigUtil.updateApChannelConfig(mWifiNative, mResources, TEST_COUNTRY_CODE,
                configBuilder, configBuilder.build(), false));
    }

    /**
     * Verify updateApChannelConfig will select a channel number that meets OEM restriction
     * when acs is disabled.
     */
    @Test
    public void updateApChannelConfigWithAcsDisabledOemConfigured() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_2GHZ);
        when(mResources.getString(R.string.config_wifiSoftap2gChannelList))
                .thenReturn("6");
        when(mResources.getString(R.string.config_wifiSoftap5gChannelList))
                .thenReturn("149, 36-100");
        when(mWifiNative.isHalStarted()).thenReturn(true);
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_24_GHZ))
                .thenReturn(ALLOWED_2G_FREQS); // ch# 11
        when(mWifiNative.getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ))
                .thenReturn(ALLOWED_5G_FREQS); // ch# 149, 153
        when(mWifiNative.isHalStarted()).thenReturn(true);
        assertEquals(ApConfigUtil.SUCCESS,
                ApConfigUtil.updateApChannelConfig(mWifiNative, mResources, TEST_COUNTRY_CODE,
                configBuilder, configBuilder.build(), false));
        assertEquals(SoftApConfiguration.BAND_5GHZ, configBuilder.build().getBand());
        assertEquals(149, configBuilder.build().getChannel());
    }

    /**
     * Verify updateApChannelConfig will not select a channel number and band when acs is
     * enabled.
     */
    @Test
    public void updateApChannelConfigWithAcsEnabled() throws Exception {
        Builder configBuilder = new SoftApConfiguration.Builder();
        configBuilder.setBand(SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_2GHZ);
        when(mWifiNative.isHalStarted()).thenReturn(true);
        assertEquals(ApConfigUtil.SUCCESS,
                ApConfigUtil.updateApChannelConfig(mWifiNative, mResources, TEST_COUNTRY_CODE,
                configBuilder, configBuilder.build(), true));
        assertEquals(SoftApConfiguration.BAND_5GHZ | SoftApConfiguration.BAND_2GHZ,
                configBuilder.build().getBand());
        assertEquals(0, configBuilder.build().getChannel());
    }

    @Test
    public void testSoftApCapabilityInitWithResourceValue() throws Exception {
        long testFeatures = SoftApCapability.SOFTAP_FEATURE_CLIENT_FORCE_DISCONNECT;
        SoftApCapability capability = new SoftApCapability(testFeatures);
        int test_max_client = 10;
        capability.setMaxSupportedClients(test_max_client);

        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getInteger(R.integer.config_wifiHardwareSoftapMaxClientCount))
                .thenReturn(test_max_client);
        when(mResources.getBoolean(R.bool.config_wifi_softap_acs_supported))
                .thenReturn(false);
        when(mResources.getBoolean(R.bool.config_wifiSofapClientForceDisconnectSupported))
                .thenReturn(true);
        assertEquals(ApConfigUtil.updateCapabilityFromResource(mContext),
                capability);
    }

    @Test
    public void testConvertInvalidWifiConfigurationToSoftApConfiguration() throws Exception {
        WifiConfiguration wifiConfig = new WifiConfiguration();
        wifiConfig.SSID = "AndroidAP";
        wifiConfig.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA2_PSK);
        wifiConfig.preSharedKey = "1233443";
        assertNull(ApConfigUtil.fromWifiConfiguration(wifiConfig));
    }


    @Test
    public void testCheckConfigurationChangeNeedToRestart() throws Exception {
        SoftApConfiguration currentConfig = new SoftApConfiguration.Builder()
                .setSsid("TestSSid")
                .setBssid(MacAddress.fromString("11:22:33:44:55:66"))
                .setPassphrase("testpassphrase", SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .setBand(SoftApConfiguration.BAND_2GHZ)
                .setChannel(11, SoftApConfiguration.BAND_2GHZ)
                .setHiddenSsid(true)
                .build();

        // Test no changed
        // DO NOT use copy constructor to copy to test since it's instance is the same.
        SoftApConfiguration newConfig_noChange = new SoftApConfiguration.Builder()
                .setSsid("TestSSid")
                .setBssid(MacAddress.fromString("11:22:33:44:55:66"))
                .setPassphrase("testpassphrase", SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
                .setBand(SoftApConfiguration.BAND_2GHZ)
                .setChannel(11, SoftApConfiguration.BAND_2GHZ)
                .setHiddenSsid(true)
                .build();
        assertFalse(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_noChange));

        // Test SSID changed
        SoftApConfiguration newConfig_ssidChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setSsid("NewTestSSid").build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_ssidChanged));
        // Test BSSID changed
        SoftApConfiguration newConfig_bssidChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setBssid(MacAddress.fromString("aa:bb:cc:dd:ee:ff")).build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_bssidChanged));
        // Test Passphrase Changed
        SoftApConfiguration newConfig_passphraseChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setPassphrase("newtestpassphrase",
                SoftApConfiguration.SECURITY_TYPE_WPA2_PSK).build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_passphraseChanged));
        // Test Security Type Changed
        SoftApConfiguration newConfig_securityeChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setPassphrase("newtestpassphrase",
                SoftApConfiguration.SECURITY_TYPE_WPA3_SAE).build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_securityeChanged));
        // Test Channel Changed
        SoftApConfiguration newConfig_channelChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setChannel(6, SoftApConfiguration.BAND_2GHZ).build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_channelChanged));
        // Test Band Changed
        SoftApConfiguration newConfig_bandChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setBand(SoftApConfiguration.BAND_5GHZ).build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_bandChanged));
        // Test isHidden Changed
        SoftApConfiguration newConfig_hiddenChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setHiddenSsid(false).build();
        assertTrue(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_hiddenChanged));
        // Test Others Changed
        SoftApConfiguration newConfig_nonRevalentChanged = new SoftApConfiguration
                .Builder(newConfig_noChange)
                .setMaxNumberOfClients(10)
                .setAutoShutdownEnabled(false)
                .setShutdownTimeoutMillis(500000)
                .setClientControlByUserEnabled(true)
                .setBlockedClientList(new ArrayList<>())
                .setAllowedClientList(new ArrayList<>())
                .build();
        assertFalse(ApConfigUtil.checkConfigurationChangeNeedToRestart(currentConfig,
                newConfig_nonRevalentChanged));
    }
}
