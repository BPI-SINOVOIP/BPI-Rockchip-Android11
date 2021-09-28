/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.graphics.cts;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import android.app.UiAutomation;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import android.view.SurfaceControl;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class SetFrameRateTest {
    private static String TAG = "SetFrameRateTest";

    @Rule
    public ActivityTestRule<FrameRateCtsActivity> mActivityRule =
            new ActivityTestRule<>(FrameRateCtsActivity.class);
    private long mFrameRateFlexibilityToken;

    @Before
    public void setUp() throws Exception {
        long frameRateFlexibilityToken = 0;
        final UiDevice uiDevice =
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        try {
            uiDevice.wakeUp();
            uiDevice.executeShellCommand("wm dismiss-keyguard");
            // Surface flinger requires the ACCESS_SURFACE_FLINGER permission to acquire a frame
            // rate flexibility token. Switch to shell permission identity so we'll have the
            // necessary permission when surface flinger checks.
            UiAutomation uiAutomation = getInstrumentation().getUiAutomation();
            uiAutomation.adoptShellPermissionIdentity();
            try {
                frameRateFlexibilityToken = SurfaceControl.acquireFrameRateFlexibilityToken();
            } finally {
                uiAutomation.dropShellPermissionIdentity();
            }

            // Setup succeeded. Take ownership of the frame rate flexibility token, if we were able
            // to get one - we'll release it in tearDown().
            mFrameRateFlexibilityToken = frameRateFlexibilityToken;
            frameRateFlexibilityToken = 0;
            if (mFrameRateFlexibilityToken == 0) {
                Log.e(TAG,
                        "Failed to acquire frame rate flexibility token."
                                + " Frame rate tests may fail.");
            }
        } finally {
            if (frameRateFlexibilityToken != 0) {
                SurfaceControl.releaseFrameRateFlexibilityToken(frameRateFlexibilityToken);
                frameRateFlexibilityToken = 0;
            }
        }
    }

    @After
    public void tearDown() {
        final UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        if (mFrameRateFlexibilityToken != 0) {
            SurfaceControl.releaseFrameRateFlexibilityToken(mFrameRateFlexibilityToken);
            mFrameRateFlexibilityToken = 0;
        }
    }

    @Test
    public void testExactFrameRateMatch() throws InterruptedException {
        FrameRateCtsActivity activity = mActivityRule.getActivity();
        activity.testExactFrameRateMatch();
    }

    @Test
    public void testFixedSource() throws InterruptedException {
        FrameRateCtsActivity activity = mActivityRule.getActivity();
        activity.testFixedSource();
    }

    @Test
    public void testInvalidParams() throws InterruptedException {
        FrameRateCtsActivity activity = mActivityRule.getActivity();
        activity.testInvalidParams();
    }
}
