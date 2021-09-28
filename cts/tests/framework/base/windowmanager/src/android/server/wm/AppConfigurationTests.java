/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.server.wm;

import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.WINDOWING_MODE_FREEFORM;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN_OR_SPLIT_SCREEN_SECONDARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;
import static android.content.res.Configuration.ORIENTATION_PORTRAIT;
import static android.server.wm.ComponentNameUtils.getWindowName;
import static android.server.wm.StateLogger.logE;
import static android.server.wm.WindowManagerState.STATE_RESUMED;
import static android.server.wm.WindowManagerState.dpToPx;
import static android.server.wm.app.Components.BROADCAST_RECEIVER_ACTIVITY;
import static android.server.wm.app.Components.DIALOG_WHEN_LARGE_ACTIVITY;
import static android.server.wm.app.Components.LANDSCAPE_ORIENTATION_ACTIVITY;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.NIGHT_MODE_ACTIVITY;
import static android.server.wm.app.Components.PORTRAIT_ORIENTATION_ACTIVITY;
import static android.server.wm.app.Components.RESIZEABLE_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_APP_CONFIG_INFO;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_CONFIG_INFO_IN_ON_CREATE;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_DISPLAY_REAL_SIZE;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_SYSTEM_RESOURCES_CONFIG_INFO;
import static android.server.wm.translucentapp26.Components.SDK26_TRANSLUCENT_LANDSCAPE_ACTIVITY;
import static android.view.Surface.ROTATION_0;
import static android.view.Surface.ROTATION_180;
import static android.view.Surface.ROTATION_270;
import static android.view.Surface.ROTATION_90;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.server.wm.CommandSession.ActivitySession;
import android.server.wm.CommandSession.ConfigInfo;
import android.server.wm.CommandSession.SizeInfo;
import android.server.wm.TestJournalProvider.TestJournalContainer;
import android.util.DisplayMetrics;
import android.view.Display;

import org.junit.Test;

import java.util.List;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:AppConfigurationTests
 */
@Presubmit
public class AppConfigurationTests extends MultiDisplayTestBase {

    private static final int SMALL_WIDTH_DP = 426;
    private static final int SMALL_HEIGHT_DP = 320;

