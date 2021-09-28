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
package android.platform.test.longevity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;
import static java.lang.Math.abs;

import android.app.Instrumentation;
import android.content.Context;
import android.host.test.longevity.listener.TimeoutTerminator;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.longevity.samples.testing.SampleBasicProfileSuite;
import android.platform.test.longevity.samples.testing.SampleExtraArgsSuite;
import android.platform.test.longevity.samples.testing.SampleTimedProfileSuite;
import android.platform.test.scenario.annotation.Scenario;
import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.internal.builders.AllDefaultPossibilitiesBuilder;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.JUnit4;
import org.junit.runners.Parameterized;
import org.junit.runners.Suite.SuiteClasses;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.RunnerBuilder;
import org.junit.runners.model.TestTimedOutException;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.exceptions.base.MockitoAssertionError;

import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/** Unit tests for the {@link ProfileSuite} runner. */
@RunWith(JUnit4.class)
public class ProfileSuiteTest {
    @Rule
    public ExpectedException mExpectedException = ExpectedException.none();

    @Mock private Instrumentation mInstrumentation;
    @Mock private Context mContext;
    @Mock private Profile mProfile;
    private RunNotifier mRunNotifier;

    // Threshold above which missing a schedule is considered a failure.
    private static final long SCHEDULE_LEEWAY_MS = 500;

    // Holds the state of the instrumentation args before each test for restoring after, as one test
    // might affect the state of another otherwise.
    // TODO(b/124239142): Avoid manipulating the instrumentation args here.
    private Bundle mArgumentsBeforeTest;

    @Before
    public void setUpSuite() throws InitializationError {
        initMocks(this);
        mRunNotifier = spy(new RunNotifier());
        mArgumentsBeforeTest = InstrumentationRegistry.getArguments();
    }

    @After
    public void restoreSuite() {
        InstrumentationRegistry.registerInstance(
                InstrumentationRegistry.getInstrumentation(), mArgumentsBeforeTest);
    }

    /** Test that profile suites with classes that aren't scenarios are rejected. */
    @Test
    public void testRejectInvalidTests_notScenarios() throws InitializationError {
        mExpectedException.expect(InitializationError.class);
        new ProfileSuite(NonScenarioSuite.class, new AllDefaultPossibilitiesBuilder(true),
                mInstrumentation, mContext, new Bundle());
    }

    /** Test that profile suites with classes that aren't scenarios are rejected. */
    @Test
    public void testRejectInvalidTests_notSupportedRunner() throws InitializationError {
        mExpectedException.expect(InitializationError.class);
        new ProfileSuite(InvalidRunnerSuite.class, new AllDefaultPossibilitiesBuilder(true),
                mInstrumentation, mContext, new Bundle());
    }

    /** Test that profile suites with classes that have no runner are rejected. */
    @Test
    public void testRejectInvalidTests_badRunnerBuilder() throws Throwable {
        mExpectedException.expect(InitializationError.class);
        RunnerBuilder builder = spy(new AllDefaultPossibilitiesBuilder(true));
        when(builder.runnerForClass(BasicScenario.class)).thenThrow(new Throwable("empty"));
        new ProfileSuite(BasicSuite.class, builder, mInstrumentation, mContext, new Bundle());
    }

    /** Test that the basic scenario suite is accepted if properly annotated. */
    @Test
    public void testValidScenario_basic() throws InitializationError {
        new ProfileSuite(BasicSuite.class, new AllDefaultPossibilitiesBuilder(true),
                    mInstrumentation, mContext, new Bundle());
    }

    // Scenarios and suites used for the suite validation tests above.

    @RunWith(ProfileSuite.class)
    @SuiteClasses({
        BasicScenario.class,
    })
    public static class BasicSuite {}

    @RunWith(ProfileSuite.class)
    @SuiteClasses({
        NonScenario.class,
    })
    public static class NonScenarioSuite { }

    @RunWith(ProfileSuite.class)
    @SuiteClasses({
        NotSupportedRunner.class,
    })
    public static class InvalidRunnerSuite { }

    @Scenario
    @RunWith(JUnit4.class)
    public static class BasicScenario {
        @Test
        public void testNothing() { }
    }

    // Note: @Scenario annotations are not inherited.
    @RunWith(JUnit4.class)
    public static class NonScenario extends BasicScenario { }

    @Scenario
    @RunWith(Parameterized.class)
    public static class NotSupportedRunner extends BasicScenario {}

