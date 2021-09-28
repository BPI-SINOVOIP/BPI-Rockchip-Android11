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
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;

import java.io.File;

/**
 * Perfetto preparer pushes the config file in to the device at a standard location to which
 * perfetto have access to.
 */
@OptionClass(alias = "perfetto-preparer")
public class PerfettoPreparer extends BaseTargetPreparer {
    // Perfetto have access only to the config files under /data/misc/perfetto-traces/
    // in the device.
    private static final String DEVICE_CONFIG_PATH = "/data/misc/perfetto-traces/trace_config.pb";

    @Option(
            name = "abort-on-failure",
            description =
            "If false, continue if preparer fail.  If true, abort the invocation on any failure.")
    private boolean mAbortOnFailure = true;

    @Option(
            name = "binary-perfetto-config",
            description = "Full path to the binary version of perfetto config file.")
    private File mBinaryPerfettoConfigFile = null;


    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {

        // If the encoded version is passed, push it to the device directly
        // under /data/misc/perfetto-traces/ and use it.
        if (mBinaryPerfettoConfigFile != null) {
            if (!mBinaryPerfettoConfigFile.exists()) {
                fail("Failed to find the local binary perfetto file", null, device);
            } else if (!device.pushFile(mBinaryPerfettoConfigFile, DEVICE_CONFIG_PATH)) {
                fail("Failed to push the binary perfetto file", null, device);
            }
        }
    }

    /**
     * Helper method to only throw if mAbortOnFailure is enabled. Callers should behave as if this
     * method may return.
     */
    private void fail(String message, Throwable cause, ITestDevice device) throws TargetSetupError {
        if (mAbortOnFailure) {
            if (cause != null) {
                throw new TargetSetupError(message, cause, device.getDeviceDescriptor());
            }
            else {
                throw new TargetSetupError(message, device.getDeviceDescriptor());
            }
        } else {
            // Log the error and return
            CLog.w(message);
        }
    }

    @Override
    public void tearDown(ITestDevice device, IBuildInfo buildInfo, Throwable e)
            throws DeviceNotAvailableException {
        device.executeShellCommand("rm " + DEVICE_CONFIG_PATH);
    }
}
