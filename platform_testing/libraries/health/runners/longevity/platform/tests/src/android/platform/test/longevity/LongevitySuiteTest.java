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
package android.platform.test.longevity;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.os.BatteryManager;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.internal.builders.AllDefaultPossibilitiesBuilder;
import org.junit.internal.runners.ErrorReportingRunner;
import org.junit.runner.Runner;
import org.junit.runner.RunWith;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.RunnerBuilder;
import org.junit.runners.Suite.SuiteClasses;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for the {@link LongevitySuite} runner.
 */
@RunWith(JUnit4.class)
public class LongevitySuiteTest {
    private final Instrumentation mInstrumentation = Mockito.mock(Instrumentation.class);
    private final Context mContext = Mockito.mock(Context.class);

    private final RunNotifier mRunNotifier = Mockito.spy(new RunNotifier());
    private final Intent mBatteryIntent = new Intent();

    private LongevitySuite mSuite;

    @Before
    public void setUp() {
        // Android context mocking.
        when(mContext.registerReceiver(any(), any())).thenReturn(mBatteryIntent);
    }

    /** Tests that devices with batteries terminate on low battery. */
    @Test
    public void testDeviceWithBattery_registersReceiver() throws InitializationError {
        // Create the core suite to test.
        mSuite =
                new LongevitySuite(
                        TestSuite.class,
                        new AllDefaultPossibilitiesBuilder(true),
                        new ArrayList<Runner>(),
                        mInstrumentation,
                        mContext,
                        new Bundle());
        mBatteryIntent.putExtra(BatteryManager.EXTRA_PRESENT, true);
        mBatteryIntent.putExtra(BatteryManager.EXTRA_LEVEL, 1);
        mBatteryIntent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        mSuite.run(mRunNotifier);
        verify(mRunNotifier).pleaseStop();
    }

    /** Tests that devices without batteries do not terminate on low battery. */
    @Test
    public void testDeviceWithoutBattery_doesNotRegisterReceiver() throws InitializationError {
        // Create the core suite to test.
        mSuite =
                new LongevitySuite(
                        TestSuite.class,
                        new AllDefaultPossibilitiesBuilder(true),
                        new ArrayList<Runner>(),
                        mInstrumentation,
                        mContext,
                        new Bundle());
        mBatteryIntent.putExtra(BatteryManager.EXTRA_PRESENT, false);
        mBatteryIntent.putExtra(BatteryManager.EXTRA_LEVEL, 1);
        mBatteryIntent.putExtra(BatteryManager.EXTRA_SCALE, 100);
        mSuite.run(mRunNotifier);
        verify(mRunNotifier, never()).pleaseStop();
    }

