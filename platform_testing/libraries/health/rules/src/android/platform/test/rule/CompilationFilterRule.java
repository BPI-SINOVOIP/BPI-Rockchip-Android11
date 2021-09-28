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

package android.platform.test.rule;

import static java.util.stream.Collectors.joining;

import android.os.SystemClock;
import android.util.Log;
import androidx.annotation.VisibleForTesting;

import com.google.common.collect.ImmutableList;

import org.junit.runner.Description;
import org.junit.runners.model.InitializationError;

import java.util.HashSet;
import java.util.Set;

/** This rule compiles the applications with the specified filter, or skips if unspecified. */
public class CompilationFilterRule extends TestWatcher {
    //
    private static final String LOG_TAG = CompilationFilterRule.class.getSimpleName();
    // Compilation constants
    @VisibleForTesting static final String COMPILE_CMD_FORMAT = "cmd package compile -f -m %s %s";
    @VisibleForTesting static final String DUMP_PROFILE_CMD = "killall -s SIGUSR1 %s";
    private static final ImmutableList<String> COMPILE_FILTER_LIST =
            ImmutableList.of("speed", "speed-profile", "quicken", "verify");
    @VisibleForTesting static final String SPEED_PROFILE_FILTER = "speed-profile";
    private static final String PROFILE_SAVE_TIMEOUT = "profile-save-timeout";
    @VisibleForTesting static final String COMPILE_FILTER_OPTION = "compilation-filter";
    @VisibleForTesting static final String COMPILE_SUCCESS = "Success";
    private static Set<String> mCompiledTests = new HashSet<>();

    private final String[] mApplications;

    public CompilationFilterRule() throws InitializationError {
        throw new InitializationError("Must supply an application to compile.");
    }

    public CompilationFilterRule(String... applications) {
        mApplications = applications;
    }

    @Override
    protected void finished(Description description) {
        // Identify the filter option to use.
        String filter = getArguments().getString(COMPILE_FILTER_OPTION);
        // Default speed profile save timeout set to 5 secs.
        long profileSaveTimeout = Integer
                .parseInt(getArguments().getString(PROFILE_SAVE_TIMEOUT, "5000"));
        if (filter == null) {
            // No option provided, default to a no-op.
            Log.d(LOG_TAG, "Skipping complation because filter option is unset.");
            return;
        } else if (!COMPILE_FILTER_LIST.contains(filter)) {
            String filterOptions = COMPILE_FILTER_LIST.stream().collect(joining(", "));
            throw new IllegalArgumentException(
                    String.format(
                            "Unknown compiler filter: %s, not part of %s", filter, filterOptions));
        }
        // Profile varies based on the test even for the same app. Tracking the test id to make
        // sure the test compiled once after the first iteration of the test.
        String testId = description.getDisplayName();
        if (!mCompiledTests.contains(testId)) {
            for (String app : mApplications) {
                // For speed profile compilation, ART team recommended to wait for 5 secs when app
                // is in the foreground, dump the profile, wait for another 5 secs before
                // speed-profile compilation.
                if (filter.equalsIgnoreCase(SPEED_PROFILE_FILTER)) {
                    SystemClock.sleep(profileSaveTimeout);
                    // Send SIGUSR1 to force dumping a profile.
                    String response = executeShellCommand(String.format(DUMP_PROFILE_CMD, app));
                    if (!response.isEmpty()) {
                        Log.d(LOG_TAG,
                                String.format("Received dump profile cmd response: %s", response));
                        throw new RuntimeException(
                                String.format("Failed to dump profile %s.", app));
                    }
                    // killall is async, wait few seconds to let the app save the profile.
                    SystemClock.sleep(profileSaveTimeout);
                }
                String response = executeShellCommand(
                        String.format(COMPILE_CMD_FORMAT, filter, app));
                if (!response.contains(COMPILE_SUCCESS)) {
                    Log.d(LOG_TAG, String.format("Received compile cmd response: %s", response));
                    throw new RuntimeException(String.format("Failed to compile %s.", app));
                } else {
                    mCompiledTests.add(testId);
                }
            }
        } else {
            Log.d(LOG_TAG, String.format("Test %s already compiled", testId));
        }
    }
}
