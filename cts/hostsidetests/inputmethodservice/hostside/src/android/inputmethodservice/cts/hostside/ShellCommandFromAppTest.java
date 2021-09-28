/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.inputmethodservice.cts.hostside;

import static org.junit.Assume.assumeTrue;

import android.inputmethodservice.cts.common.test.DeviceTestConstants;
import android.inputmethodservice.cts.common.test.ShellCommandUtils;
import android.inputmethodservice.cts.common.test.TestInfo;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test IInputMethodManager#shellComman verifies callers.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ShellCommandFromAppTest extends BaseHostJUnit4Test {

    /**
     * {@code true} if {@link #tearDown()} needs to be fully executed.
     *
     * <p>When {@link #setUp()} is interrupted by {@link org.junit.AssumptionViolatedException}
     * before the actual setup tasks are executed, all the corresponding cleanup tasks should also
     * be skipped.</p>
     *
     * <p>Once JUnit 5 becomes available in Android, we can remove this by moving the assumption
     * checks into a non-static {@link org.junit.BeforeClass} method.</p>
     */
    private boolean mNeedsTearDown = false;

    /**
     * Run device test with disabling hidden API check.
     *
     * @param testInfo test to be executed.
     * @param instant {@code true} when {@code testInfo} needs to be installed as an instant app.
     */
    private void runDeviceTestMethodWithoutHiddenApiCheck(TestInfo testInfo, boolean instant)
            throws Exception {
        if (instant) {
            installPackage(DeviceTestConstants.APK, "-r", "--instant");
        } else {
            installPackage(DeviceTestConstants.APK, "-r");
        }
        runDeviceTests(new DeviceTestRunOptions(testInfo.testPackage)
                .setDevice(getDevice())
                .setDisableHiddenApiCheck(false)
                .setTestClassName(testInfo.testClass)
                .setTestMethodName(testInfo.testMethod));
    }

    /**
     * Set up test case.
     */
    @Before
    public void setUp() throws Exception {
        // Skip whole tests when DUT has no android.software.input_methods feature.
        assumeTrue(hasDeviceFeature(ShellCommandUtils.FEATURE_INPUT_METHODS));
        mNeedsTearDown = true;
    }

    /**
     * Tear down test case.
     */
    @After
    public void tearDown() throws Exception {
        if (!mNeedsTearDown) {
            return;
        }
        // Tear down logic must be placed here.
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{}, null, receiver)}
     * returns {@link SecurityException} for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testShellCommandFull() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND, false);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{}, null, receiver)}
     * returns {@link SecurityException} for instant apps.
     */
    @AppModeInstant
    @Test
    public void testShellCommandInstant() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND, true);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime"}, null, receiver)}
     * returns {@link SecurityException} for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testShellCommandImeFull() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_IME, false);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime"}, null, receiver)}
     * returns {@link SecurityException} for instant apps.
     */
    @AppModeInstant
    @Test
    public void testShellCommandImeInstant() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_IME, true);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException} for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testShellCommandImeListFull() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_IME_LIST,
                false);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException} for instant apps.
     */
    @AppModeInstant
    @Test
    public void testShellCommandImeListInstant() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_IME_LIST,
                true);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException} for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testShellCommandDumpFull() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_DUMP,
                false);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException} for instant apps.
     */
    @AppModeInstant
    @Test
    public void testShellCommandDumpInstant() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_DUMP, true);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException} for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testShellCommandHelpFull() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_HELP,
                false);
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException} for instant apps.
     */
    @AppModeInstant
    @Test
    public void testShellCommandHelpInstant() throws Exception {
        runDeviceTestMethodWithoutHiddenApiCheck(DeviceTestConstants.TEST_SHELL_COMMAND_HELP, true);
    }
}
