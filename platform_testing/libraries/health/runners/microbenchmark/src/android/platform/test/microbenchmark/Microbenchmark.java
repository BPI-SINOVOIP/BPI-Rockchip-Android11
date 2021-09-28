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
package android.platform.test.microbenchmark;

import static android.content.Context.BATTERY_SERVICE;
import static android.os.BatteryManager.BATTERY_PROPERTY_CAPACITY;
import static android.os.BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER;

import android.os.BatteryManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.composer.Iterate;
import android.platform.test.rule.TracePointRule;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.internal.AssumptionViolatedException;
import org.junit.internal.runners.model.EachTestNotifier;
import org.junit.internal.runners.model.ReflectiveCallable;
import org.junit.internal.runners.statements.RunAfters;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;
import org.junit.rules.RunRules;

/**
 * The {@code Microbenchmark} runner allows you to run test methods repeatedly and with {@link
 * TightMethodRule}s in order to reliably measure a specific test method in isolation. Samples are
 * soon to follow.
 */
public class Microbenchmark extends BlockJUnit4ClassRunner {

    private static final String LOG_TAG = Microbenchmark.class.getSimpleName();

    @VisibleForTesting static final String ITERATION_SEP_OPTION = "iteration-separator";
    @VisibleForTesting static final String ITERATION_SEP_DEFAULT = "$";
    // A constant to indicate that the iteration number is not set.
    @VisibleForTesting static final int ITERATION_NOT_SET = -1;
    public static final String RENAME_ITERATION_OPTION = "rename-iterations";
    private static final Statement EMPTY =
            new Statement() {
                @Override
                public void evaluate() throws Throwable {}
            };
    private static final String MIN_BATTERY_LEVEL_OPTION = "min-battery";

    // Options for aligning with the battery charge (coulomb) counter for power tests. We want to
    // start microbenchmarks just after the coulomb counter has decremented to account for the
    // counter being quantized. The counter most accurately reflects the true value just after it
    // decrements.
    private static final String ALIGN_WITH_CHARGE_COUNTER_OPTION = "align-with-charge-counter";
    private static final String COUNTER_DECREMENT_TIMEOUT_OPTION = "counter-decrement-timeout_ms";

    private static final String TERMINATE_ON_TEST_FAIL_OPTION = "terminate-on-test-fail";

    private final String mIterationSep;
    private final Bundle mArguments;
    private final boolean mRenameIterations;
    private final int mMinBatteryLevel;
    private final int mCounterDecrementTimeoutMs;
    private final boolean mAlignWithChargeCounter;
    private final boolean mTerminateOnTestFailure;
    private final Map<Description, Integer> mIterations = new HashMap<>();

    private final BatteryManager mBatteryManager;

    /**
     * Called reflectively on classes annotated with {@code @RunWith(Microbenchmark.class)}.
     */
    public Microbenchmark(Class<?> klass) throws InitializationError {
        this(klass, InstrumentationRegistry.getArguments());
    }

    /** Do not call. Called explicitly from tests to provide an arguments. */
    @VisibleForTesting
    Microbenchmark(Class<?> klass, Bundle arguments) throws InitializationError {
        super(klass);
        mArguments = arguments;
        // Parse out additional options.
        mRenameIterations = Boolean.parseBoolean(arguments.getString(RENAME_ITERATION_OPTION));
        mIterationSep =
                arguments.containsKey(ITERATION_SEP_OPTION)
                        ? arguments.getString(ITERATION_SEP_OPTION)
                        : ITERATION_SEP_DEFAULT;
        mMinBatteryLevel = Integer.parseInt(arguments.getString(MIN_BATTERY_LEVEL_OPTION, "-1"));
        mCounterDecrementTimeoutMs =
                Integer.parseInt(arguments.getString(COUNTER_DECREMENT_TIMEOUT_OPTION, "30000"));
        mAlignWithChargeCounter =
                Boolean.parseBoolean(
                        arguments.getString(ALIGN_WITH_CHARGE_COUNTER_OPTION, "false"));

        mTerminateOnTestFailure =
                Boolean.parseBoolean(
                        arguments.getString(TERMINATE_ON_TEST_FAIL_OPTION, "false"));

        // Get the battery manager for later use.
        mBatteryManager =
                (BatteryManager)
                        InstrumentationRegistry.getContext().getSystemService(BATTERY_SERVICE);
    }

