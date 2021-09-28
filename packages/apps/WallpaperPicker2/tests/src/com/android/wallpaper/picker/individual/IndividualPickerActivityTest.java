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
package com.android.wallpaper.picker.individual;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static junit.framework.TestCase.assertFalse;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.app.WallpaperManager;
import android.content.Context;
import android.content.Intent;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.wallpaper.R;
import com.android.wallpaper.model.Category;
import com.android.wallpaper.model.PickerIntentFactory;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.model.WallpaperRotationInitializer;
import com.android.wallpaper.model.WallpaperRotationInitializer.RotationInitializationState;
import com.android.wallpaper.module.FormFactorChecker;
import com.android.wallpaper.module.Injector;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.testing.TestCategoryProvider;
import com.android.wallpaper.testing.TestFormFactorChecker;
import com.android.wallpaper.testing.TestInjector;
import com.android.wallpaper.testing.TestWallpaperCategory;
import com.android.wallpaper.testing.TestWallpaperInfo;
import com.android.wallpaper.testing.TestWallpaperRotationInitializer;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link IndividualPickerActivity}.
 */
@RunWith(AndroidJUnit4ClassRunner.class)
@MediumTest
public class IndividualPickerActivityTest {

    private static final String EXTRA_WALLPAPER_INFO =
            "com.android.wallpaper.picker.wallpaper_info";
    private static final TestWallpaperInfo sWallpaperInfo1 = new TestWallpaperInfo(
            TestWallpaperInfo.COLOR_BLACK, "test-wallpaper-1");
    private static final TestWallpaperInfo sWallpaperInfo2 = new TestWallpaperInfo(
            TestWallpaperInfo.COLOR_BLACK, "test-wallpaper-2");
    private static final TestWallpaperInfo sWallpaperInfo3 = new TestWallpaperInfo(
            TestWallpaperInfo.COLOR_BLACK, "test-wallpaper-3");

    private TestCategoryProvider mTestCategoryProvider;

    private TestFormFactorChecker mTestFormFactorChecker;
    private Injector mInjector;

    private TestWallpaperCategory mTestCategory;

    @Rule
    public ActivityTestRule<IndividualPickerActivity> mActivityRule =
            new ActivityTestRule<>(IndividualPickerActivity.class, false, false);

    @Before
    public void setUp() {
        Intents.init();

        mInjector = new TestInjector();
        InjectorProvider.setInjector(mInjector);

        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mTestFormFactorChecker = (TestFormFactorChecker) mInjector.getFormFactorChecker(context);
        mTestCategoryProvider = (TestCategoryProvider) mInjector.getCategoryProvider(context);

        sWallpaperInfo1.setAttributions(Arrays.asList(
                "Attribution 0", "Attribution 1", "Attribution 2"));
    }

    @After
    public void tearDown() {
        Intents.release();
        mActivityRule.finishActivity();
    }

    private IndividualPickerActivity getActivity() {
        return mActivityRule.getActivity();
    }

    private void setUpFragmentForTesting() {
        IndividualPickerFragment fragment = (IndividualPickerFragment)
                getActivity().getSupportFragmentManager().findFragmentById(R.id.fragment_container);
        fragment.setTestingMode(true);
    }

    private void setActivityWithMockWallpapers(boolean isRotationEnabled,
            @RotationInitializationState int rotationState) {
        sWallpaperInfo1.setCollectionId("collection");

        ArrayList<WallpaperInfo> wallpapers = new ArrayList<>();
        wallpapers.add(sWallpaperInfo1);
        wallpapers.add(sWallpaperInfo2);
        wallpapers.add(sWallpaperInfo3);

        mTestCategory = new TestWallpaperCategory(
                "Test category", "collection", wallpapers, 0 /* priority */);
        mTestCategory.setIsRotationEnabled(isRotationEnabled);
        mTestCategory.setRotationInitializationState(rotationState);

        List<Category> testCategories = mTestCategoryProvider.getTestCategories();
        testCategories.set(0, mTestCategory);

        PickerIntentFactory intentFactory =
                new IndividualPickerActivity.IndividualPickerActivityIntentFactory();
        Intent intent = intentFactory.newIntent(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                mTestCategory.getCollectionId());
        mActivityRule.launchActivity(intent);
    }

