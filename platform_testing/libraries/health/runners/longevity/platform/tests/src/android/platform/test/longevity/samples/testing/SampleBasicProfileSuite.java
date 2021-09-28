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

import android.platform.test.longevity.ProfileSuite;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.Suite.SuiteClasses;

@RunWith(ProfileSuite.class)
@SuiteClasses({
    SampleBasicProfileSuite.PassingTest1.class,
    SampleBasicProfileSuite.PassingTest2.class,
})
/** Sample device-side test cases using a profile. */
public class SampleBasicProfileSuite {

    @Scenario
    @RunWith(JUnit4.class)
    public static class PassingTest1 {
        @Test
        public void testPassing() {
            Assert.assertEquals(1, 1);
        }
    }

    @Scenario
    @RunWith(JUnit4.class)
    public static class PassingTest2 {
        @Test
        public void testPassing() {
            Assert.assertEquals(2, 2);
        }
    }
}
