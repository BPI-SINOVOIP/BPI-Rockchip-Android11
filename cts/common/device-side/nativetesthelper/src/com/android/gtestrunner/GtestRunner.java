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

package com.android.gtestrunner;

import java.util.Iterator;
import java.util.List;
import org.junit.runner.Description;
import org.junit.runner.Runner;
import org.junit.runner.manipulation.Filter;
import org.junit.runner.manipulation.Filterable;
import org.junit.runner.manipulation.NoTestsRemainException;
import org.junit.runner.notification.RunNotifier;

/**
 * Custom Runner that implements a bridge between JUnit and GTest.
 * 
 * Use this Runner in a @RunWith annotation together with a @TargetLibrary
 * annotation on an empty class to create a CTS test that consists of native
 * tests written against the Google Test Framework. See the CTS module in
 * cts/tests/tests/nativehardware for an example.
 */
public class GtestRunner extends Runner implements Filterable {
    private static boolean sOnceFlag = false;

    private Class mTargetClass;
    private Description mDescription;

    public GtestRunner(Class testClass) {
        synchronized (GtestRunner.class) {
            if (sOnceFlag) {
                throw new IllegalStateException("Error multiple GtestRunners defined");
            }
            sOnceFlag = true;
        }

        mTargetClass = testClass;
        TargetLibrary library = (TargetLibrary) testClass.getAnnotation(TargetLibrary.class);
        if (library == null) {
            throw new IllegalStateException("Missing required @TargetLibrary annotation");
        }
        System.loadLibrary(library.value());
        mDescription = Description.createSuiteDescription(testClass);
        // The nInitialize native method will populate the description based on
        // GTest test data.
        nInitialize(testClass.getName(), mDescription);
    }

    @Override
    public Description getDescription() {
        return mDescription;
    }

    @Override
    public void filter(Filter filter) throws NoTestsRemainException {
        List<Description> children = mDescription.getChildren();
        mDescription = Description.createSuiteDescription(mTargetClass);
        for (Iterator<Description> iter = children.iterator(); iter.hasNext(); ) {
            Description testDescription = iter.next();
            if (filter.shouldRun(testDescription)) {
                mDescription.addChild(testDescription);
            }
        }
        if (mDescription.getChildren().isEmpty()) {
            throw new NoTestsRemainException();
        }
    }

    @Override
    public void run(RunNotifier notifier) {
        for (Description description : mDescription.getChildren()) {
            nAddTest(description.getMethodName());
        }
        nRun(mTargetClass.getName(), notifier);
    }

    private static native void nInitialize(String className, Description description);
    private static native void nAddTest(String testName);
    private static native boolean nRun(String className, RunNotifier notifier);
}
