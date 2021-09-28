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

package android.systemui.cts;

import static android.server.wm.BarTestUtils.assumeHasColoredNavigationBar;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.support.test.uiautomator.UiDevice;
import android.view.View;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

/**
 * Tests for light system bars that set the flag via theme.
 *
 * atest CtsSystemUiTestCases:LightBarThemeTest
 */
@RunWith(AndroidJUnit4.class)
public class LightBarThemeTest extends LightBarTestBase {

    private UiDevice mDevice;

    @Rule
    public ActivityTestRule<LightBarThemeActivity> mActivityRule = new ActivityTestRule<>(
            LightBarThemeActivity.class);

    @Rule
    public TestName mTestName = new TestName();

    @Before
    public void setUp() {
        mDevice = UiDevice.getInstance(getInstrumentation());
    }

    @Test
    public void testThemeSetsFlags() throws Exception {
        final int visibility = mActivityRule.getActivity().getSystemUiVisibility();
        assertTrue((visibility & View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR) != 0);
        assertTrue((visibility & View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR) != 0);
    }

    @Test
    public void testGetNavigationBarDividerColor() throws Throwable {
        assumeHasColoredNavigationBar(mActivityRule);

        assertEquals(getInstrumentation().getContext().getColor(R.color.navigationBarDividerColor),
                mActivityRule.getActivity().getWindow().getNavigationBarDividerColor());
    }

    @Test
    public void testNavigationBarDividerColor() throws Throwable {
        assumeHasColoredNavigationBar(mActivityRule);

        // Wait until the activity is fully visible
        mDevice.waitForIdle();

        // Wait until window animation is finished
        Thread.sleep(WAIT_TIME);

        final Context instrumentationContext = getInstrumentation().getContext();
        checkNavigationBarDivider(mActivityRule.getActivity(),
                instrumentationContext.getColor(R.color.navigationBarDividerColor),
                instrumentationContext.getColor(R.color.navigationBarColor),
                mTestName.getMethodName());
    }
}
