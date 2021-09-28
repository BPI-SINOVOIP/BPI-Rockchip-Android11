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

import java.util.concurrent.TimeUnit;

/** HDMI CEC test to check if the device reports power status correctly (Section 11.2.14) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecPowerStatusTest extends BaseHostJUnit4Test {

    private static final int ON = 0x0;
    private static final int OFF = 0x1;
    private static final int IN_TRANSITION_TO_STANDBY = 0x3;

    private static final int SLEEP_TIMESTEP_SECONDS = 1;
    private static final int WAIT_TIME = 5;
    private static final int MAX_SLEEP_TIME = 8;

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
     * Test 11.2.14-1
     * Tests that the device broadcasts a <REPORT_POWER_STATUS> with params 0x0 when the device is
     * powered on.
     */
    @Test
    public void cect_11_2_14_1_PowerStatusWhenOn() throws Exception {
        ITestDevice device = getDevice();
        /* Make sure the device is not booting up/in standby */
        device.waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, CecOperand.GIVE_POWER_STATUS);
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
            CecOperand.REPORT_POWER_STATUS);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
    }

    /**
     * Test 11.2.14-2
     * Tests that the device broadcasts a <REPORT_POWER_STATUS> with params 0x1 when the device is
     * powered on.
     */
    @Test
    public void cect_11_2_14_2_PowerStatusWhenOff() throws Exception {
        ITestDevice device = getDevice();
        try {
            /* Make sure the device is not booting up/in standby */
            device.waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
            /* Home Key to prevent device from going to deep suspend state */
            device.executeShellCommand("input keyevent KEYCODE_HOME");
            device.executeShellCommand("input keyevent KEYCODE_SLEEP");
            TimeUnit.SECONDS.sleep(WAIT_TIME);
            int waitTimeSeconds = WAIT_TIME;
            int powerStatus;
            do {
                TimeUnit.SECONDS.sleep(SLEEP_TIMESTEP_SECONDS);
                waitTimeSeconds += SLEEP_TIMESTEP_SECONDS;
                hdmiCecClient.sendCecMessage(LogicalAddress.TV, CecOperand.GIVE_POWER_STATUS);
                powerStatus = CecMessage.getParams(hdmiCecClient.checkExpectedOutput(
                        LogicalAddress.TV, CecOperand.REPORT_POWER_STATUS));
            } while (powerStatus == IN_TRANSITION_TO_STANDBY && waitTimeSeconds <= MAX_SLEEP_TIME);
            assertThat(powerStatus).isEqualTo(OFF);
        } finally {
            /* Wake up the device */
            device.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        }
    }
}
