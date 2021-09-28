/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.helpers;

import android.app.KeyguardManager;
import android.content.Context;
import android.os.SystemClock;
import android.hardware.display.DisplayManager;
import android.support.test.uiautomator.UiDevice;
import android.view.Display;
import androidx.test.InstrumentationRegistry;

import java.io.IOException;

public class HelperTestUtility {

    // Clear the cache.
    private static final String CLEAR_CACHE_CMD = "echo 3 > /proc/sys/vm/drop_caches";
    // Dismiss the lockscreen.
    private static final String UNLOCK_CMD = "wm dismiss-keyguard";
    // Input a keycode.
    private static final String KEYEVENT_CMD_TEMPLATE = "input keyevent %s";
    // Launch an app.
    private static final String LAUNCH_APP_CMD_TEMPLATE =
            "monkey -p %s -c android.intent.category.LAUNCHER 1";
    // Keycode(s) for convenience functions.
    private static final String KEYCODE_WAKEUP = "KEYCODE_WAKEUP";
    // Delay between actions happening in the device.
    public static final int ACTION_DELAY = 2000;

    private static UiDevice mDevice;

    /**
     * Returns the active {@link UiDevice} to interact with.
     */
    public static UiDevice getUiDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }
        return mDevice;
    }

    /**
     * Runs a shell command, {@code cmd}, and returns the output.
     */
    public static String executeShellCommand(String cmd) {
        try {
            return getUiDevice().executeShellCommand(cmd);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Clears the cache.
     */
    public static void clearCache() {
        SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
        executeShellCommand(CLEAR_CACHE_CMD);
        SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
    }

    /**
     * Close the test app and clear the cache.
     * TODO: Replace it with the rules.
     */
    public static void clearApp(String killCmd) {
        SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
        executeShellCommand(killCmd);
        clearCache();
    }

    /**
     * Send a keycode via adb.
     */
    public static void sendKeyCode(String keyCode) {
        executeShellCommand(String.format(KEYEVENT_CMD_TEMPLATE, keyCode));
        SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
    }

    /**
     * Launch an app via adb.
     */
    public static void launchPackageViaAdb(String pkgName) {
        executeShellCommand(String.format(LAUNCH_APP_CMD_TEMPLATE, pkgName));
        SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
    }

    /**
     * Wake up and unlock the device if the device is not in the unlocked state already.
     */
    public static void wakeUpAndUnlock() {
        // Check whether display is on using DisplayManager.
        Context context = InstrumentationRegistry.getContext();
        DisplayManager displayManager =
                (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
        boolean displayIsOn = (displayManager.getDisplays()[0].getState() == Display.STATE_ON);
        // Check whether lockscreen is dismissed using KeyguardManager.
        KeyguardManager keyguardManager =
                (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        boolean displayIsUnlocked = (!keyguardManager.isKeyguardLocked());
        if (!(displayIsOn && displayIsUnlocked)) {
            sendKeyCode(KEYCODE_WAKEUP);
            executeShellCommand(UNLOCK_CMD);
            SystemClock.sleep(HelperTestUtility.ACTION_DELAY);
        }
    }
}

