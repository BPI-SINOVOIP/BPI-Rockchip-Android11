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

package com.android.cts.runner;

import androidx.test.internal.runner.listener.InstrumentationRunListener;
import android.util.Log;
import org.junit.runner.Description;

/**
 * A {@link RunListener} for CrashParser. Dumps the test name to logs when
 * tests start.
 */
public class CrashParserRunListener extends InstrumentationRunListener {

    private static final String TAG = "CrashParserRunListener";

    // Constant must be kept in sync with CrashUtils.java
    public static final String NEW_TEST_ALERT = "New test starting with name: ";

    @Override
    public void testStarted(Description description) throws Exception {
        Log.i(TAG, NEW_TEST_ALERT + description.toString());
    }

}