    /** Test that a timestamped profile's scheduling is followed. */
    @Test
    public void testTimestampScheduling_respectsSchedule() throws InitializationError {
        // TODO(harrytczhang@): Find a way to run this without relying on actual idles.

        // Arguments with the profile under test.
        Bundle args = new Bundle();
        args.putString(Profile.PROFILE_OPTION_NAME, "testTimestampScheduling_respectsSchedule");
        // Scenario names from the profile.
        final String firstScenarioName =
                "android.platform.test.longevity.samples.testing."
                        + "SampleTimedProfileSuite$LongIdleTest";
        final String secondScenarioName =
                "android.platform.test.longevity.samples.testing."
                        + "SampleTimedProfileSuite$PassingTest";
        // Stores the start time of the test run for the suite. Using AtomicLong here as the time
        // should be initialized when run() is called on the suite, but Java does not want
        // assignment to local varaible in lambda expressions. AtomicLong allows for using the
        // same reference but altering the value.
        final AtomicLong runStartTimeMs = new AtomicLong(SystemClock.elapsedRealtime());
        ProfileSuite suite =
                spy(
                        new ProfileSuite(
                                SampleTimedProfileSuite.class,
                                new AllDefaultPossibilitiesBuilder(true),
                                mInstrumentation,
                                mContext,
                                args));
        // Stub the lifecycle calls to verify that tests are run on schedule.
        doAnswer(
                        invocation -> {
                            runStartTimeMs.set(SystemClock.elapsedRealtime());
                            invocation.callRealMethod();
                            return null;
                        })
                .when(suite)
                .run(argThat(notifier -> notifier.equals(mRunNotifier)));
        doAnswer(
                        invocation -> {
                            // The first scenario should start immediately.
                            Assert.assertTrue(
                                    abs(SystemClock.elapsedRealtime() - runStartTimeMs.longValue())
                                            <= SCHEDULE_LEEWAY_MS);
                            invocation.callRealMethod();
                            return null;
                        })
                .when(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(firstScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
        doAnswer(
                        invocation -> {
                            // The second scenario should begin at 00:00:10 - 00:00:01 = 9 seconds.
                            Assert.assertTrue(
                                    abs(
                                                    SystemClock.elapsedRealtime()
                                                            - runStartTimeMs.longValue()
                                                            - TimeUnit.SECONDS.toMillis(9))
                                            <= SCHEDULE_LEEWAY_MS);
                            invocation.callRealMethod();
                            return null;
                        })
                .when(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(secondScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
        InOrder inOrderVerifier = inOrder(suite);

        suite.run(mRunNotifier);
        // Verify that the test run is started.
        inOrderVerifier.verify(suite).run(argThat(notifier -> notifier.equals(mRunNotifier)));
        // Verify that the first scenario is started.
        inOrderVerifier
                .verify(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(firstScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
        // Verify that the second scenario is started.
        inOrderVerifier
                .verify(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(secondScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
    }

    /** Test that a timestamp profile's last scenario is bounded by the suite timeout. */
    @Test
    public void testTimestampScheduling_respectsSuiteTimeout() throws InitializationError {
        long suiteTimeoutMsecs = TimeUnit.SECONDS.toMillis(10);
        ArgumentCaptor<Failure> failureCaptor = ArgumentCaptor.forClass(Failure.class);

        // Arguments with the profile under test and suite timeout.
        Bundle args = new Bundle();
        args.putString(Profile.PROFILE_OPTION_NAME, "testTimestampScheduling_respectsSuiteTimeout");
        args.putString(TimeoutTerminator.OPTION, String.valueOf(suiteTimeoutMsecs));

        // Construct and run the profile suite.
        ProfileSuite suite =
                spy(
                        new ProfileSuite(
                                SampleTimedProfileSuite.class,
                                new AllDefaultPossibilitiesBuilder(true),
                                mInstrumentation,
                                mContext,
                                args));
        suite.run(mRunNotifier);

        // Verify that a TestTimedOutException is fired and the timeout is correct.
        verify(mRunNotifier, atLeastOnce()).fireTestFailure(failureCaptor.capture());
        List<Failure> failures = failureCaptor.getAllValues();
        boolean correctTestTimedOutExceptionFired =
                failures.stream()
                        .anyMatch(
                                f -> {
                                    if (!(f.getException() instanceof TestTimedOutException)) {
                                        return false;
                                    }
                                    TestTimedOutException exception =
                                            (TestTimedOutException) f.getException();
                                    long exceptionTimeout =
                                            exception
                                                    .getTimeUnit()
                                                    .toMillis(exception.getTimeout());
                                    // Expected timeout the duration from the last scenario to when
                                    // the suite should time out, minus the end time leeway set in
                                    // ScheduledScenarioRunner. Note that the second scenario is
                                    // executed at 00:00:04 as the first scenario is always
                                    // considered to be at 00:00:00.
                                    long expectedTimeout =
                                            suiteTimeoutMsecs
                                                    - TimeUnit.SECONDS.toMillis(4)
                                                    - ScheduledScenarioRunner
                                                            .TEARDOWN_LEEWAY_DEFAULT;
                                    return abs(exceptionTimeout - expectedTimeout)
                                            <= SCHEDULE_LEEWAY_MS;
                                });
        Assert.assertTrue(correctTestTimedOutExceptionFired);
    }

    /** Test that an indexed profile's scheduling is followed. */
    @Test
    public void testIndexedScheduling_respectsSchedule() throws InitializationError {
        // Arguments with the profile under test.
        Bundle args = new Bundle();
        args.putString(Profile.PROFILE_OPTION_NAME, "testIndexedScheduling_respectsSchedule");
        // Scenario names from the profile.
        final String firstScenarioName =
                "android.platform.test.longevity.samples.testing."
                        + "SampleBasicProfileSuite$PassingTest1";
        final String secondScenarioName =
                "android.platform.test.longevity.samples.testing."
                        + "SampleBasicProfileSuite$PassingTest2";
        final String thirdScenarioName =
                "android.platform.test.longevity.samples.testing."
                        + "SampleBasicProfileSuite$PassingTest1";
        ProfileSuite suite =
                spy(
                        new ProfileSuite(
                                SampleBasicProfileSuite.class,
                                new AllDefaultPossibilitiesBuilder(true),
                                mInstrumentation,
                                mContext,
                                args));

        InOrder inOrderVerifier = inOrder(suite);

        suite.run(mRunNotifier);
        // Verify that the first scenario is started.
        inOrderVerifier
                .verify(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(firstScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
        // Verify that the second scenario is started.
        inOrderVerifier
                .verify(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(secondScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
        // Verify that the third scenario is started.
        inOrderVerifier
                .verify(suite)
                .runChild(
                        argThat(
                                runner ->
                                        runner.getDescription()
                                                .getDisplayName()
                                                .equals(thirdScenarioName)),
                        argThat(notifier -> notifier.equals(mRunNotifier)));
    }

    /** Test that extra args for a scenario are registered before tests. */
    @Test
    public void testExtraArgs_registeredBeforeTest() throws Throwable {
        // Arguments with the profile under test.
        Bundle args = new Bundle();
        args.putString(Profile.PROFILE_OPTION_NAME, "testExtraArgs_registeredBeforeTest");
        ProfileSuite suite =
                new ProfileSuite(
                        SampleExtraArgsSuite.class,
                        new AllDefaultPossibilitiesBuilder(true),
                        mInstrumentation,
                        mContext,
                        args);
        // Run the suite, which is expected to pass.
        suite.run(mRunNotifier);
        // If it doesn't, fire what caused the tests within the suite to fail.
        verifyForAssertionFailures(mRunNotifier);
    }

    /** Test that extra args for a scenario are unregistered after tests. */
    @Test
    public void testExtraArgs_unregisteredAfterTest() throws Throwable {
        // Arguments with the profile under test.
        Bundle args = new Bundle();
        args.putString(Profile.PROFILE_OPTION_NAME, "testExtraArgs_unregisteredAfterTest");
        ProfileSuite suite =
                new ProfileSuite(
                        SampleExtraArgsSuite.class,
                        new AllDefaultPossibilitiesBuilder(true),
                        mInstrumentation,
                        mContext,
                        args);
        // Run the suite, which is expected to pass.
        suite.run(mRunNotifier);
        // If it doesn't, fire what caused the tests within the suite to fail.
        verifyForAssertionFailures(mRunNotifier);
    }

    /**
     * Verify that no failures are fired to the test notifier. If the verification fails, check
     * whether it's due to assertions made in a dummy test. If yes, throw that exception out;
     * otherwise, throw the first exception.
     */
    private void verifyForAssertionFailures(final RunNotifier notifier) throws Throwable {
        try {
            verify(notifier, never()).fireTestFailure(any());
        } catch (MockitoAssertionError e) {
            ArgumentCaptor<Failure> failureCaptor = ArgumentCaptor.forClass(Failure.class);
            verify(notifier, atLeastOnce()).fireTestFailure(failureCaptor.capture());
            List<Failure> failures = failureCaptor.getAllValues();
            // Go through the failures, look for an known failure case from the above exceptions
            // and throw the exception in the first one out if any.
            for (Failure failure : failures) {
                if (failure.getException()
                        .getMessage()
                        .contains(SampleExtraArgsSuite.ASSERTION_FAILURE_MESSAGE)) {
                    throw failure.getException();
                }
            }
            // Otherwise, throw the exception from the first failure reported.
            throw failures.get(0).getException();
        }
    }
}
