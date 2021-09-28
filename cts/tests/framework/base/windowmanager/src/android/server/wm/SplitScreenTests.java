/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static android.app.ActivityManager.LOCK_TASK_MODE_NONE;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_HOME;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_RECENTS;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN_OR_SPLIT_SCREEN_SECONDARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_SECONDARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_UNDEFINED;
import static android.server.wm.WindowManagerState.STATE_STOPPED;
import static android.server.wm.app.Components.DOCKED_ACTIVITY;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.NON_RESIZEABLE_ACTIVITY;
import static android.server.wm.app.Components.NO_RELAUNCH_ACTIVITY;
import static android.server.wm.app.Components.SINGLE_INSTANCE_ACTIVITY;
import static android.server.wm.app.Components.SINGLE_TASK_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY_WITH_SAME_AFFINITY;
import static android.server.wm.app.Components.TRANSLUCENT_TEST_ACTIVITY;
import static android.server.wm.app.Components.TestActivity.TEST_ACTIVITY_ACTION_FINISH_SELF;
import static android.server.wm.app27.Components.SDK_27_LAUNCHING_ACTIVITY;
import static android.server.wm.app27.Components.SDK_27_SEPARATE_PROCESS_ACTIVITY;
import static android.server.wm.app27.Components.SDK_27_TEST_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Surface.ROTATION_0;
import static android.view.Surface.ROTATION_180;
import static android.view.Surface.ROTATION_270;
import static android.view.Surface.ROTATION_90;

import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.graphics.Rect;
import android.platform.test.annotations.Presubmit;
import android.server.wm.CommandSession.ActivityCallback;

import androidx.test.filters.FlakyTest;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:SplitScreenTests
 */
@Presubmit
@android.server.wm.annotation.Group2
public class SplitScreenTests extends ActivityManagerTestBase {

    private static final int TASK_SIZE = 600;
    private static final int STACK_SIZE = 300;

    private boolean mIsHomeRecentsComponent;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        mIsHomeRecentsComponent = mWmState.isHomeRecentsComponent();

        assumeTrue("Skipping test: no split multi-window support",
                supportsSplitScreenMultiWindow());
    }

    @Test
    public void testMinimumDeviceSize() throws Exception {
        mWmState.assertDeviceDefaultDisplaySize(
                "Devices supporting multi-window must be larger than the default minimum"
                        + " task size");
    }


