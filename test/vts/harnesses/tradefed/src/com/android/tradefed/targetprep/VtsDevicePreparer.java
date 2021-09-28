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

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;

/**
 * Perform basic device preparation tasks such as check (and turn on/off) device framework.
 *
 * Options with prefix "enable" or "disable" enables or disables a feature,
 * and setting them to false means no operation.
 *
 * For a same feature, if "enable" and "disable" options are both set to true will result in no-op.
 *
 * Any of the "restore" option will restore the feature to the state before test. This means actions
 * may be taken even if the feature is not enabled. For example, if device framework is on,
 * "enable-framework" is "false", "restore-all" is set to "true", and during a test the framework
 * is turned on/off, this class will start the framework in the end.
 */
public class VtsDevicePreparer implements ITargetPreparer, ITargetCleaner {
    @Option(name = "start-framework",
            description = "Whether to start android framework on device. "
                    + "Setting this option to false does not stop framework. "
                    + "If both start-framework and stop-framework is set to true, "
                    + "no action will be taken.")
    boolean mStartFramework = false;

    @Option(name = "stop-framework",
            description = "Whether to stop android framework on device. "
                    + "Setting this option to false does not start framework. "
                    + "If both start-framework and disable-framework is set to true, "
                    + "no action will be taken.")
    boolean mStopFramework = false;

    @Option(name = "restore-framework",
            description = "Whether to restore the initial framework "
                    + "start/stop status after tests. "
                    + "This option will be overriden by restore-all if that is set to true.")
    boolean mRestoreFramework = false;

    @Option(name = "enable-adb-root",
            description = "Whether to enable adb root on device. "
                    + "If set to true, `adb root` will be called. "
                    + "Setting this option to false does not disable adb root. "
                    + "This option requires enable-root setting be true in test plan setting. "
                    + "If both enable-adb-root and disable-adb-root is set to true, no action "
                    + "will be taken.")
    boolean mEnableAdbRoot = false;

    @Option(name = "disable-adb-root",
            description = "Whether to disable adb root on device. "
                    + "If set to true, `adb root` will be called. "
                    + "Setting this option to false does not enable adb root. "
                    + "If both enable-adb-root and disable-adb-root is set to true, "
                    + "no action will be taken.")
    boolean mDisableAdbRoot = false;

    @Option(name = "restore-adb-root",
            description = "Whether to restore the initial adb root "
                    + "status after tests. "
                    + "This option will be overriden by restore-all if that is set to true.")
    boolean mRestoreAdbRoot = false;

    @Option(name = "restore-all",
            description = "Whether to restore device status after tests. "
                    + "This option overrides individual restore options such as restore-framework.")
    boolean mRestoreAll = false;

    @Option(name = "enable-radio-log",
            description = "Whether to enable radio modem logcat. Device will reboot if enabled. "
                    + "This option requires adb root but will not automatically root adb. "
                    + "Setting this option false does not disable already enabled radio log.")
    boolean mEnableRadioLog = false;

    @Option(name = "restore-radio-log",
            description = "Whether to restore radio modem logcat status after test. "
                    + "This option requires adb root but will not automatically root adb. "
                    + "If both enable-radio-log and this option is set to true, "
                    + "device will reboot again at the end of test.")
    boolean mRestoreRadioLog = false;

    public static long DEVICE_BOOT_TIMEOUT = 3 * 60 * 1000;
    static final String SYSPROP_DEV_BOOTCOMPLETE = "dev.bootcomplete";
    static final String SYSPROP_SYS_BOOT_COMPLETED = "sys.boot_completed";
    public static String SYSPROP_RADIO_LOG = "persist.vendor.radio.adb_log_on";
    public static String SYSPROP_RADIO_LOG_OLD = "persist.radio.adb_log_on";
    public String mSyspropRadioLog = SYSPROP_RADIO_LOG;

    // The name of a system property which tells whether to stop properly configured
    // native servers where properly configured means a server's init.rc is
    // configured to stop when that property's value is 1.
    static final String SYSPROP_VTS_NATIVE_SERVER = "vts.native_server.on";

    boolean mInitialFrameworkStarted = true;
    boolean mInitialAdbRoot = false;
    DeviceOptionState mInitialRadioLog = DeviceOptionState.UNKNOWN;
    ITestDevice mDevice = null;

    // Whether to reboot device during setUp
    boolean mRebootSetup = false;
    // Whether to reboot device during tearDown
    boolean mRebootTearDown = false;

    public enum DeviceOptionState {
        UNKNOWN,
        ENABLED,
        DISABLED,
        NOT_AVAILABLE;
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        mDevice = device;

        // The order of the following steps matters.

        adbRootPreRebootSetUp();
        radioLogPreRebootSetup();
        frameworkPreRebootSetUp();

        if (mRebootSetup) {
            device.reboot();
            device.waitForBootComplete(DEVICE_BOOT_TIMEOUT);
        }