    @Test
    public void testDrawsTilesForProvidedWallpapers() {
        setActivityWithMockWallpapers(false /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        IndividualPickerActivity activity = getActivity();

        RecyclerView recyclerView = activity.findViewById(R.id.wallpaper_grid);

        // There are only three wallpapers in the category, so the grid should only have three
        // items.
        assertNotNull(recyclerView.findViewHolderForAdapterPosition(0));
        assertNotNull(recyclerView.findViewHolderForAdapterPosition(1));
        assertNotNull(recyclerView.findViewHolderForAdapterPosition(2));
        assertNull(recyclerView.findViewHolderForAdapterPosition(3));
    }

    @Test
    public void testClickTile_Mobile_showsBottomActions() {
        mTestFormFactorChecker.setFormFactor(FormFactorChecker.FORM_FACTOR_MOBILE);

        setActivityWithMockWallpapers(false /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        onView(withId(R.id.wallpaper_grid)).perform(
                RecyclerViewActions.actionOnItemAtPosition(0, click()));

        onView(withId(R.id.action_back)).check(matches(isDisplayed()));
        onView(withId(R.id.action_information)).check(matches(isDisplayed()));
        onView(withId(R.id.action_apply)).check(matches(isDisplayed()));
    }

    @Test
    public void testClickDailyRefreshAction_ShowsStartRotationDialog() {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        onView(withId(R.id.action_rotation)).perform(click());

        onView(withId(R.id.start_rotation_wifi_only_checkbox))
                .check(matches(isDisplayed()));
        // WiFi-only option should be checked by default.
        onView(withId(R.id.start_rotation_wifi_only_checkbox))
                .check(matches(isChecked()));
    }

    @Test
    public void testStartRotationDialogUiComponents() {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        onView(withId(R.id.action_rotation)).perform(click());

        onView(withText(R.string.start_rotation_dialog_body)).check(matches(isDisplayed()));
        onView(withText(android.R.string.cancel)).check(matches(isDisplayed()));
        onView(withText(android.R.string.cancel)).check(matches(isClickable()));
        onView(withText(android.R.string.ok)).check(matches(isDisplayed()));
        onView(withText(android.R.string.ok)).check(matches(isClickable()));
        onView(withId(R.id.start_rotation_wifi_only_checkbox))
                .check(matches(isDisplayed()));
        onView(withId(R.id.start_rotation_wifi_only_checkbox))
                .check(matches(isClickable()));
        onView(withText(R.string.start_rotation_dialog_wifi_only_option_message))
                .check(matches(isDisplayed()));
    }

    @Test
    public void testShowStartRotationDialog_WifiOnly_ClickOK_StartsRotation() throws Throwable {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        setUpFragmentForTesting();
        TestWallpaperRotationInitializer
                testWPRotationInitializer = (TestWallpaperRotationInitializer)
                mTestCategory.getWallpaperRotationInitializer();
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        // Mock out the intent and response for the live wallpaper preview.
        Matcher<Intent> expectedIntent = hasAction(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER);
        intending(expectedIntent).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        onView(withId(R.id.action_rotation)).perform(click());

        onView(withText(android.R.string.ok)).perform(click());
        mActivityRule.runOnUiThread(() -> {
            testWPRotationInitializer.finishDownloadingFirstWallpaper(true /* isSuccessful */);
            assertTrue(testWPRotationInitializer.isRotationInitialized());
            assertTrue(testWPRotationInitializer.isWifiOnly());

            // The activity should finish if starting a rotation was successful.
            assertTrue(getActivity().isFinishing());
        });
    }

    @Test
    public void testShowStartRotationDialog_WifiOnly_ClickOK_Fails_ShowsErrorDialog() {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        setUpFragmentForTesting();
        TestWallpaperRotationInitializer
                testWPRotationInitializer = (TestWallpaperRotationInitializer)
                mTestCategory.getWallpaperRotationInitializer();
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        onView(withId(R.id.action_rotation)).perform(click());
        onView(withText(android.R.string.ok)).perform(click());

        testWPRotationInitializer.finishDownloadingFirstWallpaper(false /* isSuccessful */);
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        // Error dialog should be shown with retry option.
        onView(withText(R.string.start_rotation_error_message))
                .inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.try_again))
                .inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    public void testStartRotation_WifiOnly_FailOnce_Retry() throws Throwable {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        setUpFragmentForTesting();
        TestWallpaperRotationInitializer
                testWPRotationInitializer = (TestWallpaperRotationInitializer)
                mTestCategory.getWallpaperRotationInitializer();
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        onView(withId(R.id.action_rotation)).perform(click());
        onView(withText(android.R.string.ok)).perform(click());

        testWPRotationInitializer.finishDownloadingFirstWallpaper(false /* isSuccessful */);
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        // Click try again to retry.
        onView(withText(R.string.try_again)).inRoot(isDialog()).perform(click());

        mActivityRule.runOnUiThread(() -> {
            testWPRotationInitializer.finishDownloadingFirstWallpaper(true /* isSuccessful */);
            assertTrue(testWPRotationInitializer.isRotationInitialized());
            assertTrue(testWPRotationInitializer.isWifiOnly());
        });
    }

    @Test
    public void testShowStartRotationDialog_TurnOffWifiOnly_ClickOK_StartsRotation()
            throws Throwable {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        setUpFragmentForTesting();
        TestWallpaperRotationInitializer
                testWPRotationInitializer = (TestWallpaperRotationInitializer)
                mTestCategory.getWallpaperRotationInitializer();
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        onView(withId(R.id.action_rotation)).perform(click());
        // Click on WiFi-only option to toggle it off.
        onView(withId(R.id.start_rotation_wifi_only_checkbox)).perform(click());
        onView(withText(android.R.string.ok)).perform(click());

        mActivityRule.runOnUiThread(() -> {
            testWPRotationInitializer.finishDownloadingFirstWallpaper(true /* isSuccessful */);
            assertTrue(testWPRotationInitializer.isRotationInitialized());
            assertFalse(testWPRotationInitializer.isWifiOnly());
        });
    }

    @Test
    public void testStartRotation_WifiOnly_FailOnce_Retry_ShouldStillHaveWifiTurnedOff()
            throws Throwable {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        setUpFragmentForTesting();
        TestWallpaperRotationInitializer
                testWPRotationInitializer = (TestWallpaperRotationInitializer)
                mTestCategory.getWallpaperRotationInitializer();
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        onView(withId(R.id.action_rotation)).perform(click());
        // Click on WiFi-only option to toggle it off.
        onView(withId(R.id.start_rotation_wifi_only_checkbox)).perform(click());
        onView(withText(android.R.string.ok)).perform(click());

        testWPRotationInitializer.finishDownloadingFirstWallpaper(false /* isSuccessful */);
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        // Click try again to retry.
        onView(withText(R.string.try_again)).inRoot(isDialog()).perform(click());

        mActivityRule.runOnUiThread(() -> {
            testWPRotationInitializer.finishDownloadingFirstWallpaper(true /* isSuccessful */);
            assertTrue(testWPRotationInitializer.isRotationInitialized());
            assertFalse(testWPRotationInitializer.isWifiOnly());
        });
    }

    @Test
    public void testShowStartRotationDialog_ClickCancel_DismissesDialog() {
        setActivityWithMockWallpapers(true /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        getActivity();

        setUpFragmentForTesting();
        TestWallpaperRotationInitializer
                testWPRotationInitializer = (TestWallpaperRotationInitializer)
                mTestCategory.getWallpaperRotationInitializer();
        assertFalse(testWPRotationInitializer.isRotationInitialized());

        onView(withId(R.id.action_rotation)).perform(click());
        onView(withId(R.id.start_rotation_wifi_only_checkbox)).check(matches(isDisplayed()));
        // WiFi-only option should be checked by default.
        onView(withId(R.id.start_rotation_wifi_only_checkbox)).check(matches(isChecked()));

        // Click "Cancel" to dismiss the dialog.
        onView(withText(android.R.string.cancel)).perform(click());

        // Rotation was not initialized and dialog is no longer visible.
        assertFalse(testWPRotationInitializer.isRotationInitialized());
        onView(withId(R.id.start_rotation_wifi_only_checkbox)).check(doesNotExist());
    }

    /**
     * Tests that when no title is present, a wallpaper tile's content description attribute is
     * set to the first attribution.
     */
    @Test
    public void testWallpaperHasContentDescriptionFromAttribution() {
        setActivityWithMockWallpapers(false /* isRotationEnabled */,
                WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED);
        IndividualPickerActivity activity = getActivity();

        RecyclerView recyclerView = activity.findViewById(R.id.wallpaper_grid);

        RecyclerView.ViewHolder holder = recyclerView.findViewHolderForAdapterPosition(0);
        assertEquals("Attribution 0", holder.itemView.findViewById(R.id.tile)
                .getContentDescription());
    }
}
