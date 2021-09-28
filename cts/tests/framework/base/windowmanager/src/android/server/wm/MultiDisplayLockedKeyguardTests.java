/*
 * Copyright (C) 2017 The Android Open Source Project
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
import static android.server.wm.WindowManagerState.STATE_STOPPED;
import static android.server.wm.ActivityManagerTestBase.LockScreenSession.FLAG_REMOVE_ACTIVITIES_ON_CLOSE;
import static android.server.wm.app.Components.DISMISS_KEYGUARD_ACTIVITY;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.VIRTUAL_DISPLAY_ACTIVITY;

import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.Presubmit;
import android.server.wm.WindowManagerState.DisplayContent;

import androidx.test.filters.FlakyTest;

import org.junit.Before;
import org.junit.Test;

/**
 * Display tests that require a locked keyguard.
 *
 * <p>Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:MultiDisplayLockedKeyguardTests
 */
@Presubmit
@android.server.wm.annotation.Group3
public class MultiDisplayLockedKeyguardTests extends MultiDisplayTestBase {

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        assumeTrue(supportsMultiDisplay());
        assumeTrue(supportsSecureLock());
    }

    /**
     * Test that virtual display content is hidden when device is locked.
     */
    @Test
    public void testVirtualDisplayHidesContentWhenLocked() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();

        // Create new usual virtual display.
        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();
        mWmState.assertVisibility(VIRTUAL_DISPLAY_ACTIVITY, true /* visible */);

        // Launch activity on new secondary display.
        launchActivityOnDisplay(TEST_ACTIVITY, newDisplay.mId);
        mWmState.assertVisibility(TEST_ACTIVITY, true /* visible */);

        // Lock the device.
        lockScreenSession.gotoKeyguard();
        waitAndAssertActivityState(TEST_ACTIVITY, STATE_STOPPED,
                "Expected stopped activity on secondary display ");
        mWmState.assertVisibility(TEST_ACTIVITY, false /* visible */);

        // Unlock and check if visibility is back.
        lockScreenSession.unlockDevice();

        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitAndAssertKeyguardGone();
        waitAndAssertActivityState(TEST_ACTIVITY, STATE_RESUMED,
                "Expected resumed activity on secondary display");
        mWmState.assertVisibility(TEST_ACTIVITY, true /* visible */);
    }

    /**
     * Tests that private display cannot show content while device locked.
     */
    @Test
    public void testPrivateDisplayHideContentWhenLocked() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.setLockCredential();

        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(false)
                .createDisplay();
        launchActivityOnDisplay(TEST_ACTIVITY, newDisplay.mId);

        lockScreenSession.gotoKeyguard();

        waitAndAssertActivityState(TEST_ACTIVITY, STATE_STOPPED,
                "Expected stopped activity on private display");
        mWmState.assertVisibility(TEST_ACTIVITY, false /* visible */);
    }

    /**
     * Tests whether a FLAG_DISMISS_KEYGUARD activity on a secondary display dismisses the keyguard.
     */
    @Test
    public void testDismissKeyguard_secondaryDisplay() {
        final LockScreenSession lockScreenSession =
                mObjectTracker.manage(new LockScreenSession(FLAG_REMOVE_ACTIVITIES_ON_CLOSE));
        lockScreenSession.setLockCredential();

        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();

        lockScreenSession.gotoKeyguard();
        mWmState.assertKeyguardShowingAndNotOccluded();
        getLaunchActivityBuilder().setUseInstrumentation()
                .setTargetActivity(DISMISS_KEYGUARD_ACTIVITY).setNewTask(true)
                .setMultipleTask(true).setDisplayId(newDisplay.mId)
                .setWaitForLaunched(false).execute();
        waitAndAssertActivityState(DISMISS_KEYGUARD_ACTIVITY, STATE_STOPPED,
                "Expected stopped activity on secondary display");
        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitAndAssertKeyguardGone();
        mWmState.assertVisibility(DISMISS_KEYGUARD_ACTIVITY, true);
    }

    @Test
    public void testDismissKeyguard_whileOccluded_secondaryDisplay() {
        final LockScreenSession lockScreenSession =
                mObjectTracker.manage(new LockScreenSession(FLAG_REMOVE_ACTIVITIES_ON_CLOSE));
        lockScreenSession.setLockCredential();

        final DisplayContent newDisplay = createManagedVirtualDisplaySession()
                .setPublicDisplay(true)
                .createDisplay();

        lockScreenSession.gotoKeyguard();
        mWmState.assertKeyguardShowingAndNotOccluded();
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);
        getLaunchActivityBuilder().setUseInstrumentation()
                .setTargetActivity(DISMISS_KEYGUARD_ACTIVITY).setNewTask(true)
                .setMultipleTask(true).setDisplayId(newDisplay.mId)
                .setWaitForLaunched(false).execute();
        waitAndAssertActivityState(DISMISS_KEYGUARD_ACTIVITY, STATE_STOPPED,
                "Expected stopped activity on secondary display");
        lockScreenSession.enterAndConfirmLockCredential();
        mWmState.waitAndAssertKeyguardGone();
        mWmState.computeState(DISMISS_KEYGUARD_ACTIVITY);
        mWmState.assertVisibility(DISMISS_KEYGUARD_ACTIVITY, true);
        mWmState.assertVisibility(SHOW_WHEN_LOCKED_ACTIVITY, true);
    }
}
