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

import static android.app.ActivityTaskManager.INVALID_STACK_ID;
import static android.server.wm.WindowManagerState.STATE_RESUMED;
import static android.server.wm.app.Components.MPP_ACTIVITY;
import static android.server.wm.app.Components.MPP_ACTIVITY2;
import static android.server.wm.app.Components.MPP_ACTIVITY3;
import static android.server.wm.app.Components.MinimalPostProcessingActivity.EXTRA_PREFER_MPP;
import static android.server.wm.app.Components.POPUP_MPP_ACTIVITY;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.ComponentName;
import android.platform.test.annotations.Presubmit;

import org.junit.Test;

@Presubmit
public class MinimalPostProcessingTests extends ActivityManagerTestBase {
    private static final boolean PREFER_MPP = true;
    private static final boolean NOT_PREFER_MPP = false;

    private void launchMppActivity(ComponentName name, boolean preferMinimalPostProcessing) {
        if (preferMinimalPostProcessing) {
            launchActivity(name, EXTRA_PREFER_MPP, "anything");
        } else {
            launchActivity(name);
        }
        mWmState.waitForValidState(name);

        final int stackId = mWmState.getStackIdByActivity(name);

        assertNotEquals(stackId, INVALID_STACK_ID);

        mWmState.assertVisibility(name, true);
    }

    private boolean isMinimalPostProcessingSupported(int displayId) {
        return mDm.getDisplay(displayId).isMinimalPostProcessingSupported();
    }

    private boolean isMinimalPostProcessingRequested(int displayId) {
        return mDm.isMinimalPostProcessingRequested(displayId);
    }

    private int getDisplayId(ComponentName name) {
        return mWmState.getDisplayByActivity(name);
    }

    private void assertDisplayRequestedMinimalPostProcessing(ComponentName name, boolean on) {
        final int displayId = getDisplayId(name);

        boolean supported = isMinimalPostProcessingSupported(displayId);
        boolean requested = isMinimalPostProcessingRequested(displayId);

        assertTrue(supported ? requested == on : !requested);
    }

    @Test
    public void testPreferMinimalPostProcessingSimple() throws Exception {
        launchMppActivity(MPP_ACTIVITY, PREFER_MPP);
        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, PREFER_MPP);
    }

    @Test
    public void testNotPreferMinimalPostProcessingSimple() throws Exception {
        launchMppActivity(MPP_ACTIVITY, NOT_PREFER_MPP);
        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, NOT_PREFER_MPP);
    }

    @Test
    public void testAttrPreferMinimalPostProcessingDefault() throws Exception {
        launchMppActivity(MPP_ACTIVITY, NOT_PREFER_MPP);
        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, NOT_PREFER_MPP);
    }

    @Test
    public void testAttrPreferMinimalPostProcessingSimple() throws Exception {
        launchMppActivity(MPP_ACTIVITY3, NOT_PREFER_MPP);
        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY3, PREFER_MPP);
    }

    @Test
    public void testTwoVisibleWindowsNeitherPreferMinimalPostProcessing() throws Exception {
        launchMppActivity(MPP_ACTIVITY, NOT_PREFER_MPP);
        launchMppActivity(POPUP_MPP_ACTIVITY, NOT_PREFER_MPP);

        mWmState.assertVisibility(MPP_ACTIVITY, true);
        mWmState.assertVisibility(POPUP_MPP_ACTIVITY, true);

        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, NOT_PREFER_MPP);
    }

    @Test
    public void testTwoVisibleWindowsFirstOnePrefersMinimalPostProcessing() throws Exception {
        launchMppActivity(MPP_ACTIVITY, PREFER_MPP);
        launchMppActivity(POPUP_MPP_ACTIVITY, NOT_PREFER_MPP);

        mWmState.assertVisibility(MPP_ACTIVITY, true);
        mWmState.assertVisibility(POPUP_MPP_ACTIVITY, true);

        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, PREFER_MPP);
    }

    @Test
    public void testTwoVisibleWindowsSecondOnePrefersMinimalPostProcessing() throws Exception {
        launchMppActivity(MPP_ACTIVITY, NOT_PREFER_MPP);
        launchMppActivity(POPUP_MPP_ACTIVITY, PREFER_MPP);

        mWmState.assertVisibility(MPP_ACTIVITY, true);
        mWmState.assertVisibility(POPUP_MPP_ACTIVITY, true);

        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, PREFER_MPP);
    }

    @Test
    public void testTwoVisibleWindowsBothPreferMinimalPostProcessing() throws Exception {
        launchMppActivity(MPP_ACTIVITY, PREFER_MPP);
        launchMppActivity(POPUP_MPP_ACTIVITY, PREFER_MPP);

        mWmState.assertVisibility(MPP_ACTIVITY, true);
        mWmState.assertVisibility(POPUP_MPP_ACTIVITY, true);

        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, PREFER_MPP);
    }

    @Test
    public void testNewWindowTurnsOffMinimalPostProcessing() throws Exception {
        launchMppActivity(MPP_ACTIVITY, PREFER_MPP);
        launchMppActivity(MPP_ACTIVITY2, NOT_PREFER_MPP);

        mWmState.waitAndAssertVisibilityGone(MPP_ACTIVITY);
        mWmState.assertVisibility(MPP_ACTIVITY2, true);

        assertDisplayRequestedMinimalPostProcessing(MPP_ACTIVITY, NOT_PREFER_MPP);
    }
}
