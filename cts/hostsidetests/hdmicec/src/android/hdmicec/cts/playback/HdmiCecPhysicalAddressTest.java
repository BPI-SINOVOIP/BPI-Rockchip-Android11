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

import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.Test;

/** HDMI CEC test to verify physical address after device reboot (Section 10.1.2) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecPhysicalAddressTest extends BaseHostJUnit4Test {

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
     * Test 10.1.2-1
     * Tests that the device broadcasts a <REPORT_PHYSICAL_ADDRESS> after a reboot and that the
     * device has taken the physical address 1.0.0.0.
     */
    @Test
    public void cect_10_1_2_1_RebootPhysicalAddress() throws Exception {
        ITestDevice device = getDevice();
        device.executeShellCommand("reboot");
        device.waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.REPORT_PHYSICAL_ADDRESS);
        int physicalAddress = CecMessage.getParams(message,
            HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH);
        assertThat(HdmiCecConstants.PHYSICAL_ADDRESS).isEqualTo(physicalAddress);
    }
}
