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
 * limitations under the License
 */

package android.server.wm;

import static android.app.ActivityManager.LOCK_TASK_MODE_NONE;
import static android.app.ActivityTaskManager.INVALID_STACK_ID;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_HOME;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
import static android.app.WindowConfiguration.WINDOWING_MODE_PINNED;
import static android.app.WindowConfiguration.WINDOWING_MODE_UNDEFINED;
import static android.server.wm.ComponentNameUtils.getActivityName;
import static android.server.wm.ComponentNameUtils.getWindowName;
import static android.server.wm.UiDeviceUtils.pressWindowButton;
import static android.server.wm.WindowManagerState.STATE_PAUSED;
import static android.server.wm.WindowManagerState.STATE_RESUMED;
import static android.server.wm.WindowManagerState.STATE_STOPPED;
import static android.server.wm.WindowManagerState.dpToPx;
import static android.server.wm.app.Components.ALWAYS_FOCUSABLE_PIP_ACTIVITY;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.LAUNCH_ENTER_PIP_ACTIVITY;
import static android.server.wm.app.Components.LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY;
import static android.server.wm.app.Components.NON_RESIZEABLE_ACTIVITY;
import static android.server.wm.app.Components.PIP_ACTIVITY;
import static android.server.wm.app.Components.PIP_ACTIVITY2;
import static android.server.wm.app.Components.PIP_ACTIVITY_WITH_MINIMAL_SIZE;
import static android.server.wm.app.Components.PIP_ACTIVITY_WITH_SAME_AFFINITY;
import static android.server.wm.app.Components.PIP_ACTIVITY_WITH_TINY_MINIMAL_SIZE;
import static android.server.wm.app.Components.PIP_ON_STOP_ACTIVITY;
import static android.server.wm.app.Components.PipActivity.ACTION_ENTER_PIP;
import static android.server.wm.app.Components.PipActivity.ACTION_FINISH;
import static android.server.wm.app.Components.PipActivity.ACTION_MOVE_TO_BACK;
import static android.server.wm.app.Components.PipActivity.ACTION_ON_PIP_REQUESTED;
import static android.server.wm.app.Components.PipActivity.EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ON_PAUSE;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ON_PIP_REQUESTED;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP_ON_USER_LEAVE_HINT;
import static android.server.wm.app.Components.PipActivity.EXTRA_FINISH_SELF_ON_RESUME;
import static android.server.wm.app.Components.PipActivity.EXTRA_ON_PAUSE_DELAY;
import static android.server.wm.app.Components.PipActivity.EXTRA_PIP_ORIENTATION;
import static android.server.wm.app.Components.PipActivity.EXTRA_SET_ASPECT_RATIO_DENOMINATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_SET_ASPECT_RATIO_NUMERATOR;
import static android.server.wm.app.Components.PipActivity.EXTRA_START_ACTIVITY;
import static android.server.wm.app.Components.PipActivity.EXTRA_TAP_TO_FINISH;
import static android.server.wm.app.Components.RESUME_WHILE_PAUSING_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY_WITH_SAME_AFFINITY;
import static android.server.wm.app.Components.TRANSLUCENT_TEST_ACTIVITY;
import static android.server.wm.app.Components.TestActivity.EXTRA_CONFIGURATION;
import static android.server.wm.app.Components.TestActivity.EXTRA_FIXED_ORIENTATION;
import static android.server.wm.app.Components.TestActivity.TEST_ACTIVITY_ACTION_FINISH_SELF;
import static android.server.wm.app27.Components.SDK_27_LAUNCH_ENTER_PIP_ACTIVITY;
import static android.server.wm.app27.Components.SDK_27_PIP_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.SecurityTest;
import android.provider.Settings;
import android.server.wm.CommandSession.ActivityCallback;
import android.server.wm.CommandSession.SizeInfo;
import android.server.wm.TestJournalProvider.TestJournalContainer;
import android.server.wm.WindowManagerState.ActivityTask;
import android.server.wm.settings.SettingsSession;
import android.util.Log;
import android.util.Size;

import androidx.test.filters.FlakyTest;

import com.android.compatibility.common.util.AppOpsUtils;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Build/Install/Run:
 * atest CtsWindowManagerDeviceTestCases:PinnedStackTests
 */
@Presubmit
@android.server.wm.annotation.Group2
public class PinnedStackTests extends ActivityManagerTestBase {
    private static final String TAG = PinnedStackTests.class.getSimpleName();

    private static final String APP_OPS_OP_ENTER_PICTURE_IN_PICTURE = "PICTURE_IN_PICTURE";
    private static final int APP_OPS_MODE_IGNORED = 1;

    private static final int ROTATION_0 = 0;
    private static final int ROTATION_90 = 1;
    private static final int ROTATION_180 = 2;
    private static final int ROTATION_270 = 3;

    // Corresponds to ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
    private static final int ORIENTATION_LANDSCAPE = 0;
    // Corresponds to ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
    private static final int ORIENTATION_PORTRAIT = 1;

    private static final float FLOAT_COMPARE_EPSILON = 0.005f;

