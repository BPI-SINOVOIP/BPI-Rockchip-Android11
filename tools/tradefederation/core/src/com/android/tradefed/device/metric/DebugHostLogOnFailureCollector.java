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
package com.android.tradefed.device.metric;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.log.LogRegistry;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.SnapshotInputStreamSource;
import com.android.tradefed.result.TestDescription;

import java.io.IOException;
import java.io.InputStream;

/** Collector that will gather and log the host-side logs when a test case failure occurs. */
public class DebugHostLogOnFailureCollector extends BaseDeviceMetricCollector {

    private static final String NAME_FORMAT = "%s-debug-hostlog-on-failure";

    private Long offset = null;

    @Override
    public void onTestRunStart(DeviceMetricData runData) {
        offset = null;
        // TODO: Improve the offset from the start of the method instead.
        try (InputStreamSource source = getLogger().getLog()) {
            if (source == null) {
                CLog.e(
                        "Could not obtain the host logs for debugging. It won't be available "
                                + "in the event of test cases failures.");
                return;
            }
            offset = source.size();
        }
    }

    @Override
    public void onTestFail(DeviceMetricData testData, TestDescription test) {
        if (offset == null) {
            return;
        }
        try (InputStreamSource source = getLogger().getLog()) {
            if (source == null) {
                return;
            }
            try (InputStream stream = source.createInputStream()) {
                stream.skip(offset);
                try (InputStreamSource logSource =
                        new SnapshotInputStreamSource("host-log-failure", stream)) {
                    super.testLog(
                            String.format(NAME_FORMAT, test.toString()),
                            LogDataType.TEXT,
                            logSource);
                }
            }
        } catch (IOException e) {
            CLog.e(e);
        }
    }

    @VisibleForTesting
    ILeveledLogOutput getLogger() {
        LogRegistry registry = (LogRegistry) LogRegistry.getLogRegistry();
        return registry.getLogger();
    }
}
