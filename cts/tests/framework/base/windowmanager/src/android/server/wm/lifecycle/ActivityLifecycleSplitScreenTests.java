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

import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_ACTIVITY_RESULT;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_CREATE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_DESTROY;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_PAUSE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_POST_CREATE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESTART;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESUME;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_START;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_STOP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_TOP_POSITION_GAINED;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_TOP_POSITION_LOST;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.PRE_ON_CREATE;
import static android.server.wm.lifecycle.LifecycleVerifier.transition;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.platform.test.annotations.Presubmit;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ActivityLifecycleSplitScreenTests
 */
@MediumTest
@Presubmit
@android.server.wm.annotation.Group3
public class ActivityLifecycleSplitScreenTests extends ActivityLifecycleClientTestBase {

    @Before
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsSplitScreenMultiWindow());
        // TODO(b/149338177): Fix test to pass with organizer API.
        mUseTaskOrganizer = false;
    }

    @Test
    public void testResumedWhenRecreatedFromInNonFocusedStack() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Launch second activity to stop first
        final Activity secondActivity = launchActivityAndWait(SecondActivity.class);

        // Wait for the first activity to stop, so that this event is not included in the logs.
        waitAndAssertActivityStates(state(firstActivity, ON_STOP));

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(secondActivity);

        // CLear logs so we can capture just the destroy sequence
        getLifecycleLog().clear();

        // Start an activity in separate task (will be placed in secondary stack)
        new Launcher(ThirdActivity.class)
                .setFlags(FLAG_ACTIVITY_MULTIPLE_TASK | FLAG_ACTIVITY_NEW_TASK)
                .launch();

        // Wait for SecondActivity in primary split screen leave minimize dock.
        waitAndAssertActivityStates(state(secondActivity, ON_RESUME));

        // Finish top activity
        secondActivity.finish();

        waitAndAssertActivityStates(state(secondActivity, ON_DESTROY),
                state(firstActivity, ON_RESUME));

        // Verify that the first activity was recreated to resume as it was created before
        // windowing mode was switched
        LifecycleVerifier.assertRecreateAndResumeSequence(FirstActivity.class, getLifecycleLog());

        // Verify that the lifecycle state did not change for activity in non-focused stack
        LifecycleVerifier.assertLaunchSequence(ThirdActivity.class, getLifecycleLog());
    }

    @Test
    public void testOccludingOnSplitSecondaryStack() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        final ComponentName firstActivityName = getComponentName(FirstActivity.class);
        mWmState.computeState(firstActivityName);
        int primarySplitStack = mWmState.getStackIdByActivity(firstActivityName);

        // Launch second activity to side
        getLifecycleLog().clear();
        final Activity secondActivity = new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait for first activity to resume after being moved to split-screen.
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "launchToSide");

        // Launch third activity on top of second
        getLifecycleLog().clear();
        new Launcher(ThirdActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();
        waitAndAssertActivityStates(state(secondActivity, ON_STOP));
    }

    @Test
    public void testTranslucentOnSplitSecondaryStack() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        final ComponentName firstActivityName = getComponentName(FirstActivity.class);
        mWmState.computeState(firstActivityName);
        int primarySplitStack = mWmState.getStackIdByActivity(firstActivityName);

        // Launch second activity to side
        getLifecycleLog().clear();
        final Activity secondActivity = new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait for first activity to resume after being moved to split-screen.
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "launchToSide");

        // Launch translucent activity on top of second
        getLifecycleLog().clear();

        new Launcher(TranslucentActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();
        waitAndAssertActivityStates(state(secondActivity, ON_PAUSE));
    }

    @Test
    @Ignore // TODO(b/142345211): Skipping until the issue is fixed, or it will impact other tests.
    public void testResultInNonFocusedStack() throws Exception {
        // Launch first activity
        final Activity callbackTrackingActivity =
                launchActivityAndWait(CallbackTrackingActivity.class);

        // Enter split screen, the activity will be relaunched.
        // Start side activity so callbackTrackingActivity won't be paused due to minimized dock.
        moveTaskToPrimarySplitScreen(callbackTrackingActivity.getTaskId(),
            true/* showSideActivity */);
        getLifecycleLog().clear();

        // Launch second activity
        // Create an ActivityMonitor that catch ChildActivity and return mock ActivityResult:
        Instrumentation.ActivityMonitor activityMonitor = getInstrumentation()
                .addMonitor(SecondActivity.class.getName(), null /* activityResult */,
                        false /* block */);

        callbackTrackingActivity.startActivityForResult(
                new Intent(callbackTrackingActivity, SecondActivity.class), 1 /* requestCode */);

        // Wait for the ActivityMonitor to be hit
        final Activity secondActivity = getInstrumentation()
                .waitForMonitorWithTimeout(activityMonitor, 5 * 1000);

        // Wait for second activity to resume
        assertNotNull("Second activity should be started", secondActivity);
        waitAndAssertActivityStates(state(secondActivity, ON_RESUME));

        // Verify if the first activity stopped (since it is not currently visible)
        waitAndAssertActivityStates(state(callbackTrackingActivity, ON_STOP));

        // Start an activity in separate task (will be placed in secondary stack)
        new Launcher(ThirdActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Finish top activity and verify that activity below became focused.
        getLifecycleLog().clear();
        secondActivity.setResult(Activity.RESULT_OK);
        secondActivity.finish();

        // Check that activity was resumed and result was delivered
        waitAndAssertActivityStates(state(callbackTrackingActivity, ON_RESUME));
        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESTART, ON_START, ON_ACTIVITY_RESULT, ON_RESUME), "resume");
    }

    @Test
    public void testResumedWhenRestartedFromInNonFocusedStack() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Start an activity in separate task (will be placed in secondary stack)
        final Activity newTaskActivity = new Launcher(ThirdActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Launch second activity, first become stopped
        getLifecycleLog().clear();
        final Activity secondActivity = launchActivityAndWait(SecondActivity.class);

        // Wait for second activity to resume and first to stop
        waitAndAssertActivityStates(state(newTaskActivity, ON_STOP));

        // Finish top activity
        getLifecycleLog().clear();
        secondActivity.finish();

        waitAndAssertActivityStates(state(newTaskActivity, ON_RESUME));
        waitAndAssertActivityStates(state(secondActivity, ON_DESTROY));

        // Verify that the first activity was restarted to resumed state as it was brought back
        // after windowing mode was switched
        LifecycleVerifier.assertRestartAndResumeSequence(ThirdActivity.class, getLifecycleLog());
        LifecycleVerifier.assertResumeToDestroySequence(SecondActivity.class, getLifecycleLog());
    }

    @Test
    public void testResumedTranslucentWhenRestartedFromInNonFocusedStack() throws Exception {
        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreen(firstActivity.getTaskId(), true /* showSideActivity */);

        // Launch a translucent activity, first become paused
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);

        // Wait for first activity to pause
        waitAndAssertActivityStates(state(firstActivity, ON_PAUSE));

        // Start an activity in separate task (will be placed in secondary stack)
        new Launcher(ThirdActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        getLifecycleLog().clear();

        // Finish top activity
        translucentActivity.finish();

        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));
        waitAndAssertActivityStates(state(translucentActivity, ON_DESTROY));

        // Verify that the first activity was resumed
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "resume");
        LifecycleVerifier.assertResumeToDestroySequence(TranslucentActivity.class,
                getLifecycleLog());
    }

    @Test
    public void testLifecycleOnMoveToFromSplitScreenRelaunch() throws Exception {
        // Launch a singleTop activity
        launchActivityAndWait(CallbackTrackingActivity.class);

        // Wait for the activity to resume
        LifecycleVerifier.assertLaunchSequence(CallbackTrackingActivity.class, getLifecycleLog());

        // Enter split screen
        getLifecycleLog().clear();
        setActivityTaskWindowingMode(CALLBACK_TRACKING_ACTIVITY,
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);

        // Wait for the activity to relaunch and receive multi-window mode change
        final List<LifecycleLog.ActivityCallback> expectedEnterSequence =
                Arrays.asList(ON_TOP_POSITION_LOST, ON_PAUSE, ON_STOP, ON_DESTROY, PRE_ON_CREATE,
                        ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME, ON_TOP_POSITION_GAINED,
                        ON_MULTI_WINDOW_MODE_CHANGED, ON_TOP_POSITION_LOST, ON_PAUSE);
        waitForActivityTransitions(CallbackTrackingActivity.class, expectedEnterSequence);
        LifecycleVerifier.assertOrder(getLifecycleLog(), CallbackTrackingActivity.class,
                Arrays.asList(ON_TOP_POSITION_LOST, ON_PAUSE, ON_STOP, ON_DESTROY, ON_CREATE,
                        ON_RESUME), "moveToSplitScreen");
        LifecycleVerifier.assertTransitionObserved(getLifecycleLog(),
                transition(CallbackTrackingActivity.class, ON_MULTI_WINDOW_MODE_CHANGED),
                "moveToSplitScreen");

        // Exit split-screen
        getLifecycleLog().clear();
        setActivityTaskWindowingMode(CALLBACK_TRACKING_ACTIVITY, WINDOWING_MODE_FULLSCREEN);

        // Wait for the activity to relaunch and receive multi-window mode change
        final List<LifecycleLog.ActivityCallback> expectedExitSequence =
                Arrays.asList(ON_STOP, ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START,
                        ON_POST_CREATE, ON_RESUME, ON_PAUSE, ON_MULTI_WINDOW_MODE_CHANGED,
                        ON_RESUME, ON_TOP_POSITION_GAINED);
        waitForActivityTransitions(CallbackTrackingActivity.class, expectedExitSequence);
        LifecycleVerifier.assertOrder(getLifecycleLog(), CallbackTrackingActivity.class,
                Arrays.asList(ON_DESTROY, ON_CREATE, ON_MULTI_WINDOW_MODE_CHANGED),
                "moveFromSplitScreen");
        LifecycleVerifier.assertOrder(getLifecycleLog(), CallbackTrackingActivity.class,
                Arrays.asList(ON_RESUME, ON_TOP_POSITION_GAINED),
                "moveFromSplitScreen");
    }

    @Test
    public void testLifecycleOnMoveToFromSplitScreenNoRelaunch() throws Exception {

        // Launch activities and enter split screen. Launched an activity on
        // split-screen secondary stack to ensure the TOP_POSITION_LOST is send
        // prior to MULTI_WINDOW_MODE_CHANGED.
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().
                        setTargetActivity(getComponentName(ConfigChangeHandlingActivity.class)),
                getLaunchActivityBuilder().
                        setTargetActivity(getComponentName(SecondActivity.class)));

        // Wait for the activity to receive the change
        waitForActivityTransitions(ConfigChangeHandlingActivity.class,
                Arrays.asList(ON_TOP_POSITION_LOST, ON_MULTI_WINDOW_MODE_CHANGED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), ConfigChangeHandlingActivity.class,
                Arrays.asList(ON_MULTI_WINDOW_MODE_CHANGED, ON_TOP_POSITION_LOST),
                "moveToSplitScreen");

        // Exit split-screen
        getLifecycleLog().clear();
        setActivityTaskWindowingMode(CONFIG_CHANGE_HANDLING_ACTIVITY, WINDOWING_MODE_FULLSCREEN);

        // Wait for the activity to receive the change
        final List<LifecycleLog.ActivityCallback> expectedSequence =
                Arrays.asList(ON_TOP_POSITION_GAINED, ON_MULTI_WINDOW_MODE_CHANGED);
        waitForActivityTransitions(ConfigChangeHandlingActivity.class, expectedSequence);
        LifecycleVerifier.assertTransitionObserved(getLifecycleLog(),
                transition(ConfigChangeHandlingActivity.class, ON_MULTI_WINDOW_MODE_CHANGED),
                "exitSplitScreen");
        LifecycleVerifier.assertTransitionObserved(getLifecycleLog(),
                transition(ConfigChangeHandlingActivity.class, ON_TOP_POSITION_GAINED),
                "exitSplitScreen");
    }
}
