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
package android.device.collectors;

import android.content.Context;
import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import com.android.helpers.PerfettoHelper;

import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import java.util.function.Supplier;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;

/**
 * A {@link PerfettoListener} that captures the perfetto trace during each test method
 * and save the perfetto trace files under
 * <root_folder>/<test_display_name>/PerfettoListener/<test_display_name>-<invocation_count>.pb
 */
@OptionClass(alias = "perfetto-collector")
public class PerfettoListener extends BaseMetricListener {

    // Default perfetto config file name.
    private static final String DEFAULT_CONFIG_FILE = "trace_config.pb";
    // Default wait time before stopping the perfetto trace.
    private static final String DEFAULT_WAIT_TIME_MSECS = "3000";
    // Default output folder to store the perfetto output traces.
    private static final String DEFAULT_OUTPUT_ROOT = "/sdcard/test_results";
    // Argument to indicate the perfetto config file is text proto file.
    public static final String PERFETTO_CONFIG_TEXT_PROTO = "perfetto_config_text_proto";
    // Argument to get custom config file name for collecting the trace.
    private static final String PERFETTO_CONFIG_FILE_ARG = "perfetto_config_file";
    // Argument to get custom time in millisecs to wait before dumping the trace.
    // This has to be atleast the dump interval time set in the trace config file
    // or greater than that. Otherwise we will miss trace information from the test.
    private static final String PERFETTO_WAIT_TIME_ARG = "perfetto_wait_time_ms";
    // Destination directory to save the trace results.
    private static final String TEST_OUTPUT_ROOT = "test_output_root";
    // Perfetto file path key.
    private static final String PERFETTO_FILE_PATH = "perfetto_file_path";
    // Collect per run if it is set to true otherwise collect per test.
    public static final String COLLECT_PER_RUN = "per_run";
    public static final String PERFETTO_PREFIX = "perfetto_";
    // Skip failure metrics collection if this flag is set to true.
    public static final String SKIP_TEST_FAILURE_METRICS = "skip_test_failure_metrics";

    private final WakeLockContext mWakeLockContext;
    private final Supplier<WakeLock> mWakelockSupplier;
    private final WakeLockAcquirer mWakeLockAcquirer;
    private final WakeLockReleaser mWakeLockReleaser;

    // Trace config file name to use while collecting the trace which is defaulted to
    // trace_config.pb. It can be changed via the perfetto_config_file arg.
    private String mConfigFileName;
    // Wait time can be customized based on the dump interval set in the trace config.
    private long mWaitTimeInMs;
    // Perfetto traces collected during the test will be saved under this root folder.
    private String mTestOutputRoot;
    // Store the method name and invocation count to create unique file name for each trace.
    private Map<String, Integer> mTestIdInvocationCount = new HashMap<>();
    private boolean mPerfettoStartSuccess = false;
    private boolean mIsConfigTextProto = false;
    private boolean mIsCollectPerRun;
    private boolean mSkipTestFailureMetrics;
    private boolean mIsTestFailed = false;

    private PerfettoHelper mPerfettoHelper = new PerfettoHelper();

    // For USB disconnected cases you may want this option to be true. This option makes sure
    // the device does not go to sleep while collecting.
    @VisibleForTesting
    static final String HOLD_WAKELOCK_WHILE_COLLECTING = "hold_wakelock_while_collecting";

    private boolean mHoldWakelockWhileCollecting;

