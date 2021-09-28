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
 * limitations under the License
 */

package android.server.wm;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.junit.Assert.assertThat;
import static org.junit.Assume.assumeThat;

import android.app.Activity;
import android.platform.test.annotations.Presubmit;
import android.view.Display;

import androidx.test.rule.ActivityTestRule;

import org.junit.Rule;
import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:AspectRatioTests
 */
@Presubmit
public class AspectRatioTests extends AspectRatioTestsBase {

    // The max. aspect ratio the test activities are using.
    private static final float MAX_ASPECT_RATIO = 1.0f;

    // The min. aspect ratio the test activities are using.
    private static final float MIN_ASPECT_RATIO = 3.0f;

    // Test target activity that has maxAspectRatio="true" and resizeableActivity="false".
    public static class MaxAspectRatioActivity extends Activity {
    }

    // Test target activity that has maxAspectRatio="1.0" and resizeableActivity="true".
    public static class MaxAspectRatioResizeableActivity extends Activity {
    }

    // Test target activity that has no maxAspectRatio defined and resizeableActivity="false".
    public static class MaxAspectRatioUnsetActivity extends Activity {
    }

    // Test target activity that has maxAspectRatio defined as
    //   <meta-data android:name="android.max_aspect" android:value="1.0" />
    // and resizeableActivity="false".
    public static class MetaDataMaxAspectRatioActivity extends Activity {
    }

    // Test target activity that has minAspectRatio="true" and resizeableActivity="false".
    public static class MinAspectRatioActivity extends Activity {
    }

    // Test target activity that has minAspectRatio="5.0" and resizeableActivity="true".
    public static class MinAspectRatioResizeableActivity extends Activity {
    }

    // Test target activity that has no minAspectRatio defined and resizeableActivity="false".
    public static class MinAspectRatioUnsetActivity extends Activity {
    }

    // Test target activity that has minAspectRatio="true", resizeableActivity="false",
    // and screenOrientation="landscape".
    public static class MinAspectRatioLandscapeActivity extends Activity {
    }

    // Test target activity that has minAspectRatio="true", resizeableActivity="false",
    // and screenOrientation="portrait".
    public static class MinAspectRatioPortraitActivity extends Activity {
    }

    @Rule
    public ActivityTestRule<MaxAspectRatioActivity> mMaxAspectRatioActivity =
            new ActivityTestRule<>(MaxAspectRatioActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MaxAspectRatioResizeableActivity> mMaxAspectRatioResizeableActivity =
            new ActivityTestRule<>(MaxAspectRatioResizeableActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MetaDataMaxAspectRatioActivity> mMetaDataMaxAspectRatioActivity =
            new ActivityTestRule<>(MetaDataMaxAspectRatioActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MaxAspectRatioUnsetActivity> mMaxAspectRatioUnsetActivity =
            new ActivityTestRule<>(MaxAspectRatioUnsetActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MinAspectRatioActivity> mMinAspectRatioActivity =
            new ActivityTestRule<>(MinAspectRatioActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MinAspectRatioResizeableActivity> mMinAspectRatioResizeableActivity =
            new ActivityTestRule<>(MinAspectRatioResizeableActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MinAspectRatioUnsetActivity> mMinAspectRatioUnsetActivity =
            new ActivityTestRule<>(MinAspectRatioUnsetActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MinAspectRatioLandscapeActivity> mMinAspectRatioLandscapeActivity =
            new ActivityTestRule<>(MinAspectRatioLandscapeActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Rule
    public ActivityTestRule<MinAspectRatioPortraitActivity> mMinAspectRatioPortraitActivity =
            new ActivityTestRule<>(MinAspectRatioPortraitActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Test
    public void testMaxAspectRatio() {
        // Activity has a maxAspectRatio, assert that the actual ratio is less than that.
        runAspectRatioTest(mMaxAspectRatioActivity, (actual, displayId, size) -> {
            assertThat(actual, lessThanOrEqualTo(MAX_ASPECT_RATIO));
        });
    }

    @Test
    public void testMetaDataMaxAspectRatio() {
        // Activity has a maxAspectRatio, assert that the actual ratio is less than that.
        runAspectRatioTest(mMetaDataMaxAspectRatioActivity, (actual, displayId, size) -> {
            assertThat(actual, lessThanOrEqualTo(MAX_ASPECT_RATIO));
        });
    }

    @Test
    public void testMaxAspectRatioResizeableActivity() {
        // Since this activity is resizeable, its max aspect ratio should be ignored.
        runAspectRatioTest(mMaxAspectRatioResizeableActivity, (actual, displayId, size) -> {
            // TODO(b/69982434): Add ability to get native aspect ratio non-default display.
            assumeThat(displayId, is(Display.DEFAULT_DISPLAY));

            final float defaultDisplayAspectRatio = getDefaultDisplayAspectRatio();
            assertThat(actual, greaterThanOrEqualToInexact(defaultDisplayAspectRatio));
        });
    }

    @Test
    public void testMaxAspectRatioUnsetActivity() {
        // Since this activity didn't set an explicit maxAspectRatio, there should be no such
        // ratio enforced.
        runAspectRatioTest(mMaxAspectRatioUnsetActivity, (actual, displayId, size) -> {
            // TODO(b/69982434): Add ability to get native aspect ratio non-default display.
            assumeThat(displayId, is(Display.DEFAULT_DISPLAY));

            assertThat(actual, greaterThanOrEqualToInexact(getDefaultDisplayAspectRatio()));
        });
    }

    @Test
    public void testMinAspectRatio() {
        // Activity has a minAspectRatio, assert the ratio is at least that.
        runAspectRatioTest(mMinAspectRatioActivity, (actual, displayId, size) -> {
            assertThat(actual, greaterThanOrEqualToInexact(MIN_ASPECT_RATIO));
        });
    }

    @Test
    public void testMinAspectRatioResizeableActivity() {
        // Since this activity is resizeable, the minAspectRatio should be ignored.
        runAspectRatioTest(mMinAspectRatioResizeableActivity, (actual, displayId, size) -> {
            // TODO(b/69982434): Add ability to get native aspect ratio non-default display.
            assumeThat(displayId, is(Display.DEFAULT_DISPLAY));

            assertThat(actual, lessThanOrEqualToInexact(getDefaultDisplayAspectRatio()));
        });
    }

    @Test
    public void testMinAspectRatioUnsetActivity() {
        // Since this activity didn't set an explicit minAspectRatio, there should be no such
        // ratio enforced.
        runAspectRatioTest(mMinAspectRatioUnsetActivity, (actual, displayId, size) -> {
            // TODO(b/69982434): Add ability to get native aspect ratio non-default display.
            assumeThat(displayId, is(Display.DEFAULT_DISPLAY));

            assertThat(actual, lessThanOrEqualToInexact(getDefaultDisplayAspectRatio()));
        });
    }

    @Test
    public void testMinAspectLandscapeActivity() {
        // Activity has requested a fixed orientation, assert the orientation is that.
        runAspectRatioTest(mMinAspectRatioLandscapeActivity, (actual, displayId, size) -> {
            assertThat(actual, greaterThanOrEqualToInexact(MIN_ASPECT_RATIO));
            assertThat(size.x, greaterThan(size.y));
        });
    }

    @Test
    public void testMinAspectPortraitActivity() {
        // Activity has requested a fixed orientation, assert the orientation is that.
        runAspectRatioTest(mMinAspectRatioPortraitActivity, (actual, displayId, size) -> {
            assertThat(actual, greaterThanOrEqualToInexact(MIN_ASPECT_RATIO));
            assertThat(size.y, greaterThan(size.x));
        });
    }
}
