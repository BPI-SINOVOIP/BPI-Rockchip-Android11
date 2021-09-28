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

package android.server.wm;

import static android.server.wm.WindowManagerState.STATE_RESUMED;
import static android.server.wm.ComponentNameUtils.getActivityName;
import static android.server.wm.MockImeHelper.createManagedMockImeSession;
import static android.server.wm.MultiDisplaySystemDecorationTests.ImeTestActivity;
import static android.server.wm.app.Components.DISPLAY_ACCESS_CHECK_EMBEDDING_ACTIVITY;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.LAUNCH_BROADCAST_RECEIVER;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.ACTION_TEST_ACTIVITY_START;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.EXTRA_COMPONENT_NAME;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.EXTRA_TARGET_DISPLAY;
import static android.server.wm.app.Components.LaunchBroadcastReceiver.LAUNCH_BROADCAST_ACTION;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.VIRTUAL_DISPLAY_ACTIVITY;
import static android.server.wm.second.Components.EMBEDDING_ACTIVITY;
import static android.server.wm.second.Components.EmbeddingActivity.ACTION_EMBEDDING_TEST_ACTIVITY_START;
import static android.server.wm.second.Components.EmbeddingActivity.EXTRA_EMBEDDING_COMPONENT_NAME;
import static android.server.wm.second.Components.EmbeddingActivity.EXTRA_EMBEDDING_TARGET_DISPLAY;
import static android.server.wm.second.Components.SECOND_ACTIVITY;
import static android.server.wm.second.Components.SECOND_LAUNCH_BROADCAST_ACTION;
import static android.server.wm.second.Components.SECOND_LAUNCH_BROADCAST_RECEIVER;
import static android.server.wm.second.Components.SECOND_NO_EMBEDDING_ACTIVITY;
import static android.server.wm.second.Components.SecondActivity.EXTRA_DISPLAY_ACCESS_CHECK;
import static android.server.wm.third.Components.THIRD_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.notExpectEvent;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.server.wm.WindowManagerState.DisplayContent;
import android.server.wm.WindowManagerState.ActivityTask;
import android.server.wm.CommandSession.ActivitySession;
import android.server.wm.TestJournalProvider.TestJournalContainer;
import android.view.Display;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;

import com.android.compatibility.common.util.SystemUtil;
import com.android.compatibility.common.util.TestUtils;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.MockImeSession;

import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.TimeUnit;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:MultiDisplaySecurityTests
 *
 * Tests if be allowed to launch an activity on multi-display environment.
 */
