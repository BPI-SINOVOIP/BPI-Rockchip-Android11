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
package android.device.collectors;

import android.device.collectors.util.SendToInstrumentation;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import androidx.annotation.VisibleForTesting;

import com.android.helpers.ICollectorHelper;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

import org.junit.runner.Description;
import org.junit.runner.Result;

/**
 * Extend this class for a periodic metric collection which relies on ICollectorHelper to collect
 * metrics and dump the time-series in csv format. In case of system crashes, the time series up to
 * the point where the crash happened will still be stored.
 *
 * In case of running tests with Tradefed file pulller, use the option
 * {@link file-puller-log-collector:directory-keys} from {{@link FilePullerLogCollector} to
 * specify the directory path under which the output file should be pulled from (i.e.
 * <external_storage>/test_results, where <external_storage> is /sdcard for Android phones and
 * /storage/emulated/10 for Android Auto), instead of using
 * {@link file-puller-log-collector:pull-pattern-keys}.
 */
public class ScheduledRunCollectionListener<T extends Number> extends ScheduledRunMetricListener {
    private static final String LOG_TAG = ScheduledRunCollectionListener.class.getSimpleName();
    private static final String TIME_SERIES_PREFIX = "time_series_";
    @VisibleForTesting public static final String OUTPUT_ROOT = "test_results";
    @VisibleForTesting public static final String OUTPUT_FILE_PATH = "%s_time_series_path";

    @VisibleForTesting
    public static final String TIME_SERIES_HEADER =
            String.format("%-20s,%-100s,%-20s", "time", "metric_key", "value");

    private static final String TIME_SERIES_BODY = "%-20d,%-100s,%-20s";
    @VisibleForTesting public static final String MEAN_SUFFIX = "-mean";
    @VisibleForTesting public static final String MAX_SUFFIX = "-max";
    @VisibleForTesting public static final String MIN_SUFFIX = "-min";

    protected ICollectorHelper<T> mHelper;
    private TimeSeriesCsvWriter mTimeSeriesCsvWriter;
    private TimeSeriesStatistics mTimeSeriesStatistics;
    private long mStartTime;

    public ScheduledRunCollectionListener() {}

    @VisibleForTesting
    ScheduledRunCollectionListener(Bundle argsBundle, ICollectorHelper helper) {
        super(argsBundle);
        mHelper = helper;
    }

    /**
     * Write a time-series in csv format to the given destination under external storage as an
     * unpivoted table like:
     *
     * time  ,metric_key ,value
     * 0     ,metric1    ,5
     * 0     ,metric2    ,10
     * 0     ,metric3    ,15
     * 1000  ,metric1    ,6
     * 1000  ,metric2    ,11
     * 1000  ,metric3    ,16
     */
    private class TimeSeriesCsvWriter {
        private File mDestFile;
        private boolean mIsHeaderWritten = false;

        private TimeSeriesCsvWriter(Path destination) {
            // Create parent directory if it doesn't exist.
            File destDir = createAndEmptyDirectory(destination.getParent().toString());
            mDestFile = new File(destDir, destination.getFileName().toString());
        }

        private void write(Map<String, T> dataPoint, long timeStamp) {
            try (BufferedWriter writer = new BufferedWriter(new FileWriter(mDestFile, true))) {
                if (!mIsHeaderWritten) {
                    writer.append(TIME_SERIES_HEADER);
                    writer.append("\n");
                    mIsHeaderWritten = true;
                }

                for (String key : dataPoint.keySet()) {
                    writer.append(
                            String.format(TIME_SERIES_BODY, timeStamp, key, dataPoint.get(key)));
                    writer.append("\n");
                }
            } catch (IOException e) {
                Log.e(
                        LOG_TAG,
                        String.format("Fail to output time series due to : %s.", e.getMessage()));
            }
        }
    }

