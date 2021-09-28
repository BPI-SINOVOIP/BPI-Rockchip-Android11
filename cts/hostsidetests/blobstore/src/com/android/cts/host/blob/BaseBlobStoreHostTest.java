/*
 * Copyright (C) 2020 Android Open Source Project
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
package com.android.cts.host.blob;

import static com.google.common.truth.Truth.assertWithMessage;

import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions;
import com.android.tradefed.util.Pair;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.stream.Collectors;

abstract class BaseBlobStoreHostTest extends BaseHostJUnit4Test {
    protected static final String TARGET_APK = "CtsBlobStoreHostTestHelper.apk";
    protected static final String TARGET_PKG = "com.android.cts.device.blob";

    private static final long TIMEOUT_BOOT_COMPLETE_MS = 120_000;
    private static final long DEFAULT_INSTRUMENTATION_TIMEOUT_MS = 900_000; // 15min

    protected static final String KEY_SESSION_ID = "session";

    protected static final String KEY_DIGEST = "digest";
    protected static final String KEY_EXPIRY = "expiry";
    protected static final String KEY_LABEL = "label";
    protected static final String KEY_TAG = "tag";

    protected static final String KEY_ALLOW_PUBLIC = "public";

    protected void runDeviceTest(String testPkg, String testClass, String testMethod)
            throws Exception {
        runDeviceTest(testPkg, testClass, testMethod, null);
    }

    protected void runDeviceTestAsUser(String testPkg, String testClass, String testMethod,
            int userId) throws Exception {
        runDeviceTestAsUser(testPkg, testClass, testMethod, null, userId);
    }

    protected void runDeviceTest(String testPkg, String testClass, String testMethod,
            Map<String, String> instrumentationArgs) throws Exception {
        runDeviceTestAsUser(testPkg, testClass, testMethod, instrumentationArgs, -1);
    }

    protected void runDeviceTestAsUser(String testPkg, String testClass, String testMethod,
            Map<String, String> instrumentationArgs, int userId) throws Exception {
        final DeviceTestRunOptions deviceTestRunOptions = new DeviceTestRunOptions(testPkg)
                .setTestClassName(testClass)
                .setTestMethodName(testMethod)
                .setMaxInstrumentationTimeoutMs(DEFAULT_INSTRUMENTATION_TIMEOUT_MS);
        if (userId != -1) {
            deviceTestRunOptions.setUserId(userId);
        }
        if (instrumentationArgs != null) {
            for (Map.Entry<String, String> entry : instrumentationArgs.entrySet()) {
                deviceTestRunOptions.addInstrumentationArg(entry.getKey(), entry.getValue());
            }
        }
        assertWithMessage(testMethod + " failed").that(
                runDeviceTests(deviceTestRunOptions)).isTrue();
    }

    protected void rebootAndWaitUntilReady() throws Exception {
        // TODO: use rebootUserspace()
        getDevice().reboot(); // reboot() waits for device available
    }

    protected boolean isMultiUserSupported() throws Exception {
        return getDevice().isMultiUserSupported();
    }

    protected Map<String, String> createArgsFromLastTestRun() {
        final Map<String, String> args = new HashMap<>();
        for (String key : new String[] {
                KEY_SESSION_ID,
                KEY_DIGEST,
                KEY_EXPIRY,
                KEY_LABEL,
                KEY_TAG
        }) {
            final String value = getLastDeviceRunResults().getRunMetrics().get(key);
            if (value != null) {
                args.put(key, value);
            }
        }
        return args;
    }

    protected Map<String, String> createArgs(Pair<String, String>... keyValues) {
        return Arrays.stream(keyValues).collect(Collectors.toMap(p -> p.first, p -> p.second));
    }
}