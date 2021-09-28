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

import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.view.Display.FLAG_PRIVATE;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.platform.test.annotations.Presubmit;
import android.server.wm.WindowManagerState.DisplayContent;
import android.util.Log;

import com.android.compatibility.common.util.SystemUtil;

import java.util.ArrayList;

import org.junit.Before;
import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:MultiDisplayPrivateDisplayTests
 *
 * Tests if be allowed to launch/access an activity on private display
 * in multi-display environment.
 */
@Presubmit
@android.server.wm.annotation.Group3
public class MultiDisplayPrivateDisplayTests extends MultiDisplayTestBase {
    private static final String TAG = "MultiDisplayPrivateDisplayTests";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);
    private static final String INTERNAL_SYSTEM_WINDOW =
            "android.permission.INTERNAL_SYSTEM_WINDOW";
    private ArrayList<Integer> mPrivateDisplayIds = new ArrayList<>();

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(supportsMultiDisplay());
        findPrivateDisplays();
        assumeFalse("Skipping test: no physical private display found.",
                mPrivateDisplayIds.isEmpty());
    }

    /** Saves physical private displays in mPrivateDisplayIds */
    private void findPrivateDisplays() {
        mPrivateDisplayIds.clear();
        mWmState.computeState();

        for (DisplayContent displayContent: getDisplaysStates()) {
            int displayId = displayContent.mId;
            DisplayContent display = mWmState.getDisplay(displayId);
            if ((display.getFlags() & FLAG_PRIVATE) != 0) {
                mPrivateDisplayIds.add(displayId);
            }
        }
    }

    /**
     * Tests launching an activity on a private display without special permission must not be
     * allowed.
     */
    @Test
    public void testCantLaunchOnPrivateDisplay() throws Exception {
        // try on each private display
        for (int displayId: mPrivateDisplayIds) {
            separateTestJournal();

            getLaunchActivityBuilder()
                .setDisplayId(displayId)
                .setTargetActivity(TEST_ACTIVITY)
                .execute();

            assertSecurityExceptionFromActivityLauncher();

            mWmState.computeState(TEST_ACTIVITY);
            assertFalse("Activity must not be launched on a private display",
                mWmState.containsActivity(TEST_ACTIVITY));
        }
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * call to start an activity on private display is not allowed without special permission
     */
    @Test
    public void testCantAccessPrivateDisplay() throws Exception {
        final ActivityManager activityManager =
            (ActivityManager) mTargetContext.getSystemService(Context.ACTIVITY_SERVICE);
        final Intent intent = new Intent(Intent.ACTION_VIEW).setComponent(TEST_ACTIVITY);

        for (int displayId: mPrivateDisplayIds) {
            assertFalse(activityManager.isActivityStartAllowedOnDisplay(mTargetContext,
                displayId, intent));
        }
    }

    /**
     * Tests
     * {@link android.app.ActivityManager#isActivityStartAllowedOnDisplay(Context, int, Intent)}
     * for a private display with INTERNAL_SYSTEM_WINDOW permission.
     */
    @Test
    public void testCanAccessPrivateDisplayWithInternalPermission() throws Exception {
        final ActivityManager activityManager =
            (ActivityManager) mTargetContext.getSystemService(Context.ACTIVITY_SERVICE);
        final Intent intent = new Intent(Intent.ACTION_VIEW)
            .setComponent(TEST_ACTIVITY);

        for (int displayId: mPrivateDisplayIds) {
            SystemUtil.runWithShellPermissionIdentity(() ->
                assertTrue(activityManager.isActivityStartAllowedOnDisplay(mTargetContext,
                    displayId, intent)), INTERNAL_SYSTEM_WINDOW);
        }
    }
}
