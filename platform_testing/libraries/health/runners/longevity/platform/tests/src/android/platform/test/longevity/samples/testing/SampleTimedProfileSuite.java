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

package android.platform.test.longevity.samples.testing;

import android.os.SystemClock;
import android.platform.test.longevity.ProfileSuite;
import android.platform.test.scenario.annotation.Scenario;

import java.util.concurrent.TimeUnit;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.Suite.SuiteClasses;

@RunWith(ProfileSuite.class)
@SuiteClasses({
    SampleTimedProfileSuite.LongIdleTest.class,
    SampleTimedProfileSuite.PassingTest.class,
})
/** Sample device-side test cases using a profile. */
public class SampleTimedProfileSuite {
    @Scenario
    @RunWith(JUnit4.class)
    public static class LongIdleTest {
        @Test
        public void testLongIdle() {
            SystemClock.sleep(TimeUnit.SECONDS.toMillis(10));
        }

        // Simulates a quick teardown step.
        @AfterClass
        public static void dummyTearDown() {
            SystemClock.sleep(100);
        }
    }

    @Scenario
    @RunWith(JUnit4.class)
    public static class PassingTest {
        @Test
        public void testPassing() {
            Assert.assertEquals(1, 1);
        }

        // Simulates a quick teardown step.
        @AfterClass
        public static void dummyTearDown() {
            SystemClock.sleep(100);
        }
    }
}
