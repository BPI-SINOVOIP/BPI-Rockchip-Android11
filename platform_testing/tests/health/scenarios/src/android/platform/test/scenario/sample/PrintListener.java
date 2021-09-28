/*
 * Copyright (C) 2020 The Android Open Source Project
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
 * limitations under the License
 */

package android.platform.test.scenario.sample;

import android.util.Log;

import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;

/** A {@link RunListener} that prints the methods it executes. */
public class PrintListener extends RunListener {

    private static final String LOG_TAG = SampleTest.LOG_TAG;

    @Override
    public void testRunStarted(Description description) throws Exception {
        Log.d(LOG_TAG, "RunListener#testRunStarted");
    }

    @Override
    public void testRunFinished(Result result) throws Exception {
        Log.d(LOG_TAG, "RunListener#testRunFinished");
    }

    @Override
    public void testStarted(Description description) throws Exception {
        Log.d(LOG_TAG, "RunListener#testStarted");
    }

    @Override
    public void testFinished(Description description) throws Exception {
        Log.d(LOG_TAG, "RunListener#testFinished");
    }

    @Override
    public void testFailure(Failure failure) throws Exception {
        Log.d(LOG_TAG, "RunListener#testFailure");
    }

    @Override
    public void testAssumptionFailure(Failure failure) {
        Log.d(LOG_TAG, "RunListener#testAssumptionFailure");
    }

    @Override
    public void testIgnored(Description description) throws Exception {
        Log.d(LOG_TAG, "RunListener#testIgnored");
    }
}
