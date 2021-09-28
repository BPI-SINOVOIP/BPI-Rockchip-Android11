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

import static android.server.wm.ActivityManagerTestBase.launchHomeActivityNoWait;
import static android.server.wm.BarTestUtils.assumeHasStatusBar;
import static android.server.wm.UiDeviceUtils.pressUnlockButton;
import static android.server.wm.UiDeviceUtils.pressWakeupButton;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ContentResolver;
import android.content.Intent;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.annotations.Presubmit;
import android.provider.Settings;
import android.server.wm.settings.SettingsSession;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.CtsTouchUtils;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Test;

import java.util.ArrayList;
import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Ensure moving windows and tapping is done synchronously.
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:WindowInputTests
 */
@Presubmit
public class WindowInputTests {
    private final int TOTAL_NUMBER_OF_CLICKS = 100;
    private final ActivityTestRule<TestActivity> mActivityRule =
            new ActivityTestRule<>(TestActivity.class);

    private Instrumentation mInstrumentation;
    private TestActivity mActivity;
    private View mView;
    private final Random mRandom = new Random();

    private int mClickCount = 0;

    @Before
    public void setUp() {
        pressWakeupButton();
        pressUnlockButton();
        launchHomeActivityNoWait();

        mInstrumentation = getInstrumentation();
        mActivity = mActivityRule.launchActivity(null);
        mInstrumentation.waitForIdleSync();
        mClickCount = 0;
    }

    @Test
    public void testMoveWindowAndTap() throws Throwable {
        final WindowManager wm = mActivity.getWindowManager();
        Point displaySize = new Point();
        mActivity.getDisplay().getSize(displaySize);

        final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        mClickCount = 0;

        // Set up window.
        mActivityRule.runOnUiThread(() -> {
            mView = new View(mActivity);
            p.width = 20;
            p.height = 20;
            p.gravity = Gravity.LEFT | Gravity.TOP;
            mView.setOnClickListener((v) -> {
                mClickCount++;
            });
            mActivity.addWindow(mView, p);
        });
        mInstrumentation.waitForIdleSync();

        WindowInsets insets = mActivity.getWindow().getDecorView().getRootWindowInsets();
        final Rect windowBounds = new Rect(insets.getSystemWindowInsetLeft(),
                insets.getSystemWindowInsetTop(),
                displaySize.x - insets.getSystemWindowInsetRight(),
                displaySize.y - insets.getSystemWindowInsetBottom());

        // Move the window to a random location in the window and attempt to tap on view multiple
        // times.
        final Point locationInWindow = new Point();
        for (int i = 0; i < TOTAL_NUMBER_OF_CLICKS; i++) {
            selectRandomLocationInWindow(windowBounds, locationInWindow);
            mActivityRule.runOnUiThread(() -> {
                p.x = locationInWindow.x;
                p.y = locationInWindow.y;
                wm.updateViewLayout(mView, p);
            });
            mInstrumentation.waitForIdleSync();
            CtsTouchUtils.emulateTapOnViewCenter(mInstrumentation, mActivityRule, mView);
        }

        assertEquals(TOTAL_NUMBER_OF_CLICKS, mClickCount);
    }

    private void selectRandomLocationInWindow(Rect bounds, Point outLocation) {
        int randomX = mRandom.nextInt(bounds.right - bounds.left) + bounds.left;
        int randomY = mRandom.nextInt(bounds.bottom - bounds.top) + bounds.top;
        outLocation.set(randomX, randomY);
    }

    @Test
    public void testFilterTouchesWhenObscured() throws Throwable {
        final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        mClickCount = 0;

        // Set up window.
        mActivityRule.runOnUiThread(() -> {
            mView = new View(mActivity);
            p.width = 20;
            p.height = 20;
            p.gravity = Gravity.LEFT | Gravity.TOP;
            mView.setFilterTouchesWhenObscured(true);
            mView.setOnClickListener((v) -> {
                mClickCount++;
            });
            mActivity.addWindow(mView, p);

            View viewOverlap = new View(mActivity);
            p.gravity = Gravity.RIGHT | Gravity.TOP;
            p.type = WindowManager.LayoutParams.TYPE_APPLICATION;
            mActivity.addWindow(viewOverlap, p);
        });
        mInstrumentation.waitForIdleSync();

        CtsTouchUtils.emulateTapOnViewCenter(mInstrumentation, mActivityRule, mView);
        assertEquals(0, mClickCount);
    }

    @Test
    public void testOverlapWindow() throws Throwable {
        final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        mClickCount = 0;
        try (final PointerLocationSession session = new PointerLocationSession()) {
            session.set(true);
            // Set up window.
            mActivityRule.runOnUiThread(() -> {
                mView = new View(mActivity);
                p.width = 20;
                p.height = 20;
                p.gravity = Gravity.LEFT | Gravity.TOP;
                mView.setFilterTouchesWhenObscured(true);
                mView.setOnClickListener((v) -> {
                    mClickCount++;
                });
                mActivity.addWindow(mView, p);

            });
            mInstrumentation.waitForIdleSync();

            CtsTouchUtils.emulateTapOnViewCenter(mInstrumentation, mActivityRule, mView);
        }
        assertEquals(1, mClickCount);
    }

