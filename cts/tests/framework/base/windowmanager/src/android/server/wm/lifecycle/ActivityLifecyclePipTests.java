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

import static android.app.ActivityTaskManager.INVALID_STACK_ID;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_CREATE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_DESTROY;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_PAUSE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESTART;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESUME;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_START;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_STOP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.PRE_ON_CREATE;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.content.ComponentName;
import android.platform.test.annotations.Presubmit;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ActivityLifecyclePipTests
 */
@MediumTest
@Presubmit
@android.server.wm.annotation.Group3
public class ActivityLifecyclePipTests extends ActivityLifecycleClientTestBase {

    @Before
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsPip());
    }

    @Test
    public void testGoToPip() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Launch Pip-capable activity
        final Activity pipActivity = launchActivityAndWait(PipActivity.class);

        waitAndAssertActivityStates(state(firstActivity, ON_STOP));

        // Move activity to Picture-In-Picture
        getLifecycleLog().clear();
        final ComponentName pipActivityName = getComponentName(PipActivity.class);
        mWmState.computeState(pipActivityName);
        final int stackId = mWmState.getStackIdByActivity(pipActivityName);
        assertNotEquals(stackId, INVALID_STACK_ID);
        moveTopActivityToPinnedStack(stackId);

        // Wait and assert lifecycle
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME), state(pipActivity, ON_PAUSE));
        LifecycleVerifier.assertRestartAndResumeSequence(FirstActivity.class, getLifecycleLog());
        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(ON_PAUSE), "enterPip");
    }

    @Test
    public void testPipOnLaunch() throws Exception {
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

        final List<LifecycleLog.ActivityCallback> expectedSequence =
                Arrays.asList(ON_PAUSE, ON_RESUME);
        final List<LifecycleLog.ActivityCallback> extraCycleSequence =
                Arrays.asList(ON_PAUSE, ON_STOP, ON_RESTART, ON_START, ON_RESUME);
        LifecycleVerifier.assertSequenceMatchesOneOf(FirstActivity.class,
                getLifecycleLog(), Arrays.asList(expectedSequence, extraCycleSequence),
                "activityEnteringPipOnTop");
        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME, ON_PAUSE),
                "launchAndEnterPip");
    }

    @Test
    public void testDestroyPip() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Clear the log before launching to Pip
        getLifecycleLog().clear();

        // Launch Pip-capable activity and enter Pip immediately
        final Activity pipActivity = new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        // Wait and assert lifecycle
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Exit PiP
        getLifecycleLog().clear();
        pipActivity.finish();

        waitAndAssertActivityStates(state(pipActivity, ON_DESTROY));
        LifecycleVerifier.assertEmptySequence(FirstActivity.class, getLifecycleLog(), "finishPip");
        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(ON_STOP, ON_DESTROY), "finishPip");
    }

    @Test
    public void testLaunchBelowPip() throws Exception {
        // Launch Pip-capable activity and enter Pip immediately
        new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        // Launch a regular activity below
        getLifecycleLog().clear();
        new Launcher(FirstActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait and verify the sequence
        LifecycleVerifier.assertLaunchSequence(FirstActivity.class, getLifecycleLog());
        LifecycleVerifier.assertEmptySequence(PipActivity.class, getLifecycleLog(),
                "launchBelowPip");
    }

    @Test
    public void testIntoPipSameTask() throws Exception {
        // Launch Pip-capable activity and enter Pip immediately
        new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        // Launch a regular activity into same task
        getLifecycleLog().clear();
        new Launcher(FirstActivity.class)
                .setExpectedState(ON_PAUSE)
                // Skip launch time verification - it can be affected by PiP menu activity
                .setSkipLaunchTimeCheck()
                .launch();

        // Wait and verify the sequence
        waitAndAssertActivityStates(state(PipActivity.class, ON_STOP));

        // TODO(b/123013403): sometimes extra one or even more relaunches happen
        //final List<LifecycleLog.ActivityCallback> extraDestroySequence =
        //        Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME, ON_PAUSE, ON_STOP,
        //                ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME, ON_PAUSE);
        //waitForActivityTransitions(FirstActivity.class, extraDestroySequence);
        //final List<LifecycleLog.ActivityCallback> expectedSequence =
        //        Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME, ON_PAUSE);
        //LifecycleVerifier.assertSequenceMatchesOneOf(FirstActivity.class, getLifecycleLog(),
        //        Arrays.asList(extraDestroySequence, expectedSequence),
        //        "launchIntoPip");

        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(ON_STOP), "launchIntoPip");
    }

    @Test
    public void testDestroyBelowPip() throws Exception {
        // Launch a regular activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Launch Pip-capable activity and enter Pip immediately
        new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Destroy the activity below
        getLifecycleLog().clear();
        firstActivity.finish();
        waitAndAssertActivityStates(state(firstActivity, ON_DESTROY));
        LifecycleVerifier.assertResumeToDestroySequence(FirstActivity.class, getLifecycleLog());
        LifecycleVerifier.assertEmptySequence(PipActivity.class, getLifecycleLog(),
                "destroyBelowPip");
    }

    @Test
    public void testSplitScreenBelowPip() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // TODO(b/149338177): Fix test to pass with organizer API.
        mUseTaskOrganizer = false;
        // Launch Pip-capable activity and enter Pip immediately
        new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        // Launch first activity
        getLifecycleLog().clear();
        final Activity firstActivity = new Launcher(FirstActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();
        LifecycleVerifier.assertLaunchSequence(FirstActivity.class, getLifecycleLog());

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);
        // TODO(b/123013403): will fail with callback tracking enabled - delivers extra
        // MULTI_WINDOW_MODE_CHANGED
        LifecycleVerifier.assertEmptySequence(PipActivity.class, getLifecycleLog(),
                "launchBelow");

        // Launch second activity to side
        getLifecycleLog().clear();
        new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        LifecycleVerifier.assertLaunchSequence(SecondActivity.class, getLifecycleLog());
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "launchToSide");
        LifecycleVerifier.assertEmptySequence(PipActivity.class, getLifecycleLog(),
                "launchBelow");
    }

    @Test
    public void testPipAboveSplitScreen() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // TODO(b/149338177): Fix test to pass with organizer API.
        mUseTaskOrganizer = false;
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Launch second activity to side
        final Activity secondActivity = new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Launch Pip-capable activity and enter Pip immediately
        getLifecycleLog().clear();
        new Launcher(PipActivity.class)
                .setExpectedState(ON_PAUSE)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .launch();

        // Wait for it to launch and pause. Other activities should not be affected.
        waitAndAssertActivityStates(state(secondActivity, ON_RESUME));
        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME, ON_PAUSE),
                "launchAndEnterPip");
        LifecycleVerifier.assertEmptySequence(FirstActivity.class, getLifecycleLog(),
                "launchPipOnTop");
        final List<LifecycleLog.ActivityCallback> expectedSequence =
                Arrays.asList(ON_PAUSE, ON_RESUME);
        final List<LifecycleLog.ActivityCallback> extraCycleSequence =
                Arrays.asList(ON_PAUSE, ON_STOP, ON_RESTART, ON_START, ON_RESUME);
        // TODO(b/123013403): sometimes extra destroy is observed
        LifecycleVerifier.assertSequenceMatchesOneOf(SecondActivity.class,
                getLifecycleLog(), Arrays.asList(expectedSequence, extraCycleSequence),
                "activityEnteringPipOnTop");
    }
}
