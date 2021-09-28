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

package android.inputmethodservice.cts.devicetest;

import static org.junit.Assert.assertTrue;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.inputmethodservice.cts.ime.Watermark;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Provides utility methods to test whether test IMEs are visible to the user or not.
 */
final class InputMethodVisibilityVerifier {

    private static final long SCREENSHOT_DELAY = 100;  // msec
    private static final long SCREENSHOT_TIME_SLICE = 500;  // msec

    @NonNull
    private static boolean containsWatermark(@NonNull UiAutomation uiAutomation,
            @NonNull Watermark watermark) {
        return watermark.isContainedIn(uiAutomation.takeScreenshot());
    }

    private static boolean waitUntilWatermarkBecomesVisible(long timeout,
            @NonNull Watermark watermark) {
        final long startTime = SystemClock.elapsedRealtime();
        SystemClock.sleep(SCREENSHOT_DELAY);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();

        // Wait until the main thread becomes idle.
        final CountDownLatch latch = new CountDownLatch(1);
        instrumentation.waitForIdle(latch::countDown);
        try {
            if (!latch.await(timeout, TimeUnit.MILLISECONDS)) {
                return false;
            }
        } catch (InterruptedException e) {
        }

        final UiAutomation uiAutomation = instrumentation.getUiAutomation();

        // Make sure the watermark is tested at least two times, regardless of the timeout.
        boolean firstCheck = true;
        while (true) {
            final boolean lastCheck;
            if (firstCheck) {
                firstCheck = false;
                lastCheck = false;
            } else {
                SystemClock.sleep(SCREENSHOT_TIME_SLICE);
                lastCheck = (SystemClock.elapsedRealtime() - startTime) >= timeout;
            }
            if (containsWatermark(uiAutomation, watermark)) {
                return true;
            }
            if (lastCheck) {
                return false;
            }
        }
    }

    /**
     * Asserts that IME1 is visible to the user.
     *
     * @param timeout timeout in milliseconds.
     */
    static void assertIme1Visible(long timeout) {
        assertTrue(waitUntilWatermarkBecomesVisible(timeout, Watermark.IME1));
    }

    /**
     * Asserts that IME2 is visible to the user.
     *
     * @param timeout timeout in milliseconds.
     */
    static void assertIme2Visible(long timeout) {
        assertTrue(waitUntilWatermarkBecomesVisible(timeout, Watermark.IME2));
    }
}