    private class TimeSeriesStatistics {
        Map<String, T> minMap = new HashMap<>();
        Map<String, T> maxMap = new HashMap<>();
        Map<String, Double> sumMap = new HashMap<>();
        Map<String, Long> countMap = new HashMap<>();

        private void update(Map<String, T> dataPoint) {
            for (String key : dataPoint.keySet()) {
                T value = dataPoint.get(key);
                // Add / replace min.
                minMap.computeIfPresent(key, (k, v) -> compareAsDouble(value, v) == -1 ? value : v);
                minMap.computeIfAbsent(key, k -> value);
                // Add / replace max.
                maxMap.computeIfPresent(key, (k, v) -> compareAsDouble(value, v) == 1 ? value : v);
                maxMap.computeIfAbsent(key, k -> value);
                // Add / update sum.
                sumMap.put(key, value.doubleValue() + sumMap.getOrDefault(key, 0.));
                // Add / update count.
                countMap.put(key, 1 + countMap.getOrDefault(key, 0L));
            }
        }

        private Map<String, String> getStatistics() {
            Map<String, String> res = new HashMap<>();
            for (String key : minMap.keySet()) {
                res.put(key + MIN_SUFFIX, minMap.get(key).toString());
            }
            for (String key : maxMap.keySet()) {
                res.put(key + MAX_SUFFIX, maxMap.get(key).toString());
            }
            for (String key : sumMap.keySet()) {
                if (countMap.containsKey(key)) {
                    double mean = sumMap.get(key) / countMap.get(key);
                    res.put(key + MEAN_SUFFIX, Double.toString(mean));
                }
            }
            return res;
        }

        /** Compare to Number objects. Return -1 if the n1 < n2; 0 if n1 == n2; 1 if n1 > n2. */
        private int compareAsDouble(Number n1, Number n2) {
            Double d1 = Double.valueOf(n1.doubleValue());
            Double d2 = Double.valueOf(n2.doubleValue());
            return d1.compareTo(d2);
        }
    }

    /** {@inheritDoc} */
    @Override
    void onStart(DataRecord runData, Description description) {
        setupAdditionalArgs();
        Path path =
                Paths.get(
                        OUTPUT_ROOT,
                        getClass().getSimpleName(),
                        String.format(
                                "%s%s-%d.csv",
                                TIME_SERIES_PREFIX,
                                getClass().getSimpleName(),
                                UUID.randomUUID().hashCode()));
        mTimeSeriesCsvWriter = new TimeSeriesCsvWriter(path);
        mTimeSeriesStatistics = new TimeSeriesStatistics();
        mStartTime = SystemClock.uptimeMillis();
        mHelper.startCollecting();
        // Send to stdout the path where the time-series files will be stored.
        Bundle filePathBundle = new Bundle();
        filePathBundle.putString(
                String.format(OUTPUT_FILE_PATH, getClass().getSimpleName()),
                mTimeSeriesCsvWriter.mDestFile.toString());
        SendToInstrumentation.sendBundle(getInstrumentation(), filePathBundle);
    }

    /** {@inheritDoc} */
    @Override
    void onEnd(DataRecord runData, Result result) {
        mHelper.stopCollecting();
        for (Map.Entry<String, String> entry : mTimeSeriesStatistics.getStatistics().entrySet()) {
            runData.addStringMetric(entry.getKey(), entry.getValue());
        }
    }

    /** {@inheritDoc} */
    @Override
    public void collect(DataRecord runData, Description description) throws InterruptedException {
        long timeStamp = SystemClock.uptimeMillis() - mStartTime;
        Map<String, T> dataPoint = mHelper.getMetrics();
        mTimeSeriesCsvWriter.write(dataPoint, timeStamp);
        mTimeSeriesStatistics.update(dataPoint);
    }

    /**
     * To add listener specific extra args implement this method in the sub class and add the
     * listener specific args.
     */
    public void setupAdditionalArgs() {
        // NO-OP by default
    }

    protected void createHelperInstance(ICollectorHelper helper) {
        mHelper = helper;
    }
}

