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

import android.hdmicec.cts.HdmiCecClientWrapper;
import android.hdmicec.cts.HdmiCecConstants;
import android.hdmicec.cts.LogHelper;
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

/** HDMI CEC test to check if the device reports power status correctly (Section 11.2.13) */
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecRemoteControlPassThroughTest extends BaseHostJUnit4Test {

    /**
     * The package name of the APK.
     */
    private static final String PACKAGE = "android.hdmicec.app";
    /**
     * The class name of the main activity in the APK.
     */
    private static final String CLASS = "HdmiCecKeyEventCapture";
    /**
     * The command to launch the main activity.
     */
    private static final String START_COMMAND = String.format(
            "am start -W -a android.intent.action.MAIN -n %s/%s.%s", PACKAGE, PACKAGE, CLASS);
    /**
     * The command to clear the main activity.
     */
    private static final String CLEAR_COMMAND = String.format("pm clear %s", PACKAGE);

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
     * Test 11.2.13-1
     * Tests that the device responds correctly to a <USER_CONTROL_PRESSED> message followed
     * immediately by a <USER_CONTROL_RELEASED> message.
     */
    @Test
    public void cect_11_2_13_1_UserControlPressAndRelease() throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Clear logcat.
        device.executeAdbCommand("logcat", "-c");
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND);
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_UP, false);
        LogHelper.assertLog(getDevice(), CLASS, "Short press KEYCODE_DPAD_UP");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_DOWN, false);
        LogHelper.assertLog(getDevice(), CLASS, "Short press KEYCODE_DPAD_DOWN");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_LEFT, false);
        LogHelper.assertLog(getDevice(), CLASS, "Short press KEYCODE_DPAD_LEFT");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_RIGHT, false);
        LogHelper.assertLog(getDevice(), CLASS, "Short press KEYCODE_DPAD_RIGHT");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_SELECT, false);
        LogHelper.assertLog(getDevice(), CLASS,
                "Short press KEYCODE_DPAD_CENTER", "Short press KEYCODE_ENTER");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_BACK, false);
        LogHelper.assertLog(getDevice(), CLASS, "Short press KEYCODE_BACK");
    }

    /**
     * Test 11.2.13-2
     * Tests that the device responds correctly to a <USER_CONTROL_PRESSED> message for press and
     * hold operations.
     */
    @Test
    public void cect_11_2_13_2_UserControlPressAndHold() throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Clear logcat.
        device.executeAdbCommand("logcat", "-c");
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND);
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_UP, true);
        LogHelper.assertLog(getDevice(), CLASS, "Long press KEYCODE_DPAD_UP");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_DOWN, true);
        LogHelper.assertLog(getDevice(), CLASS, "Long press KEYCODE_DPAD_DOWN");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_LEFT, true);
        LogHelper.assertLog(getDevice(), CLASS, "Long press KEYCODE_DPAD_LEFT");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_RIGHT, true);
        LogHelper.assertLog(getDevice(), CLASS, "Long press KEYCODE_DPAD_RIGHT");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_SELECT, true);
        LogHelper.assertLog(getDevice(), CLASS,
                "Long press KEYCODE_DPAD_CENTER", "Long press KEYCODE_ENTER");
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, LogicalAddress.PLAYBACK_1,
                HdmiCecConstants.CEC_CONTROL_BACK, true);
        LogHelper.assertLog(getDevice(), CLASS, "Long press KEYCODE_BACK");
    }
}
