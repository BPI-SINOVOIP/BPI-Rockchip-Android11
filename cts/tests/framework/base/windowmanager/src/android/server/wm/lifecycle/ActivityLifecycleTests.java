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

import static android.app.Instrumentation.ActivityMonitor;
import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.WindowManagerState.STATE_PAUSED;
import static android.server.wm.WindowManagerState.STATE_STOPPED;
import static android.server.wm.UiDeviceUtils.pressBackButton;
import static android.server.wm.lifecycle.ActivityLifecycleClientTestBase.LaunchForResultActivity.EXTRA_LAUNCH_ON_RESULT;
import static android.server.wm.lifecycle.ActivityLifecycleClientTestBase.LaunchForResultActivity.EXTRA_LAUNCH_ON_RESUME_AFTER_RESULT;
import static android.server.wm.lifecycle.ActivityLifecycleClientTestBase.NoDisplayActivity.EXTRA_LAUNCH_ACTIVITY;
import static android.server.wm.lifecycle.ActivityLifecycleClientTestBase.NoDisplayActivity.EXTRA_NEW_TASK;
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
import static android.view.Surface.ROTATION_0;
import static android.view.Surface.ROTATION_180;
import static android.view.Surface.ROTATION_270;
import static android.view.Surface.ROTATION_90;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.fail;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.platform.test.annotations.Presubmit;

import androidx.test.filters.FlakyTest;
import androidx.test.filters.MediumTest;

import com.android.compatibility.common.util.AmUtils;

import org.junit.Test;

import java.util.Arrays;
import java.util.List;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ActivityLifecycleTests
 */
@MediumTest
@Presubmit
@android.server.wm.annotation.Group3
public class ActivityLifecycleTests extends ActivityLifecycleClientTestBase {

    @Test
    public void testSingleLaunch() throws Exception {
        launchActivityAndWait(FirstActivity.class);

        LifecycleVerifier.assertLaunchSequence(FirstActivity.class, getLifecycleLog());
    }

    @Test
    public void testLaunchOnTop() throws Exception {
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        getLifecycleLog().clear();
        launchActivityAndWait(SecondActivity.class);
        waitAndAssertActivityStates(state(firstActivity, ON_STOP));

        LifecycleVerifier.assertLaunchSequence(SecondActivity.class, FirstActivity.class,
                getLifecycleLog(), false /* launchIsTranslucent */);
    }

    @Test
    public void testLaunchTranslucentOnTop() throws Exception {
        // Launch fullscreen activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Launch translucent activity on top
        getLifecycleLog().clear();
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        waitAndAssertActivityStates(occludedActivityState(firstActivity, translucentActivity));

        LifecycleVerifier.assertLaunchSequence(TranslucentActivity.class, FirstActivity.class,
                getLifecycleLog(), true /* launchIsTranslucent */);
    }

    @Test
    public void testLaunchDoubleTranslucentOnTop() throws Exception {
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);

        // Launch translucent activity on top
        getLifecycleLog().clear();
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        waitAndAssertActivityStates(occludedActivityState(firstActivity, translucentActivity));

        LifecycleVerifier.assertLaunchSequence(TranslucentActivity.class, FirstActivity.class,
                getLifecycleLog(), true /* launchIsTranslucent */);

        // Launch another translucent activity on top
        getLifecycleLog().clear();
        final Activity secondTranslucentActivity =
                launchActivityAndWait(SecondTranslucentActivity.class);
        waitAndAssertActivityStates(
                occludedActivityState(translucentActivity, secondTranslucentActivity));
        LifecycleVerifier.assertSequence(TranslucentActivity.class, getLifecycleLog(),
                Arrays.asList(ON_PAUSE), "launch");
        LifecycleVerifier.assertEmptySequence(FirstActivity.class, getLifecycleLog(), "launch");

        // Finish top translucent activity
        getLifecycleLog().clear();
        secondTranslucentActivity.finish();