        frameworkPostRebootSetUp();
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(ITestDevice device, IBuildInfo buildInfo, Throwable e)
            throws DeviceNotAvailableException {
        if (e instanceof DeviceNotAvailableException) {
            CLog.i("Skip tear down due to DeviceNotAvailableException");
            return;
        }
        // The order of the following steps matters.

        radioLogPreTearDown();

        if (mRebootTearDown) {
            device.reboot();
            device.waitForBootComplete(DEVICE_BOOT_TIMEOUT);
        }

        adbRootPostTearDown();
        frameworkPostTearDown();
    }

    /**
     * Prepares adb root status.
     *
     * @throws DeviceNotAvailableException
     */
    private void adbRootPreRebootSetUp() throws DeviceNotAvailableException {
        if (mRestoreAll || mRestoreAdbRoot) {
            // mInitialAdbRoot is only needed at tearDown because AndroidDeviceController
            // already checks initial adb root status
            mInitialAdbRoot = mDevice.isAdbRoot();
        }

        if (mEnableAdbRoot && !mDisableAdbRoot) {
            adbRoot();
        } else if (mDisableAdbRoot && !mEnableAdbRoot) {
            adbUnroot();
        }
    }

    /**
     * Restores adb root status
     *
     * @throws DeviceNotAvailableException
     *
     */
    private void adbRootPostTearDown() throws DeviceNotAvailableException {
        if (!mRestoreAll && !mRestoreAdbRoot) {
            return;
        }

        if (mInitialAdbRoot) {
            adbRoot();
        } else {
            adbUnroot();
        }
    }

    /**
     * Collect framework on/off status if restore option is enabled.
     * @throws DeviceNotAvailableException
     */
    private void frameworkPreRebootSetUp() throws DeviceNotAvailableException {
        if (mRestoreAll || mRestoreFramework) {
            // mInitialFrameworkStarted is only needed at tearDown because AndroidDeviceController
            // already checks initial framework status
            mInitialFrameworkStarted = isFrameworkRunning();
        }
    }

    /**
     * Prepares device framework start/stop status
     *
     * @throws DeviceNotAvailableException
     */
    private void frameworkPostRebootSetUp() throws DeviceNotAvailableException {
        if (mStartFramework && !mStopFramework) {
            startFramework();
        } else if (mStopFramework && !mStartFramework) {
            stopFramework();
        }
    }

    /**
     * Restores the framework start/stop status
     *
     * @throws DeviceNotAvailableException
     */
    private void frameworkPostTearDown() throws DeviceNotAvailableException {
        if (!mRestoreAll && !mRestoreFramework) {
            return;
        }

        boolean current = isFrameworkRunning();

        if (mInitialFrameworkStarted && !current) {
            startFramework();
        } else if (!mInitialFrameworkStarted && current) {
            stopFramework();
        }
    }

    /**
     * @throws DeviceNotAvailableException
     *
     */
    private void radioLogPreRebootSetup() throws DeviceNotAvailableException {
        if (mEnableRadioLog || mRestoreAll || mRestoreRadioLog) {
            mInitialRadioLog = radioLogGetState();

            if (mInitialRadioLog == DeviceOptionState.NOT_AVAILABLE) {
                CLog.d("Radio modem log configured but the setting is not available "
                        + " on device. Skipping.");
                return;
            }
        }

        if (mEnableRadioLog && mInitialRadioLog == DeviceOptionState.DISABLED) {
            this.setProperty(SYSPROP_RADIO_LOG, "1");
            CLog.d("Turing on radio modem log.");
            mRebootSetup = true;
        }
    }

    /**
     * @throws DeviceNotAvailableException
     *
     */
    private void radioLogPreTearDown() throws DeviceNotAvailableException {
        if (mInitialRadioLog == DeviceOptionState.NOT_AVAILABLE) {
            return;
        }

        if (!mRestoreAll && !mRestoreRadioLog) {
            return;
        }

        DeviceOptionState current = radioLogGetState();

        if (mInitialRadioLog == DeviceOptionState.DISABLED
                && current == DeviceOptionState.ENABLED) {
            CLog.d("Turing off radio modem log.");
            this.setProperty(SYSPROP_RADIO_LOG, "0");
        } else if (mInitialRadioLog == DeviceOptionState.ENABLED
                && current == DeviceOptionState.DISABLED) {
            CLog.d("Turing on radio modem log.");
            this.setProperty(SYSPROP_RADIO_LOG, "1");
        } else {
            return;
        }

        mRebootTearDown = true;
    }

    /**
     * Returns the state of radio modem log on/off state.
     * @return DeviceOptionState specifying the state.
     * @throws DeviceNotAvailableException
     */
    private DeviceOptionState radioLogGetState() throws DeviceNotAvailableException {
        String radioProp = mDevice.getProperty(mSyspropRadioLog);

        if (radioProp == null && mSyspropRadioLog != SYSPROP_RADIO_LOG_OLD) {
            mSyspropRadioLog = SYSPROP_RADIO_LOG_OLD;
            radioProp = mDevice.getProperty(mSyspropRadioLog);
        }

        if (radioProp == null) {
            return DeviceOptionState.NOT_AVAILABLE;
        }

        switch (radioProp) {
            case "1":
                return DeviceOptionState.ENABLED;
            case "0":
                return DeviceOptionState.DISABLED;
            default:
                return DeviceOptionState.NOT_AVAILABLE;
        }
    }

