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

package android.hdmicec.cts.playback;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.hdmicec.cts.CecClientMessage;
import android.hdmicec.cts.CecMessage;
import android.hdmicec.cts.CecOperand;
import android.hdmicec.cts.HdmiCecClientWrapper;
import android.hdmicec.cts.HdmiCecConstants;
import android.hdmicec.cts.LogicalAddress;
import android.hdmicec.cts.RequiredPropertyRule;
import android.hdmicec.cts.RequiredFeatureRule;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.Test;

/** HDMI CEC system information tests (Section 11.2.6) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecSystemInformationTest extends BaseHostJUnit4Test {

    /** The version number 0x05 refers to CEC v1.4 */
    private static final int CEC_VERSION_NUMBER = 0x05;

    private static final String PROPERTY_LOCALE = "persist.sys.locale";

    public HdmiCecClientWrapper hdmiCecClient = new HdmiCecClientWrapper(LogicalAddress.PLAYBACK_1);

    @Rule
    public RuleChain ruleChain =
        RuleChain
            .outerRule(new RequiredFeatureRule(this, LogicalAddress.HDMI_CEC_FEATURE))
            .around(new RequiredFeatureRule(this, LogicalAddress.LEANBACK_FEATURE))
            .around(RequiredPropertyRule.asCsvContainsValue(
                this,
                LogicalAddress.HDMI_DEVICE_TYPE_PROPERTY,
                LogicalAddress.PLAYBACK_1.getDeviceType()))
            .around(hdmiCecClient);

    /**
     * Test 11.2.6-1
     * Tests for Ack <Polling Message> message.
     */
    @Test
    public void cect_11_2_6_1_Ack() throws Exception {
        String command = CecClientMessage.POLL + " " + LogicalAddress.PLAYBACK_1;
        String expectedOutput = "POLL sent";
        hdmiCecClient.sendConsoleMessage(command);
        if (!hdmiCecClient.checkConsoleOutput(expectedOutput)) {
            throw new Exception("Could not find " + expectedOutput);
        }
    }

    /**
     * Test 11.2.6-2
     * Tests that the device sends a <REPORT_PHYSICAL_ADDRESS> in response to a
     * <GIVE_PHYSICAL_ADDRESS>
     */
    @Test
    public void cect_11_2_6_2_GivePhysicalAddress() throws Exception {
        hdmiCecClient.sendCecMessage(CecOperand.GIVE_PHYSICAL_ADDRESS);
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.REPORT_PHYSICAL_ADDRESS);
        /* The checkExpectedOutput has already verified the first 4 nibbles of the message. We
            * have to verify the last 6 nibbles */
        int receivedParams = CecMessage.getParams(message);
        assertThat(HdmiCecConstants.PHYSICAL_ADDRESS).isEqualTo(receivedParams >> 8);
        assertThat(HdmiCecConstants.PLAYBACK_DEVICE_TYPE).isEqualTo(receivedParams & 0xFF);
    }

    /**
     * Test 11.2.6-6
     * Tests that the device sends a <CEC_VERSION> in response to a <GET_CEC_VERSION>
     */
    @Test
    public void cect_11_2_6_6_GiveCecVersion() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, CecOperand.GET_CEC_VERSION);
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
                                                            CecOperand.CEC_VERSION);
        assertThat(CecMessage.getParams(message)).isEqualTo(CEC_VERSION_NUMBER);
    }

    /**
     * Test 11.2.6-7
     * Tests that the device sends a <FEATURE_ABORT> in response to a <GET_MENU_LANGUAGE>
     */
    @Test
    public void cect_11_2_6_7_GetMenuLanguage() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, CecOperand.GET_MENU_LANGUAGE);
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.FEATURE_ABORT);
        int abortedOpcode = CecMessage.getParams(message,
                CecOperand.GET_MENU_LANGUAGE.toString().length());
        assertThat(CecOperand.getOperand(abortedOpcode)).isEqualTo(CecOperand.GET_MENU_LANGUAGE);
    }

    private String getSystemLocale() throws Exception {
        ITestDevice device = getDevice();
        return device.executeShellCommand("getprop " + PROPERTY_LOCALE).trim();
    }

    private void setSystemLocale(String locale) throws Exception {
        ITestDevice device = getDevice();
        device.executeShellCommand("setprop " + PROPERTY_LOCALE + " " + locale);
    }

    private boolean isLanguageEditable() throws Exception {
        String val = getDevice().executeShellCommand("getprop ro.hdmi.set_menu_language");
        return val.trim().equals("true") ? true : false;
    }

    private static String extractLanguage(String locale) {
        return locale.split("[^a-zA-Z]")[0];
    }

    /**
     * Test 11.2.6-3
     * Tests that the device handles a <SET_MENU_LANGUAGE> with a valid language correctly.
     */
    @Test
    public void cect_11_2_6_3_SetValidMenuLanguage() throws Exception {
        assumeTrue(isLanguageEditable());
        final String locale = getSystemLocale();
        final String originalLanguage = extractLanguage(locale);
        final String language = originalLanguage.equals("spa") ? "eng" : "spa";
        final String newLanguage = originalLanguage.equals("spa") ? "en" : "es";
        try {
            hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST,
                    CecOperand.SET_MENU_LANGUAGE, CecMessage.convertStringToHexParams(language));
            assertThat(extractLanguage(getSystemLocale())).isEqualTo(newLanguage);
        } finally {
            setSystemLocale(locale);
        }
    }

    /**
     * Test 11.2.6-4
     * Tests that the device ignores a <SET_MENU_LANGUAGE> with an invalid language.
     */
    @Test
    public void cect_11_2_6_4_SetInvalidMenuLanguage() throws Exception {
        assumeTrue(isLanguageEditable());
        final String locale = getSystemLocale();
        final String originalLanguage = extractLanguage(locale);
        final String language = "spb";
        try {
            hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST,
                    CecOperand.SET_MENU_LANGUAGE, CecMessage.convertStringToHexParams(language));
            assertThat(extractLanguage(getSystemLocale())).isEqualTo(originalLanguage);
        } finally {
            setSystemLocale(locale);
        }
    }

    /**
     * Test 11.2.6-5
     * Tests that the device ignores a <SET_MENU_LANGUAGE> with a valid language that comes from a
     * source device which is not TV.
     */
    @Test
    public void cect_11_2_6_5_SetValidMenuLanguageFromInvalidSource() throws Exception {
        assumeTrue(isLanguageEditable());
        final String locale = getSystemLocale();
        final String originalLanguage = extractLanguage(locale);
        final String language = originalLanguage.equals("spa") ? "eng" : "spa";
        try {
            hdmiCecClient.sendCecMessage(LogicalAddress.RECORDER_1, LogicalAddress.BROADCAST,
                    CecOperand.SET_MENU_LANGUAGE, CecMessage.convertStringToHexParams(language));
            assertThat(extractLanguage(getSystemLocale())).isEqualTo(originalLanguage);
        } finally {
            setSystemLocale(locale);
        }
    }
}
