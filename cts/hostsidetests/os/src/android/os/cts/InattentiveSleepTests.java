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

package android.os.cts;

import static android.os.PowerManagerInternalProto.Wakefulness.WAKEFULNESS_ASLEEP;

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.os.PowerManagerInternalProto.Wakefulness;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.ProtoUtils;
import com.android.compatibility.common.util.WindowManagerUtil;
import com.android.server.power.PowerManagerServiceDumpProto;
import com.android.server.power.PowerServiceSettingsAndConfigurationDumpProto;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class InattentiveSleepTests extends BaseHostJUnit4Test {
    private static final String FEATURE_LEANBACK_ONLY = "android.software.leanback_only";
    private static final String PACKAGE_NAME = "android.os.inattentivesleeptests";
    private static final String APK_NAME = "CtsInattentiveSleepTestApp.apk";

    private static final long TIME_BEFORE_WARNING_MS = 1200L;

    private static final String CMD_DUMPSYS_POWER = "dumpsys power --proto";
    private static final String WARNING_WINDOW_TOKEN_TITLE = "InattentiveSleepWarning";
    private static final String CMD_START_APP_TEMPLATE =
            "am start -W -a android.intent.action.MAIN -p %s -c android.intent.category.LAUNCHER";

    private static final String CMD_GET_STAY_ON = "settings get global stay_on_while_plugged_in";
    private static final String CMD_PUT_STAY_ON_TEMPLATE =
            "settings put global stay_on_while_plugged_in %d";
    private static final String CMD_DISABLE_STAY_ON =
            "settings put global stay_on_while_plugged_in 0";
    private static final String CMD_ENABLE_STAY_ON =
            "settings put global stay_on_while_plugged_in 7";

    private static final String CMD_SET_TIMEOUT_TEMPLATE =
            "settings put secure attentive_timeout %d";
    private static final String CMD_DELETE_TIMEOUT_SETTING =
            "settings delete secure attentive_timeout";

    private static final String CMD_KEYEVENT_HOME = "input keyevent KEYCODE_HOME";
    private static final String CMD_KEYEVENT_WAKEUP = "input keyevent KEYCODE_WAKEUP";
    private static final String CMD_KEYEVENT_DPAD_CENTER = "input keyevent KEYCODE_DPAD_CENTER";

    // A reference to the device under test, which gives us a handle to run commands.
    private ITestDevice mDevice;

    private long mOriginalStayOnSetting;
    private long mWarningDurationConfig;

    @Before
    public synchronized void setUp() throws Exception {
        mDevice = getDevice();
        assumeTrue("Test only applicable to TVs.", hasDeviceFeature(FEATURE_LEANBACK_ONLY));

        mWarningDurationConfig = getWarningDurationConfig();
        mOriginalStayOnSetting = Long.parseLong(
                mDevice.executeShellCommand(CMD_GET_STAY_ON).trim());
        mDevice.executeShellCommand(CMD_DISABLE_STAY_ON);
        setInattentiveSleepTimeout(TIME_BEFORE_WARNING_MS + mWarningDurationConfig);
    }

    @After
    public void tearDown() throws Exception {
        mDevice.executeShellCommand(
                String.format(CMD_PUT_STAY_ON_TEMPLATE, mOriginalStayOnSetting));
        mDevice.executeShellCommand(CMD_DELETE_TIMEOUT_SETTING);
    }

    private void wakeUpToHome() throws Exception {
        wakeUp();
        mDevice.executeShellCommand(CMD_KEYEVENT_HOME);
    }

    private void wakeUp() throws Exception {
        mDevice.executeShellCommand(CMD_KEYEVENT_WAKEUP);
    }

    private void startKeepScreenOnActivity() throws Exception {
        installPackage(APK_NAME);
        mDevice.executeShellCommand(String.format(CMD_START_APP_TEMPLATE, PACKAGE_NAME));
    }

    private void setInattentiveSleepTimeout(long timeoutMs) throws Exception {
        mDevice.executeShellCommand(String.format(CMD_SET_TIMEOUT_TEMPLATE, timeoutMs));
    }

    @Test
    public void testInattentiveSleep_noWarningIfStayOnIsEnabled() throws Exception {
        assumeTrue("Device is not powered.", getPowerManagerDump().getIsPowered());
        mDevice.executeShellCommand(CMD_ENABLE_STAY_ON);

        wakeUpToHome();
        Thread.sleep(TIME_BEFORE_WARNING_MS);
        assertWarningShown(
                "Warning was shown, although the stay-on developer option was enabled.", false);
    }

    @Test
    public void testInattentiveSleep_warningShowsBeforeSleep() throws Exception {
        wakeUpToHome();
        Thread.sleep(TIME_BEFORE_WARNING_MS);
        assertWarningShown(
                "Warning was not shown before the attentive sleep timeout expired.", true);
    }

    @Test
    public void testInattentiveSleep_keypressDismissesWarning() throws Exception {
        wakeUpToHome();
        waitUntilWarningIsShowing();
        mDevice.executeShellCommand(CMD_KEYEVENT_DPAD_CENTER);
        assertWarningShown("Warning was shown after a keypress.", false);
    }

    @Test
    public void testInattentiveSleep_warningHiddenAfterWakingUp() throws Exception {
        wakeUpToHome();
        waitUntilAsleep();
        wakeUp();
        assertWarningShown("Warning was shown after waking up.", false);
    }

    @Test
    public void testInattentiveSleep_noWarningShownIfInattentiveSleepDisabled() throws Exception {
        setInattentiveSleepTimeout(-1);
        wakeUpToHome();
        Thread.sleep(TIME_BEFORE_WARNING_MS);
        assertWarningShown(
                "Warning was shown, even though the attentive sleep timeout is disabled.", false);
    }

    @Test
    public void testInattentiveSleep_goesToSleepAfterTimeout() throws Exception {
        long minScreenOffTimeout =
                getPowerManagerDump().getSettingsAndConfiguration().getMinimumScreenOffTimeoutConfigMs();
        setInattentiveSleepTimeout(minScreenOffTimeout);
        wakeUpToHome();
        Thread.sleep(minScreenOffTimeout);
        PollingCheck.check("Expected device to be asleep after timeout", TIME_BEFORE_WARNING_MS,
                () -> getWakefulness() == WAKEFULNESS_ASLEEP);
    }

    @Test
    public void testInattentiveSleep_goesToSleepAfterTimeoutWithWakeLock() throws Exception {
        long minScreenOffTimeout =
                getPowerManagerDump().getSettingsAndConfiguration().getMinimumScreenOffTimeoutConfigMs();
        setInattentiveSleepTimeout(minScreenOffTimeout);
        wakeUpToHome();
        startKeepScreenOnActivity();
        Thread.sleep(minScreenOffTimeout);
        PollingCheck.check("Expected device to be asleep after timeout", 1000,
                () -> getWakefulness() == WAKEFULNESS_ASLEEP);
    }

    @Test
    public void testInattentiveSleep_showsSleepWarningWithWakeLock() throws Exception {
        wakeUpToHome();
        startKeepScreenOnActivity();
        Thread.sleep(TIME_BEFORE_WARNING_MS);
        assertWarningShown(
                "Warning was not shown before the attentive sleep timeout expired.", true);
    }

    @Test
    public void testInattentiveSleep_warningTiming() throws Exception {
        setInattentiveSleepTimeout(5000 + mWarningDurationConfig);

        long eps = 1000;
        wakeUpToHome();

        long start = System.currentTimeMillis();
        waitUntilWarningIsNotShowing(1000);
        waitUntilWarningIsShowing(5000);
        long warningShown = System.currentTimeMillis();

        long actualTimeToWarningShown = warningShown - start;
        assertTrue("Warning was shown at unexpected time, after " + actualTimeToWarningShown + "ms",
                Math.abs(actualTimeToWarningShown - 5000) <= eps);

        long sleepTime = warningShown + mWarningDurationConfig - eps;
        while (System.currentTimeMillis() < sleepTime) {
            assertTrue("Warning dismissed early", isWarningShown());
            Thread.sleep(50);
        }
    }

    private Wakefulness getWakefulness() throws Exception {
        return getPowerManagerDump().getWakefulness();
    }

    private PowerManagerServiceDumpProto getPowerManagerDump() throws Exception {
        return ProtoUtils.getProto(getDevice(), PowerManagerServiceDumpProto.parser(),
                CMD_DUMPSYS_POWER);
    }

    private long getWarningDurationConfig() throws Exception {
        PowerServiceSettingsAndConfigurationDumpProto settingsAndConfiguration =
                getPowerManagerDump().getSettingsAndConfiguration();
        return settingsAndConfiguration.getAttentiveWarningDurationConfigMs();
    }

    private void assertWarningShown(String message, boolean expected) throws Exception {
        PollingCheck.check(message, TIME_BEFORE_WARNING_MS + 500,
                () -> isWarningShown() == expected);
    }

    private boolean isWarningShown() throws Exception {
        return WindowManagerUtil.hasWindowWithTitle(getDevice(), WARNING_WINDOW_TOKEN_TITLE);
    }

    private void waitUntilAsleep() throws Exception {
        PollingCheck.waitFor(TIME_BEFORE_WARNING_MS + mWarningDurationConfig + 2000,
                () -> getWakefulness() == WAKEFULNESS_ASLEEP);
    }

    private void waitUntilWarningIsShowing() throws Exception {
        waitUntilWarningIsShowing(TIME_BEFORE_WARNING_MS + 1000);
    }

    private void waitUntilWarningIsShowing(long timeout) throws Exception {
        PollingCheck.waitFor(timeout, this::isWarningShown);
    }

    private void waitUntilWarningIsNotShowing(long timeout) throws Exception {
        PollingCheck.waitFor(timeout, () -> !isWarningShown());
    }
}
