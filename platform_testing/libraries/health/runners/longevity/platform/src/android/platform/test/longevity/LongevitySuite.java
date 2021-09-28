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

import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Bundle;
import android.platform.test.composer.Iterate;
import android.platform.test.composer.Shuffle;
import android.platform.test.longevity.listener.BatteryTerminator;
import android.platform.test.longevity.listener.ErrorTerminator;
import android.platform.test.longevity.listener.TimeoutTerminator;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiFunction;

import org.junit.internal.runners.ErrorReportingRunner;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.RunnerBuilder;

/**
 * {@inheritDoc}
 *
 * This class is used for constructing longevity suites that run on an Android device.
 */
public class LongevitySuite extends android.host.test.longevity.LongevitySuite {
    private static final String LOG_TAG = LongevitySuite.class.getSimpleName();

    public static final String RENAME_ITERATION_OPTION = "rename-iterations";
    private final boolean mRenameIterations;

    private Context mContext;

    // Cached {@link TimeoutTerminator} instance.
    private TimeoutTerminator mTimeoutTerminator;

    private final Map<Description, Integer> mIterations = new HashMap<>();

    /**
     * Takes a {@link Bundle} and maps all String K/V pairs into a {@link Map<String, String>}.
     *
     * @param bundle the input arguments to return in a {@link Map}
     * @return a {@code Map<String, String>} of all key, value pairs in {@code bundle}.
     */
    protected static final Map<String, String> toMap(Bundle bundle) {
        Map<String, String> result = new HashMap<>();
        for (String key : bundle.keySet()) {
            if (!bundle.containsKey(key)) {
                Log.w(LOG_TAG, String.format("Couldn't find value for option: %s", key));
            } else {
                // Arguments are assumed String <-> String
                result.put(key, bundle.getString(key));
            }
        }
        return result;
    }

    /**
     * Called reflectively on classes annotated with {@code @RunWith(LongevitySuite.class)}
     */
    public LongevitySuite(Class<?> klass, RunnerBuilder builder)
            throws InitializationError {
        this(
                klass,
                builder,
                new ArrayList<Runner>(),
                InstrumentationRegistry.getInstrumentation(),
                InstrumentationRegistry.getContext(),
                InstrumentationRegistry.getArguments());
    }

    /** Used to dynamically pass in test classes to run as part of the suite in subclasses. */
    public LongevitySuite(Class<?> klass, RunnerBuilder builder, List<Runner> additional)
            throws InitializationError {
        this(
                klass,
                builder,
                additional,
                InstrumentationRegistry.getInstrumentation(),
                InstrumentationRegistry.getContext(),
                InstrumentationRegistry.getArguments());
    }

    /**
     * Enables subclasses, e.g.{@link ProfileSuite}, to construct a suite using its own list of
     * Runners.
     */
    protected LongevitySuite(Class<?> klass, List<Runner> runners, Bundle args)
            throws InitializationError {
        super(klass, runners, toMap(args));
        mContext = InstrumentationRegistry.getContext();

        // Parse out additional options.
        mRenameIterations = Boolean.parseBoolean(args.getString(RENAME_ITERATION_OPTION));
    }

    /** Used to pass in mock-able Android features for testing. */
    @VisibleForTesting
    public LongevitySuite(
            Class<?> klass,
            RunnerBuilder builder,
            List<Runner> additional,
            Instrumentation instrumentation,
            Context context,
            Bundle arguments)
            throws InitializationError {
        this(klass, constructClassRunners(klass, additional, builder, arguments), arguments);
        // Overwrite instrumentation and context here with the passed-in objects.
        mContext = context;
    }

