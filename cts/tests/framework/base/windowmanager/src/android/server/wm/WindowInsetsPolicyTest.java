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

import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_SECONDARY;
import static android.content.res.Configuration.ORIENTATION_PORTRAIT;
import static android.server.wm.app.Components.LAUNCHING_ACTIVITY;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Surface.ROTATION_0;
import static android.view.Surface.ROTATION_90;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.graphics.Insets;
import android.os.Bundle;
import android.platform.test.annotations.Presubmit;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager.LayoutParams;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.hamcrest.CustomTypeSafeMatcher;
import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;

import java.util.function.Supplier;

@Presubmit
public class WindowInsetsPolicyTest extends ActivityManagerTestBase {
    private static final String TAG = WindowInsetsPolicyTest.class.getSimpleName();

    private ComponentName mTestActivityComponentName;

    @Rule
    public final ErrorCollector mErrorCollector = new ErrorCollector();

    @Rule
    public final ActivityTestRule<TestActivity> mTestActivity =
            new ActivityTestRule<>(TestActivity.class, false /* initialTouchMode */,
                    false /* launchActivity */);

    @Rule
    public final ActivityTestRule<FullscreenTestActivity> mFullscreenTestActivity =
            new ActivityTestRule<>(FullscreenTestActivity.class, false /* initialTouchMode */,
                    false /* launchActivity */);

