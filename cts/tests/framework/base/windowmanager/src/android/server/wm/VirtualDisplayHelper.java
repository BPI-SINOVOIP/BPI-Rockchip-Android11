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

import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY;
import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION;
import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC;
import static android.server.wm.ActivityManagerTestBase.isDisplayOn;
import static android.server.wm.ActivityManagerTestBase.waitForOrFail;
import static android.server.wm.StateLogger.logAlways;
import static android.view.Display.DEFAULT_DISPLAY;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.ImageReader;

import com.android.compatibility.common.util.SystemUtil;

/**
 * Helper class to create virtual display.
 */
class VirtualDisplayHelper {

    private boolean mPublicDisplay = false;
    private boolean mCanShowWithInsecureKeyguard = false;
    private boolean mShowSystemDecorations = false;

    private static final String VIRTUAL_DISPLAY_NAME = "CtsVirtualDisplay";
    /** See {@link DisplayManager#VIRTUAL_DISPLAY_FLAG_CAN_SHOW_WITH_INSECURE_KEYGUARD}. */
    private static final int VIRTUAL_DISPLAY_FLAG_CAN_SHOW_WITH_INSECURE_KEYGUARD = 1 << 5;
    /** See {@link DisplayManager#VIRTUAL_DISPLAY_FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS}. */
    private static final int VIRTUAL_DISPLAY_FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS = 1 << 9;

    private static final int DENSITY = 160;
    static final int HEIGHT = 480;
    static final int WIDTH = 800;

    private ImageReader mReader;
    private VirtualDisplay mVirtualDisplay;
    private boolean mCreated;

    VirtualDisplayHelper setPublicDisplay(boolean publicDisplay) {
        mPublicDisplay = publicDisplay;
        return this;
    }

    VirtualDisplayHelper setCanShowWithInsecureKeyguard(boolean canShowWithInsecureKeyguard) {
        mCanShowWithInsecureKeyguard = canShowWithInsecureKeyguard;
        return this;
    }

    VirtualDisplayHelper setShowSystemDecorations(boolean showSystemDecorations) {
        mShowSystemDecorations = showSystemDecorations;
        return this;
    }

    int createAndWaitForDisplay() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            createVirtualDisplay();
            waitForDisplayState(mVirtualDisplay.getDisplay().getDisplayId(), true /* wantOn */);
            mCreated = true;
        });
        return mVirtualDisplay.getDisplay().getDisplayId();
    }

    void turnDisplayOff() {
        mVirtualDisplay.setSurface(null);
        waitForDisplayState(mVirtualDisplay.getDisplay().getDisplayId(), false /* wantOn */);
    }

    void turnDisplayOn() {
        mVirtualDisplay.setSurface(mReader.getSurface());
        waitForDisplayState(mVirtualDisplay.getDisplay().getDisplayId(), true /* wantOn */);
    }

    void releaseDisplay() {
        if (mCreated) {
            mVirtualDisplay.release();
            mReader.close();
            waitForDisplayState(mVirtualDisplay.getDisplay().getDisplayId(), false /* wantOn */);
        }
        mCreated = false;
    }

    private void createVirtualDisplay() {
        mReader = ImageReader.newInstance(WIDTH, HEIGHT, PixelFormat.RGBA_8888, 2);

        final DisplayManager displayManager = getInstrumentation()
                .getContext().getSystemService(DisplayManager.class);

        int flags = VIRTUAL_DISPLAY_FLAG_PRESENTATION | VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY;

        if (mPublicDisplay) {
            flags |= VIRTUAL_DISPLAY_FLAG_PUBLIC;
        }
        if (mCanShowWithInsecureKeyguard) {
            flags |= VIRTUAL_DISPLAY_FLAG_CAN_SHOW_WITH_INSECURE_KEYGUARD;
        }
        if (mShowSystemDecorations) {
            flags |= VIRTUAL_DISPLAY_FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS;
        }

        logAlways("createVirtualDisplay: " + WIDTH + "x" + HEIGHT + ", dpi: " + DENSITY
                + ", publicDisplay=" + mPublicDisplay
                + ", canShowWithInsecureKeyguard=" + mCanShowWithInsecureKeyguard
                + ", showSystemDecorations=" + mShowSystemDecorations);

        mVirtualDisplay = displayManager.createVirtualDisplay(
                VIRTUAL_DISPLAY_NAME, WIDTH, HEIGHT, DENSITY, mReader.getSurface(), flags);
    }

    static void waitForDefaultDisplayState(boolean wantOn) {
        waitForDisplayState(DEFAULT_DISPLAY, wantOn);
    }

    private static void waitForDisplayState(int displayId, boolean wantOn) {
        final String message = (displayId == DEFAULT_DISPLAY ? "default" : "virtual")
                + " display " + (wantOn ? "on" : "off");
        waitForOrFail(message, () -> isDisplayOn(displayId) == wantOn);
    }
}