    @Test
    public void testWindowBecomesUnTouchable() throws Throwable {
        final WindowManager wm = mActivity.getWindowManager();
        final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        mClickCount = 0;

        final View viewOverlap = new View(mActivity);

        // Set up window.
        mActivityRule.runOnUiThread(() -> {
            mView = new View(mActivity);
            p.width = 20;
            p.height = 20;
            p.gravity = Gravity.LEFT | Gravity.TOP;
            mView.setOnClickListener((v) -> {
                mClickCount++;
            });
            mActivity.addWindow(mView, p);

            p.width = 100;
            p.height = 100;
            p.gravity = Gravity.LEFT | Gravity.TOP;
            p.type = WindowManager.LayoutParams.TYPE_APPLICATION;
            mActivity.addWindow(viewOverlap, p);
        });
        mInstrumentation.waitForIdleSync();

        CtsTouchUtils.emulateTapOnViewCenter(mInstrumentation, mActivityRule, mView);
        assertEquals(0, mClickCount);

        mActivityRule.runOnUiThread(() -> {
            p.flags = FLAG_NOT_FOCUSABLE | FLAG_NOT_TOUCHABLE;
            wm.updateViewLayout(viewOverlap, p);
        });
        mInstrumentation.waitForIdleSync();

        CtsTouchUtils.emulateTapOnViewCenter(mInstrumentation, mActivityRule, mView);
        assertEquals(1, mClickCount);
    }

    @Test
    public void testInjectToStatusBar() {
        // Try to inject event to status bar.
        assumeHasStatusBar(mActivityRule);
        final long downTime = SystemClock.uptimeMillis();
        final MotionEvent eventHover = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_HOVER_MOVE, 0, 0, 0);
        eventHover.setSource(InputDevice.SOURCE_MOUSE);
        try {
            mInstrumentation.sendPointerSync(eventHover);
            fail("Not allowed to inject event to the window from another process.");
        } catch (SecurityException e) {
            // Should not be allowed to inject event to the window from another process.
        }
    }

    @Test
    public void testInjectFromThread() throws InterruptedException {
        // Continually inject event to activity from thread.
        final Point displaySize = new Point();
        mActivity.getDisplay().getSize(displaySize);
        final Point testPoint = new Point(displaySize.x / 2, displaySize.y / 2);

        final long downTime = SystemClock.uptimeMillis();
        final MotionEvent eventDown = MotionEvent.obtain(
                downTime, downTime, MotionEvent.ACTION_DOWN, testPoint.x, testPoint.y, 1);
        mInstrumentation.sendPointerSync(eventDown);

        final ExecutorService executor = Executors.newSingleThreadExecutor();
        executor.execute(() -> {
            mInstrumentation.sendPointerSync(eventDown);
            for (int i = 0; i < 20; i++) {
                final long eventTime = SystemClock.uptimeMillis();
                final MotionEvent eventMove = MotionEvent.obtain(
                        downTime, eventTime, MotionEvent.ACTION_MOVE, testPoint.x, testPoint.y, 1);
                try {
                    mInstrumentation.sendPointerSync(eventMove);
                } catch (SecurityException e) {
                    fail("Should be allowed to inject event.");
                }
            }
        });

        // Launch another activity, should not crash the process.
        final Intent intent = new Intent(mActivity, TestActivity.class);
        mActivityRule.launchActivity(intent);
        mInstrumentation.waitForIdleSync();

        executor.shutdown();
        executor.awaitTermination(5L, TimeUnit.SECONDS);
    }

    public static class TestActivity extends Activity {
        private ArrayList<View> mViews = new ArrayList<>();

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
        }

        void addWindow(View view, WindowManager.LayoutParams attrs) {
            getWindowManager().addView(view, attrs);
            mViews.add(view);
        }

        void removeAllWindows() {
            for (View view : mViews) {
                getWindowManager().removeViewImmediate(view);
            }
            mViews.clear();
        }

        @Override
        protected void onPause() {
            super.onPause();
            removeAllWindows();
        }
    }

    /** Helper class to save, set, and restore pointer location preferences. */
    private static class PointerLocationSession extends SettingsSession<Boolean> {
        PointerLocationSession() {
            super(Settings.System.getUriFor("pointer_location" /* POINTER_LOCATION */),
                    PointerLocationSession::get,
                    PointerLocationSession::put);
        }

        private static void put(ContentResolver contentResolver, String s, boolean v) {
            SystemUtil.runShellCommand(
                    "settings put system " + "pointer_location" + " " + (v ? 1 : 0));
        }

        private static boolean get(ContentResolver contentResolver, String s) {
            try {
                return Integer.parseInt(SystemUtil.runShellCommand(
                        "settings get system " + "pointer_location").trim()) == 1;
            } catch (NumberFormatException e) {
                return false;
            }
        }
    }
}
