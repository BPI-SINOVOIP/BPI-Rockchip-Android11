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
 * limitations under the License
 */

package android.server.wm.lifecycle;

import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_PAUSE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESTART;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESUME;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_START;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_STOP;

import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.platform.test.annotations.Presubmit;

import androidx.test.filters.MediumTest;

import org.junit.Test;

import java.util.Arrays;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ActivityLifecycleKeyguardTests
 */
@MediumTest
@Presubmit
@android.server.wm.annotation.Group3
public class ActivityLifecycleKeyguardTests extends ActivityLifecycleClientTestBase {

    @Test
    public void testSingleLaunch() throws Exception {
        assumeTrue(supportsSecureLock());
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();

            new Launcher(FirstActivity.class)
                    .setExpectedState(ON_STOP)
                    .setNoInstance()
                    .launch();
            LifecycleVerifier.assertLaunchAndStopSequence(FirstActivity.class, getLifecycleLog());
        }
    }

    @Test
    public void testKeyguardShowHide() throws Exception {
        assumeTrue(supportsSecureLock());

        // Launch first activity and wait for resume
        final Activity activity = launchActivityAndWait(FirstActivity.class);

        // Show and hide lock screen
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();
            waitAndAssertActivityStates(state(activity, ON_STOP));

            LifecycleVerifier.assertLaunchAndStopSequence(FirstActivity.class, getLifecycleLog());
            getLifecycleLog().clear();
        } // keyguard hidden

        // Verify that activity was resumed
        waitAndAssertActivityStates(state(activity, ON_RESUME));
        LifecycleVerifier.assertRestartAndResumeSequence(FirstActivity.class, getLifecycleLog());
    }

    @Test
    public void testKeyguardShowHideOverSplitScreen() throws Exception {
        assumeTrue(supportsSecureLock());
        assumeTrue(supportsSplitScreenMultiWindow());

        // TODO(b/149338177): Fix test to pass with organizer API.
        mUseTaskOrganizer = false;

        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Launch second activity to side
        final Activity secondActivity = new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Leaving the minimized dock, the stack state on the primary split screen should change
        // from Paused to Resumed.
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Show and hide lock screen
        getLifecycleLog().clear();
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();
            waitAndAssertActivityStates(state(firstActivity, ON_STOP));
            waitAndAssertActivityStates(state(secondActivity, ON_STOP));

            LifecycleVerifier.assertResumeToStopSequence(FirstActivity.class, getLifecycleLog());
            LifecycleVerifier.assertResumeToStopSequence(SecondActivity.class, getLifecycleLog());
            getLifecycleLog().clear();
        } // keyguard hidden

        waitAndAssertActivityStates(state(firstActivity, ON_RESUME),
                state(secondActivity, ON_RESUME));
        LifecycleVerifier.assertRestartAndResumeSequence(FirstActivity.class, getLifecycleLog());
        LifecycleVerifier.assertRestartAndResumeSequence(SecondActivity.class, getLifecycleLog());
    }

    @Test
    public void testKeyguardShowHideOverPip() throws Exception {
        assumeTrue(supportsSecureLock());
        if (!supportsPip()) {
            // Skipping test: no Picture-In-Picture support
            return;
        }

        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Clear the log before launching to Pip
        getLifecycleLog().clear();

        // Launch Pip-capable activity and enter Pip immediately
        new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        // Wait and assert lifecycle
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Show and hide lock screen
        getLifecycleLog().clear();
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();
            waitAndAssertActivityStates(state(firstActivity, ON_STOP));
            waitAndAssertActivityStates(state(PipActivity.class, ON_STOP));

            LifecycleVerifier.assertResumeToStopSequence(FirstActivity.class, getLifecycleLog());
            LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                    Arrays.asList(ON_STOP), "keyguardShown");
            getLifecycleLog().clear();
        } // keyguard hidden

        // Wait and assert lifecycle
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME),
                state(PipActivity.class, ON_PAUSE));
        LifecycleVerifier.assertRestartAndResumeSequence(FirstActivity.class, getLifecycleLog());
        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESTART, ON_START, ON_RESUME, ON_PAUSE), "keyguardGone");
    }
}
