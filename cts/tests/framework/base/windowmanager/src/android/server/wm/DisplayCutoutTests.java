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

import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
import static android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
import static android.server.wm.DisplayCutoutTests.TestActivity.EXTRA_CUTOUT_MODE;
import static android.server.wm.DisplayCutoutTests.TestActivity.EXTRA_ORIENTATION;
import static android.server.wm.DisplayCutoutTests.TestDef.Which.DISPATCHED;
import static android.server.wm.DisplayCutoutTests.TestDef.Which.ROOT;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.everyItem;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsets.Type;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.hamcrest.CustomTypeSafeMatcher;
import org.hamcrest.FeatureMatcher;
import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;

import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.stream.Collectors;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:DisplayCutoutTests
 */
@Presubmit
@android.server.wm.annotation.Group3
@RunWith(Parameterized.class)
public class DisplayCutoutTests {
    static final String LEFT = "left";
    static final String TOP = "top";
    static final String RIGHT = "right";
    static final String BOTTOM = "bottom";

    @Parameterized.Parameters(name= "{1}({0})")
    public static Object[][] data() {
        return new Object[][]{
                {SCREEN_ORIENTATION_PORTRAIT, "SCREEN_ORIENTATION_PORTRAIT"},
                {SCREEN_ORIENTATION_LANDSCAPE, "SCREEN_ORIENTATION_LANDSCAPE"},
                {SCREEN_ORIENTATION_REVERSE_LANDSCAPE, "SCREEN_ORIENTATION_REVERSE_LANDSCAPE"},
                {SCREEN_ORIENTATION_REVERSE_PORTRAIT, "SCREEN_ORIENTATION_REVERSE_PORTRAIT"},
        };
    }

    @Parameter(0)
    public int orientation;

    @Parameter(1)
    public String orientationName;

    @Rule
    public final ErrorCollector mErrorCollector = new ErrorCollector();

    @Rule
    public final ActivityTestRule<TestActivity> mDisplayCutoutActivity =
            new ActivityTestRule<>(TestActivity.class, false /* initialTouchMode */,
                    false /* launchActivity */);

    @Test
    public void testConstructor() {
        final Insets safeInsets = Insets.of(1, 2, 3, 4);
        final Rect boundLeft = new Rect(5, 6, 7, 8);
        final Rect boundTop = new Rect(9, 0, 10, 1);
        final Rect boundRight = new Rect(2, 3, 4, 5);
        final Rect boundBottom = new Rect(6, 7, 8, 9);

        final DisplayCutout displayCutout =
                new DisplayCutout(safeInsets, boundLeft, boundTop, boundRight, boundBottom);

        assertEquals(safeInsets.left, displayCutout.getSafeInsetLeft());
        assertEquals(safeInsets.top, displayCutout.getSafeInsetTop());
        assertEquals(safeInsets.right, displayCutout.getSafeInsetRight());
        assertEquals(safeInsets.bottom, displayCutout.getSafeInsetBottom());

        assertTrue(boundLeft.equals(displayCutout.getBoundingRectLeft()));
        assertTrue(boundTop.equals(displayCutout.getBoundingRectTop()));
        assertTrue(boundRight.equals(displayCutout.getBoundingRectRight()));
        assertTrue(boundBottom.equals(displayCutout.getBoundingRectBottom()));

        assertEquals(Insets.NONE, displayCutout.getWaterfallInsets());
    }

    @Test
    public void testConstructor_waterfall() {
        final Insets safeInsets = Insets.of(1, 2, 3, 4);
        final Rect boundLeft = new Rect(5, 6, 7, 8);
        final Rect boundTop = new Rect(9, 0, 10, 1);
        final Rect boundRight = new Rect(2, 3, 4, 5);
        final Rect boundBottom = new Rect(6, 7, 8, 9);
        final Insets waterfallInsets = Insets.of(4, 8, 12, 16);

        final DisplayCutout displayCutout =
                new DisplayCutout(safeInsets, boundLeft, boundTop, boundRight, boundBottom,
                        waterfallInsets);

        assertEquals(safeInsets.left, displayCutout.getSafeInsetLeft());
        assertEquals(safeInsets.top, displayCutout.getSafeInsetTop());
        assertEquals(safeInsets.right, displayCutout.getSafeInsetRight());
        assertEquals(safeInsets.bottom, displayCutout.getSafeInsetBottom());

        assertTrue(boundLeft.equals(displayCutout.getBoundingRectLeft()));
        assertTrue(boundTop.equals(displayCutout.getBoundingRectTop()));
        assertTrue(boundRight.equals(displayCutout.getBoundingRectRight()));
        assertTrue(boundBottom.equals(displayCutout.getBoundingRectBottom()));

        assertEquals(waterfallInsets, displayCutout.getWaterfallInsets());
    }