    @Override
    public void run(final RunNotifier notifier) {
        if (mAlignWithChargeCounter) {
            // Try to wait until the coulomb counter has just decremented to start the test.
            int startChargeCounter = getBatteryChargeCounter();
            long startTimestamp = SystemClock.uptimeMillis();
            while (startChargeCounter == getBatteryChargeCounter()) {
                if (SystemClock.uptimeMillis() - startTimestamp > mCounterDecrementTimeoutMs) {
                    Log.d(
                            LOG_TAG,
                            "Timed out waiting for the counter to change. Continuing anyway.");
                    break;
                } else {
                    Log.d(
                            LOG_TAG,
                            String.format(
                                    "Charge counter still reads: %d. Waiting.",
                                    startChargeCounter));
                    SystemClock.sleep(getCounterPollingInterval());
                }
            }
        }
        Log.d(LOG_TAG, String.format("The charge counter reads: %d.", getBatteryChargeCounter()));

        super.run(notifier);
    }

    /**
     * Returns a {@link Statement} that invokes {@code method} on {@code test}, surrounded by any
     * explicit or command-line-supplied {@link TightMethodRule}s. This allows for tighter {@link
     * TestRule}s that live inside {@link Before} and {@link After} statements.
     */
    @Override
    protected Statement methodInvoker(FrameworkMethod method, Object test) {
        // Iterate on the test method multiple times for more data. If unset, defaults to 1.
        Iterate<Statement> methodIterator = new Iterate<Statement>();
        methodIterator.setOptionName("method-iterations");
        final List<Statement> testMethodStatement =
                methodIterator.apply(
                        mArguments,
                        Arrays.asList(new Statement[] {super.methodInvoker(method, test)}));
        Statement start =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        for (Statement method : testMethodStatement) {
                            method.evaluate();
                        }
                    }
                };
        // Wrap the multiple-iteration test method with trace points.
        start = getTracePointRule().apply(start, describeChild(method));
        // Invoke special @TightMethodRules that wrap @Test methods.
        List<TestRule> tightMethodRules =
                getTestClass().getAnnotatedFieldValues(test, TightMethodRule.class, TestRule.class);
        for (TestRule tightMethodRule : tightMethodRules) {
            start = tightMethodRule.apply(start, describeChild(method));
        }
        return start;
    }

    @VisibleForTesting
    protected TracePointRule getTracePointRule() {
        return new TracePointRule();
    }

    /**
     * Returns a list of repeated {@link FrameworkMethod}s to execute.
     */
    @Override
    protected List<FrameworkMethod> getChildren() {
       return new Iterate<FrameworkMethod>().apply(mArguments, super.getChildren());
    }

    /**
     * An annotation for the corresponding tight rules above. These rules are ordered differently
     * from standard JUnit {@link Rule}s because they live between {@link Before} and {@link After}
     * methods, instead of wrapping those methods.
     *
     * <p>In particular, these serve as a proxy for tight metric collection in microbenchmark-style
     * tests, where collection is isolated to just the method under test. This is important for when
     * {@link Before} and {@link After} methods will obscure signal reliability.
     *
     * <p>Currently these are only registered from inside a test class as follows, but should soon
     * be extended for command-line support.
     *
     * ```
     * @RunWith(Microbenchmark.class)
     * public class TestClass {
     *     @TightMethodRule
     *     public ExampleRule exampleRule = new ExampleRule();
     *
     *     @Test ...
     * }
     * ```
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.FIELD, ElementType.METHOD})
    public @interface TightMethodRule {}

    /**
     * A temporary annotation that acts like the {@code @Before} but is excluded from metric
     * collection.
     *
     * <p>This should be removed as soon as possible. Do not use this unless explicitly instructed
     * to do so. You'll regret it!
     *
     * <p>Note that all {@code TestOption}s must be instantiated as {@code @ClassRule}s to work
     * inside these annotations.
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.FIELD, ElementType.METHOD})
    public @interface NoMetricBefore {}

    /** A temporary annotation, same as the above, but for replacing {@code @After} methods. */
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.FIELD, ElementType.METHOD})
    public @interface NoMetricAfter {}

    /**
     * Rename the child class name to add iterations if the renaming iteration option is enabled.
     *
     * <p>Renaming the class here is chosen over renaming the method name because
     *
     * <ul>
     *   <li>Conceptually, the runner is running a class multiple times, as opposed to a method.
     *   <li>When instrumenting a suite in command line, by default the instrumentation command
     *       outputs the class name only. Renaming the class helps with interpretation in this case.
     */
    @Override
    protected Description describeChild(FrameworkMethod method) {
        Description original = super.describeChild(method);
        if (!mRenameIterations) {
            return original;
        }
        return Description.createTestDescription(
                String.join(mIterationSep, original.getClassName(),
                        String.valueOf(mIterations.get(original))), original.getMethodName());
    }

    /** Re-implement the private rules wrapper from {@link BlockJUnit4ClassRunner} in JUnit 4.12. */
    private Statement withRules(FrameworkMethod method, Object target, Statement statement) {
        Statement result = statement;
        List<TestRule> testRules = getTestRules(target);
        // Apply legacy MethodRules, if they don't overlap with TestRules.
        for (org.junit.rules.MethodRule each : rules(target)) {
            if (!testRules.contains(each)) {
                result = each.apply(result, method, target);
            }
        }
        // Apply modern, method-level TestRules in outer statements.
        result =
                testRules.isEmpty()
                        ? statement
                        : new RunRules(result, testRules, describeChild(method));
        return result;
    }

    /**
     * Combine the {@code #runChild}, {@code #methodBlock}, and final {@code #runLeaf} methods to
     * implement the specific {@code Microbenchmark} test behavior. In particular, (1) keep track of
     * the number of iterations for a particular method description, and (2) run {@code
     * NoMetricBefore} and {@code NoMetricAfter} methods outside of the {@code RunListener} test
     * wrapping methods.
     */
    @Override
    protected void runChild(final FrameworkMethod method, RunNotifier notifier) {
        if (isBatteryLevelTooLow()) {
            throw new TerminateEarlyException("battery level is below threshold.");
        }

        // Update the number of iterations this method has been run.
        if (mRenameIterations) {
            Description original = super.describeChild(method);
            mIterations.computeIfPresent(original, (k, v) -> v + 1);
            mIterations.computeIfAbsent(original, k -> 1);
        }

        Description description = describeChild(method);
        if (isIgnored(method)) {
            notifier.fireTestIgnored(description);
        } else {
            EachTestNotifier eachNotifier = new EachTestNotifier(notifier, description);

            Object test;
            try {
                // Fail fast if the test is not successfully created.
                test =
                        new ReflectiveCallable() {
                            @Override
                            protected Object runReflectiveCall() throws Throwable {
                                return createTest();
                            }
                        }.run();

                // Run {@code NoMetricBefore} methods first. Fail fast if they fail.
                for (FrameworkMethod noMetricBefore :
                        getTestClass().getAnnotatedMethods(NoMetricBefore.class)) {
                    noMetricBefore.invokeExplosively(test);
                }
            } catch (Throwable e) {
                eachNotifier.fireTestStarted();
                eachNotifier.addFailure(e);
                eachNotifier.fireTestFinished();
                if(mTerminateOnTestFailure) {
                    throw new TerminateEarlyException("test failed.");
                }
                return;
            }

            Statement statement = methodInvoker(method, test);
            statement = possiblyExpectingExceptions(method, test, statement);
            statement = withPotentialTimeout(method, test, statement);
            statement = withBefores(method, test, statement);
            statement = withAfters(method, test, statement);
            statement = withRules(method, test, statement);

            boolean testFailed = false;
            // Fire test events from inside to exclude "no metric" methods.
            eachNotifier.fireTestStarted();
            try {
                statement.evaluate();
            } catch (AssumptionViolatedException e) {
                eachNotifier.addFailedAssumption(e);
                testFailed = true;
            } catch (Throwable e) {
                eachNotifier.addFailure(e);
                testFailed = true;
            } finally {
                eachNotifier.fireTestFinished();
            }

            try {
                // Run {@code NoMetricAfter} methods last, reporting all errors.
                List<FrameworkMethod> afters =
                        getTestClass().getAnnotatedMethods(NoMetricAfter.class);
                if (!afters.isEmpty()) {
                    new RunAfters(EMPTY, afters, test).evaluate();
                }
            } catch (AssumptionViolatedException e) {
                eachNotifier.addFailedAssumption(e);
                testFailed = true;
            } catch (Throwable e) {
                eachNotifier.addFailure(e);
                testFailed = true;
            }

            if(mTerminateOnTestFailure && testFailed) {
                throw new TerminateEarlyException("test failed.");
            }
        }
    }

    /* Checks if the battery level is below the specified level where the test should terminate. */
    @VisibleForTesting
    public boolean isBatteryLevelTooLow() {
        return mBatteryManager.getIntProperty(BATTERY_PROPERTY_CAPACITY) < mMinBatteryLevel;
    }

    /* Gets the current battery charge counter (coulomb counter). */
    @VisibleForTesting
    public int getBatteryChargeCounter() {
        return mBatteryManager.getIntProperty(BATTERY_PROPERTY_CHARGE_COUNTER);
    }

    /* Gets the polling interval to check for changes in the battery charge counter. */
    @VisibleForTesting
    public long getCounterPollingInterval() {
        return 100;
    }

    /**
     * A {@code RuntimeException} class for terminating test runs early for some specified reason.
     */
    @VisibleForTesting
    static class TerminateEarlyException extends RuntimeException {
        public TerminateEarlyException(String message) {
            super(String.format("Terminating early because %s", message));
        }
    }

}
