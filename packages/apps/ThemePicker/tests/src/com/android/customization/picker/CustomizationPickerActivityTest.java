package com.android.customization.picker;

import static android.content.Intent.ACTION_SET_WALLPAPER;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.matcher.IntentMatchers.filterEquals;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isSelected;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.android.customization.picker.CustomizationPickerActivity.WALLPAPER_FLAVOR_EXTRA;
import static com.android.customization.picker.CustomizationPickerActivity.WALLPAPER_FOCUS;
import static com.android.customization.picker.CustomizationPickerActivity.WALLPAPER_ONLY;

import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.text.TextUtils;

import androidx.fragment.app.Fragment;
import androidx.test.espresso.intent.Intents;
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import androidx.test.rule.ActivityTestRule;

import com.android.customization.picker.theme.ThemeFragment;
import com.android.customization.testing.TestCustomizationInjector;
import com.android.customization.testing.TestThemeManager;
import com.android.wallpaper.R;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.picker.CategoryFragment;
import com.android.wallpaper.picker.TopLevelPickerActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for {@link CustomizationPickerActivity}.
 */
@RunWith(AndroidJUnit4ClassRunner.class)
public class CustomizationPickerActivityTest {

    @Rule
    public ActivityTestRule<CustomizationPickerActivity> mActivityRule =
            new ActivityTestRule<>(CustomizationPickerActivity.class, false, false);

    @Before
    public void setUp() {
        Intents.init();
    }

    @After
    public void tearDown() {
        mActivityRule.finishActivity();
        Intents.release();
    }

    @Test
    public void launchActivity_doesNotSupportCustomization_launchesTopLevelPickerActivity() {
        TestThemeManager.setAvailable(false);

        launchActivity();

        Intents.intended(filterEquals(new Intent(mActivityRule.getActivity(),
                TopLevelPickerActivity.class)));
    }

    // ----------------------------------------------------------------------
    // Tests for customization supports.
    // ----------------------------------------------------------------------

    @Test
    public void launchActivity_themeManagerIsAvailable_NavBarShowsStyleAndWallpaperItem() {
        TestThemeManager.setAvailable(true);

        launchActivity();

        onView(withId(R.id.nav_theme)).check(matches(isCompletelyDisplayed()));
        onView(withId(R.id.nav_wallpaper)).check(matches(isCompletelyDisplayed()));
        onView(withId(R.id.nav_clock)).check(doesNotExist());
        onView(withId(R.id.nav_grid)).check(doesNotExist());
    }

    @Test
    public void launchActivity_navigateToTheStyleTabByDefault() {
        TestThemeManager.setAvailable(true);

        launchActivity();

        onView(withId(R.id.nav_theme)).check(matches(isSelected()));
        assertTrue(getCurrentShowingFragment() instanceof ThemeFragment);
    }

    @Test
    public void launchActivity_withExtraWallpaperOnly_launchesTopLevelPickerActivity() {
        TestThemeManager.setAvailable(true);

        launchActivityWithFlavorExtra(WALLPAPER_ONLY);

        // While receiving WALLPAPER_ONLY extra, it should launch TopLevelPickerActivity whatever it
        // supports customization.
        Intents.intended(filterEquals(new Intent(mActivityRule.getActivity(),
                TopLevelPickerActivity.class)));
    }

    @Test
    public void clickStyleButton_showsThemeFragment() {
        TestThemeManager.setAvailable(true);
        launchActivity();

        onView(withId(R.id.nav_theme)).perform(click());

        onView(withId(R.id.nav_theme)).check(matches(isSelected()));
        assertTrue(getCurrentShowingFragment() instanceof ThemeFragment);
    }

    @Test
    public void clickWallpaperButton_showsCategoryFragment() {
        TestThemeManager.setAvailable(true);
        launchActivity();

        onView(withId(R.id.nav_wallpaper)).perform(click());

        onView(withId(R.id.nav_wallpaper)).check(matches(isSelected()));
        assertTrue(getCurrentShowingFragment() instanceof CategoryFragment);
    }

    @Test
    public void launchActivity_withExtraWallpaperFocus_navigateToTheWallpaperTab() {
        TestThemeManager.setAvailable(true);

        launchActivityWithFlavorExtra(WALLPAPER_FOCUS);

        onView(withId(R.id.nav_wallpaper)).check(matches(isSelected()));
        assertTrue(getCurrentShowingFragment() instanceof CategoryFragment);
    }

    private void launchActivity() {
        launchActivityWithFlavorExtra("");
    }

    private void launchActivityWithFlavorExtra(String extra) {
        InjectorProvider.setInjector(new TestCustomizationInjector());
        Intent intent = new Intent(ACTION_SET_WALLPAPER);
        if (!TextUtils.isEmpty(extra)) {
            intent.putExtra(WALLPAPER_FLAVOR_EXTRA, extra);
        }
        mActivityRule.launchActivity(intent);
    }

    private Fragment getCurrentShowingFragment() {
        return mActivityRule.getActivity().getSupportFragmentManager()
                .findFragmentById(R.id.fragment_container);
    }
}