    /** Test that the runner does not report iterations when the option is not set. */
    @Test
    public void testReportingIteration_notSet() throws InitializationError {
        // Create and spy the core suite to test. The option is not set as the args bundle is empty.
        mSuite =
                Mockito.spy(
                        new LongevitySuite(
                                IterationSuite.class,
                                new AllDefaultPossibilitiesBuilder(true),
                                new ArrayList<Runner>(),
                                mInstrumentation,
                                mContext,
                                new Bundle()));
        // Store the runners that the tests are executing. Since these are object references,
        // subsequent modifications to the runners (setting the iteration) will still be observable
        // here.
        List<LongevityClassRunner> runners = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            LongevityClassRunner runner =
                                    (LongevityClassRunner) invocation.callRealMethod();
                            runners.add(runner);
                            return runner;
                        })
                .when(mSuite)
                .getSuiteRunner(any(Runner.class));
        mSuite.run(mRunNotifier);
        Assert.assertEquals(runners.size(), 3);
        // No runner should have a iteration number set.
        Assert.assertEquals(runners.get(0).getIteration(), LongevityClassRunner.ITERATION_NOT_SET);
        Assert.assertEquals(runners.get(1).getIteration(), LongevityClassRunner.ITERATION_NOT_SET);
        Assert.assertEquals(runners.get(2).getIteration(), LongevityClassRunner.ITERATION_NOT_SET);
    }

    /** Test that the runner reports iterations when the option is set. */
    @Test
    public void testReportingIteration_set() throws InitializationError {
        Bundle args = new Bundle();
        args.putString(LongevitySuite.RENAME_ITERATION_OPTION, String.valueOf(true));
        // Create and spy the core suite to test.
        mSuite =
                Mockito.spy(
                        new LongevitySuite(
                                IterationSuite.class,
                                new AllDefaultPossibilitiesBuilder(true),
                                new ArrayList<Runner>(),
                                mInstrumentation,
                                mContext,
                                args));
        // Store the runners that the tests are executing. Since these are object references,
        // subsequent modifications to the runners (setting the iteration) will still be observable
        // here.
        List<LongevityClassRunner> runners = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            LongevityClassRunner runner =
                                    (LongevityClassRunner) invocation.callRealMethod();
                            runners.add(runner);
                            return runner;
                        })
                .when(mSuite)
                .getSuiteRunner(any(Runner.class));
        mSuite.run(mRunNotifier);
        Assert.assertEquals(runners.size(), 3);
        // Check the runners and their corresponding iterations.
        Assert.assertTrue(runners.get(0).getDescription().getDisplayName().contains("TestOne"));
        Assert.assertEquals(runners.get(0).getIteration(), 1);
        Assert.assertTrue(runners.get(1).getDescription().getDisplayName().contains("TestTwo"));
        Assert.assertEquals(runners.get(1).getIteration(), 1);
        Assert.assertTrue(runners.get(2).getDescription().getDisplayName().contains("TestOne"));
        Assert.assertEquals(runners.get(2).getIteration(), 2);
    }

    /** Test that appending classes to a suite will properly append them and iterate on them. */
    @Test
    public void testAdditionalRunners() throws InitializationError {
        Bundle args = new Bundle();
        // TODO: Access the iterations option name directly.
        args.putString("iterations", String.valueOf(2));
        List<Runner> additions = new ArrayList<>();
        additions.add(new BlockJUnit4ClassRunner(AdditionalTest.class));
        additions.add(new BlockJUnit4ClassRunner(AdditionalTest.class));
        // Construct the suite with additional classes to run.
        mSuite =
                Mockito.spy(
                        new LongevitySuite(
                                IterationSuite.class,
                                new AllDefaultPossibilitiesBuilder(true),
                                additions,
                                mInstrumentation,
                                mContext,
                                args));
        // Store the runners that the tests are executing. Since these are object references,
        // subsequent modifications to the runners (setting the iteration) will still be observable
        // here.
        List<LongevityClassRunner> runners = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            LongevityClassRunner runner =
                                    (LongevityClassRunner) invocation.callRealMethod();
                            runners.add(runner);
                            return runner;
                        })
                .when(mSuite)
                .getSuiteRunner(any(Runner.class));
        mSuite.run(mRunNotifier);
        Assert.assertEquals(runners.size(), 10);
        // Check the runners and their corresponding iterations.
        Assert.assertTrue(runners.get(0).getDescription().getDisplayName().contains("TestOne"));
        Assert.assertTrue(runners.get(1).getDescription().getDisplayName().contains("TestTwo"));
        Assert.assertTrue(runners.get(2).getDescription().getDisplayName().contains("TestOne"));
        Assert.assertTrue(
                runners.get(3).getDescription().getDisplayName().contains("AdditionalTest"));
        Assert.assertTrue(
                runners.get(4).getDescription().getDisplayName().contains("AdditionalTest"));
        // 5, 6, and 7 are repetitions of "TestOne", "TestTwo", and "TestOne".
        Assert.assertTrue(
                runners.get(8).getDescription().getDisplayName().contains("AdditionalTest"));
        Assert.assertTrue(
                runners.get(9).getDescription().getDisplayName().contains("AdditionalTest"));
    }

    /**
     * Test that when a builder ends up being a {@link ErrorReportingRunner}, the underlying error
     * is reported as an InitializationError.
     */
    @Test
    public void testThrowingErrorReportingRunnerCauses() throws Throwable {
        RunnerBuilder builder = mock(RunnerBuilder.class);
        InitializationError expected =
                new InitializationError(
                        Arrays.asList(
                                new RuntimeException("Cause 1"), new RuntimeException("Cause 2")));
        when(builder.runnerForClass(IterationSuite.TestOne.class))
                .thenReturn(new BlockJUnit4ClassRunner(IterationSuite.TestOne.class));
        when(builder.runnerForClass(IterationSuite.TestTwo.class))
                .thenReturn(new ErrorReportingRunner(IterationSuite.TestTwo.class, expected));
        try {
            mSuite =
                    new LongevitySuite(
                            IterationSuite.class,
                            builder,
                            new ArrayList<Runner>(),
                            mInstrumentation,
                            mContext,
                            new Bundle());
            Assert.fail("An InitializationError should have been thrown");
        } catch (InitializationError e) {
            Assert.assertEquals(e.getCauses().size(), 2);
            Assert.assertEquals(e.getCauses().get(0).getMessage(), "Cause 1");
            Assert.assertEquals(e.getCauses().get(1).getMessage(), "Cause 2");
        }
    }

    /** Sample device-side test cases. */
    @RunWith(LongevitySuite.class)
    @SuiteClasses({
        TestSuite.BasicTest.class,
    })
    public static class TestSuite {
        // no local test cases.

        public static class BasicTest {
            @Test
            public void testNothing() { }
        }
    }

    /** Sample test class with multiple iterations of the same test. */
    @RunWith(LongevitySuite.class)
    @SuiteClasses({
        IterationSuite.TestOne.class,
        IterationSuite.TestTwo.class,
        IterationSuite.TestOne.class,
    })
    public static class IterationSuite {
        // no local test cases.

        public static class TestOne {
            @Test
            public void testNothing() {}
        }

        public static class TestTwo {
            @Test
            public void testNothing() {}
        }
    }

    /** Additional test class to add to append to the longevity suite. */
    // @RunWith(JUnit4.class)
    public static class AdditionalTest {
        @Test
        public void testNothing() {}
    }
}
