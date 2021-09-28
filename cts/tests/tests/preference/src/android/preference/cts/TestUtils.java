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
 * limitations under the License.
 */

package android.preference.cts;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.app.UiModeManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.util.DisplayMetrics;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

/**
 * Collection of helper utils for testing preferences.
 */
public class TestUtils {

    public final UiDevice mDevice;

    private final Context mContext;
    private final Instrumentation mInstrumentation;
    private final String mPackageName;
    private final UiAutomation mAutomation;
    private int mStatusBarHeight = -1;
    private int mNavigationBarHeight = -1;
    private ActivityTestRule<?> mRule;

    TestUtils(ActivityTestRule<?> rule) {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        mPackageName = mContext.getPackageName();
        mDevice = UiDevice.getInstance(mInstrumentation);
        mAutomation = mInstrumentation.getUiAutomation();
        mRule = rule;
    }

    void waitForIdle() {
        mDevice.waitForIdle(1000);
    }

    Bitmap takeScreenshot() {
        // Only take a screenshot once the screen is stable enough.
        waitForIdle();

        Bitmap bt = mAutomation.takeScreenshot();

        // Crop-out the status bar to avoid flakiness with changing notifications / time.
        int statusBarHeight = getStatusBarHeight();

        // Crop-out the navigation bar to avoid flakiness with button animations.
        int navigationBarHeight = getNavigationBarHeight();

        // Crop-out the right side for the scrollbar which may or may not be visible.
        // On wearable devices the scroll bar is a curve and occupies 20% of the right side.
        int xToCut = isOnWatchUiMode() ? bt.getWidth() / 5 : bt.getWidth() / 20;
        int yToCut = statusBarHeight;

        if (hasVerticalNavBar()) {
            xToCut += navigationBarHeight;
        } else {
            yToCut += navigationBarHeight;
        }

        bt = Bitmap.createBitmap(
            bt, 0, statusBarHeight, bt.getWidth() - xToCut, bt.getHeight() - yToCut);

        return bt;
    }

    void tapOnViewWithText(String text) {
        UiObject2 object2 = getTextObject(text);
        if (object2 != null) {
            object2.click();
        } else {
            scrollToAndGetTextObject(text);
            getTextObject(text).click();
        }
        waitForIdle();
    }

    boolean isTextShown(String text) {
        if (getTextObject(text) != null) {
            return true;
        }

        return scrollToAndGetTextObject(text);
    }

    boolean isTextHidden(String text) {
        return getTextObject(text) == null;
    }

    boolean isTextFocused(String text) {
        UiObject2 object = getTextObject(text);
        return object != null && object.isFocused();
    }

    boolean isOnWatchUiMode() {
        UiModeManager uiModeManager = mContext.getSystemService(UiModeManager.class);
        return uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_WATCH;
    }

    private int getStatusBarHeight() {
        // Cache the result to keep it fast.
        if (mStatusBarHeight >= 0) {
            return mStatusBarHeight;
        }

        int resourceId = mInstrumentation.getTargetContext().getResources()
                .getIdentifier("status_bar_height", "dimen", "android");
        if (resourceId > 0) {
            mStatusBarHeight = mInstrumentation.getTargetContext().getResources()
                    .getDimensionPixelSize(resourceId);
        } else {
            mStatusBarHeight = 0;
        }
        return mStatusBarHeight;
    }

    private int getNavigationBarHeight() {
        // Cache the result to keep it fast.
        if (mNavigationBarHeight >= 0) {
            return mNavigationBarHeight;
        }

        int resourceId = mInstrumentation.getTargetContext().getResources()
                .getIdentifier("navigation_bar_height", "dimen", "android");
        if (resourceId > 0) {
            mNavigationBarHeight = mInstrumentation.getTargetContext().getResources()
                    .getDimensionPixelSize(resourceId);
        } else {
            mNavigationBarHeight = 0;
        }
        return mNavigationBarHeight;
    }

    private boolean hasVerticalNavBar() {
        Rect displayFrame = new Rect();
        final Activity activity = mRule.getActivity();
        activity.getWindow().getDecorView().getWindowVisibleDisplayFrame(displayFrame);
        DisplayMetrics dm = new DisplayMetrics();
        activity.getDisplay().getRealMetrics(dm);
        return dm.heightPixels == displayFrame.bottom;
    }

    private UiObject2 getTextObject(String text) {
        // Wait for up to 1 second to find the object. Returns null if the object cannot be found.
        return mDevice.wait(Until.findObject(By.text(text).pkg(mPackageName)), 1000);
    }

    private boolean scrollToAndGetTextObject(String text) {
        UiScrollable scroller = new UiScrollable(new UiSelector().scrollable(true));
        try {
            // Swipe far away from the edges to avoid triggering navigation gestures
            scroller.setSwipeDeadZonePercentage(0.25);
            return scroller.scrollTextIntoView(text);
        } catch (UiObjectNotFoundException e) {
            throw new AssertionError("View with text '" + text + "' was not found!", e);
        }
    }
}
