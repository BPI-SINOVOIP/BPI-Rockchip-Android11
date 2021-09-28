/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.server.wm;

import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.content.Intent.FLAG_ACTIVITY_SINGLE_TOP;
import static android.server.wm.ComponentNameUtils.getActivityName;
import static android.server.wm.app.Components.ENTRY_POINT_ALIAS_ACTIVITY;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.SINGLE_TASK_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.junit.Assert.assertNotEquals;

import android.content.ComponentName;
import android.platform.test.annotations.Presubmit;

import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:AmStartOptionsTests
 */
@Presubmit
public class AmStartOptionsTests extends ActivityManagerTestBase {

    @Test
    public void testDashD() {
        // Run at least 2 rounds to verify that -D works with an existing process.
        // -D could fail in this case if the force stop of process is broken.
        int prevProcId = -1;
        for (int i = 0; i < 2; i++) {
            executeShellCommand("am start -n " + getActivityName(TEST_ACTIVITY) + " -D");

            mWmState.waitForDebuggerWindowVisible(TEST_ACTIVITY);
            int procId = mWmState.getActivityProcId(TEST_ACTIVITY);

            assertThat("Invalid ProcId.", procId, greaterThanOrEqualTo(0));
            if (i > 0) {
                assertNotEquals("Run " + i + " didn't start new proc.", prevProcId, procId);
            }
            prevProcId = procId;
        }
    }

    @Test
    public void testDashW_Direct() throws Exception {
        testDashW(SINGLE_TASK_ACTIVITY, SINGLE_TASK_ACTIVITY);
    }

    @Test
    public void testDashW_Indirect() throws Exception {
        testDashW(ENTRY_POINT_ALIAS_ACTIVITY, SINGLE_TASK_ACTIVITY);
    }

    @Test
    public void testDashW_FinishingTop() {
        // Start LaunchingActivity and TestActivity
        getLaunchActivityBuilder().setLaunchingActivity(LAUNCHING_ACTIVITY)
                .setTargetActivity(TEST_ACTIVITY).execute();

        // Return to home
        launchHomeActivity();

        // Start LaunchingActivity again and finish TestActivity
        final int flags =
                FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TOP | FLAG_ACTIVITY_SINGLE_TOP;
        executeShellCommand("am start -W -f " + flags + " -n " + getActivityName(LAUNCHING_ACTIVITY)
                + " --display " + DEFAULT_DISPLAY);
        waitAndAssertTopResumedActivity(LAUNCHING_ACTIVITY, DEFAULT_DISPLAY,
                "Activity must be launched.");
    }

    private void testDashW(final ComponentName entryActivity, final ComponentName actualActivity)
            throws Exception {
        // Test cold start
        startActivityAndVerifyResult(entryActivity, actualActivity, true);

        // Test warm start
        launchHomeActivity();
        startActivityAndVerifyResult(entryActivity, actualActivity, false);

        // Test "hot" start (app already in front)
        startActivityAndVerifyResult(entryActivity, actualActivity, false);
    }

    private void startActivityAndVerifyResult(final ComponentName entryActivity,
            final ComponentName actualActivity, boolean shouldStart) {
        mWmState.waitForAppTransitionIdleOnDisplay(DEFAULT_DISPLAY);

        // Pass in different data only when cold starting. This is to make the intent
        // different in subsequent warm/hot launches, so that the entrypoint alias
        // activity is always started, but the actual activity is not started again
        // because of the NEW_TASK and singleTask flags.
        executeShellCommand("am start -n " + getActivityName(entryActivity) + " -W --display "
                + DEFAULT_DISPLAY + (shouldStart ? " -d about:blank" : ""));

        waitAndAssertTopResumedActivity(actualActivity, DEFAULT_DISPLAY,
                "Activity must be launched");
    }
}
