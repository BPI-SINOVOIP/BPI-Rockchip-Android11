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

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import com.google.common.collect.Range;

import android.hdmicec.cts.CecMessage;
import android.hdmicec.cts.CecOperand;
import android.hdmicec.cts.HdmiCecClientWrapper;
import android.hdmicec.cts.HdmiCecConstants;
import android.hdmicec.cts.LogHelper;
import android.hdmicec.cts.LogicalAddress;
import android.hdmicec.cts.RequiredPropertyRule;
import android.hdmicec.cts.RequiredFeatureRule;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

/** HDMI CEC test to test system audio mode (Section 11.2.15) */
@Ignore("b/162820841")
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecSystemAudioModeTest extends BaseHostJUnit4Test {

    /** The package name of the APK. */
    private static final String PACKAGE = "android.hdmicec.app";

    /** The class name of the main activity in the APK. */
    private static final String CLASS = "HdmiCecAudioManager";

    /** The command to launch the main activity. */
    private static final String START_COMMAND = String.format(
            "am start -n %s/%s.%s -a ", PACKAGE, PACKAGE, CLASS);

    /** The command to clear the main activity. */
    private static final String CLEAR_COMMAND = String.format("pm clear %s", PACKAGE);

    private static final LogicalAddress AUDIO_DEVICE = LogicalAddress.AUDIO_SYSTEM;
    private static final int ON = 0x1;
    private static final int OFF = 0x0;
    private static final int MAX_AUDIO_FORMATS = 4;
    private static final int MAX_VALID_AUDIO_FORMATS = 2;

    private List<Integer> mSupportedAudioFormats = null;

    public HdmiCecClientWrapper hdmiCecClient = new HdmiCecClientWrapper(AUDIO_DEVICE, "-t", "t");

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

    private String getRequestSadFormatsParams(boolean sendValidFormats) throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Clear logcat.
        device.executeAdbCommand("logcat", "-c");
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND + "android.hdmicec.app.GET_SUPPORTED_SAD_FORMATS");
        mSupportedAudioFormats = LogHelper.getSupportedAudioFormats(getDevice());

        // Create a list of all the audio format codes according to CEA-861-D. Remove the supported
        // audio format codes from it, to get the unsupported audio format codes.
        List<Integer> mAllCodecFormats =
                IntStream.range(1, 15).boxed().collect(Collectors.toList());
        List<Integer> unsupportedAudioFormats = new ArrayList<>();
        unsupportedAudioFormats.addAll(mAllCodecFormats);
        unsupportedAudioFormats.removeAll(mSupportedAudioFormats);
        // Create params message for REQUEST_SHORT_AUDIO_DESCRIPTOR
        String messageParams = "";
        int i = 0;
        int listIndex = 0;
        if (sendValidFormats) {
            while (i < Math.min(MAX_VALID_AUDIO_FORMATS, mSupportedAudioFormats.size())) {
                messageParams +=
                        CecMessage.formatParams(mSupportedAudioFormats.get(listIndex), 2);
                i++;
                listIndex++;
            }
            listIndex = 0;
        }
        while (i < Math.min(MAX_AUDIO_FORMATS, unsupportedAudioFormats.size())) {
            messageParams += CecMessage.formatParams(unsupportedAudioFormats.get(listIndex), 2);
            i++;
            listIndex++;
        }
        return messageParams;
    }

    private void muteDevice() throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Clear logcat.
        device.executeAdbCommand("logcat", "-c");
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND + "android.hdmicec.app.MUTE");
    }

    private void unmuteDevice() throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND + "android.hdmicec.app.UNMUTE");
    }

    public boolean isDeviceMuted() throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Clear logcat.
        device.executeAdbCommand("logcat", "-c");
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND + "android.hdmicec.app.REPORT_VOLUME");
        try {
            LogHelper.assertLog(getDevice(), CLASS, "Device muted.");
            return true;
        } catch(Exception e) {
            return false;
        }
    }

    public void setDeviceVolume(int percentVolume) throws Exception {
        ITestDevice device = getDevice();
        // Clear activity
        device.executeShellCommand(CLEAR_COMMAND);
        // Start the APK and wait for it to complete.
        device.executeShellCommand(START_COMMAND + "android.hdmicec.app.SET_VOLUME --ei " +
                "\"volumePercent\" " + percentVolume);
    }

    public void sendSystemAudioModeTermination() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST);
    }

    public void sendSystemAudioModeInitiation() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                    HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
    }

    private int getDutAudioStatus() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE, CecOperand.GIVE_AUDIO_STATUS);
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
                CecOperand.REPORT_AUDIO_STATUS);
        return CecMessage.getParams(message);
    }

    private void initateSystemAudioModeFromTuner() throws Exception {
        getDevice().reboot();
        hdmiCecClient.sendCecMessage(LogicalAddress.TUNER_1, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                        HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        handleSetSystemAudioModeOnToTv();
    }

    private void handleSetSystemAudioModeOnToTv() throws Exception {
        hdmiCecClient.checkExpectedOutput(CecOperand.REQUEST_ACTIVE_SOURCE);
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.BROADCAST, CecOperand.ACTIVE_SOURCE,
                CecMessage.formatParams("2000"));
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
                CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
    }

    private void initiateSystemAudioModeFromDut() throws Exception {
        getDevice().reboot();
        hdmiCecClient.checkExpectedOutput(CecOperand.REPORT_PHYSICAL_ADDRESS);
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.AUDIO_SYSTEM,
                CecOperand.GIVE_SYSTEM_AUDIO_MODE_STATUS);
        hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.INITIATE_ARC);
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, LogicalAddress.AUDIO_SYSTEM,
                CecOperand.ARC_INITIATED);
        handleSetSystemAudioModeOnToTv();
    }

    @After
    public void resetVolume() throws Exception {
        setDeviceVolume(20);
    }

    /**
     * Test 11.2.15-1
     * Tests that the device handles <System Audio Mode Request> messages from various logical
     * addresses correctly as a follower.
     */
    @Test
    public void cect_11_2_15_1_SystemAudioModeRequestAsFollower() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                    HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);

        /* Repeat test for device 0x3 (TUNER_1) */
        hdmiCecClient.sendCecMessage(LogicalAddress.TUNER_1, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                    HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
    }

    /**
     * Test 11.2.15-2
     * Tests that the device issues <Set System Audio Mode>
     * message when the feature is initiated in the device .
     */
    @Test
    public void cect_11_2_15_2_SystemAudioModeWithFeatureInitiation() throws Exception {
        initiateSystemAudioModeFromDut();
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
    }

    /**
     * Test 11.2.15-3
     * Tests that the device doesn't broadcast any <Set System Audio Mode>
     * messages when TV responds with a <Feature Abort> to a directly addressed
     * <Set System Audio Mode> message.
     */
    @Test
    public void cect_11_2_15_3_SystemAudioModeWithFeatureAbort() throws Exception {
        initiateSystemAudioModeFromDut();
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE, CecOperand.FEATURE_ABORT,
                CecMessage.formatParams(CecOperand.SET_SYSTEM_AUDIO_MODE + "04"));
        hdmiCecClient.checkOutputDoesNotContainMessage(LogicalAddress.BROADCAST,
                CecOperand.SET_SYSTEM_AUDIO_MODE);
        //The DUT will need a reboot here so it'll forget the feature abort from the previous
        // message. Else it may not respond correctly with a SET_SYSTEM_AUDIO_MODE message
        // in future tests.
        getDevice().reboot();
    }

    /**
     * Test 11.2.15-4
     * Tests that the device responds correctly to a <Give System Audio Status>
     * message when System Audio Mode is "On".
     */
    @Test
    public void cect_11_2_15_4_SystemAudioModeStatusOn() throws Exception {
        sendSystemAudioModeInitiation();
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.GIVE_SYSTEM_AUDIO_MODE_STATUS);
        message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
                CecOperand.SYSTEM_AUDIO_MODE_STATUS);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
    }

    /**
     * Test 11.2.15-5
     * Tests that the device sends a <Set System Audio Mode> ["Off"]
     * message when a <System Audio Mode Request> is received with no operands
     */
    @Test
    public void cect_11_2_15_5_SetSystemAudioModeOff() throws Exception {
        sendSystemAudioModeInitiation();
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
        sendSystemAudioModeTermination();
        message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(OFF);
    }

    /**
     * Test 11.2.15-6
     * Tests that the device sends a <Set System Audio Mode> ["Off"]
     * message before going into standby when System Audio Mode is on.
     */
    @Test
    public void cect_11_2_15_6_SystemAudioModeOffBeforeStandby() throws Exception {
        try {
            getDevice().executeShellCommand("input keyevent KEYCODE_WAKEUP");
            sendSystemAudioModeInitiation();
            String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
            assertThat(CecMessage.getParams(message)).isEqualTo(ON);
            getDevice().executeShellCommand("input keyevent KEYCODE_SLEEP");
            message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
            assertThat(CecMessage.getParams(message)).isEqualTo(OFF);
        } finally {
            getDevice().executeShellCommand("input keyevent KEYCODE_WAKEUP");
        }
    }

    /**
    * Test 11.2.15-7
    * Tests that the device responds correctly to a <Give System Audio Mode Status>
    * message when the System Audio Mode is "Off".
    */
   @Test
    public void cect_11_2_15_7_SystemAudioModeStatusOff() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SET_SYSTEM_AUDIO_MODE, CecMessage.formatParams(OFF));
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.GIVE_SYSTEM_AUDIO_MODE_STATUS);
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
                CecOperand.SYSTEM_AUDIO_MODE_STATUS);
        assertThat(CecMessage.getParams(message)).isEqualTo(OFF);
    }

    /**
     * Test 11.2.15-8
     * Tests that the device handles <User Controlled Pressed> ["Mute"]
     * correctly when System Audio Mode is "On".
     */
    @Test
    public void cect_11_2_15_8_HandleUcpMute() throws Exception {
        unmuteDevice();
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                    HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        hdmiCecClient.sendUserControlPressAndRelease(LogicalAddress.TV, AUDIO_DEVICE,
                HdmiCecConstants.CEC_CONTROL_MUTE, false);
        assertWithMessage("Device is not muted").that(isDeviceMuted()).isTrue();
    }

    /**
     * Test 11.2.15-9
     * Tests that the DUT responds with a <Report Audio Status> message with correct parameters
     * to a <Give Audio Status> message when volume is set to 0% and not muted.
     */
    @Test
    public void cect_11_2_15_9_ReportAudioStatus_0_unmuted() throws Exception {
        sendSystemAudioModeInitiation();
        unmuteDevice();
        setDeviceVolume(0);
        int reportedVolume = getDutAudioStatus();
        assertThat(reportedVolume).isEqualTo(0);
    }

    /**
     * Test 11.2.15-9
     * Tests that the DUT responds with a <Report Audio Status> message with correct parameters
     * to a <Give Audio Status> message when volume is set to 50% and not muted.
     */
    @Test
    public void cect_11_2_15_9_ReportAudioStatus_50_unmuted() throws Exception {
        sendSystemAudioModeInitiation();
        unmuteDevice();
        setDeviceVolume(50);
        int reportedVolume = getDutAudioStatus();
        /* Allow for a range of volume, since the actual volume set will depend on the device's
        volume resolution. */
        assertThat(reportedVolume).isIn(Range.closed(46, 54));
    }

    /**
     * Test 11.2.15-9
     * Tests that the DUT responds with a <Report Audio Status> message with correct parameters
     * to a <Give Audio Status> message when volume is set to 100% and not muted.
     */
    @Test
    public void cect_11_2_15_9_ReportAudioStatus_100_unmuted() throws Exception {
        sendSystemAudioModeInitiation();
        unmuteDevice();
        setDeviceVolume(100);
        int reportedVolume = getDutAudioStatus();
        assertThat(reportedVolume).isEqualTo(100);
    }

    /**
     * Test 11.2.15-9
     * Tests that the DUT responds with a <Report Audio Status> message with correct parameters
     * to a <Give Audio Status> message when volume is muted.
     */
    @Test
    public void cect_11_2_15_9_ReportAudioStatusMuted() throws Exception {
        sendSystemAudioModeInitiation();
        muteDevice();
        int reportedVolume = getDutAudioStatus();
        /* If device is muted, the 8th bit of CEC message parameters is set and the volume will
        be greater than 127. */
        assertWithMessage("Device not muted").that(reportedVolume).isGreaterThan(127);
    }

    /**
     * Test 11.2.15-13
     * Tests that the device responds to a <Request Short Audio Descriptor> message with a
     * <Report Short Audio Descriptor> message with only those audio descriptors that are supported.
     */
    @Test
    public void cect_11_2_15_13_ValidShortAudioDescriptor() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.REQUEST_SHORT_AUDIO_DESCRIPTOR, getRequestSadFormatsParams(true));
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV,
                CecOperand.REPORT_SHORT_AUDIO_DESCRIPTOR);
        /* Each Short Audio Descriptor is 3 bytes long. In the first byte of the params, bits 3-6
         * will have the audio format. Bit 7 will always 0 for audio format defined in CEA-861-D.
         * Bits 0-2 represent (Max number of channels - 1). Discard bits 0-2 and check for the
         * format.
         * Iterate the params by 3 bytes(6 nibbles) and extract only the first byte(2 nibbles).
         */
        for (int i = 0; i < Math.min(mSupportedAudioFormats.size(), MAX_VALID_AUDIO_FORMATS); i++) {
            int audioFormat =
                    CecMessage.getParams(message, 6 * i, 6 * i + 2) >>> 3;
            assertWithMessage("Could not find audio format " + audioFormat)
                    .that(mSupportedAudioFormats).contains(audioFormat);
        }
    }

    /**
     * Test 11.2.15-14
     * Tests that the device responds to a <Request Short Audio Descriptor> message with a
     * <Feature Abort> [“Invalid Operand”] when a <Request Short Audio Descriptor> message is
     * received with a single [Audio Format ID][Audio Format Code] pair that is not supported.
     */
    @Test
    public void cect_11_2_15_14_InvalidShortAudioDescriptor() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.REQUEST_SHORT_AUDIO_DESCRIPTOR, getRequestSadFormatsParams(false));
        String message = hdmiCecClient.checkExpectedOutput(LogicalAddress.TV, CecOperand.FEATURE_ABORT);
        assertThat(CecOperand.getOperand(CecMessage.getParams(message, 2)))
                .isEqualTo(CecOperand.REQUEST_SHORT_AUDIO_DESCRIPTOR);
        assertThat(CecMessage.getParams(message, 2, 4))
                .isEqualTo(HdmiCecConstants.ABORT_INVALID_OPERAND);
    }

    /**
     * Test 11.2.15-16
     * Tests that the device unmute its volume when it broadcasts a
     * <Set System Audio Mode> ["On"] message
     */
    @Test
    public void cect_11_2_15_16_UnmuteForSystemAudioRequestOn() throws Exception {
        muteDevice();
        sendSystemAudioModeTermination();
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(OFF);
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                    HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
        assertWithMessage("Device muted").that(isDeviceMuted()).isFalse();
    }

    /**
     * Test 11.2.15-17
     * Tests that the device mute its volume when it broadcasts a
     * <Set System Audio Mode> ["Off"] message
     */
    @Test
    public void cect_11_2_15_17_MuteForSystemAudioRequestOff() throws Exception {
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE,
                CecOperand.SYSTEM_AUDIO_MODE_REQUEST,
                CecMessage.formatParams(HdmiCecConstants.TV_PHYSICAL_ADDRESS,
                    HdmiCecConstants.PHYSICAL_ADDRESS_LENGTH));
        String message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(ON);
        sendSystemAudioModeTermination();
        message = hdmiCecClient.checkExpectedOutput(CecOperand.SET_SYSTEM_AUDIO_MODE);
        assertThat(CecMessage.getParams(message)).isEqualTo(OFF);
        assertWithMessage("Device not muted").that(isDeviceMuted()).isTrue();
    }

    /**
     * Test 11.2.15-18
     * Tests that the device doesn't broadcast <Set System Audio Mode>
     * messages if TV responds with a <Feature Abort> message to directly addressed
     * <Set System Audio Mode> message within the required maximum response time of 1 second.
     */
    @Test
    public void cect_11_2_15_18_SystemAudioModeWithFeatureAbortWithinTime() throws Exception {
        initiateSystemAudioModeFromDut();
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE, CecOperand.FEATURE_ABORT,
                CecMessage.formatParams(CecOperand.SET_SYSTEM_AUDIO_MODE + "04"));
        hdmiCecClient.checkOutputDoesNotContainMessage(LogicalAddress.BROADCAST,
                CecOperand.SET_SYSTEM_AUDIO_MODE);
    }

    /**
     * Test 11.2.15-19
     * Tests that the device doesn't broadcast <Set System Audio Mode>
     * messages if TV responds with a <Feature Abort> message to directly addressed
     * <Set System Audio Mode> message within the required maximum response time of 1 second.
     * The <Set System Audio Mode> message should be initiated from logical address 3.
     */
    @Test
    public void cect_11_2_15_19_SystemAudioModeWithFeatureAbortWithinTime() throws Exception {
        initateSystemAudioModeFromTuner();
        hdmiCecClient.sendCecMessage(LogicalAddress.TV, AUDIO_DEVICE, CecOperand.FEATURE_ABORT,
                CecMessage.formatParams(CecOperand.SET_SYSTEM_AUDIO_MODE + "04"));
        hdmiCecClient.checkOutputDoesNotContainMessage(LogicalAddress.BROADCAST,
            CecOperand.SET_SYSTEM_AUDIO_MODE);
    }
}