    /**
     * Tests that the WindowManager#getDefaultDisplay() and the Configuration of the Activity
     * has an updated size when the Activity is resized from fullscreen to docked state.
     *
     * The Activity handles configuration changes, so it will not be restarted between resizes.
     * On Configuration changes, the Activity logs the Display size and Configuration width
     * and heights. The values reported in fullscreen should be larger than those reported in
     * docked state.
     */
    @Test
    public void testConfigurationUpdatesWhenResizedFromFullscreen() {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        separateTestJournal();
        launchActivity(RESIZEABLE_ACTIVITY, WINDOWING_MODE_FULLSCREEN_OR_SPLIT_SCREEN_SECONDARY);
        final SizeInfo fullscreenSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        separateTestJournal();
        setActivityTaskWindowingMode(RESIZEABLE_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        final SizeInfo dockedSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        assertSizesAreSane(fullscreenSizes, dockedSizes);
    }

    /**
     * Same as {@link #testConfigurationUpdatesWhenResizedFromFullscreen()} but resizing
     * from docked state to fullscreen (reverse).
     */
    @Test
    public void testConfigurationUpdatesWhenResizedFromDockedStack() {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        separateTestJournal();
        launchActivity(RESIZEABLE_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        final SizeInfo dockedSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        separateTestJournal();
        setActivityTaskWindowingMode(RESIZEABLE_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        final SizeInfo fullscreenSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        assertSizesAreSane(fullscreenSizes, dockedSizes);
    }

    /**
     * Tests whether the Display sizes change when rotating the device.
     */
    @Test
    public void testConfigurationUpdatesWhenRotatingWhileFullscreen() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);

        separateTestJournal();
        launchActivity(RESIZEABLE_ACTIVITY, WINDOWING_MODE_FULLSCREEN_OR_SPLIT_SCREEN_SECONDARY);
        final SizeInfo initialSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        rotateAndCheckSizes(rotationSession, initialSizes);
    }

    /**
     * Same as {@link #testConfigurationUpdatesWhenRotatingWhileFullscreen()} but when the Activity
     * is in the docked stack.
     */
    @Test
    public void testConfigurationUpdatesWhenRotatingWhileDocked() {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);

        separateTestJournal();
        // Launch our own activity to side in case Recents (or other activity to side) doesn't
        // support rotation.
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(TEST_ACTIVITY));
        // Launch target activity in docked stack.
        getLaunchActivityBuilder().setTargetActivity(RESIZEABLE_ACTIVITY).execute();
        final SizeInfo initialSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        rotateAndCheckSizes(rotationSession, initialSizes);
    }

    /**
     * Same as {@link #testConfigurationUpdatesWhenRotatingWhileDocked()} but when the Activity
     * is launched to side from docked stack.
     */
    @Test
    public void testConfigurationUpdatesWhenRotatingToSideFromDocked() {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);

        separateTestJournal();
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(RESIZEABLE_ACTIVITY));
        final SizeInfo initialSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);

        rotateAndCheckSizes(rotationSession, initialSizes);
    }

    private void rotateAndCheckSizes(RotationSession rotationSession, SizeInfo prevSizes) {
        final WindowManagerState.ActivityTask task =
                mWmState.getTaskByActivity(RESIZEABLE_ACTIVITY);
        final int displayId = mWmState.getRootTask(task.mRootTaskId).mDisplayId;

        assumeTrue(supportsLockedUserRotation(rotationSession, displayId));

        final int[] rotations = { ROTATION_270, ROTATION_180, ROTATION_90, ROTATION_0 };
        for (final int rotation : rotations) {
            separateTestJournal();
            rotationSession.set(rotation);
            final int newDeviceRotation = getDeviceRotation(displayId);
            if (newDeviceRotation == INVALID_DEVICE_ROTATION) {
                logE("Got an invalid device rotation value. "
                        + "Continuing the test despite of that, but it is likely to fail.");
            }

            final SizeInfo rotatedSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);
            assertSizesRotate(prevSizes, rotatedSizes,
                    // Skip orientation checks if we are not in fullscreen mode.
                    task.getWindowingMode() != WINDOWING_MODE_FULLSCREEN);
            prevSizes = rotatedSizes;
        }
    }

    /**
     * Tests when activity moved from fullscreen stack to docked and back. Activity will be
     * relaunched twice and it should have same config as initial one.
     */
    @Test
    public void testSameConfigurationFullSplitFullRelaunch() {
        moveActivityFullSplitFull(TEST_ACTIVITY);
    }

    /**
     * Same as {@link #testSameConfigurationFullSplitFullRelaunch} but without relaunch.
     */
    @Test
    public void testSameConfigurationFullSplitFullNoRelaunch() {
        moveActivityFullSplitFull(RESIZEABLE_ACTIVITY);
    }

    /**
     * Launches activity in fullscreen stack, moves to docked stack and back to fullscreen stack.
     * Last operation is done in a way which simulates split-screen divider movement maximizing
     * docked stack size and then moving task to fullscreen stack - the same way it is done when
     * user long-presses overview/recents button to exit split-screen.
     * Asserts that initial and final reported sizes in fullscreen stack are the same.
     */
    private void moveActivityFullSplitFull(ComponentName activityName) {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        // Launch to fullscreen stack and record size.
        separateTestJournal();
        launchActivity(activityName, WINDOWING_MODE_FULLSCREEN);
        final SizeInfo initialFullscreenSizes = getActivityDisplaySize(activityName);
        final Rect displayRect = getDisplayRect(activityName);

        // Move to docked stack.
        separateTestJournal();
        setActivityTaskWindowingMode(activityName, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        final SizeInfo dockedSizes = getActivityDisplaySize(activityName);
        assertSizesAreSane(initialFullscreenSizes, dockedSizes);

        // Resize docked stack to fullscreen size. This will trigger activity relaunch with
        // non-empty override configuration corresponding to fullscreen size.
        separateTestJournal();
        final int width = displayRect.width();
        final int height = displayRect.height();
        resizeDockedStack(width /* stackWidth */, height /* stackHeight */,
                width /* taskWidth */, height /* taskHeight */);

        // Move activity back to fullscreen stack.
        setActivityTaskWindowingMode(activityName, WINDOWING_MODE_FULLSCREEN);
        final SizeInfo finalFullscreenSizes = getActivityDisplaySize(activityName);

        // After activity configuration was changed twice it must report same size as original one.
        assertSizesAreSame(initialFullscreenSizes, finalFullscreenSizes);
    }

    /**
     * Tests when activity moved from docked stack to fullscreen and back. Activity will be
     * relaunched twice and it should have same config as initial one.
     */
    @Test
    public void testSameConfigurationSplitFullSplitRelaunch() {
        moveActivitySplitFullSplit(TEST_ACTIVITY);
    }

    /**
     * Same as {@link #testSameConfigurationSplitFullSplitRelaunch} but without relaunch.
     */
    @Test
    public void testSameConfigurationSplitFullSplitNoRelaunch() {
        moveActivitySplitFullSplit(RESIZEABLE_ACTIVITY);
    }

    /**
     * Tests that an activity with the DialogWhenLarge theme can transform properly when in split
     * screen.
     */
    @Test
    public void testDialogWhenLargeSplitSmall() {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        launchActivity(DIALOG_WHEN_LARGE_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        final WindowManagerState.ActivityTask stack = mWmState
                .getStandardStackByWindowingMode(WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        final WindowManagerState.DisplayContent display =
                mWmState.getDisplay(stack.mDisplayId);
        final int density = display.getDpi();
        final int smallWidthPx = dpToPx(SMALL_WIDTH_DP, density);
        final int smallHeightPx = dpToPx(SMALL_HEIGHT_DP, density);

        resizeDockedStack(0, 0, smallWidthPx, smallHeightPx);
        mWmState.waitForValidState(
                new WaitForValidActivityState.Builder(DIALOG_WHEN_LARGE_ACTIVITY)
                        .setWindowingMode(WINDOWING_MODE_SPLIT_SCREEN_PRIMARY)
                        .setActivityType(ACTIVITY_TYPE_STANDARD)
                        .build());
    }

    /**
     * Test that device handles consequent requested orientations and displays the activities.
     */
    @Test
    public void testFullscreenAppOrientationRequests() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        separateTestJournal();
        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        mWmState.assertVisibility(PORTRAIT_ORIENTATION_ACTIVITY, true /* visible */);
        SizeInfo reportedSizes = getLastReportedSizesForActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        assertEquals("portrait activity should be in portrait",
                ORIENTATION_PORTRAIT, reportedSizes.orientation);
        separateTestJournal();

        launchActivity(LANDSCAPE_ORIENTATION_ACTIVITY);
        mWmState.assertVisibility(LANDSCAPE_ORIENTATION_ACTIVITY, true /* visible */);
        reportedSizes = getLastReportedSizesForActivity(LANDSCAPE_ORIENTATION_ACTIVITY);
        assertEquals("landscape activity should be in landscape",
                ORIENTATION_LANDSCAPE, reportedSizes.orientation);
        separateTestJournal();

        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        mWmState.assertVisibility(PORTRAIT_ORIENTATION_ACTIVITY, true /* visible */);
        reportedSizes = getLastReportedSizesForActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        assertEquals("portrait activity should be in portrait",
                ORIENTATION_PORTRAIT, reportedSizes.orientation);
    }

    @Test
    public void testNonfullscreenAppOrientationRequests() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        separateTestJournal();
        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        final SizeInfo initialReportedSizes =
                getLastReportedSizesForActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        assertEquals("portrait activity should be in portrait",
                ORIENTATION_PORTRAIT, initialReportedSizes.orientation);
        separateTestJournal();

        launchActivity(SDK26_TRANSLUCENT_LANDSCAPE_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        assertEquals("Legacy non-fullscreen activity requested landscape orientation",
                SCREEN_ORIENTATION_LANDSCAPE, mWmState.getLastOrientation());

        // TODO(b/36897968): uncomment once we can suppress unsupported configurations
        // final ReportedSizes updatedReportedSizes =
        //      getLastReportedSizesForActivity(PORTRAIT_ACTIVITY_NAME, logSeparator);
        // assertEquals("portrait activity should not have moved from portrait",
        //         1 /* portrait */, updatedReportedSizes.orientation);
    }

    /**
     * Test that device handles consequent requested orientations and will not report a config
     * change to an invisible activity.
     */
    @Test
    public void testAppOrientationRequestConfigChanges() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        separateTestJournal();
        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        mWmState.assertVisibility(PORTRAIT_ORIENTATION_ACTIVITY, true /* visible */);

        assertLifecycleCounts(PORTRAIT_ORIENTATION_ACTIVITY,
                1 /* create */, 1 /* start */, 1 /* resume */,
                0 /* pause */, 0 /* stop */, 0 /* destroy */, 0 /* config */);

        launchActivity(LANDSCAPE_ORIENTATION_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        mWmState.assertVisibility(LANDSCAPE_ORIENTATION_ACTIVITY, true /* visible */);

        assertLifecycleCounts(PORTRAIT_ORIENTATION_ACTIVITY,
                1 /* create */, 1 /* start */, 1 /* resume */,
                1 /* pause */, 1 /* stop */, 0 /* destroy */, 0 /* config */);
        assertLifecycleCounts(LANDSCAPE_ORIENTATION_ACTIVITY,
                1 /* create */, 1 /* start */, 1 /* resume */,
                0 /* pause */, 0 /* stop */, 0 /* destroy */, 0 /* config */);

        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        mWmState.assertVisibility(PORTRAIT_ORIENTATION_ACTIVITY, true /* visible */);

        assertLifecycleCounts(PORTRAIT_ORIENTATION_ACTIVITY,
                2 /* create */, 2 /* start */, 2 /* resume */,
                1 /* pause */, 1 /* stop */, 0 /* destroy */, 0 /* config */);
        assertLifecycleCounts(LANDSCAPE_ORIENTATION_ACTIVITY,
                1 /* create */, 1 /* start */, 1 /* resume */,
                1 /* pause */, 1 /* stop */, 0 /* destroy */, 0 /* config */);
    }

    /**
     * Test that device orientation is restored when an activity that requests it is no longer
     * visible.
     *
     * TODO(b/139936670, b/112688380): This test case fails on some vendor devices which has
     * rotation sensing optimization. So this is listed in cts-known-failures.xml.
     */
    @Test
    public void testAppOrientationRequestConfigClears() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        separateTestJournal();
        launchActivity(TEST_ACTIVITY);
        mWmState.assertVisibility(TEST_ACTIVITY, true /* visible */);
        final SizeInfo initialReportedSizes = getLastReportedSizesForActivity(TEST_ACTIVITY);
        final int initialOrientation = initialReportedSizes.orientation;

        // Launch an activity that requests different orientation and check that it will be applied
        final boolean launchingPortrait;
        if (initialOrientation == ORIENTATION_LANDSCAPE) {
            launchingPortrait = true;
        } else if (initialOrientation == ORIENTATION_PORTRAIT) {
            launchingPortrait = false;
        } else {
            fail("Unexpected orientation value: " + initialOrientation);
            return;
        }
        final ComponentName differentOrientationActivity = launchingPortrait
                ? PORTRAIT_ORIENTATION_ACTIVITY : LANDSCAPE_ORIENTATION_ACTIVITY;
        separateTestJournal();
        launchActivity(differentOrientationActivity);
        mWmState.assertVisibility(differentOrientationActivity, true /* visible */);
        final SizeInfo rotatedReportedSizes =
                getLastReportedSizesForActivity(differentOrientationActivity);
        assertEquals("Applied orientation must correspond to activity request",
                launchingPortrait ? 1 : 2, rotatedReportedSizes.orientation);

        // Launch another activity on top and check that its orientation is not affected by previous
        // activity.
        separateTestJournal();
        launchActivity(RESIZEABLE_ACTIVITY);
        mWmState.assertVisibility(RESIZEABLE_ACTIVITY, true /* visible */);
        final SizeInfo finalReportedSizes = getLastReportedSizesForActivity(RESIZEABLE_ACTIVITY);
        assertEquals("Applied orientation must not be influenced by previously visible activity",
                initialOrientation, finalReportedSizes.orientation);
    }

    @Test
    public void testRotatedInfoWithFixedRotationTransform() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Start a portrait activity first to ensure that the orientation will change.
        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        mWmState.waitForLastOrientation(SCREEN_ORIENTATION_PORTRAIT);

        getLaunchActivityBuilder()
                .setUseInstrumentation()
                .setTargetActivity(LANDSCAPE_ORIENTATION_ACTIVITY)
                // Request the info from onCreate because at that moment the real display hasn't
                // rotated but the activity is rotated.
                .setIntentExtra(bundle -> bundle.putBoolean(EXTRA_CONFIG_INFO_IN_ON_CREATE, true))
                .execute();
        mWmState.waitForLastOrientation(SCREEN_ORIENTATION_LANDSCAPE);

        final SizeInfo reportedSizes = getActivityDisplaySize(LANDSCAPE_ORIENTATION_ACTIVITY);
        final Bundle extras = TestJournalContainer.get(LANDSCAPE_ORIENTATION_ACTIVITY).extras;
        final ConfigInfo appConfigInfo = extras.getParcelable(EXTRA_APP_CONFIG_INFO);
        final Point onCreateRealDisplaySize = extras.getParcelable(EXTRA_DISPLAY_REAL_SIZE);
        final ConfigInfo onCreateConfigInfo = extras.getParcelable(EXTRA_CONFIG_INFO_IN_ON_CREATE);
        final SizeInfo onCreateSize = onCreateConfigInfo.sizeInfo;
        final ConfigInfo globalConfigInfo =
                extras.getParcelable(EXTRA_SYSTEM_RESOURCES_CONFIG_INFO);
        final SizeInfo globalSizeInfo = globalConfigInfo.sizeInfo;

        assertEquals("The last reported size should be the same as the one from onCreate",
                reportedSizes, onCreateConfigInfo.sizeInfo);

        final Display display = mDm.getDisplay(Display.DEFAULT_DISPLAY);
        final Point expectedRealDisplaySize = new Point();
        display.getRealSize(expectedRealDisplaySize);

        final int expectedRotation = display.getRotation();
        assertEquals("The activity should get the final display rotation in onCreate",
                expectedRotation, onCreateConfigInfo.rotation);
        assertEquals("The application should get the final display rotation in onCreate",
                expectedRotation, appConfigInfo.rotation);
        assertEquals("The orientation of application must be landscape",
                ORIENTATION_LANDSCAPE, appConfigInfo.sizeInfo.orientation);
        assertEquals("The orientation of system resources must be landscape",
                ORIENTATION_LANDSCAPE, globalSizeInfo.orientation);
        assertEquals("The activity should get the final display size in onCreate",
                expectedRealDisplaySize, onCreateRealDisplaySize);

        final boolean isLandscape = expectedRealDisplaySize.x > expectedRealDisplaySize.y;
        assertEquals("The app size of activity should have the same orientation", isLandscape,
                onCreateSize.displayWidth > onCreateSize.displayHeight);
        assertEquals("The application should get the same orientation", isLandscape,
                appConfigInfo.sizeInfo.displayWidth > appConfigInfo.sizeInfo.displayHeight);
        assertEquals("The app display metrics must be landscape", isLandscape,
                appConfigInfo.sizeInfo.metricsWidth > appConfigInfo.sizeInfo.metricsHeight);

        final DisplayMetrics globalMetrics = Resources.getSystem().getDisplayMetrics();
        assertEquals("The display metrics of system resources must be landscape",
                new Point(globalMetrics.widthPixels, globalMetrics.heightPixels),
                new Point(globalSizeInfo.metricsWidth, globalSizeInfo.metricsHeight));
    }

    @Test
    public void testNonFullscreenActivityPermitted() throws Exception {
        if(!supportsRotation()) {
            //cannot physically rotate the screen on automotive device, skip
            return;
        }

        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);

        launchActivity(SDK26_TRANSLUCENT_LANDSCAPE_ACTIVITY);
        mWmState.assertResumedActivity(
                "target SDK <= 26 non-fullscreen activity should be allowed to launch",
                SDK26_TRANSLUCENT_LANDSCAPE_ACTIVITY);
        assertEquals("non-fullscreen activity requested landscape orientation",
                SCREEN_ORIENTATION_LANDSCAPE, mWmState.getLastOrientation());
    }

    /**
     * Test that device handles moving between two tasks with different orientations.
     */
    @Test
    public void testTaskCloseRestoreFixedOrientation() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Start landscape activity.
        launchActivity(LANDSCAPE_ORIENTATION_ACTIVITY);
        mWmState.assertVisibility(LANDSCAPE_ORIENTATION_ACTIVITY, true /* visible */);
        assertEquals("Fullscreen app requested landscape orientation",
                SCREEN_ORIENTATION_LANDSCAPE, mWmState.getLastOrientation());

        // Start another activity in a different task.
        launchActivityInNewTask(BROADCAST_RECEIVER_ACTIVITY);

        // Request portrait
        mBroadcastActionTrigger.requestOrientation(SCREEN_ORIENTATION_PORTRAIT);
        mWmState.waitForLastOrientation(SCREEN_ORIENTATION_PORTRAIT);
        waitForBroadcastActivityReady(ORIENTATION_PORTRAIT);

        // Finish activity
        mBroadcastActionTrigger.finishBroadcastReceiverActivity();

        // Verify that activity brought to front is in originally requested orientation.
        mWmState.computeState(LANDSCAPE_ORIENTATION_ACTIVITY);
        mWmState.waitAndAssertLastOrientation("Should return to app in landscape orientation",
                SCREEN_ORIENTATION_LANDSCAPE);
    }

    /**
     * Test that device handles moving between two tasks with different orientations.
     *
     * TODO(b/139936670, b/112688380): This test case fails on some vendor devices which has
     * rotation sensing optimization. So this is listed in cts-known-failures.xml.
     */
    @Test
    public void testTaskCloseRestoreFreeOrientation() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Start landscape activity.
        launchActivity(RESIZEABLE_ACTIVITY);
        mWmState.assertVisibility(RESIZEABLE_ACTIVITY, true /* visible */);
        final int initialServerOrientation = mWmState.getLastOrientation();

        // Verify fixed-landscape
        separateTestJournal();
        launchActivityInNewTask(BROADCAST_RECEIVER_ACTIVITY);
        mBroadcastActionTrigger.requestOrientation(SCREEN_ORIENTATION_LANDSCAPE);
        mWmState.waitForLastOrientation(SCREEN_ORIENTATION_LANDSCAPE);
        waitForBroadcastActivityReady(ORIENTATION_LANDSCAPE);
        mBroadcastActionTrigger.finishBroadcastReceiverActivity();

        // Verify that activity brought to front is in originally requested orientation.
        mWmState.waitForActivityState(RESIZEABLE_ACTIVITY, STATE_RESUMED);
        SizeInfo reportedSizes = getLastReportedSizesForActivity(RESIZEABLE_ACTIVITY);
        assertNull("Should come back in original orientation", reportedSizes);
        mWmState.waitAndAssertLastOrientation("Should come back in original server orientation",
                initialServerOrientation);
        assertRelaunchOrConfigChanged(RESIZEABLE_ACTIVITY, 0 /* numRelaunch */,
                0 /* numConfigChange */);

        // Verify fixed-portrait
        separateTestJournal();
        launchActivityInNewTask(BROADCAST_RECEIVER_ACTIVITY);
        mBroadcastActionTrigger.requestOrientation(SCREEN_ORIENTATION_PORTRAIT);
        mWmState.waitForLastOrientation(SCREEN_ORIENTATION_PORTRAIT);
        waitForBroadcastActivityReady(ORIENTATION_PORTRAIT);
        mBroadcastActionTrigger.finishBroadcastReceiverActivity();

        // Verify that activity brought to front is in originally requested orientation.
        mWmState.waitForActivityState(RESIZEABLE_ACTIVITY, STATE_RESUMED);
        reportedSizes = getLastReportedSizesForActivity(RESIZEABLE_ACTIVITY);
        assertNull("Should come back in original orientation", reportedSizes);
        assertEquals("Should come back in original server orientation",
                initialServerOrientation, mWmState.getLastOrientation());
        assertRelaunchOrConfigChanged(RESIZEABLE_ACTIVITY, 0 /* numRelaunch */,
                0 /* numConfigChange */);
    }

    /**
     * Test that activity orientation will change when device is rotated.
     * Also verify that occluded activity will not get config changes.
     */
    @Test
    public void testAppOrientationWhenRotating() throws Exception {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Start resizeable activity that handles configuration changes.
        separateTestJournal();
        launchActivity(TEST_ACTIVITY);
        launchActivity(RESIZEABLE_ACTIVITY);
        mWmState.assertVisibility(RESIZEABLE_ACTIVITY, true /* visible */);

        final int displayId = mWmState.getDisplayByActivity(RESIZEABLE_ACTIVITY);

        // Rotate the activity and check that it receives configuration changes with a different
        // orientation each time.
        final RotationSession rotationSession = createManagedRotationSession();
        assumeTrue("Skipping test: no locked user rotation mode support.",
                supportsLockedUserRotation(rotationSession, displayId));

        rotationSession.set(ROTATION_0);
        SizeInfo reportedSizes = getLastReportedSizesForActivity(RESIZEABLE_ACTIVITY);
        int prevOrientation = reportedSizes.orientation;

        final int[] rotations = { ROTATION_270, ROTATION_180, ROTATION_90, ROTATION_0 };
        for (final int rotation : rotations) {
            separateTestJournal();
            rotationSession.set(rotation);

            // Verify lifecycle count and orientation changes.
            assertRelaunchOrConfigChanged(RESIZEABLE_ACTIVITY, 0 /* numRelaunch */,
                    1 /* numConfigChange */);
            reportedSizes = getLastReportedSizesForActivity(RESIZEABLE_ACTIVITY);
            assertNotEquals(prevOrientation, reportedSizes.orientation);
            assertRelaunchOrConfigChanged(TEST_ACTIVITY, 0 /* numRelaunch */,
                    0 /* numConfigChange */);

            prevOrientation = reportedSizes.orientation;
        }
    }

    /**
     * Test that the orientation for a simulated display context will not change when the device is
     * rotated.
     */
    @Test
    public void testAppContextDerivedDisplayContextOrientationWhenRotating() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        assumeTrue("Skipping test: no multi-display support", supportsMultiDisplay());

        RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);

        TestActivitySession<ConfigChangeHandlingActivity> activitySession
                = createManagedTestActivitySession();
        activitySession.launchTestActivityOnDisplaySync(ConfigChangeHandlingActivity.class,
                Display.DEFAULT_DISPLAY);
        final ConfigChangeHandlingActivity activity = activitySession.getActivity();

        VirtualDisplaySession virtualDisplaySession = createManagedVirtualDisplaySession();
        WindowManagerState.DisplayContent displayContent = virtualDisplaySession
                .setSimulateDisplay(true)
                .setSimulationDisplaySize(100 /* width */, 200 /* height */)
                .createDisplay();

        DisplayManager dm = activity.getSystemService(DisplayManager.class);
        Display simulatedDisplay = dm.getDisplay(displayContent.mId);
        Context simulatedDisplayContext = activity.getApplicationContext()
                .createDisplayContext(simulatedDisplay);
        assertEquals(ORIENTATION_PORTRAIT,
                simulatedDisplayContext.getResources().getConfiguration().orientation);

        separateTestJournal();

        final int[] rotations = {ROTATION_270, ROTATION_180, ROTATION_90, ROTATION_0};
        for (final int rotation : rotations) {
            rotationSession.set(rotation);

            assertRelaunchOrConfigChanged(activity.getComponentName(), 0 /* numRelaunch */,
                    1 /* numConfigChange */);
            separateTestJournal();

            assertEquals("Display context orientation must not be changed", ORIENTATION_PORTRAIT,
                    simulatedDisplayContext.getResources().getConfiguration().orientation);
        }
    }

    /**
     * Test that activity orientation will not change when trying to rotate fixed-orientation
     * activity.
     * Also verify that occluded activity will not get config changes.
     */
    @Test
    public void testFixedOrientationWhenRotating() throws Exception {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        // TODO(b/110533226): Fix test on devices with display cutout
        assumeFalse("Skipping test: display cutout present, can't predict exact lifecycle",
                hasDisplayCutout());

        // Start portrait-fixed activity
        separateTestJournal();
        launchActivity(RESIZEABLE_ACTIVITY);
        launchActivity(PORTRAIT_ORIENTATION_ACTIVITY);
        mWmState.assertVisibility(PORTRAIT_ORIENTATION_ACTIVITY, true /* visible */);

        final int displayId = mWmState
                .getDisplayByActivity(PORTRAIT_ORIENTATION_ACTIVITY);

        // Rotate the activity and check that the orientation doesn't change
        final RotationSession rotationSession = createManagedRotationSession();
        assumeTrue("Skipping test: no user locked rotation support.",
                supportsLockedUserRotation(rotationSession, displayId));

        rotationSession.set(ROTATION_0);

        final int[] rotations = { ROTATION_270, ROTATION_180, ROTATION_90, ROTATION_0 };
        for (final int rotation : rotations) {
            separateTestJournal();
            rotationSession.set(rotation);

            // Verify lifecycle count and orientation changes.
            assertRelaunchOrConfigChanged(PORTRAIT_ORIENTATION_ACTIVITY, 0 /* numRelaunch */,
                    0 /* numConfigChange */);
            final SizeInfo reportedSizes = getLastReportedSizesForActivity(
                    PORTRAIT_ORIENTATION_ACTIVITY);
            assertNull("No new sizes must be reported", reportedSizes);
            assertRelaunchOrConfigChanged(RESIZEABLE_ACTIVITY, 0 /* numRelaunch */,
                    0 /* numConfigChange */);
        }
    }

    /**
     * Test that device handles moving between two tasks with different orientations.
     */
    @Test
    public void testTaskMoveToBackOrientation() {
        assumeTrue("Skipping test: no rotation support", supportsRotation());

        // Start landscape activity.
        launchActivity(LANDSCAPE_ORIENTATION_ACTIVITY);
        mWmState.assertVisibility(LANDSCAPE_ORIENTATION_ACTIVITY, true /* visible */);
        assertEquals("Fullscreen app requested landscape orientation",
                SCREEN_ORIENTATION_LANDSCAPE, mWmState.getLastOrientation());

        // Start another activity in a different task.
        launchActivityInNewTask(BROADCAST_RECEIVER_ACTIVITY);

        // Request portrait
        mBroadcastActionTrigger.requestOrientation(SCREEN_ORIENTATION_PORTRAIT);
        mWmState.waitForLastOrientation(SCREEN_ORIENTATION_PORTRAIT);
        waitForBroadcastActivityReady(ORIENTATION_PORTRAIT);

        // Finish activity
        mBroadcastActionTrigger.moveTopTaskToBack();

        // Verify that activity brought to front is in originally requested orientation.
        mWmState.waitForValidState(LANDSCAPE_ORIENTATION_ACTIVITY);
        mWmState.waitAndAssertLastOrientation("Should return to app in landscape orientation",
                SCREEN_ORIENTATION_LANDSCAPE);
    }

    /**
     * Test that device doesn't change device orientation by app request while in multi-window.
     */
    @Test
    public void testSplitscreenPortraitAppOrientationRequests() throws Exception {
        assumeTrue("Skipping test: no rotation support", supportsRotation());
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        requestOrientationInSplitScreen(createManagedRotationSession(),
                ROTATION_90 /* portrait */, LANDSCAPE_ORIENTATION_ACTIVITY);
    }

    /**
     * Test that device doesn't change device orientation by app request while in multi-window.
     */
    @Test
    public void testSplitscreenLandscapeAppOrientationRequests() throws Exception {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        requestOrientationInSplitScreen(createManagedRotationSession(),
                ROTATION_0 /* landscape */, PORTRAIT_ORIENTATION_ACTIVITY);
    }

    /**
     * Rotate the device and launch specified activity in split-screen, checking if orientation
     * didn't change.
     */
    private void requestOrientationInSplitScreen(RotationSession rotationSession, int orientation,
            ComponentName activity) throws Exception {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        // Set initial orientation.
        rotationSession.set(orientation);

        // Launch activities that request orientations and check that device doesn't rotate.
        launchActivitiesInSplitScreen(
                getLaunchActivityBuilder().setTargetActivity(LAUNCHING_ACTIVITY),
                getLaunchActivityBuilder().setTargetActivity(activity).setMultipleTask(true));

        mWmState.assertVisibility(activity, true /* visible */);
        assertEquals("Split-screen apps shouldn't influence device orientation",
                orientation, mWmState.getRotation());

        getLaunchActivityBuilder().setMultipleTask(true).setTargetActivity(activity).execute();
        mWmState.computeState(activity);
        mWmState.assertVisibility(activity, true /* visible */);
        assertEquals("Split-screen apps shouldn't influence device orientation",
                orientation, mWmState.getRotation());
    }

    /**
     * Launches activity in docked stack, moves to fullscreen stack and back to docked stack.
     * Asserts that initial and final reported sizes in docked stack are the same.
     */
    private void moveActivitySplitFullSplit(ComponentName activityName) {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        // Launch to docked stack and record size.
        separateTestJournal();
        launchActivityInSplitScreenWithRecents(activityName);
        final SizeInfo initialDockedSizes = getActivityDisplaySize(activityName);
        mWmState.computeState(
                new WaitForValidActivityState.Builder(activityName).build());

        // Move to fullscreen stack.
        separateTestJournal();
        setActivityTaskWindowingMode(
                activityName, WINDOWING_MODE_FULLSCREEN);
        // Home task could be on top since it was the top-most task while in split-screen mode
        // (dock task was minimized), start the activity again to ensure the activity is at
        // foreground.
        launchActivity(activityName, WINDOWING_MODE_FULLSCREEN);
        final SizeInfo fullscreenSizes = getActivityDisplaySize(activityName);
        assertSizesAreSane(fullscreenSizes, initialDockedSizes);

        // Move activity back to docked stack.
        separateTestJournal();
        setActivityTaskWindowingMode(activityName, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        final SizeInfo finalDockedSizes = getActivityDisplaySize(activityName);

        // After activity configuration was changed twice it must report same size as original one.
        assertSizesAreSame(initialDockedSizes, finalDockedSizes);
    }

    /**
     * Asserts that after rotation, the aspect ratios of display size, metrics, and configuration
     * have flipped.
     */
    private static void assertSizesRotate(SizeInfo rotationA, SizeInfo rotationB,
            boolean skipOrientationCheck) {
        assertEquals(rotationA.displayWidth, rotationA.metricsWidth);
        assertEquals(rotationA.displayHeight, rotationA.metricsHeight);
        assertEquals(rotationB.displayWidth, rotationB.metricsWidth);
        assertEquals(rotationB.displayHeight, rotationB.metricsHeight);

        if (skipOrientationCheck) {
            // All done if we are not doing orientation check.
            return;
        }
        final boolean beforePortrait = rotationA.displayWidth < rotationA.displayHeight;
        final boolean afterPortrait = rotationB.displayWidth < rotationB.displayHeight;
        assertFalse(beforePortrait == afterPortrait);

        final boolean beforeConfigPortrait = rotationA.widthDp < rotationA.heightDp;
        final boolean afterConfigPortrait = rotationB.widthDp < rotationB.heightDp;
        assertEquals(beforePortrait, beforeConfigPortrait);
        assertEquals(afterPortrait, afterConfigPortrait);

        assertEquals(rotationA.smallestWidthDp, rotationB.smallestWidthDp);
    }

    /**
     * Throws an AssertionError if fullscreenSizes has widths/heights (depending on aspect ratio)
     * that are smaller than the dockedSizes.
     */
    private static void assertSizesAreSane(SizeInfo fullscreenSizes, SizeInfo dockedSizes) {
        final boolean portrait = fullscreenSizes.displayWidth < fullscreenSizes.displayHeight;
        if (portrait) {
            assertThat(dockedSizes.displayHeight, lessThan(fullscreenSizes.displayHeight));
            assertThat(dockedSizes.heightDp, lessThan(fullscreenSizes.heightDp));
            assertThat(dockedSizes.metricsHeight, lessThan(fullscreenSizes.metricsHeight));
        } else {
            assertThat(dockedSizes.displayWidth, lessThan(fullscreenSizes.displayWidth));
            assertThat(dockedSizes.widthDp, lessThan(fullscreenSizes.widthDp));
            assertThat(dockedSizes.metricsWidth, lessThan(fullscreenSizes.metricsWidth));
        }
    }

    /**
     * Throws an AssertionError if sizes are different.
     */
    private static void assertSizesAreSame(SizeInfo firstSize, SizeInfo secondSize) {
        assertEquals(firstSize.widthDp, secondSize.widthDp);
        assertEquals(firstSize.heightDp, secondSize.heightDp);
        assertEquals(firstSize.displayWidth, secondSize.displayWidth);
        assertEquals(firstSize.displayHeight, secondSize.displayHeight);
        assertEquals(firstSize.metricsWidth, secondSize.metricsWidth);
        assertEquals(firstSize.metricsHeight, secondSize.metricsHeight);
        assertEquals(firstSize.smallestWidthDp, secondSize.smallestWidthDp);
    }

    private SizeInfo getActivityDisplaySize(ComponentName activityName) {
        mWmState.computeState(
                new WaitForValidActivityState(activityName));
        final SizeInfo details = getLastReportedSizesForActivity(activityName);
        assertNotNull(details);
        return details;
    }

    private Rect getDisplayRect(ComponentName activityName) {
        final String windowName = getWindowName(activityName);

        mWmState.computeState(activityName);
        mWmState.assertFocusedWindow("Test window must be the front window.", windowName);

        final List<WindowManagerState.WindowState> windowList =
                mWmState.getMatchingVisibleWindowState(windowName);

        assertEquals("Should have exactly one window state for the activity.", 1,
                windowList.size());

        WindowManagerState.WindowState windowState = windowList.get(0);
        assertNotNull("Should have a valid window", windowState);

        WindowManagerState.DisplayContent display = mWmState
                .getDisplay(windowState.getDisplayId());
        assertNotNull("Should be on a display", display);

        return display.getDisplayRect();
    }

    private void waitForBroadcastActivityReady(int orientation) {
        mWmState.waitForActivityOrientation(BROADCAST_RECEIVER_ACTIVITY, orientation);
        mWmState.waitForActivityState(BROADCAST_RECEIVER_ACTIVITY, STATE_RESUMED);
    }

    /**
     * Test launching an activity which requests specific UI mode during creation.
     */
    @Test
    public void testLaunchWithUiModeChange() {
        // Launch activity that changes UI mode and handles this configuration change.
        launchActivity(NIGHT_MODE_ACTIVITY);
        mWmState.waitForActivityState(NIGHT_MODE_ACTIVITY, STATE_RESUMED);

        // Check if activity is launched successfully.
        mWmState.assertVisibility(NIGHT_MODE_ACTIVITY, true /* visible */);
        mWmState.assertFocusedActivity("Launched activity should be focused",
                NIGHT_MODE_ACTIVITY);
        mWmState.assertResumedActivity("Launched activity must be resumed", NIGHT_MODE_ACTIVITY);
    }

    @Test
    public void testAppConfigurationMatchesActivityInMultiWindow() throws Exception {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        final ActivitySession activitySession = createManagedActivityClientSession()
                .startActivity(getLaunchActivityBuilder()
                        .setUseInstrumentation()
                        .setTargetActivity(RESIZEABLE_ACTIVITY)
                        .setWindowingMode(WINDOWING_MODE_SPLIT_SCREEN_PRIMARY));
        SizeInfo dockedActivitySizes = getActivitySizeInfo(activitySession);
        SizeInfo applicationSizes = getAppSizeInfo(activitySession);
        assertSizesAreSame(dockedActivitySizes, applicationSizes);

        // Move the activity to fullscreen and check that the size was updated
        separateTestJournal();
        setActivityTaskWindowingMode(RESIZEABLE_ACTIVITY, WINDOWING_MODE_FULLSCREEN);
        waitForOrFail("Activity and application configuration must match",
                () -> activityAndAppSizesMatch(activitySession));
        final SizeInfo fullscreenSizes = getActivityDisplaySize(RESIZEABLE_ACTIVITY);
        applicationSizes = getAppSizeInfo(activitySession);
        assertSizesAreSane(fullscreenSizes, dockedActivitySizes);
        assertSizesAreSame(fullscreenSizes, applicationSizes);

        // Move the activity to docked size again, check if the sizes were updated
        separateTestJournal();
        setActivityTaskWindowingMode(RESIZEABLE_ACTIVITY, WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        waitForOrFail("Activity and application configuration must match",
                () -> activityAndAppSizesMatch(activitySession));
        dockedActivitySizes = getActivitySizeInfo(activitySession);
        applicationSizes = getAppSizeInfo(activitySession);
        assertSizesAreSane(fullscreenSizes, dockedActivitySizes);
        assertSizesAreSame(dockedActivitySizes, applicationSizes);
    }

    @Test
    public void testAppConfigurationMatchesTopActivityInMultiWindow() throws Exception {
        assumeTrue("Skipping test: no multi-window support", supportsSplitScreenMultiWindow());

        // Launch initial activity in fullscreen and assert sizes
        final ActivitySession fullscreenActivitySession = createManagedActivityClientSession()
                .startActivity(getLaunchActivityBuilder()
                        .setUseInstrumentation()
                        .setTargetActivity(TEST_ACTIVITY)
                        .setWindowingMode(WINDOWING_MODE_FULLSCREEN));
        SizeInfo fullscreenActivitySizes = getActivitySizeInfo(fullscreenActivitySession);
        SizeInfo applicationSizes = getAppSizeInfo(fullscreenActivitySession);
        assertSizesAreSame(fullscreenActivitySizes, applicationSizes);

        // Launch second activity in split-screen and assert that sizes were updated
        separateTestJournal();
        final ActivitySession secondActivitySession = createManagedActivityClientSession()
                .startActivity(getLaunchActivityBuilder()
                        .setUseInstrumentation()
                        .setTargetActivity(RESIZEABLE_ACTIVITY)
                        .setNewTask(true)
                        .setMultipleTask(true)
                        .setWindowingMode(WINDOWING_MODE_SPLIT_SCREEN_PRIMARY));
        waitForOrFail("Activity and application configuration must match",
                () -> activityAndAppSizesMatch(secondActivitySession));
        SizeInfo dockedActivitySizes = getActivitySizeInfo(secondActivitySession);
        applicationSizes = getAppSizeInfo(secondActivitySession);
        assertSizesAreSame(dockedActivitySizes, applicationSizes);
        assertSizesAreSane(fullscreenActivitySizes, dockedActivitySizes);

        // Launch third activity in secondary split-screen and assert that sizes were updated
        separateTestJournal();
        final ActivitySession thirdActivitySession = createManagedActivityClientSession()
                .startActivity(getLaunchActivityBuilder()
                        .setUseInstrumentation()
                        .setTargetActivity(RESIZEABLE_ACTIVITY)
                        .setNewTask(true)
                        .setMultipleTask(true)
                        .setWindowingMode(WINDOWING_MODE_SPLIT_SCREEN_PRIMARY));
        waitForOrFail("Activity and application configuration must match",
                () -> activityAndAppSizesMatch(thirdActivitySession));
        SizeInfo secondarySplitActivitySizes = getActivitySizeInfo(thirdActivitySession);
        applicationSizes = getAppSizeInfo(thirdActivitySession);
        assertSizesAreSame(secondarySplitActivitySizes, applicationSizes);
        assertSizesAreSane(fullscreenActivitySizes, secondarySplitActivitySizes);
    }

    @Test
    public void testAppConfigurationMatchesActivityInFreeform() throws Exception {
        assumeTrue("Skipping test: no freeform support", supportsFreeform());

        // Launch activity in freeform and assert sizes
        final ActivitySession freeformActivitySession = createManagedActivityClientSession()
                .startActivity(getLaunchActivityBuilder()
                        .setUseInstrumentation()
                        .setTargetActivity(TEST_ACTIVITY)
                        .setWindowingMode(WINDOWING_MODE_FREEFORM));
        SizeInfo freeformActivitySizes = getActivitySizeInfo(freeformActivitySession);
        SizeInfo applicationSizes = getAppSizeInfo(freeformActivitySession);
        assertSizesAreSame(freeformActivitySizes, applicationSizes);
    }

    private boolean activityAndAppSizesMatch(ActivitySession activitySession) {
        final SizeInfo activitySize = activitySession.getConfigInfo().sizeInfo;
        final SizeInfo appSize = activitySession.getAppConfigInfo().sizeInfo;
        return activitySize.equals(appSize);
    }

    private SizeInfo getActivitySizeInfo(ActivitySession activitySession) {
        return activitySession.getConfigInfo().sizeInfo;
    }

    private SizeInfo getAppSizeInfo(ActivitySession activitySession) {
        return activitySession.getAppConfigInfo().sizeInfo;
    }
}
