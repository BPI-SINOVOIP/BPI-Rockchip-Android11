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

package android.platform.test.longevity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.os.Bundle;

import java.util.List;

import org.junit.Assert;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.JUnit4;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.exceptions.base.MockitoAssertionError;

/** Unit tests for the {@link LongevityClassRunner}. */
public class LongevityClassRunnerTest {
    // A sample test class to test the runner with.
    @RunWith(JUnit4.class)
    public static class NoOpTest {
        @BeforeClass
        public static void beforeClassMethod() {}

        @Before
        public void beforeMethod() {}

        @Test
        public void testMethod() {}

        @After
        public void afterMethod() {}

        @AfterClass
        public static void afterClassMethod() {}
    }

    @RunWith(JUnit4.class)
    public static class FailingTest extends NoOpTest {
        @Test
        public void testMethod() {
            throw new RuntimeException("I failed.");
        }
    }

    @Mock private RunNotifier mRunNotifier;

    private LongevityClassRunner mRunner;

    private static final Statement PASSING_STATEMENT =
            new Statement() {
                public void evaluate() throws Throwable {
                    // No-op.
                }
            };

    private static final Statement FAILING_STATEMENT =
            new Statement() {
                public void evaluate() throws Throwable {
                    throw new RuntimeException("I failed.");
                }
            };

    // A failure message for assertion calls in Mockito stubs. These assertion failures will cause
    // the runner under test to fail but will not trigger a test failure directly. This message is
    // used to filter failures reported by the mocked RunNotifier and re-throw the ones injected in
    // the spy.
    private static final String ASSERTION_FAILURE_MESSAGE = "Test assertions failed";

    @Before
    public void setUp() {
        initMocks(this);
    }

