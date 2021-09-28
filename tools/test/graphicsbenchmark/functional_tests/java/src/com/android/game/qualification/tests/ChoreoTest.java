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

package com.android.game.qualification.tests;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.InstrumentationRegistry;

import com.android.game.qualification.tests.ChoreoTestActivity;
import com.google.common.collect.Range;

import java.util.ArrayList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.Rule;

import static com.google.common.truth.Truth.assertWithMessage;

/**
 * Tests related to Choreographer
 */

@RunWith(AndroidJUnit4.class)
public class ChoreoTest {

    @Rule
    public ActivityTestRule<ChoreoTestActivity> mActivityRule =
            new ActivityTestRule(ChoreoTestActivity.class, false, false);

    @Before
    public void setUp() {
        TestUtils.assumeGameCoreCertified(InstrumentationRegistry.getContext().getPackageManager());
    }

    /**
     * Check if the differences between Choreographer callbacks are not only near the refresh rate,
     * but that the data does not deviate too much from the mean.
    */
    @Test
    public void choreoCallbackTimes() throws InterruptedException {
        mActivityRule.launchActivity(null);
        Thread.sleep(3000);
        ChoreoTestActivity activity = mActivityRule.getActivity();
        mActivityRule.finishActivity();
        activity.waitUntilDestroyed();

        ArrayList<Long> intervals = activity.getFrameIntervals();

        double mean = 0;
        double min = Double.MAX_VALUE, max = 0;
        int maxIndex = 0;

        assertWithMessage(
            "Need an adequate amount of frames in order to test Choreographer")
            .that(intervals.size())
            .named("number of frames")
            .isGreaterThan(60);

        for (int i = 0; i < intervals.size(); i++) {
            long interval = intervals.get(i);

            if(interval > max) {
                max = interval;
            }

            if(interval < min) {
                min = interval;
            }

            mean += interval;
        }
        mean = mean / (intervals.size() * 1E6);

        double range = (max - min) / 1E6;

        // TODO: Handle phones with different refresh rates like 90Hz
        assertWithMessage(
            "Choreographer callbacks must occur at roughly the same pace as the refresh rate")
            .that(mean)
            .named("mean callback time")
            .isIn(Range.closed(16.45, 16.75));

        assertWithMessage(
            "Choreographer callbacks must not deviate too much from the mean")
            .that(range)
            .named("callback time range")
            .isLessThan(2.0);
    }
}
