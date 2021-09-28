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

package android.server.wm;

import static android.content.pm.PackageManager.FEATURE_AUTOMOTIVE;
import static android.server.wm.EnsureBarContrastTest.TestActivity.EXTRA_ENSURE_CONTRAST;
import static android.server.wm.EnsureBarContrastTest.TestActivity.EXTRA_LIGHT_BARS;
import static android.server.wm.EnsureBarContrastTest.TestActivity.backgroundForBar;
import static android.server.wm.BarTestUtils.assumeHasColoredBars;
import static android.server.wm.BarTestUtils.assumeHasColoredNavigationBar;
import static android.server.wm.BarTestUtils.assumeHasColoredStatusBar;
import static android.server.wm.BarTestUtils.isAssumptionViolated;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;
import static org.junit.Assume.assumeFalse;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.util.SparseIntArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.hamcrest.CustomTypeSafeMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.AssumptionViolatedException;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
import org.junit.rules.RuleChain;

import java.util.function.Supplier;

/**
 * Tests for Window's setEnsureStatusBarContrastWhenTransparent and
 * setEnsureNavigationBarContrastWhenTransparent.
 */
@Presubmit
public class EnsureBarContrastTest {

    private final ErrorCollector mErrorCollector = new ErrorCollector();
    private final DumpOnFailure mDumper = new DumpOnFailure();
    private final ActivityTestRule<TestActivity> mTestActivity =
            new ActivityTestRule<>(TestActivity.class, false /* initialTouchMode */,
                    false /* launchActivity */);

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mDumper)
            .around(mErrorCollector)
            .around(mTestActivity);

    @Test
    public void test_ensureContrast_darkBars() {
        final boolean lightBars = false;
        runTestEnsureContrast(lightBars);
    }

    @Test
    public void test_ensureContrast_lightBars() {
        final boolean lightBars = true;
        runTestEnsureContrast(lightBars);
    }

    public void runTestEnsureContrast(boolean lightBars) {
        assumeHasColoredBars();
        TestActivity activity = launchAndWait(mTestActivity, lightBars, true /* ensureContrast */);
        for (Bar bar : Bar.BARS) {
            if (isAssumptionViolated(() -> bar.checkAssumptions(mTestActivity))) {
                continue;
            }

            Bitmap bitmap = getOnMainSync(() -> activity.screenshotBar(bar, mDumper));

            if (getOnMainSync(() -> activity.barIsTapThrough(bar))) {
                assertThat(bar.name + "Bar is tap through, therefore must NOT be scrimmed.", bitmap,
                        hasNoScrim(lightBars));
            } else {
                // Bar is NOT tap through, may therefore have a scrim.
            }
            assertThat(bar.name + "Bar: Ensure contrast was requested, therefore contrast " +
                    "must be ensured", bitmap, hasContrast(lightBars));
        }
    }

    @Test
    public void test_dontEnsureContrast_darkBars() {
        final boolean lightBars = false;
        runTestDontEnsureContrast(lightBars);
    }

    @Test
    public void test_dontEnsureContrast_lightBars() {
        final boolean lightBars = true;
        runTestDontEnsureContrast(lightBars);
    }

    public void runTestDontEnsureContrast(boolean lightBars) {
        assumeHasColoredBars();
        TestActivity activity = launchAndWait(mTestActivity, lightBars, false /* ensureContrast */);
        for (Bar bar : Bar.BARS) {
            if (isAssumptionViolated(() -> bar.checkAssumptions(mTestActivity))) {
                continue;
            }

            Bitmap bitmap = getOnMainSync(() -> activity.screenshotBar(bar, mDumper));

            assertThat(bar.name + "Bar: contrast NOT requested, therefore must NOT be scrimmed.",
                    bitmap, hasNoScrim(lightBars));
        }
    }

    private static Matcher<Bitmap> hasNoScrim(boolean light) {
        return new CustomTypeSafeMatcher<Bitmap>(
                "must not have a " + (light ? "light" : "dark") + " scrim") {
            @Override
            protected boolean matchesSafely(Bitmap actual) {
                int mostFrequentColor = getMostFrequentColor(actual);
                return mostFrequentColor == expectedMostFrequentColor();
            }

            @Override
            protected void describeMismatchSafely(Bitmap item, Description mismatchDescription) {
                super.describeMismatchSafely(item, mismatchDescription);
                mismatchDescription.appendText(" mostFrequentColor: expected #" +
                        Integer.toHexString(expectedMostFrequentColor()) + ", but was #" +
                        Integer.toHexString(getMostFrequentColor(item)));
            }

            private int expectedMostFrequentColor() {
                return backgroundForBar(light);
            }
        };
    }

    private static Matcher<Bitmap> hasContrast(boolean light) {
        return new CustomTypeSafeMatcher<Bitmap>(
                (light ? "light" : "dark") + " bar must have contrast") {
            @Override
            protected boolean matchesSafely(Bitmap actual) {
                int[] ps = getPixels(actual);
                int bg = backgroundForBar(light);

                for (int p : ps) {
                    if (!sameColor(p, bg)) {
                        return true;
                    }
                }
                return false;
            }

            @Override
            protected void describeMismatchSafely(Bitmap item, Description mismatchDescription) {
                super.describeMismatchSafely(item, mismatchDescription);
                mismatchDescription.appendText(" expected some color different from " +
                        backgroundForBar(light));
            }
        };
    }

    private static int[] getPixels(Bitmap bitmap) {
        int[] pixels = new int[bitmap.getHeight() * bitmap.getWidth()];
        bitmap.getPixels(pixels, 0, bitmap.getWidth(), 0, 0, bitmap.getWidth(), bitmap.getHeight());
        return pixels;
    }

    private static int getMostFrequentColor(Bitmap bitmap) {
        final int[] ps = getPixels(bitmap);
        final SparseIntArray count = new SparseIntArray();
        for (int p : ps) {
            count.put(p, count.get(p) + 1);
        }
        int max = 0;
        for (int i = 0; i < count.size(); i++) {
            if (count.valueAt(i) > count.valueAt(max)) {
                max = i;
            }
        }
        return count.keyAt(max);
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

    private <T extends TestActivity> T launchAndWait(ActivityTestRule<T> rule, boolean lightBars,
            boolean ensureContrast) {
        final T activity = rule.launchActivity(new Intent()
                .putExtra(EXTRA_LIGHT_BARS, lightBars)
                .putExtra(EXTRA_ENSURE_CONTRAST, ensureContrast));
        PollingCheck.waitFor(activity::isReady);
        activity.onEnterAnimationComplete();
        return activity;
    }

    private static boolean sameColor(int a, int b) {
        return Math.abs(Color.alpha(a) - Color.alpha(b)) +
                Math.abs(Color.red(a) - Color.red(b)) +
                Math.abs(Color.green(a) - Color.green(b)) +
                Math.abs(Color.blue(a) - Color.blue(b)) < 10;
    }

    public static class TestActivity extends Activity {

        static final String EXTRA_LIGHT_BARS = "extra.light_bars";
        static final String EXTRA_ENSURE_CONTRAST = "extra.ensure_contrast";

        private boolean mReady = false;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            View view = new View(this);
            view.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));

            if (getIntent() != null) {
                boolean lightBars = getIntent().getBooleanExtra(EXTRA_LIGHT_BARS, false);
                boolean ensureContrast = getIntent().getBooleanExtra(EXTRA_ENSURE_CONTRAST, false);

                // Install the decor
                getWindow().getDecorView();

                getWindow().setStatusBarContrastEnforced(ensureContrast);
                getWindow().setNavigationBarContrastEnforced(ensureContrast);

                getWindow().setStatusBarColor(Color.TRANSPARENT);
                getWindow().setNavigationBarColor(Color.TRANSPARENT);
                getWindow().setBackgroundDrawable(new ColorDrawable(backgroundForBar(lightBars)));

                view.setSystemUiVisibility(lightBars ? (View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
                        | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR) : 0);
            }
            setContentView(view);
        }

        @Override
        public void onEnterAnimationComplete() {
            super.onEnterAnimationComplete();
            mReady = true;
        }

        public boolean isReady() {
            return mReady && hasWindowFocus();
        }

        static int backgroundForBar(boolean lightBar) {
            return lightBar ? Color.BLACK : Color.WHITE;
        }

        boolean barIsTapThrough(Bar bar) {
            final WindowInsets insets = getWindow().getDecorView().getRootWindowInsets();

            return bar.getInset(insets.getTappableElementInsets())
                    < bar.getInset(insets.getSystemWindowInsets());
        }

        Bitmap screenshotBar(Bar bar, DumpOnFailure dumper) {
            final View dv = getWindow().getDecorView();
            final Insets insets = dv.getRootWindowInsets().getSystemWindowInsets();

            Rect r = bar.getLocation(insets,
                    new Rect(dv.getLeft(), dv.getTop(), dv.getRight(), dv.getBottom()));

            Bitmap fullBitmap = getInstrumentation().getUiAutomation().takeScreenshot();
            dumper.dumpOnFailure("full" + bar.name, fullBitmap);
            Bitmap barBitmap = Bitmap.createBitmap(fullBitmap, r.left, r.top, r.width(),
                    r.height());
            dumper.dumpOnFailure("bar" + bar.name, barBitmap);
            return barBitmap;
        }
    }

    abstract static class Bar {

        static final Bar STATUS = new Bar("Status") {
            @Override
            int getInset(Insets insets) {
                return insets.top;
            }

            @Override
            Rect getLocation(Insets insets, Rect screen) {
                final Rect r = new Rect(screen);
                r.bottom = r.top + getInset(insets);
                return r;
            }

            @Override
            void checkAssumptions(ActivityTestRule<?> rule) throws AssumptionViolatedException {
                assumeHasColoredStatusBar(rule);
            }
        };

        static final Bar NAVIGATION = new Bar("Navigation") {
            @Override
            int getInset(Insets insets) {
                return insets.bottom;
            }

            @Override
            Rect getLocation(Insets insets, Rect screen) {
                final Rect r = new Rect(screen);
                r.top = r.bottom - getInset(insets);
                return r;
            }

            @Override
            void checkAssumptions(ActivityTestRule<?> rule) throws AssumptionViolatedException {
                assumeHasColoredNavigationBar(rule);
            }
        };

        static final Bar[] BARS = {STATUS, NAVIGATION};

        final String name;

        public Bar(String name) {
            this.name = name;
        }

        abstract int getInset(Insets insets);

        abstract Rect getLocation(Insets insets, Rect screen);

        abstract void checkAssumptions(ActivityTestRule<?> rule) throws AssumptionViolatedException;
    }
}
