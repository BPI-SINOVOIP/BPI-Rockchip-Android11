/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.assist.cts;

import static com.google.common.truth.Truth.assertThat;

import android.assist.common.AutoResetLatch;
import android.assist.common.Utils;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;

import org.junit.Test;

public class ScreenshotTest extends AssistTestBase {
    static final String TAG = "ScreenshotTest";

    private static final String TEST_CASE_TYPE = Utils.SCREENSHOT;

    @Override
    protected void customSetup() throws Exception {
        // start test start activity
        startTestActivity(TEST_CASE_TYPE);
    }

    @Test
    public void testRedScreenshot() throws Throwable {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }

        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();

        Bundle bundle = new Bundle();
        bundle.putInt(Utils.SCREENSHOT_COLOR_KEY, Color.RED);
        start3pApp(TEST_CASE_TYPE, bundle);

        eventuallyWithSessionClose(() -> {
            delayAndStartSession(Color.RED);
            verifyAssistDataNullness(false, false, false, false);
            assertThat(mScreenshotMatches).isTrue();
        });
    }

    @Test
    public void testGreenScreenshot() throws Throwable {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }

        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();

        Bundle bundle = new Bundle();
        bundle.putInt(Utils.SCREENSHOT_COLOR_KEY, Color.GREEN);
        start3pApp(TEST_CASE_TYPE, bundle);

        eventuallyWithSessionClose(() -> {
            delayAndStartSession(Color.GREEN);
            verifyAssistDataNullness(false, false, false, false);
            assertThat(mScreenshotMatches).isTrue();
        });
    }

    @Test
    public void testBlueScreenshot() throws Throwable {
        if (mActivityManager.isLowRamDevice()) {
            Log.d(TAG, "Not running assist tests on low-RAM device.");
            return;
        }

        startTest(TEST_CASE_TYPE);
        waitForAssistantToBeReady();

        Bundle bundle = new Bundle();
        bundle.putInt(Utils.SCREENSHOT_COLOR_KEY, Color.BLUE);
        start3pApp(TEST_CASE_TYPE, bundle);

        eventuallyWithSessionClose(() -> {
            delayAndStartSession(Color.BLUE);
            verifyAssistDataNullness(false, false, false, false);
            assertThat(mScreenshotMatches).isTrue();
        });
    }

    private void delayAndStartSession(int color) throws Exception {
        Bundle extras = new Bundle();
        extras.putInt(Utils.SCREENSHOT_COLOR_KEY, color);
        final AutoResetLatch latch = startSession(TEST_CASE_TYPE, extras);
        waitForContext(latch);
    }
}
