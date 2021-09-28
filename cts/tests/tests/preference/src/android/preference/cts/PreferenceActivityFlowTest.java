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

package android.preference.cts;

import static android.preference.PreferenceActivity.EXTRA_NO_HEADERS;
import static android.preference.PreferenceActivity.EXTRA_SHOW_FRAGMENT;
import static android.preference.PreferenceActivity.EXTRA_SHOW_FRAGMENT_TITLE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Environment;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.BitmapUtils;

import org.junit.Rule;
import org.junit.rules.TestName;

import java.io.File;
import java.io.IOException;

/**
 * This test suite covers {@link android.preference.PreferenceActivity} to ensure its correct
 * behavior in orientation and multi-window changes together with navigation between different
 * screens and going back in the history. It should also cover any possible transition between
 * single and multi pane modes. Some tests are designed to run only on large or small screen devices
 * and some can run both. These tests are run from {@link PreferenceActivityFlowLandscapeTest} and
 * {@link PreferenceActivityFlowPortraitTest} to ensure that both display configurations are
 * checked.
 */
public abstract class PreferenceActivityFlowTest {

    private static final String TAG = "PreferenceFlowTest";

    // Helper strings to ensure that some parts of preferences are visible or not.
    private static final String PREFS1_HEADER_TITLE = "Prefs 1";
    private static final String PREFS2_HEADER_TITLE = "Prefs 2";
    private static final String PREFS1_PANEL_TITLE = "Preferences panel 1";
    private static final String PREFS2_PANEL_TITLE = "Preferences panel 2";
    private static final String INNER_FRAGMENT_PREF_BUTTON = "Fragment preference";
    private static final String INNER_FRAGMENT_PREF_TITLE = "Inner fragment";
    private static final String LIST_PREF_TITLE = "List preference";
    private static final String LIST_PREF_OPTION = "Alpha Option 01";

    private static final int INITIAL_TITLE_RES_ID = R.string.test_title;
    private static final int EXPECTED_HEADERS_COUNT = 3;

    private static final String LOCAL_DIRECTORY = Environment.getExternalStorageDirectory()
            + "/CtsPreferenceTestCases";

    @Rule
    public final TestName mTestName = new TestName();

    TestUtils mTestUtils;
    protected PreferenceWithHeaders mActivity;
    protected final Context mContext = InstrumentationRegistry.getInstrumentation()
            .getTargetContext();
    private boolean mIsMultiPane;

    void switchHeadersInner() {
        launchActivity();
        if (shouldRunLargeDeviceTest()) {
            largeScreenSwitchHeadersInner();
        } else {
            smallScreenSwitchHeadersInner();
        }
    }

    /**
     * For: Large screen (multi-pane).
     * Scenario: Tests that tapping on header changes to its proper preference panel and that the
     * headers are still visible.
     */
    private void largeScreenSwitchHeadersInner() {
        assertTrue(shouldRunLargeDeviceTest());
        assertInitialState();

        tapOnPrefs2Header();

        // Headers and panel Prefs2 must be shown.
        assertHeadersShown();
        assertPanelPrefs1Hidden();
        assertPanelPrefs2Shown();

        tapOnPrefs1Header();

        // Headers and panel Prefs1 must be shown.
        assertHeadersShown();
        assertPanelPrefs1Shown();
        assertPanelPrefs2Hidden();
    }

    /**
     * For: Small screen (single-pane).
     * Scenario: Tests that tapping on header changes to its proper preference panel and that the
     * headers are hidden and pressing back button after that shows them again.
     */
    private void smallScreenSwitchHeadersInner() {
        assertTrue(shouldRunSmallDeviceTest());
        assertInitialState();

        tapOnPrefs2Header();

        // Only Prefs2 must be shown.
        assertHeadersHidden();
        assertPanelPrefs1Hidden();
        assertPanelPrefs2Shown();

        pressBack();

        tapOnPrefs1Header();

        // Only Prefs1 must be shown.
        assertHeadersHidden();
        assertPanelPrefs1Shown();
        assertPanelPrefs2Hidden();
    }

    /**
     * For: Small screen (single-pane).
     * Scenario: Tests that after navigating back to the headers list there will be no header
     * highlighted and that the title was properly restored..
     */
    void smallScreenNoHighlightInHeadersListInner() {
        launchActivity();
        if (!shouldRunSmallDeviceTest()) {
            return;
        }

        assertInitialState();

        CharSequence title = mActivity.getTitle();

        tapOnPrefs2Header();
        assertHeadersHidden();

        pressBack();
        assertHeadersShown();

        // Verify that no headers are focused.
        assertHeadersNotFocused();

        // Verify that the title was properly restored.
        assertEquals(title, mActivity.getTitle());

        // Verify that everything restores back to initial state again.
        assertInitialState();
    }

