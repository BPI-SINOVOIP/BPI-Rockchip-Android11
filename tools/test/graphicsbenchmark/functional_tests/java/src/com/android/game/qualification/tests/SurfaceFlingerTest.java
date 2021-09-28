/*
 * Copyright (C) 2019 The Android Open Source Project
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

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assume.assumeTrue;
import static com.google.common.truth.Truth.assertWithMessage;

/**
 * Tests related to the surface flinger.
 */
@RunWith(AndroidJUnit4.class)
public class SurfaceFlingerTest {

    @Rule
    public final ActivityTestRule<SurfaceFlingerTestActivity> mActivityRule =
            new ActivityTestRule<>(SurfaceFlingerTestActivity.class, false, false);

    @Before
    public void setUp() {
        TestUtils.assumeGameCoreCertified(InstrumentationRegistry.getContext().getPackageManager());
    }

    /**
     * Check surface flinger does not latch a frame before the GPU work is completed.
     *
     * Latching a frame before GPU work is completed can lead to unpredictable delays in the HWC
     * that is impossible to detect in the app.
     */
    @Test
    public void latchAfterReady() throws InterruptedException {
        mActivityRule.launchActivity(null);
        // Let the activity run for a few seconds.
        Thread.sleep(3000);
        SurfaceFlingerTestActivity activity = mActivityRule.getActivity();
        mActivityRule.finishActivity();
        activity.waitUntilDestroyed();

        Long[] readyTimes = activity.getReadyTimes().toArray(new Long[0]);
        Long[] latchTimes = activity.getLatchTimes().toArray(new Long[0]);

        assertWithMessage("Unable to retrieve frame ready time.")
                .that(readyTimes)
                .isNotEmpty();

        assertWithMessage("Unable to retrieve buffer latch time.")
                .that(latchTimes)
                .isNotEmpty();

        for (int i = 0; i < readyTimes.length; i++) {
            // Check all frame ready time is before latch time.  Add a slight (0.1ms) tolerance
            // because the latch time is slightly before the actual condition check in the
            // surface flinger.
            assertWithMessage(
                    "SurfaceFlinger must latch after GPU work is completed for the frame")
                    .that(latchTimes[i])
                    .named("latch time")
                    .isGreaterThan(readyTimes[i] - 100_000);
        }
    }
}
