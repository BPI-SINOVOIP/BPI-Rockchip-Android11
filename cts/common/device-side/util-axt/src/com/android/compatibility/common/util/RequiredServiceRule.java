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

package com.android.compatibility.common.util;

import static org.junit.Assume.assumeTrue;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;

import org.junit.AssumptionViolatedException;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Custom JUnit4 rule that does not run a test case if the device does not have a given service.
 *
 * <p>The tests are skipped by throwing a {@link AssumptionViolatedException}.  CTS test runners
 * will report this as a {@code ASSUMPTION_FAILED}.
 *
 * <p><b>NOTE:</b> it must be used as {@code Rule}, not {@code ClassRule}.
 *
 */
public class RequiredServiceRule implements TestRule {
    private static final String TAG = "RequiredServiceRule";

    private final String mService;
    private final boolean mHasService;

    /**
     * Creates a rule for the given service.
     */
    public RequiredServiceRule(@NonNull String service) {
        mService = service;
        mHasService = hasService(service);
    }

    @Override
    public Statement apply(@NonNull Statement base, @NonNull Description description) {
        return new Statement() {

            @Override
            public void evaluate() throws Throwable {
                if (!mHasService) {
                    Log.d(TAG, "skipping "
                            + description.getClassName() + "#" + description.getMethodName()
                            + " because device does not have service '" + mService + "'");
                    assumeTrue("Device does not have service '" + mService + "'",
                            mHasService);
                    return;
                }
                base.evaluate();
            }
        };
    }

    /**
     * Checks if the device has the given service.
     */
    public static boolean hasService(@NonNull String service) {
        // TODO: ideally should call SystemServiceManager directly, but we would need to open
        // some @Testing APIs for that.
        String command = "service check " + service;
        try {
            String commandOutput = SystemUtil.runShellCommand(
                    InstrumentationRegistry.getInstrumentation(), command);
            return !commandOutput.contains("not found");
        } catch (Exception e) {
            Log.w(TAG, "Exception running '" + command + "': " + e);
            return false;
        }
    }

    @Override
    public String toString() {
        return "RequiredServiceRule[" + mService + "]";
    }
}