    void backPressToExitInner() {
        launchActivity();
        if (shouldRunLargeDeviceTest()) {
            largeScreenBackPressToExitInner();
        } else {
            smallScreenBackPressToExitInner();
        }
    }

    /**
     * For: Small screen (single-pane).
     * Scenario: Tests that pressing back button twice after having preference panel opened will
     * exit the app when running single-pane.
     */
    private void smallScreenBackPressToExitInner() {
        assertTrue(shouldRunSmallDeviceTest());
        assertInitialState();

        tapOnPrefs2Header();

        // Only Prefs2 must be shown - covered by smallScreenSwitchHeadersTest

        pressBack();
        pressBack();

        // Now we should be out of the activity
        assertHeadersHidden();
        assertPanelPrefs1Hidden();
        assertPanelPrefs2Hidden();
    }

    /**
     * For: Large screen (multi-pane).
     * Scenario: Selects a header and then leaves the activity by pressing back button. Tests that
     * we don't transition to the previous header or list of header like in single-pane.
     */
    private void largeScreenBackPressToExitInner() {
        assertTrue(shouldRunLargeDeviceTest());
        assertInitialState();

        tapOnPrefs2Header();

        // Headers and panel Prefs2 must be shown - covered by largeScreenSwitchHeadersInner.

        pressBack();

        assertHeadersHidden();
    }

    void goToFragmentInner() {
        launchActivity();
        if (shouldRunLargeDeviceTest()) {
            largeScreenGoToFragmentInner();
        } else {
            smallScreenGoToFragmentInner();
        }
    }

    /**
     * For: Large screen (multi-pane).
     * Scenario: Navigates to inner fragment. Test that the fragment was opened correctly and
     * headers are still visible. Also tests that back press doesn't close the app but navigates
     * back from the fragment.
     */
    private void largeScreenGoToFragmentInner() {
        assertTrue(shouldRunLargeDeviceTest());
        assertInitialState();

        tapOnPrefs1Header();

        // Go to preferences inner fragment.
        mTestUtils.tapOnViewWithText(INNER_FRAGMENT_PREF_BUTTON);

        // Headers and inner fragment must be shown.
        assertHeadersShown();
        assertPanelPrefs1Hidden();
        assertInnerFragmentShown();

        pressBack();

        // Headers and panel Prefs1 must be shown.
        assertHeadersShown();
        assertPanelPrefs1Shown();
        assertPanelPrefs2Hidden();
        assertInnerFragmentHidden();
    }

