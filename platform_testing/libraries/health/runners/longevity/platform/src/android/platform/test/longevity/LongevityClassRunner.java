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

import android.os.Bundle;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.internal.runners.statements.RunAfters;
import org.junit.internal.runners.statements.RunBefores;
import org.junit.runner.Description;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.MultipleFailureException;
import org.junit.runners.model.Statement;
import org.junit.runner.notification.StoppedByUserException;

/**
 * A {@link BlockJUnit4ClassRunner} that runs the test class's {@link BeforeClass} methods as {@link
 * Before} methods and {@link AfterClass} methods as {@link After} methods for metric collection in
 * longevity tests.
 */
public class LongevityClassRunner extends BlockJUnit4ClassRunner {
    @VisibleForTesting static final String FILTER_OPTION = "exclude-class";
    @VisibleForTesting static final String ITERATION_SEP_OPTION = "iteration-separator";
    @VisibleForTesting static final String ITERATION_SEP_DEFAULT = "@";
    // A constant to indicate that the iteration number is not set.
    @VisibleForTesting static final int ITERATION_NOT_SET = -1;

    private final String[] mExcludedClasses;
    private String mIterationSep = ITERATION_SEP_DEFAULT;

    private boolean mTestFailed = true;
    private boolean mTestAttempted = false;
    // Iteration number.
    private int mIteration = ITERATION_NOT_SET;

    public LongevityClassRunner(Class<?> klass) throws InitializationError {
        this(klass, InstrumentationRegistry.getArguments());
    }

    @VisibleForTesting
    LongevityClassRunner(Class<?> klass, Bundle args) throws InitializationError {
        super(klass);
        mExcludedClasses =
                args.containsKey(FILTER_OPTION)
                        ? args.getString(FILTER_OPTION).split(",")
                        : new String[] {};
        mIterationSep =
                args.containsKey(ITERATION_SEP_OPTION)
                        ? args.getString(ITERATION_SEP_OPTION)
                        : mIterationSep;
    }

    /** Set the iteration of the test that this runner is running. */
    public void setIteration(int iteration) {
        mIteration = iteration;
    }

    /**
     * Utilized by tests to check that the iteration is set, independent of the description logic.
     */
    @VisibleForTesting
    int getIteration() {
        return mIteration;
    }

    /**
     * Override the parent {@code withBeforeClasses} method to be a no-op.
     *
     * <p>The {@link BeforeClass} methods will be included later as {@link Before} methods.
     */
    @Override
    protected Statement withBeforeClasses(Statement statement) {
        return statement;
    }

    /**
     * Override the parent {@code withAfterClasses} method to be a no-op.
     *
     * <p>The {@link AfterClass} methods will be included later as {@link After} methods.
     */
    @Override
    protected Statement withAfterClasses(Statement statement) {
        return new RunAfterClassMethodsOnTestFailure(
                statement, getTestClass().getAnnotatedMethods(AfterClass.class), null);
    }

    /**
     * Runs the {@link BeforeClass} methods before running all the {@link Before} methods of the
     * test class.
     */
    @Override
    protected Statement withBefores(FrameworkMethod method, Object target, Statement statement) {
        List<FrameworkMethod> allBeforeMethods = new ArrayList<>();
        allBeforeMethods.addAll(getTestClass().getAnnotatedMethods(BeforeClass.class));
        allBeforeMethods.addAll(getTestClass().getAnnotatedMethods(Before.class));
        return allBeforeMethods.isEmpty()
                ? statement
                : addRunBefores(statement, allBeforeMethods, target);
    }

    /**
     * Runs the {@link AfterClass} methods after running all the {@link After} methods of the test
     * class.
     */
    @Override
    protected Statement withAfters(FrameworkMethod method, Object target, Statement statement) {
        return addRunAfters(
                statement,
                getTestClass().getAnnotatedMethods(After.class),
                getTestClass().getAnnotatedMethods(AfterClass.class),
                target);
    }

    /** Factory method to return the {@link RunBefores} object. Exposed for testing only. */
    @VisibleForTesting
    protected RunBefores addRunBefores(
            Statement statement, List<FrameworkMethod> befores, Object target) {
        return new RunBefores(statement, befores, target);
    }

