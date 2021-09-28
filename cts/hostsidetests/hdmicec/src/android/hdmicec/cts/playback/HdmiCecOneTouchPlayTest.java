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

import android.hdmicec.cts.CecMessage;
import android.hdmicec.cts.CecOperand;
import android.hdmicec.cts.HdmiCecClientWrapper;
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

/** HDMI CEC tests for One Touch Play (Section 11.2.1) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecOneTouchPlayTest extends BaseHostJUnit4Test {

    private static final int PHYSICAL_ADDRESS = 0x1000;

    /** Intent to launch the remote pairing activity */
    private static final String ACTION_CONNECT_INPUT_NORMAL =
            "com.google.android.intent.action.CONNECT_INPUT";

    /** Package name of the Settings app */
    private static final String SETTINGS_PACKAGE =
            "com.android.tv.settings";

    /** The command to broadcast an intent. */
    private static final String START_COMMAND = "am start -a ";

    /** The command to stop an app. */
    private static final String FORCE_STOP_COMMAND = "am force-stop ";

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
     * Test 11.2.1-1
     * Tests that the device sends a <TEXT_VIEW_ON> when the home key is pressed on device, followed
     * by a <ACTIVE_SOURCE> message.
     */
    @Test
    public void cect_11_2_1_1_OneTouchPlay() throws Exception {
        ITestDevice device = getDevice();
        device.reboot();
        device.executeShellCommand("input keyevent KEYCODE_HOME");
        hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.TEXT_VIEW_ON);
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.ACTIVE_SOURCE);
        assertThat(CecMessage.getParams(message)).isEqualTo(PHYSICAL_ADDRESS);
    }

    /**
     * Tests that the device sends a <TEXT_VIEW_ON> when the pairing activity is started on
     * device, followed by a <ACTIVE_SOURCE> message.
     */
    @Test
    public void cect_PairingActivity_OneTouchPlay() throws Exception {
        ITestDevice device = getDevice();
        device.reboot();
        device.executeShellCommand(START_COMMAND + ACTION_CONNECT_INPUT_NORMAL);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.TEXT_VIEW_ON);
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.ACTIVE_SOURCE);
        assertThat(CecMessage.getParams(message)).isEqualTo(PHYSICAL_ADDRESS);
        device.executeShellCommand(FORCE_STOP_COMMAND + SETTINGS_PACKAGE);
    }
}
