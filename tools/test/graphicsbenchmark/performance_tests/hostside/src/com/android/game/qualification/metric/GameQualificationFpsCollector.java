/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.metric;

import static com.android.game.qualification.metric.MetricSummary.TimeType.PRESENT;
import static com.android.game.qualification.metric.MetricSummary.TimeType.READY;

import com.android.annotations.VisibleForTesting;
import com.android.game.qualification.proto.ResultDataProto;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;

import com.google.common.base.Preconditions;

import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

/**
 * A {@link com.android.tradefed.device.metric.IMetricCollector} to collect FPS data.
 */
public class GameQualificationFpsCollector extends GameQualificationScheduledMetricCollector {
    private long mLatestSeen = 0;
    private long mVSyncPeriod = 0;
    private List<GameQualificationMetric> mElapsedTimes = new ArrayList<>();
    private File mRawFile;
    private Pattern mLayerPattern;
    private String mTestLayer;
    private boolean mAppStarted;

    public GameQualificationFpsCollector() {
        mIntervalMs = 1000L;
    }

    @VisibleForTesting
    List<GameQualificationMetric> getElapsedTimes() {
        return mElapsedTimes;
    }

    @Override
    protected void doStart(DeviceMetricData runData) {
        if (!isEnabled()) {
            return;
        }
        Preconditions.checkState(getApkInfo() != null);
        CLog.v("Test run started on device %s.", mDevice);

        try {
            mRawFile = File.createTempFile("GameQualification_RAW_TIMES", ".txt");
        } catch (IOException e) {
            setErrorMessage("Failed creating file to store raw FPS data.");
            throw new RuntimeException(e);
        }

        mElapsedTimes.clear();
        mLatestSeen = 0;
        mAppStarted = false;
        setErrorMessage(
                "Unable to retrieve any metrics.  App might not have started or the target "
                        + "layer name did not exists.");
        setHasError(true);

        try {
            mLayerPattern = Pattern.compile(getApkInfo().getLayerName());
        } catch (PatternSyntaxException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Task periodically & asynchronously run during the test running.
     */
    protected void collect() {
        try {
            String[] raw = getRawData();
            processRawData(raw);
        } catch (DeviceNotAvailableException e) {
            setErrorMessage(
                    "Device not available during FPS data collection: " + e.getMessage());
            throw new RuntimeException(e);
        }
    }

    private String[] getRawData() throws DeviceNotAvailableException {
        if (!mAppStarted) {
            String listCmd = "dumpsys SurfaceFlinger --list";
            String[] layerList = mDevice.executeShellCommand(listCmd).split("\n");

            for (String layer : layerList) {
                Matcher m = mLayerPattern.matcher(layer);
                if (m.matches()) {
                    mTestLayer = layer;
                    break;
                }
            }
        }
        CLog.d("Collecting benchmark stats for layer: %s", mTestLayer);


        String cmd = "dumpsys SurfaceFlinger --latency \"" + mTestLayer+ "\"";
        return mDevice.executeShellCommand(cmd).split("\n");
    }

    @VisibleForTesting
    void processRawData(String[] raw) {
        if (raw.length == 1) {
            if (mAppStarted) {
                throw new RuntimeException("App was terminated");
            } else {
                return;
            }
        }

        if (!mAppStarted) {
            mVSyncPeriod = Long.parseLong(raw[0]);
            mAppStarted = true;
            setHasError(false);
            setErrorMessage("");
        }

        try (BufferedWriter outputFile = new BufferedWriter(new FileWriter(mRawFile, true))) {
            outputFile.write("Vsync: " + raw[0] + "\n");
            outputFile.write("Latest Seen: " + mLatestSeen + "\n");

            outputFile.write(String.format("%20s", "Desired Present Time") + "\t");
            outputFile.write(String.format("%20s", "Actual Present Time") + "\t");
            outputFile.write(String.format("%20s", "Frame Ready Time") + "\n");

            boolean overlap = false;
            for (int i = 1; i < raw.length; i++) {
                String[] parts = raw[i].split("\t");

                if (parts.length == 3) {
                    if (sample(Long.parseLong(parts[2]), Long.parseLong(parts[1]))) {
                        overlap = true;
                    }
                }

                outputFile.write(String.format("%20d", Long.parseLong(parts[0])) + "\t");
                outputFile.write(String.format("%20d", Long.parseLong(parts[1])) + "\t");
                outputFile.write(String.format("%20d", Long.parseLong(parts[2])) + "\n");
            }

            if (!overlap) {
                CLog.e("No overlap with previous poll, we missed some frames!"); // FIND SOMETHING BETTER
            }

            outputFile.write("\n\n");
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }


    private boolean sample(long readyTimeStamp, long presentTimeStamp) {
        if (presentTimeStamp == Long.MAX_VALUE || readyTimeStamp == Long.MAX_VALUE) {
            return false;
        }
        else if (presentTimeStamp < mLatestSeen) {
            return false;
        }
        else if (presentTimeStamp == mLatestSeen) {
            return true;
        }
        else {
            mElapsedTimes.add(new GameQualificationMetric(presentTimeStamp, readyTimeStamp));
            mLatestSeen = presentTimeStamp;
            return false;
        }
    }


    private void processTimestampsSlice(
            MetricSummary.Builder summary,
            int runIndex,
            long startTimestamp,
            long endTimestamp,
            BufferedWriter outputFile) throws IOException {
        outputFile.write("Loop " + runIndex + " timestamp: " + startTimestamp + " ns\n");
        outputFile.write("Present Time (ms)\tFrame Ready Time (ms)\n");

        long prevPresentTime = 0, prevReadyTime = 0;

        List<Long> frameTimes = new ArrayList<>();

        summary.beginLoop();
        for(GameQualificationMetric metric : mElapsedTimes)
        {
            long presentTime = metric.getActualPresentTime();
            long readyTime = metric.getFrameReadyTime();

            if (presentTime < startTimestamp) {
                continue;
            }
            if (presentTime > endTimestamp) {
                break;
            }

            if (prevPresentTime == 0) {
                prevPresentTime = presentTime;
                prevReadyTime = readyTime;
                continue;
            }

            long presentTimeDiff = presentTime - prevPresentTime;
            prevPresentTime = presentTime;
            summary.addFrameTime(PRESENT, presentTimeDiff);

            long readyTimeDiff = readyTime - prevReadyTime;
            prevReadyTime = readyTime;
            summary.addFrameTime(READY, readyTimeDiff);

            outputFile.write(
                    String.format(
                            "%d.%06d\t\t%d.%06d\n",
                            presentTimeDiff / 1000000,
                            presentTimeDiff % 1000000,
                            readyTimeDiff / 1000000,
                            readyTimeDiff % 1000000));
            frameTimes.add(presentTimeDiff);
        }
        summary.endLoop();
        printHistogram(frameTimes, runIndex);
    }

    @Override
    protected void doEnd(DeviceMetricData runData) {
        if (mElapsedTimes.isEmpty()) {
            return;
        }
        try {
            try(InputStreamSource rawData = new FileInputStreamSource(mRawFile, true)) {
                testLog("RAW-" + getApkInfo().getName(), LogDataType.TEXT, rawData);
            }

            mRawFile.delete();

            File tmpFile = File.createTempFile("GameQualification-frametimes", ".txt");
            try (BufferedWriter outputFile = new BufferedWriter(new FileWriter(tmpFile))) {
                MetricSummary.Builder summaryBuilder =
                        new MetricSummary.Builder(mCertificationRequirements, mVSyncPeriod);

                long startTime = 0L;
                int runIndex = 0;

                // Calculate load time.
                long appLaunchedTime = 0;
                for (ResultDataProto.Event e : mDeviceResultData.getEventsList()) {
                    if (e.getType() == ResultDataProto.Event.Type.APP_LAUNCH) {
                        appLaunchedTime = e.getTimestamp();
                        continue;
                    }
                    // Get the first START_LOOP.  Assume START_LOOP is in chronological order
                    // and comes after APP_LAUNCH.
                    if (e.getType() == ResultDataProto.Event.Type.START_LOOP) {
                        summaryBuilder.setLoadTimeMs(e.getTimestamp() - appLaunchedTime);
                        break;
                    }
                }
                for (ResultDataProto.Event e : mDeviceResultData.getEventsList()) {
                    if (e.getType() != ResultDataProto.Event.Type.START_LOOP) {
                        continue;
                    }

                    long endTime = e.getTimestamp() * 1000000;  /* ms to ns */

                    if (startTime != 0) {
                        processTimestampsSlice(summaryBuilder, runIndex++, startTime, endTime, outputFile);
                    }
                    startTime = endTime;
                }

                processTimestampsSlice(
                        summaryBuilder,
                        runIndex,
                        startTime,
                        mElapsedTimes.get(mElapsedTimes.size() - 1).getActualPresentTime(),
                        outputFile);

                MetricSummary summary = summaryBuilder.build();
                summary.addToMetricData(runData);
                outputFile.flush();
                try(InputStreamSource source = new FileInputStreamSource(tmpFile, true)) {
                    testLog("GameQualification-frametimes-" + getApkInfo().getName(), LogDataType.TEXT, source);
                }
            }
            tmpFile.delete();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    void printHistogram(Collection<Long> frameTimes, int runIndex) {
        try(ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            Histogram histogram =
                    new Histogram(frameTimes, mVSyncPeriod / 30L, null, 5 * mVSyncPeriod);
            histogram.plotAscii(output, 100);
            try(InputStreamSource source = new ByteArrayInputStreamSource(output.toByteArray())) {
                testLog(
                        "GameQualification-histogram-" + getApkInfo().getName() + "-run" + runIndex,
                        LogDataType.TEXT,
                        source);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
