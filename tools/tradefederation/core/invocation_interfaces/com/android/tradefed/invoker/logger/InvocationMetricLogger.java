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
package com.android.tradefed.invoker.logger;

import com.android.tradefed.log.LogUtil.CLog;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** A utility class for an invocation to log some metrics. */
public class InvocationMetricLogger {

    /** Some special named key that we will always populate for the invocation. */
    public enum InvocationMetricKey {
        WIFI_AP_NAME("wifi_ap_name", false),
        CLEARED_RUN_ERROR("cleared_run_error", true),
        FETCH_BUILD("fetch_build_time_ms", true),
        SETUP("setup_time_ms", true),
        SHARDING_DEVICE_SETUP_TIME("remote_device_sharding_setup_ms", true),
        AUTO_RETRY_TIME("auto_retry_time_ms", true),
        STAGE_TESTS_TIME("stage_tests_time_ms", true),
        STAGE_TESTS_BYTES("stage_tests_bytes", true),
        STAGE_TESTS_INDIVIDUAL_DOWNLOADS("stage_tests_individual_downloads", true),
        SHUTDOWN_HARD_LATENCY("shutdown_hard_latency_ms", false),
        DEVICE_DONE_TIMESTAMP("device_done_timestamp", false),
        DEVICE_RELEASE_STATE("device_release_state", false),
        DEVICE_LOST_DETECTED("device_lost_detected", false),
        VIRTUAL_DEVICE_LOST_DETECTED("virtual_device_lost_detected", false),
        DEVICE_RECOVERY("device_recovery", true),
        DEVICE_RECOVERY_FROM_RECOVERY("device_recovery_from_recovery", true),
        DEVICE_RECOVERY_FAIL("device_recovery_fail", true),
        SANDBOX_EXIT_CODE("sandbox_exit_code", false),
        CF_FETCH_ARTIFACT_TIME("cf_fetch_artifact_time_ms", false),
        CF_GCE_CREATE_TIME("cf_gce_create_time_ms", false),
        CF_LAUNCH_CVD_TIME("cf_launch_cvd_time_ms", false),
        CF_INSTANCE_COUNT("cf_instance_count", false);

        private final String mKeyName;
        // Whether or not to add the value when the key is added again.
        private final boolean mAdditive;

        private InvocationMetricKey(String key, boolean additive) {
            mKeyName = key;
            mAdditive = additive;
        }

        @Override
        public String toString() {
            return mKeyName;
        }

        public boolean shouldAdd() {
            return mAdditive;
        }
    }

    private InvocationMetricLogger() {}

    /**
     * Track metrics per ThreadGroup as a proxy to invocation since an invocation run within one
     * threadgroup.
     */
    private static final Map<ThreadGroup, Map<String, String>> mPerGroupMetrics =
            Collections.synchronizedMap(new HashMap<ThreadGroup, Map<String, String>>());

    /**
     * Add one key-value to be tracked at the invocation level.
     *
     * @param key The key under which the invocation metric will be tracked.
     * @param value The value of the invocation metric.
     */
    public static void addInvocationMetrics(InvocationMetricKey key, long value) {
        if (key.shouldAdd()) {
            String existingVal = getInvocationMetrics().get(key.toString());
            long existingLong = 0L;
            if (existingVal != null) {
                try {
                    existingLong = Long.parseLong(existingVal);
                } catch (NumberFormatException e) {
                    CLog.e(
                            "%s is expected to contain a number, instead found: %s",
                            key.toString(), existingVal);
                }
            }
            value += existingLong;
        }
        addInvocationMetrics(key.toString(), Long.toString(value));
    }

    /**
     * Add one key-value to be tracked at the invocation level.
     *
     * @param key The key under which the invocation metric will be tracked.
     * @param value The value of the invocation metric.
     */
    public static void addInvocationMetrics(InvocationMetricKey key, String value) {
        if (key.shouldAdd()) {
            String existingVal = getInvocationMetrics().get(key.toString());
            if (existingVal != null) {
                value = String.format("%s,%s", existingVal, value);
            }
        }
        addInvocationMetrics(key.toString(), value);
    }

    /**
     * Add one key-value to be tracked at the invocation level. Don't expose the String key yet to
     * avoid abuse, stick to the official {@link InvocationMetricKey} to start with.
     *
     * @param key The key under which the invocation metric will be tracked.
     * @param value The value of the invocation metric.
     */
    private static void addInvocationMetrics(String key, String value) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupMetrics) {
            if (mPerGroupMetrics.get(group) == null) {
                mPerGroupMetrics.put(group, new HashMap<>());
            }
            mPerGroupMetrics.get(group).put(key, value);
        }
    }

    /** Returns the Map of invocation metrics for the invocation in progress. */
    public static Map<String, String> getInvocationMetrics() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupMetrics) {
            if (mPerGroupMetrics.get(group) == null) {
                mPerGroupMetrics.put(group, new HashMap<>());
            }
        }
        return new HashMap<>(mPerGroupMetrics.get(group));
    }

    /** Clear the invocation metrics for an invocation. */
    public static void clearInvocationMetrics() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupMetrics) {
            mPerGroupMetrics.remove(group);
        }
    }
}
