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
import android.hdmicec.cts.HdmiCecConstants;
import android.hdmicec.cts.LogicalAddress;
import android.hdmicec.cts.RequiredPropertyRule;
import android.hdmicec.cts.RequiredFeatureRule;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Ignore;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.Test;

/** HDMI CEC test to verify system audio control commands (Section 11.2.15) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecSystemAudioControlTest extends BaseHostJUnit4Test {

    private static final LogicalAddress PLAYBACK_DEVICE = LogicalAddress.PLAYBACK_1;

    public HdmiCecClientWrapper hdmiCecClient =
        new HdmiCecClientWrapper(LogicalAddress.PLAYBACK_1, "-t", "a");

    @Rule
    public RuleChain ruleChain =
        RuleChain
            .outerRule(new RequiredFeatureRule(this, LogicalAddress.HDMI_CEC_FEATURE))
            .around(new RequiredFeatureRule(this, LogicalAddress.LEANBACK_FEATURE))
            .around(RequiredPropertyRule.asCsvContainsValue(
                this,
                LogicalAddress.HDMI_DEVICE_TYPE_PROPERTY,
                PLAYBACK_DEVICE.getDeviceType()))
            .around(hdmiCecClient);

    /**
     * Test 11.2.15-10
     * Tests that the device sends a <GIVE_SYSTEM_AUDIO_STATUS> message when brought out of standby
     */
    @Test
    public void cect_11_2_15_10_GiveSystemAudioModeStatus() throws Exception {
        ITestDevice device = getDevice();
        /* Home Key to prevent device from going to deep suspend state */
        device.executeShellCommand("input keyevent KEYCODE_HOME");
        device.executeShellCommand("input keyevent KEYCODE_SLEEP");
        device.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM,
                CecOperand.GIVE_SYSTEM_AUDIO_MODE_STATUS);
    }

    /**
     * Test 11.2.15-11
     * Tests that the device sends <USER_CONTROL_PRESSED> and <USER_CONTROL_RELEASED> messages when
     * the volume up and down keys are pressed on the DUT. Test also verifies that the
     * <USER_CONTROL_PRESSED> message has the right control param.
     */
    @Ignore("b/162836413")
    @Test
    public void cect_11_2_15_11_VolumeUpDownUserControlPressed() throws Exception {
        ITestDevice device = getDevice();
        hdmiCecClient.sendCecMessage(LogicalAddress.AUDIO_SYSTEM, LogicalAddress.BROADCAST,
                CecOperand.SET_SYSTEM_AUDIO_MODE, CecMessage.formatParams(1));
        device.executeShellCommand("input keyevent KEYCODE_VOLUME_UP");
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM,
                CecOperand.USER_CONTROL_PRESSED);
        assertThat(CecMessage.getParams(message))
                .isEqualTo(HdmiCecConstants.CEC_CONTROL_VOLUME_UP);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM, CecOperand.USER_CONTROL_RELEASED);


        device.executeShellCommand("input keyevent KEYCODE_VOLUME_DOWN");
        message = hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM,
                CecOperand.USER_CONTROL_PRESSED);
        assertThat(CecMessage.getParams(message))
                .isEqualTo(HdmiCecConstants.CEC_CONTROL_VOLUME_DOWN);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM, CecOperand.USER_CONTROL_RELEASED);
    }

    /**
     * Test 11.2.15-12
     * Tests that the device sends <USER_CONTROL_PRESSED> and <USER_CONTROL_RELEASED> messages when
     * the mute key is pressed on the DUT. Test also verifies that the <USER_CONTROL_PRESSED>
     * message has the right control param.
     */
    @Ignore("b/162836413")
    @Test
    public void cect_11_2_15_12_MuteUserControlPressed() throws Exception {
        ITestDevice device = getDevice();
        hdmiCecClient.sendCecMessage(LogicalAddress.AUDIO_SYSTEM, LogicalAddress.BROADCAST,
                CecOperand.SET_SYSTEM_AUDIO_MODE, CecMessage.formatParams(1));
        device.executeShellCommand("input keyevent KEYCODE_VOLUME_MUTE");
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM,
                CecOperand.USER_CONTROL_PRESSED);
        assertThat(CecMessage.getParams(message)).isEqualTo(HdmiCecConstants.CEC_CONTROL_MUTE);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.AUDIO_SYSTEM, CecOperand.USER_CONTROL_RELEASED);
    }
}