// TODO: Add test to make sure you can't register to split-windowing mode organization if test
//  doesn't support it.


    @Test
    public void testStackList() throws Exception {
        launchActivity(TEST_ACTIVITY);
        mWmState.computeState(TEST_ACTIVITY);
        mWmState.assertContainsStack("Must contain home stack.",
                WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
        mWmState.assertContainsStack("Must contain fullscreen stack.",
                WINDOWING_MODE_FULLSCREEN, ACTIVITY_TYPE_STANDARD);
        mWmState.assertDoesNotContainStack("Must not contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testDockActivity() throws Exception {
        launchActivityInSplitScreenWithRecents(TEST_ACTIVITY);
        mWmState.assertContainsStack("Must contain home stack.",
                WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
        mWmState.assertContainsStack("Must contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testNonResizeableNotDocked() throws Exception {
        launchActivityInSplitScreenWithRecents(NON_RESIZEABLE_ACTIVITY);

        mWmState.assertContainsStack("Must contain home stack.",
                WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
        mWmState.assertDoesNotContainStack("Must not contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertFrontStack("Fullscreen stack must be front stack.",
                WINDOWING_MODE_FULLSCREEN, ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testNonResizeableWhenAlreadyInSplitScreenPrimary() throws Exception {
        launchActivityInSplitScreenWithRecents(SDK_27_LAUNCHING_ACTIVITY);
        launchActivity(NON_RESIZEABLE_ACTIVITY, WINDOWING_MODE_UNDEFINED);

        mWmState.assertDoesNotContainStack("Must not contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertFrontStack("Fullscreen stack must be front stack.",
                WINDOWING_MODE_FULLSCREEN, ACTIVITY_TYPE_STANDARD);

        waitAndAssertTopResumedActivity(NON_RESIZEABLE_ACTIVITY, DEFAULT_DISPLAY,
                "NON_RESIZEABLE_ACTIVITY launched on default display must be focused");
    }

    @Test
    public void testNonResizeableWhenAlreadyInSplitScreenSecondary() throws Exception {
        launchActivityInSplitScreenWithRecents(SDK_27_LAUNCHING_ACTIVITY);
        // Launch home so secondary side as focus.
        launchHomeActivity();
        launchActivity(NON_RESIZEABLE_ACTIVITY, WINDOWING_MODE_UNDEFINED);

        mWmState.assertDoesNotContainStack("Must not contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertFrontStack("Fullscreen stack must be front stack.",
                WINDOWING_MODE_FULLSCREEN, ACTIVITY_TYPE_STANDARD);

        waitAndAssertTopResumedActivity(NON_RESIZEABLE_ACTIVITY, DEFAULT_DISPLAY,
                "NON_RESIZEABLE_ACTIVITY launched on default display must be focused");
    }

    @Test
    public void testLaunchToSide() throws Exception {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        mWmState.assertContainsStack("Must contain fullscreen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertContainsStack("Must contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testLaunchToSideMultiWindowCallbacks() throws Exception {
        // Launch two activities in split-screen mode.
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        mWmState.assertContainsStack("Must contain fullscreen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertContainsStack("Must contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);

        // Exit split-screen mode and ensure we only get 1 multi-window mode changed callback.
        separateTestJournal();
        SystemUtil.runWithShellPermissionIdentity(() -> mTaskOrganizer.dismissedSplitScreen());
        final ActivityLifecycleCounts lifecycleCounts = waitForOnMultiWindowModeChanged(
                TEST_ACTIVITY);
        assertEquals(1, lifecycleCounts.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED));
    }

    @Test
    public void testNoUserLeaveHintOnMultiWindowModeChanged() throws Exception {
        launchActivity(TEST_ACTIVITY, WINDOWING_MODE_FULLSCREEN);

        // Move to docked stack.
        separateTestJournal();
        setActivityTaskWindowingMode(TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        ActivityLifecycleCounts lifecycleCounts = waitForOnMultiWindowModeChanged(TEST_ACTIVITY);
        assertEquals("mMultiWindowModeChangedCount",
                1, lifecycleCounts.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED));
        assertEquals("mUserLeaveHintCount",
                0, lifecycleCounts.getCount(ActivityCallback.ON_USER_LEAVE_HINT));

        // Make sure docked stack is focused. This way when we dismiss it later fullscreen stack
        // will come up.
        launchActivity(LAUNCHING_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        launchActivity(TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);

        // Move activity back to fullscreen stack.
        separateTestJournal();
        setActivityTaskWindowingMode(TEST_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        lifecycleCounts = waitForOnMultiWindowModeChanged(TEST_ACTIVITY);
        assertEquals("mMultiWindowModeChangedCount",
                1, lifecycleCounts.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED));
        assertEquals("mUserLeaveHintCount",
                0, lifecycleCounts.getCount(ActivityCallback.ON_USER_LEAVE_HINT));
    }

    @Test
    public void testLaunchToSideAndBringToFront() throws Exception {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));

        int taskNumberInitial = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        mWmState.assertFocusedActivity("Launched to side activity must be in front.",
                TEST_ACTIVITY);

        // Launch another activity to side to cover first one.
        launchActivity(NO_RELAUNCH_ACTIVITY, WINDOWING_MODE_FULLSCREEN_OR_SPLIT_SCREEN_SECONDARY);
        int taskNumberCovered = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertEquals("Fullscreen stack must have one task added.",
                taskNumberInitial + 1, taskNumberCovered);
        mWmState.assertFocusedActivity("Launched to side covering activity must be in front.",
                NO_RELAUNCH_ACTIVITY);

        // Launch activity that was first launched to side. It should be brought to front.
        getLaunchActivityBuilder()
                .setTargetActivity(TEST_ACTIVITY)
                .setToSide(true)
                .setWaitForLaunched(true)
                .execute();
        int taskNumberFinal = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertEquals("Task number in fullscreen stack must remain the same.",
                taskNumberCovered, taskNumberFinal);
        mWmState.assertFocusedActivity("Launched to side covering activity must be in front.",
                TEST_ACTIVITY);
    }

    @Test
    public void testLaunchToSideMultiple() throws Exception {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));

        int taskNumberInitial = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertNotNull("Launched to side activity must be in fullscreen stack.",
                mWmState.getTaskByActivity(
                        TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));

        // Try to launch to side same activity again.
        getLaunchActivityBuilder().setToSide(true).execute();
        mWmState.computeState(TEST_ACTIVITY, LAUNCHING_ACTIVITY);
        int taskNumberFinal = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertEquals("Task number mustn't change.", taskNumberInitial, taskNumberFinal);
        mWmState.assertFocusedActivity("Launched to side activity must remain in front.",
                TEST_ACTIVITY);
        assertNotNull("Launched to side activity must remain in fullscreen stack.",
                mWmState.getTaskByActivity(
                        TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));
    }

    @Test
    public void testLaunchToSideSingleInstance() throws Exception {
        launchTargetToSide(SINGLE_INSTANCE_ACTIVITY, false);
    }

    @Test
    public void testLaunchToSideSingleTask() throws Exception {
        launchTargetToSide(SINGLE_TASK_ACTIVITY, false);
    }

    @Test
    public void testLaunchToSideMultipleWithDifferentIntent() throws Exception {
        launchTargetToSide(TEST_ACTIVITY, true);
    }

    private void launchTargetToSide(ComponentName targetActivityName,
            boolean taskCountMustIncrement) throws Exception {
        // Launch in fullscreen first
        getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY)
                .setUseInstrumentation()
                .setWaitForLaunched(true)
                .execute();

        // Move to split-screen primary
        final int taskId = mWmState.getTaskByActivity(LAUNCHING_ACTIVITY).mTaskId;
        moveTaskToPrimarySplitScreen(taskId, true /* showSideActivity */);

        // Launch target to side
        final LaunchActivityBuilder targetActivityLauncher = getLaunchActivityBuilder()
                .setTargetActivity(targetActivityName)
                .setToSide(true)
                .setRandomData(true)
                .setMultipleTask(false);
        targetActivityLauncher.execute();

        mWmState.computeState(targetActivityName, LAUNCHING_ACTIVITY);
        mWmState.assertContainsStack("Must contain secondary stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        int taskNumberInitial = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertNotNull("Launched to side activity must be in fullscreen stack.",
                mWmState.getTaskByActivity(
                        targetActivityName, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));

        // Try to launch to side same activity again with different data.
        targetActivityLauncher.execute();
        mWmState.computeState(targetActivityName, LAUNCHING_ACTIVITY);
        int taskNumberSecondLaunch = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        if (taskCountMustIncrement) {
            assertEquals("Task number must be incremented.", taskNumberInitial + 1,
                    taskNumberSecondLaunch);
        } else {
            assertEquals("Task number must not change.", taskNumberInitial,
                    taskNumberSecondLaunch);
        }
        mWmState.assertFocusedActivity("Launched to side activity must be in front.",
                targetActivityName);
        assertNotNull("Launched to side activity must be launched in fullscreen stack.",
                mWmState.getTaskByActivity(
                        targetActivityName, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));

        // Try to launch to side same activity again with different random data. Note that null
        // cannot be used here, since the first instance of TestActivity is launched with no data
        // in order to launch into split screen.
        targetActivityLauncher.execute();
        mWmState.computeState(targetActivityName, LAUNCHING_ACTIVITY);
        int taskNumberFinal = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        if (taskCountMustIncrement) {
            assertEquals("Task number must be incremented.", taskNumberSecondLaunch + 1,
                    taskNumberFinal);
        } else {
            assertEquals("Task number must not change.", taskNumberSecondLaunch,
                    taskNumberFinal);
        }
        mWmState.assertFocusedActivity("Launched to side activity must be in front.",
                targetActivityName);
        assertNotNull("Launched to side activity must be launched in fullscreen stack.",
                mWmState.getTaskByActivity(
                        targetActivityName, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));
    }

    @Test
    public void testLaunchToSideMultipleWithFlag() throws Exception {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        int taskNumberInitial = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertNotNull("Launched to side activity must be in fullscreen stack.",
                mWmState.getTaskByActivity(
                        TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));

        // Try to launch to side same activity again, but with Intent#FLAG_ACTIVITY_MULTIPLE_TASK.
        getLaunchActivityBuilder().setToSide(true).setMultipleTask(true).execute();
        mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        int taskNumberFinal = mWmState.getStandardTaskCountByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        assertEquals("Task number must be incremented.", taskNumberInitial + 1,
                taskNumberFinal);
        mWmState.assertFocusedActivity("Launched to side activity must be in front.",
                TEST_ACTIVITY);
        assertNotNull("Launched to side activity must remain in fullscreen stack.",
                mWmState.getTaskByActivity(
                        TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_SECONDARY));
    }

    @Test
    public void testRotationWhenDocked() {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        mWmState.assertContainsStack("Must contain fullscreen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertContainsStack("Must contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);

        // Rotate device single steps (90°) 0-1-2-3.
        // Each time we compute the state we implicitly assert valid bounds.
        final RotationSession rotationSession = createManagedRotationSession();
        for (int i = 0; i < 4; i++) {
            rotationSession.set(i);
            mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        }
        // Double steps (180°) We ended the single step at 3. So, we jump directly to 1 for
        // double step. So, we are testing 3-1-3 for one side and 0-2-0 for the other side.
        rotationSession.set(ROTATION_90);
        mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        rotationSession.set(ROTATION_270);
        mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        rotationSession.set(ROTATION_0);
        mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        rotationSession.set(ROTATION_180);
        mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        rotationSession.set(ROTATION_0);
        mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
    }

    @Test
    public void testRotationWhenDockedWhileLocked() {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        mWmState.assertSanity();
        mWmState.assertContainsStack("Must contain fullscreen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertContainsStack("Must contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);

        final RotationSession rotationSession = createManagedRotationSession();
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        for (int i = 0; i < 4; i++) {
            lockScreenSession.sleepDevice();
            // The display may not be rotated while device is locked.
            rotationSession.set(i, false /* waitDeviceRotation */);
            lockScreenSession.wakeUpDevice()
                    .unlockDevice();
            mWmState.computeState(LAUNCHING_ACTIVITY, TEST_ACTIVITY);
        }
    }

    /**
     * Verify split screen mode visibility after stack resize occurs.
     */
    @Test
    public void testResizeDockedStack() throws Exception {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(DOCKED_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        final Rect restoreDockBounds = mWmState.getStandardRootTaskByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY) .getBounds();
        resizeDockedStack(STACK_SIZE, STACK_SIZE, TASK_SIZE, TASK_SIZE);
        mWmState.computeState(
                new WaitForValidActivityState(TEST_ACTIVITY),
                new WaitForValidActivityState(DOCKED_ACTIVITY));
        mWmState.assertContainsStack("Must contain secondary split-screen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertContainsStack("Must contain primary split-screen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertVisibility(DOCKED_ACTIVITY, true);
        mWmState.assertVisibility(TEST_ACTIVITY, true);
        int restoreW = restoreDockBounds.width();
        int restoreH = restoreDockBounds.height();
        resizeDockedStack(restoreW, restoreH, restoreW, restoreH);
    }

    @Test
    public void testSameProcessActivityResumedPreQ() {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(SDK_27_TEST_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(SDK_27_LAUNCHING_ACTIVITY));

        assertEquals("There must be only one resumed activity in the package.", 1,
                mWmState.getResumedActivitiesCountInPackage(
                        SDK_27_TEST_ACTIVITY.getPackageName()));
    }

    @Test
    public void testDifferentProcessActivityResumedPreQ() {
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(SDK_27_TEST_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(SDK_27_SEPARATE_PROCESS_ACTIVITY));

        assertEquals("There must be only two resumed activities in the package.", 2,
                mWmState.getResumedActivitiesCountInPackage(
                        SDK_27_TEST_ACTIVITY.getPackageName()));
    }

    @Test
    @FlakyTest(bugId = 131005232)
    public void testActivityLifeCycleOnResizeDockedStack() throws Exception {
        launchActivity(TEST_ACTIVITY);
        mWmState.computeState(TEST_ACTIVITY);
        final Rect fullScreenBounds = mWmState.getStandardRootTaskByWindowingMode(
                WINDOWING_MODE_FULLSCREEN).getBounds();

        setActivityTaskWindowingMode(TEST_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        mWmState.computeState(TEST_ACTIVITY);
        launchActivity(NO_RELAUNCH_ACTIVITY, WINDOWING_MODE_FULLSCREEN_OR_SPLIT_SCREEN_SECONDARY);

        mWmState.computeState(TEST_ACTIVITY, NO_RELAUNCH_ACTIVITY);
        final Rect initialDockBounds = mWmState.getStandardRootTaskByWindowingMode(
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY) .getBounds();

        separateTestJournal();

        Rect newBounds = computeNewDockBounds(fullScreenBounds, initialDockBounds, true);
        resizeDockedStack(
                newBounds.width(), newBounds.height(), newBounds.width(), newBounds.height());
        mWmState.computeState(TEST_ACTIVITY, NO_RELAUNCH_ACTIVITY);

        // We resize twice to make sure we cross an orientation change threshold for both
        // activities.
        newBounds = computeNewDockBounds(fullScreenBounds, initialDockBounds, false);
        resizeDockedStack(
                newBounds.width(), newBounds.height(), newBounds.width(), newBounds.height());
        mWmState.computeState(TEST_ACTIVITY, NO_RELAUNCH_ACTIVITY);
        assertActivityLifecycle(TEST_ACTIVITY, true /* relaunched */);
        assertActivityLifecycle(NO_RELAUNCH_ACTIVITY, false /* relaunched */);
    }

    @Test
    public void testDisallowEnterSplitscreenWhenInLockedTask() throws Exception {
        launchActivity(TEST_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        WindowManagerState.ActivityTask task =
                mWmState.getStandardRootTaskByWindowingMode(
                        WINDOWING_MODE_FULLSCREEN).getTopTask();

        // Lock the task and ensure that we can't enter split screen
        try {
            SystemUtil.runWithShellPermissionIdentity(() -> {
                mAtm.startSystemLockTaskMode(task.mTaskId);
            });
            waitForOrFail("Task in lock mode", () -> {
                return mAm.getLockTaskModeState() != LOCK_TASK_MODE_NONE;
            });

            assertFalse(setActivityTaskWindowingMode(TEST_ACTIVITY,
                    WINDOWING_MODE_SPLIT_SCREEN_PRIMARY));
        } finally {
            SystemUtil.runWithShellPermissionIdentity(() -> {
                mAtm.stopSystemLockTaskMode();
            });
        }
    }

    private Rect computeNewDockBounds(
            Rect fullscreenBounds, Rect dockBounds, boolean reduceSize) {
        final boolean inLandscape = fullscreenBounds.width() > dockBounds.width();
        // We are either increasing size or reducing it.
        final float sizeChangeFactor = reduceSize ? 0.5f : 1.5f;
        final Rect newBounds = new Rect(dockBounds);
        if (inLandscape) {
            // In landscape we change the width.
            newBounds.right = (int) (newBounds.left + (newBounds.width() * sizeChangeFactor));
        } else {
            // In portrait we change the height
            newBounds.bottom = (int) (newBounds.top + (newBounds.height() * sizeChangeFactor));
        }

        return newBounds;
    }

    @Test
    public void testStackListOrderLaunchDockedActivity() throws Exception {
        assumeTrue(!mIsHomeRecentsComponent);

        launchActivityInSplitScreenWithRecents(TEST_ACTIVITY);

        final int homeStackIndex = mWmState.getStackIndexByActivityType(ACTIVITY_TYPE_HOME);
        final int recentsStackIndex = mWmState.getStackIndexByActivityType(ACTIVITY_TYPE_RECENTS);
        assertThat("Recents stack should be on top of home stack",
                recentsStackIndex, lessThan(homeStackIndex));
    }


    /**
     * Asserts that the activity is visible when the top opaque activity finishes and with another
     * translucent activity on top while in split-screen-secondary task.
     */
    @Test
    public void testVisibilityWithTranslucentAndTopFinishingActivity() throws Exception {
        // Launch two activities in split-screen mode.
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY));

        // Launch two more activities on a different task on top of split-screen-secondary and
        // only the top opaque activity should be visible.
        getLaunchActivityBuilder().setTargetActivity(TRANSLUCENT_TEST_ACTIVITY)
                .setUseInstrumentation()
                .setWaitForLaunched(true)
                .execute();
        getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY)
                .setUseInstrumentation()
                .setWaitForLaunched(true)
                .execute();
        mWmState.assertVisibility(TEST_ACTIVITY, true);
        mWmState.waitForActivityState(TRANSLUCENT_TEST_ACTIVITY, STATE_STOPPED);
        mWmState.assertVisibility(TRANSLUCENT_TEST_ACTIVITY, false);
        mWmState.assertVisibility(TEST_ACTIVITY_WITH_SAME_AFFINITY, false);

        // Finish the top opaque activity and both the two activities should be visible.
        mBroadcastActionTrigger.doAction(TEST_ACTIVITY_ACTION_FINISH_SELF);
        mWmState.computeState(new WaitForValidActivityState(TRANSLUCENT_TEST_ACTIVITY));
        mWmState.assertVisibility(TRANSLUCENT_TEST_ACTIVITY, true);
        mWmState.assertVisibility(TEST_ACTIVITY_WITH_SAME_AFFINITY, true);
    }
}
