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

package com.android.uibench.microbenchmark;

import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.helpers.AbstractStandardAppHelper;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.widget.ListView;

import junit.framework.Assert;

public class UiBenchJankHelper extends AbstractStandardAppHelper implements IUiBenchJankHelper {
    public static final int LONG_TIMEOUT = 5000;
    public static final int FULL_TEST_DURATION = 25000;
    public static final int FIND_OBJECT_TIMEOUT = 250;
    public static final int SHORT_TIMEOUT = 2000;
    public static final int EXPECTED_FRAMES = 100;
    public static final int KEY_DELAY = 1000;
    public static final String EXTRA_BITMAP_UPLOAD = "extra_bitmap_upload";
    public static final String EXTRA_SHOW_FAST_LANE = "extra_show_fast_lane";

    /**
     * Only to be used for initial-fling tests, or similar cases where perf during brief experience
     * is important.
     */
    public static final int SHORT_EXPECTED_FRAMES = 30;

    public static final String PACKAGE_NAME = "com.android.test.uibench";

    public static final String APP_LAUNCHER_NAME = "UiBench";

    private static final int SLOW_FLING_SPEED = 3000; // compare to UiObject2#DEFAULT_FLING_SPEED

    // Main UiObject2 exercised by the test.
    private UiObject2 mContents, mNavigation;

    public UiBenchJankHelper(Instrumentation instr) {
        super(instr);
    }

    /** {@inheritDoc} */
    @Override
    public String getPackage() {
        return PACKAGE_NAME;
    }

    /** {@inheritDoc} */
    @Override
    public String getLauncherName() {
        return APP_LAUNCHER_NAME;
    }

    /** {@inheritDoc} */
    @Override
    public void dismissInitialDialogs() {}

    /** Launch activity using intent */
    void launchActivity(String activityName, Bundle extras, String verifyText) {
        ComponentName cn =
                new ComponentName(PACKAGE_NAME, String.format("%s.%s", PACKAGE_NAME, activityName));
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        if (extras != null) {
            intent.putExtras(extras);
        }
        intent.setComponent(cn);
        // Launch the activity
        mInstrumentation.getContext().startActivity(intent);
        UiObject2 expectedTextCmp =
                mDevice.wait(Until.findObject(By.text(verifyText)), LONG_TIMEOUT);
        Assert.assertNotNull(String.format("Issue in opening %s", activityName), expectedTextCmp);
    }

    void launchActivity(String activityName, String verifyText) {
        launchActivity(activityName, null, verifyText);
    }

    void launchActivityAndAssert(String activityName, String verifyText) {
        launchActivity(activityName, verifyText);
        mContents =
                mDevice.wait(Until.findObject(By.res("android", "content")), FIND_OBJECT_TIMEOUT);
        Assert.assertNotNull(activityName + " isn't found", mContents);
    }

    int getEdgeSensitivity() {
        int resId =
                mInstrumentation
                        .getContext()
                        .getResources()
                        .getIdentifier("config_backGestureInset", "dimen", "android");
        return mInstrumentation.getContext().getResources().getDimensionPixelSize(resId) + 1;
    }

    /** To perform the fling down and up on given content for flingCount number of times */
    @Override
    public void flingUpDown(int flingCount) {
        flingUpDown(flingCount, false);
    }

    @Override
    public void flingDownUp(int flingCount) {
        flingUpDown(flingCount, true);
    }

    void flingUpDown(int flingCount, boolean reverse) {
        mContents.setGestureMargin(getEdgeSensitivity());
        for (int count = 0; count < flingCount; count++) {
            SystemClock.sleep(SHORT_TIMEOUT);
            mContents.fling(reverse ? Direction.UP : Direction.DOWN);
            SystemClock.sleep(SHORT_TIMEOUT);
            mContents.fling(reverse ? Direction.DOWN : Direction.UP);
        }
    }