    /**
     * Test that the {@link BeforeClass} methods are added to the test statement as {@link Before}
     * methods.
     */
    @Test
    public void testBeforeClassMethodsAddedAsBeforeMethods() throws Throwable {
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        // Spy the withBeforeClasses() method to check that the method does not make changes to the
        // statement despite the presence of a @BeforeClass method.
        doAnswer(
                        invocation -> {
                            Statement returnedStatement = (Statement) invocation.callRealMethod();
                            // If this assertion fails, mRunNofitier will fire a test failure.
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE,
                                    returnedStatement,
                                    invocation.getArgument(0));
                            return returnedStatement;
                        })
                .when(mRunner)
                .withBeforeClasses(any(Statement.class));
        // Spy the addRunBefores() method to check that the @BeforeClass method is added to the
        // @Before methods.
        doAnswer(
                        invocation -> {
                            List<FrameworkMethod> methodList =
                                    (List<FrameworkMethod>) invocation.getArgument(1);
                            // If any of these assertions fail, mRunNofitier will fire a test
                            // failure.
                            // There should be two methods.
                            Assert.assertEquals(ASSERTION_FAILURE_MESSAGE, methodList.size(), 2);
                            // The first one should be the @BeforeClass one.
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE,
                                    methodList.get(0).getName(),
                                    "beforeClassMethod");
                            // The second one should be the @Before one.
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE,
                                    methodList.get(1).getName(),
                                    "beforeMethod");
                            return invocation.callRealMethod();
                        })
                .when(mRunner)
                .addRunBefores(any(Statement.class), any(List.class), any(Object.class));
        // Run the runner.
        mRunner.run(mRunNotifier);
        verifyForAssertionFailures(mRunNotifier);
        // Verify that the stubbed methods are indeed called.
        verify(mRunner, times(1)).withBeforeClasses(any(Statement.class));
        verify(mRunner, times(1))
                .addRunBefores(any(Statement.class), any(List.class), any(Object.class));
    }

    /**
     * Test that the {@link AfterClass} methods are added to the test statement as potential {@link
     * After} methods.
     */
    @Test
    public void testAfterClassMethodsAddedAsAfterMethods() throws Throwable {
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        // Spy the withAfterClasses() method to check that the method returns an instance of
        // LongevityClassRunner.RunAfterClassMethodsOnTestFailure.
        doAnswer(
                        invocation -> {
                            Statement returnedStatement = (Statement) invocation.callRealMethod();
                            // If this assertion fails, mRunNofitier will fire a test failure.
                            Assert.assertTrue(
                                    ASSERTION_FAILURE_MESSAGE,
                                    returnedStatement
                                            instanceof
                                            LongevityClassRunner.RunAfterClassMethodsOnTestFailure);
                            return returnedStatement;
                        })
                .when(mRunner)
                .withAfterClasses(any(Statement.class));
        // Spy the addRunAfters() method to check that the method returns an instance of
        // LongevityClassRunner.RunAfterMethods.
        doAnswer(
                        invocation -> {
                            Statement returnedStatement = (Statement) invocation.callRealMethod();
                            // If any of these assertions fail, mRunNotifier will fire a test
                            // failure.
                            Assert.assertTrue(
                                    ASSERTION_FAILURE_MESSAGE,
                                    returnedStatement
                                            instanceof LongevityClassRunner.RunAfterMethods);
                            // The second argument should only contain the @After method.
                            List<FrameworkMethod> afterMethodList =
                                    (List<FrameworkMethod>) invocation.getArgument(1);
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE, afterMethodList.size(), 1);
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE,
                                    afterMethodList.get(0).getName(),
                                    "afterMethod");
                            // The third argument should only contain the @AfterClass method.
                            List<FrameworkMethod> afterClassMethodList =
                                    (List<FrameworkMethod>) invocation.getArgument(2);
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE, afterClassMethodList.size(), 1);
                            Assert.assertEquals(
                                    ASSERTION_FAILURE_MESSAGE,
                                    afterClassMethodList.get(0).getName(),
                                    "afterClassMethod");
                            return returnedStatement;
                        })
                .when(mRunner)
                .addRunAfters(
                        any(Statement.class), any(List.class), any(List.class), any(Object.class));
        // Run the runner.
        mRunner.run(mRunNotifier);
        verifyForAssertionFailures(mRunNotifier);
        // Verify that the stubbed methods are indeed called.
        verify(mRunner, times(1)).withAfterClasses(any(Statement.class));
        verify(mRunner, times(1))
                .addRunAfters(
                        any(Statement.class), any(List.class), any(List.class), any(Object.class));
    }

    /**
     * Test that {@link LongevityClassRunner.RunAfterMethods} marks the test as failed for a failed
     * test.
     */
    @Test
    public void testAfterClassMethodsHandling_marksFailure() throws Throwable {
        // Initialization parameter does not matter as this test does not concern the tested class.
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        try {
            mRunner.hasTestFailed();
            Assert.fail("Test status should not be able to be checked before it's run.");
        } catch (Throwable e) {
            Assert.assertTrue(e.getMessage().contains("should not be checked"));
        }
        // Create and partially run the runner with a failing statement.
        Statement statement =
                mRunner.withAfters(
                        null,
                        new NoOpTest(),
                        new Statement() {
                            public void evaluate() throws Throwable {
                                throw new RuntimeException("I failed.");
                            }
                        });
        try {
            statement.evaluate();
        } catch (Throwable e) {
            // Expected and no action needed.
        }
        Assert.assertTrue(mRunner.hasTestFailed());
    }

    /**
     * Test that {@link LongevityClassRunner.RunAfterMethods} marks the test as passed for a passed
     * test.
     */
    @Test
    public void testRunAfterClassMethodsHandling_marksPassed() throws Throwable {
        // Initialization parameter does not matter as this test does not concern the tested class.
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        // Checking test status before the statements are run should throw.
        try {
            mRunner.hasTestFailed();
            Assert.fail("Test status should not be able to be checked before it's run.");
        } catch (Throwable e) {
            Assert.assertTrue(e.getMessage().contains("should not be checked"));
        }
        // Create and partially run the runner with a passing statement.
        Statement statement =
                mRunner.withAfters(
                        null,
                        new NoOpTest(),
                        new Statement() {
                            public void evaluate() throws Throwable {
                                // Does nothing and thus passes.
                            }
                        });
        statement.evaluate();
        Assert.assertFalse(mRunner.hasTestFailed());
    }

    /** Test that {@link AfterClass} methods are run as {@link After} methods for a passing test. */
    @Test
    public void testAfterClass_runAsAfterForPassingTest() throws Throwable {
        // Initialization parameter does not matter as this test does not concern the tested class.
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        // For a passing test, the AfterClass method should be run in the statement returned from
        // withAfters().
        Statement statement =
                mRunner.withAfters(
                        null, // Passing null as parameter in the interface is not actually used.
                        new NoOpTest(),
                        PASSING_STATEMENT);
        statement.evaluate();
        // Check that the @AfterClass method is called.
        ArgumentCaptor<List> methodsCaptor = ArgumentCaptor.forClass(List.class);
        verify(mRunner, times(1)).invokeAndCollectErrors(methodsCaptor.capture(), any());
        Assert.assertEquals(methodsCaptor.getValue().size(), 1);
        Assert.assertTrue(
                ((FrameworkMethod) methodsCaptor.getValue().get(0))
                        .getName()
                        .contains("afterClassMethod"));
    }

    /**
     * Test that {@link AfterClass} methods are run as {@link AfterClass} method for a failing test.
     */
    @Test
    public void testAfterClass_runAsAfterClassForFailingTest() throws Throwable {
        // Initialization parameter does not matter as this test does not concern the tested class.
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        // For a failing test, the AfterClass method should be run in the statement returned from
        // withAfterClass().
        doReturn(true).when(mRunner).hasTestFailed();
        Statement statement = mRunner.withAfterClasses(FAILING_STATEMENT);
        try {
            statement.evaluate();
        } catch (Throwable e) {
            // Expected and no action needed.
        }
        // Check that the @AfterClass method is called.
        ArgumentCaptor<List> methodsCaptor = ArgumentCaptor.forClass(List.class);
        verify(mRunner, times(1)).invokeAndCollectErrors(methodsCaptor.capture(), any());
        Assert.assertEquals(methodsCaptor.getValue().size(), 1);
        Assert.assertTrue(
                ((FrameworkMethod) methodsCaptor.getValue().get(0))
                        .getName()
                        .contains("afterClassMethod"));
    }

    /** Test that {@link AfterClass} methods are only executed once for a passing test. */
    @Test
    public void testAfterClassRunOnlyOnce_passingTest() throws Throwable {
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        mRunner.run(mRunNotifier);
        verify(mRunner, times(1))
                .invokeAndCollectErrors(getMethodNameMatcher("afterClassMethod"), any());
    }

    /** Test that {@link AfterClass} methods are only executed once for a failing test. */
    @Test
    public void testAfterClassRunOnlyOnce_failingTest() throws Throwable {
        mRunner = spy(new LongevityClassRunner(FailingTest.class));
        mRunner.run(mRunNotifier);
        verify(mRunner, times(1))
                .invokeAndCollectErrors(getMethodNameMatcher("afterClassMethod"), any());
    }

    /** Test that excluded classes are not executed. */
    @Test
    public void testIgnore_excludedClasses() throws Throwable {
        RunNotifier notifier = spy(new RunNotifier());
        RunListener listener = mock(RunListener.class);
        notifier.addListener(listener);
        Bundle ignores = new Bundle();
        ignores.putString(LongevityClassRunner.FILTER_OPTION, FailingTest.class.getCanonicalName());
        mRunner = spy(new LongevityClassRunner(FailingTest.class, ignores));
        mRunner.run(notifier);
        verify(listener, times(1)).testIgnored(any());
    }

    /** Test that the runner does not report iteration when iteration is not set. */
    @Test
    public void testReportIteration_noIterationSet() throws Throwable {
        ArgumentCaptor<Description> captor = ArgumentCaptor.forClass(Description.class);
        RunNotifier notifier = mock(RunNotifier.class);
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        mRunner.run(notifier);
        verify(notifier).fireTestStarted(captor.capture());
        Assert.assertFalse(
                "Description class name should not contain the iteration number.",
                captor.getValue()
                        .getClassName()
                        .matches(
                                String.join(
                                        LongevityClassRunner.ITERATION_SEP_DEFAULT,
                                        "^.*",
                                        "[0-9]+$")));
    }

    /** Test that the runner reports iteration when set and the default separator was used. */
    @Test
    public void testReportIteration_withIteration_withDefaultSeparator() throws Throwable {
        ArgumentCaptor<Description> captor = ArgumentCaptor.forClass(Description.class);
        RunNotifier notifier = mock(RunNotifier.class);
        mRunner = spy(new LongevityClassRunner(NoOpTest.class));
        mRunner.setIteration(7);
        mRunner.run(notifier);
        verify(notifier).fireTestStarted(captor.capture());
        Assert.assertTrue(
                "Description class name should contain the iteration number.",
                captor.getValue()
                        .getClassName()
                        .matches(
                                String.join(
                                        LongevityClassRunner.ITERATION_SEP_DEFAULT, "^.*", "7$")));
    }

    /** Test that the runner reports iteration when set and a custom separator was supplied. */
    @Test
    public void testReportIteration_withIteration_withCustomSeparator() throws Throwable {
        String sep = "--";
        Bundle args = new Bundle();
        args.putString(LongevityClassRunner.ITERATION_SEP_OPTION, sep);

        ArgumentCaptor<Description> captor = ArgumentCaptor.forClass(Description.class);
        RunNotifier notifier = mock(RunNotifier.class);
        mRunner = spy(new LongevityClassRunner(NoOpTest.class, args));
        mRunner.setIteration(7);
        mRunner.run(notifier);
        verify(notifier).fireTestStarted(captor.capture());
        Assert.assertTrue(
                "Description class name should contain the iteration number.",
                captor.getValue().getClassName().matches(String.join(sep, "^.*", "7$")));
    }

    private List<FrameworkMethod> getMethodNameMatcher(String methodName) {
        return argThat(
                l ->
                        l.stream()
                                .anyMatch(
                                        f -> ((FrameworkMethod) f).getName().contains(methodName)));
    }

    /**
     * Verify that no test failure is fired because of an assertion failure in the stubbed methods.
     * If the verfication fails, check whether it's due the injected assertions failing. If yes,
     * throw that exception out; otherwise, throw the first exception.
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
                if (failure.getException().getMessage().contains(ASSERTION_FAILURE_MESSAGE)) {
                    throw failure.getException();
                }
            }
            // Otherwise, throw the exception from the first failure reported.
            throw failures.get(0).getException();
        }
    }
}