        waitAndAssertActivityStates(state(translucentActivity, ON_RESUME));
        waitAndAssertActivityStates(state(secondTranslucentActivity, ON_DESTROY));
        LifecycleVerifier.assertResumeToDestroySequence(SecondTranslucentActivity.class,
                getLifecycleLog());
        LifecycleVerifier.assertSequence(TranslucentActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "launch");
        LifecycleVerifier.assertEmptySequence(FirstActivity.class, getLifecycleLog(), "launch");
    }

    @Test
    public void testTranslucentMovedIntoStack() throws Exception {
        // Launch a translucent activity and a regular activity in separate stacks
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        final Activity firstActivity = new Launcher(FirstActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();
        waitAndAssertActivityStates(state(translucentActivity, ON_STOP));

        final ComponentName firstActivityName = getComponentName(FirstActivity.class);
        mWmState.computeState(firstActivityName);
        int firstActivityStack = mWmState.getStackIdByActivity(firstActivityName);

        // Move translucent activity into the stack with the first activity
        getLifecycleLog().clear();
        moveActivityToStackOrOnTop(getComponentName(TranslucentActivity.class), firstActivityStack);

        // Wait for translucent activity to resume and first activity to pause
        waitAndAssertActivityStates(state(translucentActivity, ON_RESUME),
                state(firstActivity, ON_PAUSE));
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_PAUSE), "launchOnTop");
        LifecycleVerifier.assertRestartAndResumeSequence(TranslucentActivity.class,
                getLifecycleLog());
    }

    @Test
    public void testDestroyTopTranslucent() throws Exception {
        // Launch a regular activity and a a translucent activity in the same stack
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        waitAndAssertActivityStates(occludedActivityState(firstActivity, translucentActivity));

        // Finish translucent activity
        getLifecycleLog().clear();
        translucentActivity.finish();

        waitAndAssertActivityStates(state(firstActivity, ON_RESUME),
                state(translucentActivity, ON_DESTROY));

        // Verify destruction lifecycle
        LifecycleVerifier.assertResumeToDestroySequence(TranslucentActivity.class,
                getLifecycleLog());
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "resumeAfterTopDestroyed");
    }

    @Test
    public void testDestroyOnTopOfTranslucent() throws Exception {
        // Launch fullscreen activity
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);
        // Launch translucent activity
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        // Launch another fullscreen activity
        final Activity secondActivity = launchActivityAndWait(SecondActivity.class);

        // Wait for top activity to resume
        waitAndAssertActivityStates(state(translucentActivity, ON_STOP),
                state(firstActivity, ON_STOP));

        getLifecycleLog().clear();

        final boolean secondActivityIsTranslucent = ActivityInfo.isTranslucentOrFloating(
                secondActivity.getWindow().getWindowStyle());

        // Finish top activity
        secondActivity.finish();

        waitAndAssertActivityStates(state(secondActivity, ON_DESTROY));
        LifecycleVerifier.assertResumeToDestroySequence(SecondActivity.class, getLifecycleLog());
        if (secondActivityIsTranslucent) {
            // In this case we don't expect the state of the firstActivity to change since it is
            // already in the visible paused state. So, we just verify that translucentActivity
            // transitions to resumed state.
            waitAndAssertActivityStates(state(translucentActivity, ON_RESUME));
        } else {
            // Wait for translucent activity to resume
            waitAndAssertActivityStates(state(translucentActivity, ON_RESUME),
                    state(firstActivity, ON_START));

            // Verify that the first activity was restarted
            LifecycleVerifier.assertRestartSequence(FirstActivity.class, getLifecycleLog());
        }
    }

    @Test
    public void testDestroyDoubleTranslucentOnTop() throws Exception {
        final Activity firstActivity = launchActivityAndWait(FirstActivity.class);
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        final Activity secondTranslucentActivity =
                launchActivityAndWait(SecondTranslucentActivity.class);
        waitAndAssertActivityStates(occludedActivityState(firstActivity, secondTranslucentActivity),
                occludedActivityState(translucentActivity, secondTranslucentActivity));

        // Finish top translucent activity
        getLifecycleLog().clear();
        secondTranslucentActivity.finish();

        waitAndAssertActivityStates(state(translucentActivity, ON_RESUME));
        waitAndAssertActivityStates(state(secondTranslucentActivity, ON_DESTROY));
        LifecycleVerifier.assertResumeToDestroySequence(SecondTranslucentActivity.class,
                getLifecycleLog());
        LifecycleVerifier.assertSequence(TranslucentActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "destroy");
        LifecycleVerifier.assertEmptySequence(FirstActivity.class, getLifecycleLog(), "destroy");

        // Finish first translucent activity
        getLifecycleLog().clear();
        translucentActivity.finish();

        waitAndAssertActivityStates(state(firstActivity, ON_RESUME));
        waitAndAssertActivityStates(state(translucentActivity, ON_DESTROY));
        LifecycleVerifier.assertResumeToDestroySequence(TranslucentActivity.class,
                getLifecycleLog());
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_RESUME), "secondDestroy");
    }

    @FlakyTest(bugId=137329632)
    @Test
    public void testFinishBottom() throws Exception {
        final Activity bottomActivity = launchActivityAndWait(FirstActivity.class);
        final Activity topActivity = launchActivityAndWait(SecondActivity.class);
        waitAndAssertActivityStates(state(bottomActivity, ON_STOP));

        // Finish the activity on the bottom
        getLifecycleLog().clear();
        bottomActivity.finish();

        // Assert that activity on the bottom went directly to destroyed state, and activity on top
        // did not get any lifecycle changes.
        waitAndAssertActivityStates(state(bottomActivity, ON_DESTROY));
        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_DESTROY), "destroyOnBottom");
        LifecycleVerifier.assertEmptySequence(SecondActivity.class, getLifecycleLog(),
                "destroyOnBottom");
    }

    @FlakyTest(bugId=137329632)
    @Test
    public void testFinishAndLaunchOnResult() throws Exception {
        testLaunchForResultAndLaunchAfterResultSequence(EXTRA_LAUNCH_ON_RESULT);
    }

    @FlakyTest(bugId=137329632)
    @Test
    public void testFinishAndLaunchAfterOnResultInOnResume() throws Exception {
        testLaunchForResultAndLaunchAfterResultSequence(EXTRA_LAUNCH_ON_RESUME_AFTER_RESULT);
    }

    /**
     * This triggers launch of an activity for result, which immediately finishes. After receiving
     * result new activity launch is triggered automatically.
     * @see android.server.wm.lifecycle.ActivityLifecycleClientTestBase.LaunchForResultActivity
     */
    private void testLaunchForResultAndLaunchAfterResultSequence(String flag) throws Exception {
        new Launcher(LaunchForResultActivity.class)
                .customizeIntent(LaunchForResultActivity.forwardFlag(EXTRA_FINISH_IN_ON_RESUME))
                .setExtraFlags(flag)
                .setExpectedState(ON_STOP)
                .setNoInstance()
                .launch();

        waitAndAssertActivityStates(state(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED),
                state(ResultActivity.class, ON_DESTROY));
        LifecycleVerifier.assertOrder(getLifecycleLog(), Arrays.asList(
                // Base launching activity starting.
                transition(LaunchForResultActivity.class, PRE_ON_CREATE),
                transition(LaunchForResultActivity.class, ON_CREATE),
                transition(LaunchForResultActivity.class, ON_START),
                transition(LaunchForResultActivity.class, ON_POST_CREATE),
                transition(LaunchForResultActivity.class, ON_RESUME),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_GAINED),
                // An activity is automatically launched for result.
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_LOST),
                transition(LaunchForResultActivity.class, ON_PAUSE),
                transition(ResultActivity.class, PRE_ON_CREATE),
                transition(ResultActivity.class, ON_CREATE),
                transition(ResultActivity.class, ON_START),
                transition(ResultActivity.class, ON_RESUME),
                transition(ResultActivity.class, ON_TOP_POSITION_GAINED),
                // Activity that was launched for result is finished automatically - the base
                // launching activity is brought to front.
                transition(LaunchForResultActivity.class, ON_ACTIVITY_RESULT),
                transition(LaunchForResultActivity.class, ON_RESUME),
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_GAINED),
                // New activity is launched after receiving result in base activity.
                transition(LaunchForResultActivity.class, ON_TOP_POSITION_LOST),
                transition(LaunchForResultActivity.class, ON_PAUSE),
                transition(CallbackTrackingActivity.class, PRE_ON_CREATE),
                transition(CallbackTrackingActivity.class, ON_CREATE),
                transition(CallbackTrackingActivity.class, ON_START),
                transition(CallbackTrackingActivity.class, ON_RESUME),
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED)),
                "launchForResultAndLaunchAfterOnResult");
    }

    @Test
    public void testLaunchAndDestroy() throws Exception {
        final Activity activity = launchActivityAndWait(FirstActivity.class);

        activity.finish();
        waitAndAssertActivityStates(state(activity, ON_DESTROY));

        LifecycleVerifier.assertLaunchAndDestroySequence(FirstActivity.class, getLifecycleLog());
    }

    @Test
    public void testTrampoline() throws Exception {
        testTrampolineLifecycle(false /* newTask */);
    }

    @Test
    public void testTrampolineNewTask() throws Exception {
        testTrampolineLifecycle(true /* newTask */);
    }

    /**
     * Verifies that activity start from a trampoline will have the correct lifecycle and complete
     * in time. The expected lifecycle is that the trampoline will skip ON_START - ON_STOP part of
     * the usual sequence, and will go straight to ON_DESTROY after creation.
     */
    private void testTrampolineLifecycle(boolean newTask) throws Exception {
        // Run activity start manually (without using instrumentation) to make it async and measure
        // time from the request correctly.
        // TODO verify
        final Launcher launcher = new Launcher(NoDisplayActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setExtraFlags(EXTRA_LAUNCH_ACTIVITY, EXTRA_FINISH_IN_ON_CREATE)
                .setExpectedState(ON_DESTROY)
                .setNoInstance();
        if (newTask) {
            launcher.setExtraFlags(EXTRA_NEW_TASK);
        }
        launcher.launch();
        waitAndAssertActivityStates(state(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED));

        LifecycleVerifier.assertEntireSequence(Arrays.asList(
                transition(NoDisplayActivity.class, PRE_ON_CREATE),
                transition(NoDisplayActivity.class, ON_CREATE),
                transition(CallbackTrackingActivity.class, PRE_ON_CREATE),
                transition(CallbackTrackingActivity.class, ON_CREATE),
                transition(CallbackTrackingActivity.class, ON_START),
                transition(CallbackTrackingActivity.class, ON_POST_CREATE),
                transition(CallbackTrackingActivity.class, ON_RESUME),
                transition(CallbackTrackingActivity.class, ON_TOP_POSITION_GAINED),
                transition(NoDisplayActivity.class, ON_DESTROY)),
                getLifecycleLog(), "trampolineLaunch");
    }

    /** @see #testTrampolineWithAnotherProcess */
    @Test
    public void testTrampolineAnotherProcessNewTask() {
        testTrampolineWithAnotherProcess();
    }

    /**
     * Same as {@link #testTrampolineAnotherProcessNewTask()}, but with a living second process.
     */
    @Test
    public void testTrampolineAnotherExistingProcessNewTask() {
        // Start the second process before running the test. It is to make a specific path that the
        // the activity may be started when checking visibility instead of attaching its process.
        mContext.startActivity(new Intent(mContext, SecondProcessCallbackTrackingActivity.class)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        waitAndAssertActivityStates(
                state(SecondProcessCallbackTrackingActivity.class, ON_TOP_POSITION_GAINED));
        getLifecycleLog().clear();

        testTrampolineWithAnotherProcess();
    }

    /**
     * Simulates X starts Y in the same task, and Y starts Z in another task then finishes itself:
     * <pre>
     * Top Task B: SecondProcessCallbackTrackingActivity (Z)
     *     Task A: SecondProcessCallbackTrackingActivity (Y) (finishing)
     *             FirstActivity (X)
     * </pre>
     * Expect Y to become invisible and then destroyed when the transition is done.
     */
    private void testTrampolineWithAnotherProcess() {
        // Use another process so its lifecycle won't be affected by the caller activity.
        final Intent intent2 = new Intent(mContext, SecondProcessCallbackTrackingActivity.class)
                .addFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK);
        final Intent intent1 = new Intent(mContext, SecondProcessCallbackTrackingActivity.class)
                .putExtra(EXTRA_START_ACTIVITY_IN_ON_CREATE, intent2)
                .putExtra(EXTRA_FINISH_IN_ON_CREATE, true);
        mContext.startActivity(new Intent(mContext, FirstActivity.class)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(EXTRA_START_ACTIVITY_WHEN_IDLE, intent1));
        waitAndAssertActivityStates(state(SecondProcessCallbackTrackingActivity.class, ON_DESTROY));
    }

    @Test
    public void testRelaunchResumed() throws Exception {
        final Activity activity = launchActivityAndWait(FirstActivity.class);

        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(activity::recreate);
        waitAndAssertActivityStates(state(activity, ON_RESUME));

        LifecycleVerifier.assertRelaunchSequence(FirstActivity.class, getLifecycleLog(), ON_RESUME);
    }

    @Test
    public void testRelaunchPaused() throws Exception {
        final Activity pausedActivity = launchActivityAndWait(FirstActivity.class);
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);

        waitAndAssertActivityStates(occludedActivityState(pausedActivity, translucentActivity));

        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(pausedActivity::recreate);
        waitAndAssertActivityStates(state(pausedActivity, ON_PAUSE));

        LifecycleVerifier.assertRelaunchSequence(FirstActivity.class, getLifecycleLog(), ON_PAUSE);
    }

    @Test
    public void testRelaunchStopped() throws Exception {
        final Activity stoppedActivity = launchActivityAndWait(FirstActivity.class);
        launchActivityAndWait(SecondActivity.class);

        waitAndAssertActivityStates(state(stoppedActivity, ON_STOP));

        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(stoppedActivity::recreate);
        waitAndAssertActivityStates(state(stoppedActivity, ON_STOP));

        LifecycleVerifier.assertRelaunchSequence(FirstActivity.class, getLifecycleLog(),
                ON_STOP);
    }

    @Test
    public void testRelaunchConfigurationChangedWhileBecomingVisible() throws Exception {
        if (!supportsRotation()) {
            // Skip rotation test if device doesn't support it.
            return;
        }

        final Activity becomingVisibleActivity = launchActivityAndWait(FirstActivity.class);
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);
        final Activity topOpaqueActivity = launchActivityAndWait(SecondActivity.class);

        waitAndAssertActivityStates(
                state(becomingVisibleActivity, ON_STOP),
                state(translucentActivity, ON_STOP));

        final RotationSession rotationSession = createManagedRotationSession();
        if (!supportsLockedUserRotation(
                rotationSession, translucentActivity.getDisplay().getDisplayId())) {
            return;
        }

        getLifecycleLog().clear();

        final int current = rotationSession.get();
        // Set new rotation to cause a configuration change.
        switch (current) {
            case ROTATION_0:
            case ROTATION_180:
                rotationSession.set(ROTATION_90);
                break;
            case ROTATION_90:
            case ROTATION_270:
                rotationSession.set(ROTATION_0);
                break;
            default:
                fail("Unknown rotation:" + current);
        }

        // Assert that the top activity was relaunched.
        waitAndAssertActivityStates(state(topOpaqueActivity, ON_RESUME));
        LifecycleVerifier.assertRelaunchSequence(
                SecondActivity.class, getLifecycleLog(), ON_RESUME);

        // Finish the top activity
        getLifecycleLog().clear();
        topOpaqueActivity.finish();

        // Assert that the translucent activity and the activity visible behind it were
        // relaunched.
        waitAndAssertActivityStates(state(becomingVisibleActivity, ON_PAUSE),
                state(translucentActivity, ON_RESUME));

        LifecycleVerifier.assertSequence(FirstActivity.class, getLifecycleLog(),
                Arrays.asList(ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME,
                        ON_PAUSE), "becomingVisiblePaused");
        final List<LifecycleLog.ActivityCallback> expectedSequence =
                Arrays.asList(ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START, ON_RESUME);
        LifecycleVerifier.assertSequence(TranslucentActivity.class, getLifecycleLog(),
                expectedSequence, "becomingVisibleResumed");
    }

    @Test
    public void testOnActivityResult() throws Exception {
        new Launcher(LaunchForResultActivity.class)
                .customizeIntent(LaunchForResultActivity.forwardFlag(EXTRA_FINISH_IN_ON_RESUME))
                .launch();

        final List<LifecycleLog.ActivityCallback> expectedSequence =
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                        ON_PAUSE, ON_ACTIVITY_RESULT, ON_RESUME, ON_TOP_POSITION_GAINED);
        waitForActivityTransitions(LaunchForResultActivity.class, expectedSequence);

        // TODO(b/79218023): First activity might also be stopped before getting result.
        final List<LifecycleLog.ActivityCallback> sequenceWithStop =
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                        ON_PAUSE, ON_STOP, ON_ACTIVITY_RESULT, ON_RESTART, ON_START, ON_RESUME,
                        ON_TOP_POSITION_GAINED);
        final List<LifecycleLog.ActivityCallback> thirdSequence =
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                        ON_PAUSE, ON_STOP, ON_ACTIVITY_RESULT, ON_RESTART, ON_START, ON_RESUME,
                        ON_TOP_POSITION_GAINED);
        final List<LifecycleLog.ActivityCallback> fourthSequence =
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                        ON_PAUSE, ON_STOP, ON_RESTART, ON_START, ON_ACTIVITY_RESULT, ON_RESUME,
                        ON_TOP_POSITION_GAINED);
        LifecycleVerifier.assertSequenceMatchesOneOf(LaunchForResultActivity.class,
                getLifecycleLog(),
                Arrays.asList(expectedSequence, sequenceWithStop, thirdSequence, fourthSequence),
                "activityResult");
    }

    @Test
    @FlakyTest(bugId=127741025)
    public void testOnActivityResultAfterStop() throws Exception {
        final ActivityMonitor resultMonitor = getInstrumentation().addMonitor(
                ResultActivity.class.getName(), null /* result */, false /* block */);
        final ActivityMonitor launchMonitor = getInstrumentation().addMonitor(
                LaunchForResultActivity.class.getName(), null/* result */, false /* block */);
        new Launcher(LaunchForResultActivity.class)
                // TODO (b/127741025) temporarily use setNoInstance, because startActivitySync will
                // cause launch timeout when more than 2 activities start consecutively.
                .setNoInstance()
                .setExpectedState(ON_STOP)
                .launch();
        final Activity activity = getInstrumentation()
                .waitForMonitorWithTimeout(launchMonitor, 5000);
        waitAndAssertActivityStates(state(activity, ON_STOP));
        final Activity resultActivity = getInstrumentation()
                .waitForMonitorWithTimeout(resultMonitor, 5000);
        waitAndAssertActivityStates(state(resultActivity, ON_TOP_POSITION_GAINED));
        getInstrumentation().runOnMainSync(resultActivity::finish);

        final boolean isTranslucent = isTranslucent(activity);

        final List<List<LifecycleLog.ActivityCallback>> expectedSequences;
        if (isTranslucent) {
            expectedSequences = Arrays.asList(
                    Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                            ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST, ON_PAUSE,
                            ON_ACTIVITY_RESULT, ON_RESUME, ON_TOP_POSITION_GAINED)
            );
        } else {
            expectedSequences = Arrays.asList(
                    Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                            ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                            ON_PAUSE, ON_STOP, ON_RESTART, ON_START, ON_ACTIVITY_RESULT, ON_RESUME,
                            ON_TOP_POSITION_GAINED),
                    Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                            ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                            ON_PAUSE, ON_STOP, ON_ACTIVITY_RESULT, ON_RESTART, ON_START, ON_RESUME,
                            ON_TOP_POSITION_GAINED)
            );
        }
        waitForActivityTransitions(LaunchForResultActivity.class, expectedSequences.get(0));

        LifecycleVerifier.assertSequenceMatchesOneOf(LaunchForResultActivity.class,
                getLifecycleLog(), expectedSequences, "activityResult");
    }

    @Test
    public void testOnPostCreateAfterRecreateInOnResume() throws Exception {
        final Activity trackingActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Call "recreate" and assert sequence
        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(trackingActivity::recreate);
        waitAndAssertActivityStates(state(trackingActivity, ON_TOP_POSITION_GAINED));

        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class,
                getLifecycleLog(),
                Arrays.asList(ON_TOP_POSITION_LOST, ON_PAUSE, ON_STOP, ON_DESTROY, PRE_ON_CREATE,
                        ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME, ON_TOP_POSITION_GAINED),
                "recreate");
    }

    @Test
    public void testOnPostCreateAfterRecreateInOnPause() throws Exception {
        final Activity trackingActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        // Launch translucent activity, which will make the first one paused.
        final Activity translucentActivity = launchActivityAndWait(TranslucentActivity.class);

        // Wait for first activity to become paused
        waitAndAssertActivityStates(occludedActivityState(trackingActivity, translucentActivity));

        // Call "recreate" and assert sequence
        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(trackingActivity::recreate);
        waitAndAssertActivityStates(occludedActivityState(trackingActivity, translucentActivity));

        LifecycleVerifier.assertSequence(CallbackTrackingActivity.class,
                getLifecycleLog(),
                Arrays.asList(ON_STOP, ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START,
                        ON_POST_CREATE, ON_RESUME, ON_PAUSE),
                "recreate");
    }

    @Test
    public void testOnPostCreateAfterRecreateInOnStop() throws Exception {
        // Launch first activity
        final Activity trackingActivity = launchActivityAndWait(CallbackTrackingActivity.class);
        // Launch second activity to cover and stop first
        final Activity secondActivity = launchActivityAndWait(SecondActivity.class);
        // Wait for first activity to become stopped
        waitAndAssertActivityStates(state(trackingActivity, ON_STOP));

        // Call "recreate" and assert sequence
        getLifecycleLog().clear();
        getInstrumentation().runOnMainSync(trackingActivity::recreate);
        waitAndAssertActivityStates(state(trackingActivity, ON_STOP));

        final List<LifecycleLog.ActivityCallback> callbacks;
        if (isTranslucent(secondActivity)) {
            callbacks = Arrays.asList(ON_STOP, ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START,
                    ON_POST_CREATE, ON_RESUME, ON_PAUSE);
        } else {
            callbacks = Arrays.asList(ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START,
                    ON_POST_CREATE, ON_RESUME, ON_PAUSE, ON_STOP);
        }

        LifecycleVerifier.assertSequence(
                CallbackTrackingActivity.class, getLifecycleLog(), callbacks, "recreate");
    }

    /**
     * The following test ensures an activity is brought back if its process is ended in the
     * background.
     */
    @Test
    public void testRestoreFromKill() throws Exception {
        final LaunchActivityBuilder builder = getLaunchActivityBuilder();
        final ComponentName targetActivity = builder.getTargetActivity();

        // Launch activity whose process will be killed
        builder.execute();

        // Start activity in another process to put original activity in background.
        final Activity testActivity = launchActivityAndWait(FirstActivity.class);
        final boolean isTranslucent = isTranslucent(testActivity);
        mWmState.waitForActivityState(
                targetActivity, isTranslucent ? STATE_PAUSED : STATE_STOPPED);

        // Only try to kill targetActivity if the top activity isn't translucent. If the top
        // activity is translucent then targetActivity will be visible, so the process will be
        // started again really quickly.
        if (!isTranslucent) {
            // Kill first activity
            AmUtils.runKill(targetActivity.getPackageName(), true /* wait */);
        }

        // Return back to first activity
        pressBackButton();

        // Verify activity is resumed
        mWmState.waitForValidState(targetActivity);
        mWmState.assertResumedActivity("Originally launched activity should be resumed",
                targetActivity);
    }

    /**
     * Tests that recreate request from an activity is executed immediately.
     */
    @Test
    public void testLocalRecreate() throws Exception {
        // Launch the activity that will recreate itself
        final Activity recreatingActivity = launchActivityAndWait(SingleTopActivity.class);

        // Launch second activity to cover and stop first
        final Activity secondActivity = new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        // Wait for first activity to become occluded
        waitAndAssertActivityStates(state(recreatingActivity, ON_STOP));

        // Launch the activity again to recreate
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setExtraFlags(EXTRA_RECREATE)
                .launch();

        // Wait for activity to relaunch and resume
        final List<LifecycleLog.ActivityCallback> expectedRelaunchSequence;
        if (isTranslucent(secondActivity)) {
            expectedRelaunchSequence = Arrays.asList(ON_NEW_INTENT, ON_RESUME,
                    ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST,
                    ON_PAUSE, ON_STOP, ON_DESTROY, PRE_ON_CREATE, ON_CREATE, ON_START,
                    ON_POST_CREATE, ON_RESUME, ON_TOP_POSITION_GAINED);
        } else {
            expectedRelaunchSequence = Arrays.asList(ON_RESTART, ON_START, ON_NEW_INTENT, ON_RESUME,
                    ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST, ON_PAUSE, ON_STOP, ON_DESTROY,
                    PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                    ON_TOP_POSITION_GAINED);
        }

        waitForActivityTransitions(SingleTopActivity.class, expectedRelaunchSequence);
        LifecycleVerifier.assertSequence(SingleTopActivity.class, getLifecycleLog(),
                expectedRelaunchSequence, "recreate");
    }

    @Test
    public void testOnNewIntent() throws Exception {
        // Launch a singleTop activity
        launchActivityAndWait(SingleTopActivity.class);

        LifecycleVerifier.assertLaunchSequence(SingleTopActivity.class, getLifecycleLog());

        // Try to launch again
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setNoInstance()
                .launch();

        // Verify that the first activity was paused, new intent was delivered and resumed again
        LifecycleVerifier.assertSequence(SingleTopActivity.class, getLifecycleLog(),
                Arrays.asList(ON_TOP_POSITION_LOST, ON_PAUSE, ON_NEW_INTENT, ON_RESUME,
                        ON_TOP_POSITION_GAINED), "newIntent");
    }

    @Test
    public void testOnNewIntentFromHidden() throws Exception {
        // Launch a singleTop activity
        final Activity singleTopActivity = launchActivityAndWait(SingleTopActivity.class);
        LifecycleVerifier.assertLaunchSequence(SingleTopActivity.class, getLifecycleLog());

        // Launch something on top
        final Activity secondActivity = new Launcher(SecondActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK)
                .launch();

        waitAndAssertActivityStates(state(singleTopActivity, ON_STOP));

        // Try to launch again
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK)
                .setNoInstance()
                .launch();

        // Verify that the first activity was restarted, new intent was delivered and resumed again
        final List<LifecycleLog.ActivityCallback> expectedSequence;
        if (isTranslucent(singleTopActivity)) {
            expectedSequence = Arrays.asList(ON_NEW_INTENT, ON_RESUME, ON_TOP_POSITION_GAINED);
        } else {
            expectedSequence = Arrays.asList(ON_RESTART, ON_START, ON_NEW_INTENT, ON_RESUME,
                    ON_TOP_POSITION_GAINED);
        }
        LifecycleVerifier.assertSequence(SingleTopActivity.class, getLifecycleLog(),
                expectedSequence, "newIntent");
    }

    @Test
    public void testOnNewIntentFromPaused() throws Exception {
        // Launch a singleTop activity
        final Activity singleTopActivity = launchActivityAndWait(SingleTopActivity.class);
        LifecycleVerifier.assertLaunchSequence(SingleTopActivity.class, getLifecycleLog());

        // Launch translucent activity, which will make the first one paused.
        launchActivityAndWait(TranslucentActivity.class);

        // Wait for the activity below to pause
        waitAndAssertActivityStates(state(singleTopActivity, ON_PAUSE));

        // Try to launch again
        getLifecycleLog().clear();
        new Launcher(SingleTopActivity.class)
                .setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TOP)
                .setNoInstance()
                .launch();

        // Wait for the activity to resume again
        // Verify that the new intent was delivered and resumed again
        final List<LifecycleLog.ActivityCallback> expectedSequence =
                Arrays.asList(ON_NEW_INTENT, ON_RESUME, ON_TOP_POSITION_GAINED);
        waitForActivityTransitions(SingleTopActivity.class, expectedSequence);
        LifecycleVerifier.assertSequence(SingleTopActivity.class, getLifecycleLog(),
                expectedSequence, "newIntent");
    }

    @Test
    public void testFinishInOnCreate() throws Exception {
        verifyFinishAtStage( ResultActivity.class, EXTRA_FINISH_IN_ON_CREATE,
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_DESTROY), "onCreate");
    }

    @Test
    public void testFinishInOnCreateNoDisplay() throws Exception {
        verifyFinishAtStage(NoDisplayActivity.class, EXTRA_FINISH_IN_ON_CREATE,
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_DESTROY), "onCreate");
    }

    @Test
    public void testFinishInOnStart() throws Exception {
        verifyFinishAtStage(ResultActivity.class, EXTRA_FINISH_IN_ON_START,
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_STOP,
                        ON_DESTROY), "onStart");
    }

    @Test
    public void testFinishInOnStartNoDisplay() throws Exception {
        verifyFinishAtStage(NoDisplayActivity.class, EXTRA_FINISH_IN_ON_START,
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_STOP,
                        ON_DESTROY), "onStart");
    }

    @Test
    public void testFinishInOnResume() throws Exception {
        verifyFinishAtStage(ResultActivity.class, EXTRA_FINISH_IN_ON_RESUME,
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST, ON_PAUSE, ON_STOP,
                        ON_DESTROY), "onResume");
    }

    @Test
    public void testFinishInOnResumeNoDisplay() throws Exception {
        verifyFinishAtStage(NoDisplayActivity.class, EXTRA_FINISH_IN_ON_RESUME,
                Arrays.asList(PRE_ON_CREATE, ON_CREATE, ON_START, ON_POST_CREATE, ON_RESUME,
                        ON_TOP_POSITION_GAINED, ON_TOP_POSITION_LOST, ON_PAUSE, ON_STOP,
                        ON_DESTROY), "onResume");
    }

    private void verifyFinishAtStage(Class<? extends Activity> activityClass,
            String finishStageExtra, List<LifecycleLog.ActivityCallback> expectedSequence,
            String stageName) throws Exception {
        new Launcher(activityClass)
                .setExpectedState(ON_DESTROY)
                .setExtraFlags(finishStageExtra)
                .setNoInstance()
                .launch();

        waitAndAssertActivityTransitions(activityClass, expectedSequence, "finish in " + stageName);
    }

    @Test
    public void testFinishInOnPause() throws Exception {
        verifyFinishAtStage(ResultActivity.class, EXTRA_FINISH_IN_ON_PAUSE, "onPause",
                TranslucentActivity.class);
    }

    @Test
    public void testFinishInOnStop() throws Exception {
        verifyFinishAtStage(ResultActivity.class, EXTRA_FINISH_IN_ON_STOP, "onStop",
                FirstActivity.class);
    }

    @FlakyTest(bugId=142125019) // Add to presubmit when proven stable
    @Test
    public void testFinishBelowDialogActivity() throws Exception {
        verifyFinishAtStage(ResultActivity.class, EXTRA_FINISH_IN_ON_PAUSE, "onPause",
                TranslucentCallbackTrackingActivity.class);
    }

    private void verifyFinishAtStage(Class<? extends Activity> activityClass,
            String finishStageExtra, String stageName, Class<? extends Activity> launchOnTopClass)
            throws Exception {

        // Activity will finish itself after onResume, so need to launch an extra activity on
        // top to get it there.
        new Launcher(activityClass)
                .setExtraFlags(finishStageExtra)
                .launch();

        // Launch an activity on top, which will make the first one paused or stopped.
        launchActivityAndWait(launchOnTopClass);

        final List<LifecycleLog.ActivityCallback> expectedSequence =
                LifecycleVerifier.getLaunchAndDestroySequence(activityClass);
        waitAndAssertActivityTransitions(activityClass, expectedSequence, "finish in " + stageName);
    }

    @FlakyTest(bugId=142125019) // Add to presubmit when proven stable
    @Test
    public void testFinishBelowTranslucentActivityAfterDelay() throws Exception {
        final Activity bottomActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        launchActivityAndWait(TranslucentCallbackTrackingActivity.class);
        waitAndAssertActivityStates(state(bottomActivity, ON_PAUSE));
        getLifecycleLog().clear();

        waitForIdle();
        bottomActivity.finish();
        waitAndAssertActivityStates(state(bottomActivity, ON_DESTROY));
        LifecycleVerifier.assertEmptySequence(TranslucentCallbackTrackingActivity.class,
                getLifecycleLog(), "finishBelow");
    }

    @FlakyTest(bugId=142125019) // Add to presubmit when proven stable
    @Test
    public void testFinishBelowFullscreenActivityAfterDelay() throws Exception {
        final Activity bottomActivity = launchActivityAndWait(CallbackTrackingActivity.class);

        launchActivityAndWait(FirstActivity.class);
        waitAndAssertActivityStates(state(bottomActivity, ON_STOP));
        getLifecycleLog().clear();

        waitForIdle();
        bottomActivity.finish();
        waitAndAssertActivityStates(state(bottomActivity, ON_DESTROY));
        LifecycleVerifier.assertEmptySequence(FirstActivity.class, getLifecycleLog(),
                "finishBelow");
    }
}
