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
 * limitations under the License.
 */

package android.server.wm;

import static android.server.wm.app.Components.TEST_ACTIVITY;
import static android.server.wm.displaysize.Components.SMALLEST_WIDTH_ACTIVITY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Point;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.platform.test.annotations.Presubmit;
import android.view.Display;

import org.junit.After;
import org.junit.Test;

/**
 * Ensure that compatibility dialog is shown when launching an application with
 * an unsupported smallest width.
 *
 * <p>Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:DisplaySizeTest
 */
@Presubmit
@android.server.wm.annotation.Group3
public class DisplaySizeTest extends ActivityManagerTestBase {

    /** @see com.android.server.wm.UnsupportedDisplaySizeDialog */
    private static final String UNSUPPORTED_DISPLAY_SIZE_DIALOG_NAME =
            "UnsupportedDisplaySizeDialog";

    @After
    public void tearDown() {
        // Ensure app process is stopped.
        stopTestPackage(SMALLEST_WIDTH_ACTIVITY.getPackageName());
        stopTestPackage(TEST_ACTIVITY.getPackageName());
    }

    @Test
    public void testCompatibilityDialog() {
        // Launch some other app (not to perform density change on launcher).
        launchActivity(TEST_ACTIVITY);
        mWmState.assertActivityDisplayed(TEST_ACTIVITY);

        createManagedScreenDensitySession().setUnsupportedDensity();

        // Launch target app.
        launchActivity(SMALLEST_WIDTH_ACTIVITY);
        mWmState.assertActivityDisplayed(SMALLEST_WIDTH_ACTIVITY);
        mWmState.assertWindowDisplayed(UNSUPPORTED_DISPLAY_SIZE_DIALOG_NAME);
    }

    @Test
    public void testCompatibilityDialogWhenFocused() {
        launchActivity(SMALLEST_WIDTH_ACTIVITY);
        mWmState.assertActivityDisplayed(SMALLEST_WIDTH_ACTIVITY);

        createManagedScreenDensitySession().setUnsupportedDensity();

        mWmState.assertWindowDisplayed(UNSUPPORTED_DISPLAY_SIZE_DIALOG_NAME);
    }

    @Test
    public void testCompatibilityDialogAfterReturn() {
        // Launch target app.
        launchActivity(SMALLEST_WIDTH_ACTIVITY);
        mWmState.assertActivityDisplayed(SMALLEST_WIDTH_ACTIVITY);
        // Launch another activity.
        final CommandSession.ActivitySession activity = createManagedActivityClientSession()
                .startActivity(getLaunchActivityBuilder().setUseInstrumentation()
                        .setTargetActivity(TEST_ACTIVITY));
        mWmState.assertActivityDisplayed(TEST_ACTIVITY);
        separateTestJournal();

        createManagedScreenDensitySession().setUnsupportedDensity();

        assertActivityLifecycle(TEST_ACTIVITY, true /* relaunched */);
        activity.finish();

        mWmState.assertActivityDisplayed(SMALLEST_WIDTH_ACTIVITY);
        mWmState.assertWindowDisplayed(UNSUPPORTED_DISPLAY_SIZE_DIALOG_NAME);
    }

    @Test
    public void testSizeRangesAfterSettingDisplaySize() throws InterruptedException {
        VirtualDisplay virtualDisplay = null;
        try {
            final int initialLength = 500;
            final int newLength = 1000;
            final DisplayManager displayManager = mDm;
            virtualDisplay = displayManager.createVirtualDisplay("CtsDisplay", initialLength,
                    initialLength, 160 /* densityDpi */, null /* surface */, 0 /* flags */);
            final Display targetDisplay = virtualDisplay.getDisplay();
            final int targetDisplayId = targetDisplay.getDisplayId();
            final boolean[] displayChanged = { false };
            displayManager.registerDisplayListener(new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int displayId) {}

                @Override
                public void onDisplayRemoved(int displayId) {}

                @Override
                public void onDisplayChanged(int displayId) {
                    if (displayId == targetDisplayId) {
                        synchronized (displayManager) {
                            displayChanged[0] = true;
                            displayManager.notify();
                        }
                        displayManager.unregisterDisplayListener(this);
                    }
                }
            }, new Handler(Looper.getMainLooper()));

            executeShellCommand(String.format("wm size %sx%s -d %s",
                    newLength, newLength, targetDisplayId));
            synchronized (displayManager) {
                if (!displayChanged[0]) {
                    displayManager.wait(1000 /* milliseconds */);
                }
            }

            final Point expectedSize = new Point(newLength, newLength);
            final Point smallestSize = new Point();
            final Point largestSize = new Point();
            targetDisplay.getCurrentSizeRange(smallestSize, largestSize);
            assertEquals("Smallest size must be changed.", expectedSize, smallestSize);
            assertEquals("Largest size must be changed.", expectedSize, largestSize);
        } finally {
            if (virtualDisplay != null) {
                virtualDisplay.release();
            }
        }
    }

    protected ScreenDensitySession createManagedScreenDensitySession() {
        return mObjectTracker.manage(new ScreenDensitySession());
    }

    private static class ScreenDensitySession implements AutoCloseable {
        private static final String DENSITY_PROP_DEVICE = "ro.sf.lcd_density";
        private static final String DENSITY_PROP_EMULATOR = "qemu.sf.lcd_density";

        void setUnsupportedDensity() {
            // Set device to 0.85 zoom. It doesn't matter that we're zooming out
            // since the feature verifies that we're in a non-default density.
            final int stableDensity = getStableDensity();
            final int targetDensity = (int) (stableDensity * 0.85);
            setDensity(targetDensity);
        }

        @Override
        public void close() throws Exception {
            resetDensity();
        }

        private int getStableDensity() {
            final String densityProp;
            if (Build.IS_EMULATOR) {
                densityProp = DENSITY_PROP_EMULATOR;
            } else {
                densityProp = DENSITY_PROP_DEVICE;
            }

            return Integer.parseInt(executeShellCommand("getprop " + densityProp).trim());
        }

        private void setDensity(int targetDensity) {
            executeShellCommand("wm density " + targetDensity);

            // Verify that the density is changed.
            final String output = executeShellCommand("wm density");
            final boolean success = output.contains("Override density: " + targetDensity);

            assertTrue("Failed to set density to " + targetDensity, success);
        }

        private void resetDensity() {
            executeShellCommand("wm density reset");
        }
    }
}