    @Rule
    public final ActivityTestRule<FullscreenWmFlagsTestActivity> mFullscreenWmFlagsTestActivity =
            new ActivityTestRule<>(FullscreenWmFlagsTestActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public final ActivityTestRule<ImmersiveFullscreenTestActivity> mImmersiveTestActivity =
            new ActivityTestRule<>(ImmersiveFullscreenTestActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        mTestActivityComponentName = new ComponentName(mContext, TestActivity.class);
    }

    @Test
    public void testWindowInsets_dispatched() {
        final TestActivity activity = launchAndWait(mTestActivity);

        WindowInsets insets = getOnMainSync(activity::getDispatchedInsets);
        Assert.assertThat("test setup failed, no insets dispatched", insets, notNullValue());

        commonAsserts(insets);
    }

    @Test
    public void testWindowInsets_root() {
        final TestActivity activity = launchAndWait(mTestActivity);

        WindowInsets insets = getOnMainSync(activity::getRootInsets);
        Assert.assertThat("test setup failed, no insets at root", insets, notNullValue());

        commonAsserts(insets);
    }

    /**
     * Tests whether an activity in split screen gets the top insets force consumed if
     * {@link View#SYSTEM_UI_FLAG_FULLSCREEN} is set, and doesn't otherwise.
     */
    @Test
    public void testForcedConsumedTopInsets() throws Exception {
        assumeTrue("Skipping test: no split multi-window support",
                supportsSplitScreenMultiWindow());

        mWmState.computeState(new ComponentName[] {});
        final boolean naturalOrientationPortrait =
                mWmState.getDisplay(DEFAULT_DISPLAY)
                        .mFullConfiguration.orientation == ORIENTATION_PORTRAIT;

        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(naturalOrientationPortrait ? ROTATION_90 : ROTATION_0);

        launchActivityInSplitScreenWithRecents(LAUNCHING_ACTIVITY);
        final TestActivity activity = launchAndWait(mTestActivity);
        mWmState.computeState(mTestActivityComponentName);

        mWmState.assertContainsStack("Must contain fullscreen stack.",
                WINDOWING_MODE_SPLIT_SCREEN_SECONDARY, ACTIVITY_TYPE_STANDARD);
        mWmState.assertContainsStack("Must contain docked stack.",
                WINDOWING_MODE_SPLIT_SCREEN_PRIMARY, ACTIVITY_TYPE_STANDARD);

        mWmState.computeState(LAUNCHING_ACTIVITY, mTestActivityComponentName);

        // Ensure that top insets are not consumed for LAYOUT_FULLSCREEN
        WindowInsets insets = getOnMainSync(activity::getDispatchedInsets);
        final WindowInsets rootInsets = getOnMainSync(activity::getRootInsets);
        assertEquals("top inset must be dispatched in split screen",
                rootInsets.getSystemWindowInsetTop(), insets.getSystemWindowInsetTop());

        // Ensure that top insets are fully consumed for FULLSCREEN
        final TestActivity fullscreenActivity = launchAndWait(mFullscreenTestActivity);
        insets = getOnMainSync(fullscreenActivity::getDispatchedInsets);
        assertEquals("top insets must be consumed if FULLSCREEN is set",
                0, insets.getSystemWindowInsetTop());

        // Ensure that top insets are fully consumed for FULLSCREEN when setting it over wm
        // layout params
        final TestActivity fullscreenWmFlagsActivity =
                launchAndWait(mFullscreenWmFlagsTestActivity);
        insets = getOnMainSync(fullscreenWmFlagsActivity::getDispatchedInsets);
        assertEquals("top insets must be consumed if FULLSCREEN is set",
                0, insets.getSystemWindowInsetTop());
    }

    @Test
    public void testNonAutomotiveFullScreenNotBlockedBySystemComponents() {
        assumeFalse("Skipping test: Automotive is allowed to partially block fullscreen "
                        + "applications with system bars.", isAutomotive());

        final TestActivity fullscreenActivity = launchAndWait(mFullscreenTestActivity);
        View decorView = fullscreenActivity.getDecorView();
        View contentView = decorView.findViewById(android.R.id.content);
        boolean hasFullWidth = decorView.getMeasuredWidth() == contentView.getMeasuredWidth();
        boolean hasFullHeight = decorView.getMeasuredHeight() == contentView.getMeasuredHeight();

        assertTrue(hasFullWidth && hasFullHeight);
    }

    @Test
    public void testImmersiveFullscreenHidesSystemBars() throws Throwable {
        // Run the test twice, because the issue that shows system bars even in the immersive mode,
        // happens at the 2nd try.
        for (int i = 1; i <= 2; ++i) {
            Log.d(TAG, "testImmersiveFullscreenHidesSystemBars: try" + i);

            TestActivity immersiveActivity = launchAndWait(mImmersiveTestActivity);
            WindowInsets insets = getOnMainSync(immersiveActivity::getDispatchedInsets);

            assertFalse(insets.isVisible(WindowInsets.Type.statusBars()));
            assertFalse(insets.isVisible(WindowInsets.Type.navigationBars()));

            WindowInsets rootInsets = getOnMainSync(immersiveActivity::getRootInsets);
            assertFalse(rootInsets.isVisible(WindowInsets.Type.statusBars()));
            assertFalse(rootInsets.isVisible(WindowInsets.Type.navigationBars()));

            View statusBarBgView = getOnMainSync(immersiveActivity::getStatusBarBackgroundView);
            // The status bar background view can be non-existent or invisible.
            assertTrue(statusBarBgView == null
                    || statusBarBgView.getVisibility() == android.view.View.INVISIBLE);

            View navigationBarBgView = getOnMainSync(
                    immersiveActivity::getNavigationBarBackgroundView);
            // The navigation bar background view can be non-existent or invisible.
            assertTrue(navigationBarBgView == null
                    || navigationBarBgView.getVisibility() == android.view.View.INVISIBLE);
        }
    }

    private void commonAsserts(WindowInsets insets) {
        assertForAllInsets("must be non-negative", insets, insetsGreaterThanOrEqualTo(Insets.NONE));

        assertThat("system gesture insets must include mandatory system gesture insets",
                insets.getMandatorySystemGestureInsets(),
                insetsLessThanOrEqualTo(insets.getSystemGestureInsets()));

        Insets stableAndSystem = Insets.min(insets.getSystemWindowInsets(),
                insets.getStableInsets());
        assertThat("mandatory system gesture insets must include intersection between "
                        + "stable and system window insets",
                stableAndSystem,
                insetsLessThanOrEqualTo(insets.getMandatorySystemGestureInsets()));

        assertThat("tappable insets must be at most system window insets",
                insets.getTappableElementInsets(),
                insetsLessThanOrEqualTo(insets.getSystemWindowInsets()));
    }

    private void assertForAllInsets(String reason, WindowInsets actual,
            Matcher<? super Insets> matcher) {
        assertThat("getSystemWindowInsets" + ": " + reason,
                actual.getSystemWindowInsets(), matcher);
        assertThat("getStableInsets" + ": " + reason,
                actual.getStableInsets(), matcher);
        assertThat("getSystemGestureInsets" + ": " + reason,
                actual.getSystemGestureInsets(), matcher);
        assertThat("getMandatorySystemGestureInsets" + ": " + reason,
                actual.getMandatorySystemGestureInsets(), matcher);
        assertThat("getTappableElementInsets" + ": " + reason,
                actual.getTappableElementInsets(), matcher);
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

    private <T extends Activity> T launchAndWait(ActivityTestRule<T> rule) {
        final T activity = rule.launchActivity(null);
        PollingCheck.waitFor(activity::hasWindowFocus);
        return activity;
    }

    private boolean isAutomotive() {
        return mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
    }

    private static Matcher<Insets> insetsLessThanOrEqualTo(Insets max) {
        return new CustomTypeSafeMatcher<Insets>("must be smaller on each side than " + max) {
            @Override
            protected boolean matchesSafely(Insets actual) {
                return actual.left <= max.left && actual.top <= max.top
                        && actual.right <= max.right && actual.bottom <= max.bottom;
            }
        };
    }

    private static Matcher<Insets> insetsGreaterThanOrEqualTo(Insets min) {
        return new CustomTypeSafeMatcher<Insets>("must be greater on each side than " + min) {
            @Override
            protected boolean matchesSafely(Insets actual) {
                return actual.left >= min.left && actual.top >= min.top
                        && actual.right >= min.right && actual.bottom >= min.bottom;
            }
        };
    }

    public static class TestActivity extends Activity {

        private WindowInsets mDispatchedInsets;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().requestFeature(Window.FEATURE_NO_TITLE);
            View view = new View(this);
            view.setLayoutParams(new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
            getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
            view.setOnApplyWindowInsetsListener((v, insets) -> mDispatchedInsets = insets);
            setContentView(view);
        }

        View getDecorView() {
            return getWindow().getDecorView();
        }

        View getStatusBarBackgroundView() {
            return getWindow().getStatusBarBackgroundView();
        }

        View getNavigationBarBackgroundView() {
            return getWindow().getNavigationBarBackgroundView();
        }

        WindowInsets getRootInsets() {
            return getWindow().getDecorView().getRootWindowInsets();
        }

        WindowInsets getDispatchedInsets() {
            return mDispatchedInsets;
        }
    }

    public static class FullscreenTestActivity extends TestActivity {

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getDecorView().setSystemUiVisibility(
                    getDecorView().getSystemUiVisibility() | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    public static class FullscreenWmFlagsTestActivity extends TestActivity {

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getWindow().addFlags(LayoutParams.FLAG_FULLSCREEN);
        }
    }

    public static class ImmersiveFullscreenTestActivity extends TestActivity {

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            // See https://developer.android.com/training/system-ui/immersive#EnableFullscreen
            getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    // Set the content to appear under the system bars so that the
                    // content doesn't resize when the system bars hide and show.
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    // Hide the nav bar and status bar
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }
}
