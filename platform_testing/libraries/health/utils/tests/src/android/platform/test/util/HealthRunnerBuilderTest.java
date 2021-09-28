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
package android.platform.test.util;

import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.Runner;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;

/** Unit tests for {@link HealthRunnerBuilder}. */
@RunWith(JUnit4.class)
public class HealthRunnerBuilderTest {
    // A dummy test class.
    @RunWith(JUnit4.class)
    public static class SampleTest {
        @Test
        public void testMethod() {
            // No-op so passes.
        }
    }

    // A dummy runner class.
    public static class SampleRunner extends BlockJUnit4ClassRunner {
        public SampleRunner(Class<?> testClass) throws InitializationError {
            super(testClass);
        }
    }

    @Rule public ExpectedException mExpectedException = ExpectedException.none();

    /** Test that the runner builder returns a {@link JUnit4} runner by default. */
    @Test
    public void testDefaultsToJUnit4() throws Throwable {
        Runner runner = (new HealthRunnerBuilder(new Bundle())).runnerForClass(SampleTest.class);
        assertTrue(runner instanceof JUnit4);
    }

    /** Test that the runner builder returns the specified, valid runner. */
    @Test
    public void testUsingSpecifiedRunner_valid() throws Throwable {
        Bundle args = new Bundle();
        args.putString(HealthRunnerBuilder.RUNNER_OPTION, SampleRunner.class.getName());
        Runner runner = (new HealthRunnerBuilder(args)).runnerForClass(SampleTest.class);
        assertTrue(runner instanceof SampleRunner);
    }

    /** Test that the runner throws when the supplied argument is not a class. */
    @Test
    public void testUsingSpecifiedRunner_invalid_notClass() throws Throwable {
        mExpectedException.expectMessage("Could not find class");
        Bundle args = new Bundle();
        args.putString(HealthRunnerBuilder.RUNNER_OPTION, "notAClass");
        Runner runner = (new HealthRunnerBuilder(args)).runnerForClass(SampleTest.class);
    }

    /** Test that the runner throws when the supplied argument is not a {@link Runner}. */
    @Test
    public void testUsingSpecifiedRunner_invalid_notRunner() throws Throwable {
        mExpectedException.expectMessage("not a runner");
        Bundle args = new Bundle();
        args.putString(HealthRunnerBuilder.RUNNER_OPTION, HealthRunnerBuilderTest.class.getName());
        Runner runner = (new HealthRunnerBuilder(args)).runnerForClass(SampleTest.class);
    }
}