    /** Constructs the sequence of {@link Runner}s using platform composers. */
    private static List<Runner> constructClassRunners(
            Class<?> suite, List<Runner> additional, RunnerBuilder builder, Bundle args)
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
            // If a scenario is an ErrorReportingRunner, an InitializationError has occurred when
            // initializing the runner. Throw out a new error with the causes.
            if (runner instanceof ErrorReportingRunner) {
                throw new InitializationError(getCauses((ErrorReportingRunner) runner));
            }
            // All scenarios must extend BlockJUnit4ClassRunner.
            if (!(runner instanceof BlockJUnit4ClassRunner)) {
                throw new InitializationError(
                        String.format(
                                "All runners must extend BlockJUnit4ClassRunner. %s:%s doesn't.",
                                runner.getClass(), runner.getDescription().getDisplayName()));
            }
        }
        // Combine annotated runners and additional ones.
        List<Runner> runners = builder.runners(suite, annotation.value());
        runners.addAll(additional);
        // Apply the modifiers to construct the full suite.
        BiFunction<Bundle, List<Runner>, List<Runner>> modifier =
                new Iterate<Runner>().andThen(new Shuffle<Runner>());
        return modifier.apply(args, runners);
    }

    @Override
    public void run(final RunNotifier notifier) {
        // Register the battery terminator available only on the platform library, if present.
        if (hasBattery()) {
            notifier.addListener(new BatteryTerminator(notifier, mArguments, mContext));
        }
        // Register other listeners and continue with standard longevity run.
        super.run(notifier);
    }

    @Override
    protected void runChild(Runner runner, final RunNotifier notifier) {
        // Update iterations.
        mIterations.computeIfPresent(runner.getDescription(), (k, v) -> v + 1);
        mIterations.computeIfAbsent(runner.getDescription(), k -> 1);

        LongevityClassRunner suiteRunner = getSuiteRunner(runner);
        if (mRenameIterations) {
            suiteRunner.setIteration(mIterations.get(runner.getDescription()));
        }
        super.runChild(suiteRunner, notifier);
    }

    /** Returns the platform-specific {@link ErrorTerminator} for an Android device. */
    @Override
    public android.host.test.longevity.listener.ErrorTerminator getErrorTerminator(
            final RunNotifier notifier) {
        return new ErrorTerminator(notifier);
    }

    /**
     * Returns the platform-specific {@link TimeoutTerminator} for an Android device.
     *
     * <p>This method will always return the same {@link TimeoutTerminator} instance.
     */
    @Override
    public android.host.test.longevity.listener.TimeoutTerminator getTimeoutTerminator(
            final RunNotifier notifier) {
        if (mTimeoutTerminator == null) {
            mTimeoutTerminator = new TimeoutTerminator(notifier, mArguments);
        }
        return mTimeoutTerminator;
    }

    /** Returns the timeout set on the suite in milliseconds. */
    public long getSuiteTimeoutMs() {
        if (mTimeoutTerminator == null) {
            throw new IllegalStateException("No suite timeout is set. This should never happen.");
        }
        return mTimeoutTerminator.getTotalSuiteTimeoutMs();
    }

    /**
     * Returns a {@link Runner} specific for the suite, if any. Can be overriden by subclasses to
     * supply different runner implementations.
     */
    protected LongevityClassRunner getSuiteRunner(Runner runner) {
        try {
            // Cast is safe as we verified the runner is BlockJUnit4Runner at initialization.
            return new LongevityClassRunner(
                    ((BlockJUnit4ClassRunner) runner).getTestClass().getJavaClass());
        } catch (InitializationError e) {
            throw new RuntimeException(
                    String.format(
                            "Unable to run scenario %s with a longevity-specific runner.",
                            runner.getDescription().getDisplayName()),
                    e);
        }
    }

    /**
     * Determines if the device has a battery attached.
     */
    private boolean hasBattery () {
        final Intent batteryInfo =
                mContext.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        return batteryInfo.getBooleanExtra(BatteryManager.EXTRA_PRESENT, true);
    }

    /** Gets the first cause out of a {@link ErrorReportingRunner}. Also logs the rest. */
    private static List<Throwable> getCauses(ErrorReportingRunner runner) {
        // Reflection is used for this operation as the runner itself does not allow the errors
        // to be read directly, and masks everything as an InitializationError in its description,
        // which is not very useful.
        // It is ok to throw RuntimeException here as we have already entered a failure state. It is
        // helpful to know that a ErrorReportingRunner has occurred even if we can't decipher it.
        try {
            Field causesField = runner.getClass().getDeclaredField("causes");
            causesField.setAccessible(true);
            return (List<Throwable>) causesField.get(runner);
        } catch (NoSuchFieldException e) {
            throw new RuntimeException(
                    String.format(
                            "Unable to find a \"causes\" field in the ErrorReportingRunner %s.",
                            runner.getDescription()),
                    e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(
                    String.format(
                            "Unable to access the \"causes\" field in the ErrorReportingRunner %s.",
                            runner.getDescription()),
                    e);
        }
    }
}