@Presubmit
@android.server.wm.annotation.Group3
public class MultiDisplaySecurityTests extends MultiDisplayTestBase {

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsMultiDisplay());
    }

    /**
     * Tests launching an activity on a virtual display without special permission must not be
     * allowed.
     */
    @Test
    public void testLaunchWithoutPermissionOnVirtualDisplayByOwner() {
        // Create new virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession().createDisplay();

        separateTestJournal();

        // Try to launch an activity and check if security exception was triggered.
        getLaunchActivityBuilder()
                .setUseBroadcastReceiver(LAUNCH_BROADCAST_RECEIVER, LAUNCH_BROADCAST_ACTION)
                .setDisplayId(newDisplay.mId)
                .setTargetActivity(TEST_ACTIVITY)
                .execute();
        assertSecurityExceptionFromActivityLauncher();
        mWmState.computeState(TEST_ACTIVITY);
        assertFalse("Restricted activity must not be launched",
                mWmState.containsActivity(TEST_ACTIVITY));
    }

    /**
     * Tests launching an activity on a virtual display without special permission must not be
     * allowed.
     */
    @Test
    public void testLaunchWithoutPermissionOnVirtualDisplay() {
        // Create new virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession().createDisplay();

        separateTestJournal();

        // Try to launch an activity and check it security exception was triggered.
        getLaunchActivityBuilder()
                .setUseBroadcastReceiver(SECOND_LAUNCH_BROADCAST_RECEIVER,
                        SECOND_LAUNCH_BROADCAST_ACTION)
                .setDisplayId(newDisplay.mId)
                .setTargetActivity(TEST_ACTIVITY)
                .execute();
        assertSecurityExceptionFromActivityLauncher();
        mWmState.computeState(TEST_ACTIVITY);
        assertFalse("Restricted activity must not be launched",
                mWmState.containsActivity(TEST_ACTIVITY));
    }

    /**
     * Tests launching an activity on virtual display and then launching another activity that
     * doesn't allow embedding - it should fail with security exception.
     */
    @Test
    public void testConsequentLaunchActivityFromVirtualDisplayNoEmbedding() {
        // Create new virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession().createDisplay();

        // Launch activity on new secondary display.
        launchActivityOnDisplay(LAUNCHING_ACTIVITY, newDisplay.mId);

        waitAndAssertActivityStateOnDisplay(LAUNCHING_ACTIVITY, STATE_RESUMED, newDisplay.mId,
                "Activity launched on secondary display must be resumed");

        separateTestJournal();

        // Launch second activity from app on secondary display specifying same display id.
        getLaunchActivityBuilder()
                .setTargetActivity(SECOND_NO_EMBEDDING_ACTIVITY)
                .setDisplayId(newDisplay.mId)
                .execute();

        assertSecurityExceptionFromActivityLauncher();
    }

    private boolean isActivityStartAllowedOnDisplay(int displayId, ComponentName activity) {
        final Intent intent = new Intent(Intent.ACTION_VIEW).setComponent(activity);
        return mTargetContext.getSystemService(ActivityManager.class)
                .isActivityStartAllowedOnDisplay(mTargetContext, displayId, intent);
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for simulated display. It is owned by system and is public, so should be accessible.
     */
    @Test
    public void testCanAccessSystemOwnedDisplay()  {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        assertTrue(isActivityStartAllowedOnDisplay(newDisplay.mId, TEST_ACTIVITY));
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a public virtual display and an activity that doesn't support embedding from shell.
     */
    @Test
    public void testCanAccessPublicVirtualDisplayWithInternalPermission() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();

        SystemUtil.runWithShellPermissionIdentity(
                () -> assertTrue(isActivityStartAllowedOnDisplay(
                        newDisplay.mId, SECOND_NO_EMBEDDING_ACTIVITY)),
                "android.permission.INTERNAL_SYSTEM_WINDOW");
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a private virtual display and an activity that doesn't support embedding from shell.
     */
    @Test
    public void testCanAccessPrivateVirtualDisplayWithInternalPermission() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();

        SystemUtil.runWithShellPermissionIdentity(
                () -> assertTrue(isActivityStartAllowedOnDisplay(
                        newDisplay.mId, SECOND_NO_EMBEDDING_ACTIVITY)),
                "android.permission.INTERNAL_SYSTEM_WINDOW");
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a public virtual display, an activity that supports embedding but the launching entity
     * does not have required permission to embed an activity from other app.
     */
    @Test
    public void testCantAccessPublicVirtualDisplayNoEmbeddingPermission() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();

        assertFalse(isActivityStartAllowedOnDisplay(newDisplay.mId, SECOND_ACTIVITY));
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a public virtual display and an activity that does not support embedding.
     */
    @Test
    public void testCantAccessPublicVirtualDisplayActivityEmbeddingNotAllowed() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();

        SystemUtil.runWithShellPermissionIdentity(
                () -> assertFalse(isActivityStartAllowedOnDisplay(
                        newDisplay.mId, SECOND_NO_EMBEDDING_ACTIVITY)),
                "android.permission.ACTIVITY_EMBEDDING");
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a public virtual display and an activity that supports embedding.
     */
    @Test
    public void testCanAccessPublicVirtualDisplayActivityEmbeddingAllowed() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();

        SystemUtil.runWithShellPermissionIdentity(
                () -> assertTrue(isActivityStartAllowedOnDisplay(
                        newDisplay.mId, SECOND_ACTIVITY)),
                "android.permission.ACTIVITY_EMBEDDING");
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a private virtual display.
     */
    @Test
    public void testCantAccessPrivateVirtualDisplay() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();

        assertFalse(isActivityStartAllowedOnDisplay(newDisplay.mId, SECOND_ACTIVITY));
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a private virtual display to check the start of its own activity.
     */
    @Test
    public void testCantAccessPrivateVirtualDisplayByOwner() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();

        // Check the embedding call.
        separateTestJournal();
        mContext.sendBroadcast(new Intent(ACTION_TEST_ACTIVITY_START)
                .setPackage(LAUNCH_BROADCAST_RECEIVER.getPackageName())
                .setFlags(Intent.FLAG_RECEIVER_FOREGROUND)
                .putExtra(EXTRA_COMPONENT_NAME, TEST_ACTIVITY)
                .putExtra(EXTRA_TARGET_DISPLAY, newDisplay.mId));

        assertActivityStartCheckResult(false);
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a private virtual display by UID present on that display and target activity that allows
     * embedding.
     */
    @Test
    public void testCanAccessPrivateVirtualDisplayByUidPresentOnDisplayActivityEmbeddingAllowed() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();
        // Launch a test activity into the target display.
        launchActivityOnDisplay(EMBEDDING_ACTIVITY, newDisplay.mId);

        // Check the embedding call.
        separateTestJournal();
        mContext.sendBroadcast(new Intent(ACTION_EMBEDDING_TEST_ACTIVITY_START)
                .setFlags(Intent.FLAG_RECEIVER_FOREGROUND)
                .putExtra(EXTRA_EMBEDDING_COMPONENT_NAME, SECOND_ACTIVITY)
                .putExtra(EXTRA_EMBEDDING_TARGET_DISPLAY, newDisplay.mId));

        assertActivityStartCheckResult(true);
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a private virtual display by UID present on that display and target activity that does
     * not allow embedding.
     */
    @Test
    public void testCanAccessPrivateVirtualDisplayByUidPresentOnDisplayActivityEmbeddingNotAllowed()
            throws Exception {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();
        // Launch a test activity into the target display.
        launchActivityOnDisplay(EMBEDDING_ACTIVITY, newDisplay.mId);

        // Check the embedding call.
        separateTestJournal();
        mContext.sendBroadcast(new Intent(ACTION_EMBEDDING_TEST_ACTIVITY_START)
                .setFlags(Intent.FLAG_RECEIVER_FOREGROUND)
                .putExtra(EXTRA_EMBEDDING_COMPONENT_NAME, SECOND_NO_EMBEDDING_ACTIVITY)
                .putExtra(EXTRA_EMBEDDING_TARGET_DISPLAY, newDisplay.mId));

        assertActivityStartCheckResult(false);
    }

    private void assertActivityStartCheckResult(boolean expected) {
        final String component = ActivityLauncher.TAG;
        final Bundle resultExtras = Condition.waitForResult(
                new Condition<Bundle>("activity start check for " + component)
                        .setRetryIntervalMs(500)
                        .setResultSupplier(() -> TestJournalContainer.get(component).extras)
                        .setResultValidator(extras -> extras.containsKey(
                                ActivityLauncher.KEY_IS_ACTIVITY_START_ALLOWED_ON_DISPLAY)));
        if (resultExtras != null) {
            assertEquals("Activity start check must match", expected, resultExtras
                    .getBoolean(ActivityLauncher.KEY_IS_ACTIVITY_START_ALLOWED_ON_DISPLAY));
            return;
        }
        fail("Expected activity start check from " + component + " not found");
    }

    @Test
    public void testDisplayHasAccess_UIDCanPresentOnPrivateDisplay() {
        final VirtualDisplayLauncher virtualDisplayLauncher =
                mObjectTracker.manage(new VirtualDisplayLauncher());
        // Create a virtual private display.
        final DisplayContent newDisplay = virtualDisplayLauncher
                .setPublicDisplay(false)
                .createDisplay();
        // Launch an embeddable activity into the private display.
        // Assert that the UID can present on display.
        final ActivitySession session1 = virtualDisplayLauncher.launchActivityOnDisplay(
                DISPLAY_ACCESS_CHECK_EMBEDDING_ACTIVITY, newDisplay);
        assertEquals("Activity which the UID should accessible on private display",
                isUidAccesibleOnDisplay(session1), true);

        // Launch another embeddable activity with a different UID, verify that it will be
        // able to access the display where it was put.
        // Note that set withShellPermission as true in launchActivityOnDisplay is to
        // make sure ACTIVITY_EMBEDDING can be granted by shell.
        final ActivitySession session2 = virtualDisplayLauncher.launchActivityOnDisplay(
                SECOND_ACTIVITY, newDisplay,
                bundle -> bundle.putBoolean(EXTRA_DISPLAY_ACCESS_CHECK, true),
                true /* withShellPermission */, true /* waitForLaunch */);

        // Verify SECOND_ACTIVITY's UID has access to this virtual private display.
        assertEquals("Second activity which the UID should accessible on private display",
                isUidAccesibleOnDisplay(session2), true);
    }

    @Test
    public void testDisplayHasAccess_NoAccessWhenUIDNotPresentOnPrivateDisplay() {
        final VirtualDisplayLauncher virtualDisplayLauncher =
                mObjectTracker.manage(new VirtualDisplayLauncher());
        // Create a virtual private display.
        final DisplayContent newDisplay = virtualDisplayLauncher
                .setPublicDisplay(false)
                .createDisplay();
        // Launch an embeddable activity into the private display.
        // Assume that the UID can access on display.
        final ActivitySession session1 = virtualDisplayLauncher.launchActivityOnDisplay(
                DISPLAY_ACCESS_CHECK_EMBEDDING_ACTIVITY, newDisplay);
        assertEquals("Activity which the UID should accessible on private display",
                isUidAccesibleOnDisplay(session1), true);

        // Verify SECOND_NO_EMBEDDING_ACTIVITY's UID can't access this virtual private display
        // since there is no entity with this UID on this display.
        // Note that set withShellPermission as false in launchActivityOnDisplay is to
        // prevent activity can launch when INTERNAL_SYSTEM_WINDOW granted by shell case.
        separateTestJournal();
        final ActivitySession session2 = virtualDisplayLauncher.launchActivityOnDisplay(
                SECOND_NO_EMBEDDING_ACTIVITY, newDisplay, null /* extrasConsumer */,
                false /* withShellPermission */, false /* waitForLaunch */);
        assertEquals("Second activity which the UID should not accessible on private display",
                isUidAccesibleOnDisplay(session2), false);
    }

    @Test
    public void testDisplayHasAccess_ExceptionWhenAddViewWithoutPresentOnPrivateDisplay() {
        // Create a virtual private display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();
        try {
            final Display display = mDm.getDisplay(newDisplay.mId);
            final Context newDisplayContext = mContext.createDisplayContext(display);
            newDisplayContext.getSystemService(WindowManager.class).addView(new View(mContext),
                    new ViewGroup.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        } catch (IllegalArgumentException e) {
            // Exception happened when createDisplayContext with invalid display.
            return;
        }
        fail("UID should not have access to private display without present entities.");
    }

    private boolean isUidAccesibleOnDisplay(ActivitySession session) {
        boolean result = false;
        try {
            result = session.isUidAccesibleOnDisplay();
        } catch (RuntimeException e) {
            // Catch the exception while waiting reply (i.e. timeout)
        }
        return result;
    }

    /** Test that shell is allowed to launch on secondary displays. */
    @Test
    public void testPermissionLaunchFromShell(){
        // Create new virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession().createDisplay();
        mWmState.assertVisibility(VIRTUAL_DISPLAY_ACTIVITY, true /* visible */);
        mWmState.assertFocusedActivity("Virtual display activity must be on top",
                VIRTUAL_DISPLAY_ACTIVITY);
        final int defaultDisplayFocusedStackId = mWmState.getFocusedStackId();
        ActivityTask frontStack = mWmState.getRootTask(
                defaultDisplayFocusedStackId);
        assertEquals("Top stack must remain on primary display",
                DEFAULT_DISPLAY, frontStack.mDisplayId);

        // Launch activity on new secondary display.
        launchActivityOnDisplay(TEST_ACTIVITY, newDisplay.mId);

        waitAndAssertActivityStateOnDisplay(TEST_ACTIVITY, STATE_RESUMED, newDisplay.mId,
                "Test activity must be on secondary display");
        assertBothDisplaysHaveResumedActivities(pair(DEFAULT_DISPLAY, VIRTUAL_DISPLAY_ACTIVITY),
                pair(newDisplay.mId, TEST_ACTIVITY));

        // Launch other activity with different uid and check it is launched on dynamic stack on
        // secondary display.
        final String startCmd = "am start -n " + getActivityName(SECOND_ACTIVITY)
                + " --display " + newDisplay.mId;
        executeShellCommand(startCmd);

        waitAndAssertActivityStateOnDisplay(SECOND_ACTIVITY, STATE_RESUMED, newDisplay.mId,
                "Second activity must be on newly launched app");
        assertBothDisplaysHaveResumedActivities(pair(DEFAULT_DISPLAY, VIRTUAL_DISPLAY_ACTIVITY),
                pair(newDisplay.mId, SECOND_ACTIVITY));
    }

    /** Test that launching from app that is on external display is allowed. */
    @Test
    public void testPermissionLaunchFromAppOnSecondary() {
        // Create new simulated display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        // Launch activity with different uid on secondary display.
        final String startCmd = "am start -n " + getActivityName(SECOND_ACTIVITY)
                + " --display " + newDisplay.mId;
        executeShellCommand(startCmd);

        waitAndAssertTopResumedActivity(SECOND_ACTIVITY, newDisplay.mId,
                "Top activity must be the newly launched one");

        // Launch another activity with third different uid from app on secondary display and
        // check it is launched on secondary display.
        getLaunchActivityBuilder()
                .setUseBroadcastReceiver(SECOND_LAUNCH_BROADCAST_RECEIVER,
                        SECOND_LAUNCH_BROADCAST_ACTION)
                .setDisplayId(newDisplay.mId)
                .setTargetActivity(THIRD_ACTIVITY)
                .execute();

        waitAndAssertTopResumedActivity(THIRD_ACTIVITY, newDisplay.mId,
                "Top activity must be the newly launched one");
    }

    /** Tests that an activity can launch an activity from a different UID into its own task. */
    @Test
    public void testPermissionLaunchMultiUidTask() {
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();

        launchActivityOnDisplay(LAUNCHING_ACTIVITY, newDisplay.mId);
        mWmState.computeState(LAUNCHING_ACTIVITY);

        // Check that the first activity is launched onto the secondary display.
        final int frontStackId = mWmState.getFrontRootTaskId(newDisplay.mId);
        ActivityTask frontStack = mWmState.getRootTask(frontStackId);
        assertEquals("Activity launched on secondary display must be resumed",
                getActivityName(LAUNCHING_ACTIVITY), frontStack.mResumedActivity);
        mWmState.assertFocusedStack("Top stack must be on secondary display", frontStackId);

        // Launch an activity from a different UID into the first activity's task.
        getLaunchActivityBuilder().setTargetActivity(SECOND_ACTIVITY).execute();

        waitAndAssertTopResumedActivity(SECOND_ACTIVITY, newDisplay.mId,
                "Top activity must be the newly launched one");
        frontStack = mWmState.getRootTask(frontStackId);
        assertEquals("Secondary display must contain 1 task", 1,
                mWmState.getDisplay(newDisplay.mId).getRootTasks().size());
    }

    /**
     * Test that launching from app that is not present on external display and doesn't own it to
     * that external display is not allowed.
     */
    @Test
    public void testPermissionLaunchFromDifferentApp() {
        // Create new virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession().createDisplay();
        mWmState.assertVisibility(VIRTUAL_DISPLAY_ACTIVITY, true /* visible */);
        mWmState.assertFocusedActivity("Virtual display activity must be focused",
                VIRTUAL_DISPLAY_ACTIVITY);
        final int defaultDisplayFocusedStackId = mWmState.getFocusedStackId();
        ActivityTask frontStack = mWmState.getRootTask(
                defaultDisplayFocusedStackId);
        assertEquals("Top stack must remain on primary display",
                DEFAULT_DISPLAY, frontStack.mDisplayId);

        // Launch activity on new secondary display.
        launchActivityOnDisplay(TEST_ACTIVITY, newDisplay.mId);
        waitAndAssertActivityStateOnDisplay(TEST_ACTIVITY, STATE_RESUMED, newDisplay.mId,
                "Test activity must be the newly launched one");

        separateTestJournal();

        // Launch other activity with different uid and check security exception is triggered.
        getLaunchActivityBuilder()
                .setUseBroadcastReceiver(SECOND_LAUNCH_BROADCAST_RECEIVER,
                        SECOND_LAUNCH_BROADCAST_ACTION)
                .setDisplayId(newDisplay.mId)
                .setTargetActivity(THIRD_ACTIVITY)
                .execute();

        assertSecurityExceptionFromActivityLauncher();
    }

    /**
     * Test that only private virtual display can show content with insecure keyguard.
     */
    @Test
    public void testFlagShowWithInsecureKeyguardOnPublicVirtualDisplay() {
        // Try to create new show-with-insecure-keyguard public virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .setCanShowWithInsecureKeyguard(true)
                .createDisplay(false /* mustBeCreated */);

        // Check that the display is not created.
        assertNull(newDisplay);
    }

    /**
     * Test setting system decoration flag and show IME flag without sufficient permissions.
     */
    @Test
    public void testSettingFlagWithoutInternalSystemPermission() throws Exception {
        // The reason to use a trusted display is that we can guarantee the security exception
        // is coming from lacking internal system permission.
        final DisplayContent trustedDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();
        final WindowManager wm = mTargetContext.getSystemService(WindowManager.class);

        // Verify setting system decorations flag without internal system permission.
        try {
            wm.setShouldShowSystemDecors(trustedDisplay.mId, true);

            // Unexpected result, restore flag to avoid affecting other tests.
            wm.setShouldShowSystemDecors(trustedDisplay.mId, false);
            TestUtils.waitUntil("Waiting for system decoration flag to be set",
                    5 /* timeoutSecond */,
                    () -> !wm.shouldShowSystemDecors(trustedDisplay.mId));
            fail("Should not allow setting system decoration flag without internal system "
                    + "permission");
        } catch (SecurityException e) {
            // Expected security exception.
        }

        // Verify setting show IME flag without internal system permission.
        try {
            wm.setShouldShowIme(trustedDisplay.mId, true);

            // Unexpected result, restore flag to avoid affecting other tests.
            wm.setShouldShowIme(trustedDisplay.mId, false);
            TestUtils.waitUntil("Waiting for show IME flag to be set",
                    5 /* timeoutSecond */,
                    () -> !wm.shouldShowIme(trustedDisplay.mId));
            fail("Should not allow setting show IME flag without internal system permission");
        } catch (SecurityException e) {
            // Expected security exception.
        }
    }

    /**
     * Test getting system decoration flag and show IME flag without sufficient permissions.
     */
    @Test
    public void testGettingFlagWithoutInternalSystemPermission() {
        // The reason to use a trusted display is that we can guarantee the security exception
        // is coming from lacking internal system permission.
        final DisplayContent trustedDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();
        final WindowManager wm = mTargetContext.getSystemService(WindowManager.class);

        // Verify getting system decorations flag without internal system permission.
        try {
            wm.shouldShowSystemDecors(trustedDisplay.mId);
            fail("Only allow internal system to get system decoration flag");
        } catch (SecurityException e) {
            // Expected security exception.
        }

        // Verify getting show IME flag without internal system permission.
        try {
            wm.shouldShowIme(trustedDisplay.mId);
            fail("Only allow internal system to get show IME flag");
        } catch (SecurityException e) {
            // Expected security exception.
        }
    }

    /**
     * Test setting system decoration flag and show IME flag to the untrusted display.
     */
    @Test
    public void testSettingFlagToUntrustedDisplay() throws Exception {
        final DisplayContent untrustedDisplay = createManagedVirtualDisplaySession()
                .createDisplay();
        final WindowManager wm = mTargetContext.getSystemService(WindowManager.class);

        // Verify setting system decoration flag to an untrusted display.
        getInstrumentation().getUiAutomation().adoptShellPermissionIdentity();
        try {
            wm.setShouldShowSystemDecors(untrustedDisplay.mId, true);

            // Unexpected result, restore flag to avoid affecting other tests.
            wm.setShouldShowSystemDecors(untrustedDisplay.mId, false);
            TestUtils.waitUntil("Waiting for system decoration flag to be set",
                    5 /* timeoutSecond */,
                    () -> !wm.shouldShowSystemDecors(untrustedDisplay.mId));
            fail("Should not allow setting system decoration flag to the untrusted virtual "
                    + "display");
        } catch (SecurityException e) {
            // Expected security exception.
        } finally {
            getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
        }

        // Verify setting show IME flag to an untrusted display.
        getInstrumentation().getUiAutomation().adoptShellPermissionIdentity();
        try {
            wm.setShouldShowIme(untrustedDisplay.mId, true);

            // Unexpected result, restore flag to avoid affecting other tests.
            wm.setShouldShowIme(untrustedDisplay.mId, false);
            TestUtils.waitUntil("Waiting for show IME flag to be set",
                    5 /* timeoutSecond */,
                    () -> !wm.shouldShowIme(untrustedDisplay.mId));
            fail("Should not allow setting show IME flag to the untrusted virtual display");
        } catch (SecurityException e) {
            // Expected security exception.
        } finally {
            getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
        }
    }

    /**
     * Test getting system decoration flag and show IME flag from the untrusted display.
     */
    @Test
    public void testGettingFlagFromUntrustedDisplay() {
        final DisplayContent untrustedDisplay = createManagedVirtualDisplaySession()
                .createDisplay();
        final WindowManager wm = mTargetContext.getSystemService(WindowManager.class);

        // Verify getting system decoration flag from an untrusted display.
        SystemUtil.runWithShellPermissionIdentity(() -> assertFalse(
                "Display should not support showing system decorations",
                wm.shouldShowSystemDecors(untrustedDisplay.mId)));

        // Verify getting show IME flag from an untrusted display.
        SystemUtil.runWithShellPermissionIdentity(() -> assertFalse(
                "Display should not support showing IME window",
                wm.shouldShowIme(untrustedDisplay.mId)));
    }

    /**
     * Test setting system decoration flag and show IME flag to the trusted display.
     */
    @Test
    public void testSettingFlagToTrustedDisplay() throws Exception {
        final DisplayContent trustedDisplay = createManagedVirtualDisplaySession()
                .setSimulateDisplay(true)
                .createDisplay();
        final WindowManager wm = mTargetContext.getSystemService(WindowManager.class);

        // Verify setting system decoration flag to a trusted display.
        SystemUtil.runWithShellPermissionIdentity(() -> {
            // Assume the display should not support system decorations by default.
            assertFalse(wm.shouldShowSystemDecors(trustedDisplay.mId));

            try {
                wm.setShouldShowSystemDecors(trustedDisplay.mId, true);
                TestUtils.waitUntil("Waiting for system decoration flag to be set",
                        5 /* timeoutSecond */,
                        () -> wm.shouldShowSystemDecors(trustedDisplay.mId));

                assertTrue(wm.shouldShowSystemDecors(trustedDisplay.mId));
            } finally {
                // Restore flag to avoid affecting other tests.
                wm.setShouldShowSystemDecors(trustedDisplay.mId, false);
                TestUtils.waitUntil("Waiting for system decoration flag to be set",
                        5 /* timeoutSecond */,
                        () -> !wm.shouldShowSystemDecors(trustedDisplay.mId));
            }
        });

        // Verify setting show IME flag to a trusted display.
        SystemUtil.runWithShellPermissionIdentity(() -> {
            // Assume the display should not show IME window by default.
            assertFalse(wm.shouldShowIme(trustedDisplay.mId));

            try {
                wm.setShouldShowIme(trustedDisplay.mId, true);
                TestUtils.waitUntil("Waiting for show IME flag to be set",
                        5 /* timeoutSecond */,
                        () -> wm.shouldShowIme(trustedDisplay.mId));

                assertTrue(wm.shouldShowIme(trustedDisplay.mId));
            } finally {
                // Restore flag to avoid affecting other tests.
                wm.setShouldShowIme(trustedDisplay.mId, false);
                TestUtils.waitUntil("Waiting for show IME flag to be set",
                        5 /* timeoutSecond */,
                        () -> !wm.shouldShowIme(trustedDisplay.mId));
            }
        });
    }

    @Test
    public void testNoInputConnectionForUntrustedVirtualDisplay() throws Exception {
        assumeTrue(MSG_NO_MOCK_IME, supportsInstallableIme());

        final long NOT_EXPECT_TIMEOUT = TimeUnit.SECONDS.toMillis(2);

        final MockImeSession mockImeSession = createManagedMockImeSession(this);
        final TestActivitySession<ImeTestActivity> imeTestActivitySession =
                createManagedTestActivitySession();
         // Create a untrusted virtual display and assume the display should not show IME window.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true).createDisplay();

        // Launch Ime test activity in virtual display.
        imeTestActivitySession.launchTestActivityOnDisplay(ImeTestActivity.class,
                newDisplay.mId);
        // Verify that activity which lives in untrusted display should not be focused.
        assertNotEquals("ImeTestActivity should not be focused",
                mWmState.getFocusedActivity(),
                imeTestActivitySession.getActivity().getComponentName().toString());

        // Expect onStartInput won't executed in the IME client.
        final ImeEventStream stream = mockImeSession.openEventStream();
        final EditText editText = imeTestActivitySession.getActivity().mEditText;
        imeTestActivitySession.runOnMainSyncAndWait(
                imeTestActivitySession.getActivity()::showSoftInput);
        notExpectEvent(stream, editorMatcher("onStartInput",
                editText.getPrivateImeOptions()), NOT_EXPECT_TIMEOUT);

        // Expect onStartInput / showSoftInput would be executed when user tapping on the
        // untrusted display intentionally.
        final Rect drawRect = new Rect();
        editText.getDrawingRect(drawRect);
        tapOnDisplaySync(drawRect.left, drawRect.top, newDisplay.mId);
        imeTestActivitySession.runOnMainSyncAndWait(
                imeTestActivitySession.getActivity()::showSoftInput);
        waitOrderedImeEventsThenAssertImeShown(stream, DEFAULT_DISPLAY,
                editorMatcher("onStartInput", editText.getPrivateImeOptions()),
                event -> "showSoftInput".equals(event.getEventName()));

        // Switch focus to top focused display as default display, verify onStartInput won't
        // be called since the untrusted display should no longer get focus.
        tapOnDisplayCenter(DEFAULT_DISPLAY);
        mWmState.computeState();
        assertEquals(DEFAULT_DISPLAY, mWmState.getFocusedDisplayId());
        imeTestActivitySession.getActivity().resetPrivateImeOptionsIdentifier();
        imeTestActivitySession.runOnMainSyncAndWait(
                imeTestActivitySession.getActivity()::showSoftInput);
        notExpectEvent(stream, editorMatcher("onStartInput",
                editText.getPrivateImeOptions()), NOT_EXPECT_TIMEOUT);
    }
}