    /**
     * Factory method to return the {@link Statement} object for running "after" methods. Exposed
     * for testing only.
     */
    @VisibleForTesting
    protected Statement addRunAfters(
            Statement statement,
            List<FrameworkMethod> afterMethods,
            List<FrameworkMethod> afterClassMethods,
            Object target) {
        return new RunAfterMethods(statement, afterMethods, afterClassMethods, target);
    }

    @VisibleForTesting
    protected boolean hasTestFailed() {
        if (!mTestAttempted) {
            throw new IllegalStateException(
                    "Test success status should not be checked before the test is attempted.");
        }
        return mTestFailed;
    }

    @Override
    protected boolean isIgnored(FrameworkMethod child) {
        if (super.isIgnored(child)) return true;
        // Check if this class has been filtered.
        String name = getTestClass().getJavaClass().getCanonicalName();
        return Arrays.stream(mExcludedClasses)
                .map(f -> Pattern.compile(f).matcher(name))
                .anyMatch(Matcher::matches);
    }

    /**
     * {@link Statement} to run the statement and the {@link After} methods. If the test does not
     * fail, also runs the {@link AfterClass} method as {@link After} methods.
     */
    @VisibleForTesting
    class RunAfterMethods extends Statement {
        private final List<FrameworkMethod> mAfterMethods;
        private final List<FrameworkMethod> mAfterClassMethods;
        private final Statement mStatement;
        private final Object mTarget;

        public RunAfterMethods(
                Statement statement,
                List<FrameworkMethod> afterMethods,
                List<FrameworkMethod> afterClassMethods,
                Object target) {
            mStatement = statement;
            mAfterMethods = afterMethods;
            mAfterClassMethods = afterClassMethods;
            mTarget = target;
        }

        @Override
        public void evaluate() throws Throwable {
            Statement withAfters = new RunAfters(mStatement, mAfterMethods, mTarget);
            LongevityClassRunner.this.mTestAttempted = true;
            withAfters.evaluate();
            // If the evaluation fails, the part from here on will not be executed, and
            // RunAfterClassMethodsOnTestFailure will then know to run the @AfterClass methods.
            LongevityClassRunner.this.mTestFailed = false;
            invokeAndCollectErrors(mAfterClassMethods, mTarget);
        }
    }

    /**
     * {@link Statement} to run the {@link AfterClass} methods only in the event that a test failed.
     */
    @VisibleForTesting
    class RunAfterClassMethodsOnTestFailure extends Statement {
        private final List<FrameworkMethod> mAfterClassMethods;
        private final Statement mStatement;
        private final Object mTarget;

        public RunAfterClassMethodsOnTestFailure(
                Statement statement, List<FrameworkMethod> afterClassMethods, Object target) {
            mStatement = statement;
            mAfterClassMethods = afterClassMethods;
            mTarget = target;
        }

        @Override
        public void evaluate() throws Throwable {
            List<Throwable> errors = new ArrayList<>();
            boolean stoppedByUser = false;
            try {
                mStatement.evaluate();
            } catch (Throwable e) {
                if (e instanceof StoppedByUserException) {
                    stoppedByUser = true;
                }
                errors.add(e);
            } finally {
                if (!stoppedByUser && LongevityClassRunner.this.hasTestFailed()) {
                    errors.addAll(invokeAndCollectErrors(mAfterClassMethods, mTarget));
                }
            }
            MultipleFailureException.assertEmpty(errors);
        }
    }

    /** Invoke the list of methods and collect errors into a list. */
    @VisibleForTesting
    protected List<Throwable> invokeAndCollectErrors(List<FrameworkMethod> methods, Object target)
            throws Throwable {
        List<Throwable> errors = new ArrayList<>();
        for (FrameworkMethod method : methods) {
            try {
                method.invokeExplosively(target);
            } catch (Throwable e) {
                errors.add(e);
            }
        }
        return errors;
    }

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
        return addIterationIfEnabled(super.describeChild(method));
    }

    /** Rename the class name to add iterations if the renaming iteration option is enabled. */
    protected Description addIterationIfEnabled(Description input) {
        if (mIteration == ITERATION_NOT_SET) {
            return input;
        }
        return Description.createTestDescription(
                String.join(mIterationSep, input.getClassName(), String.valueOf(mIteration)),
                input.getMethodName());
    }
}
