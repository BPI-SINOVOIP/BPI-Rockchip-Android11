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
 * limitations under the License.
 */

package android.preference.cts;

import static android.server.wm.WindowManagerState.STATE_RESUMED;
import static org.junit.Assert.assertTrue;

import android.content.ComponentName;
import android.graphics.Bitmap;
import android.server.wm.WindowManagerStateHelper;

import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.BitmapUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * This test suite covers {@link android.preference.PreferenceActivity} and its legacy usage that
 * inflates only a preference screen without using any headers or fragments. This should verify
 * correct behavior in orientation and multi-window changes.
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
public class PreferenceActivityLegacyFlowTest {

    // Helper strings to ensure that some parts of preferences are visible or not.
    private static final String LEGACY_SCREEN_TEXT = "Preset Title";

    static final String TEST_APP_PACKAGE = "android.preference.cts";
    private static final String TEST_APP_ACTIVITY = TEST_APP_PACKAGE + ".PreferencesFromXml";

    private TestUtils mTestUtils;
    private PreferencesFromXml mActivity;

    @Rule
    public ActivityTestRule<PreferencesFromXml> mActivityRule =
            new ActivityTestRule<>(PreferencesFromXml.class, true);

    @Before
    public void setup() {
        mTestUtils = new TestUtils(mActivityRule);
        mActivity = mActivityRule.getActivity();
    }

    /**
     * Scenario: Tests that items from the given preference screen are shown.
     */
    @Test
    public void legacyActivityTest() {
        // Prefs list should be shown.
        assertTextShown(LEGACY_SCREEN_TEXT);
    }

    /**
     * Scenario: Tests that the activity correctly restores its state after recreation in legacy
     * mode.
     */
    @Test
    public void legacyActivityRecreateTest() {
        Bitmap before = mTestUtils.takeScreenshot();

        recreate();
        
        // Compare screenshots
        Bitmap after = mTestUtils.takeScreenshot();
        assertScreenshotsAreEqual(before, after);
    }

    private void recreate() {
        runOnUiThread(() -> mActivity.recreate());
        mTestUtils.waitForIdle();
        ComponentName testComponentName = new ComponentName(TEST_APP_PACKAGE, TEST_APP_ACTIVITY);
        waitForActivityState(testComponentName, STATE_RESUMED);

    }

    private void runOnUiThread(final Runnable runnable) {
        try {
            mActivityRule.runOnUiThread(runnable);
        } catch (Throwable ex) {
            throw new RuntimeException("Failure on the UI thread", ex);
        }
    }

    private void assertScreenshotsAreEqual(Bitmap before, Bitmap after) {
        assertTrue("Screenshots do not match!", BitmapUtils.compareBitmaps(before, after));
    }

    private void assertTextShown(String text) {
        assertTrue(mTestUtils.isTextShown(text));
    }

    public void waitForActivityState(ComponentName activityName, String activityState) {
        new WindowManagerStateHelper().waitForActivityState(activityName, activityState);
    }

}
