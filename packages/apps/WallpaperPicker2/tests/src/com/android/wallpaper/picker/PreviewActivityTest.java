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
package com.android.wallpaper.picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.widget.TextView;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.wallpaper.R;
import com.android.wallpaper.module.Injector;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.module.WallpaperPersister;
import com.android.wallpaper.testing.TestAsset;
import com.android.wallpaper.testing.TestExploreIntentChecker;
import com.android.wallpaper.testing.TestInjector;
import com.android.wallpaper.testing.TestUserEventLogger;
import com.android.wallpaper.testing.TestWallpaperInfo;
import com.android.wallpaper.testing.TestWallpaperPersister;
import com.android.wallpaper.util.ScreenSizeCalculator;
import com.android.wallpaper.util.WallpaperCropUtils;

import com.davemorrissey.labs.subscaleview.SubsamplingScaleImageView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link PreviewActivity}.
 */
@RunWith(AndroidJUnit4ClassRunner.class)
@MediumTest
public class PreviewActivityTest {

    private static final float FLOAT_ERROR_MARGIN = 0.001f;
    private static final String ACTION_URL = "http://google.com";

    private TestWallpaperInfo mMockWallpaper;
    private Injector mInjector;
    private TestWallpaperPersister mWallpaperPersister;
    private TestUserEventLogger mEventLogger;
    private TestExploreIntentChecker mExploreIntentChecker;

    @Rule
    public ActivityTestRule<PreviewActivity> mActivityRule =
            new ActivityTestRule<>(PreviewActivity.class, false, false);

    @Before
    public void setUp() {

        mInjector = new TestInjector();
        InjectorProvider.setInjector(mInjector);

        Intents.init();

        mMockWallpaper = new TestWallpaperInfo(TestWallpaperInfo.COLOR_BLACK);
        List<String> attributions = new ArrayList<>();
        attributions.add("Title");
        attributions.add("Subtitle 1");
        attributions.add("Subtitle 2");
        mMockWallpaper.setAttributions(attributions);
        mMockWallpaper.setCollectionId("collection");
        mMockWallpaper.setWallpaperId("wallpaper");
        mMockWallpaper.setActionUrl(ACTION_URL);

        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mWallpaperPersister = (TestWallpaperPersister) mInjector.getWallpaperPersister(context);
        mEventLogger = (TestUserEventLogger) mInjector.getUserEventLogger(context);
        mExploreIntentChecker = (TestExploreIntentChecker)
                mInjector.getExploreIntentChecker(context);
    }

    @After
    public void tearDown() {
        Intents.release();
        mActivityRule.finishActivity();
    }

    private void launchActivityIntentWithMockWallpaper() {
        Intent intent = PreviewActivity.newIntent(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), mMockWallpaper);
        intent.putExtra(BasePreviewActivity.EXTRA_TESTING_MODE_ENABLED, true);

