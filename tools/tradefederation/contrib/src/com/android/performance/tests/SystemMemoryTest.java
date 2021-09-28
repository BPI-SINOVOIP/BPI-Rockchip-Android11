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

package com.android.performance.tests;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;
import java.util.HashMap;
import java.util.Map;

/** Test to gather post boot System memory usage */
public class SystemMemoryTest implements IDeviceTest, IRemoteTest {

    private static final String PROC_MEMINFO = "cat /proc/meminfo";
    private static final String MEMTOTAL = "MemTotal";
    private static final String MEMFREE = "MemFree";
    private static final String CACHED = "Cached";
    private static final String SEPARATOR = "\\s+";
    private static final String LINE_SEPARATOR = "\\n";

    @Option(
            name = "reporting-key",
            description =
                    "Reporting key is the unique identifier"
                            + "used to report data in the dashboard.")
    private String mRuKey = "";

    private ITestDevice mTestDevice = null;
    private ITestInvocationListener mlistener = null;
    private Map<String, String> mMetrics = new HashMap<>();

    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        mlistener = listener;
        String memInfo = mTestDevice.executeShellCommand(PROC_MEMINFO);
        if (!memInfo.isEmpty()) {

            uploadLogFile(memInfo, "System MemInfo");
            parseProcInfo(memInfo);
        } else {
            CLog.e("Not able to collect the /proc/meminfo before launching app");
        }
        reportMetrics(mlistener, mRuKey, mMetrics);
    }

    /**
     * Method to write the data to test logs.
     *
     * @param data
     * @param fileName
     */
    private void uploadLogFile(String data, String fileName) {
        ByteArrayInputStreamSource inputStreamSrc = null;
        try {
            inputStreamSrc = new ByteArrayInputStreamSource(data.getBytes());
            mlistener.testLog(fileName, LogDataType.TEXT, inputStreamSrc);
        } finally {
            StreamUtil.cancel(inputStreamSrc);
        }
    }

    /**
     * Method to parse the system memory details
     *
     * @param memInfo string dump of cat /proc/meminfo
     */
    private void parseProcInfo(String memInfo) {
        for (String line : memInfo.split(LINE_SEPARATOR)) {
            line = line.replace(":", "").trim();
            String dataSplit[] = line.split(SEPARATOR);
            switch (dataSplit[0].toLowerCase()) {
                case "memtotal":
                    mMetrics.put("System_MEMTOTAL", dataSplit[1]);
                    break;
                case "memfree":
                    mMetrics.put("System_MEMFREE", dataSplit[1]);
                    break;
                case "cached":
                    mMetrics.put("System_CACHED", dataSplit[1]);
                    break;
            }
        }
    }

    /**
     * Report run metrics by creating an empty test run to stick them in
     *
     * @param listener the {@link ITestInvocationListener} of test results
     * @param runName the test name
     * @param metrics the {@link Map} that contains metrics for the given test
     */
    void reportMetrics(
            ITestInvocationListener listener, String runName, Map<String, String> metrics) {
        // Create an empty testRun to report the parsed runMetrics
        CLog.d("About to report metrics: %s", metrics);
        listener.testRunStarted(runName, 0);
        listener.testRunEnded(0, TfMetricProtoUtil.upgradeConvert(metrics));
    }

    @Override
    public void setDevice(ITestDevice device) {
        mTestDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mTestDevice;
    }
}
