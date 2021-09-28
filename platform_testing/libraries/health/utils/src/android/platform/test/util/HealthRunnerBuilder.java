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

import android.os.Bundle;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.lang.reflect.Constructor;

import org.junit.runner.Runner;
import org.junit.runners.JUnit4;
import org.junit.runners.model.RunnerBuilder;

/**
 * A {@link RunnerBuilder} that enables using a specified runner to run tests in a suite.
 *
 * <p>Usage: When instrumenting with AndroidJUnitRunner, use
 *
 * <pre>-e runnerBuilder android.platform.test.longevity.HealthRunnerBuilder -e use-runner
 * (name_of_runner)
 *
 * <p>The runner name can be supplied with the fully qualified class name of the runner. It also has
 * to be a {@link Runner} class and statically included in the APK.
 */
public class HealthRunnerBuilder extends RunnerBuilder {
    @VisibleForTesting static final String RUNNER_OPTION = "use-runner";
    // The runner class to use as specified by the option. Defaults to the JUnit4 runner.
    private Class<?> mRunnerClass = JUnit4.class;

    public HealthRunnerBuilder() {
        this(InstrumentationRegistry.getArguments());
    }

    @VisibleForTesting
    HealthRunnerBuilder(Bundle args) {
        String runnerName = args.getString(RUNNER_OPTION);
        if (runnerName != null) {
            Class<?> loadedClass = null;
            try {
                loadedClass = HealthRunnerBuilder.class.getClassLoader().loadClass(runnerName);
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(
                        String.format(
                                "Could not find class with fully qualified name %s.", runnerName));
            }
            // Ensure that the class found is a Runner.
            if (loadedClass != null && Runner.class.isAssignableFrom(loadedClass)) {
                mRunnerClass = loadedClass;
            } else {
                throw new RuntimeException(String.format("Class %s is not a runner.", loadedClass));
            }
        }
    }

    @Override
    public Runner runnerForClass(Class<?> testClass) throws Throwable {
        try {
            Constructor<?> runnerConstructor = mRunnerClass.getConstructor(Class.class);
            // Cast is safe as getRunnerClass() ensures that instantiations of its return type are
            // subclasses of Runner.
            return (Runner) runnerConstructor.newInstance(testClass);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(
                    String.format(
                            "Runner %s cannot be instantiated with the test class alone.",
                            mRunnerClass),
                    e);
        } catch (SecurityException e) {
            throw new RuntimeException(e);
        }
    }
}
