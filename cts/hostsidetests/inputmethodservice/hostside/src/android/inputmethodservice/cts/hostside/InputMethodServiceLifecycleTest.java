/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.inputmethodservice.cts.common.BusyWaitUtils.pollingCheck;
import static android.inputmethodservice.cts.common.DeviceEventConstants.ACTION_DEVICE_EVENT;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.TEST_START;
import static android.inputmethodservice.cts.common.DeviceEventConstants.EXTRA_EVENT_SENDER;
import static android.inputmethodservice.cts.common.DeviceEventConstants.EXTRA_EVENT_TYPE;
import static android.inputmethodservice.cts.common.DeviceEventConstants.RECEIVER_COMPONENT;

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.inputmethodservice.cts.common.EditTextAppConstants;
import android.inputmethodservice.cts.common.EventProviderConstants.EventTableConstants;
import android.inputmethodservice.cts.common.Ime1Constants;
import android.inputmethodservice.cts.common.Ime2Constants;
import android.inputmethodservice.cts.common.test.DeviceTestConstants;
import android.inputmethodservice.cts.common.test.ShellCommandUtils;
import android.inputmethodservice.cts.common.test.TestInfo;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test general lifecycle events around InputMethodService.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class InputMethodServiceLifecycleTest extends BaseHostJUnit4Test {

    private static final long WAIT_TIMEOUT = TimeUnit.SECONDS.toMillis(1);
    private static final long PACKAGE_OP_TIMEOUT = TimeUnit.SECONDS.toMillis(7);
    private static final long POLLING_INTERVAL = 100;

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
     * Set up test case.
     */
    @Before
    public void setUp() throws Exception {
        // Skip whole tests when DUT has no android.software.input_methods feature.
        assumeTrue(hasDeviceFeature(ShellCommandUtils.FEATURE_INPUT_METHODS));
        mNeedsTearDown = true;

        cleanUpTestImes();
        installPackage(DeviceTestConstants.APK, "-r");
        shell(ShellCommandUtils.deleteContent(EventTableConstants.CONTENT_URI));
    }

    /**
     * Tear down test case.
     */
    @After
    public void tearDown() throws Exception {
        if (!mNeedsTearDown) {
            return;
        }
        shell(ShellCommandUtils.resetImes());
    }

    /**
     * Install an app apk file synchronously.
     *
     * <p>This methods waits until package is available in PackageManger</p>
     *
     * <p>Note: For installing IME APKs use {@link #installImePackageSync(String, String)}
     * instead.</p>
     * @param apkFileName App apk to install
     * @param packageName packageName of the installed apk
     * @param options adb shell install options.
     * @throws Exception
     */
    private void installPackageSync(
            String apkFileName, String packageName, String... options) throws Exception {
        installPackage(apkFileName, options);
        pollingCheck(() ->
                        shell(ShellCommandUtils.listPackage(packageName)).contains(packageName),
                PACKAGE_OP_TIMEOUT,
                packageName + " should be installed.");
    }

    /**
     * Install IME packages synchronously.
     *
     * <p>This method verifies that IME is available in IMMS.</p>
     * @param apkFileName IME apk to install
     * @param imeId of the IME being installed.
     * @throws Exception
     */
    private void installImePackageSync(String apkFileName, String imeId) throws Exception {
        installPackage(apkFileName, "-r");
        waitUntilImesAreAvailable(imeId);
    }

    private void installPossibleInstantPackage(
            String apkFileName, String packageName, boolean instant) throws Exception {
        if (instant) {
            installPackageSync(apkFileName, packageName, "-r", "--instant");
        } else {
            installPackageSync(apkFileName, packageName, "-r");
        }
    }

    private void testSwitchIme(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_SWITCH_IME1_TO_IME2);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        installImePackageSync(Ime2Constants.APK, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_SWITCH_IME1_TO_IME2));
    }

    /**
     * Test IME switching APIs for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testSwitchImeFull() throws Exception {
        testSwitchIme(false);
    }

    /**
     * Test IME switching APIs for instant apps.
     */
    @AppModeInstant
    @Test
    public void testSwitchImeInstant() throws Exception {
        testSwitchIme(true);
    }

    private void testUninstallCurrentIme(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_CREATE_IME1);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID);

        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));
        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_CREATE_IME1));

        uninstallPackageSyncIfExists(Ime1Constants.PACKAGE);
        assertImeNotSelectedInSecureSettings(Ime1Constants.IME_ID, WAIT_TIMEOUT);
    }

    /**
     * Test uninstalling the currently selected IME for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testUninstallCurrentImeFull() throws Exception {
        testUninstallCurrentIme(false);
    }

    /**
     * Test uninstalling the currently selected IME for instant apps.
     */
    @AppModeInstant
    @Test
    public void testUninstallCurrentImeInstant() throws Exception {
        testUninstallCurrentIme(true);
    }

    private void testDisableCurrentIme(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_CREATE_IME1);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));
        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_CREATE_IME1));

        shell(ShellCommandUtils.disableIme(Ime1Constants.IME_ID));
        assertImeNotSelectedInSecureSettings(Ime1Constants.IME_ID, WAIT_TIMEOUT);
    }

    /**
     * Test disabling the currently selected IME for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testDisableCurrentImeFull() throws Exception {
        testDisableCurrentIme(false);
    }

    /**
     * Test disabling the currently selected IME for instant apps.
     */
    @AppModeInstant
    @Test
    public void testDisableCurrentImeInstant() throws Exception {
        testDisableCurrentIme(true);
    }

    private void testSwitchInputMethod(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_SWITCH_INPUTMETHOD);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        installImePackageSync(Ime2Constants.APK, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_SWITCH_INPUTMETHOD));
    }

    /**
     * Test "InputMethodService#switchInputMethod" API for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testSwitchInputMethodFull() throws Exception {
        testSwitchInputMethod(false);
    }

    /**
     * Test "InputMethodService#switchInputMethod" API for instant apps.
     */
    @AppModeInstant
    @Test
    public void testSwitchInputMethodInstant() throws Exception {
        testSwitchInputMethod(true);
    }

    private void testSwitchToNextInput(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_SWITCH_NEXT_INPUT);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        installImePackageSync(Ime2Constants.APK, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        // Make sure that there is at least one more IME that specifies
        // supportsSwitchingToNextInputMethod="true"
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_SWITCH_NEXT_INPUT));
    }

    /**
     * Test "InputMethodService#switchToNextInputMethod" API for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testSwitchToNextInputFull() throws Exception {
        testSwitchToNextInput(false);
    }

    /**
     * Test "InputMethodService#switchToNextInputMethod" API for instant apps.
     */
    @AppModeInstant
    @Test
    public void testSwitchToNextInputInstant() throws Exception {
        testSwitchToNextInput(true);
    }

    private void testSwitchToPreviousInput(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_SWITCH_PREVIOUS_INPUT);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        installImePackageSync(Ime2Constants.APK, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_SWITCH_PREVIOUS_INPUT));
    }

    /**
     * Test "InputMethodService#switchToPreviousInputMethod" API for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testSwitchToPreviousInputFull() throws Exception {
        testSwitchToPreviousInput(false);
    }

    /**
     * Test "InputMethodService#switchToPreviousInputMethod" API for instant apps.
     */
    @AppModeInstant
    @Test
    public void testSwitchToPreviousInputInstant() throws Exception {
        testSwitchToPreviousInput(true);
    }

    private void testInputUnbindsOnImeStopped(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_INPUT_UNBINDS_ON_IME_STOPPED);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        installImePackageSync(Ime2Constants.APK, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_INPUT_UNBINDS_ON_IME_STOPPED));
    }

    /**
     * Test if uninstalling the currently selected IME then selecting another IME triggers standard
     * startInput/bindInput sequence for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testInputUnbindsOnImeStoppedFull() throws Exception {
        testInputUnbindsOnImeStopped(false);
    }

    /**
     * Test if uninstalling the currently selected IME then selecting another IME triggers standard
     * startInput/bindInput sequence for instant apps.
     */
    @AppModeInstant
    @Test
    public void testInputUnbindsOnImeStoppedInstant() throws Exception {
        testInputUnbindsOnImeStopped(true);
    }

    private void testInputUnbindsOnAppStop(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_INPUT_UNBINDS_ON_APP_STOPPED);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(DeviceTestConstants.TEST_INPUT_UNBINDS_ON_APP_STOPPED));
    }

    /**
     * Test if uninstalling the currently running IME client triggers
     * "InputMethodService#onUnbindInput" callback for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testInputUnbindsOnAppStopFull() throws Exception {
        testInputUnbindsOnAppStop(false);
    }

    /**
     * Test if uninstalling the currently running IME client triggers
     * "InputMethodService#onUnbindInput" callback for instant apps.
     */
    @AppModeInstant
    @Test
    public void testInputUnbindsOnAppStopInstant() throws Exception {
        testInputUnbindsOnAppStop(true);
    }

    private void testImeVisibilityAfterImeSwitching(boolean instant) throws Exception {
        sendTestStartEvent(DeviceTestConstants.TEST_SWITCH_IME1_TO_IME2);
        installPossibleInstantPackage(
                EditTextAppConstants.APK, EditTextAppConstants.PACKAGE, instant);
        installImePackageSync(Ime1Constants.APK, Ime1Constants.IME_ID);
        installImePackageSync(Ime2Constants.APK, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID));
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID));
        waitUntilImesAreEnabled(Ime1Constants.IME_ID, Ime2Constants.IME_ID);
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID));

        assertTrue(runDeviceTestMethod(
                DeviceTestConstants.TEST_IME_VISIBILITY_AFTER_IME_SWITCHING));
    }

    /**
     * Test if IMEs remain to be visible after switching to other IMEs for full (non-instant) apps.
     *
     * <p>Regression test for Bug 152876819.</p>
     */
    @AppModeFull
    @Test
    public void testImeVisibilityAfterImeSwitchingFull() throws Exception {
        testImeVisibilityAfterImeSwitching(false);
    }

    /**
     * Test if IMEs remain to be visible after switching to other IMEs for instant apps.
     *
     * <p>Regression test for Bug 152876819.</p>
     */
    @AppModeInstant
    @Test
    public void testImeVisibilityAfterImeSwitchingInstant() throws Exception {
        testImeVisibilityAfterImeSwitching(true);
    }

    private void sendTestStartEvent(TestInfo deviceTest) throws Exception {
        final String sender = deviceTest.getTestName();
        // {@link EventType#EXTRA_EVENT_TIME} will be recorded at device side.
        shell(ShellCommandUtils.broadcastIntent(
                ACTION_DEVICE_EVENT, RECEIVER_COMPONENT,
                "--es", EXTRA_EVENT_SENDER, sender,
                "--es", EXTRA_EVENT_TYPE, TEST_START.name()));
    }

    private boolean runDeviceTestMethod(TestInfo deviceTest) throws Exception {
        return runDeviceTests(deviceTest.testPackage, deviceTest.testClass, deviceTest.testMethod);
    }

    private String shell(String command) throws Exception {
        return getDevice().executeShellCommand(command).trim();
    }

    private void cleanUpTestImes() throws Exception {
        uninstallPackageSyncIfExists(Ime1Constants.PACKAGE);
        uninstallPackageSyncIfExists(Ime2Constants.PACKAGE);
    }

    private void uninstallPackageSyncIfExists(String packageName) throws Exception {
        if (isPackageInstalled(getDevice(), packageName)) {
            uninstallPackage(getDevice(), packageName);
            pollingCheck(() -> !isPackageInstalled(getDevice(), packageName), PACKAGE_OP_TIMEOUT,
                    packageName + " should be uninstalled.");
        }
    }

    /**
     * Makes sure that the given IME is not in the stored in the secure settings as the current IME.
     *
     * @param imeId IME ID to be monitored
     * @param timeout timeout in millisecond
     */
    private void assertImeNotSelectedInSecureSettings(String imeId, long timeout) throws Exception {
        while (true) {
            if (timeout < 0) {
                throw new TimeoutException(imeId + " is still the current IME even after "
                        + timeout + " msec.");
            }
            if (!imeId.equals(shell(ShellCommandUtils.getCurrentIme()))) {
                break;
            }
            Thread.sleep(POLLING_INTERVAL);
            timeout -= POLLING_INTERVAL;
        }
    }

    /**
     * Wait until IMEs are available in IMMS.
     * @throws Exception
     */
    private void waitUntilImesAreAvailable(String... imeIds) throws Exception {
        waitUntilImesAreAvailableOrEnabled(false, imeIds);
    }

    /**
     * Wait until IMEs are enabled in IMMS.
     * @throws Exception
     */
    private void waitUntilImesAreEnabled(String... imeIds) throws Exception {
        waitUntilImesAreAvailableOrEnabled(true, imeIds);
    }

    private void waitUntilImesAreAvailableOrEnabled(
            boolean shouldBeEnabled, String... imeIds) throws Exception {
        final String cmd = shouldBeEnabled
                ? ShellCommandUtils.getEnabledImes() : ShellCommandUtils.getAvailableImes();
        for (String imeId : imeIds) {
            pollingCheck(() -> shell(cmd).contains(imeId), PACKAGE_OP_TIMEOUT,
                    imeId + " should be " + (shouldBeEnabled ? "enabled." : "available."));
        }
    }
}
