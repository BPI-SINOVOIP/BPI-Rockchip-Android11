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

import static android.server.wm.ComponentNameUtils.getActivityName;
import static android.server.wm.app.Components.ALT_LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.server.wm.app.Components.RESIZEABLE_ACTIVITY;
import static android.server.wm.app.Components.VR_TEST_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.Presubmit;
import android.provider.Settings;
import android.server.wm.WindowManagerState.DisplayContent;
import android.server.wm.settings.SettingsSession;

import com.android.cts.verifier.vr.MockVrListenerService;

import org.junit.Before;
import org.junit.Test;

import java.util.List;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:VrDisplayTests
 */
@Presubmit
@FlakyTest
@android.server.wm.annotation.Group3
public class VrDisplayTests extends MultiDisplayTestBase {
    private static final int VR_VIRTUAL_DISPLAY_WIDTH = 700;
    private static final int VR_VIRTUAL_DISPLAY_HEIGHT = 900;
    private static final int VR_VIRTUAL_DISPLAY_DPI = 320;


    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        assumeTrue(supportsVrMode());
    }

    /**
     * VrModeSession is used to enable or disable persistent vr mode and the vr virtual display. For
     * standalone vr devices, VrModeSession has no effect, because the device is already in
     * persistent vr mode whenever it's on, and turning off persistent vr mode on a standalone vr
     * device puts the device in a bad state.
     */
    private static class VrModeSession implements AutoCloseable {
        private boolean applyVrModeChanges = !ActivityManagerTestBase.isUiModeLockedToVrHeadset();

        void enablePersistentVrMode() {
            if (!applyVrModeChanges) { return; }
            executeShellCommand("setprop vr_virtualdisplay true");
            executeShellCommand("vr set-persistent-vr-mode-enabled true");
        }

        @Override
        public void close() {
            if (!applyVrModeChanges) { return; }
            executeShellCommand("vr set-persistent-vr-mode-enabled false");
            executeShellCommand("setprop vr_virtualdisplay false");
        }
    }

    /**
     * Helper class to enable vr listener.
     * VrManagerService uses SettingChangeListener to monitor ENABLED_VR_LISTENERS changed.
     * We need to update Settings to let Vr service know if MockVrListenerService is enabled.
     */
    private static class EnableVrListenerSession extends SettingsSession<String> {
        public EnableVrListenerSession() {
            super(Settings.Secure.getUriFor(Settings.Secure.ENABLED_VR_LISTENERS),
                    Settings.Secure::getString,
                    Settings.Secure::putString);
        }

        public void enableVrListener(ComponentName targetVrComponent) {
            ComponentName component = new ComponentName(targetVrComponent.getPackageName(),
                    MockVrListenerService.class.getName());
            set(component.flattenToString());
        }
    }

    /**
     * Tests that any new activity launch in Vr mode is in Vr display.
     */
    @Test
    public void testVrActivityLaunch() {
        assumeTrue(supportsMultiDisplay());

        final VrModeSession vrModeSession = mObjectTracker.manage(new VrModeSession());
        final EnableVrListenerSession enableVrListenerSession =
                mObjectTracker.manage(new EnableVrListenerSession());

        // Put the device in persistent vr mode.
        vrModeSession.enablePersistentVrMode();
        enableVrListenerSession.enableVrListener(VR_TEST_ACTIVITY);

        // Launch the VR activity.
        launchActivity(VR_TEST_ACTIVITY);
        mWmState.computeState(VR_TEST_ACTIVITY);
        mWmState.assertVisibility(VR_TEST_ACTIVITY, true /* visible */);

        // Launch the non-VR 2D activity and check where it ends up.
        launchActivity(LAUNCHING_ACTIVITY);
        mWmState.computeState(LAUNCHING_ACTIVITY);

        // Ensure that the subsequent activity is visible
        mWmState.assertVisibility(LAUNCHING_ACTIVITY, true /* visible */);

        // Check that activity is launched in focused stack on primary display.
        mWmState.assertFocusedActivity("Launched activity must be focused",
                LAUNCHING_ACTIVITY);
        final int focusedStackId = mWmState.getFocusedStackId();
        final WindowManagerState.ActivityTask focusedStack
                = mWmState.getRootTask(focusedStackId);
        assertEquals("Launched activity must be resumed in focused stack",
                getActivityName(LAUNCHING_ACTIVITY), focusedStack.mResumedActivity);

        // Check if the launch activity is in Vr virtual display id.
        final List<DisplayContent> reportedDisplays = getDisplaysStates();
        final DisplayContent vrDisplay = getDisplayState(reportedDisplays,
                VR_VIRTUAL_DISPLAY_WIDTH, VR_VIRTUAL_DISPLAY_HEIGHT, VR_VIRTUAL_DISPLAY_DPI);
        assertNotNull("Vr mode should have a virtual display", vrDisplay);

        // Check if the focused activity is on this virtual stack.
        assertEquals("Launch in Vr mode should be in virtual stack", vrDisplay.mId,
                focusedStack.mDisplayId);
    }

    /**
     * Tests that any activity already present is re-launched in Vr display in vr mode.
     */
    @Test
    public void testVrActivityReLaunch() {
        assumeTrue(supportsMultiDisplay());

        // Launch a 2D activity.
        launchActivity(LAUNCHING_ACTIVITY);

        final VrModeSession vrModeSession = mObjectTracker.manage(new VrModeSession());
        final EnableVrListenerSession enableVrListenerSession =
                mObjectTracker.manage(new EnableVrListenerSession());

        // Put the device in persistent vr mode.
        vrModeSession.enablePersistentVrMode();
        enableVrListenerSession.enableVrListener(VR_TEST_ACTIVITY);

        // Launch the VR activity.
        launchActivity(VR_TEST_ACTIVITY);
        mWmState.computeState(VR_TEST_ACTIVITY);
        mWmState.assertVisibility(VR_TEST_ACTIVITY, true /* visible */);

        // Re-launch the non-VR 2D activity and check where it ends up.
        launchActivity(LAUNCHING_ACTIVITY);
        mWmState.computeState(LAUNCHING_ACTIVITY);

        // Ensure that the subsequent activity is visible
        mWmState.assertVisibility(LAUNCHING_ACTIVITY, true /* visible */);

        // Check that activity is launched in focused stack on primary display.
        mWmState.assertFocusedActivity("Launched activity must be focused",
                LAUNCHING_ACTIVITY);
        final int focusedStackId = mWmState.getFocusedStackId();
        final WindowManagerState.ActivityTask focusedStack
                = mWmState.getRootTask(focusedStackId);
        assertEquals("Launched activity must be resumed in focused stack",
                getActivityName(LAUNCHING_ACTIVITY), focusedStack.mResumedActivity);

        // Check if the launch activity is in Vr virtual display id.
        final List<DisplayContent> reportedDisplays = getDisplaysStates();
        final DisplayContent vrDisplay = getDisplayState(reportedDisplays,
                VR_VIRTUAL_DISPLAY_WIDTH, VR_VIRTUAL_DISPLAY_HEIGHT, VR_VIRTUAL_DISPLAY_DPI);
        assertNotNull("Vr mode should have a virtual display", vrDisplay);

        // Check if the focused activity is on this virtual stack.
        assertEquals("Launch in Vr mode should be in virtual stack", vrDisplay.mId,
                focusedStack.mDisplayId);
    }

    /**
     * Tests that any new activity launch post Vr mode is in the main display.
     */
    @Test
    public void testActivityLaunchPostVr() throws Exception {
        assumeTrue(supportsMultiDisplay());
        // This test doesn't apply to a standalone vr device, since vr is always enabled, and
        // there is no "post vr" behavior to verify.
        assumeFalse(isUiModeLockedToVrHeadset());

        try (final VrModeSession vrModeSession = new VrModeSession();
             final EnableVrListenerSession enableVrListenerSession =
                     new EnableVrListenerSession()) {
            // Put the device in persistent vr mode.
            vrModeSession.enablePersistentVrMode();
            enableVrListenerSession.enableVrListener(VR_TEST_ACTIVITY);

            // Launch the VR activity.
            launchActivity(VR_TEST_ACTIVITY);
            mWmState.computeState(VR_TEST_ACTIVITY);
            mWmState.assertVisibility(VR_TEST_ACTIVITY, true /* visible */);

            // Launch the non-VR 2D activity and check where it ends up.
            launchActivity(ALT_LAUNCHING_ACTIVITY);
            mWmState.computeState(ALT_LAUNCHING_ACTIVITY);

            // Ensure that the subsequent activity is visible
            mWmState.assertVisibility(ALT_LAUNCHING_ACTIVITY, true /* visible */);

            // Check that activity is launched in focused stack on primary display.
            mWmState.assertFocusedActivity("Launched activity must be focused",
                    ALT_LAUNCHING_ACTIVITY);
            final int focusedStackId = mWmState.getFocusedStackId();
            final WindowManagerState.ActivityTask focusedStack
                    = mWmState.getRootTask(focusedStackId);
            assertEquals("Launched activity must be resumed in focused stack",
                    getActivityName(ALT_LAUNCHING_ACTIVITY),
                    focusedStack.mResumedActivity);

            // Check if the launch activity is in Vr virtual display id.
            final List<DisplayContent> reportedDisplays = getDisplaysStates();
            final DisplayContent vrDisplay = getDisplayState(reportedDisplays,
                    VR_VIRTUAL_DISPLAY_WIDTH, VR_VIRTUAL_DISPLAY_HEIGHT,
                    VR_VIRTUAL_DISPLAY_DPI);
            assertNotNull("Vr mode should have a virtual display", vrDisplay);

            // Check if the focused activity is on this virtual stack.
            assertEquals("Launch in Vr mode should be in virtual stack", vrDisplay.mId,
                    focusedStack.mDisplayId);

        }

        // There isn't a direct launch of activity which can take an user out of persistent VR mode.
        // This sleep is to account for that delay and let device settle once it comes out of VR
        // mode.
        try {
            Thread.sleep(2000);
        } catch (Exception e) {
            e.printStackTrace();
        }

        // Launch the non-VR 2D activity and check where it ends up.
        launchActivity(RESIZEABLE_ACTIVITY);
        mWmState.computeState(RESIZEABLE_ACTIVITY);

        // Ensure that the subsequent activity is visible
        mWmState.assertVisibility(RESIZEABLE_ACTIVITY, true /* visible */);

        // Check that activity is launched in focused stack on primary display.
        mWmState.assertFocusedActivity("Launched activity must be focused", RESIZEABLE_ACTIVITY);
        final int frontStackId = mWmState.getFrontRootTaskId(DEFAULT_DISPLAY);
        final WindowManagerState.ActivityTask frontStack
                = mWmState.getRootTask(frontStackId);
        assertEquals("Launched activity must be resumed in front stack",
                getActivityName(RESIZEABLE_ACTIVITY), frontStack.mResumedActivity);
        assertEquals("Front stack must be on primary display",
                DEFAULT_DISPLAY, frontStack.mDisplayId);
    }
}
