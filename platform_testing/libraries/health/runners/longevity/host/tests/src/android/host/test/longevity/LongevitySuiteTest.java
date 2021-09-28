/*
 * Copyright (C) 2017 The Android Open Source Project
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
package android.host.test.longevity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.host.test.longevity.listener.TimeoutTerminator;

import java.util.HashMap;
import java.util.Map;

import org.junit.Test;
import org.junit.internal.builders.AllDefaultPossibilitiesBuilder;
import org.junit.runner.RunWith;
import org.junit.runner.notification.RunNotifier;
import org.junit.runner.notification.StoppedByUserException;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;
import org.junit.runners.Suite.SuiteClasses;

/**
 * Unit tests for the {@link LongevitySuite} runner.
 */
@RunWith(JUnit4.class)
public class LongevitySuiteTest {
    private static final String ITERATIONS_OPTION_NAME = "iterations";

    /**
     * Unit test that the {@link SuiteClasses} annotation is required.
     */
    @Test
    public void testAnnotationRequired() {
        try {
            new LongevitySuite(NoSuiteClassesSuite.class, new AllDefaultPossibilitiesBuilder(true));
            fail("This suite should not be possible to construct.");
        } catch (InitializationError e) {
            // ignore and pass.
        }
    }

    @RunWith(LongevitySuite.class)
    private static class NoSuiteClassesSuite { }

    /**
     * Tests that test runs are considered passing if the invalidate option is set to false.
     */
    @Test
    public void testDoNotInvalidateTestRuns() throws InitializationError {
        Map<String, String> args = new HashMap();
        args.put(LongevitySuite.INVALIDATE_OPTION, "false");
        args.put(LongevitySuite.QUITTER_OPTION, "true");
        LongevitySuite suite =
                new LongevitySuite(
                        FailingTestSuite.class, new AllDefaultPossibilitiesBuilder(true), args);
        suite.run(new RunNotifier());
    }

    /**
     * Tests that test runs are considered failures if the invalidate option is set to true.
     */
    @Test
    public void testInvalidateTestRuns() throws InitializationError {
        Map<String, String> args = new HashMap();
        args.put(LongevitySuite.INVALIDATE_OPTION, "true");
        args.put(LongevitySuite.QUITTER_OPTION, "true");
        args.put(ITERATIONS_OPTION_NAME, String.valueOf(10));
        LongevitySuite suite =
                new LongevitySuite(
                        FailingTestSuite.class, new AllDefaultPossibilitiesBuilder(true), args);
        try {
            suite.run(new RunNotifier());
            fail("This run should be invalidated by test failure.");
        } catch (StoppedByUserException e) {
            // Expect this failure for an invalid, erroring test run.
        }
    }

    @RunWith(LongevitySuite.class)
    @SuiteClasses({
        FailingTestSuite.FailingTest.class,
    })
    /** Sample device-side test case that fails. */
    public static class FailingTestSuite {
        public static class FailingTest {
            @Test
            public void testFailure() throws InterruptedException {
                assertEquals(1, 2);
            }
        }
    }

    /** Tests that test runs are timing out if the tests run over the allotted suite time. */
    @Test
    public void testTimeoutTestRuns() throws InitializationError {
        Map<String, String> args = new HashMap();
        args.put(LongevitySuite.INVALIDATE_OPTION, "true");
        args.put(TimeoutTerminator.OPTION, "25");
        args.put(ITERATIONS_OPTION_NAME, String.valueOf(10));
        LongevitySuite suite =
                new LongevitySuite(
                        TimeoutTestSuite.class, new AllDefaultPossibilitiesBuilder(true), args);
        try {
            suite.run(new RunNotifier());
            fail("This run should be ended by a timeout failure.");
        } catch (StoppedByUserException e) {
            // Expect this failure for an invalid, erroring test run.
        }
    }

    @RunWith(LongevitySuite.class)
    @SuiteClasses({
        TimeoutTestSuite.TimedTest.class,
    })
    /** Sample device-side test case that takes time. */
    public static class TimeoutTestSuite {
        public static class TimedTest {
            @Test
            public void testSleep() throws InterruptedException {
                Thread.sleep(10);
            }
        }
    }

    /**
     * Tests that the {@link LongevitySuite} properly accounts for the number of tests in children.
     */
    @Test
    public void testChildAccounting() throws InitializationError {
        int expectedIterations = 10000;
        Map<String, String> args = new HashMap();
        args.put(ITERATIONS_OPTION_NAME, String.valueOf(expectedIterations));
        LongevitySuite suite =
                new LongevitySuite(TestSuite.class, new AllDefaultPossibilitiesBuilder(true), args);
        assertEquals(suite.testCount(), expectedIterations * 3);
    }


    @RunWith(LongevitySuite.class)
    @SuiteClasses({
        TestSuite.OneTest.class,
        TestSuite.TwoTests.class
    })
    /**
     * Sample device-side test cases.
     */
    public static class TestSuite {
        // no local test cases.

        public static class OneTest {
            @Test
            public void testNothing1() { }
        }

        public static class TwoTests {
            @Test
            public void testNothing1() { }

            @Test
            public void testNothing2() { }
        }
    }
}