    @Test
    public void testDisplayCutout_default() {
        runTest(LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                (activity, insets, displayCutout, which) -> {
            if (displayCutout == null) {
                return;
            }
            if (which == ROOT) {
                assertThat("cutout must be contained within system bars in default mode",
                        safeInsets(displayCutout), insetsLessThanOrEqualTo(stableInsets(insets)));
            } else if (which == DISPATCHED) {
                assertThat("must not dipatch to hierarchy in default mode",
                        displayCutout, nullValue());
            }
        });
    }

    @Test
    public void testDisplayCutout_shortEdges() {
        runTest(LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES, (a, insets, cutout, which) -> {
            if (which == ROOT) {
                final Rect appBounds = getAppBounds(a);
                final Insets displaySafeInsets = Insets.of(safeInsets(a.getDisplay().getCutout()));
                final Insets expected;
                if (appBounds.height() > appBounds.width()) {
                    // Portrait display
                    expected = Insets.of(0, displaySafeInsets.top, 0, displaySafeInsets.bottom);
                } else if (appBounds.height() < appBounds.width()) {
                    // Landscape display
                    expected = Insets.of(displaySafeInsets.left, 0, displaySafeInsets.right, 0);
                } else {
                    expected = Insets.NONE;
                }
                assertThat("cutout must provide the display's safe insets on short edges and zero"
                                + " on the long edges.",
                        Insets.of(safeInsets(cutout)),
                        equalTo(expected));
            }
        });
    }

    @Test
    public void testDisplayCutout_never() {
        runTest(LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER, (a, insets, displayCutout, which) -> {
            assertThat("must not layout in cutout area in never mode", displayCutout, nullValue());
        });
    }

    @Test
    public void testDisplayCutout_always() {
        runTest(LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS, (a, insets, displayCutout, which) -> {
            if (which == ROOT) {
                assertThat("Display.getCutout() must equal view root cutout",
                        a.getDisplay().getCutout(), equalTo(displayCutout));
            }
        });
    }

    private void runTest(int cutoutMode, TestDef test) {
        runTest(cutoutMode, test, orientation);
    }

    private void runTest(int cutoutMode, TestDef test, int orientation) {
        assumeTrue("Skipping test: orientation not supported", supportsOrientation(orientation));
        final TestActivity activity = launchAndWait(mDisplayCutoutActivity,
                cutoutMode, orientation);

        WindowInsets insets = getOnMainSync(activity::getRootInsets);
        WindowInsets dispatchedInsets = getOnMainSync(activity::getDispatchedInsets);
        Assert.assertThat("test setup failed, no insets at root", insets, notNullValue());
        Assert.assertThat("test setup failed, no insets dispatched",
                dispatchedInsets, notNullValue());

        final DisplayCutout displayCutout = insets.getDisplayCutout();
        final DisplayCutout dispatchedDisplayCutout = dispatchedInsets.getDisplayCutout();
        if (displayCutout != null) {
            commonAsserts(activity, displayCutout);
            if (cutoutMode != LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS) {
                shortEdgeAsserts(activity, insets, displayCutout);
            }
            assertCutoutsAreConsistentWithInsets(activity, displayCutout);
            assertSafeInsetsAreConsistentWithDisplayCutoutInsets(insets);
        }
        test.run(activity, insets, displayCutout, ROOT);

        if (dispatchedDisplayCutout != null) {
            commonAsserts(activity, dispatchedDisplayCutout);
            if (cutoutMode != LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS) {
                shortEdgeAsserts(activity, insets, displayCutout);
            }
            assertCutoutsAreConsistentWithInsets(activity, dispatchedDisplayCutout);
            if (cutoutMode != LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT) {
                assertSafeInsetsAreConsistentWithDisplayCutoutInsets(dispatchedInsets);
            }
        }
        test.run(activity, dispatchedInsets, dispatchedDisplayCutout, DISPATCHED);
    }