    /** To perform the swipe right and left on given content for swipeCount number of times */
    @Override
    public void swipeRightLeft(int swipeCount) {
        mNavigation =
                mDevice.wait(
                        Until.findObject(By.desc("Open navigation drawer")), FIND_OBJECT_TIMEOUT);
        mContents.setGestureMargin(getEdgeSensitivity());
        for (int count = 0; count < swipeCount; count++) {
            SystemClock.sleep(SHORT_TIMEOUT);
            mNavigation.click();
            SystemClock.sleep(SHORT_TIMEOUT);
            mContents.swipe(Direction.LEFT, 1);
        }
    }

    @Override
    public void slowSingleFlingDown() {
        SystemClock.sleep(SHORT_TIMEOUT);
        Context context = mInstrumentation.getContext();
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        mContents.setGestureMargin(getEdgeSensitivity());
        mContents.fling(Direction.DOWN, (int) (SLOW_FLING_SPEED * displayMetrics.density));
        mDevice.waitForIdle();
    }

    @Override
    public void openDialogList() {
        launchActivity("DialogListActivity", "Dialog");
        mContents = mDevice.wait(Until.findObject(By.clazz(ListView.class)), FIND_OBJECT_TIMEOUT);
        Assert.assertNotNull("Dialog List View isn't found", mContents);
    }

    @Override
    public void openFullscreenOverdraw() {
        launchActivity("FullscreenOverdrawActivity", "General/Fullscreen Overdraw");
    }

    @Override
    public void openGLTextureView() {
        launchActivity("GlTextureViewActivity", "General/GL TextureView");
    }

    @Override
    public void openInvalidate() {
        launchActivity("InvalidateActivity", "General/Invalidate");
    }

    @Override
    public void openInvalidateTree() {
        launchActivity("InvalidateTreeActivity", "General/Invalidate Tree");
    }

    @Override
    public void openTrivialAnimation() {
        launchActivity("TrivialAnimationActivity", "General/Trivial Animation");
    }

    @Override
    public void openTrivialListView() {
        launchActivityAndAssert("TrivialListActivity", "General/Trivial ListView");
    }

    @Override
    public void openFadingEdgeListView() {
        launchActivityAndAssert("FadingEdgeListActivity", "General/Fading Edge ListView");
    }

    @Override
    public void openSaveLayerInterleaveActivity() {
        launchActivityAndAssert("SaveLayerInterleaveActivity", "General/SaveLayer Animation");
    }

    @Override
    public void openTrivialRecyclerView() {
        launchActivityAndAssert("TrivialRecyclerViewActivity", "General/Trivial RecyclerView");
    }

    @Override
    public void openSlowBindRecyclerView() {
        launchActivityAndAssert("SlowBindRecyclerViewActivity", "General/Slow Bind RecyclerView");
    }

    @Override
    public void openSlowNestedRecyclerView() {
        launchActivityAndAssert(
                "SlowNestedRecyclerViewActivity", "General/Slow Nested RecyclerView");
    }

    @Override
    public void openInflatingListView() {
        launchActivityAndAssert("InflatingListActivity", "Inflation/Inflating ListView");
    }

    @Override
    public void openInflatingEmojiListView() {
        launchActivityAndAssert(
                "InflatingEmojiListActivity", "Inflation/Inflating ListView with Emoji");
    }

    @Override
    public void openInflatingHanListView() {
        launchActivityAndAssert(
                "InflatingHanListActivity", "Inflation/Inflating ListView with Han Characters");
    }

    @Override
    public void openInflatingLongStringListView() {
        launchActivityAndAssert(
                "InflatingLongStringListActivity", "Inflation/Inflating ListView with long string");
    }

    @Override
    public void openNavigationDrawerActivity() {
        launchActivityAndAssert("NavigationDrawerActivity", "Navigation Drawer Activity");
        mContents.setGestureMargins(0, 0, 10, 0);
    }

    @Override
    public void openNotificationShade() {
        launchActivityAndAssert("NotificationShadeActivity", "Notification Shade");
    }

