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
package com.android.tradefed.device.cloud;

import com.android.ddmlib.IDevice;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceMonitor;
import com.android.tradefed.device.IDeviceStateMonitor;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.device.TestDevice;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.cloud.GceAvdInfo.GceStatus;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.StreamUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;

/**
 * A device running inside a virtual machine that we manage remotely via a Tradefed instance inside
 * the VM.
 */
public class ManagedRemoteDevice extends TestDevice implements ITestLoggerReceiver {

    private GceManager mGceHandler = null;
    private GceAvdInfo mGceAvd;
    private ITestLogger mTestLogger;

    private TestDeviceOptions mCopiedOptions;
    private IConfiguration mValidationConfig;

    /**
     * Creates a {@link ManagedRemoteDevice}.
     *
     * @param device the associated {@link IDevice}
     * @param stateMonitor the {@link IDeviceStateMonitor} mechanism to use
     * @param allocationMonitor the {@link IDeviceMonitor} to inform of allocation state changes.
     */
    public ManagedRemoteDevice(
            IDevice device, IDeviceStateMonitor stateMonitor, IDeviceMonitor allocationMonitor) {
        super(device, stateMonitor, allocationMonitor);
    }

    @Override
    public void preInvocationSetup(IBuildInfo info)
            throws TargetSetupError, DeviceNotAvailableException {
        super.preInvocationSetup(info);
        mGceAvd = null;
        // First get the options
        TestDeviceOptions options = getOptions();
        // We create a brand new GceManager each time to ensure clean state.
        mGceHandler = new GceManager(getDeviceDescriptor(), options, info);
        getGceHandler().logStableHostImageInfos(info);
        setFastbootEnabled(false);

        // Launch GCE helper script.
        long startTime = getCurrentTime();
        launchGce();
        long remainingTime = options.getGceCmdTimeout() - (getCurrentTime() - startTime);
        if (remainingTime < 0) {
            throw new DeviceNotAvailableException(
                    String.format("Failed to launch GCE after %sms", options.getGceCmdTimeout()),
                    getSerialNumber(),
                    DeviceErrorIdentifier.FAILED_TO_LAUNCH_GCE);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void postInvocationTearDown(Throwable exception) {
        try {
            CLog.i("Shutting down GCE device %s", getSerialNumber());
            // Log the last part of the logcat from the tear down.
            if (!(getIDevice() instanceof StubDevice)) {
                try (InputStreamSource logcatSource = getLogcat()) {
                    clearLogcat();
                    String name = "device_logcat_teardown_gce";
                    mTestLogger.testLog(name, LogDataType.LOGCAT, logcatSource);
                }
            }

            if (mGceAvd != null) {
                if (mGceAvd.hostAndPort() != null) {
                    // attempt to get a bugreport if Gce Avd is a failure
                    if (!GceStatus.SUCCESS.equals(mGceAvd.getStatus())) {
                        // Get a bugreport via ssh
                        getSshBugreport();
                    }
                    // Log the serial output of the instance.
                    getGceHandler().logSerialOutput(mGceAvd, mTestLogger);

                    // Fetch remote files
                    CommonLogRemoteFileUtil.fetchCommonFiles(
                            mTestLogger, mGceAvd, getOptions(), getRunUtil());
                }
                // Cleanup GCE first to make sure ssh tunnel has nowhere to go.
                if (!getOptions().shouldSkipTearDown()) {
                    getGceHandler().shutdownGce();
                }
            }

            setFastbootEnabled(false);

            if (getGceHandler() != null) {
                getGceHandler().cleanUp();
            }
        } finally {
            // Reset the internal variable
            mCopiedOptions = null;
            if (mValidationConfig != null) {
                mValidationConfig.cleanConfigurationData();
                mValidationConfig = null;
            }
            // Ensure parent postInvocationTearDown is always called.
            super.postInvocationTearDown(exception);
        }
    }

    @Override
    public void setTestLogger(ITestLogger testLogger) {
        mTestLogger = testLogger;
    }

    /** Returns the {@link GceAvdInfo} describing the remote instance. */
    public GceAvdInfo getRemoteAvdInfo() {
        return mGceAvd;
    }

    /** Launch the actual gce device based on the build info. */
    protected void launchGce() throws TargetSetupError {
        TargetSetupError exception = null;
        for (int attempt = 0; attempt < getOptions().getGceMaxAttempt(); attempt++) {
            try {
                mGceAvd = getGceHandler().startGce();
                if (mGceAvd != null) break;
            } catch (TargetSetupError tse) {
                CLog.w(
                        "Failed to start Gce with attempt: %s out of %s. With Exception: %s",
                        attempt + 1, getOptions().getGceMaxAttempt(), tse);
                exception = tse;
            }
        }
        if (mGceAvd == null) {
            throw exception;
        } else {
            CLog.i("GCE AVD has been started: %s", mGceAvd);
            if (GceAvdInfo.GceStatus.BOOT_FAIL.equals(mGceAvd.getStatus())) {
                throw new TargetSetupError(mGceAvd.getErrors(), getDeviceDescriptor());
            }
        }
    }

    /** Capture a remote bugreport by ssh-ing into the device directly. */
    private void getSshBugreport() {
        File bugreportFile = null;
        try {
            bugreportFile =
                    GceManager.getNestedDeviceSshBugreportz(mGceAvd, getOptions(), getRunUtil());
            if (bugreportFile != null) {
                InputStreamSource bugreport = new FileInputStreamSource(bugreportFile);
                mTestLogger.testLog("bugreportz-ssh", LogDataType.BUGREPORTZ, bugreport);
                StreamUtil.cancel(bugreport);
            }
        } catch (IOException e) {
            CLog.e(e);
        } finally {
            FileUtil.deleteFile(bugreportFile);
        }
    }

    /** Returns the current system time. Exposed for testing. */
    @VisibleForTesting
    protected long getCurrentTime() {
        return System.currentTimeMillis();
    }

    /** Returns the instance of the {@link GceManager}. */
    @VisibleForTesting
    GceManager getGceHandler() {
        return mGceHandler;
    }

    /**
     * Override the base getter to be able to resolve dynamic options before attempting to do the
     * remote setup.
     */
    @Override
    public TestDeviceOptions getOptions() {
        if (mCopiedOptions == null) {
            mCopiedOptions = new TestDeviceOptions();
            TestDeviceOptions options = super.getOptions();
            OptionCopier.copyOptionsNoThrow(options, mCopiedOptions);
            mValidationConfig = new Configuration("validation", "validation");
            mValidationConfig.setDeviceOptions(mCopiedOptions);
            try {
                mValidationConfig.resolveDynamicOptions(new DynamicRemoteFileResolver());
            } catch (BuildRetrievalError | ConfigurationException e) {
                throw new RuntimeException(e);
            }
        }
        return mCopiedOptions;
    }
}
