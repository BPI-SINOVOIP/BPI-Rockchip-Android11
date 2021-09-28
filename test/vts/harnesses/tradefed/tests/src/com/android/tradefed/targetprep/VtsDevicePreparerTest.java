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

package com.android.tradefed.targetprep;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.contains;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.targetprep.VtsDevicePreparer.DeviceOptionState;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link VtsDevicePreparer}.</p>
 */
@RunWith(JUnit4.class)
public class VtsDevicePreparerTest {
    VtsDevicePreparer mPreparer;
    @Mock private ITestDevice mockDevice;
    @Mock private IBuildInfo mockBuildInfo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPreparer = new VtsDevicePreparer();
        mPreparer.mDevice = mockDevice;
    }

    /**
     * Tests the functionality of adbRoot
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_adbUnroot_notAlreadyRoot() throws DeviceNotAvailableException {
        doReturn(false).when(mockDevice).isAdbRoot();
        mPreparer.adbUnroot();
        verify(mockDevice, times(0)).executeAdbCommand(ArgumentMatchers.anyString());
    }

    /**
     * Tests the functionality of adbRoot
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_adbUnroot_alreadyRoot() throws DeviceNotAvailableException {
        doReturn(true).when(mockDevice).isAdbRoot();
        mPreparer.adbUnroot();
        verify(mockDevice, times(1)).executeAdbCommand(eq("unroot"));
    }

    /**
     * Tests the functionality of adbRoot
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_adbRoot_notAlreadyRoot() throws DeviceNotAvailableException {
        doReturn(false).when(mockDevice).isAdbRoot();
        mPreparer.adbRoot();
        verify(mockDevice, times(1)).executeAdbCommand(eq("root"));
    }

    /**
     * Tests the functionality of adbRoot
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_adbRoot_alreadyRoot() throws DeviceNotAvailableException {
        doReturn(true).when(mockDevice).isAdbRoot();
        mPreparer.adbRoot();
        verify(mockDevice, times(0)).executeAdbCommand(ArgumentMatchers.anyString());
    }

    /**
     * Tests the functionality of startFramework
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_startFramework() throws DeviceNotAvailableException {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        doReturn("system_server")
                .when(mockDevice)
                .executeShellCommand(eq("ps -g system | grep system_server"));
        mPreparer.startFramework();
        verify(mockDevice, times(1))
                .executeShellCommand(
                        eq("setprop " + VtsDevicePreparer.SYSPROP_VTS_NATIVE_SERVER + " 0"));
        verify(mockDevice, times(1)).executeShellCommand(eq("start"));
    }

    /**
     * Tests the functionality of stopFramework
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_stopFramework() throws DeviceNotAvailableException {
        mPreparer.stopFramework();
        verify(mockDevice, times(1)).executeShellCommand(eq("stop"));
        verify(mockDevice, times(1))
                .executeShellCommand(
                        eq("setprop " + VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED + " 0"));
    }

    /**
     * Tests the functionality of startNativeServers
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_startNativeServers() throws DeviceNotAvailableException {
        mPreparer.startNativeServers();
        verify(mockDevice, times(1))
                .executeShellCommand(
                        eq("setprop " + VtsDevicePreparer.SYSPROP_VTS_NATIVE_SERVER + " 0"));
    }

    /**
     * Tests the functionality of stopNativeServers
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_stopNativeServers() throws DeviceNotAvailableException {
        mPreparer.stopNativeServers();
        verify(mockDevice, times(1))
                .executeShellCommand(
                        eq("setprop " + VtsDevicePreparer.SYSPROP_VTS_NATIVE_SERVER + " 1"));
    }

    /**
     * Tests the functionality of setProp
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_setProp() throws DeviceNotAvailableException {
        mPreparer.setProperty("key", "value");
        verify(mockDevice, times(1)).executeShellCommand(eq("setprop key value"));
    }

    /**
     * Tests the functionality of isBootCompleted
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_true() throws DeviceNotAvailableException {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(mPreparer.isBootCompleted());
        verify(mockDevice, times(1)).getProperty(eq(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE));
        verify(mockDevice, times(1)).getProperty(eq(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED));
    }

    /**
     * Tests the functionality of isBootCompleted
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_false1() throws DeviceNotAvailableException {
        doReturn("0").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(!mPreparer.isBootCompleted());
    }

    /**
     * Tests the functionality of isBootCompleted
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_false2() throws DeviceNotAvailableException {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("0").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(!mPreparer.isBootCompleted());
    }

    /**
     * Tests the functionality of isBootCompleted
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_false3() throws DeviceNotAvailableException {
        doReturn("0").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("0").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(!mPreparer.isBootCompleted());
    }

    /**
     * Tests the functionality of isBootCompleted when a dev boot completed sysprop is undefined.
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_null1() throws DeviceNotAvailableException {
        doReturn(null).when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(!mPreparer.isBootCompleted());
    }

    /**
     * Tests the functionality of isBootCompleted when a sys boot completed sysprop is undefined.
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_null2() throws DeviceNotAvailableException {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn(null).when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(!mPreparer.isBootCompleted());
    }

    /**
     * Tests the functionality of isBootCompleted when two boot completed sysprops are undefined.
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isBootCompleted_null3() throws DeviceNotAvailableException {
        doReturn(null).when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn(null).when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        assertTrue(!mPreparer.isBootCompleted());
    }

    /**
     * Tests the functionality of isFrameworkRunning
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isFrameworkRunning_true() throws DeviceNotAvailableException {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        doReturn("system_server").when(mockDevice).executeShellCommand(contains("ps -g system"));
        assertTrue(mPreparer.isFrameworkRunning());
    }

    /**
     * Tests the functionality of isFrameworkRunning
     *
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_isFrameworkRunning_false() throws DeviceNotAvailableException {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        doReturn("").when(mockDevice).executeShellCommand(contains("ps -g system"));
        assertTrue(!mPreparer.isFrameworkRunning());
    }

    /**
     * Tests the functionality of enable-radio-log option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_enableRadioLog()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn("0").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(1))
                .executeShellCommand("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 1");
        verify(mockDevice, times(1)).reboot();
    }

    /**
     * Tests the functionality of enable-radio-log option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_enableRadioLog_alreadyEnabled()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(0))
                .executeShellCommand(eq("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 1"));
        verify(mockDevice, times(0)).reboot();
    }

    /**
     * Tests the functionality of enable-radio-log option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_enableRadioLog_notAvailable1()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn(null).when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(0))
                .executeShellCommand(eq("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 1"));
        verify(mockDevice, times(0)).reboot();
    }

    /**
     * Tests the functionality of enable-radio-log option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_enableRadioLog_notAvailable2()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn("").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(0))
                .executeShellCommand(eq("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 1"));
        verify(mockDevice, times(0)).reboot();
    }

    /**
     * Test tearDown is skipped when pre-existing DeviceNotAvailableException is seen.
     */
    @Test
    public void test_tearDown_with_DNAE() throws Exception {
        DeviceNotAvailableException exception =
                new DeviceNotAvailableException("device not available");
        mPreparer.tearDown(mockDevice, mockBuildInfo, exception);
    }

    /**
     * Tests the functionality of radio log restore option.
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_tearDown_enableAdbRoot_turnOff() throws DeviceNotAvailableException {
        doReturn(true).when(mockDevice).isAdbRoot();
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = true;
        mPreparer.mRestoreRadioLog = true;
        mPreparer.mInitialRadioLog = DeviceOptionState.DISABLED;
        mPreparer.tearDown(mockDevice, mockBuildInfo, null);
        verify(mockDevice, times(1))
                .executeShellCommand("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 0");
        verify(mockDevice, times(1)).reboot();
    }

    /**
     * Tests the functionality of radio log restore option.
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_tearDown_enableAdbRoot_noNeedTurnOff() throws DeviceNotAvailableException {
        doReturn(true).when(mockDevice).isAdbRoot();
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = true;
        mPreparer.mRestoreRadioLog = true;
        mPreparer.mInitialRadioLog = DeviceOptionState.ENABLED;
        mPreparer.tearDown(mockDevice, mockBuildInfo, null);
        verify(mockDevice, times(0))
                .executeShellCommand("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 0");
        verify(mockDevice, times(0)).reboot();
    }

    /**
     * Tests the functionality of radio log restore option.
     * @throws DeviceNotAvailableException
     */
    @Test
    public void test_tearDown_enableAdbRoot_noNeedTurnOff2() throws DeviceNotAvailableException {
        doReturn(true).when(mockDevice).isAdbRoot();
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_RADIO_LOG);
        mPreparer.mEnableRadioLog = false;
        mPreparer.mRestoreRadioLog = true;
        mPreparer.mInitialRadioLog = DeviceOptionState.ENABLED;
        mPreparer.tearDown(mockDevice, mockBuildInfo, null);
        verify(mockDevice, times(0))
                .executeShellCommand("setprop " + VtsDevicePreparer.SYSPROP_RADIO_LOG + " 0");
        verify(mockDevice, times(0)).reboot();
    }

    /**
     * Tests the functionality of enable-adb-root option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_enableAdbRoot()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn(false).when(mockDevice).isAdbRoot();
        mPreparer.mEnableAdbRoot = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(1)).executeAdbCommand(eq("root"));
    }

    /**
     * Tests the functionality of enable-adb-root option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_enableAdbRoot_alreadyRoot()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn(true).when(mockDevice).isAdbRoot();
        mPreparer.mEnableAdbRoot = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(0)).executeAdbCommand(eq("root"));
    }

    /**
     * Tests the functionality of enable-adb-root option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_disableAdbRoot()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn(true).when(mockDevice).isAdbRoot();
        mPreparer.mDisableAdbRoot = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(1)).executeAdbCommand(eq("unroot"));
    }

    /**
     * Tests the functionality of enable-adb-root option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_disableAdbRoot_alreadyUnroot()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn(false).when(mockDevice).isAdbRoot();
        mPreparer.mDisableAdbRoot = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(0)).executeAdbCommand(eq("unroot"));
    }

    /**
     * Tests the functionality of start-framework option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_startFramework()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        doReturn("system_server")
                .when(mockDevice)
                .executeShellCommand("ps -g system | grep system_server");
        mPreparer.mStartFramework = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(1)).executeShellCommand(eq("start"));
    }

    /**
     * Tests the functionality of stop-framework option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_stopFramework()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        mPreparer.mStopFramework = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(1)).executeShellCommand(eq("stop"));
    }

    /**
     * Tests whether conflict options invokes no action
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_setUp_conflict()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        mPreparer.mStartFramework = true;
        mPreparer.mStopFramework = true;
        mPreparer.mEnableAdbRoot = true;
        mPreparer.mDisableAdbRoot = true;
        mPreparer.setUp(mockDevice, mockBuildInfo);
        verify(mockDevice, times(0)).executeShellCommand(eq("start"));
        verify(mockDevice, times(0)).executeShellCommand(eq("stop"));
        verify(mockDevice, times(0)).executeAdbCommand(eq("root"));
        verify(mockDevice, times(0)).executeAdbCommand(eq("unroot"));
    }

    /**
     * Tests the functionality of restore-framework option
     *
     * @throws DeviceNotAvailableException
     * @throws BuildError
     * @throws TargetSetupError
     */
    @Test
    public void test_tearDown_restoreFramework()
            throws DeviceNotAvailableException, TargetSetupError, BuildError {
        doReturn("0")
                .doReturn("1")
                .when(mockDevice)
                .getProperty(VtsDevicePreparer.SYSPROP_DEV_BOOTCOMPLETE);
        doReturn("1").when(mockDevice).getProperty(VtsDevicePreparer.SYSPROP_SYS_BOOT_COMPLETED);
        doReturn("system_server")
                .when(mockDevice)
                .executeShellCommand("ps -g system | grep system_server");
        mPreparer.mRestoreFramework = true;
        mPreparer.mInitialFrameworkStarted = true;
        mPreparer.mStopFramework = true;
        mPreparer.tearDown(mockDevice, mockBuildInfo, new Exception());
        verify(mockDevice, times(1)).executeShellCommand(eq("start"));
    }
}
