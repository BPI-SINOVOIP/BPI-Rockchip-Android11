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

package android.view.inputmethod.cts.util;

import static org.junit.Assert.assertTrue;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.cts.mockime.Watermark;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Predicate;

/**
 * Provides utility methods to test whether test IMEs are visible to the user or not.
 */
public final class InputMethodVisibilityVerifier {

    private static final long SCREENSHOT_DELAY = 100;  // msec
    private static final long SCREENSHOT_TIME_SLICE = 500;  // msec

    /**
     * Not intended to be instantiated.
     */
    private InputMethodVisibilityVerifier() {
    }

    @NonNull
    private static boolean containsWatermark(@NonNull UiAutomation uiAutomation) {
        return Watermark.detect(uiAutomation.takeScreenshot());
    }

    @NonNull
    private static boolean notContainsWatermark(@NonNull UiAutomation uiAutomation) {
        return !Watermark.detect(uiAutomation.takeScreenshot());
    }

    private static boolean waitUntil(long timeout, @NonNull Predicate<UiAutomation> condition) {
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
        if (condition.test(uiAutomation)) {
            return true;
        }
        while ((SystemClock.elapsedRealtime() - startTime) < timeout) {
            SystemClock.sleep(SCREENSHOT_TIME_SLICE);
            if (condition.test(uiAutomation)) {
                return true;
            }
        }
        return condition.test(uiAutomation);
    }

    /**
     * Asserts that {@link com.android.cts.mockime.MockIme} is visible to the user.
     *
     * <p>This never succeeds when
     * {@link com.android.cts.mockime.ImeSettings.Builder#setWatermarkEnabled(boolean)} is
     * explicitly called with {@code false}.</p>
     *
     * @param timeout timeout in milliseconds.
     */
    public static void expectImeVisible(long timeout) {
        assertTrue(waitUntil(timeout, InputMethodVisibilityVerifier::containsWatermark));
    }

    /**
     * Asserts that {@link com.android.cts.mockime.MockIme} is not visible to the user.
     *
     * <p>This always succeeds when
     * {@link com.android.cts.mockime.ImeSettings.Builder#setWatermarkEnabled(boolean)} is
     * explicitly called with {@code false}.</p>
     *
     * @param timeout timeout in milliseconds.
     */
    public static void expectImeInvisible(long timeout) {
        assertTrue(waitUntil(timeout, InputMethodVisibilityVerifier::notContainsWatermark));
    }
}
