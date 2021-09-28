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

package android.systemui.cts;

import static android.provider.DeviceConfig.NAMESPACE_ANDROID;
import static android.provider.AndroidDeviceConfig.KEY_SYSTEM_GESTURE_EXCLUSION_LIMIT_DP;
import static android.view.View.SYSTEM_UI_CLEARABLE_FLAGS;
import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
import static android.view.View.SYSTEM_UI_FLAG_VISIBLE;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static junit.framework.Assert.assertEquals;
import static junit.framework.TestCase.fail;

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.provider.DeviceConfig;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.util.ArrayMap;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;
import com.android.compatibility.common.util.ThrowingRunnable;

import com.google.common.collect.Lists;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

@RunWith(AndroidJUnit4.class)
public class WindowInsetsBehaviorTests {
    private static final String DEF_SCREENSHOT_BASE_PATH =
            "/sdcard/WindowInsetsBehaviorTests";
    private static final String SETTINGS_PACKAGE_NAME = "com.android.settings";
    private static final String ARGUMENT_KEY_FORCE_ENABLE = "force_enable_gesture_navigation";
    private static final String NAV_BAR_INTERACTION_MODE_RES_NAME = "config_navBarInteractionMode";
    private static final int STEPS = 10;

    // The minimum value of the system gesture exclusion limit is 200 dp. The value here should be
    // greater than that, so that we can test if the limit can be changed by DeviceConfig or not.
    private static final int EXCLUSION_LIMIT_DP = 210;

    private static final int NAV_BAR_INTERACTION_MODE_GESTURAL = 2;

    private final boolean mForceEnableGestureNavigation;
    private final Map<String, Boolean> mSystemGestureOptionsMap;
    private float mPixelsPerDp;
    private float mDensityPerCm;
    private int mDisplayWidth;
    private int mExclusionLimit;
    private UiDevice mDevice;
    private Rect mSwipeBound;
    private String mEdgeToEdgeNavigationTitle;
    private String mSystemNavigationTitle;
    private String mGesturePreferenceTitle;
    private TouchHelper mTouchHelper;
    private boolean mConfiguredInSettings;

    private static String getSettingsString(Resources res, String strResName) {
        int resIdString = res.getIdentifier(strResName, "string", SETTINGS_PACKAGE_NAME);
        if (resIdString <= 0x7f000000) {
            return null; /* most of application res id must be larger than 0x7f000000 */
        }

        return res.getString(resIdString);
    }

    /**
     * To initial all of options in System Gesture.
     */
    public WindowInsetsBehaviorTests() {
        Bundle bundle = InstrumentationRegistry.getArguments();
        mForceEnableGestureNavigation = (bundle != null)
                && "true".equalsIgnoreCase(bundle.getString(ARGUMENT_KEY_FORCE_ENABLE));

        mSystemGestureOptionsMap = new ArrayMap();

        if (!mForceEnableGestureNavigation) {
            return;
        }

        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        PackageManager packageManager = context.getPackageManager();
        Resources res = null;
        try {
            res = packageManager.getResourcesForApplication(SETTINGS_PACKAGE_NAME);
        } catch (PackageManager.NameNotFoundException e) {
            return;
        }
        if (res == null) {
            return;
        }

        mEdgeToEdgeNavigationTitle = getSettingsString(res, "edge_to_edge_navigation_title");
        mGesturePreferenceTitle = getSettingsString(res, "gesture_preference_title");
        mSystemNavigationTitle = getSettingsString(res, "system_navigation_title");

        String text = getSettingsString(res, "edge_to_edge_navigation_title");
        if (text != null) {
            mSystemGestureOptionsMap.put(text, false);
        }
        text = getSettingsString(res, "swipe_up_to_switch_apps_title");
        if (text != null) {
            mSystemGestureOptionsMap.put(text, false);
        }
        text = getSettingsString(res, "legacy_navigation_title");
        if (text != null) {
            mSystemGestureOptionsMap.put(text, false);
        }

        mConfiguredInSettings = false;
    }

