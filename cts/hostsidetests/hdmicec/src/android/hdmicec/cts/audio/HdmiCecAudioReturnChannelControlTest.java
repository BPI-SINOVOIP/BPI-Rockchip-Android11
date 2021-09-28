/*
 * Copyright 2020 The Android Open Source Project
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

package android.hdmicec.cts.audio;

import static org.junit.Assume.assumeNoException;

import android.hdmicec.cts.CecMessage;
import android.hdmicec.cts.CecOperand;
import android.hdmicec.cts.HdmiCecClientWrapper;
import android.hdmicec.cts.HdmiCecConstants;
import android.hdmicec.cts.LogicalAddress;
import android.hdmicec.cts.RequiredPropertyRule;
import android.hdmicec.cts.RequiredFeatureRule;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Ignore;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.Test;

/** HDMI CEC test to test audio return channel control (Section 11.2.17) */
@Ignore("b/162820841")
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecAudioReturnChannelControlTest extends BaseHostJUnit4Test {

    private static final LogicalAddress AUDIO_DEVICE = LogicalAddress.AUDIO_SYSTEM;

    public HdmiCecClientWrapper hdmiCecClient = new HdmiCecClientWrapper(AUDIO_DEVICE);

    @Rule
    public RuleChain ruleChain =
        RuleChain
            .outerRule(new RequiredFeatureRule(this, LogicalAddress.HDMI_CEC_FEATURE))
            .around(new RequiredFeatureRule(this, LogicalAddress.LEANBACK_FEATURE))
            .around(RequiredPropertyRule.asCsvContainsValue(
                this,
                LogicalAddress.HDMI_DEVICE_TYPE_PROPERTY,
                AUDIO_DEVICE.getDeviceType()))
            .around(hdmiCecClient);

    private void checkArcIsInitiated(){
        try {
            hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                    CecOperand.REQUEST_ARC_INITIATION);
            hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.INITIATE_ARC);
        } catch(Exception e) {
            assumeNoException(e);
        }
    }

    /**
     * Test 11.2.17-1
     * Tests that the device sends a directly addressed <Initiate ARC> message
     * when it wants to initiate ARC.
     */
    @Test
    public void cect_11_2_17_1_InitiateArc() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST,
                CecOperand.REPORT_PHYSICAL_ADDRESS,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        getDevice().executeShellCommand("reboot");
        getDevice().waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.INITIATE_ARC);
    }

    /**
     * Test 11.2.17-2
     * Tests that the device sends a directly addressed <Terminate ARC> message
     * when it wants to terminate ARC.
     */
    @Test
    public void cect_11_2_17_2_TerminateArc() throws Exception {
        checkArcIsInitiated();
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST,
                CecOperand.REPORT_PHYSICAL_ADDRESS,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                        HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        getDevice().executeShellCommand("input keyevent KEYCODE_SLEEP");
        try {
            hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.TERMINATE_ARC);
        } finally {
            getDevice().executeShellCommand("input keyevent KEYCODE_WAKEUP");
        }
    }

    /**
     * Test 11.2.17-3
     * Tests that the device sends a directly addressed <Initiate ARC>
     * message when it is requested to initiate ARC.
     */
    @Test
    public void cect_11_2_17_3_RequestToInitiateArc() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST,
                CecOperand.REPORT_PHYSICAL_ADDRESS,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.REQUEST_ARC_INITIATION);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.INITIATE_ARC);
    }

    /**
     * Test 11.2.17-4
     * Tests that the device sends a directly addressed <Terminate ARC> message
     * when it is requested to terminate ARC.
     */
    @Test
    public void cect_11_2_17_4_RequestToTerminateArc() throws Exception {
        checkArcIsInitiated();
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST,
                CecOperand.REPORT_PHYSICAL_ADDRESS,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                        HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.REQUEST_ARC_TERMINATION);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.TERMINATE_ARC);
    }
}
