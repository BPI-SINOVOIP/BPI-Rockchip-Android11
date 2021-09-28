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

import static android.server.wm.WindowManagerState.STATE_STOPPED;
import static android.server.wm.WindowManagerState.TRANSIT_ACTIVITY_OPEN;
import static android.server.wm.WindowManagerState.TRANSIT_KEYGUARD_GOING_AWAY;
import static android.server.wm.WindowManagerState.TRANSIT_KEYGUARD_GOING_AWAY_ON_WALLPAPER;
import static android.server.wm.WindowManagerState.TRANSIT_KEYGUARD_OCCLUDE;
import static android.server.wm.WindowManagerState.TRANSIT_KEYGUARD_UNOCCLUDE;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_ACTIVITY;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_ATTR_ACTIVITY;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY;
import static android.server.wm.app.Components.SHOW_WHEN_LOCKED_WITH_DIALOG_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.app.Components.WALLPAPAER_ACTIVITY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.Presubmit;

import org.junit.Before;
import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:KeyguardTransitionTests
 */
@Presubmit
@android.server.wm.annotation.Group2
public class KeyguardTransitionTests extends ActivityManagerTestBase {

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        assumeTrue(supportsInsecureLock());
        assumeFalse(isUiModeLockedToVrHeadset());
    }

    @Test
    public void testUnlock() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        launchActivity(TEST_ACTIVITY);
        lockScreenSession.gotoKeyguard().unlockDevice();
        mWmState.computeState(TEST_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_KEYGUARD_GOING_AWAY,
                mWmState.getDefaultDisplayLastTransition());
    }

    @Test
    public void testUnlockWallpaper() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        launchActivity(WALLPAPAER_ACTIVITY);
        lockScreenSession.gotoKeyguard().unlockDevice();
        mWmState.computeState(WALLPAPAER_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_KEYGUARD_GOING_AWAY_ON_WALLPAPER,
                mWmState.getDefaultDisplayLastTransition());
    }

    @Test
    public void testOcclude() {
        createManagedLockScreenSession().gotoKeyguard();
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_KEYGUARD_OCCLUDE,
                mWmState.getDefaultDisplayLastTransition());
    }

    @Test
    public void testUnocclude() {
        createManagedLockScreenSession().gotoKeyguard();
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        launchActivity(TEST_ACTIVITY);
        mWmState.waitForKeyguardShowingAndNotOccluded();
        mWmState.computeState();
        assertEquals("Picked wrong transition", TRANSIT_KEYGUARD_UNOCCLUDE,
                mWmState.getDefaultDisplayLastTransition());
    }

    @Test
    public void testNewActivityDuringOccluded() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        launchActivity(SHOW_WHEN_LOCKED_ACTIVITY);
        lockScreenSession.gotoKeyguard(SHOW_WHEN_LOCKED_ACTIVITY);
        launchActivity(SHOW_WHEN_LOCKED_WITH_DIALOG_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_WITH_DIALOG_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_ACTIVITY_OPEN,
                mWmState.getDefaultDisplayLastTransition());
    }

    @Test
    public void testOccludeManifestAttr() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.gotoKeyguard();
        separateTestJournal();
        launchActivity(SHOW_WHEN_LOCKED_ATTR_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ATTR_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_KEYGUARD_OCCLUDE,
                mWmState.getDefaultDisplayLastTransition());
        assertSingleLaunch(SHOW_WHEN_LOCKED_ATTR_ACTIVITY);
    }

    @Test
    public void testOccludeAttrRemove() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        lockScreenSession.gotoKeyguard();
        separateTestJournal();
        launchActivity(SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_KEYGUARD_OCCLUDE,
                mWmState.getDefaultDisplayLastTransition());
        assertSingleLaunch(SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY);

        // Waiting for the standard keyguard since
        // {@link SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY} called
        // {@link Activity#showWhenLocked(boolean)} and removed the attribute.
        lockScreenSession.gotoKeyguard();
        separateTestJournal();
        // Waiting for {@link SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY} stopped since it
        // already lost show-when-locked attribute.
        launchActivityNoWait(SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY);
        mWmState.waitForActivityState(SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY, STATE_STOPPED);
        assertSingleStartAndStop(SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY);
    }

    @Test
    public void testNewActivityDuringOccludedWithAttr() {
        final LockScreenSession lockScreenSession = createManagedLockScreenSession();
        launchActivity(SHOW_WHEN_LOCKED_ATTR_ACTIVITY);
        lockScreenSession.gotoKeyguard(SHOW_WHEN_LOCKED_ATTR_ACTIVITY);
        launchActivity(SHOW_WHEN_LOCKED_WITH_DIALOG_ACTIVITY);
        mWmState.computeState(SHOW_WHEN_LOCKED_WITH_DIALOG_ACTIVITY);
        assertEquals("Picked wrong transition", TRANSIT_ACTIVITY_OPEN,
                mWmState.getDefaultDisplayLastTransition());
    }
}
