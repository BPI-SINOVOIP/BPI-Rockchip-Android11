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
 * limitations under the License
 */

package android.server.wm.lifecycle;

import static android.app.WindowConfiguration.WINDOWING_MODE_PINNED;
import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.content.Intent.FLAG_ACTIVITY_REORDER_TO_FRONT;
import static android.server.wm.UiDeviceUtils.pressHomeButton;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_ACTIVITY_RESULT;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_CREATE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_DESTROY;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_NEW_INTENT;
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
import static android.view.Display.DEFAULT_DISPLAY;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Intent;
import android.os.SystemClock;
import android.platform.test.annotations.Presubmit;
import android.server.wm.WindowManagerState;
import android.server.wm.WindowManagerState.ActivityTask;
import android.server.wm.lifecycle.LifecycleLog.ActivityCallback;
import android.util.Pair;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link Activity#onTopResumedActivityChanged}.
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ActivityLifecycleTopResumedStateTests
 */
@MediumTest
@Presubmit
@android.server.wm.annotation.Group3
public class ActivityLifecycleTopResumedStateTests extends ActivityLifecycleClientTestBase {

    @Before
    public void setUp() throws Exception {
        super.setUp();
        // TODO(b/149338177): Fix test to pass with organizer API.
        mUseTaskOrganizer = false;
    }

    @Test
    public void testTopPositionAfterLaunch() throws Exception {
        launchActivityAndWait(CallbackTrackingActivity.class);

        LifecycleVerifier.assertLaunchSequence(CallbackTrackingActivity.class, getLifecycleLog());
    }

    @Test
    public void testTopPositionLostOnFinish() throws Exception {
        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        activity.finish();
        mWmState.waitForActivityRemoved(getComponentName(CallbackTrackingActivity.class));

        LifecycleVerifier.assertResumeToDestroySequence(CallbackTrackingActivity.class,
                getLifecycleLog());
    }

    @Test
    public void testTopPositionSwitchToActivityOnTop() throws Exception {
        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        final Activity topActivity = launchActivityAndWait(SingleTopActivity.class);
        waitAndAssertActivityStates(state(activity, ON_STOP));

        LifecycleVerifier.assertLaunchSequence(SingleTopActivity.class,
                CallbackTrackingActivity.class, getLifecycleLog(),
                false /* launchingIsTranslucent */);
    }