    // Corresponds to com.android.internal.R.dimen.config_pictureInPictureMinAspectRatio
    private static final int MIN_ASPECT_RATIO_NUMERATOR = 100;
    private static final int MIN_ASPECT_RATIO_DENOMINATOR = 239;
    private static final int BELOW_MIN_ASPECT_RATIO_DENOMINATOR = MIN_ASPECT_RATIO_DENOMINATOR + 1;
    // Corresponds to com.android.internal.R.dimen.config_pictureInPictureMaxAspectRatio
    private static final int MAX_ASPECT_RATIO_NUMERATOR = 239;
    private static final int MAX_ASPECT_RATIO_DENOMINATOR = 100;
    private static final int ABOVE_MAX_ASPECT_RATIO_NUMERATOR = MAX_ASPECT_RATIO_NUMERATOR + 1;
    // Corresponds to com.android.internal.R.dimen.overridable_minimal_size_pip_resizable_task
    private static final int OVERRIDABLE_MINIMAL_SIZE_PIP_RESIZABLE_TASK = 48;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsPip());
    }

    @Test
    public void testMinimumDeviceSize() throws Exception {
        mWmState.assertDeviceDefaultDisplaySize(
                "Devices supporting picture-in-picture must be larger than the default minimum"
                        + " task size");
    }

    @Test
    public void testEnterPictureInPictureMode() throws Exception {
        pinnedStackTester(getAmStartCmd(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true"),
                PIP_ACTIVITY, PIP_ACTIVITY, false /* moveTopToPinnedStack */,
                false /* isFocusable */);
    }

    @Test
    public void testMoveTopActivityToPinnedStack() throws Exception {
        pinnedStackTester(getAmStartCmd(PIP_ACTIVITY), PIP_ACTIVITY, PIP_ACTIVITY,
                true /* moveTopToPinnedStack */, false /* isFocusable */);
    }

    // This test is black-listed in cts-known-failures.xml (b/35314835).
    @Ignore
    @Test
    public void testAlwaysFocusablePipActivity() throws Exception {
        pinnedStackTester(getAmStartCmd(ALWAYS_FOCUSABLE_PIP_ACTIVITY),
                ALWAYS_FOCUSABLE_PIP_ACTIVITY, ALWAYS_FOCUSABLE_PIP_ACTIVITY,
                false /* moveTopToPinnedStack */, true /* isFocusable */);
    }

    // This test is black-listed in cts-known-failures.xml (b/35314835).
    @Ignore
    @Test
    public void testLaunchIntoPinnedStack() throws Exception {
        pinnedStackTester(getAmStartCmd(LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY),
                LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY, ALWAYS_FOCUSABLE_PIP_ACTIVITY,
                false /* moveTopToPinnedStack */, true /* isFocusable */);
    }

    @Test
    public void testNonTappablePipActivity() throws Exception {
        // Launch the tap-to-finish activity at a specific place
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_TAP_TO_FINISH, "true");
        // Wait for animation complete since we are tapping on specific bounds
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Tap the screen at a known location in the pinned stack bounds, and ensure that it is
        // not passed down to the top task
        tapToFinishPip();
        mWmState.computeState(
                new WaitForValidActivityState(PIP_ACTIVITY));
        mWmState.assertVisibility(PIP_ACTIVITY, true);
    }

    @Test
    public void testPinnedStackInBoundsAfterRotation() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Launch an activity that is not fixed-orientation so that the display can rotate
        launchActivity(TEST_ACTIVITY);
        // Launch an activity into the pinned stack
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_TAP_TO_FINISH, "true");
        // Wait for animation complete since we are comparing bounds
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);

        // Ensure that the PIP stack is fully visible in each orientation
        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);
        assertPinnedStackActivityIsInDisplayBounds(PIP_ACTIVITY);
        rotationSession.set(ROTATION_90);
        assertPinnedStackActivityIsInDisplayBounds(PIP_ACTIVITY);
        rotationSession.set(ROTATION_180);
        assertPinnedStackActivityIsInDisplayBounds(PIP_ACTIVITY);
        rotationSession.set(ROTATION_270);
        assertPinnedStackActivityIsInDisplayBounds(PIP_ACTIVITY);
    }

    @Test
    public void testEnterPipToOtherOrientation() throws Exception {
        // Launch a portrait only app on the fullscreen stack
        launchActivity(TEST_ACTIVITY,
                EXTRA_FIXED_ORIENTATION, String.valueOf(ORIENTATION_PORTRAIT));
        // Launch the PiP activity fixed as landscape
        launchActivity(PIP_ACTIVITY,
                EXTRA_PIP_ORIENTATION, String.valueOf(ORIENTATION_LANDSCAPE));
        // Enter PiP, and assert that the PiP is within bounds now that the device is back in
        // portrait
        mBroadcastActionTrigger.doAction(ACTION_ENTER_PIP);
        // Wait for animation complete since we are comparing bounds
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();
        assertPinnedStackActivityIsInDisplayBounds(PIP_ACTIVITY);
    }

    @Test
    public void testEnterPipWithMinimalSize() throws Exception {
        // Launch a PiP activity with minimal size specified
        launchActivity(PIP_ACTIVITY_WITH_MINIMAL_SIZE, EXTRA_ENTER_PIP, "true");
        // Wait for animation complete since we are comparing size
        waitForEnterPipAnimationComplete(PIP_ACTIVITY_WITH_MINIMAL_SIZE);
        assertPinnedStackExists();

        // query the minimal size
        final PackageManager pm = getInstrumentation().getTargetContext().getPackageManager();
        final ActivityInfo info = pm.getActivityInfo(
                PIP_ACTIVITY_WITH_MINIMAL_SIZE, 0 /* flags */);
        final Size minSize = new Size(info.windowLayout.minWidth, info.windowLayout.minHeight);

        // compare the bounds with minimal size
        final Rect pipBounds = getPinnedStackBounds();
        assertTrue("Pinned stack bounds is no smaller than minimal",
                (pipBounds.width() == minSize.getWidth()
                        && pipBounds.height() >= minSize.getHeight())
                        || (pipBounds.height() == minSize.getHeight()
                        && pipBounds.width() >= minSize.getWidth()));
    }

    @Test
    @SecurityTest(minPatchLevel="2021-03")
    public void testEnterPipWithTinyMinimalSize() throws Exception {
        // Launch a PiP activity with minimal size specified and smaller than allowed minimum
        launchActivity(PIP_ACTIVITY_WITH_TINY_MINIMAL_SIZE, EXTRA_ENTER_PIP, "true");
        // Wait for animation complete since we are comparing size
        waitForEnterPipAnimationComplete(PIP_ACTIVITY_WITH_TINY_MINIMAL_SIZE);
        assertPinnedStackExists();

        final WindowManagerState.WindowState windowState = getWindowState(
                PIP_ACTIVITY_WITH_TINY_MINIMAL_SIZE);
        final WindowManagerState.DisplayContent display = mWmState.getDisplay(
                windowState.getDisplayId());
        final int overridableMinSize = dpToPx(
                OVERRIDABLE_MINIMAL_SIZE_PIP_RESIZABLE_TASK, display.getDpi());

        // compare the bounds to verify that it's no smaller than allowed minimum on both dimensions
        final Rect pipBounds = getPinnedStackBounds();
        assertTrue("Pinned task bounds " + pipBounds + " isn't smaller than minimal "
                        + overridableMinSize + " on both dimensions",
                pipBounds.width() >= overridableMinSize
                        && pipBounds.height() >= overridableMinSize);
    }

    @Test
    public void testEnterPipAspectRatioMin() throws Exception {
        testEnterPipAspectRatio(MIN_ASPECT_RATIO_NUMERATOR, MIN_ASPECT_RATIO_DENOMINATOR);
    }

    @Test
    public void testEnterPipAspectRatioMax() throws Exception {
        testEnterPipAspectRatio(MAX_ASPECT_RATIO_NUMERATOR, MAX_ASPECT_RATIO_DENOMINATOR);
    }

    private void testEnterPipAspectRatio(int num, int denom) throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR, Integer.toString(num),
                EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR, Integer.toString(denom));
        // Wait for animation complete since we are comparing aspect ratio
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Assert that we have entered PIP and that the aspect ratio is correct
        Rect pinnedStackBounds = getPinnedStackBounds();
        assertFloatEquals((float) pinnedStackBounds.width() / pinnedStackBounds.height(),
                (float) num / denom);
    }

    @Test
    public void testResizePipAspectRatioMin() throws Exception {
        testResizePipAspectRatio(MIN_ASPECT_RATIO_NUMERATOR, MIN_ASPECT_RATIO_DENOMINATOR);
    }

    @Test
    public void testResizePipAspectRatioMax() throws Exception {
        testResizePipAspectRatio(MAX_ASPECT_RATIO_NUMERATOR, MAX_ASPECT_RATIO_DENOMINATOR);
    }

    private void testResizePipAspectRatio(int num, int denom) throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_SET_ASPECT_RATIO_NUMERATOR, Integer.toString(num),
                EXTRA_SET_ASPECT_RATIO_DENOMINATOR, Integer.toString(denom));
        // Wait for animation complete since we are comparing aspect ratio
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();
        waitForValidAspectRatio(num, denom);
        Rect bounds = getPinnedStackBounds();
        assertFloatEquals((float) bounds.width() / bounds.height(), (float) num / denom);
    }

    @Test
    public void testEnterPipExtremeAspectRatioMin() throws Exception {
        testEnterPipExtremeAspectRatio(MIN_ASPECT_RATIO_NUMERATOR,
                BELOW_MIN_ASPECT_RATIO_DENOMINATOR);
    }

    @Test
    public void testEnterPipExtremeAspectRatioMax() throws Exception {
        testEnterPipExtremeAspectRatio(ABOVE_MAX_ASPECT_RATIO_NUMERATOR,
                MAX_ASPECT_RATIO_DENOMINATOR);
    }

    private void testEnterPipExtremeAspectRatio(int num, int denom) throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Assert that we could not create a pinned stack with an extreme aspect ratio
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR, Integer.toString(num),
                EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR, Integer.toString(denom));
        assertPinnedStackDoesNotExist();
    }

    @Test
    public void testSetPipExtremeAspectRatioMin() throws Exception {
        testSetPipExtremeAspectRatio(MIN_ASPECT_RATIO_NUMERATOR,
                BELOW_MIN_ASPECT_RATIO_DENOMINATOR);
    }

    @Test
    public void testSetPipExtremeAspectRatioMax() throws Exception {
        testSetPipExtremeAspectRatio(ABOVE_MAX_ASPECT_RATIO_NUMERATOR,
                MAX_ASPECT_RATIO_DENOMINATOR);
    }

    private void testSetPipExtremeAspectRatio(int num, int denom) throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Try to resize the a normal pinned stack to an extreme aspect ratio and ensure that
        // fails (the aspect ratio remains the same)
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR,
                        Integer.toString(MAX_ASPECT_RATIO_NUMERATOR),
                EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR,
                        Integer.toString(MAX_ASPECT_RATIO_DENOMINATOR),
                EXTRA_SET_ASPECT_RATIO_NUMERATOR, Integer.toString(num),
                EXTRA_SET_ASPECT_RATIO_DENOMINATOR, Integer.toString(denom));
        // Wait for animation complete since we are comparing aspect ratio
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();
        Rect pinnedStackBounds = getPinnedStackBounds();
        assertFloatEquals((float) pinnedStackBounds.width() / pinnedStackBounds.height(),
                (float) MAX_ASPECT_RATIO_NUMERATOR / MAX_ASPECT_RATIO_DENOMINATOR);
    }

    @Test
    public void testDisallowPipLaunchFromStoppedActivity() throws Exception {
        // Launch the bottom pip activity which will launch a new activity on top and attempt to
        // enter pip when it is stopped
        launchActivity(PIP_ON_STOP_ACTIVITY);

        // Wait for the bottom pip activity to be stopped
        mWmState.waitForActivityState(PIP_ON_STOP_ACTIVITY, STATE_STOPPED);

        // Assert that there is no pinned stack (that enterPictureInPicture() failed)
        assertPinnedStackDoesNotExist();
    }

    @Test
    public void testAutoEnterPictureInPicture() throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Launch the PIP activity on pause
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP_ON_PAUSE, "true");
        assertPinnedStackDoesNotExist();

        // Go home and ensure that there is a pinned stack
        launchHomeActivity();
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
    }

    @Test
    public void testAutoEnterPictureInPictureOnUserLeaveHintWhenPipRequestedNotOverridden()
            throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Launch the PIP activity that enters PIP on user leave hint, not on PIP requested
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP_ON_USER_LEAVE_HINT, "true");
        assertPinnedStackDoesNotExist();

        // Go home and ensure that there is a pinned stack
        separateTestJournal();
        launchHomeActivity();
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();

        final ActivityLifecycleCounts lifecycleCounts = new ActivityLifecycleCounts(PIP_ACTIVITY);
        // Check the order of the callbacks accounting for a task overlay activity that might show.
        // The PIP request (with a user leave hint) should come before the pip mode change.
        final int firstUserLeaveIndex =
                lifecycleCounts.getFirstIndex(ActivityCallback.ON_USER_LEAVE_HINT);
        final int firstPipRequestedIndex =
                lifecycleCounts.getFirstIndex(ActivityCallback.ON_PICTURE_IN_PICTURE_REQUESTED);
        final int firstPipModeChangedIndex =
                lifecycleCounts.getFirstIndex(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED);
        assertTrue("missing request", firstPipRequestedIndex != -1);
        assertTrue("missing user leave", firstUserLeaveIndex != -1);
        assertTrue("missing pip mode changed", firstPipModeChangedIndex != -1);
        assertTrue("pip requested not before pause",
                firstPipRequestedIndex < firstUserLeaveIndex);
        assertTrue("unexpected user leave hint",
                firstUserLeaveIndex < firstPipModeChangedIndex);
    }

    @Test
    public void testAutoEnterPictureInPictureOnPictureInPictureRequested() throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Launch the PIP activity on pip requested
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP_ON_PIP_REQUESTED, "true");
        assertPinnedStackDoesNotExist();

        // Call onPictureInPictureRequested and verify activity enters pip
        separateTestJournal();
        mBroadcastActionTrigger.doAction(ACTION_ON_PIP_REQUESTED);
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();

        final ActivityLifecycleCounts lifecycleCounts = new ActivityLifecycleCounts(PIP_ACTIVITY);
        // Check the order of the callbacks accounting for a task overlay activity that might show.
        // The PIP request (without a user leave hint) should come before the pip mode change.
        final int firstUserLeaveIndex =
                lifecycleCounts.getFirstIndex(ActivityCallback.ON_USER_LEAVE_HINT);
        final int firstPipRequestedIndex =
                lifecycleCounts.getFirstIndex(ActivityCallback.ON_PICTURE_IN_PICTURE_REQUESTED);
        final int firstPipModeChangedIndex =
                lifecycleCounts.getFirstIndex(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED);
        assertTrue("missing request", firstPipRequestedIndex != -1);
        assertTrue("missing pip mode changed", firstPipModeChangedIndex != -1);
        assertTrue("pip requested not before pause",
                firstPipRequestedIndex < firstPipModeChangedIndex);
        assertTrue("unexpected user leave hint",
                firstUserLeaveIndex == -1 || firstUserLeaveIndex > firstPipModeChangedIndex);
    }

    @Test
    public void testAutoEnterPictureInPictureLaunchActivity() throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Launch the PIP activity on pause, and have it start another activity on
        // top of itself.  Wait for the new activity to be visible and ensure that the pinned stack
        // was not created in the process
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP_ON_PAUSE, "true",
                EXTRA_START_ACTIVITY, getActivityName(NON_RESIZEABLE_ACTIVITY));
        mWmState.computeState(
                new WaitForValidActivityState(NON_RESIZEABLE_ACTIVITY));
        assertPinnedStackDoesNotExist();

        // Go home while the pip activity is open and ensure the previous activity is not PIPed
        launchHomeActivity();
        assertPinnedStackDoesNotExist();
    }

    @Test
    public void testAutoEnterPictureInPictureFinish() throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Launch the PIP activity on pause, and set it to finish itself after
        // some period.  Wait for the previous activity to be visible, and ensure that the pinned
        // stack was not created in the process
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP_ON_PAUSE, "true",
                EXTRA_FINISH_SELF_ON_RESUME, "true");
        assertPinnedStackDoesNotExist();
    }

    @Test
    public void testAutoEnterPictureInPictureAspectRatio() throws Exception {
        // Launch the PIP activity on pause, and set the aspect ratio
        launchActivity(PIP_ACTIVITY,
                EXTRA_ENTER_PIP_ON_PAUSE, "true",
                EXTRA_SET_ASPECT_RATIO_NUMERATOR, Integer.toString(MAX_ASPECT_RATIO_NUMERATOR),
                EXTRA_SET_ASPECT_RATIO_DENOMINATOR, Integer.toString(MAX_ASPECT_RATIO_DENOMINATOR));

        // Go home while the pip activity is open to trigger auto-PIP
        launchHomeActivity();
        // Wait for animation complete since we are comparing aspect ratio
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();

        waitForValidAspectRatio(MAX_ASPECT_RATIO_NUMERATOR, MAX_ASPECT_RATIO_DENOMINATOR);
        Rect bounds = getPinnedStackBounds();
        assertFloatEquals((float) bounds.width() / bounds.height(),
                (float) MAX_ASPECT_RATIO_NUMERATOR / MAX_ASPECT_RATIO_DENOMINATOR);
    }

    @Test
    public void testAutoEnterPictureInPictureOverPip() throws Exception {
        // Launch another PIP activity
        launchActivity(LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY);
        waitForEnterPip(ALWAYS_FOCUSABLE_PIP_ACTIVITY);
        assertPinnedStackExists();

        // Launch the PIP activity on pause
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP_ON_PAUSE, "true");

        // Go home while the PIP activity is open to try to trigger auto-enter PIP
        launchHomeActivity();
        assertPinnedStackExists();

        // Ensure that auto-enter pip failed and that the resumed activity in the pinned stack is
        // still the first activity
        final ActivityTask pinnedStack = getPinnedStack();
        assertEquals(getActivityName(ALWAYS_FOCUSABLE_PIP_ACTIVITY), pinnedStack.mRealActivity);
    }

    @Test
    public void testDismissPipWhenLaunchNewOne() throws Exception {
        // Launch another PIP activity
        launchActivity(LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY);
        waitForEnterPip(ALWAYS_FOCUSABLE_PIP_ACTIVITY);
        assertPinnedStackExists();
        final ActivityTask pinnedStack = getPinnedStack();

        launchActivityInNewTask(LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY);
        waitForEnterPip(ALWAYS_FOCUSABLE_PIP_ACTIVITY);

        assertEquals(1, mWmState.countStacks(WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD));
    }

    @Test
    public void testDisallowMultipleTasksInPinnedStack() throws Exception {
        // Launch a test activity so that we have multiple fullscreen tasks
        launchActivity(TEST_ACTIVITY);

        // Launch first PIP activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);

        // Launch second PIP activity
        launchActivity(PIP_ACTIVITY2, EXTRA_ENTER_PIP, "true");

        final ActivityTask pinnedStack = getPinnedStack();
        assertEquals(0, pinnedStack.getTasks().size());
        assertTrue(mWmState.containsActivityInWindowingMode(
                PIP_ACTIVITY2, WINDOWING_MODE_PINNED));
        assertTrue(mWmState.containsActivityInWindowingMode(
                PIP_ACTIVITY, WINDOWING_MODE_FULLSCREEN));
    }

    @Test
    public void testPipUnPipOverHome() throws Exception {
        // Go home
        launchHomeActivity();
        // Launch an auto pip activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Relaunch the activity to fullscreen to trigger the activity to exit and re-enter pip
        launchActivity(PIP_ACTIVITY);
        waitForExitPipToFullscreen(PIP_ACTIVITY);
        mBroadcastActionTrigger.doAction(ACTION_ENTER_PIP);
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        mWmState.assertHomeActivityVisible(true);
    }

    @Test
    public void testPipUnPipOverApp() throws Exception {
        // Launch a test activity so that we're not over home
        launchActivity(TEST_ACTIVITY);

        // Launch an auto pip activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Relaunch the activity to fullscreen to trigger the activity to exit and re-enter pip
        launchActivity(PIP_ACTIVITY);
        waitForExitPipToFullscreen(PIP_ACTIVITY);
        mBroadcastActionTrigger.doAction(ACTION_ENTER_PIP);
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        mWmState.assertVisibility(TEST_ACTIVITY, true);
    }

    @Test
    public void testRemovePipWithNoFullscreenStack() throws Exception {
        // Launch a pip activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Remove the stack and ensure that the task is now in the fullscreen stack (when no
        // fullscreen stack existed before)
        removeStacksInWindowingModes(WINDOWING_MODE_PINNED);
        assertPinnedStackStateOnMoveToFullscreen(PIP_ACTIVITY,
                WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
    }

    @Test
    public void testRemovePipWithVisibleFullscreenStack() throws Exception {
        // Launch a fullscreen activity, and a pip activity over that
        launchActivity(TEST_ACTIVITY);
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Remove the stack and ensure that the task is placed in the fullscreen stack, behind the
        // top fullscreen activity
        removeStacksInWindowingModes(WINDOWING_MODE_PINNED);
        assertPinnedStackStateOnMoveToFullscreen(PIP_ACTIVITY,
                WINDOWING_MODE_FULLSCREEN, ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testRemovePipWithHiddenFullscreenStack() throws Exception {
        // Launch a fullscreen activity, return home and while the fullscreen stack is hidden,
        // launch a pip activity over home
        launchActivity(TEST_ACTIVITY);
        launchHomeActivity();
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Remove the stack and ensure that the task is placed on top of the hidden fullscreen
        // stack, but that the home stack is still focused
        removeStacksInWindowingModes(WINDOWING_MODE_PINNED);
        assertPinnedStackStateOnMoveToFullscreen(PIP_ACTIVITY,
                WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
    }

    @Test
    public void testMovePipToBackWithNoFullscreenStack() throws Exception {
        // Start with a clean slate, remove all the stacks but home
        removeStacksWithActivityTypes(ALL_ACTIVITY_TYPE_BUT_HOME);

        // Launch a pip activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Remove the stack and ensure that the task is now in the fullscreen stack (when no
        // fullscreen stack existed before)
        mBroadcastActionTrigger.doAction(ACTION_MOVE_TO_BACK);
        assertPinnedStackStateOnMoveToFullscreen(PIP_ACTIVITY,
                WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
    }

    @Test
    public void testMovePipToBackWithVisibleFullscreenStack() throws Exception {
        // Launch a fullscreen activity, and a pip activity over that
        launchActivity(TEST_ACTIVITY);
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Remove the stack and ensure that the task is placed in the fullscreen stack, behind the
        // top fullscreen activity
        mBroadcastActionTrigger.doAction(ACTION_MOVE_TO_BACK);
        assertPinnedStackStateOnMoveToFullscreen(PIP_ACTIVITY,
                WINDOWING_MODE_FULLSCREEN, ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void testMovePipToBackWithHiddenFullscreenStack() throws Exception {
        // Launch a fullscreen activity, return home and while the fullscreen stack is hidden,
        // launch a pip activity over home
        launchActivity(TEST_ACTIVITY);
        launchHomeActivity();
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Remove the stack and ensure that the task is placed on top of the hidden fullscreen
        // stack, but that the home stack is still focused
        mBroadcastActionTrigger.doAction(ACTION_MOVE_TO_BACK);
        assertPinnedStackStateOnMoveToFullscreen(
                PIP_ACTIVITY, WINDOWING_MODE_UNDEFINED, ACTIVITY_TYPE_HOME);
    }

    @Test
    public void testPinnedStackAlwaysOnTop() throws Exception {
        // Launch activity into pinned stack and assert it's on top.
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
        assertPinnedStackIsOnTop();

        // Launch another activity in fullscreen stack and check that pinned stack is still on top.
        launchActivity(TEST_ACTIVITY);
        assertPinnedStackExists();
        assertPinnedStackIsOnTop();

        // Launch home and check that pinned stack is still on top.
        launchHomeActivity();
        assertPinnedStackExists();
        assertPinnedStackIsOnTop();
    }

    @Test
    public void testAppOpsDenyPipOnPause() throws Exception {
        try (final AppOpsSession appOpsSession = new AppOpsSession(PIP_ACTIVITY)) {
            // Disable enter-pip and try to enter pip
            appOpsSession.setOpToMode(APP_OPS_OP_ENTER_PICTURE_IN_PICTURE, APP_OPS_MODE_IGNORED);

            // Launch the PIP activity on pause
            launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
            assertPinnedStackDoesNotExist();

            // Go home and ensure that there is no pinned stack
            launchHomeActivity();
            assertPinnedStackDoesNotExist();
        }
    }

    @Test
    public void testEnterPipFromTaskWithMultipleActivities() throws Exception {
        // Try to enter picture-in-picture from an activity that has more than one activity in the
        // task and ensure that it works
        launchActivity(LAUNCH_ENTER_PIP_ACTIVITY);
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
    }

    @Test
    public void testLaunchStoppedActivityWithPiPInSameProcessPreQ() {
        // Try to enter picture-in-picture from an activity that has more than one activity in the
        // task and ensure that it works, for pre-Q app
        launchActivity(SDK_27_LAUNCH_ENTER_PIP_ACTIVITY,
                EXTRA_ENTER_PIP, "true");
        waitForEnterPip(SDK_27_PIP_ACTIVITY);
        assertPinnedStackExists();

        // Puts the host activity to stopped state
        launchHomeActivity();
        mWmState.assertHomeActivityVisible(true);
        waitAndAssertActivityState(SDK_27_LAUNCH_ENTER_PIP_ACTIVITY, STATE_STOPPED,
                "Activity should become STOPPED");
        mWmState.assertVisibility(SDK_27_LAUNCH_ENTER_PIP_ACTIVITY, false);

        // Host activity should be visible after re-launch and PiP window still exists
        launchActivity(SDK_27_LAUNCH_ENTER_PIP_ACTIVITY);
        waitAndAssertActivityState(SDK_27_LAUNCH_ENTER_PIP_ACTIVITY, STATE_RESUMED,
                "Activity should become RESUMED");
        mWmState.assertVisibility(SDK_27_LAUNCH_ENTER_PIP_ACTIVITY, true);
        assertPinnedStackExists();
    }

    @Test
    public void testEnterPipWithResumeWhilePausingActivityNoStop() throws Exception {
        /*
         * Launch the resumeWhilePausing activity and ensure that the PiP activity did not get
         * stopped and actually went into the pinned stack.
         *
         * Note that this is a workaround because to trigger the path that we want to happen in
         * activity manager, we need to add the leaving activity to the stopping state, which only
         * happens when a hidden stack is brought forward. Normally, this happens when you go home,
         * but since we can't launch into the home stack directly, we have a workaround.
         *
         * 1) Launch an activity in a new dynamic stack
         * 2) Start the PiP activity that will enter picture-in-picture when paused in the
         *    fullscreen stack
         * 3) Bring the activity in the dynamic stack forward to trigger PiP
         */
        launchActivity(RESUME_WHILE_PAUSING_ACTIVITY);
        // Launch an activity that will enter PiP when it is paused with a delay that is long enough
        // for the next resumeWhilePausing activity to finish resuming, but slow enough to not
        // trigger the current system pause timeout (currently 500ms)
        launchActivity(PIP_ACTIVITY, WINDOWING_MODE_FULLSCREEN,
                EXTRA_ENTER_PIP_ON_PAUSE, "true",
                EXTRA_ON_PAUSE_DELAY, "350",
                EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP, "true");
        launchActivity(RESUME_WHILE_PAUSING_ACTIVITY);
        assertPinnedStackExists();
    }

    @Test
    public void testDisallowEnterPipActivityLocked() throws Exception {
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP_ON_PAUSE, "true");
        ActivityTask task = mWmState.getStandardStackByWindowingMode(
                WINDOWING_MODE_FULLSCREEN).getTopTask();

        // Lock the task and ensure that we can't enter picture-in-picture both explicitly and
        // when paused
        SystemUtil.runWithShellPermissionIdentity(() -> {
            try {
                mAtm.startSystemLockTaskMode(task.mTaskId);
                waitForOrFail("Task in lock mode", () -> {
                    return mAm.getLockTaskModeState() != LOCK_TASK_MODE_NONE;
                });
                mBroadcastActionTrigger.doAction(ACTION_ENTER_PIP);
                waitForEnterPip(PIP_ACTIVITY);
                assertPinnedStackDoesNotExist();
                launchHomeActivity();
                assertPinnedStackDoesNotExist();
            } finally {
                mAtm.stopSystemLockTaskMode();
            }
        });
    }

    @Test
    public void testConfigurationChangeOrderDuringTransition() throws Exception {
        // Launch a PiP activity and ensure configuration change only happened once, and that the
        // configuration change happened after the picture-in-picture and multi-window callbacks
        launchActivity(PIP_ACTIVITY);
        separateTestJournal();
        mBroadcastActionTrigger.doAction(ACTION_ENTER_PIP);
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
        waitForValidPictureInPictureCallbacks(PIP_ACTIVITY);
        assertValidPictureInPictureCallbackOrder(PIP_ACTIVITY);

        // Trigger it to go back to fullscreen and ensure that only triggered one configuration
        // change as well
        separateTestJournal();
        launchActivity(PIP_ACTIVITY);
        waitForValidPictureInPictureCallbacks(PIP_ACTIVITY);
        assertValidPictureInPictureCallbackOrder(PIP_ACTIVITY);
    }

    /** Helper class to save, set, and restore transition_animation_scale preferences. */
    private static class TransitionAnimationScaleSession extends SettingsSession<Float> {
        TransitionAnimationScaleSession() {
            super(Settings.Global.getUriFor(Settings.Global.TRANSITION_ANIMATION_SCALE),
                    Settings.Global::getFloat,
                    Settings.Global::putFloat);
        }

        @Override
        public void close() {
            // Wait for the restored setting to apply before we continue on with the next test
            final CountDownLatch waitLock = new CountDownLatch(1);
            final Context context = getInstrumentation().getTargetContext();
            context.getContentResolver().registerContentObserver(mUri, false,
                    new ContentObserver(new Handler(Looper.getMainLooper())) {
                        @Override
                        public void onChange(boolean selfChange) {
                            waitLock.countDown();
                        }
                    });
            super.close();
            try {
                if (!waitLock.await(2, TimeUnit.SECONDS)) {
                    Log.i(TAG, "TransitionAnimationScaleSession value not restored");
                }
            } catch (InterruptedException impossible) {}
        }
    }

    @Ignore("b/149946388")
    @Test
    public void testEnterPipInterruptedCallbacks() {
        final TransitionAnimationScaleSession transitionAnimationScaleSession =
                mObjectTracker.manage(new TransitionAnimationScaleSession());
        // Slow down the transition animations for this test
        transitionAnimationScaleSession.set(20f);

        // Launch a PiP activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        // Wait until the PiP activity has moved into the pinned stack (happens before the
        // transition has started)
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Relaunch the PiP activity back into fullscreen
        separateTestJournal();
        launchActivity(PIP_ACTIVITY);
        // Wait until the PiP activity is reparented into the fullscreen stack (happens after
        // the transition has finished)
        waitForExitPipToFullscreen(PIP_ACTIVITY);

        // Ensure that we get the callbacks indicating that PiP/MW mode was cancelled, but no
        // configuration change (since none was sent)
        final ActivityLifecycleCounts lifecycleCounts = new ActivityLifecycleCounts(PIP_ACTIVITY);
        assertEquals("onConfigurationChanged", 0,
                lifecycleCounts.getCount(ActivityCallback.ON_CONFIGURATION_CHANGED));
        assertEquals("onPictureInPictureModeChanged", 1,
                lifecycleCounts.getCount(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED));
        assertEquals("onMultiWindowModeChanged", 1,
                lifecycleCounts.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED));
    }

    @Test
    public void testStopBeforeMultiWindowCallbacksOnDismiss() throws Exception {
        // Launch a PiP activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        // Wait for animation complete so that system has reported pip mode change event to
        // client and the last reported pip mode has updated.
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Dismiss it
        separateTestJournal();
        removeStacksInWindowingModes(WINDOWING_MODE_PINNED);
        waitForExitPipToFullscreen(PIP_ACTIVITY);
        waitForValidPictureInPictureCallbacks(PIP_ACTIVITY);

        // Confirm that we get stop before the multi-window and picture-in-picture mode change
        // callbacks
        final ActivityLifecycleCounts lifecycles = new ActivityLifecycleCounts(PIP_ACTIVITY);
        assertEquals("onStop", 1, lifecycles.getCount(ActivityCallback.ON_STOP));
        assertEquals("onPictureInPictureModeChanged", 1,
                lifecycles.getCount(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED));
        assertEquals("onMultiWindowModeChanged", 1,
                lifecycles.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED));
        final int lastStopIndex = lifecycles.getLastIndex(ActivityCallback.ON_STOP);
        final int lastPipIndex = lifecycles.getLastIndex(
                ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED);
        final int lastMwIndex = lifecycles.getLastIndex(
                ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED);
        assertThat("onStop should be before onPictureInPictureModeChanged",
                lastStopIndex, lessThan(lastPipIndex));
        assertThat("onPictureInPictureModeChanged should be before onMultiWindowModeChanged",
                lastPipIndex, lessThan(lastMwIndex));
    }

    @Test
    public void testPreventSetAspectRatioWhileExpanding() throws Exception {
        // Launch the PiP activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);

        // Trigger it to go back to fullscreen and try to set the aspect ratio, and ensure that the
        // call to set the aspect ratio did not prevent the PiP from returning to fullscreen
        mBroadcastActionTrigger.expandPipWithAspectRatio("123456789", "100000000");
        waitForExitPipToFullscreen(PIP_ACTIVITY);
        assertPinnedStackDoesNotExist();
    }

    @Test
    public void testSetRequestedOrientationWhilePinned() throws Exception {
        // Launch the PiP activity fixed as portrait, and enter picture-in-picture
        launchActivity(PIP_ACTIVITY,
                EXTRA_PIP_ORIENTATION, String.valueOf(ORIENTATION_PORTRAIT),
                EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Request that the orientation is set to landscape
        mBroadcastActionTrigger.requestOrientationForPip(ORIENTATION_LANDSCAPE);

        // Launch the activity back into fullscreen and ensure that it is now in landscape
        launchActivity(PIP_ACTIVITY);
        waitForExitPipToFullscreen(PIP_ACTIVITY);
        assertPinnedStackDoesNotExist();
        mWmState.waitForLastOrientation(ORIENTATION_LANDSCAPE);
        assertEquals(ORIENTATION_LANDSCAPE, mWmState.getLastOrientation());
    }

    @Test
    public void testWindowButtonEntersPip() throws Exception {
        assumeTrue(!mWmState.isHomeRecentsComponent());

        // Launch the PiP activity trigger the window button, ensure that we have entered PiP
        launchActivity(PIP_ACTIVITY);
        pressWindowButton();
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
    }

    @Test
    @FlakyTest(bugId=156314330)
    public void testFinishPipActivityWithTaskOverlay() throws Exception {
        // Trigger PiP menu activity to properly lose focuse when going home
        launchActivity(TEST_ACTIVITY);
        // Launch PiP activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
        int taskId = mWmState.getStandardStackByWindowingMode(
                WINDOWING_MODE_PINNED).getTopTask().mTaskId;

        // Ensure that we don't any any other overlays as a result of launching into PIP
        launchHomeActivity();

        // Launch task overlay activity into PiP activity task
        launchPinnedActivityAsTaskOverlay(TRANSLUCENT_TEST_ACTIVITY, taskId);

        // Finish the PiP activity and ensure that there is no pinned stack
        mBroadcastActionTrigger.doAction(ACTION_FINISH);
        waitForPinnedStackRemoved();
        assertPinnedStackDoesNotExist();
    }

    @Test
    public void testNoResumeAfterTaskOverlayFinishes() throws Exception {
        // Launch PiP activity
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();
        ActivityTask stack = mWmState.getStandardStackByWindowingMode(WINDOWING_MODE_PINNED);
        int taskId = stack.getTopTask().mTaskId;

        // Launch task overlay activity into PiP activity task
        launchPinnedActivityAsTaskOverlay(TRANSLUCENT_TEST_ACTIVITY, taskId);

        // Finish the task overlay activity and ensure that the PiP activity never got resumed.
        separateTestJournal();
        mBroadcastActionTrigger.doAction(TEST_ACTIVITY_ACTION_FINISH_SELF);
        mWmState.waitFor((amState) ->
                        !amState.containsActivity(TRANSLUCENT_TEST_ACTIVITY),
                "Waiting for test activity to finish...");
        final ActivityLifecycleCounts lifecycleCounts = new ActivityLifecycleCounts(PIP_ACTIVITY);
        assertEquals("onResume", 0, lifecycleCounts.getCount(ActivityCallback.ON_RESUME));
        assertEquals("onPause", 0, lifecycleCounts.getCount(ActivityCallback.ON_PAUSE));
    }

    @Test
    public void testPinnedStackWithDockedStack() throws Exception {
        assumeTrue(supportsSplitScreenMultiWindow());

        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        waitForEnterPip(PIP_ACTIVITY);
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY)
                        .setRandomData(true)
                        .setMultipleTask(false)
        );
        mWmState.assertVisibility(PIP_ACTIVITY, true);
        mWmState.assertVisibility(LAUNCHING_ACTIVITY, true);
        mWmState.assertVisibility(TEST_ACTIVITY, true);

        // Launch the activities again to take focus and make sure nothing is hidden
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY)
                        .setRandomData(true)
                        .setMultipleTask(false)
        );
        mWmState.assertVisibility(LAUNCHING_ACTIVITY, true);
        mWmState.assertVisibility(TEST_ACTIVITY, true);

        // Go to recents to make sure that fullscreen stack is invisible
        // Some devices do not support recents or implement it differently (instead of using a
        // separate stack id or as an activity), for those cases the visibility asserts will be
        // ignored
        pressAppSwitchButtonAndWaitForRecents();
        mWmState.assertVisibility(LAUNCHING_ACTIVITY, true);
        mWmState.assertVisibility(TEST_ACTIVITY, false);
    }

    @Test
    public void testLaunchTaskByComponentMatchMultipleTasks() throws Exception {
        // Launch a fullscreen activity which will launch a PiP activity in a new task with the same
        // affinity
        launchActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY);
        launchActivity(PIP_ACTIVITY_WITH_SAME_AFFINITY);
        assertPinnedStackExists();

        // Launch the root activity again...
        int rootActivityTaskId = mWmState.getTaskByActivity(
                TEST_ACTIVITY_WITH_SAME_AFFINITY).mTaskId;
        launchHomeActivity();
        launchActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY);

        // ...and ensure that the root activity task is found and reused, and that the pinned stack
        // is unaffected
        assertPinnedStackExists();
        mWmState.assertFocusedActivity("Expected root activity focused",
                TEST_ACTIVITY_WITH_SAME_AFFINITY);
        assertEquals(rootActivityTaskId, mWmState.getTaskByActivity(
                TEST_ACTIVITY_WITH_SAME_AFFINITY).mTaskId);
    }

    @Test
    public void testLaunchTaskByAffinityMatchMultipleTasks() throws Exception {
        // Launch a fullscreen activity which will launch a PiP activity in a new task with the same
        // affinity, and also launch another activity in the same task, while finishing itself. As
        // a result, the task will not have a component matching the same activity as what it was
        // started with
        launchActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY,
                EXTRA_START_ACTIVITY, getActivityName(TEST_ACTIVITY),
                EXTRA_FINISH_SELF_ON_RESUME, "true");
        mWmState.waitForValidState(new WaitForValidActivityState.Builder(TEST_ACTIVITY)
                .setWindowingMode(WINDOWING_MODE_FULLSCREEN)
                .setActivityType(ACTIVITY_TYPE_STANDARD)
                .build());
        launchActivity(PIP_ACTIVITY_WITH_SAME_AFFINITY);
        waitForEnterPip(PIP_ACTIVITY_WITH_SAME_AFFINITY);
        assertPinnedStackExists();

        // Launch the root activity again...
        int rootActivityTaskId = mWmState.getTaskByActivity(
                TEST_ACTIVITY).mTaskId;
        launchHomeActivity();
        launchActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY);

        // ...and ensure that even while matching purely by task affinity, the root activity task is
        // found and reused, and that the pinned stack is unaffected
        assertPinnedStackExists();
        mWmState.assertFocusedActivity("Expected root activity focused", TEST_ACTIVITY);
        assertEquals(rootActivityTaskId, mWmState.getTaskByActivity(
                TEST_ACTIVITY).mTaskId);
    }

    @Test
    public void testLaunchTaskByAffinityMatchSingleTask() throws Exception {
        // Launch an activity into the pinned stack with a fixed affinity
        launchActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY,
                EXTRA_ENTER_PIP, "true",
                EXTRA_START_ACTIVITY, getActivityName(PIP_ACTIVITY),
                EXTRA_FINISH_SELF_ON_RESUME, "true");
        waitForEnterPip(PIP_ACTIVITY);
        assertPinnedStackExists();

        // Launch the root activity again, of the matching task and ensure that we expand to
        // fullscreen
        int activityTaskId = mWmState.getTaskByActivity(PIP_ACTIVITY).mTaskId;
        launchHomeActivity();
        launchActivity(TEST_ACTIVITY_WITH_SAME_AFFINITY);
        waitForExitPipToFullscreen(PIP_ACTIVITY);
        assertPinnedStackDoesNotExist();
        assertEquals(activityTaskId, mWmState.getTaskByActivity(
                PIP_ACTIVITY).mTaskId);
    }

    /** Test that reported display size corresponds to fullscreen after exiting PiP. */
    @Test
    public void testDisplayMetricsPinUnpin() throws Exception {
        separateTestJournal();
        launchActivity(TEST_ACTIVITY);
        final int defaultWindowingMode = mWmState
                .getTaskByActivity(TEST_ACTIVITY).getWindowingMode();
        final SizeInfo initialSizes = getLastReportedSizesForActivity(TEST_ACTIVITY);
        final Rect initialAppBounds = getAppBounds(TEST_ACTIVITY);
        assertNotNull("Must report display dimensions", initialSizes);
        assertNotNull("Must report app bounds", initialAppBounds);

        separateTestJournal();
        launchActivity(PIP_ACTIVITY, EXTRA_ENTER_PIP, "true");
        // Wait for animation complete since we are comparing bounds
        waitForEnterPipAnimationComplete(PIP_ACTIVITY);
        final SizeInfo pinnedSizes = getLastReportedSizesForActivity(PIP_ACTIVITY);
        final Rect pinnedAppBounds = getAppBounds(PIP_ACTIVITY);
        assertNotEquals("Reported display size when pinned must be different from default",
                initialSizes, pinnedSizes);
        final Size initialAppSize = new Size(initialAppBounds.width(), initialAppBounds.height());
        final Size pinnedAppSize = new Size(pinnedAppBounds.width(), pinnedAppBounds.height());
        assertNotEquals("Reported app size when pinned must be different from default",
                initialAppSize, pinnedAppSize);

        separateTestJournal();
        launchActivity(PIP_ACTIVITY, defaultWindowingMode);
        final SizeInfo finalSizes = getLastReportedSizesForActivity(PIP_ACTIVITY);
        final Rect finalAppBounds = getAppBounds(PIP_ACTIVITY);
        final Size finalAppSize = new Size(finalAppBounds.width(), finalAppBounds.height());
        assertEquals("Must report default size after exiting PiP", initialSizes, finalSizes);
        assertEquals("Must report default app size after exiting PiP", initialAppSize,
                finalAppSize);
    }

    /** Get app bounds in last applied configuration. */
    private Rect getAppBounds(ComponentName activityName) {
        final Configuration config = TestJournalContainer.get(activityName).extras
                .getParcelable(EXTRA_CONFIGURATION);
        if (config != null) {
            return config.windowConfiguration.getAppBounds();
        }
        return null;
    }

    /**
     * Called after the given {@param activityName} has been moved to the fullscreen stack. Ensures
     * that the stack matching the {@param windowingMode} and {@param activityType} is focused, and
     * checks the top and/or bottom tasks in the fullscreen stack if
     * {@param expectTopTaskHasActivity} or {@param expectBottomTaskHasActivity} are set
     * respectively.
     */
    private void assertPinnedStackStateOnMoveToFullscreen(ComponentName activityName,
            int windowingMode, int activityType) {
        mWmState.waitForFocusedStack(windowingMode, activityType);
        mWmState.assertFocusedStack("Wrong focused stack", windowingMode, activityType);
        waitAndAssertActivityState(activityName, STATE_STOPPED,
                "Activity should go to STOPPED");
        assertTrue(mWmState.containsActivityInWindowingMode(
                activityName, WINDOWING_MODE_FULLSCREEN));
        assertPinnedStackDoesNotExist();
    }

    /**
     * Asserts that the pinned stack bounds is contained in the display bounds.
     */
    private void assertPinnedStackActivityIsInDisplayBounds(ComponentName activityName) {
        final WindowManagerState.WindowState windowState = getWindowState(activityName);
        final WindowManagerState.DisplayContent display = mWmState.getDisplay(
                windowState.getDisplayId());
        final Rect displayRect = display.getDisplayRect();
        final Rect pinnedStackBounds = getPinnedStackBounds();
        assertTrue(displayRect.contains(pinnedStackBounds));
    }

    /**
     * Asserts that the pinned stack exists.
     */
    private void assertPinnedStackExists() {
        mWmState.assertContainsStack("Must contain pinned stack.", WINDOWING_MODE_PINNED,
                ACTIVITY_TYPE_STANDARD);
    }

    /**
     * Asserts that the pinned stack does not exist.
     */
    private void assertPinnedStackDoesNotExist() {
        mWmState.assertDoesNotContainStack("Must not contain pinned stack.",
                WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD);
    }

    /**
     * Asserts that the pinned stack is the front stack.
     */
    private void assertPinnedStackIsOnTop() {
        mWmState.assertFrontStack("Pinned stack must always be on top.",
                WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD);
    }

    /**
     * Asserts that the activity received exactly one of each of the callbacks when entering and
     * exiting picture-in-picture.
     */
    private void assertValidPictureInPictureCallbackOrder(ComponentName activityName) {
        final ActivityLifecycleCounts lifecycles = new ActivityLifecycleCounts(activityName);

        assertEquals(getActivityName(activityName) + " onConfigurationChanged()",
                1, lifecycles.getCount(ActivityCallback.ON_CONFIGURATION_CHANGED));
        assertEquals(getActivityName(activityName) + " onPictureInPictureModeChanged()",
                1, lifecycles.getCount(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED));
        assertEquals(getActivityName(activityName) + " onMultiWindowModeChanged",
                1, lifecycles.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED));
        final int lastPipIndex = lifecycles
                .getLastIndex(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED);
        final int lastMwIndex = lifecycles
                .getLastIndex(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED);
        final int lastConfigIndex = lifecycles
                .getLastIndex(ActivityCallback.ON_CONFIGURATION_CHANGED);
        assertThat("onPictureInPictureModeChanged should be before onMultiWindowModeChanged",
                lastPipIndex, lessThan(lastMwIndex));
        assertThat("onMultiWindowModeChanged should be before onConfigurationChanged",
                lastMwIndex, lessThan(lastConfigIndex));
    }

    /**
     * Waits until the given activity has entered picture-in-picture mode (allowing for the
     * subsequent animation to start).
     */
    private void waitForEnterPip(ComponentName activityName) {
        mWmState.waitForWithAmState(wmState -> {
            ActivityTask task = wmState.getTaskByActivity(activityName);
            return task != null && task.getWindowingMode() == WINDOWING_MODE_PINNED;
        }, "checking task windowing mode");
    }

    /**
     * Waits until the picture-in-picture animation has finished.
     */
    private void waitForEnterPipAnimationComplete(ComponentName activityName) {
        waitForEnterPip(activityName);
        mWmState.waitForWithAmState(wmState -> {
            ActivityTask task = wmState.getTaskByActivity(activityName);
            if (task == null) {
                return false;
            }
            WindowManagerState.Activity activity = task.getActivity(activityName);
            return activity.getWindowingMode() == WINDOWING_MODE_PINNED
                    && activity.getState().equals(STATE_PAUSED);
        }, "checking activity windowing mode");
    }

    /**
     * Waits until the pinned stack has been removed.
     */
    private void waitForPinnedStackRemoved() {
        mWmState.waitFor((amState) ->
                !amState.containsStack(WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD),
                "pinned stack to be removed");
    }

    /**
     * Waits until the picture-in-picture animation to fullscreen has finished.
     */
    private void waitForExitPipToFullscreen(ComponentName activityName) {
        mWmState.waitForWithAmState(wmState -> {
            final ActivityTask task = wmState.getTaskByActivity(activityName);
            if (task == null) {
                return false;
            }
            final WindowManagerState.Activity activity = task.getActivity(activityName);
            return activity.getWindowingMode() != WINDOWING_MODE_PINNED;
        }, "checking activity windowing mode");
        mWmState.waitForWithAmState(wmState -> {
            final ActivityTask task = wmState.getTaskByActivity(activityName);
            return task != null && task.getWindowingMode() != WINDOWING_MODE_PINNED;
        }, "checking task windowing mode");
    }

    /**
     * Waits until the expected picture-in-picture callbacks have been made.
     */
    private void waitForValidPictureInPictureCallbacks(ComponentName activityName) {
        mWmState.waitFor((amState) -> {
            final ActivityLifecycleCounts lifecycles = new ActivityLifecycleCounts(activityName);
            return lifecycles.getCount(ActivityCallback.ON_CONFIGURATION_CHANGED) == 1
                    && lifecycles.getCount(ActivityCallback.ON_PICTURE_IN_PICTURE_MODE_CHANGED) == 1
                    && lifecycles.getCount(ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED) == 1;
        }, "picture-in-picture activity callbacks...");
    }

    private void waitForValidAspectRatio(int num, int denom) {
        // Hacky, but we need to wait for the auto-enter picture-in-picture animation to complete
        // and before we can check the pinned stack bounds
        mWmState.waitForWithAmState((state) -> {
            Rect bounds = state.getStandardStackByWindowingMode(WINDOWING_MODE_PINNED).getBounds();
            return floatEquals((float) bounds.width() / bounds.height(), (float) num / denom);
        }, "valid aspect ratio");
    }

    /**
     * @return the window state for the given {@param activityName}'s window.
     */
    private WindowManagerState.WindowState getWindowState(ComponentName activityName) {
        String windowName = getWindowName(activityName);
        mWmState.computeState(activityName);
        final List<WindowManagerState.WindowState> tempWindowList =
                mWmState.getMatchingVisibleWindowState(windowName);
        return tempWindowList.get(0);
    }

    /**
     * @return the current pinned stack.
     */
    private ActivityTask getPinnedStack() {
        return mWmState.getStandardStackByWindowingMode(WINDOWING_MODE_PINNED);
    }

    /**
     * @return the current pinned stack bounds.
     */
    private Rect getPinnedStackBounds() {
        return getPinnedStack().getBounds();
    }

    /**
     * Compares two floats with a common epsilon.
     */
    private void assertFloatEquals(float actual, float expected) {
        if (!floatEquals(actual, expected)) {
            fail(expected + " not equal to " + actual);
        }
    }

    private boolean floatEquals(float a, float b) {
        return Math.abs(a - b) < FLOAT_COMPARE_EPSILON;
    }

    /**
     * Triggers a tap over the pinned stack bounds to trigger the PIP to close.
     */
    private void tapToFinishPip() {
        Rect pinnedStackBounds = getPinnedStackBounds();
        int tapX = pinnedStackBounds.left + pinnedStackBounds.width() - 100;
        int tapY = pinnedStackBounds.top + pinnedStackBounds.height() - 100;
        tapOnDisplaySync(tapX, tapY, DEFAULT_DISPLAY);
    }

    /**
     * Launches the given {@param activityName} into the {@param taskId} as a task overlay.
     */
    private void launchPinnedActivityAsTaskOverlay(ComponentName activityName, int taskId) {
        executeShellCommand(getAmStartCmd(activityName) + " --task " + taskId + " --task-overlay");

        mWmState.waitForValidState(new WaitForValidActivityState.Builder(activityName)
                .setWindowingMode(WINDOWING_MODE_PINNED)
                .setActivityType(ACTIVITY_TYPE_STANDARD)
                .build());
    }

    private static class AppOpsSession implements AutoCloseable {

        private final String mPackageName;

        AppOpsSession(ComponentName activityName) {
            mPackageName = activityName.getPackageName();
        }

        /**
         * Sets an app-ops op for a given package to a given mode.
         */
        void setOpToMode(String op, int mode) {
            try {
                AppOpsUtils.setOpMode(mPackageName, op, mode);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        @Override
        public void close() {
            try {
                AppOpsUtils.reset(mPackageName);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * TODO: Improve tests check to actually check that apps are not interactive instead of checking
     *       if the stack is focused.
     */
    private void pinnedStackTester(String startActivityCmd, ComponentName startActivity,
            ComponentName topActivityName, boolean moveTopToPinnedStack, boolean isFocusable) {
        executeShellCommand(startActivityCmd);
        mWmState.waitForValidState(startActivity);

        if (moveTopToPinnedStack) {
            final int stackId = mWmState.getStackIdByActivity(topActivityName);

            assertNotEquals(stackId, INVALID_STACK_ID);
            moveTopActivityToPinnedStack(stackId);
        }

        mWmState.waitForValidState(new WaitForValidActivityState.Builder(topActivityName)
                .setWindowingMode(WINDOWING_MODE_PINNED)
                .setActivityType(ACTIVITY_TYPE_STANDARD)
                .build());
        mWmState.computeState();

        if (supportsPip()) {
            final String windowName = getWindowName(topActivityName);
            assertPinnedStackExists();
            mWmState.assertFrontStack("Pinned stack must be the front stack.",
                    WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD);
            mWmState.assertVisibility(topActivityName, true);

            if (isFocusable) {
                mWmState.assertFocusedStack("Pinned stack must be the focused stack.",
                        WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD);
                mWmState.assertFocusedActivity(
                        "Pinned activity must be focused activity.", topActivityName);
                mWmState.assertFocusedWindow(
                        "Pinned window must be focused window.", windowName);
                // Not checking for resumed state here because PiP overlay can be launched on top
                // in different task by SystemUI.
            } else {
                // Don't assert that the stack is not focused as a focusable PiP overlay can be
                // launched on top as a task overlay by SystemUI.
                mWmState.assertNotFocusedActivity(
                        "Pinned activity can't be the focused activity.", topActivityName);
                mWmState.assertNotResumedActivity(
                        "Pinned activity can't be the resumed activity.", topActivityName);
                mWmState.assertNotFocusedWindow(
                        "Pinned window can't be focused window.", windowName);
            }
        } else {
            mWmState.assertDoesNotContainStack("Must not contain pinned stack.",
                    WINDOWING_MODE_PINNED, ACTIVITY_TYPE_STANDARD);
        }
    }
}
