/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.suspensionchecker;

import static com.google.common.truth.Truth.assertThat;


import static org.junit.Assert.fail;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;

/**
 * Class to test whether activities from this package can be launched and not suspended.
 */
@RunWith(AndroidJUnit4.class)
public class ActivityLaunchTest {
    @Test
    public void testCanStartActivity() throws Exception {
        assertThat(LaunchCheckingActivity.checkLaunch(InstrumentationRegistry.getContext()))
                .isTrue();
    }

    @Test
    public void testCannotStartActivity() throws Exception {
        assertThat(LaunchCheckingActivity.checkLaunch(InstrumentationRegistry.getContext()))
                .isFalse();
    }

    @Test
    public void testWaitForActivityNotLaunchable() throws Exception {
        // Wait up to 1 minute checking if the app is not launchable anymore.
        final long deadline = System.nanoTime() + TimeUnit.MINUTES.toNanos(1);
        while (System.nanoTime() < deadline) {
            if (!LaunchCheckingActivity.checkLaunch(InstrumentationRegistry.getContext())) {
                return;
            }
            Thread.sleep(1000);
        }
        fail("Activity is still launchable");
    }
}
