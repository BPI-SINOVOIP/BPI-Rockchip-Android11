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
 * limitations under the License
 */

package android.server.wm;

import static android.provider.Settings.Global.WINDOW_ANIMATION_SCALE;
import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
import static android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.ContentResolver;
import android.graphics.Rect;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;
import android.provider.Settings;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets.Type;
import android.view.WindowManager.LayoutParams;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.ArrayList;

/**
 * Test whether WindowManager performs the correct layout after we make some changes to it.
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:LayoutTests
 */
@AppModeFull(reason = "Cannot write global settings as an instant app.")
@Presubmit
public class LayoutTests extends WindowManagerTestBase {
    private static final long TIMEOUT_RECEIVE_KEY = 100; // milliseconds
    private static final long TIMEOUT_SYSTEM_UI_VISIBILITY_CHANGE = 1000;
    private static final int SYSTEM_UI_FLAG_HIDE_ALL = SYSTEM_UI_FLAG_FULLSCREEN
            | SYSTEM_UI_FLAG_HIDE_NAVIGATION;

    private float mWindowAnimationScale;

    @Before
    public void setup() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            // The layout will be performed at the end of the animation of hiding status/navigation
            // bar, which will recover the possible issue, so we disable the animation during the
            // test.
            final ContentResolver resolver = getInstrumentation().getContext().getContentResolver();
            mWindowAnimationScale = Settings.Global.getFloat(resolver, WINDOW_ANIMATION_SCALE, 1f);
            Settings.Global.putFloat(resolver, WINDOW_ANIMATION_SCALE, 0);
        });
    }

    @After
    public void tearDown() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            // Restore the animation we disabled previously.
            Settings.Global.putFloat(getInstrumentation().getContext().getContentResolver(),
                    WINDOW_ANIMATION_SCALE, mWindowAnimationScale);
        });
    }

    @Test
    public void testLayoutAfterRemovingFocus() throws InterruptedException {
        final TestActivity activity = startActivity(TestActivity.class);

        // Get the visible frame of the main activity before adding any window.
        final Rect visibleFrame = new Rect();
        getInstrumentation().runOnMainSync(() ->
                activity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleFrame));
        assertFalse("Visible frame must not be empty.", visibleFrame.isEmpty());

        doTestLayoutAfterRemovingFocus(activity, visibleFrame, SYSTEM_UI_FLAG_FULLSCREEN);
        doTestLayoutAfterRemovingFocus(activity, visibleFrame, SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        doTestLayoutAfterRemovingFocus(activity, visibleFrame, SYSTEM_UI_FLAG_HIDE_ALL);
    }

    private void doTestLayoutAfterRemovingFocus(TestActivity activity,
            Rect visibleFrameBeforeAddingWindow, int systemUiFlags) throws InterruptedException {
        // Add a window which can affect the global layout.
        getInstrumentation().runOnMainSync(() -> {
            final View view = new View(activity);
            view.setSystemUiVisibility(systemUiFlags);
            activity.addWindow(view, new LayoutParams());
        });

        // Wait for the global layout triggered by adding window.
        activity.waitForGlobalLayout();

        // Remove the window we added previously.
        getInstrumentation().runOnMainSync(activity::removeAllWindows);

        // Wait for the global layout triggered by removing window.
        activity.waitForGlobalLayout();

        // Wait for the activity has focus before get the visible frame
        activity.waitAndAssertWindowFocusState(true);

        // Get the visible frame of the main activity after removing the window we added.
        final Rect visibleFrameAfterRemovingWindow = new Rect();
        getInstrumentation().runOnMainSync(() ->
                activity.getWindow().getDecorView().getWindowVisibleDisplayFrame(
                        visibleFrameAfterRemovingWindow));

        // Test whether the visible frame after removing window is the same as one before adding
        // window. If not, it shows that the layout after removing window has a problem.
        assertEquals(visibleFrameBeforeAddingWindow, visibleFrameAfterRemovingWindow);
    }

    @Test
    public void testAddingImmersiveWindow() throws InterruptedException {
        final boolean[] systemUiFlagsGotCleared = { false };
        final TestActivity activity = startActivity(TestActivity.class);

        // Add a window which has clearable system UI flags.
        getInstrumentation().runOnMainSync(() -> {
            final View view = new View(activity);
            view.setSystemUiVisibility(SYSTEM_UI_FLAG_IMMERSIVE_STICKY | SYSTEM_UI_FLAG_HIDE_ALL);
            view.setOnSystemUiVisibilityChangeListener(
                    visibility -> {
                        if ((visibility & SYSTEM_UI_FLAG_HIDE_ALL) != SYSTEM_UI_FLAG_HIDE_ALL) {
                            systemUiFlagsGotCleared[0] = true;
                            // Early break because things go wrong already.
                            synchronized (activity) {
                                activity.notify();
                            }
                        }
                    });
            activity.addWindow(view, new LayoutParams());
        });

        // Wait for the possible failure.
        synchronized (activity) {
            activity.wait(TIMEOUT_SYSTEM_UI_VISIBILITY_CHANGE);
        }

        // Test if flags got cleared.
        assertFalse("System UI flags must not be cleared.", systemUiFlagsGotCleared[0]);
    }

    @Test
    public void testChangingFocusableFlag() throws Exception {
        final View[] view = new View[1];
        final LayoutParams attrs = new LayoutParams(TYPE_APPLICATION_PANEL, FLAG_NOT_FOCUSABLE);
        final boolean[] childWindowHasFocus = { false };
        final boolean[] childWindowGotKeyEvent = { false };
        final TestActivity activity = startActivity(TestActivity.class);

        // Add a not-focusable window.
        getInstrumentation().runOnMainSync(() -> {
            view[0] = new View(activity) {
                public void onWindowFocusChanged(boolean hasWindowFocus) {
                    super.onWindowFocusChanged(hasWindowFocus);
                    childWindowHasFocus[0] = hasWindowFocus;
                    synchronized (activity) {
                        activity.notify();
                    }
                }

                public boolean onKeyDown(int keyCode, KeyEvent event) {
                    synchronized (activity) {
                        childWindowGotKeyEvent[0] = true;
                    }
                    return super.onKeyDown(keyCode, event);
                }
            };
            activity.addWindow(view[0], attrs);
        });
        getInstrumentation().waitForIdleSync();

        // Make the window focusable.
        getInstrumentation().runOnMainSync(() -> {
            attrs.flags &= ~FLAG_NOT_FOCUSABLE;
            activity.getWindowManager().updateViewLayout(view[0], attrs);
        });
        synchronized (activity) {
            activity.wait(TIMEOUT_WINDOW_FOCUS_CHANGED);
        }

        // The window must have focus.
        assertTrue("Child window must have focus.", childWindowHasFocus[0]);

        // Ensure the window can receive keys.
        PollingCheck.check("Child window must get key event.", TIMEOUT_RECEIVE_KEY, () -> {
            getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_0);
            synchronized (activity) {
                return childWindowGotKeyEvent[0];
            }
        });
    }

    @Test
    public void testSysuiFlagLayoutFullscreen() {
        final TestActivity activity = startActivity(TestActivity.class);

        final View[] views = new View[2];
        getInstrumentation().runOnMainSync(() -> {
            views[0] = new View(activity);
            final LayoutParams attrs = new LayoutParams();
            attrs.setFitInsetsTypes(attrs.getFitInsetsTypes() & ~Type.statusBars());
            activity.addWindow(views[0], attrs);

            views[1] = new View(activity);
            views[1].setSystemUiVisibility(SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
            activity.addWindow(views[1], new LayoutParams());
        });
        getInstrumentation().waitForIdleSync();

        assertLayoutEquals(views[0], views[1]);
    }

    @Test
    public void testSysuiFlagLayoutHideNavigation() {
        final TestActivity activity = startActivity(TestActivity.class);

        final View[] views = new View[2];
        getInstrumentation().runOnMainSync(() -> {
            views[0] = new View(activity);
            final LayoutParams attrs = new LayoutParams();
            attrs.setFitInsetsTypes(attrs.getFitInsetsTypes() & ~Type.systemBars());
            activity.addWindow(views[0], attrs);

            views[1] = new View(activity);
            views[1].setSystemUiVisibility(SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
            activity.addWindow(views[1], new LayoutParams());
        });
        getInstrumentation().waitForIdleSync();

        assertLayoutEquals(views[0], views[1]);
    }

    private static void assertLayoutEquals(View view1, View view2) {
        final int[][] locations = new int[2][2];
        view1.getLocationOnScreen(locations[0]);
        view2.getLocationOnScreen(locations[1]);
        assertArrayEquals("Location must be the same.", locations[0], locations[1]);
        assertEquals("Width must be the same.", view1.getWidth(), view2.getWidth());
        assertEquals("Height must be the same.", view1.getHeight(), view2.getHeight());
    }

    public static class TestActivity extends FocusableActivity {
        private static final long TIMEOUT_LAYOUT = 200; // milliseconds

        private final Object mLockGlobalLayout = new Object();
        private ArrayList<View> mViews = new ArrayList<>();

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(() -> {
                synchronized (mLockGlobalLayout) {
                    mLockGlobalLayout.notify();
                }
            });
        }

        void waitForGlobalLayout() throws InterruptedException {
            synchronized (mLockGlobalLayout) {
                mLockGlobalLayout.wait(TIMEOUT_LAYOUT);
            }
        }

        void addWindow(View view, LayoutParams attrs) {
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
}
