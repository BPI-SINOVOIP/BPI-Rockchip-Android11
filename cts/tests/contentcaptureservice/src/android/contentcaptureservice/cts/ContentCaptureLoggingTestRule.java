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

package android.contentcaptureservice.cts;

import static android.contentcaptureservice.cts.Helper.MY_PACKAGE;
import static android.contentcaptureservice.cts.Helper.TAG;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.util.Log;

import androidx.annotation.NonNull;

import com.android.compatibility.common.util.SafeCleanerRule;

import org.junit.AssumptionViolatedException;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Custom JUnit4 rule that improves ContentCapture-related logging by:
 *
 * <ol>
 *   <li>Setting logging level to verbose before test start.
 *   <li>Call {@code dumpsys} in case of failure.
 * </ol>
 */
public class ContentCaptureLoggingTestRule implements TestRule, SafeCleanerRule.Dumper {

    private boolean mDumped;

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {

            @Override
            public void evaluate() throws Throwable {
                final String testName = description.getDisplayName();
                try {
                    base.evaluate();
                } catch (Throwable t) {
                    dump(testName, t);
                    throw t;
                }
            }
        };
    }

    @Override
    public void dump(@NonNull String testName, @NonNull Throwable t) {
        if (mDumped) {
            Log.e(TAG, "dump(" + testName + "): already dumped");
            return;
        }
        if ((t instanceof AssumptionViolatedException)) {
            // This exception is used to indicate a test should be skipped and is
            // ignored by JUnit runners - we don't need to dump it...
            Log.w(TAG, "ignoring exception: " + t);
            return;
        }
        // TODO(b/123540602, b/120784831): should dump to a file (and integrate with tradefed)
        // instead of outputting to log directly...
        Log.e(TAG, "Dumping after exception on " + testName, t);
        final String serviceDump = runShellCommand("dumpsys content_capture");
        Log.e(TAG, "content_capture dump: \n" + serviceDump);
        final String activityDump = runShellCommand("dumpsys activity %s --contentcapture",
                MY_PACKAGE);
        Log.e(TAG, "activity dump: \n" + activityDump);
        mDumped = true;
    }
}