    @Override
    public void openResizeHWLayer() {
        launchActivity("ResizeHWLayerActivity", "General/Resize HW Layer");
    }

    @Override
    public void openClippedListView() {
        launchActivityAndAssert("ClippedListActivity", "General/Clipped ListView");
    }

    @Override
    public void openLeanbackActivity(
            boolean extraBitmapUpload,
            boolean extraShowFastLane,
            String activityName,
            String expectedText) {
        Bundle extrasBundle = new Bundle();
        extrasBundle.putBoolean(EXTRA_BITMAP_UPLOAD, extraBitmapUpload);
        extrasBundle.putBoolean(EXTRA_SHOW_FAST_LANE, extraShowFastLane);
        launchActivity(activityName, extrasBundle, expectedText);
    }

    void pressKeyCode(int keyCode) {
        SystemClock.sleep(KEY_DELAY);
        mDevice.pressKeyCode(keyCode);
    }

    @Override
    public void scrollDownAndUp(int count) {
        for (int i = 0; i < count; i++) {
            pressKeyCode(KeyEvent.KEYCODE_DPAD_DOWN);
        }
        for (int i = 0; i < count; i++) {
            pressKeyCode(KeyEvent.KEYCODE_DPAD_UP);
        }
    }

    // Open Bitmap Upload
    @Override
    public void openBitmapUpload() {
        launchActivity("BitmapUploadActivity", "Rendering/Bitmap Upload");
    }

    // Open Shadow Grid
    @Override
    public void openRenderingList() {
        launchActivity("ShadowGridActivity", "Rendering/Shadow Grid");
        mContents = mDevice.wait(Until.findObject(By.clazz(ListView.class)), FIND_OBJECT_TIMEOUT);
        Assert.assertNotNull("Shadow Grid list isn't found", mContents);
    }

    // Open EditText Typing
    @Override
    public void openEditTextTyping() {
        launchActivity("EditTextTypeActivity", "Text/EditText Typing");
    }

    // Open Layout Cache High Hitrate
    @Override
    public void openLayoutCacheHighHitrate() {
        launchActivity("TextCacheHighHitrateActivity", "Text/Layout Cache High Hitrate");
        mContents = mDevice.wait(Until.findObject(By.clazz(ListView.class)), FIND_OBJECT_TIMEOUT);
        Assert.assertNotNull("LayoutCacheHighHitrateContents isn't found", mContents);
    }

    // Open Layout Cache Low Hitrate
    @Override
    public void openLayoutCacheLowHitrate() {
        launchActivity("TextCacheLowHitrateActivity", "Text/Layout Cache Low Hitrate");
        mContents = mDevice.wait(Until.findObject(By.clazz(ListView.class)), FIND_OBJECT_TIMEOUT);
        Assert.assertNotNull("LayoutCacheLowHitrateContents isn't found", mContents);
    }

    // Open Transitions
    @Override
    public void openActivityTransition() {
        launchActivity("ActivityTransition", "Transitions/Activity Transition");
    }

    @Override
    public void openWindowInsetsController() {
        launchActivityAndAssert("WindowInsetsControllerActivity", "WindowInsetsControllerActivity");
    }

    // Get the image to click
    @Override
    public void clickImage(String imageName) {
        UiObject2 image =
                mDevice.wait(
                        Until.findObject(By.res(PACKAGE_NAME, imageName)), FIND_OBJECT_TIMEOUT);
        Assert.assertNotNull(imageName + "Image not found", image);
        image.clickAndWait(Until.newWindow(), FIND_OBJECT_TIMEOUT);
        mDevice.pressBack();
    }

    // Open Scrollable WebView from WebView test
    @Override
    public void openScrollableWebView() {
        launchActivity("ScrollableWebViewActivity", "WebView/Scrollable WebView");
        mContents =
                mDevice.wait(Until.findObject(By.res("android", "content")), FIND_OBJECT_TIMEOUT);
    }
}
