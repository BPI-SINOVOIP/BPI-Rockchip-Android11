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
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ILogcatReceiver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.LogcatReceiver;
import com.android.tradefed.device.TestDeviceState;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import java.util.HashMap;
import java.util.Map;

/** Collector that will capture and log a logcat when a test case fails. */
public class LogcatOnFailureCollector extends BaseDeviceMetricCollector {

    private static final int MAX_LOGAT_SIZE_BYTES = 4 * 1024 * 1024;
    /** Always include a bit of prior data to capture what happened before */
    private static final int OFFSET_CORRECTION = 10000;

    private static final String NAME_FORMAT = "%s-%s-logcat-on-failure";

    private static final String LOGCAT_COLLECT_CMD = "logcat -T 150";
    // -t implies -d (dump) so it's a one time collection
    private static final String LOGCAT_COLLECT_CMD_LEGACY = "logcat -t 5000";
    private static final int API_LIMIT = 20;

    private Map<ITestDevice, ILogcatReceiver> mLogcatReceivers = new HashMap<>();
    private Map<ITestDevice, Integer> mOffset = new HashMap<>();

    @Override
    public void onTestRunStart(DeviceMetricData runData) {
        for (ITestDevice device : getRealDevices()) {
            if (getApiLevelNoThrow(device) < API_LIMIT) {
                continue;
            }
            // In case of multiple runs for the same test runner, re-init the receiver.
            initReceiver(device);
            // Get the current offset of the buffer to be able to query later
            int offset = (int) mLogcatReceivers.get(device).getLogcatData().size();
            if (offset > OFFSET_CORRECTION) {
                offset -= OFFSET_CORRECTION;
            }
            mOffset.put(device, offset);
        }
    }

    @Override
    public void onTestStart(DeviceMetricData testData) {
        // TODO: Handle the buffer to reset it at the test start
    }

    @Override
    public void onTestFail(DeviceMetricData testData, TestDescription test) {
        // Delay slightly for the error to get in the logcat
        getRunUtil().sleep(100);
        collectAndLog(test);
    }

    @Override
    public void onTestRunEnd(DeviceMetricData runData, Map<String, Metric> currentRunMetrics) {
        clearReceivers();
    }

    @VisibleForTesting
    ILogcatReceiver createLogcatReceiver(ITestDevice device) {
        // Use logcat -T 'count' to only print a few line before we start and not the full buffer
        return new LogcatReceiver(
                device, LOGCAT_COLLECT_CMD, device.getOptions().getMaxLogcatDataSize(), 0);
    }

    @VisibleForTesting
    IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }

    private void collectAndLog(TestDescription test) {
        for (ITestDevice device : getRealDevices()) {
            if (!shouldCollect(device)) {
                continue;
            }
            ILogcatReceiver receiver = mLogcatReceivers.get(device);
            // Receiver is only initialized above API 19, if not supported, we use a legacy command
            if (receiver == null) {
                CollectingByteOutputReceiver outputReceiver = new CollectingByteOutputReceiver();
                try {
                    device.executeShellCommand(LOGCAT_COLLECT_CMD_LEGACY, outputReceiver);
                    saveLogcatSource(
                            test,
                            new ByteArrayInputStreamSource(outputReceiver.getOutput()),
                            device.getSerialNumber());
                } catch (DeviceNotAvailableException e) {
                    CLog.e(e);
                }
                continue;
            }
            // If supported get the logcat buffer
            saveLogcatSource(
                    test,
                    receiver.getLogcatData(MAX_LOGAT_SIZE_BYTES, mOffset.get(device)),
                    device.getSerialNumber());
        }
    }

    private void initReceiver(ITestDevice device) {
        if (mLogcatReceivers.get(device) == null) {
            ILogcatReceiver receiver = createLogcatReceiver(device);
            mLogcatReceivers.put(device, receiver);
            receiver.start();
        }
    }

    private void clearReceivers() {
        for (ILogcatReceiver receiver : mLogcatReceivers.values()) {
            receiver.stop();
            receiver.clear();
        }
        mLogcatReceivers.clear();
        mOffset.clear();
    }

    private int getApiLevelNoThrow(ITestDevice device) {
        try {
            return device.getApiLevel();
        } catch (DeviceNotAvailableException e) {
            return 1;
        }
    }

    private void saveLogcatSource(TestDescription test, InputStreamSource source, String serial) {
        try (InputStreamSource logcatSource = source) {
            String name = String.format(NAME_FORMAT, test.toString(), serial);
            super.testLog(name, LogDataType.LOGCAT, logcatSource);
        }
    }

    private boolean shouldCollect(ITestDevice device) {
        TestDeviceState state = device.getDeviceState();
        if (!TestDeviceState.ONLINE.equals(state)) {
            CLog.d("Skip LogcatOnFailureCollector device is in state '%s'", state);
            return false;
        }
        return true;
    }
}
