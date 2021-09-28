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
package com.android.tv.ui.hideable;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import java.util.concurrent.TimeUnit;

/** Test for {@link AutoHideScheduler}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class AutoHideSchedulerTest {

    private TestRunnable mTestRunnable = new TestRunnable();
    private AutoHideScheduler mAutoHideScheduler;
    private ShadowLooper mShadowLooper;

    @Before
    public void setUp() throws Exception {
        mShadowLooper = ShadowLooper.getShadowMainLooper();
        mAutoHideScheduler = new AutoHideScheduler(RuntimeEnvironment.application, mTestRunnable);
    }

    @Test
    public void initialState() {
        assertThat(mAutoHideScheduler.isScheduled()).isFalse();
    }

    @Test
    public void cancel() {
        mAutoHideScheduler.cancel();
        assertThat(mAutoHideScheduler.isScheduled()).isFalse();
    }

    @Test
    public void schedule() {
        mAutoHideScheduler.schedule(10);
        assertThat(mAutoHideScheduler.isScheduled()).isTrue();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
    }

    @Test
    public void setA11yEnabledThenSchedule() {
        mAutoHideScheduler.onAccessibilityStateChanged(true);
        mAutoHideScheduler.schedule(10);
        assertThat(mAutoHideScheduler.isScheduled()).isFalse();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
    }

    @Test
    public void scheduleThenCancel() {
        mAutoHideScheduler.schedule(10);
        assertThat(mAutoHideScheduler.isScheduled()).isTrue();
        mAutoHideScheduler.cancel();
        assertThat(mAutoHideScheduler.isScheduled()).isFalse();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
    }

    @Test
    public void scheduleThenLoop() {
        mAutoHideScheduler.schedule(10);
        assertThat(mAutoHideScheduler.isScheduled()).isTrue();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
        mShadowLooper.idle(9, TimeUnit.MILLISECONDS);
        assertThat(mAutoHideScheduler.isScheduled()).isTrue();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
        mShadowLooper.idle(1, TimeUnit.MILLISECONDS);
        assertThat(mAutoHideScheduler.isScheduled()).isFalse();
        assertThat(mTestRunnable.runCount).isEqualTo(1);
    }

    @Test
    public void scheduleSetA11yEnabledThenLoop() {
        mAutoHideScheduler.schedule(10);
        assertThat(mAutoHideScheduler.isScheduled()).isTrue();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
        mShadowLooper.idle(9, TimeUnit.MILLISECONDS);
        assertThat(mAutoHideScheduler.isScheduled()).isTrue();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
        mAutoHideScheduler.onAccessibilityStateChanged(true);
        mShadowLooper.idle(1, TimeUnit.MILLISECONDS);
        assertThat(mAutoHideScheduler.isScheduled()).isFalse();
        assertThat(mTestRunnable.runCount).isEqualTo(0);
    }

    private static class TestRunnable implements Runnable {
        int runCount = 0;

        @Override
        public void run() {
            runCount++;
        }
    }
}