    private void assertSafeInsetsAreConsistentWithDisplayCutoutInsets(WindowInsets insets) {
        DisplayCutout cutout = insets.getDisplayCutout();
        Insets safeInsets = Insets.of(safeInsets(cutout));
        assertEquals("WindowInsets.getInsets(displayCutout()) must equal"
                        + " DisplayCutout.getSafeInsets()",
                safeInsets, insets.getInsets(Type.displayCutout()));
        assertEquals("WindowInsets.getInsetsIgnoringVisibility(displayCutout()) must equal"
                        + " DisplayCutout.getSafeInsets()",
                safeInsets, insets.getInsetsIgnoringVisibility(Type.displayCutout()));
    }

    private void commonAsserts(TestActivity activity, DisplayCutout cutout) {
        assertSafeInsetsValid(cutout);
        assertCutoutsAreWithinSafeInsets(activity, cutout);
        assertBoundsAreNonEmpty(cutout);
        assertAtMostOneCutoutPerEdge(activity, cutout);
    }

    private void shortEdgeAsserts(
            TestActivity activity, WindowInsets insets, DisplayCutout cutout) {
        assertOnlyShortEdgeHasInsets(activity, cutout);
        assertOnlyShortEdgeHasBounds(activity, cutout);
        assertThat("systemWindowInsets (also known as content insets) must be at least as "
                        + "large as cutout safe insets",
                safeInsets(cutout), insetsLessThanOrEqualTo(systemWindowInsets(insets)));
    }

    private void assertCutoutIsConsistentWithInset(String position, DisplayCutout cutout,
            int safeInsetSize, Rect appBound) {
        if (safeInsetSize > 0) {
            assertThat("cutout must have a bound on the " + position,
                    hasBound(position, cutout, appBound), is(true));
        } else {
            assertThat("cutout  must have no bound on the " + position,
                    hasBound(position, cutout, appBound), is(false));
        }
    }

    public void assertCutoutsAreConsistentWithInsets(TestActivity activity, DisplayCutout cutout) {
        final Rect appBounds = getAppBounds(activity);
        assertCutoutIsConsistentWithInset(TOP, cutout, cutout.getSafeInsetTop(), appBounds);
        assertCutoutIsConsistentWithInset(BOTTOM, cutout, cutout.getSafeInsetBottom(), appBounds);
        assertCutoutIsConsistentWithInset(LEFT, cutout, cutout.getSafeInsetLeft(), appBounds);
        assertCutoutIsConsistentWithInset(RIGHT, cutout, cutout.getSafeInsetRight(), appBounds);
    }

    private void assertSafeInsetsValid(DisplayCutout displayCutout) {
        //noinspection unchecked
        assertThat("all safe insets must be non-negative", safeInsets(displayCutout),
                insetValues(everyItem((Matcher)greaterThanOrEqualTo(0))));
        assertThat("at least one safe inset must be positive,"
                        + " otherwise WindowInsets.getDisplayCutout()) must return null",
                safeInsets(displayCutout), insetValues(hasItem(greaterThan(0))));
    }

    private void assertCutoutsAreWithinSafeInsets(TestActivity a, DisplayCutout cutout) {
        final Rect safeRect = getSafeRect(a, cutout);

        assertThat("safe insets must not cover the entire screen", safeRect.isEmpty(), is(false));
        for (Rect boundingRect : cutout.getBoundingRects()) {
            assertThat("boundingRects must not extend beyond safeInsets",
                    boundingRect, not(intersectsWith(safeRect)));
        }
    }

    private void assertAtMostOneCutoutPerEdge(TestActivity a, DisplayCutout cutout) {
        final Rect safeRect = getSafeRect(a, cutout);

        assertThat("must not have more than one left cutout",
                boundsWith(cutout, (r) -> r.right <= safeRect.left), hasSize(lessThanOrEqualTo(1)));
        assertThat("must not have more than one top cutout",
                boundsWith(cutout, (r) -> r.bottom <= safeRect.top), hasSize(lessThanOrEqualTo(1)));
        assertThat("must not have more than one right cutout",
                boundsWith(cutout, (r) -> r.left >= safeRect.right), hasSize(lessThanOrEqualTo(1)));
        assertThat("must not have more than one bottom cutout",
                boundsWith(cutout, (r) -> r.top >= safeRect.bottom), hasSize(lessThanOrEqualTo(1)));
    }