    @Rule
    public ScreenshotTestRule mScreenshotTestRule =
            new ScreenshotTestRule(DEF_SCREENSHOT_BASE_PATH);

    @Rule
    public ActivityTestRule<WindowInsetsActivity> mActivityRule = new ActivityTestRule<>(
            WindowInsetsActivity.class, true, false);

    @Rule
    public RuleChain mRuleChain = RuleChain.outerRule(mActivityRule)
            .around(mScreenshotTestRule);

    private WindowInsetsActivity mActivity;
    private WindowInsets mContentViewWindowInsets;
    private List<Point> mActionCancelPoints;
    private List<Point> mActionDownPoints;
    private List<Point> mActionUpPoints;

    private Context mTargetContext;
    private int mClickCount;

    private void mainThreadRun(Runnable runnable) {
        getInstrumentation().runOnMainSync(runnable);
        mDevice.waitForIdle();
    }

    private boolean hasSystemGestureFeature() {
        final PackageManager pm = mTargetContext.getPackageManager();

        // No bars on embedded devices.
        // No bars on TVs and watches.
        return !(pm.hasSystemFeature(PackageManager.FEATURE_WATCH)
                || pm.hasSystemFeature(PackageManager.FEATURE_EMBEDDED)
                || pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
                || pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE));
    }


    private UiObject2 findSystemNavigationObject(String text, boolean addCheckSelector) {
        BySelector widgetFrameSelector = By.res("android", "widget_frame");
        BySelector checkboxSelector = By.checkable(true);
        if (addCheckSelector) {
            checkboxSelector = checkboxSelector.checked(true);
        }
        BySelector textSelector = By.text(text);
        BySelector targetSelector = By.hasChild(widgetFrameSelector).hasDescendant(textSelector)
                .hasDescendant(checkboxSelector);

        return mDevice.findObject(targetSelector);
    }

    private boolean launchToSettingsSystemGesture() {
        if (!mForceEnableGestureNavigation) {
            return false;
        }

        /* launch to the close to the system gesture fragment */
        Intent intent = new Intent(Intent.ACTION_MAIN);
        ComponentName settingComponent = new ComponentName(SETTINGS_PACKAGE_NAME,
                String.format("%s.%s$%s", SETTINGS_PACKAGE_NAME, "Settings",
                        "SystemDashboardActivity"));
        intent.setComponent(settingComponent);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        mTargetContext.startActivity(intent);

        // Wait for the app to appear
        mDevice.wait(Until.hasObject(By.pkg("com.android.settings").depth(0)),
                5000);
        mDevice.wait(Until.hasObject(By.text(mGesturePreferenceTitle)), 5000);
        if (mDevice.findObject(By.text(mGesturePreferenceTitle)) == null) {
            return false;
        }
        mDevice.findObject(By.text(mGesturePreferenceTitle)).click();
        mDevice.wait(Until.hasObject(By.text(mSystemNavigationTitle)), 5000);
        if (mDevice.findObject(By.text(mSystemNavigationTitle)) == null) {
            return false;
        }
        mDevice.findObject(By.text(mSystemNavigationTitle)).click();
        mDevice.wait(Until.hasObject(By.text(mEdgeToEdgeNavigationTitle)), 5000);

        return mDevice.hasObject(By.text(mEdgeToEdgeNavigationTitle));
    }

    private void leaveSettings() {
        mDevice.pressBack(); /* Back to Gesture */
        mDevice.waitForIdle();
        mDevice.pressBack(); /* Back to System */
        mDevice.waitForIdle();
        mDevice.pressBack(); /* back to Settings */
        mDevice.waitForIdle();
        mDevice.pressBack(); /* Back to Home */
        mDevice.waitForIdle();

        mDevice.pressHome(); /* double confirm back to home */
        mDevice.waitForIdle();
    }

    /**
     * To prepare the things needed to run the tests.
     * <p>
     * There are several things needed to prepare
     * * return to home screen
     * * launch the activity
     * * pixel per dp
     * * the WindowInsets that received by the content view of activity
     * </p>
     * @throws Exception caused by permission, nullpointer, etc.
     */
    @Before
    public void setUp() throws Exception {
        mDevice = UiDevice.getInstance(getInstrumentation());
        mTouchHelper = new TouchHelper(getInstrumentation());
        mTargetContext = getInstrumentation().getTargetContext();
        if (!hasSystemGestureFeature()) {
            return;
        }

        final DisplayManager dm = mTargetContext.getSystemService(DisplayManager.class);
        final Display display = dm.getDisplay(Display.DEFAULT_DISPLAY);
        final DisplayMetrics metrics = new DisplayMetrics();
        display.getRealMetrics(metrics);
        mPixelsPerDp = metrics.density;
        mDensityPerCm = (int) ((float) metrics.densityDpi / 2.54);
        mDisplayWidth = metrics.widthPixels;
        mExclusionLimit = (int) (EXCLUSION_LIMIT_DP * mPixelsPerDp);

        // To setup the Edge to Edge environment by do the operation on Settings
        boolean isOperatedSettingsToExpectedOption = launchToSettingsSystemGesture();
        if (isOperatedSettingsToExpectedOption) {
            for (Map.Entry<String, Boolean> entry : mSystemGestureOptionsMap.entrySet()) {
                UiObject2 uiObject2 = findSystemNavigationObject(entry.getKey(), true);
                entry.setValue(uiObject2 != null);
            }
            UiObject2 edgeToEdgeObj = mDevice.findObject(By.text(mEdgeToEdgeNavigationTitle));
            if (edgeToEdgeObj != null) {
                edgeToEdgeObj.click();
                mConfiguredInSettings = true;
            }
        }
        mDevice.waitForIdle();
        leaveSettings();


        mDevice.pressHome();
        mDevice.waitForIdle();

        // launch the Activity and wait until Activity onAttach
        CountDownLatch latch = new CountDownLatch(1);
        mActivity = mActivityRule.launchActivity(null);
        mActivity.setInitialFinishCallBack(isFinish -> latch.countDown());
        mDevice.waitForIdle();

        latch.await(5, SECONDS);
    }

    /**
     * Restore the original configured value for the system gesture by operating Settings.
     */
    @After
    public void tearDown() {
        if (!hasSystemGestureFeature()) {
            return;
        }

        if (mConfiguredInSettings) {
            launchToSettingsSystemGesture();
            for (Map.Entry<String, Boolean> entry : mSystemGestureOptionsMap.entrySet()) {
                if (entry.getValue()) {
                    UiObject2 uiObject2 = findSystemNavigationObject(entry.getKey(), false);
                    if (uiObject2 != null) {
                        uiObject2.click();
                    }
                }
            }
            leaveSettings();
        }
    }


    private void swipeByUiDevice(Point p1, Point p2) {
        mDevice.swipe(p1.x, p1.y, p2.x, p2.y, STEPS);
    }

    private void clickAndWaitByUiDevice(Point p) {
        CountDownLatch latch = new CountDownLatch(1);
        mActivity.setOnClickConsumer((view) -> {
            latch.countDown();
        });
        // mDevice.click(p.x, p.y) has the limitation without consideration of the cutout
        if (!mTouchHelper.click(p.x, p.y)) {
            fail("Can't inject event at" + p);
        }

        /* wait until the OnClickListener triggered, and then click the next point */
        try {
            latch.await(5, SECONDS);
        } catch (InterruptedException e) {
            fail("Wait too long and onClickEvent doesn't receive");
        }

        if (latch.getCount() > 0) {
            fail("Doesn't receive onClickEvent at " + p);
        }
    }

    private int swipeBigX(Rect viewBoundary, BiConsumer<Point, Point> callback) {
        final int theLeftestLine = viewBoundary.left + 1;
        final int theToppestLine = viewBoundary.top + 1;
        final int theRightestLine = viewBoundary.right - 1;
        final int theBottomestLine = viewBoundary.bottom - 1;

        if (callback != null) {
            callback.accept(new Point(theLeftestLine, theToppestLine),
                    new Point(theRightestLine, theBottomestLine));
        }
        mDevice.waitForIdle();

        if (callback != null) {
            callback.accept(new Point(theRightestLine, theToppestLine),
                    new Point(viewBoundary.left, theBottomestLine));
        }
        mDevice.waitForIdle();

        return 2;
    }

    private int clickAllOfHorizontalSamplePoints(Rect viewBoundary, int y,
            Consumer<Point> callback) {
        final int theLeftestLine = viewBoundary.left + 1;
        final int theRightestLine = viewBoundary.right - 1;
        final float interval = mDensityPerCm;

        int count = 0;
        for (int i = theLeftestLine; i < theRightestLine; i += interval) {
            if (callback != null) {
                callback.accept(new Point(i, y));
            }
            mDevice.waitForIdle();
            count++;
        }

        if (callback != null) {
            callback.accept(new Point(theRightestLine, y));
        }
        mDevice.waitForIdle();
        count++;

        return count;
    }

    private int clickAllOfSamplePoints(Rect viewBoundary, Consumer<Point> callback) {
        final int theToppestLine = viewBoundary.top + 1;
        final int theBottomestLine = viewBoundary.bottom - 1;
        final float interval = mDensityPerCm;
        int count = 0;
        for (int i = theToppestLine; i < theBottomestLine; i += interval) {
            count += clickAllOfHorizontalSamplePoints(viewBoundary, i, callback);
        }
        count += clickAllOfHorizontalSamplePoints(viewBoundary, theBottomestLine, callback);

        return count;
    }

    private int swipeAllOfHorizontalLinesFromLeftToRight(Rect viewBoundary,
            BiConsumer<Point, Point> callback) {
        final int theLeftestLine = viewBoundary.left + 1;
        final int theToppestLine = viewBoundary.top + 1;
        final int theBottomestLine = viewBoundary.bottom - 1;

        int count = 0;

        for (int i = theToppestLine; i < theBottomestLine; i += mDensityPerCm) {
            if (callback != null) {
                callback.accept(new Point(theLeftestLine, i),
                        new Point(viewBoundary.centerX(), i));
            }
            mDevice.waitForIdle();
            count++;
        }
        if (callback != null) {
            callback.accept(new Point(theLeftestLine, theBottomestLine),
                    new Point(viewBoundary.centerX(), theBottomestLine));
        }
        mDevice.waitForIdle();
        count++;

        return count;
    }

    private int swipeAllOfHorizontalLinesFromRightToLeft(Rect viewBoundary,
            BiConsumer<Point, Point> callback) {
        final int theToppestLine = viewBoundary.top + 1;
        final int theRightestLine = viewBoundary.right - 1;
        final int theBottomestLine = viewBoundary.bottom - 1;

        int count = 0;
        for (int i = theToppestLine; i < theBottomestLine; i += mDensityPerCm) {
            if (callback != null) {
                callback.accept(new Point(theRightestLine, i),
                        new Point(viewBoundary.centerX(), i));
            }
            mDevice.waitForIdle();
            count++;
        }
        if (callback != null) {
            callback.accept(new Point(theRightestLine, theBottomestLine),
                    new Point(viewBoundary.centerX(), theBottomestLine));
        }
        mDevice.waitForIdle();
        count++;

        return count;
    }

    private int swipeAllOfHorizontalLines(Rect viewBoundary, BiConsumer<Point, Point> callback) {
        int count = 0;

        count += swipeAllOfHorizontalLinesFromLeftToRight(viewBoundary, callback);
        count += swipeAllOfHorizontalLinesFromRightToLeft(viewBoundary, callback);

        return count;
    }

    private int swipeAllOfVerticalLinesFromTopToBottom(Rect viewBoundary,
            BiConsumer<Point, Point> callback) {
        final int theLeftestLine = viewBoundary.left + 1;
        final int theToppestLine = viewBoundary.top + 1;
        final int theRightestLine = viewBoundary.right - 1;

        int count = 0;
        for (int i = theLeftestLine; i < theRightestLine; i += mDensityPerCm) {
            if (callback != null) {
                callback.accept(new Point(i, theToppestLine),
                        new Point(i, viewBoundary.centerY()));
            }
            mDevice.waitForIdle();
            count++;
        }
        if (callback != null) {
            callback.accept(new Point(theRightestLine, theToppestLine),
                    new Point(theRightestLine, viewBoundary.centerY()));
        }
        mDevice.waitForIdle();
        count++;

        return count;
    }

    private int swipeAllOfVerticalLinesFromBottomToTop(Rect viewBoundary,
            BiConsumer<Point, Point> callback) {
        final int theLeftestLine = viewBoundary.left + 1;
        final int theRightestLine = viewBoundary.right - 1;
        final int theBottomestLine = viewBoundary.bottom - 1;

        int count = 0;
        for (int i = theLeftestLine; i < theRightestLine; i += mDensityPerCm) {
            if (callback != null) {
                callback.accept(new Point(i, theBottomestLine),
                        new Point(i, viewBoundary.centerY()));
            }
            mDevice.waitForIdle();
            count++;
        }
        if (callback != null) {
            callback.accept(new Point(theRightestLine, theBottomestLine),
                    new Point(theRightestLine, viewBoundary.centerY()));
        }
        mDevice.waitForIdle();
        count++;

        return count;
    }

    private int swipeAllOfVerticalLines(Rect viewBoundary, BiConsumer<Point, Point> callback) {
        int count = 0;

        count += swipeAllOfVerticalLinesFromTopToBottom(viewBoundary, callback);
        count += swipeAllOfVerticalLinesFromBottomToTop(viewBoundary, callback);

        return count;
    }

    private int swipeInViewBoundary(Rect viewBoundary, BiConsumer<Point, Point> callback) {
        int count = 0;

        count += swipeBigX(viewBoundary, callback);
        count += swipeAllOfHorizontalLines(viewBoundary, callback);
        count += swipeAllOfVerticalLines(viewBoundary, callback);

        return count;
    }

    private int swipeInViewBoundary(Rect viewBoundary) {
        return swipeInViewBoundary(viewBoundary, this::swipeByUiDevice);
    }

    private List<Rect> splitBoundsAccordingToExclusionLimit(Rect rect) {
        final int exclusionHeightLimit = (int) (getPropertyOfMaxExclusionHeight() * mPixelsPerDp
                + 0.5f);

        final List<Rect> bounds = new ArrayList<>();
        if (rect.height() < exclusionHeightLimit) {
            bounds.add(rect);
            return bounds;
        }

        int nextTop = rect.top;
        while (nextTop >= rect.bottom) {
            final int top = nextTop;
            int bottom = top + exclusionHeightLimit;
            if (bottom > rect.bottom) {
                bottom = rect.bottom;
            }

            bounds.add(new Rect(rect.left, top, rect.right, bottom));

            nextTop += bottom;
        }

        return bounds;
    }

    @Test
    public void mandatorySystemGesture_excludeViewRects_withoutAnyCancel()
            throws InterruptedException {
        assumeTrue(hasSystemGestureFeature());

        mainThreadRun(() -> mContentViewWindowInsets = mActivity.getDecorViewWindowInsets());
        mainThreadRun(() -> mSwipeBound = mActivity.getOperationArea(
                mContentViewWindowInsets.getMandatorySystemGestureInsets(),
                mContentViewWindowInsets));

        final List<Rect> swipeBounds = splitBoundsAccordingToExclusionLimit(mSwipeBound);
        int swipeCount = 0;
        for (Rect swipeBound : swipeBounds) {
            setAndWaitForSystemGestureExclusionRectsListenerTrigger(swipeBound);
            swipeCount += swipeInViewBoundary(swipeBound);
        }

        mainThreadRun(() -> {
            mActionDownPoints = mActivity.getActionDownPoints();
            mActionUpPoints = mActivity.getActionUpPoints();
            mActionCancelPoints = mActivity.getActionCancelPoints();
        });
        mScreenshotTestRule.capture();

        assertEquals(0, mActionCancelPoints.size());
        assertEquals(swipeCount, mActionUpPoints.size());
        assertEquals(swipeCount, mActionDownPoints.size());
    }

    @Test
    public void systemGesture_notExcludeViewRects_withoutAnyCancel() {
        assumeTrue(hasSystemGestureFeature());

        mainThreadRun(() -> mActivity.setSystemGestureExclusion(null));
        mainThreadRun(() -> mContentViewWindowInsets = mActivity.getDecorViewWindowInsets());
        mainThreadRun(() -> mSwipeBound = mActivity.getOperationArea(
                mContentViewWindowInsets.getSystemGestureInsets(), mContentViewWindowInsets));

        final List<Rect> swipeBounds = splitBoundsAccordingToExclusionLimit(mSwipeBound);
        int swipeCount = 0;
        for (Rect swipeBound : swipeBounds) {
            swipeCount += swipeInViewBoundary(swipeBound);
        }

        mainThreadRun(() -> {
            mActionDownPoints = mActivity.getActionDownPoints();
            mActionUpPoints = mActivity.getActionUpPoints();
            mActionCancelPoints = mActivity.getActionCancelPoints();
        });
        mScreenshotTestRule.capture();

        assertEquals(0, mActionCancelPoints.size());
        assertEquals(swipeCount, mActionUpPoints.size());
        assertEquals(swipeCount, mActionDownPoints.size());
    }

    @Test
    public void tappableElements_tapSamplePoints_excludeViewRects_withoutAnyCancel()
            throws InterruptedException {
        assumeTrue(hasSystemGestureFeature());

        final Rect[] rects = new Rect[1];
        mainThreadRun(() -> rects[0] = mActivity.getViewBound(mActivity.getContentView()));
        setAndWaitForSystemGestureExclusionRectsListenerTrigger(rects[0]);
        mainThreadRun(() -> mContentViewWindowInsets = mActivity.getDecorViewWindowInsets());
        mainThreadRun(() -> mSwipeBound = mActivity.getOperationArea(
                mContentViewWindowInsets.getTappableElementInsets(), mContentViewWindowInsets));

        final int count = clickAllOfSamplePoints(mSwipeBound, this::clickAndWaitByUiDevice);

        mainThreadRun(() -> {
            mClickCount = mActivity.getClickCount();
            mActionCancelPoints = mActivity.getActionCancelPoints();
        });
        mScreenshotTestRule.capture();

        assertEquals("The number of click not match", count, mClickCount);
        assertEquals("The Number of the canceled points not match", 0,
                mActionCancelPoints.size());
    }

    @Test
    public void tappableElements_tapSamplePoints_notExcludeViewRects_withoutAnyCancel() {
        assumeTrue(hasSystemGestureFeature());

        mainThreadRun(() -> mActivity.setSystemGestureExclusion(null));
        mainThreadRun(() -> mContentViewWindowInsets = mActivity.getDecorViewWindowInsets());
        mainThreadRun(() -> mSwipeBound = mActivity.getOperationArea(
                mContentViewWindowInsets.getTappableElementInsets(), mContentViewWindowInsets));

        final int count = clickAllOfSamplePoints(mSwipeBound, this::clickAndWaitByUiDevice);

        mainThreadRun(() -> {
            mClickCount = mActivity.getClickCount();
            mActionCancelPoints = mActivity.getActionCancelPoints();
        });
        mScreenshotTestRule.capture();

        assertEquals("The number of click not match", count, mClickCount);
        assertEquals("The Number of the canceled points not match", 0,
                mActionCancelPoints.size());
    }

    @Test
    public void swipeInsideLimit_systemUiVisible_noEventCanceled() throws Throwable {
        assumeTrue(hasSystemGestureFeature());

        final int swipeCount = 1;
        final boolean insideLimit = true;
        testSystemGestureExclusionLimit(swipeCount, insideLimit, SYSTEM_UI_FLAG_VISIBLE);

        assertEquals("Swipe must not be canceled.", 0, mActionCancelPoints.size());
        assertEquals("Action up points.", swipeCount, mActionUpPoints.size());
        assertEquals("Action down points.", swipeCount, mActionDownPoints.size());
    }

    @Test
    public void swipeOutsideLimit_systemUiVisible_allEventsCanceled() throws Throwable {
        assumeTrue(hasSystemGestureFeature());

        assumeGestureNavigationMode();

        final int swipeCount = 1;
        final boolean insideLimit = false;
        testSystemGestureExclusionLimit(swipeCount, insideLimit, SYSTEM_UI_FLAG_VISIBLE);

        assertEquals("Swipe must be always canceled.", swipeCount, mActionCancelPoints.size());
        assertEquals("Action up points.", 0, mActionUpPoints.size());
        assertEquals("Action down points.", swipeCount, mActionDownPoints.size());
    }

    @Test
    public void swipeInsideLimit_immersiveSticky_noEventCanceled() throws Throwable {
        assumeTrue(hasSystemGestureFeature());

        // The first event may be never canceled. So we need to swipe at least twice.
        final int swipeCount = 2;
        final boolean insideLimit = true;
        testSystemGestureExclusionLimit(swipeCount, insideLimit, SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | SYSTEM_UI_FLAG_FULLSCREEN | SYSTEM_UI_FLAG_HIDE_NAVIGATION);

        assertEquals("Swipe must not be canceled.", 0, mActionCancelPoints.size());
        assertEquals("Action up points.", swipeCount, mActionUpPoints.size());
        assertEquals("Action down points.", swipeCount, mActionDownPoints.size());
    }

    @Test
    public void swipeOutsideLimit_immersiveSticky_noEventCanceled() throws Throwable {
        assumeTrue(hasSystemGestureFeature());

        // The first event may be never canceled. So we need to swipe at least twice.
        final int swipeCount = 2;
        final boolean insideLimit = false;
        testSystemGestureExclusionLimit(swipeCount, insideLimit, SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | SYSTEM_UI_FLAG_FULLSCREEN | SYSTEM_UI_FLAG_HIDE_NAVIGATION);

        assertEquals("Swipe must not be canceled.", 0, mActionCancelPoints.size());
        assertEquals("Action up points.", swipeCount, mActionUpPoints.size());
        assertEquals("Action down points.", swipeCount, mActionDownPoints.size());
    }

    private void testSystemGestureExclusionLimit(int swipeCount, boolean insideLimit,
            int systemUiVisibility) throws Throwable {
        final int shiftY = insideLimit ? 1 : -1;
        assumeGestureNavigation();
        doInExclusionLimitSession(() -> {
            setSystemUiVisibility(systemUiVisibility);
            setAndWaitForSystemGestureExclusionRectsListenerTrigger(null);

            // The limit is consumed from bottom to top.
            final int[] bottom = new int[1];
            mainThreadRun(() -> {
                final View rootView = mActivity.getWindow().getDecorView();
                bottom[0] = rootView.getLocationOnScreen()[1] + rootView.getHeight();
            });
            final int swipeY = bottom[0] - mExclusionLimit + shiftY;

            for (int i = 0; i < swipeCount; i++) {
                swipeFromLeftToRight(swipeY, mDisplayWidth);
            }

            mainThreadRun(() -> {
                mActionDownPoints = mActivity.getActionDownPoints();
                mActionUpPoints = mActivity.getActionUpPoints();
                mActionCancelPoints = mActivity.getActionCancelPoints();
            });
        });
    }

    private void assumeGestureNavigation() {
        final Insets[] insets = new Insets[1];
        mainThreadRun(() -> {
            final View view = mActivity.getWindow().getDecorView();
            insets[0] = view.getRootWindowInsets().getSystemGestureInsets();
        });
        assumeTrue("Gesture navigation required.", insets[0].left > 0);
    }

    private void assumeGestureNavigationMode() {
        // TODO: b/153032202 consider the CTS on GSI case.
        Resources res = mTargetContext.getResources();
        int naviMode = res.getIdentifier(NAV_BAR_INTERACTION_MODE_RES_NAME, "integer", "android");

        assumeTrue("Gesture navigation required", naviMode == NAV_BAR_INTERACTION_MODE_GESTURAL);
    }

    /**
     * Set system UI visibility and wait for it is applied by the system.
     *
     * @param flags the visibility flags.
     * @throws InterruptedException when the test gets aborted.
     */
    private void setSystemUiVisibility(int flags) throws InterruptedException {
        final CountDownLatch flagsApplied = new CountDownLatch(1);
        final int targetFlags = SYSTEM_UI_CLEARABLE_FLAGS & flags;
        mainThreadRun(() -> {
            final View view = mActivity.getWindow().getDecorView();
            if ((view.getSystemUiVisibility() & SYSTEM_UI_CLEARABLE_FLAGS) == targetFlags) {
                // System UI visibility is already what we want. Stop waiting for the callback.
                flagsApplied.countDown();
                return;
            }
            view.setOnSystemUiVisibilityChangeListener(visibility -> {
                if (visibility == targetFlags) {
                    flagsApplied.countDown();
                }
            });
            view.setSystemUiVisibility(flags);
        });
        assertTrue("System UI visibility must be applied.", flagsApplied.await(3, SECONDS));
    }

    /**
     * Set an exclusion rectangle and wait for it is applied by the system.
     * <p>
     *     if the parameter rect doesn't provide or is null, the decorView will be used to set into
     *     the exclusion rects.
     * </p>
     *
     * @param rect the rectangle that is added into the system gesture exclusion rects.
     * @throws InterruptedException when the test gets aborted.
     */
    private void setAndWaitForSystemGestureExclusionRectsListenerTrigger(Rect rect)
            throws InterruptedException {
        final CountDownLatch exclusionApplied = new CountDownLatch(1);
        mainThreadRun(() -> {
            final View view = mActivity.getWindow().getDecorView();
            final ViewTreeObserver vto = view.getViewTreeObserver();
            vto.addOnSystemGestureExclusionRectsChangedListener(
                    rects -> exclusionApplied.countDown());
            Rect exclusiveRect = new Rect(0, 0, view.getWidth(), view.getHeight());
            if (rect != null) {
                exclusiveRect = rect;
            }
            view.setSystemGestureExclusionRects(Lists.newArrayList(exclusiveRect));
        });
        assertTrue("Exclusion must be applied.", exclusionApplied.await(3, SECONDS));
    }

    private void swipeFromLeftToRight(int y, int distance) {
        mDevice.swipe(0, y, distance, y, STEPS);
    }

    private static int getPropertyOfMaxExclusionHeight() {
        final int[] originalLimitDp = new int[1];
        SystemUtil.runWithShellPermissionIdentity(() -> {
            originalLimitDp[0] = DeviceConfig.getInt(NAMESPACE_ANDROID,
                    KEY_SYSTEM_GESTURE_EXCLUSION_LIMIT_DP, -1);
            DeviceConfig.setProperty(
                    NAMESPACE_ANDROID,
                    KEY_SYSTEM_GESTURE_EXCLUSION_LIMIT_DP,
                    Integer.toString(EXCLUSION_LIMIT_DP), false /* makeDefault */);
        });

        return originalLimitDp[0];
    }

    /**
     * Run the given task while the system gesture exclusion limit has been changed to
     * {@link #EXCLUSION_LIMIT_DP}, and then restore the value while the task is finished.
     *
     * @param task the task to be run.
     * @throws Throwable when something goes unexpectedly.
     */
    private static void doInExclusionLimitSession(ThrowingRunnable task) throws Throwable {
        int originalLimitDp = getPropertyOfMaxExclusionHeight();
        try {
            task.run();
        } finally {
            // Restore the value
            SystemUtil.runWithShellPermissionIdentity(() -> DeviceConfig.setProperty(
                    NAMESPACE_ANDROID,
                    KEY_SYSTEM_GESTURE_EXCLUSION_LIMIT_DP,
                    (originalLimitDp != -1) ? Integer.toString(originalLimitDp) : null,
                    false /* makeDefault */));
        }
    }
}
