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

import com.android.tradefed.config.Option;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement;

import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Similar to {@link com.android.tradefed.device.metric.ScheduledDeviceMetricCollector} but
 * customized to use {@link BaseGameQualificationMetricCollector}.
 */
public abstract class GameQualificationScheduledMetricCollector
        extends BaseGameQualificationMetricCollector {
    @Option(
            name = "fixed-schedule-rate",
            description = "Schedule the timetask as a fixed schedule rate"
    )
    protected boolean mFixedScheduleRate = false;

    @Option(
            name = "interval",
            description = "the interval between two tasks being scheduled",
            isTimeVal = true
    )
    protected long mIntervalMs = 60 * 1000L;

    private Timer mTimer;

    @Override
    protected final void onStart(DeviceMetricData runData) {
        if (!isEnabled()) {
            // Metric collector is only enabled by GameQualificationHostsideController.
            return;
        }
        CLog.d("starting with interval = %s", mIntervalMs);

        synchronized (this) {
            doStart(runData);
        }

        mTimer = new Timer();
        TimerTask timerTask =
                new TimerTask() {
                    @Override
                    public void run() {
                        synchronized (GameQualificationScheduledMetricCollector.this) {
                            try {
                                collect();
                            } catch (Exception e) {
                                mTimer.cancel();
                                Thread.currentThread().interrupt();
                                CLog.e("Test app '%s' terminated before data collection was completed.",
                                        getApkInfo().getName());
                                setHasError(true);
                                setErrorMessage(e.getMessage());
                            }
                        }
                    }
                };

        if (mFixedScheduleRate) {
            mTimer.scheduleAtFixedRate(timerTask, 0, mIntervalMs);
        } else {
            mTimer.schedule(timerTask, 0, mIntervalMs);
        }
    }

    @Override
    public final void onEnd(
            DeviceMetricData runData,
            Map<String, MetricMeasurement.Metric> currentRunMetrics) {
        if (!isEnabled()) {
            return;
        }

        if (mTimer != null) {
            mTimer.cancel();
            mTimer.purge();
        }
        synchronized (this) {
            doEnd(runData);
        }
        CLog.d("finished");
    }

    /**
     * Task periodically & asynchronously run during the test.
     */
    protected abstract void collect();

    /**
     * Executed when entering this collector.
     *
     * @param runData the {@link DeviceMetricData} where to put metrics.
     */
    protected void doStart(DeviceMetricData runData) {
        // Does nothing.
    }

    /**
     * Executed when finishing this collector.
     *
     * @param runData the {@link DeviceMetricData} where to put metrics.
     */
    protected void doEnd(DeviceMetricData runData) {
        // Does nothing.
    }
}