    @Test
    public void testTopPositionSwitchToActivityOnTopSlowDifferentProcess() throws Exception {
        // Launch first activity, which will be slow to release top position
        final Intent slowTopReleaseIntent = new Intent();
        slowTopReleaseIntent.putExtra(SlowActivity.EXTRA_CONTROL_FLAGS,
                SlowActivity.FLAG_SLOW_TOP_RESUME_RELEASE);

        final Activity firstActivity =
                mSlowActivityTestRule.launchActivity(slowTopReleaseIntent);
        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_GAINED));

        // Launch second activity on top
        getLifecycleLog().clear();
        final Class<? extends Activity> secondActivityClass =
                SecondProcessCallbackTrackingActivity.class;
        launchActivity(new ComponentName(firstActivity, secondActivityClass));

        // Wait and assert top position switch
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED),
                state(firstActivity, ON_STOP));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(firstActivity.getClass(), ON_TOP_POSITION_LOST),
                transition(secondActivityClass, ON_TOP_POSITION_GAINED)),
                "launchOnTop");
    }

    @Test
    public void testTopPositionSwitchToActivityOnTopTimeoutDifferentProcess() throws Exception {
        // Launch first activity, which will be slow to release top position
        final Intent slowTopReleaseIntent = new Intent();
        slowTopReleaseIntent.putExtra(SlowActivity.EXTRA_CONTROL_FLAGS,
                SlowActivity.FLAG_TIMEOUT_TOP_RESUME_RELEASE);

        final Activity firstActivity =
                mSlowActivityTestRule.launchActivity(slowTopReleaseIntent);
        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_GAINED));
        final Class<? extends Activity> firstActivityClass = firstActivity.getClass();

        // Launch second activity on top
        getLifecycleLog().clear();
        final Class<? extends Activity> secondActivityClass =
                SecondProcessCallbackTrackingActivity.class;
        launchActivity(new ComponentName(firstActivity, secondActivityClass));

        // Wait and assert top position switch,
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED),
                state(firstActivity, ON_STOP));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_GAINED),
                transition(firstActivityClass, ON_TOP_POSITION_LOST)),
                "launchOnTop");

        // Wait 5 seconds more to make sure that no new messages received after top resumed state
        // released by the slow activity
        getLifecycleLog().clear();
        Thread.sleep(5000);
        LifecycleVerifier.assertEmptySequence(firstActivityClass, getLifecycleLog(),
                "topStateLossTimeout");
        LifecycleVerifier.assertEmptySequence(secondActivityClass, getLifecycleLog(),
                "topStateLossTimeout");
    }

    @Test
    public void testTopPositionSwitchToTranslucentActivityOnTop() throws Exception {
        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        final Activity topActivity = launchActivityAndWait(
                TranslucentCallbackTrackingActivity.class);
        waitAndAssertActivityStates(state(activity, ON_PAUSE));

        LifecycleVerifier.assertLaunchSequence(TranslucentCallbackTrackingActivity.class,
                CallbackTrackingActivity.class, getLifecycleLog(),
                true /* launchingIsTranslucent */);
    }

    @Test
    public void testTopPositionSwitchOnDoubleLaunch() throws Exception {
        final Activity baseActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        new Launcher(LaunchForResultActivity.class)
                .setExpectedState(ON_STOP)
                .setNoInstance()
                .launch();

        waitAndAssertActivityStates(state(baseActivity, ON_STOP));

        final List<ActivityCallback> expectedTopActivitySequence =
                Arrays.asList(
                        PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED);
        waitForActivityTransitions(ResultActivity.class, expectedTopActivitySequence);

        final List<Pair<String, ActivityCallback>> observedTransitions =
                getLifecycleLog().getLog();
        final List<Pair<String, ActivityCallback>> expectedTransitions = Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_PAUSE),
                transition(LaunchForResultActivity.class, PRE_ON_CREATE),
                transition(LaunchForResultActivity.class, ON_CREATE),
                transition(LaunchForResultActivity.class, ON_START),
                transition(LaunchForResultActivity.class, ON_POST_CREATE),
                transition(LaunchForResultActivity.class, ON_RESUME),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_GAINED),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_LOST),
                transition(LaunchForResultActivity.class, ON_PAUSE),
                transition(ResultActivity.class, PRE_ON_CREATE),
                transition(ResultActivity.class, ON_CREATE),
                transition(ResultActivity.class, ON_START),
                transition(ResultActivity.class, ON_POST_CREATE),
                transition(ResultActivity.class, ON_RESUME),
                transition(ResultActivity.class, ON_TOP_POSITION_GAINED),
                transition(LaunchForResultActivity.class, ON_STOP),
                transition(CallbackTrackingActivity.class, ON_STOP));
        assertEquals("Double launch sequence must match", expectedTransitions, observedTransitions);
    }

    @Test
    public void testTopPositionSwitchOnDoubleLaunchAndTopFinish() throws Exception {
        final Activity baseActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        final Activity launchForResultActivity = new Launcher(LaunchForResultActivity.class)
                .customizeIntent(LaunchForResultActivity.forwardFlag(EXTRA_FINISH_IN_ON_RESUME))
                .launch();

        waitAndAssertActivityStates(state(baseActivity, ON_STOP));
        final List<ActivityCallback> expectedLaunchingSequence =
                Arrays.asList(
                        PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST, ON_PAUSE,
                        ON_ACTIVITY_RESULT, ON_RESUME, ON_TOP_POSITION_GAINED);
        waitForActivityTransitions(LaunchForResultActivity.class, expectedLaunchingSequence);

        final List<ActivityCallback> expectedTopActivitySequence =
                Arrays.asList(
                        PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED);
        waitForActivityTransitions(ResultActivity.class, expectedTopActivitySequence);

        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_PAUSE),
                transition(LaunchForResultActivity.class, PRE_ON_CREATE),
                transition(LaunchForResultActivity.class, ON_CREATE),
                transition(LaunchForResultActivity.class, ON_START),
                transition(LaunchForResultActivity.class, ON_POST_CREATE),
                transition(LaunchForResultActivity.class, ON_RESUME),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_GAINED),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_LOST),
                transition(LaunchForResultActivity.class, ON_PAUSE),
                transition(ResultActivity.class, PRE_ON_CREATE),
                transition(ResultActivity.class, ON_CREATE),
                transition(ResultActivity.class, ON_START),
                transition(ResultActivity.class, ON_POST_CREATE),
                transition(ResultActivity.class, ON_RESUME),
                transition(ResultActivity.class, ON_TOP_POSITION_GAINED),
                transition(ResultActivity.class, ON_TOP_POSITION_LOST),
                transition(ResultActivity.class, ON_PAUSE),
                transition(LaunchForResultActivity.class, ON_ACTIVITY_RESULT),
                transition(LaunchForResultActivity.class, ON_RESUME),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_GAINED),
                transition(ResultActivity.class, ON_STOP),
                transition(ResultActivity.class, ON_DESTROY),
                transition(CallbackTrackingActivity.class, ON_STOP)),
                getLifecycleLog(), "Double launch sequence must match");
    }

    @Test
    public void testTopPositionLostWhenDocked() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);
    }

    @Test
    public void testTopPositionSwitchToAnotherVisibleActivity() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Launch second activity to side
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait for first activity to resume after moving to primary split-screen
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));
        // First activity must be resumed, but not gain the top position
        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "unminimizeDockedStack");
        // Second activity must be on top now
        LifecycleVerifier.assertLaunchSequence(SingleTopActivity.class, getLifecycleLog());
    }

    @Test
    public void testTopPositionSwitchBetweenVisibleActivities() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Launch second activity to side
        getLifecycleLog().clear();
        final Activity secondActivity = new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait for first activity to resume after moving to primary split-screen
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Switch top between two activities
        getLifecycleLog().clear();
        new Launcher(CallbackTrackingActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setNoInstance()
                .launch();

        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_GAINED),
                state(secondActivity, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class, getLifecycleLog(),
                Arrays.asList(ON_TOP_POSITION_GAINED), "switchTop");
        LifecycleVerifier.assertSequence(SingleTopActivity.class, getLifecycleLog(),
                Arrays.asList(ON_TOP_POSITION_LOST), "switchTop");

        // Switch top again
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setNoInstance()
                .launch();

        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_LOST),
                state(secondActivity, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class, getLifecycleLog(),
                Arrays.asList(ON_TOP_POSITION_LOST), "switchTop");
        LifecycleVerifier.assertSequence(SingleTopActivity.class, getLifecycleLog(),
                Arrays.asList(ON_PAUSE, ON_NEW_INTENT, ON_RESUME, ON_TOP_POSITION_GAINED),
                "switchTop");
    }

    @Test
    public void testTopPositionNewIntent() throws Exception {
        // Launch single top activity
        launchActivityAndWait(SingleTopActivity.class);

        // Launch the activity again to observe new intent
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setNoInstance()
                .launch();

        waitAndAssertActivityTransitions(SingleTopActivity.class,
                Arrays.asList(
                        ON_TOP_POSITION_LOST, ON_PAUSE, ON_NEW_INTENT, ON_RESUME,
                        ON_TOP_POSITION_GAINED), "newIntent");
    }

    @Test
    public void testTopPositionNewIntentForStopped() throws Exception {
        // Launch single top activity
        final Activity singleTopActivity = launchActivityAndWait(SingleTopActivity.class);

        // Launch another activity on top
        final Activity topActivity = launchActivityAndWait(CallbackTrackingActivity.class);
        waitAndAssertActivityStates(state(singleTopActivity, ON_STOP));

        // Launch the single top activity again to observe new intent
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TOP)
                .setNoInstance()
                .launch();

        waitAndAssertActivityStates(state(topActivity, ON_DESTROY));

        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_PAUSE),
                transition(SingleTopActivity.class, ON_RESTART),
                transition(SingleTopActivity.class, ON_START),
                transition(SingleTopActivity.class, ON_NEW_INTENT),
                transition(SingleTopActivity.class, ON_RESUME),
                transition(SingleTopActivity.class, ON_TOP_POSITION_GAINED),
                transition(CallbackTrackingActivity.class, ON_STOP),
                transition(CallbackTrackingActivity.class, ON_DESTROY)),
                getLifecycleLog(), "Single top resolution sequence must match");
    }

    @Test
    public void testTopPositionNewIntentForPaused() throws Exception {
        // Launch single top activity
        final Activity singleTopActivity = launchActivityAndWait(SingleTopActivity.class);

        // Launch a translucent activity on top
        final Activity topActivity =
                launchActivityAndWait(TranslucentCallbackTrackingActivity.class);
        waitAndAssertActivityStates(state(singleTopActivity, ON_PAUSE));

        // Launch the single top activity again to observe new intent
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TOP)
                .setNoInstance()
                .launch();

        waitAndAssertActivityStates(state(topActivity, ON_DESTROY));

        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(TranslucentCallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(TranslucentCallbackTrackingActivity.class, ON_PAUSE),
                transition(SingleTopActivity.class, ON_NEW_INTENT),
                transition(SingleTopActivity.class, ON_RESUME),
                transition(SingleTopActivity.class, ON_TOP_POSITION_GAINED)),
                "Single top resolution sequence must match");
    }

    @Test
    public void testTopPositionSwitchWhenGoingHome() throws Exception {
        final Activity topActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Press HOME and verify the lifecycle
        getLifecycleLog().clear();
        pressHomeButton();
        waitAndAssertActivityStates(state(topActivity, ON_STOP));

        LifecycleVerifier.assertResumeToStopSequence(CallbackTrackingActivity.class,
                getLifecycleLog());
    }

    @Test
    public void testTopPositionSwitchOnTap() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // Launch first activity
        final Activity firstActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Launch second activity to side
        getLifecycleLog().clear();
        final Activity secondActivity = new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait for first activity to resume after moving to primary split-screen
        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));

        // Tap on first activity to switch the focus
        getLifecycleLog().clear();
        final ActivityTask dockedStack = getStackForTaskId(firstActivity.getTaskId());
        tapOnStackCenter(dockedStack);

        // Wait and assert focus switch
        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_GAINED),
                state(secondActivity, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(SingleTopActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED)),
                getLifecycleLog(), "Single top resolution sequence must match");

        // Tap on second activity to switch the focus again
        getLifecycleLog().clear();
        final ActivityTask sideStack = getStackForTaskId(secondActivity.getTaskId());
        tapOnStackCenter(sideStack);

        // Wait and assert focus switch
        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_LOST),
                state(secondActivity, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(SingleTopActivity.class, ON_TOP_POSITION_GAINED)),
                getLifecycleLog(), "Single top resolution sequence must match");
    }

    @Test
    public void testTopPositionSwitchOnTapSlowDifferentProcess() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // Launch first activity
        final Intent slowTopReleaseIntent = new Intent();
        slowTopReleaseIntent.putExtra(SlowActivity.EXTRA_CONTROL_FLAGS,
                SlowActivity.FLAG_SLOW_TOP_RESUME_RELEASE);
        final Activity firstActivity =
                mSlowActivityTestRule.launchActivity(slowTopReleaseIntent);
        waitAndAssertActivityStates(state(firstActivity, ON_TOP_POSITION_GAINED));
        final Class<? extends Activity> firstActivityClass = firstActivity.getClass();

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(firstActivity);

        // Launch second activity to side
        getLifecycleLog().clear();
        final Class<? extends Activity> secondActivityClass =
                SecondProcessCallbackTrackingActivity.class;
        final ComponentName secondActivityComponent =
                new ComponentName(firstActivity, secondActivityClass);
        getLaunchActivityBuilder()
                .setTargetActivity(secondActivityComponent)
                .setUseInstrumentation()
                .setNewTask(true)
                .setMultipleTask(true)
                .execute();

        // Wait and assert top position switch
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED));

        // Tap on first activity to switch the top resumed one
        getLifecycleLog().clear();
        final ActivityTask dockedStack = getStackForTaskId(firstActivity.getTaskId());
        tapOnStackCenter(dockedStack);

        // Wait and assert top resumed position switch
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_LOST),
                state(firstActivityClass, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_LOST),
                transition(firstActivityClass, ON_TOP_POSITION_GAINED)),
                "tapOnStack");

        // Tap on second activity to switch the top resumed activity again
        getLifecycleLog().clear();
        final ActivityTask sideTask = mWmState
                .getTaskByActivity(secondActivityComponent);
        final ActivityTask sideStack = getStackForTaskId(sideTask.getTaskId());
        tapOnStackCenter(sideStack);

        // Wait and assert top resumed position switch
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED),
                state(firstActivityClass, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(firstActivityClass, ON_TOP_POSITION_LOST),
                transition(secondActivityClass, ON_TOP_POSITION_GAINED)),
                "tapOnStack");
    }

    @Test
    public void testTopPositionSwitchOnTapTimeoutDifferentProcess() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        // Launch first activity
        final Intent slowTopReleaseIntent = new Intent();
        slowTopReleaseIntent.putExtra(SlowActivity.EXTRA_CONTROL_FLAGS,
                SlowActivity.FLAG_TIMEOUT_TOP_RESUME_RELEASE);
        final Activity slowActivity =
                mSlowActivityTestRule.launchActivity(slowTopReleaseIntent);
        waitAndAssertActivityStates(state(slowActivity, ON_TOP_POSITION_GAINED));
        final Class<? extends Activity> slowActivityClass = slowActivity.getClass();

        // Enter split screen
        moveTaskToPrimarySplitScreenAndVerify(slowActivity);

        // Launch second activity to side
        getLifecycleLog().clear();
        final Class<? extends Activity> secondActivityClass =
                SecondProcessCallbackTrackingActivity.class;
        final ComponentName secondActivityComponent =
                new ComponentName(slowActivity, secondActivityClass);
        getLaunchActivityBuilder()
                .setTargetActivity(secondActivityComponent)
                .setUseInstrumentation()
                .setNewTask(true)
                .setMultipleTask(true)
                .execute();

        // Wait and assert top position switch
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED));

        // Tap on first activity to switch the top resumed one
        getLifecycleLog().clear();
        final ActivityTask dockedStack = getStackForTaskId(slowActivity.getTaskId());
        tapOnStackCenter(dockedStack);

        // Wait and assert top resumed position switch.
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_LOST),
                state(slowActivityClass, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_LOST),
                transition(slowActivityClass, ON_TOP_POSITION_GAINED)),
                "tapOnStack");

        // Tap on second activity to switch the top resumed activity again
        getLifecycleLog().clear();
        final ActivityTask sideTask = mWmState
                .getTaskByActivity(secondActivityComponent);
        final ActivityTask sideStack = getStackForTaskId(sideTask.getTaskId());
        tapOnStackCenter(sideStack);

        // Wait and assert top resumed position switch. Because of timeout the new top position will
        // be reported to the first activity before second will finish handling it.
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED),
                state(slowActivityClass, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_GAINED),
                transition(slowActivityClass, ON_TOP_POSITION_LOST)),
                "tapOnStack");

        // Wait 5 seconds more to make sure that no new messages received after top resumed state
        // released by the slow activity
        getLifecycleLog().clear();
        Thread.sleep(5000);
        LifecycleVerifier.assertEmptySequence(slowActivityClass, getLifecycleLog(),
                "topStateLossTimeout");
        LifecycleVerifier.assertEmptySequence(secondActivityClass, getLifecycleLog(),
                "topStateLossTimeout");
    }

    @Test
    public void testTopPositionPreservedOnLocalRelaunch() throws Exception {
        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(activity::recreate);
        waitAndAssertActivityStates(state(activity, ON_TOP_POSITION_GAINED));

        LifecycleVerifier.assertRelaunchSequence(CallbackTrackingActivity.class, getLifecycleLog(),
                ON_TOP_POSITION_GAINED);
    }

    @Test
    public void testTopPositionLaunchedBehindLockScreen() throws Exception {
        assumeTrue(supportsSecureLock());

        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();

            new Launcher(CallbackTrackingActivity.class)
                    .setExpectedState(ON_STOP)
                    .setNoInstance()
                    .launch();
            LifecycleVerifier.assertLaunchAndStopSequence(CallbackTrackingActivity.class,
                    getLifecycleLog(), true /* onTop */);

            getLifecycleLog().clear();
        }

        // Lock screen removed - activity should be on top now
        waitAndAssertActivityStates(state(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertStopToResumeSequence(CallbackTrackingActivity.class,
                getLifecycleLog());
    }

    @Test
    public void testTopPositionRemovedBehindLockScreen() throws Exception {
        assumeTrue(supportsSecureLock());

        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        getLifecycleLog().clear();
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();

            waitAndAssertActivityStates(state(activity, ON_STOP));
            LifecycleVerifier.assertResumeToStopSequence(CallbackTrackingActivity.class,
                    getLifecycleLog());

            getLifecycleLog().clear();
        }

        // Lock screen removed - activity should be on top now
        waitAndAssertActivityStates(state(activity, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertStopToResumeSequence(CallbackTrackingActivity.class,
                getLifecycleLog());
    }

    @Test
    public void testTopPositionLaunchedOnTopOfLockScreen() throws Exception {
        assumeTrue(supportsSecureLock());

        final Activity showWhenLockedActivity;
        try (final LockScreenSession lockScreenSession = new LockScreenSession()) {
            lockScreenSession.setLockCredential().gotoKeyguard();

            showWhenLockedActivity =
                    launchActivityAndWait(ShowWhenLockedCallbackTrackingActivity.class);

            // TODO(b/123432490): Fix extra pause/resume
            LifecycleVerifier.assertSequence(ShowWhenLockedCallbackTrackingActivity.class,
                    getLifecycleLog(),
                    Arrays.asList(
                            PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                            ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST, ON_PAUSE, ON_RESUME,
                            ON_TOP_POSITION_GAINED),
                    "launchAboveKeyguard");

            getLifecycleLog().clear();
        }

        // Lock screen removed, but nothing should change.
        // Wait for something here, but don't expect anything to happen.
        waitAndAssertActivityStates(state(showWhenLockedActivity, ON_DESTROY));
        LifecycleVerifier.assertResumeToDestroySequence(
                ShowWhenLockedCallbackTrackingActivity.class, getLifecycleLog());
    }

    @Test
    public void testTopPositionSwitchAcrossDisplays() throws Exception {
        assumeTrue(supportsMultiDisplay());

        // Launch activity on default display.
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(DEFAULT_DISPLAY);
        new Launcher(CallbackTrackingActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setOptions(launchOptions)
                .launch();

        waitAndAssertTopResumedActivity(getComponentName(CallbackTrackingActivity.class),
                DEFAULT_DISPLAY, "Activity launched on default display must be focused");
        waitAndAssertActivityTransitions(CallbackTrackingActivity.class,
                LifecycleVerifier.getLaunchSequence(CallbackTrackingActivity.class), "launch");

        try (final VirtualDisplaySession virtualDisplaySession = new VirtualDisplaySession()) {
            // Create new simulated display
            final WindowManagerState.DisplayContent newDisplay
                    = virtualDisplaySession.setSimulateDisplay(true).createDisplay();

            // Launch another activity on new secondary display.
            getLifecycleLog().clear();
            launchOptions.setLaunchDisplayId(newDisplay.mId);
            new Launcher(SingleTopActivity.class)
                    .setFlags(FLAG_ACTIVITY_NEW_TASK)
                    .setOptions(launchOptions)
                    .launch();
            waitAndAssertTopResumedActivity(getComponentName(SingleTopActivity.class),
                    newDisplay.mId, "Activity launched on secondary display must be focused");

            waitAndAssertActivityTransitions(SingleTopActivity.class,
                    LifecycleVerifier.getLaunchSequence(SingleTopActivity.class), "launch");
            LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                    transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                    transition(SingleTopActivity.class, ON_TOP_POSITION_GAINED)),
                    "launchOnOtherDisplay");

            getLifecycleLog().clear();
        }

        // Secondary display was removed - activity will be moved to the default display
        waitForActivityTransitions(SingleTopActivity.class,
                LifecycleVerifier.getRelaunchSequence(
                        ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(SingleTopActivity.class, ON_TOP_POSITION_LOST),
                transition(SingleTopActivity.class, ON_TOP_POSITION_GAINED)),
                "hostingDisplayRemoved");
    }

    @Test
    public void testTopPositionSwitchAcrossDisplaysOnTap() throws Exception {
        assumeTrue(supportsMultiDisplay());

        // Launch activity on default display.
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(DEFAULT_DISPLAY);
        new Launcher(CallbackTrackingActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setOptions(launchOptions)
                .launch();

        waitAndAssertTopResumedActivity(getComponentName(CallbackTrackingActivity.class),
                DEFAULT_DISPLAY, "Activity launched on default display must be focused");

        // Create new simulated display
        final WindowManagerState.DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Launch another activity on new secondary display.
        getLifecycleLog().clear();
        launchOptions.setLaunchDisplayId(newDisplay.mId);
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setOptions(launchOptions)
                .launch();
        waitAndAssertTopResumedActivity(getComponentName(SingleTopActivity.class),
                newDisplay.mId, "Activity launched on secondary display must be focused");

        getLifecycleLog().clear();

        // Tap on default display to switch the top activity
        tapOnDisplayCenter(DEFAULT_DISPLAY);

        // Wait and assert focus switch
        waitAndAssertActivityTransitions(SingleTopActivity.class,
                Arrays.asList(ON_TOP_POSITION_LOST), "tapOnFocusSwitch");
        waitAndAssertActivityTransitions(CallbackTrackingActivity.class,
                Arrays.asList(ON_TOP_POSITION_GAINED), "tapOnFocusSwitch");
        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(SingleTopActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED)),
                getLifecycleLog(), "Top activity must be switched on tap");

        getLifecycleLog().clear();

        // Tap on new display to switch the top activity
        tapOnDisplayCenter(newDisplay.mId);

        // Wait and assert focus switch
        waitAndAssertActivityTransitions(CallbackTrackingActivity.class,
                Arrays.asList(ON_TOP_POSITION_LOST), "tapOnFocusSwitch");
        waitAndAssertActivityTransitions(SingleTopActivity.class,
                Arrays.asList(ON_TOP_POSITION_GAINED), "tapOnFocusSwitch");
        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(SingleTopActivity.class, ON_TOP_POSITION_GAINED)),
                getLifecycleLog(), "Top activity must be switched on tap");
    }

    @Test
    public void testTopPositionSwitchAcrossDisplaysOnTapSlowDifferentProcess() {
        assumeTrue(supportsMultiDisplay());

        // Create new simulated display.
        final WindowManagerState.DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Launch an activity on new secondary display.
        final Class<? extends Activity> secondActivityClass =
                SecondProcessCallbackTrackingActivity.class;
        final ComponentName secondActivityComponent =
                new ComponentName(mTargetContext, secondActivityClass);

        getLaunchActivityBuilder()
                .setTargetActivity(secondActivityComponent)
                .setUseInstrumentation()
                .setDisplayId(newDisplay.mId)
                .execute();
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED));

        // Launch activity on default display, which will be slow to release top position.
        getLifecycleLog().clear();
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(DEFAULT_DISPLAY);
        final Class<? extends Activity> defaultActivityClass = SlowActivity.class;
        final Intent defaultDisplaySlowIntent = new Intent(mContext, defaultActivityClass);
        defaultDisplaySlowIntent.setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK);
        defaultDisplaySlowIntent.putExtra(SlowActivity.EXTRA_CONTROL_FLAGS,
                SlowActivity.FLAG_SLOW_TOP_RESUME_RELEASE);
        mTargetContext.startActivity(defaultDisplaySlowIntent, launchOptions.toBundle());

        waitAndAssertTopResumedActivity(getComponentName(SlowActivity.class),
                DEFAULT_DISPLAY, "Activity launched on default display must be focused");

        // Wait and assert focus switch
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_LOST),
                state(defaultActivityClass, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_LOST),
                transition(defaultActivityClass, ON_TOP_POSITION_GAINED)),
                "launchOnDifferentDisplay");

        // Tap on secondary display to switch the top activity.
        getLifecycleLog().clear();
        tapOnDisplayCenter(newDisplay.mId);

        // Wait and assert top resumed position switch.
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED),
                state(defaultActivityClass, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(defaultActivityClass, ON_TOP_POSITION_LOST),
                transition(secondActivityClass, ON_TOP_POSITION_GAINED)),
                "tapOnDifferentDisplay");

        // Tap on default display to switch the top activity again.
        getLifecycleLog().clear();
        tapOnDisplayCenter(DEFAULT_DISPLAY);

        // Wait and assert top resumed position switch.
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_LOST),
                state(defaultActivityClass, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_LOST),
                transition(defaultActivityClass, ON_TOP_POSITION_GAINED)),
                "tapOnDifferentDisplay");
    }

    @Test
    public void testTopPositionSwitchAcrossDisplaysOnTapTimeoutDifferentProcess() {
        assumeTrue(supportsMultiDisplay());

        // Create new simulated display.
        final WindowManagerState.DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Launch an activity on new secondary display.
        final Class<? extends Activity> secondActivityClass =
                SecondProcessCallbackTrackingActivity.class;
        final ComponentName secondActivityComponent =
                new ComponentName(mTargetContext, secondActivityClass);

        getLaunchActivityBuilder()
                .setTargetActivity(secondActivityComponent)
                .setUseInstrumentation()
                .setDisplayId(newDisplay.mId)
                .execute();
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED));

        // Launch activity on default display, which will be slow to release top position.
        getLifecycleLog().clear();
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(DEFAULT_DISPLAY);
        final Class<? extends Activity> defaultActivityClass = SlowActivity.class;
        final Intent defaultDisplaySlowIntent = new Intent(mContext, defaultActivityClass);
        defaultDisplaySlowIntent.setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK);
        defaultDisplaySlowIntent.putExtra(SlowActivity.EXTRA_CONTROL_FLAGS,
                SlowActivity.FLAG_TIMEOUT_TOP_RESUME_RELEASE);
        mTargetContext.startActivity(defaultDisplaySlowIntent, launchOptions.toBundle());

        waitAndAssertTopResumedActivity(getComponentName(SlowActivity.class),
                DEFAULT_DISPLAY, "Activity launched on default display must be focused");

        // Wait and assert focus switch.
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_LOST),
                state(defaultActivityClass, ON_TOP_POSITION_GAINED));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_LOST),
                transition(defaultActivityClass, ON_TOP_POSITION_GAINED)),
                "launchOnDifferentDisplay");

        // Tap on secondary display to switch the top activity.
        getLifecycleLog().clear();
        tapOnDisplayCenter(newDisplay.mId);

        // Wait and assert top resumed position switch. Because of timeout top position gain
        // will appear before top position loss handling is finished.
        waitAndAssertActivityStates(state(secondActivityClass, ON_TOP_POSITION_GAINED),
                state(defaultActivityClass, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(secondActivityClass, ON_TOP_POSITION_GAINED),
                transition(defaultActivityClass, ON_TOP_POSITION_LOST)),
                "tapOnDifferentDisplay");

        // Wait 5 seconds more to make sure that no new messages received after top resumed state
        // released by the slow activity
        getLifecycleLog().clear();
        SystemClock.sleep(5000);
        LifecycleVerifier.assertEmptySequence(defaultActivityClass, getLifecycleLog(),
                "topStateLossTimeout");
        LifecycleVerifier.assertEmptySequence(secondActivityClass, getLifecycleLog(),
                "topStateLossTimeout");
    }

    @Test
    public void testFinishOnDifferentDisplay_nonFocused() throws Exception {
        assumeTrue(supportsMultiDisplay());

        // Launch activity on some display.
        final Activity callbackTrackingActivity =
                launchActivityAndWait(CallbackTrackingActivity.class);

        waitAndAssertTopResumedActivity(getComponentName(CallbackTrackingActivity.class),
                DEFAULT_DISPLAY, "Activity launched on default display must be focused");

        // Create new simulated display.
        final WindowManagerState.DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Launch another activity on new secondary display.
        getLifecycleLog().clear();
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(newDisplay.mId);
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setOptions(launchOptions)
                .launch();
        waitAndAssertTopResumedActivity(getComponentName(SingleTopActivity.class),
                newDisplay.mId, "Activity launched on secondary display must be focused");
        // An activity is launched on the new display, so the activity on default display should
        // lose the top state.
        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class, getLifecycleLog(),
                Arrays.asList(ON_TOP_POSITION_LOST), "launchFocusSwitch");

        // Finish the activity on the default display.
        getLifecycleLog().clear();
        callbackTrackingActivity.finish();

        // Verify that activity was actually destroyed.
        waitAndAssertActivityStates(state(CallbackTrackingActivity.class, ON_DESTROY));
        // Verify that the original focused display is not affected by the finished activity on
        // non-focused display.
        LifecycleVerifier.assertEmptySequence(SingleTopActivity.class, getLifecycleLog(),
                "destructionOnDifferentDisplay");
    }

    @Test
    public void testFinishOnDifferentDisplay_focused() throws Exception {
        assumeTrue(supportsMultiDisplay());

        // Launch activity on some display.
        final Activity bottomActivity = launchActivityAndWait(SecondActivity.class);
        final Activity callbackTrackingActivity =
                launchActivityAndWait(CallbackTrackingActivity.class);

        waitAndAssertTopResumedActivity(getComponentName(CallbackTrackingActivity.class),
                DEFAULT_DISPLAY, "Activity launched on default display must be focused");

        // Create new simulated display.
        final WindowManagerState.DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Launch another activity on new secondary display.
        getLifecycleLog().clear();
        final ActivityOptions launchOptions = ActivityOptions.makeBasic();
        launchOptions.setLaunchDisplayId(newDisplay.mId);
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setOptions(launchOptions)
                .launch();
        waitAndAssertTopResumedActivity(getComponentName(SingleTopActivity.class),
                newDisplay.mId, "Activity launched on secondary display must be focused");

        // Bring the focus back.
        final Intent sameInstanceIntent = new Intent(mContext, CallbackTrackingActivity.class);
        sameInstanceIntent.setFlags(FLAG_ACTIVITY_REORDER_TO_FRONT);
        bottomActivity.startActivity(sameInstanceIntent, null);
        waitAndAssertActivityStates(state(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED));

        // Finish the focused activity
        getLifecycleLog().clear();
        callbackTrackingActivity.finish();

        // Verify that lifecycle of the activity on a different display did not change.
        // Top resumed state will be given to home activity on that display.
        waitAndAssertActivityStates(state(CallbackTrackingActivity.class, ON_DESTROY),
                state(SecondActivity.class, ON_RESUME));
        LifecycleVerifier.assertEmptySequence(SingleTopActivity.class, getLifecycleLog(),
                "destructionOnDifferentDisplay");
    }

    @Test
    public void testTopPositionNotSwitchedToPip() throws Exception {
        assumeTrue(supportsPip());

        // Launch first activity
        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Clear the log before launching to Pip
        getLifecycleLog().clear();

        // Launch Pip-capable activity and enter Pip immediately
        final Activity pipActivity = new Launcher(PipActivity.class)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .setExpectedState(ON_PAUSE)
                .launch();

        // The PipMenuActivity could start anytime after moving pipActivity to pinned stack,
        // however, we cannot control when would it start or finish, so this test could fail when
        // PipMenuActivity just start and pipActivity call finish almost at the same time.
        // So the strategy here is to wait the PipMenuActivity start and finish after pipActivity
        // moved to pinned stack and paused, because pipActivity is not focusable but the
        // PipMenuActivity is focusable, when the pinned stack gain top focus means the
        // PipMenuActivity is launched and resumed, then when pinned stack lost top focus means the
        // PipMenuActivity is finished.
        mWmState.waitWindowingModeTopFocus(WINDOWING_MODE_PINNED, true /* topFocus */
                , "wait PipMenuActivity get top focus");
        mWmState.waitWindowingModeTopFocus(WINDOWING_MODE_PINNED, false /* topFocus */
                , "wait PipMenuActivity lost top focus");
        waitAndAssertActivityStates(state(activity, ON_TOP_POSITION_GAINED));

        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_PAUSE),
                transition(CallbackTrackingActivity.class, ON_RESUME),
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED)), "startPIP");

        // Exit PiP
        getLifecycleLog().clear();
        pipActivity.finish();

        waitAndAssertActivityStates(state(pipActivity, ON_DESTROY));
        LifecycleVerifier.assertSequence(PipActivity.class, getLifecycleLog(),
                Arrays.asList(
                        ON_STOP, ON_DESTROY), "finishPip");
        LifecycleVerifier.assertEmptySequence(CallbackTrackingActivity.class, getLifecycleLog(),
                "finishPip");
    }

    @Test
    public void testTopPositionForAlwaysFocusableActivityInPip() throws Exception {
        assumeTrue(supportsPip());

        // Launch first activity
        final Activity activity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Clear the log before launching to Pip
        getLifecycleLog().clear();

        // Launch Pip-capable activity and enter Pip immediately
        final Activity pipActivity = new Launcher(PipActivity.class)
                .setExtraFlags(EXTRA_ENTER_PIP)
                .setExpectedState(ON_PAUSE)
                .launch();

        // Launch always focusable activity into PiP

        // Notice that do not clear the lifecycle log here, because it may clear the event
        // ON_TOP_POSITION_LOST of CallbackTrackingActivity if PipMenuActivity is started earlier.
        final Activity alwaysFocusableActivity =
                launchActivityAndWait(AlwaysFocusablePipActivity.class);
        waitAndAssertActivityStates(state(pipActivity, ON_STOP),
                state(activity, ON_TOP_POSITION_LOST));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_LOST),
                transition(AlwaysFocusablePipActivity.class, ON_TOP_POSITION_GAINED)),
                "launchAlwaysFocusablePip");

        // Finish always focusable activity - top position should go back to fullscreen activity
        getLifecycleLog().clear();
        alwaysFocusableActivity.finish();

        waitAndAssertActivityStates(state(alwaysFocusableActivity, ON_DESTROY),
                state(activity, ON_TOP_POSITION_GAINED), state(pipActivity, ON_PAUSE));
        LifecycleVerifier.assertResumeToDestroySequence(AlwaysFocusablePipActivity.class,
                getLifecycleLog());
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                transition(AlwaysFocusablePipActivity.class, ON_TOP_POSITION_LOST),
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED)),
                "finishAlwaysFocusablePip");
    }
}
