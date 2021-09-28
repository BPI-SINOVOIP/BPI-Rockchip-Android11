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
package com.android.cts.devicepolicy.metrics;

import static com.google.common.truth.Truth.assertWithMessage;

import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.EventMetricData;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Helper class to assert <code>DevicePolicyEvent</code> atoms were logged.
 */
public final class DevicePolicyEventLogVerifier {

    public interface Action {
        void apply() throws Exception;
    }

    private static final int WAIT_TIME_SHORT = 500;

    /**
     * Asserts that <code>expectedLogs</code> were logged as a result of executing
     * <code>action</code>, in the same order. Note that {@link Action#apply() } is always
     * invoked on the <code>action</code> parameter, even if statsd logs are disabled.
     */
    public static void assertMetricsLogged(ITestDevice device, Action action,
            DevicePolicyEventWrapper... expectedLogs) throws Exception {
        final AtomMetricTester logVerifier = new AtomMetricTester(device);
        if (logVerifier.isStatsdDisabled()) {
            action.apply();
            return;
        }
        try {
            logVerifier.cleanLogs();
            logVerifier.createAndUploadConfig(Atom.DEVICE_POLICY_EVENT_FIELD_NUMBER);
            action.apply();

            Thread.sleep(WAIT_TIME_SHORT);

            final List<EventMetricData> data = logVerifier.getEventMetricDataList();
            for (DevicePolicyEventWrapper expectedLog : expectedLogs) {
                assertExpectedMetricLogged(data, expectedLog);
            }
        } finally {
            logVerifier.cleanLogs();
        }
    }

    /**
     * Asserts that <code>expectedLogs</code> were not logged as a result of executing
     * <code>action</code>. Note that {@link Action#apply() } is always
     * invoked on the <code>action</code> parameter, even if statsd expectedLogs are disabled.
     */
    public static void assertMetricsNotLogged(ITestDevice device, Action action,
            DevicePolicyEventWrapper... expectedLogs) throws Exception {
        final AtomMetricTester logVerifier = new AtomMetricTester(device);
        if (logVerifier.isStatsdDisabled()) {
            action.apply();
            return;
        }
        try {
            logVerifier.cleanLogs();
            logVerifier.createAndUploadConfig(Atom.DEVICE_POLICY_EVENT_FIELD_NUMBER);
            action.apply();

            Thread.sleep(WAIT_TIME_SHORT);

            final List<EventMetricData> data = logVerifier.getEventMetricDataList();
            for (DevicePolicyEventWrapper expectedLog : expectedLogs) {
                assertExpectedMetricNotLogged(data, expectedLog);
            }
        } finally {
            logVerifier.cleanLogs();
        }
    }

    public static boolean isStatsdEnabled(ITestDevice device) throws DeviceNotAvailableException {
        final AtomMetricTester logVerifier = new AtomMetricTester(device);
        return !logVerifier.isStatsdDisabled();
    }

    private static void assertExpectedMetricLogged(List<EventMetricData> data,
            DevicePolicyEventWrapper expectedLog) {
        assertWithMessage("Expected metric was not logged.")
                .that(isExpectedMetricLogged(data, expectedLog)).isTrue();
    }

    private static void assertExpectedMetricNotLogged(List<EventMetricData> data,
            DevicePolicyEventWrapper expectedLog) {
        assertWithMessage("Expected metric was logged.")
                .that(isExpectedMetricLogged(data, expectedLog)).isFalse();
    }

    private static boolean isExpectedMetricLogged(List<EventMetricData> data,
            DevicePolicyEventWrapper expectedLog) {
        final List<DevicePolicyEventWrapper> closestMatches = new ArrayList<>();
        AtomMetricTester.dropWhileNot(data, atom -> {
            final DevicePolicyEventWrapper actualLog =
                    DevicePolicyEventWrapper.fromDevicePolicyAtom(atom.getDevicePolicyEvent());
            if (actualLog.getEventId() == expectedLog.getEventId()) {
                closestMatches.add(actualLog);
            }
            return Objects.equals(actualLog, expectedLog);
        });
        return closestMatches.contains(expectedLog);
    }
}