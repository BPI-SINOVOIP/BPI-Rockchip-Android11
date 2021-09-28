/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static android.view.WindowInsets.Type.displayCutout;
import static android.view.WindowInsets.Type.navigationBars;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;

import android.app.Activity;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.view.Display;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowMetrics;

import androidx.test.rule.ActivityTestRule;

import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests that verify the behavior of {@link WindowMetrics} and {@link android.app.WindowContext} API
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:WindowMetricsTests
 */
@Presubmit
public class WindowMetricsTests extends WindowManagerTestBase {

    private ActivityTestRule<MetricsActivity> mMetricsActivity =
            new ActivityTestRule<>(MetricsActivity.class);

    @Test
    public void testMetricsSanity() {
        // TODO(b/149668895): handle device with cutout.
        assumeFalse(hasDisplayCutout());

        final MetricsActivity activity = mMetricsActivity.launchActivity(null);
        activity.waitForLayout();

        assertEquals(activity.mOnLayoutBounds, activity.mOnCreateCurrentMetrics.getBounds());
        assertTrue(activity.mOnCreateMaximumMetrics.getBounds().width()
                >= activity.mOnCreateCurrentMetrics.getBounds().width());
        assertTrue(activity.mOnCreateMaximumMetrics.getBounds().height()
                >= activity.mOnCreateCurrentMetrics.getBounds().height());

        assertEquals(activity.mOnLayoutInsets.getSystemWindowInsets(),
                activity.mOnCreateCurrentMetrics.getWindowInsets().getSystemWindowInsets());
        assertEquals(activity.mOnLayoutInsets.getStableInsets(),
                activity.mOnCreateCurrentMetrics.getWindowInsets().getStableInsets());
        assertEquals(activity.mOnLayoutInsets.getDisplayCutout(),
                activity.mOnCreateCurrentMetrics.getWindowInsets().getDisplayCutout());
    }

    @Test
    public void testMetricsMatchesDisplay() {
        final MetricsActivity activity = mMetricsActivity.launchActivity(null);
        activity.waitForLayout();

        final Display display = activity.getDisplay();

        // Check window size
        final Point displaySize = new Point();
        display.getSize(displaySize);
        final WindowMetrics windowMetrics = activity.getWindowManager().getCurrentWindowMetrics();
        final Rect bounds = getLegacyBounds(windowMetrics);
        assertEquals("Reported display width must match window width",
                displaySize.x, bounds.width());
        assertEquals("Reported display height must match window height",
                displaySize.y, bounds.height());

        // Check max window size
        final Point realDisplaySize = new Point();
        display.getRealSize(realDisplaySize);
        final WindowMetrics maxWindowMetrics = activity.getWindowManager()
                .getMaximumWindowMetrics();
        assertEquals("Reported real display width must match max window size",
                realDisplaySize.x, maxWindowMetrics.getBounds().width());
        assertEquals("Reported real display height must match max window size",
                realDisplaySize.y, maxWindowMetrics.getBounds().height());
    }

    private static Rect getLegacyBounds(WindowMetrics windowMetrics) {
        WindowInsets windowInsets = windowMetrics.getWindowInsets();
        final Insets insetsWithCutout =
                windowInsets.getInsetsIgnoringVisibility(navigationBars() | displayCutout());

        final Rect bounds = windowMetrics.getBounds();
        return inset(bounds, insetsWithCutout);
    }

    private static Rect inset(Rect original, Insets insets) {
        final int left = original.left + insets.left;
        final int top = original.top + insets.top;
        final int right = original.right - insets.right;
        final int bottom = original.bottom - insets.bottom;
        return new Rect(left, top, right, bottom);
    }

    public static class MetricsActivity extends Activity implements View.OnLayoutChangeListener {

        private final CountDownLatch mLayoutLatch = new CountDownLatch(1);

        private WindowMetrics mOnCreateMaximumMetrics;
        private WindowMetrics mOnCreateCurrentMetrics;

        private Rect mOnLayoutBounds;
        private WindowInsets mOnLayoutInsets;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            mOnCreateCurrentMetrics = getWindowManager().getCurrentWindowMetrics();
            mOnCreateMaximumMetrics = getWindowManager().getMaximumWindowMetrics();
            getWindow().getDecorView().addOnLayoutChangeListener(this);
        }

        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
                int oldTop, int oldRight, int oldBottom) {
            final View decorView = getWindow().getDecorView();
            mOnLayoutBounds = new Rect(decorView.getTop(), decorView.getLeft(),
                    decorView.getRight(), decorView.getBottom());
            mOnLayoutInsets = getWindow().getDecorView().getRootWindowInsets();
            mLayoutLatch.countDown();
        }

        void waitForLayout() {
            try {
                assertTrue("timed out waiting for activity to layout",
                        mLayoutLatch.await(4, TimeUnit.SECONDS));
            } catch (InterruptedException e) {
                throw new AssertionError(e);
            }
        }
    }
}
