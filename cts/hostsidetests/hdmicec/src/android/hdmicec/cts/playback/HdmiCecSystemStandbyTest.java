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

/** HDMI CEC test to verify the device handles standby correctly (Section 11.2.3) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecSystemStandbyTest extends BaseHostJUnit4Test {

    private static final LogicalAddress PLAYBACK_DEVICE = LogicalAddress.PLAYBACK_1;

    private static final String HDMI_CONTROL_DEVICE_AUTO_OFF =
            "hdmi_control_auto_device_off_enabled";

    public HdmiCecClientWrapper hdmiCecClient = new HdmiCecClientWrapper(LogicalAddress.PLAYBACK_1);

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

    private boolean setHdmiControlDeviceAutoOff(boolean turnOn) throws Exception {
        ITestDevice device = getDevice();
        String val = device.executeShellCommand("settings get global " +
                HDMI_CONTROL_DEVICE_AUTO_OFF).trim();
        String valToSet = turnOn ? "1" : "0";
        device.executeShellCommand("settings put global "
                + HDMI_CONTROL_DEVICE_AUTO_OFF + " " + valToSet);
        device.executeShellCommand("settings get global " + HDMI_CONTROL_DEVICE_AUTO_OFF).trim();
        return val.equals("1") ? true : false;
    }

    private void checkDeviceAsleepAfterStandbySent(LogicalAddress source, LogicalAddress destination)
            throws Exception {
        ITestDevice device = getDevice();
        try {
            device.executeShellCommand("input keyevent KEYCODE_HOME");
            TimeUnit.SECONDS.sleep(5);
            hdmiCecClient.sendCecMessage(source, destination, CecOperand.STANDBY);
            TimeUnit.SECONDS.sleep(5);
            String wakeState = device.executeShellCommand("dumpsys power | grep mWakefulness=");
            assertThat(wakeState.trim()).isEqualTo("mWakefulness=Asleep");
        } finally {
            /* Wake up the device */
            device.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        }
    }

    /**
     * Test 11.2.3-2
     * Tests that the device goes into standby when a <STANDBY> message is broadcast.
     */
    @Test
    public void cect_11_2_3_2_HandleBroadcastStandby() throws Exception {
        getDevice().executeShellCommand("reboot");
        getDevice().waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
        try {
            TimeUnit.SECONDS.sleep(5);
            checkDeviceAsleepAfterStandbySent(LogicalAddress.TV, LogicalAddress.BROADCAST);
            /* Wake up the TV */
            hdmiCecClient.sendConsoleMessage("on " + LogicalAddress.TV);
            checkDeviceAsleepAfterStandbySent(LogicalAddress.RECORDER_1, LogicalAddress.BROADCAST);
            /* Wake up the TV */
            hdmiCecClient.sendConsoleMessage("on " + LogicalAddress.TV);
            checkDeviceAsleepAfterStandbySent(LogicalAddress.AUDIO_SYSTEM, LogicalAddress.BROADCAST);
            /* Wake up the TV */
            hdmiCecClient.sendConsoleMessage("on " + LogicalAddress.TV);
            checkDeviceAsleepAfterStandbySent(LogicalAddress.PLAYBACK_2, LogicalAddress.BROADCAST);
        } finally {
            /* Wake up the TV */
            hdmiCecClient.sendConsoleMessage("on " + LogicalAddress.TV);
        }
    }

    /**
     * Test 11.2.3-3
     * Tests that the device goes into standby when a <STANDBY> message is sent to it.
     */
    @Test
    public void cect_11_2_3_3_HandleAddressedStandby() throws Exception {
        getDevice().executeShellCommand("reboot");
        getDevice().waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
        checkDeviceAsleepAfterStandbySent(LogicalAddress.TV, LogicalAddress.PLAYBACK_1);
        checkDeviceAsleepAfterStandbySent(LogicalAddress.RECORDER_1, LogicalAddress.PLAYBACK_1);
        checkDeviceAsleepAfterStandbySent(LogicalAddress.AUDIO_SYSTEM, LogicalAddress.PLAYBACK_1);
        checkDeviceAsleepAfterStandbySent(LogicalAddress.PLAYBACK_2, LogicalAddress.PLAYBACK_1);
        checkDeviceAsleepAfterStandbySent(LogicalAddress.BROADCAST, LogicalAddress.PLAYBACK_1);
    }

    /**
     * Test 11.2.3-4
     * Tests that the device does not broadcast a <STANDBY> when going into standby mode.
     */
    @Test
    public void cect_11_2_3_4_NoBroadcastStandby() throws Exception {
        ITestDevice device = getDevice();
        boolean wasOn = setHdmiControlDeviceAutoOff(false);
        try {
            device.executeShellCommand("input keyevent KEYCODE_SLEEP");
            hdmiCecClient.checkOutputDoesNotContainMessage(LogicalAddress.BROADCAST, CecOperand.STANDBY);
            device.executeShellCommand("input keyevent KEYCODE_WAKEUP");
        } finally {
            setHdmiControlDeviceAutoOff(wasOn);
        }
    }
}