    public PerfettoListener() {
        super();
        mWakeLockContext = this::runWithWakeLock;
        mWakelockSupplier = this::getWakeLock;
        mWakeLockAcquirer = this::acquireWakelock;
        mWakeLockReleaser = this::releaseWakelock;
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    PerfettoListener(
            Bundle args,
            PerfettoHelper helper,
            Map invocationMap,
            WakeLockContext wakeLockContext,
            Supplier<WakeLock> wakelockSupplier,
            WakeLockAcquirer wakeLockAcquirer,
            WakeLockReleaser wakeLockReleaser) {
        super(args);
        mPerfettoHelper = helper;
        mTestIdInvocationCount = invocationMap;
        mWakeLockContext = wakeLockContext;
        mWakeLockAcquirer = wakeLockAcquirer;
        mWakeLockReleaser = wakeLockReleaser;
        mWakelockSupplier = wakelockSupplier;
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        Bundle args = getArgsBundle();

        // Whether to collect the for the entire test run or per test.
        mIsCollectPerRun = Boolean.parseBoolean(args.getString(COLLECT_PER_RUN));

        // Whether the config is text proto or not. By default set to false.
        mIsConfigTextProto = Boolean.parseBoolean(args.getString(PERFETTO_CONFIG_TEXT_PROTO));

        // Perfetto config file has to be under /data/misc/perfetto-traces/
        // defaulted to trace_config.pb is perfetto_config_file is not passed.
        mConfigFileName = args.getString(PERFETTO_CONFIG_FILE_ARG, DEFAULT_CONFIG_FILE);


        // Whether to hold wakelocks on all Prefetto tracing functions. You may want to enable
        // this if your device is not USB connected. This option prevents the device from
        // going into suspend mode while this listener is running intensive tasks.
        mHoldWakelockWhileCollecting =
                Boolean.parseBoolean(args.getString(HOLD_WAKELOCK_WHILE_COLLECTING));

        // Wait time before stopping the perfetto trace collection after the test
        // is completed. Defaulted to 3000 msecs if perfetto_wait_time_ms is not passed.
        // TODO: b/118122395 for parsing failures.
        mWaitTimeInMs =
                Long.parseLong(args.getString(PERFETTO_WAIT_TIME_ARG, DEFAULT_WAIT_TIME_MSECS));

        // Destination folder in the device to save all the trace files.
        // Defaulted to /sdcard/test_results if test_output_root is not passed.
        mTestOutputRoot = args.getString(TEST_OUTPUT_ROOT, DEFAULT_OUTPUT_ROOT);

        // By default this flag is set to false to collect the metrics on test failure.
        mSkipTestFailureMetrics = "true".equals(args.getString(SKIP_TEST_FAILURE_METRICS));

        if (!mIsCollectPerRun) {
            return;
        }

        Runnable task =
                () -> {
                    Log.i(getTag(), "Starting perfetto before test run started.");
                    startPerfettoTracing();
                };

        if (mHoldWakelockWhileCollecting) {
            Log.d(getTag(), "Holding a wakelock at onTestRunSTart.");
            mWakeLockContext.run(task);
        } else {
            task.run();
        }
    }

    @Override
    public void onTestStart(DataRecord testData, Description description) {
        mIsTestFailed = false;
        if (mIsCollectPerRun) {
            return;
        }

        Runnable task =
                () -> {
                    mTestIdInvocationCount.compute(
                            getTestFileName(description),
                            (key, value) -> (value == null) ? 1 : value + 1);
                    Log.i(getTag(), "Starting perfetto before test started.");
                    startPerfettoTracing();
                };

        if (mHoldWakelockWhileCollecting) {
            Log.d(getTag(), "Holding a wakelock at onTestStart.");
            mWakeLockContext.run(task);
        } else {
            task.run();
        }
    }

    @Override
    public void onTestFail(DataRecord testData, Description description, Failure failure) {
        mIsTestFailed = true;
    }

    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        if (mIsCollectPerRun) {
            return;
        }

        if (!mPerfettoStartSuccess) {
            Log.i(
                    getTag(),
                    "Skipping perfetto stop attempt onTestEnd because perfetto did not "
                            + "start successfully.");
            return;
        }

        Runnable task = null;
        if (mSkipTestFailureMetrics && mIsTestFailed) {
            Log.i(getTag(), "Skipping the metric collection due to test failure.");
            // Stop the existing perfetto trace collection.
            try {
                if (!mPerfettoHelper.stopPerfetto()) {
                    Log.e(getTag(), "Failed to stop the perfetto process.");
                }
            } catch (IOException e) {
                Log.e(getTag(), "Failed to stop the perfetto.", e);
            }
        } else {
            task =
                    () -> {
                        Log.i(getTag(), "Stopping perfetto after test ended.");
                        // Construct test output directory in the below format
                        // <root_folder>/<test_name>/PerfettoListener/<test_name>-<count>.pb
                        Path path =
                                Paths.get(
                                        mTestOutputRoot,
                                        getTestFileName(description),
                                        this.getClass().getSimpleName(),
                                        String.format(
                                                "%s%s-%d.pb",
                                                PERFETTO_PREFIX,
                                                getTestFileName(description),
                                                mTestIdInvocationCount.get(
                                                        getTestFileName(description))));
                        stopPerfettoTracing(path, testData);
                    };
            if (mHoldWakelockWhileCollecting) {
                Log.d(getTag(), "Holding a wakelock at onTestEnd.");
                mWakeLockContext.run(task);
            } else {
                task.run();
            }
        }
    }