    private void assertBoundsAreNonEmpty(DisplayCutout cutout) {
        for (Rect boundingRect : cutout.getBoundingRects()) {
            assertThat("rect in boundingRects must not be empty",
                    boundingRect.isEmpty(), is(false));
        }
    }

    private void assertOnlyShortEdgeHasInsets(TestActivity activity,
            DisplayCutout displayCutout) {
        final Rect appBounds = getAppBounds(activity);
        if (appBounds.height() > appBounds.width()) {
            // Portrait display
            assertThat("left edge has a cutout despite being long edge",
                    displayCutout.getSafeInsetLeft(), is(0));
            assertThat("right edge has a cutout despite being long edge",
                    displayCutout.getSafeInsetRight(), is(0));
        }
        if (appBounds.height() < appBounds.width()) {
            // Landscape display
            assertThat("top edge has a cutout despite being long edge",
                    displayCutout.getSafeInsetTop(), is(0));
            assertThat("bottom edge has a cutout despite being long edge",
                    displayCutout.getSafeInsetBottom(), is(0));
        }
    }

    private void assertOnlyShortEdgeHasBounds(TestActivity activity, DisplayCutout cutout) {
        final Rect appBounds = getAppBounds(activity);
        if (appBounds.height() > appBounds.width()) {
            // Portrait display
            assertThat("left edge has a cutout despite being long edge",
                    hasBound(LEFT, cutout, appBounds), is(false));

            assertThat("right edge has a cutout despite being long edge",
                    hasBound(RIGHT, cutout, appBounds), is(false));
        }
        if (appBounds.height() < appBounds.width()) {
            // Landscape display
            assertThat("top edge has a cutout despite being long edge",
                    hasBound(TOP, cutout, appBounds), is(false));

            assertThat("bottom edge has a cutout despite being long edge",
                    hasBound(BOTTOM, cutout, appBounds), is(false));
        }
    }

    private boolean hasBound(String position, DisplayCutout cutout, Rect appBound) {
        final Rect cutoutRect;
        final int waterfallSize;
        if (LEFT.equals(position)) {
            cutoutRect = cutout.getBoundingRectLeft();
            waterfallSize = cutout.getWaterfallInsets().left;
        } else if (TOP.equals(position)) {
            cutoutRect = cutout.getBoundingRectTop();
            waterfallSize = cutout.getWaterfallInsets().top;
        } else if (RIGHT.equals(position)) {
            cutoutRect = cutout.getBoundingRectRight();
            waterfallSize = cutout.getWaterfallInsets().right;
        } else {
            cutoutRect = cutout.getBoundingRectBottom();
            waterfallSize = cutout.getWaterfallInsets().bottom;
        }
        return Rect.intersects(cutoutRect, appBound) || waterfallSize > 0;
    }

    private List<Rect> boundsWith(DisplayCutout cutout, Predicate<Rect> predicate) {
        return cutout.getBoundingRects().stream().filter(predicate).collect(Collectors.toList());
    }

    private static Rect safeInsets(DisplayCutout displayCutout) {
        if (displayCutout == null) {
            return null;
        }
        return new Rect(displayCutout.getSafeInsetLeft(), displayCutout.getSafeInsetTop(),
                displayCutout.getSafeInsetRight(), displayCutout.getSafeInsetBottom());
    }