        mActivityRule.launchActivity(intent);
    }

    private void finishSettingWallpaper() throws Throwable {
        mActivityRule.runOnUiThread(() -> mWallpaperPersister.finishSettingWallpaper());
    }

    @Test
    public void testRendersWallpaperDrawableFromIntent() {
        launchActivityIntentWithMockWallpaper();
        PreviewActivity activity = mActivityRule.getActivity();
        SubsamplingScaleImageView mosaicView = activity.findViewById(R.id.full_res_image);

        assertTrue(mosaicView.hasImage());
    }

    @Test
    public void testClickSetWallpaper_Success_HomeScreen() throws Throwable {
        launchActivityIntentWithMockWallpaper();
        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Home screen".
        onView(withText(R.string.set_wallpaper_home_screen_destination)).perform(click());

        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());

        finishSettingWallpaper();

        // Mock system wallpaper bitmap should be equal to the mock WallpaperInfo's bitmap.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Bitmap srcBitmap = ((TestAsset) mMockWallpaper.getAsset(context)).getBitmap();
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentHomeWallpaper()));

        // The wallpaper should have been set on the home screen.
        assertEquals(WallpaperPersister.DEST_HOME_SCREEN, mWallpaperPersister.getLastDestination());
        assertEquals(1, mEventLogger.getNumWallpaperSetEvents());

        assertEquals(1, mEventLogger.getNumWallpaperSetResultEvents());
        assertEquals(UserEventLogger.WALLPAPER_SET_RESULT_SUCCESS,
                mEventLogger.getLastWallpaperSetResult());
    }

    @Test
    public void testClickSetWallpaper_Success_LockScreen() throws Throwable {
        launchActivityIntentWithMockWallpaper();
        assertNull(mWallpaperPersister.getCurrentLockWallpaper());

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Lock screen."
        onView(withText(R.string.set_wallpaper_lock_screen_destination)).perform(click());

        assertNull(mWallpaperPersister.getCurrentLockWallpaper());

        finishSettingWallpaper();

        // Mock system wallpaper bitmap should be equal to the mock WallpaperInfo's bitmap.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Bitmap srcBitmap = ((TestAsset) mMockWallpaper.getAsset(context)).getBitmap();
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentLockWallpaper()));

        // The wallpaper should have been set on the lock screen.
        assertEquals(WallpaperPersister.DEST_LOCK_SCREEN, mWallpaperPersister.getLastDestination());
        assertEquals(1, mEventLogger.getNumWallpaperSetEvents());

        assertEquals(1, mEventLogger.getNumWallpaperSetResultEvents());
        assertEquals(UserEventLogger.WALLPAPER_SET_RESULT_SUCCESS,
                mEventLogger.getLastWallpaperSetResult());
    }

    @Test
    public void testClickSetWallpaper_Success_BothHomeAndLockScreen() throws Throwable {
        launchActivityIntentWithMockWallpaper();
        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());
        assertNull(mWallpaperPersister.getCurrentLockWallpaper());

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Both."
        onView(withText(R.string.set_wallpaper_both_destination)).perform(click());

        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());
        assertNull(mWallpaperPersister.getCurrentLockWallpaper());

        finishSettingWallpaper();

        // Mock system wallpaper bitmap should be equal to the mock WallpaperInfo's bitmap.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Bitmap srcBitmap = ((TestAsset) mMockWallpaper.getAsset(context)).getBitmap();
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentHomeWallpaper()));
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentLockWallpaper()));

        // The wallpaper should have been set on both the home and the lock screen.
        assertEquals(WallpaperPersister.DEST_BOTH, mWallpaperPersister.getLastDestination());
        assertEquals(1, mEventLogger.getNumWallpaperSetEvents());

        assertEquals(1, mEventLogger.getNumWallpaperSetResultEvents());
        assertEquals(UserEventLogger.WALLPAPER_SET_RESULT_SUCCESS,
                mEventLogger.getLastWallpaperSetResult());
    }

    @Test
    public void testClickSetWallpaper_Fails_HomeScreen_ShowsErrorDialog()
            throws Throwable {
        launchActivityIntentWithMockWallpaper();
        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());

        mWallpaperPersister.setFailNextCall(true);

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Home screen."
        onView(withText(R.string.set_wallpaper_home_screen_destination)).perform(click());

        finishSettingWallpaper();

        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());
        onView(withText(R.string.set_wallpaper_error_message)).check(matches(isDisplayed()));

        assertEquals(1, mEventLogger.getNumWallpaperSetResultEvents());
        assertEquals(UserEventLogger.WALLPAPER_SET_RESULT_FAILURE,
                mEventLogger.getLastWallpaperSetResult());

        // Set next call to succeed and current wallpaper bitmap should not be null and equals to
        // the
        // mock wallpaper bitmap after clicking "try again".
        mWallpaperPersister.setFailNextCall(false);

        onView(withText(R.string.try_again)).perform(click());
        finishSettingWallpaper();

        assertNotNull(mWallpaperPersister.getCurrentHomeWallpaper());
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Bitmap srcBitmap = ((TestAsset) mMockWallpaper.getAsset(context)).getBitmap();
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentHomeWallpaper()));

        // The wallpaper should have been set on the home screen.
        assertEquals(WallpaperPersister.DEST_HOME_SCREEN, mWallpaperPersister.getLastDestination());
    }

    @Test
    public void testClickSetWallpaper_Fails_LockScreen_ShowsErrorDialog()
            throws Throwable {
        launchActivityIntentWithMockWallpaper();
        assertNull(mWallpaperPersister.getCurrentLockWallpaper());

        mWallpaperPersister.setFailNextCall(true);

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Lock screen."
        onView(withText(R.string.set_wallpaper_lock_screen_destination)).perform(click());

        finishSettingWallpaper();

        assertNull(mWallpaperPersister.getCurrentLockWallpaper());
        onView(withText(R.string.set_wallpaper_error_message)).check(matches(isDisplayed()));

        assertEquals(1, mEventLogger.getNumWallpaperSetResultEvents());
        assertEquals(UserEventLogger.WALLPAPER_SET_RESULT_FAILURE,
                mEventLogger.getLastWallpaperSetResult());

        // Set next call to succeed and current wallpaper bitmap should not be null and equals to
        // the
        // mock wallpaper bitmap after clicking "try again".
        mWallpaperPersister.setFailNextCall(false);

        onView(withText(R.string.try_again)).perform(click());
        finishSettingWallpaper();

        assertNotNull(mWallpaperPersister.getCurrentLockWallpaper());
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Bitmap srcBitmap = ((TestAsset) mMockWallpaper.getAsset(context)).getBitmap();
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentLockWallpaper()));

        // The wallpaper should have been set on the lock screen.
        assertEquals(WallpaperPersister.DEST_LOCK_SCREEN, mWallpaperPersister.getLastDestination());
    }

    @Test
    public void testClickSetWallpaper_Fails_BothHomeAndLock_ShowsErrorDialog()
            throws Throwable {
        launchActivityIntentWithMockWallpaper();
        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());
        assertNull(mWallpaperPersister.getCurrentLockWallpaper());

        mWallpaperPersister.setFailNextCall(true);

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Both."
        onView(withText(R.string.set_wallpaper_both_destination)).perform(click());

        finishSettingWallpaper();

        assertNull(mWallpaperPersister.getCurrentHomeWallpaper());
        assertNull(mWallpaperPersister.getCurrentLockWallpaper());
        onView(withText(R.string.set_wallpaper_error_message)).check(matches(isDisplayed()));

        assertEquals(1, mEventLogger.getNumWallpaperSetResultEvents());
        assertEquals(UserEventLogger.WALLPAPER_SET_RESULT_FAILURE,
                mEventLogger.getLastWallpaperSetResult());

        // Set next call to succeed and current wallpaper bitmap should not be null and equals to
        // the mock wallpaper bitmap after clicking "try again".
        mWallpaperPersister.setFailNextCall(false);

        onView(withText(R.string.try_again)).perform(click());
        finishSettingWallpaper();

        assertNotNull(mWallpaperPersister.getCurrentHomeWallpaper());
        assertNotNull(mWallpaperPersister.getCurrentLockWallpaper());
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Bitmap srcBitmap = ((TestAsset) mMockWallpaper.getAsset(context)).getBitmap();
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentHomeWallpaper()));
        assertTrue(srcBitmap.sameAs(mWallpaperPersister.getCurrentLockWallpaper()));

        // The wallpaper should have been set on both the home screen and the lock screen.
        assertEquals(WallpaperPersister.DEST_BOTH, mWallpaperPersister.getLastDestination());
    }

    @Test
    public void testClickSetWallpaper_CropsAndScalesWallpaper() {
        launchActivityIntentWithMockWallpaper();
        // Scale should not have a meaningful value before clicking "set wallpaper".
        assertTrue(mWallpaperPersister.getScale() < 0);

        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Home screen".
        onView(withText(R.string.set_wallpaper_home_screen_destination)).perform(click());

        // WallpaperPersister's scale should match the ScaleImageView's scale.
        float zoom = ((SubsamplingScaleImageView)
                mActivityRule.getActivity().findViewById(R.id.full_res_image)).getScale();
        assertEquals(mWallpaperPersister.getScale(), zoom, FLOAT_ERROR_MARGIN);

        Point screenSize = ScreenSizeCalculator.getInstance().getScreenSize(
                mActivityRule.getActivity().getWindowManager().getDefaultDisplay());
        int maxDim = Math.max(screenSize.x, screenSize.y);
        Rect cropRect = mWallpaperPersister.getCropRect();

        // Crop rect should be greater or equal than screen size in both directions.
        assertTrue(cropRect.width() >= maxDim);
        assertTrue(cropRect.height() >= maxDim);
    }

    @Test
    public void testClickSetWallpaper_FailsCroppingAndScalingWallpaper_ShowsErrorDialog()
            throws Throwable {
        launchActivityIntentWithMockWallpaper();
        mWallpaperPersister.setFailNextCall(true);
        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());
        // Destination dialog is shown; click "Home screen".
        onView(withText(R.string.set_wallpaper_home_screen_destination)).perform(click());

        finishSettingWallpaper();

        onView(withText(R.string.set_wallpaper_error_message)).check(matches(isDisplayed()));
    }

    /**
     * Tests that tapping Set Wallpaper shows the destination dialog (i.e., choosing
     * between Home screen, Lock screen, or Both).
     */
    @Test
    public void testClickSetWallpaper_ShowsDestinationDialog() {
        launchActivityIntentWithMockWallpaper();
        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());
        onView(withText(R.string.set_wallpaper_dialog_message)).check(matches(isDisplayed()));
    }

    @Test
    public void testSetsDefaultWallpaperZoomAndScroll() {
        float expectedWallpaperZoom;
        int expectedWallpaperScrollX;
        int expectedWallpaperScrollY;

        launchActivityIntentWithMockWallpaper();
        PreviewActivity activity = mActivityRule.getActivity();
        SubsamplingScaleImageView fullResImageView = activity.findViewById(R.id.full_res_image);

        Point defaultCropSurfaceSize = WallpaperCropUtils.getDefaultCropSurfaceSize(
                activity.getResources(), activity.getWindowManager().getDefaultDisplay());
        Point screenSize = ScreenSizeCalculator.getInstance().getScreenSize(
                activity.getWindowManager().getDefaultDisplay());
        TestAsset asset = (TestAsset) mMockWallpaper.getAsset(activity);
        Point wallpaperSize = new Point(asset.getBitmap().getWidth(),
                asset.getBitmap().getHeight());

        expectedWallpaperZoom = WallpaperCropUtils.calculateMinZoom(
                wallpaperSize, defaultCropSurfaceSize);

        // Current zoom should match the minimum zoom required to fit wallpaper
        // completely on the crop surface.
        assertEquals(expectedWallpaperZoom, fullResImageView.getScale(), FLOAT_ERROR_MARGIN);

        Point scaledWallpaperSize = new Point(
                (int) (wallpaperSize.x * expectedWallpaperZoom),
                (int) (wallpaperSize.y * expectedWallpaperZoom));
        Point cropSurfaceToScreen = WallpaperCropUtils.calculateCenterPosition(
                defaultCropSurfaceSize, screenSize, true /* alignStart */, false /* isRtl */);
        Point wallpaperToCropSurface = WallpaperCropUtils.calculateCenterPosition(
                scaledWallpaperSize, defaultCropSurfaceSize, false /* alignStart */,
                false /* isRtl */);

        expectedWallpaperScrollX = wallpaperToCropSurface.x + cropSurfaceToScreen.x;
        expectedWallpaperScrollY = wallpaperToCropSurface.y + cropSurfaceToScreen.y;

        // ZoomView should be scrolled in X and Y directions such that the crop surface is centered
        // relative to the wallpaper and the screen is centered (and aligned left) relative to the
        // crop surface.
        assertEquals(expectedWallpaperScrollX, fullResImageView.getScrollX());
        assertEquals(expectedWallpaperScrollY, fullResImageView.getScrollY());
    }

    @Test
    public void testShowSetWallpaperDialog_TemporarilyLocksScreenOrientation() {
        launchActivityIntentWithMockWallpaper();
        PreviewActivity activity = mActivityRule.getActivity();
        assertNotEquals(ActivityInfo.SCREEN_ORIENTATION_LOCKED, activity.getRequestedOrientation());

        // Show SetWallpaperDialog.
        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        assertEquals(ActivityInfo.SCREEN_ORIENTATION_LOCKED, activity.getRequestedOrientation());

        // Press back to dismiss the dialog.
        onView(isRoot()).perform(pressBack());

        assertNotEquals(ActivityInfo.SCREEN_ORIENTATION_LOCKED, activity.getRequestedOrientation());
    }

    @Test
    public void testSetWallpaper_TemporarilyLocksScreenOrientation() throws Throwable {
        launchActivityIntentWithMockWallpaper();
        PreviewActivity activity = mActivityRule.getActivity();
        assertNotEquals(ActivityInfo.SCREEN_ORIENTATION_LOCKED, activity.getRequestedOrientation());

        // Show SetWallpaperDialog.
        onView(withId(R.id.preview_attribution_pane_set_wallpaper_button)).perform(click());

        // Destination dialog is shown; click "Home screen".
        onView(withText(R.string.set_wallpaper_home_screen_destination)).perform(click());

        assertEquals(ActivityInfo.SCREEN_ORIENTATION_LOCKED, activity.getRequestedOrientation());

        // Finish setting the wallpaper to check that the screen orientation is no longer locked.
        finishSettingWallpaper();

        assertNotEquals(ActivityInfo.SCREEN_ORIENTATION_LOCKED, activity.getRequestedOrientation());
    }

    @Test
    public void testShowsWallpaperAttribution() {
        launchActivityIntentWithMockWallpaper();
        PreviewActivity activity = mActivityRule.getActivity();

        TextView titleView = activity.findViewById(R.id.preview_attribution_pane_title);
        assertEquals("Title", titleView.getText());

        TextView subtitle1View = activity.findViewById(R.id.preview_attribution_pane_subtitle1);
        assertEquals("Subtitle 1", subtitle1View.getText());

        TextView subtitle2View = activity.findViewById(R.id.preview_attribution_pane_subtitle2);
        assertEquals("Subtitle 2", subtitle2View.getText());
    }

    /**
     * Tests that if there was a failure decoding the wallpaper bitmap, then the activity shows an
     * informative error dialog with an "OK" button, when clicked finishes the activity.
     */
    @Test
    public void testLoadWallpaper_Failed_ShowsErrorDialog() {
        // Simulate a corrupted asset that fails to perform decoding operations.
        mMockWallpaper.corruptAssets();
        launchActivityIntentWithMockWallpaper();

        onView(withText(R.string.load_wallpaper_error_message)).check(matches(isDisplayed()));

        onView(withText(android.R.string.ok)).perform(click());

        assertTrue(mActivityRule.getActivity().isFinishing());
    }

    /**
     * Tests that the explore button is not visible, even if there is an action URL present, if
     * there is no activity on the device which can handle such an explore action.
     */
    @Test
    public void testNoActionViewHandler_ExploreButtonNotVisible() {
        mExploreIntentChecker.setViewHandlerExists(false);

        launchActivityIntentWithMockWallpaper();
        onView(withId(R.id.preview_attribution_pane_explore_button)).check(
                matches(not(isDisplayed())));
    }
}