    @Override
    public void onTestRunEnd(DataRecord runData, Result result) {
        if (!mIsCollectPerRun) {
            return;
        }
        if (!mPerfettoStartSuccess) {
            Log.i(
                    getTag(),
                    "Skipping perfetto stop attempt because perfetto did not "
                            + "start successfully.");
            return;
        }

        Runnable task =
                () -> {
                    Log.i(getTag(), "Stopping perfetto after test run ended.");
                    // Construct test output directory in the below format
                    // <root_folder>/PerfettoListener/<randomUUID>.pb
                    Path path =
                            Paths.get(
                                    mTestOutputRoot,
                                    this.getClass().getSimpleName(),
                                    String.format(
                                            "%s%d.pb",
                                            PERFETTO_PREFIX, UUID.randomUUID().hashCode()));
                    stopPerfettoTracing(path, runData);
                };

        if (mHoldWakelockWhileCollecting) {
            Log.d(getTag(), "Holding a wakelock at onTestRunEnd.");
            mWakeLockContext.run(task);
        } else {
            task.run();
        }
    }

    @VisibleForTesting
    void runWithWakeLock(Runnable runnable) {
        WakeLock wakelock = null;
        try {
            wakelock = mWakelockSupplier.get();
            mWakeLockAcquirer.acquire(wakelock);
            runnable.run();
        } finally {
            mWakeLockReleaser.release(wakelock);
        }
    }

    interface WakeLockContext {
        void run(Runnable runnable);
    }

    interface WakeLockAcquirer {
        void acquire(WakeLock wakelock);
    }

    interface WakeLockReleaser {
        void release(WakeLock wakelock);
    }

    @VisibleForTesting
    public void acquireWakelock(WakeLock wakelock) {
        if (wakelock != null) {
            Log.d(getTag(), "wakelock.isHeld: " + wakelock.isHeld());
            Log.d(getTag(), "acquiring wakelock.");
            wakelock.acquire();
            Log.d(getTag(), "wakelock acquired.");
            Log.d(getTag(), "wakelock.isHeld: " + wakelock.isHeld());
        }
    }

    @VisibleForTesting
    public void releaseWakelock(WakeLock wakelock) {
        if (wakelock != null) {
            Log.d(getTag(), "wakelock.isHeld: " + wakelock.isHeld());
            Log.d(getTag(), "releasing wakelock.");
            wakelock.release();
            Log.d(getTag(), "wakelock released.");
            Log.d(getTag(), "wakelock.isHeld: " + wakelock.isHeld());
        }
    }

    private WakeLock getWakeLock() {
        PowerManager pm =
                (PowerManager)
                        getInstrumentation().getContext().getSystemService(Context.POWER_SERVICE);

        return pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, PerfettoListener.class.getName());
    }

    /**
     * Start perfetto tracing using the given config file.
     */
    private void startPerfettoTracing() {
        mPerfettoStartSuccess = mPerfettoHelper.startCollecting(mConfigFileName,
                mIsConfigTextProto);
        if (!mPerfettoStartSuccess) {
            Log.e(getTag(), "Perfetto did not start successfully.");
        }
    }

    /**
     * Stop perfetto tracing and dumping the collected trace file in given path and updating the
     * record with the path to the trace file.
     */
    private void stopPerfettoTracing(Path path, DataRecord record) {
        if (!mPerfettoHelper.stopCollecting(mWaitTimeInMs, path.toString())) {
            Log.e(getTag(), "Failed to collect the perfetto output.");
        } else {
            record.addStringMetric(PERFETTO_FILE_PATH, path.toString());
        }
    }

    /**
     * Returns the packagename.classname_methodname which has no special characters and used to
     * create file names.
     */
    public static String getTestFileName(Description description) {
        return String.format("%s_%s", description.getClassName(), description.getMethodName());
    }
}