    private static Rect systemWindowInsets(WindowInsets insets) {
        return new Rect(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
    }

    private static Rect stableInsets(WindowInsets insets) {
        return new Rect(insets.getStableInsetLeft(), insets.getStableInsetTop(),
                insets.getStableInsetRight(), insets.getStableInsetBottom());
    }

    private Rect getSafeRect(TestActivity a, DisplayCutout cutout) {
        final Rect safeRect = safeInsets(cutout);
        safeRect.bottom = getOnMainSync(() -> a.getDecorView().getHeight()) - safeRect.bottom;
        safeRect.right = getOnMainSync(() -> a.getDecorView().getWidth()) - safeRect.right;
        return safeRect;
    }

    private Rect getAppBounds(TestActivity a) {
        final Rect appBounds = new Rect();
        runOnMainSync(() -> {
            appBounds.right = a.getDecorView().getWidth();
            appBounds.bottom = a.getDecorView().getHeight();
        });
        return appBounds;
    }

    private static Matcher<Rect> insetsLessThanOrEqualTo(Rect max) {
        return new CustomTypeSafeMatcher<Rect>("must be smaller on each side than " + max) {
            @Override
            protected boolean matchesSafely(Rect actual) {
                return actual.left <= max.left && actual.top <= max.top
                        && actual.right <= max.right && actual.bottom <= max.bottom;
            }
        };
    }

    private static Matcher<Rect> intersectsWith(Rect safeRect) {
        return new CustomTypeSafeMatcher<Rect>("intersects with " + safeRect) {
            @Override
            protected boolean matchesSafely(Rect item) {
                return Rect.intersects(safeRect, item);
            }
        };
    }

    private static Matcher<Rect> insetValues(Matcher<Iterable<? super Integer>> valuesMatcher) {
        return new FeatureMatcher<Rect, Iterable<Integer>>(valuesMatcher, "inset values",
                "inset values") {
            @Override
            protected Iterable<Integer> featureValueOf(Rect actual) {
                return Arrays.asList(actual.left, actual.top, actual.right, actual.bottom);
            }
        };
    }

    private <T> void assertThat(String reason, T actual, Matcher<? super T> matcher) {
        mErrorCollector.checkThat(reason, actual, matcher);
    }

    private <R> R getOnMainSync(Supplier<R> f) {
        final Object[] result = new Object[1];
        runOnMainSync(() -> result[0] = f.get());
        //noinspection unchecked
        return (R) result[0];
    }

    private void runOnMainSync(Runnable runnable) {
        getInstrumentation().runOnMainSync(runnable);
    }

    private <T extends TestActivity> T launchAndWait(ActivityTestRule<T> rule, int cutoutMode,
            int orientation) {
        final T activity = rule.launchActivity(
                new Intent().putExtra(EXTRA_CUTOUT_MODE, cutoutMode)
                        .putExtra(EXTRA_ORIENTATION, orientation));
        PollingCheck.waitFor(activity::hasWindowFocus);
        PollingCheck.waitFor(() -> {
            final Rect appBounds = getAppBounds(activity);
            final Point displaySize = new Point();
            activity.getDisplay().getRealSize(displaySize);
            // During app launch into a different rotation, we have temporarily have the display
            // in a different rotation than the app itself. Wait for this to settle.
            return (appBounds.width() > appBounds.height()) == (displaySize.x > displaySize.y);
        });
        return activity;
    }

    private boolean supportsOrientation(int orientation) {
        String systemFeature = "";
        switch(orientation) {
            case SCREEN_ORIENTATION_PORTRAIT:
            case SCREEN_ORIENTATION_REVERSE_PORTRAIT:
                systemFeature = PackageManager.FEATURE_SCREEN_PORTRAIT;
                break;
            case SCREEN_ORIENTATION_LANDSCAPE:
            case SCREEN_ORIENTATION_REVERSE_LANDSCAPE:
                systemFeature = PackageManager.FEATURE_SCREEN_LANDSCAPE;
                break;
            default:
                throw new UnsupportedOperationException("Orientation not supported");
        }

        return getInstrumentation().getTargetContext().getPackageManager()
                .hasSystemFeature(systemFeature);
    }

    public static class TestActivity extends Activity {

        static final String EXTRA_CUTOUT_MODE = "extra.cutout_mode";
        static final String EXTRA_ORIENTATION = "extra.orientation";
        private WindowInsets mDispatchedInsets;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().requestFeature(Window.FEATURE_NO_TITLE);
            if (getIntent() != null) {
                getWindow().getAttributes().layoutInDisplayCutoutMode = getIntent().getIntExtra(
                        EXTRA_CUTOUT_MODE, LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
                setRequestedOrientation(getIntent().getIntExtra(
                        EXTRA_ORIENTATION, SCREEN_ORIENTATION_UNSPECIFIED));
            }
            View view = new View(this);
            view.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
            view.setOnApplyWindowInsetsListener((v, insets) -> mDispatchedInsets = insets);
            setContentView(view);
        }

        @Override
        public void onWindowFocusChanged(boolean hasFocus) {
            if (hasFocus) {
                getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
            }
        }

        View getDecorView() {
            return getWindow().getDecorView();
        }

        WindowInsets getRootInsets() {
            return getWindow().getDecorView().getRootWindowInsets();
        }

        WindowInsets getDispatchedInsets() {
            return mDispatchedInsets;
        }
    }

    interface TestDef {
        void run(TestActivity a, WindowInsets insets, DisplayCutout cutout, Which whichInsets);

        enum Which {
            DISPATCHED, ROOT
        }
    }
}
