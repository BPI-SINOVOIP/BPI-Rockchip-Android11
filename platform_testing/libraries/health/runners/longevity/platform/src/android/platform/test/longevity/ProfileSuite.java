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

import android.app.Instrumentation;
import android.content.Context;
import android.os.Bundle;
import android.platform.test.longevity.proto.Configuration.Scenario;
import android.platform.test.longevity.proto.Configuration.Scenario.ExtraArg;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.util.List;

import org.junit.runner.Runner;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.RunnerBuilder;

/**
 * {@inheritDoc}
 *
 * This class is used for constructing longevity suites that run on an Android device based on a
 * profile.
 */
public class ProfileSuite extends LongevitySuite {

    // Profile instance for scheduling tests.
    private final Profile mProfile;

    /**
     * Called reflectively on classes annotated with {@code @RunWith(LongevitySuite.class)}
     */
    public ProfileSuite(Class<?> klass, RunnerBuilder builder) throws InitializationError {
        this(klass, builder, InstrumentationRegistry.getInstrumentation(),
                InstrumentationRegistry.getContext(), InstrumentationRegistry.getArguments());
    }

    /**
     * Used to pass in mock-able Android features for testing.
     * TODO(b/120508148): write tests for logic specific to profiles.
     */
    @VisibleForTesting
    public ProfileSuite(Class<?> klass, RunnerBuilder builder,
            Instrumentation instrumentation, Context context, Bundle arguments)
            throws InitializationError {
        super(klass, constructClassRunners(klass, builder, arguments), arguments);
        mProfile = new Profile(arguments);
    }

    /**
     * Constructs the sequence of {@link Runner}s using platform composers.
     */
    private static List<Runner> constructClassRunners(
                Class<?> suite, RunnerBuilder builder, Bundle args)
            throws InitializationError {
        // TODO(b/118340229): Refactor to share logic with base class. In the meanwhile, keep the
        // logic here in sync with the base class.
        // Retrieve annotated suite classes.
        SuiteClasses annotation = suite.getAnnotation(SuiteClasses.class);
        if (annotation == null) {
            throw new InitializationError(String.format(
                    "Longevity suite, '%s', must have a SuiteClasses annotation", suite.getName()));
        }
        // Validate that runnable scenarios are passed into the suite.
        for (Class<?> scenario : annotation.value()) {
            Runner runner = null;
            try {
                runner = builder.runnerForClass(scenario);
            } catch (Throwable t) {
                throw new InitializationError(t);
            }
            // All scenarios must be annotated with @Scenario.
            if (!scenario.isAnnotationPresent(
                    android.platform.test.scenario.annotation.Scenario.class)) {
                throw new InitializationError(
                        String.format(
                                "%s is not annotated with @Scenario.",
                                runner.getDescription().getDisplayName()));
            }
            // All scenarios must extend BlockJUnit4ClassRunner.
            if (!(runner instanceof BlockJUnit4ClassRunner)) {
                throw new InitializationError(
                        String.format(
                                "All runners must extend BlockJUnit4ClassRunner. %s:%s doesn't.",
                                runner.getClass(), runner.getDescription().getDisplayName()));
            }
        }
        // Construct and store custom runners for the full suite.
        return new Profile(args).getRunnerSequence(builder.runners(suite, annotation.value()));
    }

    /** {@inheritDoc} */
    @Override
    public void run(final RunNotifier notifier) {
        // Add the profile listener.
        notifier.addListener(mProfile);
        // Register other listeners and continue with standard longevity run.
        super.run(notifier);
    }

    /**
     * {@inheritDoc}
     *
     * <p>The "extras" are injected here before the entire child test class is run to ensure that
     * test options passed in as class-level rules still work.
     */
    @Override
    protected void runChild(Runner runner, final RunNotifier notifier) {
        // Fall back to LongevitySuite behavior if no profile is supplied.
        if (mProfile.getConfiguration() == null) {
            super.runChild(runner, notifier);
            return;
        }
        Bundle existingArguments = InstrumentationRegistry.getArguments().deepCopy();
        Bundle modifiedArguments = InstrumentationRegistry.getArguments().deepCopy();
        for (ExtraArg argPair : mProfile.getCurrentScenario().getExtrasList()) {
            if (!argPair.hasKey() || !argPair.hasValue()) {
                throw new IllegalArgumentException(
                        String.format(
                                "Each extra arg entry in scenario must have both a key and a value,"
                                        + " but scenario is %s.",
                                mProfile.getCurrentScenario()));
            }
            modifiedArguments.putString(argPair.getKey(), argPair.getValue());
        }
        // Swap the arguments, run the scenario, and then restore arguments.
        InstrumentationRegistry.registerInstance(
                InstrumentationRegistry.getInstrumentation(), modifiedArguments);
        super.runChild(runner, notifier);
        InstrumentationRegistry.registerInstance(
                InstrumentationRegistry.getInstrumentation(), existingArguments);
    }

    /**
     * {@inheritDoc}
     *
     * <p>Returns a runner suitable for the schedule that the profile uses. Currently supports
     * {@link ScheduledScenarioRunner} only, but support for more runners will be added as profile
     * features expand.
     */
    @Override
    protected LongevityClassRunner getSuiteRunner(Runner runner) {
        if (mProfile.getConfiguration() == null) {
            return super.getSuiteRunner(runner);
        }
        switch (mProfile.getConfiguration().getSchedule()) {
            case TIMESTAMPED:
                long timeout =
                        mProfile.hasNextScheduledScenario()
                                ? mProfile.getTimeUntilNextScenarioMs()
                                : (getSuiteTimeoutMs() - mProfile.getTimeSinceRunStartedMs());
                return getScheduledRunner(
                        (BlockJUnit4ClassRunner) runner,
                        mProfile.getCurrentScenario(),
                        timeout,
                        mProfile.hasNextScheduledScenario());

            case INDEXED:
                // A LongevityClassRunner, which the superclass method already returns, is suitable
                // for an indexed profile.
                return super.getSuiteRunner(runner);
        }

        throw new RuntimeException(
                String.format(
                        "Schedule type %s is not yet supported.",
                        mProfile.getConfiguration().getSchedule()));
    }

    /**
     * Replace a runner with {@link ScheduledScenarioRunner} for features specific to scheduled
     * profiles.
     */
    @VisibleForTesting
    protected ScheduledScenarioRunner getScheduledRunner(
            BlockJUnit4ClassRunner runner, Scenario scenario, long timeout, boolean shouldIdle) {
        Class<?> testClass = runner.getTestClass().getJavaClass();
        try {
            return new ScheduledScenarioRunner(testClass, scenario, timeout, shouldIdle);
        } catch (InitializationError e) {
            throw new RuntimeException(
                    String.format(
                            "Unable to run scenario %s with a scheduled runner.",
                            runner.getDescription().getDisplayName()),
                    e);
        }
    }
}