    // ----------------------- Below are device util methods -----------------------

    /**
     * Executes command "adb root" if adb is not running as root.
     *
     * @throws DeviceNotAvailableException
     */
    void adbRoot() throws DeviceNotAvailableException {
        if (!mDevice.isAdbRoot()) {
            mDevice.executeAdbCommand("root");
        }
    }

    /**
     * Executes command "adb unroot" if adb is running as root.
     *
     * @throws DeviceNotAvailableException
     */
    void adbUnroot() throws DeviceNotAvailableException {
        if (mDevice.isAdbRoot()) {
            mDevice.executeAdbCommand("unroot");
        }
    }

    /**
     * Start Android framework on a device.
     *
     * This method also starts VTS native servers which is required to start the framework
     * This method will block until boot complete or timeout.
     * A default timeout value will be used
     *
     * @throws DeviceNotAvailableException if timeout waiting for boot complete
     */
    void startFramework() throws DeviceNotAvailableException {
        startFramework(DEVICE_BOOT_TIMEOUT);
    }

    /**
     * Starts Android framework on a device.
     *
     * This method also starts VTS native servers which is required to start the framework
     * This method will block until boot complete or timeout.
     *
     * @param timeout timeout in milliseconds.
     * @throws DeviceNotAvailableException if timeout waiting for boot complete
     */
    void startFramework(long timeout) throws DeviceNotAvailableException {
        startNativeServers();
        mDevice.executeShellCommand("start");

        waitForFrameworkStartComplete();
    }

    /**
     * Wait for Android framework to complete starting.
     *
     * @throws DeviceNotAvailableException timed out
     */
    void waitForFrameworkStartComplete() throws DeviceNotAvailableException {
        waitForFrameworkStartComplete(DEVICE_BOOT_TIMEOUT);
    }

    /**
     * Wait for Android framework to complete starting.
     *
     * @param timeout
     * @throws DeviceNotAvailableException
     */
    void waitForFrameworkStartComplete(long timeout) throws DeviceNotAvailableException {
        long start = System.currentTimeMillis();

        // First, wait for boot completion
        mDevice.waitForBootComplete(timeout);

        while (!isFrameworkRunning()) {
            if (System.currentTimeMillis() - start >= timeout) {
                throw new DeviceNotAvailableException(
                        "Timed out waiting for framework start complete.",
                        mDevice.getSerialNumber());
            }

            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
                throw new DeviceNotAvailableException(
                        "Intrupted while waiting for framework start complete.",
                        mDevice.getSerialNumber());
            }
        }
    }

    /**
     * Stops Android framework on a device.
     *
     * @throws DeviceNotAvailableException
     */
    void stopFramework() throws DeviceNotAvailableException {
        mDevice.executeShellCommand("stop");
        this.setProperty(SYSPROP_SYS_BOOT_COMPLETED, "0");
    }

    /**
     * Restarts Android framework on a device.
     *
     * This method will block until start finish or timeout.
     *
     * @throws DeviceNotAvailableException
     */
    void restartFramework() throws DeviceNotAvailableException {
        stopFramework();
        startFramework();
    }

    /**
     * Starts all native servers.
     *
     * @throws DeviceNotAvailableException
     */
    void startNativeServers() throws DeviceNotAvailableException {
        this.setProperty(SYSPROP_VTS_NATIVE_SERVER, "0");
    }

    /**
     * Stops all native servers.
     *
     * @throws DeviceNotAvailableException
     */
    void stopNativeServers() throws DeviceNotAvailableException {
        this.setProperty(SYSPROP_VTS_NATIVE_SERVER, "1");
    }

    /**
     * Sets a sysprop on the device.
     *
     * TODO: to be removed once the API is added to TF
     *
     * @param key the key of a sysprop.
     * @param value the value of a sysprop.
     * @throws DeviceNotAvailableException
     */
    void setProperty(String key, String value) throws DeviceNotAvailableException {
        // TODO: check success
        mDevice.executeShellCommand(String.format("setprop %s %s", key, value));
    }

    boolean isBootCompleted() throws DeviceNotAvailableException {
        String sysBootCompleted = mDevice.getProperty(SYSPROP_SYS_BOOT_COMPLETED);
        String devBootCompleted = mDevice.getProperty(SYSPROP_DEV_BOOTCOMPLETE);
        return sysBootCompleted != null && sysBootCompleted.equals("1") && devBootCompleted != null
                && devBootCompleted.equals("1");
    }

    /**
     * Checks whether Android framework is started.
     *
     * @return True if started, False otherwise.
     * @throws DeviceNotAvailableException
     */
    boolean isFrameworkRunning() throws DeviceNotAvailableException {
        // First, check whether boot has completed.
        if (!isBootCompleted()) {
            return false;
        }

        String cmd = "ps -g system | grep system_server";
        return mDevice.executeShellCommand(cmd).contains("system_server");
    }
}
