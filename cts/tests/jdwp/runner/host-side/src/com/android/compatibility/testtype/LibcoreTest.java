/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.compatibility.testtype;

import static java.util.stream.Collectors.toList;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.AndroidJUnitTest;
import com.android.tradefed.util.ArrayUtil;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.stream.Stream;

/**
 * A specialized test type for Libcore tests that provides the ability to specify a set of
 * expectation files. Expectation files are used to indicate tests that are expected to fail,
 * often because they come from upstream projects, and should not be run under CTS.
 */
public class LibcoreTest extends AndroidJUnitTest {

    private static final String INSTRUMENTATION_ARG_NAME = "core-expectations";

    @Option(name = "core-expectation", description = "Provides failure expectations for libcore "
            + "tests via the specified file; the path must be absolute and will be resolved to "
            + "matching bundled resource files; this parameter should be repeated for each "
            + "expectation file")
    private List<String> mCoreExpectations = new ArrayList<>();

    @Option(name = "virtual-device-core-expectation", description = "Provides failure expectations "
            + "on virtual devices only for libcore tests via the specified file; the path must be "
            + "absolute and will be resolved to matching bundled resource files; this parameter "
            + "should be repeated for each expectation file")
    private List<String> mVirtualDeviceCoreExpectations = new ArrayList<>();

    /**
     * {@inheritDoc}
     */
    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener) throws DeviceNotAvailableException {
        List<String> coreExpectations = getCoreExpectations();
        if (!coreExpectations.isEmpty()) {
            addInstrumentationArg(INSTRUMENTATION_ARG_NAME, ArrayUtil.join(",", coreExpectations));
        }

        if (getTestPackageName() != null && getClassName() != null) {
            // If the test-package is set, we should ignore --class, --method it might cause issue
            // in more complex libcore suites.
            setClassName(null);
            setMethodName(null);
            CLog.d(
                    "Setting --class and --method to null to avoid conflict with --test-package "
                            + "option.");
        }
        super.run(testInfo, listener);
    }

    private List<String> getCoreExpectations() throws DeviceNotAvailableException {
        if (isVirtualDevice()) {
                return Stream.concat(
                        mCoreExpectations.stream(), mVirtualDeviceCoreExpectations.stream())
                    .collect(toList());
        } else {
            return mCoreExpectations;
        }
    }

    private boolean isVirtualDevice() throws DeviceNotAvailableException {
        return Objects.equals(getDevice().getProperty("ro.hardware.virtual_device"), "1");
    }
}
