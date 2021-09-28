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
 * limitations under the License.
 */

package com.google.android.lockscreenautomation;

import org.junit.Assert;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.provider.Settings;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.BySelector;
import androidx.test.uiautomator.UiAutomatorTestCase;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;
import androidx.test.uiautomator.Until;
import android.view.KeyEvent;

/**
 * Methods for configuring lock screen settings
 */
public class LockScreenAutomation extends UiAutomatorTestCase {

    private static final String SETTINGS_PACKAGE = "com.android.settings";

    private static final long TIMEOUT = 2000L;

    private Context mContext;
    private UiDevice mDevice;

    public void setPin() throws Exception {
        mContext = getInstrumentation().getContext();
        mDevice = UiDevice.getInstance(getInstrumentation());

        mDevice.wakeUp();
        mDevice.pressKeyCode(KeyEvent.KEYCODE_MENU);
        mDevice.waitForIdle(TIMEOUT);
        launchLockScreenSettings();

        PackageManager pm = mContext.getPackageManager();
        Resources res = pm.getResourcesForApplication(SETTINGS_PACKAGE);

        int resId = res.getIdentifier("unlock_set_unlock_pin_title", "string", SETTINGS_PACKAGE);
        findAndClick(By.text(res.getString(resId)));
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressEnter();
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);

        // Re-enter PIN
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressEnter();

        findAndClick(By.res(SETTINGS_PACKAGE, "redact_sensitive"));
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        findAndClick(By.clazz("android.widget.Button"));
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
    }

    public void unlock() throws Exception {
        mContext = getInstrumentation().getContext();
        mDevice = UiDevice.getInstance(getInstrumentation());
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_ENTER);
    }

    public void removePin() throws Exception {
        mContext = getInstrumentation().getContext();
        mDevice = UiDevice.getInstance(getInstrumentation());

        mDevice.wakeUp();
        mDevice.pressKeyCode(KeyEvent.KEYCODE_MENU);
        mDevice.waitForIdle(TIMEOUT);
        launchLockScreenSettings();

        PackageManager pm = mContext.getPackageManager();
        Resources res = pm.getResourcesForApplication(SETTINGS_PACKAGE);

        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressKeyCode(KeyEvent.KEYCODE_0);
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);
        mDevice.pressEnter();
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);

        int resId = res.getIdentifier("unlock_set_unlock_off_title", "string", SETTINGS_PACKAGE);
        findAndClick(By.text(res.getString(resId)));
        mDevice.waitForWindowUpdate(SETTINGS_PACKAGE, 5);

        findAndClick(By.res("android", "button1"));
        mDevice.waitForIdle(TIMEOUT);
    }

    private void findAndClick(BySelector selector)
    {
        for (int i = 0; i < 3; i++) {
            mDevice.wait(Until.findObject(selector), TIMEOUT);
            UiObject2 obj = mDevice.findObject(selector);
            if (obj != null) {
                obj.click();
                return;
            }
        }
        Assert.fail("Could not find and click " + selector);
    }

    private void launchLockScreenSettings() {
        final Intent intent = new Intent().setClassName(SETTINGS_PACKAGE, "com.android.settings.password.ChooseLockGeneric");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        mContext.startActivity(intent);
        mDevice.wait(Until.hasObject(By.pkg(SETTINGS_PACKAGE).depth(0)), TIMEOUT);
    }
}
