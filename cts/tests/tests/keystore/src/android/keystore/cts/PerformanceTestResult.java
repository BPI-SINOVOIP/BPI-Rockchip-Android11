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
 * limitations under the License
 */

package android.keystore.cts;

import junit.framework.TestCase;

import java.util.Arrays;

/**
 * Simple struct to package test results. All times are in nanoseconds. Doubles are used rather than
 * longs because arithmetic on nanoseconds in longs can overflow.
 */
public class PerformanceTestResult {
    private double mSetupTime;
    private int mSampleCount = 0;
    private double[] mSamples = new double[PerformanceTestBase.TEST_ITERATION_LIMIT];
    private double mTeardownTime;

    public void addSetupTime(double timeInNs) {
        mSetupTime += timeInNs / PerformanceTestBase.MS_PER_NS;
    }

    public double getSetupTime() {
        return mSetupTime;
    }

    public void addTeardownTime(double timeInNs) {
        mTeardownTime += timeInNs / PerformanceTestBase.MS_PER_NS;
    }

    public double getTearDownTime() {
        return mTeardownTime;
    }

    public void addMeasurement(double timeInNs) {
        if (mSampleCount == PerformanceTestBase.TEST_ITERATION_LIMIT) {
            TestCase.fail("Array is full, this is a test bug.");
        }
        mSamples[mSampleCount++] = timeInNs / PerformanceTestBase.MS_PER_NS;
    }

    public int getSampleCount() {
        return mSampleCount;
    }

    public double getTotalTime() {
        double sum = 0;
        for (int i = 0; i < mSampleCount; ++i) {
            sum += mSamples[i];
        }
        return sum;
    }

    public double getTotalTimeSq() {
        double sum = 0;
        for (int i = 0; i < mSampleCount; ++i) {
            sum += (double) mSamples[i] * mSamples[i];
        }
        return sum;
    }

    public double getMedian() {
        return getPercentile(0.5);
    }

    public double getPercentile(double v) {
        Arrays.sort(mSamples, 0, mSampleCount);
        return mSamples[(int) Math.ceil(mSampleCount * v) - 1];
    }

    public double getMean() {
        return getTotalTime() / mSampleCount;
    }

    public double getSampleStdDev() {
        double totalTime = getTotalTime();
        return Math.sqrt(mSampleCount * getTotalTimeSq() - totalTime * totalTime)
                / (mSampleCount * (mSampleCount - 1));
    }

    @Override
    public String toString() {
        return mSampleCount
                + ","
                + getMean()
                + ","
                + getSampleStdDev()
                + ","
                + getMedian()
                + ","
                + getPercentile(0.9);
    }
}