    /**
     * For: Small screen (single-pane).
     * Scenario: Navigates to inner fragment. Tests that the fragment was opened correctly and
     * headers are hidden. Also tests that back press doesn't close the app but navigates back from
     * the fragment.
     */
    private void smallScreenGoToFragmentInner() {
        assertTrue(shouldRunSmallDeviceTest());
        assertInitialState();

        tapOnPrefs1Header();

        // Go to preferences inner fragment.
        mTestUtils.tapOnViewWithText(INNER_FRAGMENT_PREF_BUTTON);

        // Only inner fragment must be shown.
        assertHeadersHidden();
        assertPanelPrefs1Hidden();
        assertInnerFragmentShown();

        pressBack();

        // Prefs1 must be shown.
        assertHeadersHidden();
        assertPanelPrefs1Shown();
        assertPanelPrefs2Hidden();
        assertInnerFragmentHidden();
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that opening specific preference fragment directly via intent works properly.
     */
    void startWithFragmentInner() {
        launchActivityWithExtras(PreferenceWithHeaders.PrefsTwoFragment.class,
                false /* noHeaders */, -1 /* initialTitle */);

        assertInitialStateForFragment();
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that preference fragment opened directly survives recreation (via screenshot
     * tests).
     */
    void startWithFragmentAndRecreateInner() {
        launchActivityWithExtras(PreferenceWithHeaders.PrefsTwoFragment.class,
                false /* noHeaders */, -1 /* initialTitle */);

        assertInitialStateForFragment();

        // Take screenshot
        Bitmap before = mTestUtils.takeScreenshot();

        // Force recreate
        recreate();

        assertInitialStateForFragment();

        // Compare screenshots
        Bitmap after = mTestUtils.takeScreenshot();
        assertScreenshotsAreEqual(before, after);
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Starts preference fragment directly with the given initial title and tests that
     * multi-pane does not show it and single-pane does.
     */
    void startWithFragmentAndInitTitleInner() {
        launchActivityWithExtras(PreferenceWithHeaders.PrefsTwoFragment.class,
                false /* noHeaders */, INITIAL_TITLE_RES_ID);

        assertInitialStateForFragment();

        if (mIsMultiPane) {
            String testTitle = mActivity.getResources().getString(INITIAL_TITLE_RES_ID);
            // Title should not be shown.
            assertTextHidden(testTitle);
        } else {
            // Title should be shown.
            assertTitleShown();
        }
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that EXTRA_NO_HEADERS intent arg that prevents showing headers in multi-pane
     * is applied correctly.
     */
    void startWithFragmentNoHeadersInner() {
        launchActivityWithExtras(PreferenceWithHeaders.PrefsTwoFragment.class,
                true /* noHeaders */, -1 /* initialTitle */);

        assertInitialStateForFragment();
        // Only Prefs2 should be shown.
        assertHeadersHidden();
        assertPanelPrefs1Hidden();
        assertPanelPrefs2Shown();
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that EXTRA_NO_HEADERS intent arg that prevents showing headers in multi-pane
     * is applied correctly plus initial title is displayed.
     */
    void startWithFragmentNoHeadersButInitTitleInner() {
        launchActivityWithExtras(PreferenceWithHeaders.PrefsTwoFragment.class,
                true /* noHeaders */, INITIAL_TITLE_RES_ID);

        assertInitialStateForFragment();
        // Only Prefs2 should be shown.
        assertHeadersHidden();
        assertPanelPrefs1Hidden();
        assertPanelPrefs2Shown();

        assertTitleShown();
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that list preference opens correctly and that back press correctly closes it.
     */
    void listDialogTest() {
        launchActivity();

        assertInitialState();
        if (!mIsMultiPane) {
            tapOnPrefs1Header();
        }

        mTestUtils.tapOnViewWithText(LIST_PREF_TITLE);

        assertTextShown(LIST_PREF_OPTION);

        pressBack();

        if (mIsMultiPane) {
            // Headers and Prefs1 should be shown.
            assertHeadersShown();
            assertPanelPrefs1Shown();
        } else {
            // Only Prefs1 should be shown.
            assertHeadersHidden();
            assertPanelPrefs1Shown();
        }
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that the PreferenceActivity properly restores its state after recreation.
     * Test done via screenshots.
     */
    void recreateTest() {
        launchActivity();

        assertInitialState();
        tapOnPrefs2Header();

        assertPanelPrefs2Shown();

        // Take screenshot
        Bitmap before = mTestUtils.takeScreenshot();

        recreate();

        assertPanelPrefs2Shown();

        // Compare screenshots
        Bitmap after = mTestUtils.takeScreenshot();
        assertScreenshotsAreEqual(before, after);
    }

    /**
     * For: Any screen (single or multi-pane).
     * Scenario: Tests that the PreferenceActivity properly restores its state after recreation
     * while an inner fragment is shown. Test done via screenshots.
     */
    void recreateInnerFragmentTest() {
        launchActivity();

        assertInitialState();

        if (!mIsMultiPane) {
            tapOnPrefs1Header();
        }

        // Go to preferences inner fragment.
        mTestUtils.tapOnViewWithText(INNER_FRAGMENT_PREF_BUTTON);

        // Only inner fragment must be shown.
        if (shouldRunLargeDeviceTest()) {
            assertHeadersShown();
        } else {
            assertHeadersHidden();
        }
        assertPanelPrefs1Hidden();
        assertInnerFragmentShown();

        // Take screenshot
        Log.v(TAG, "taking screenshot before");
        Bitmap before = mTestUtils.takeScreenshot();
        Log.v(TAG, "screenshot taken");

        recreate();

        // Only inner fragment must be shown.
        if (shouldRunLargeDeviceTest()) {
            assertHeadersShown();
        } else {
            assertHeadersHidden();
        }
        assertPanelPrefs1Hidden();
        assertInnerFragmentShown();

        // Compare screenshots
        Log.v(TAG, "taking screenshot after");
        Bitmap after = mTestUtils.takeScreenshot();
        Log.v(TAG, "screenshot taken");
        assertScreenshotsAreEqual(before, after);
    }

    private void assertScreenshotsAreEqual(Bitmap before, Bitmap after) {
        // TODO(b/134080964): remove the precision=0.99 arg so it does a pixel-by-pixel check
        if (!BitmapUtils.compareBitmaps(before, after, 0.99)) {
            String testName = getClass().getSimpleName() + "." + mTestName.getMethodName();
            File beforeFile = null;
            File afterFile = null;
            try {
                beforeFile = dumpBitmap(before, testName + "-before.png");
                afterFile = dumpBitmap(after, testName + "-after.png");
            } catch (IOException e) {
                Log.e(TAG,  "Error dumping bitmap", e);
            }
            fail("Screenshots do not match (check " + beforeFile + " and " + afterFile + ")");
        }
    }

    // TODO: copied from Autofill; move to common CTS code
    private File dumpBitmap(Bitmap bitmap, String filename) throws IOException {
        File file = createFile(filename);
        if (file == null) return null;
        Log.i(TAG, "Dumping bitmap at " + file);
        BitmapUtils.saveBitmap(bitmap, file.getParent(), file.getName());
        return file;

    }

    private static File createFile(String filename) throws IOException {
        File dir = getLocalDirectory();
        File file = new File(dir, filename);
        if (file.exists()) {
            Log.v(TAG, "Deleting file " + file);
            file.delete();
        }
        if (!file.createNewFile()) {
            Log.e(TAG, "Could not create file " + file);
            return null;
        }
        return file;
    }

    private static File getLocalDirectory() {
        File dir = new File(LOCAL_DIRECTORY);
        dir.mkdirs();
        if (!dir.exists()) {
            Log.e(TAG, "Could not create directory " + dir);
            return null;
        }
        return dir;
    }

    // TODO: move to common code
    void requirePortraitModeSupport() {
        String testName = mTestName.getMethodName();
        PackageManager pm = mContext.getPackageManager();

        boolean hasPortrait = pm.hasSystemFeature(PackageManager.FEATURE_SCREEN_PORTRAIT);
        boolean hasLandscape = pm.hasSystemFeature(PackageManager.FEATURE_SCREEN_LANDSCAPE);

        // From the javadoc: For backwards compatibility, you can assume that if neither
        // FEATURE_SCREEN_PORTRAIT nor FEATURE_SCREEN_LANDSCAPE is set then the device
        // supports both portrait and landscape.
        boolean supportsPortrait = hasPortrait || !hasLandscape;

        Log.v(TAG, "requirePortraitModeSupport(): FEATURE_SCREEN_PORTRAIT=" + hasPortrait
                + ", FEATURE_SCREEN_LANDSCAPE=" + hasLandscape
                + ", supportsPortrait=" + supportsPortrait);

        assumeTrue(testName + ": device does not support portrait mode", supportsPortrait);
    }

    // TODO: move to common code
    void requireLandscapeModeSupport() {
        String testName = mTestName.getMethodName();
        PackageManager pm = mContext.getPackageManager();

        boolean hasPortrait = pm.hasSystemFeature(PackageManager.FEATURE_SCREEN_PORTRAIT);
        boolean hasLandscape = pm.hasSystemFeature(PackageManager.FEATURE_SCREEN_LANDSCAPE);

        // From the javadoc: For backwards compatibility, you can assume that if neither
        // FEATURE_SCREEN_PORTRAIT nor FEATURE_SCREEN_LANDSCAPE is set then the device
        // supports both portrait and landscape.
        boolean supportsLandscape = hasLandscape || !hasPortrait;

        Log.v(TAG, "requireLandscapeModeSupport(): FEATURE_SCREEN_PORTRAIT=" + hasPortrait
                + ", FEATURE_SCREEN_LANDSCAPE=" + hasLandscape
                + ", supportsLandscape=" + supportsLandscape);

        assumeTrue(testName + ": device does not support portrait mode", supportsLandscape);
    }

    private void assertInitialState() {
        if (mIsMultiPane) {
            // Headers and panel Prefs1 must be shown.
            assertHeadersShown();
            runOnUiThread(() -> assertTrue(mActivity.hasHeaders()));
            assertPanelPrefs1Shown();
            assertPanelPrefs2Hidden();
        } else {
            // Headers must be shown and nothing else.
            assertHeadersShown();
            runOnUiThread(() -> assertTrue(mActivity.hasHeaders()));
            assertPanelPrefs1Hidden();
            assertPanelPrefs2Hidden();
        }
        assertHeadersAreLoaded();
    }

    private void assertInitialStateForFragment() {
        if (mIsMultiPane) {
            // Headers and Prefs2 should be shown.
            assertHeadersShown();
            runOnUiThread(() -> assertTrue(mActivity.hasHeaders()));
            assertPanelPrefs1Hidden();
            assertPanelPrefs2Shown();
        } else {
            // Only Prefs2 should be shown.
            assertHeadersHidden();
            runOnUiThread(() -> assertFalse(mActivity.hasHeaders()));
            assertPanelPrefs1Hidden();
            assertPanelPrefs2Shown();
        }
    }

    private boolean shouldRunLargeDeviceTest() {
        if (mActivity.onIsMultiPane()) {
            return true;
        }

        Log.d(TAG, "Skipping a large device test.");
        return false;
    }

    private boolean shouldRunSmallDeviceTest() {
        if (!mActivity.onIsMultiPane()) {
            return true;
        }

        Log.d(TAG, "Skipping a small device test.");
        return false;
    }

    private void tapOnPrefs1Header() {
        mTestUtils.tapOnViewWithText(PREFS1_HEADER_TITLE);
    }

    private void tapOnPrefs2Header() {
        mTestUtils.tapOnViewWithText(PREFS2_HEADER_TITLE);
    }

    private void assertHeadersAreLoaded() {
        runOnUiThread(() -> assertEquals(EXPECTED_HEADERS_COUNT,
                mActivity.loadedHeaders == null
                        ? 0
                        : mActivity.loadedHeaders.size()));
    }

    private void assertHeadersShown() {
        assertTextShown(PREFS1_HEADER_TITLE);
        assertTextShown(PREFS2_HEADER_TITLE);
    }

    private void assertHeadersNotFocused() {
        assertFalse(mTestUtils.isTextFocused(PREFS1_HEADER_TITLE));
        assertFalse(mTestUtils.isTextFocused(PREFS2_HEADER_TITLE));
    }

    private void assertHeadersHidden() {
        // We check that at least one is hidden instead of each individual one separately because
        // these headers are also part of individual preference panels breadcrumbs so it would fail
        // if we only checked for one.
        assertTrue(mTestUtils.isTextHidden(PREFS1_HEADER_TITLE)
                || mTestUtils.isTextHidden(PREFS2_HEADER_TITLE));
    }

    private void assertPanelPrefs1Shown() {
        assertTextShown(PREFS1_PANEL_TITLE);
    }

    private void assertPanelPrefs1Hidden() {
        assertTextHidden(PREFS1_PANEL_TITLE);
    }

    private void assertPanelPrefs2Shown() {
        assertTextShown(PREFS2_PANEL_TITLE);
    }

    private void assertPanelPrefs2Hidden() {
        assertTextHidden(PREFS2_PANEL_TITLE);
    }

    private void assertInnerFragmentShown() {
        assertTextShown(INNER_FRAGMENT_PREF_TITLE);
    }

    private void assertInnerFragmentHidden() {
        assertTextHidden(INNER_FRAGMENT_PREF_TITLE);
    }

    private void assertTextShown(String text) {
        assertTrue(mTestUtils.isTextShown(text));
    }

    private void assertTextHidden(String text) {
        assertTrue(mTestUtils.isTextHidden(text));
    }

    private void assertTitleShown() {
        if (!mTestUtils.isOnWatchUiMode()) {
            // On watch, activity title is not shown by default.
            String testTitle = mActivity.getResources().getString(INITIAL_TITLE_RES_ID);
            assertTextShown(testTitle);
        }
    }

    private void recreate() {
        runOnUiThread(() -> mActivity.recreate());
        mTestUtils.waitForIdle();
    }

    private void pressBack() {
        mTestUtils.mDevice.pressBack();
        mTestUtils.waitForIdle();
    }

    private void launchActivity() {
        mActivity = launchActivity(null);
        mTestUtils.waitForIdle();
        runOnUiThread(() -> mIsMultiPane = mActivity.isMultiPane());
    }

    private void launchActivityWithExtras(Class extraFragment, boolean noHeaders,
            int initialTitle) {
        Intent intent = new Intent(Intent.ACTION_MAIN);

        if (extraFragment != null) {
            intent.putExtra(EXTRA_SHOW_FRAGMENT, extraFragment.getName());
        }
        if (noHeaders) {
            intent.putExtra(EXTRA_NO_HEADERS, true);
        }
        if (initialTitle != -1) {
            intent.putExtra(EXTRA_SHOW_FRAGMENT_TITLE, initialTitle);
        }

        mActivity = launchActivity(intent);
        mTestUtils.waitForIdle();
        runOnUiThread(() -> mIsMultiPane = mActivity.isMultiPane());
    }

    protected abstract PreferenceWithHeaders launchActivity(Intent intent);

    protected abstract void runOnUiThread(Runnable runnable);
}
