/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.view.cts;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.view.Display;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Duration;
import java.util.Arrays;
import java.util.Optional;

@FlakyTest
@RunWith(AndroidJUnit4.class)
public class ChoreographerNativeTest {
    private long mChoreographerPtr;

    private static native long nativeGetChoreographer();
    private static native boolean nativePrepareChoreographerTests(long ptr, long[] refreshPeriods);
    private static native void nativeTestPostCallbackWithoutDelayEventuallyRunsCallbacks(long ptr);
    private static native void nativeTestPostCallbackWithDelayEventuallyRunsCallbacks(long ptr);
    private static native void nativeTestPostCallback64WithoutDelayEventuallyRunsCallbacks(
            long ptr);
    private static native void nativeTestPostCallback64WithDelayEventuallyRunsCallbacks(long ptr);
    private static native void nativeTestPostCallbackMixedWithoutDelayEventuallyRunsCallbacks(
            long ptr);
    private static native void nativeTestPostCallbackMixedWithDelayEventuallyRunsCallbacks(
            long ptr);
    private static native void nativeTestRefreshRateCallback(
            long ptr);
    private static native void nativeTestUnregisteringRefreshRateCallback(long ptr);
    private static native void nativeTestMultipleRefreshRateCallbacks(long ptr);
    private static native void nativeTestAttemptToAddRefreshRateCallbackTwiceDoesNotAddTwice(
            long ptr);
    private static native void nativeTestRefreshRateCallbackMixedWithFrameCallbacks(long ptr);

    private Context mContext;
    private DisplayManager mDisplayManager;

    static {
        System.loadLibrary("ctsview_jni");
    }

    @UiThreadTest
    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mDisplayManager = (DisplayManager) mContext.getSystemService(Context.DISPLAY_SERVICE);

        Optional<Display> defaultDisplayOpt = Arrays.stream(mDisplayManager.getDisplays())
                .filter(display -> display.getDisplayId() == Display.DEFAULT_DISPLAY)
                .findFirst();

        assertTrue(defaultDisplayOpt.isPresent());
        Display defaultDisplay = defaultDisplayOpt.get();

        long[] supportedPeriods = Arrays.stream(defaultDisplay.getSupportedModes())
                .mapToLong(mode -> (long) (Duration.ofSeconds(1).toNanos() / mode.getRefreshRate()))
                .toArray();

        mChoreographerPtr = nativeGetChoreographer();
        if (!nativePrepareChoreographerTests(mChoreographerPtr, supportedPeriods)) {
            fail("Failed to setup choreographer tests");
        }
    }

    @MediumTest
    @Test
    public void testPostCallback64WithoutDelayEventuallyRunsCallbacks() {
        nativeTestPostCallback64WithoutDelayEventuallyRunsCallbacks(mChoreographerPtr);
    }

    @MediumTest
    @Test
    public void testPostCallback64WithDelayEventuallyRunsCallbacks() {
        nativeTestPostCallback64WithDelayEventuallyRunsCallbacks(mChoreographerPtr);
    }

    @MediumTest
    @Test
    public void testPostCallbackWithoutDelayEventuallyRunsCallbacks() {
        nativeTestPostCallbackWithoutDelayEventuallyRunsCallbacks(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testPostCallbackWithDelayEventuallyRunsCallbacks() {
        nativeTestPostCallbackWithDelayEventuallyRunsCallbacks(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testPostCallbackMixedWithoutDelayEventuallyRunsCallbacks() {
        nativeTestPostCallbackMixedWithoutDelayEventuallyRunsCallbacks(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testPostCallbackMixedWithDelayEventuallyRunsCallbacks() {
        nativeTestPostCallbackMixedWithDelayEventuallyRunsCallbacks(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testRefreshRateCallback() {
        nativeTestRefreshRateCallback(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testUnregisteringRefreshRateCallback() {
        nativeTestUnregisteringRefreshRateCallback(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testMultipleRefreshRateCallbacks() {
        nativeTestMultipleRefreshRateCallbacks(mChoreographerPtr);
    }

    @SmallTest
    @Test
    public void testAttemptToAddRefreshRateCallbackTwiceDoesNotAddTwice() {
        nativeTestAttemptToAddRefreshRateCallbackTwiceDoesNotAddTwice(mChoreographerPtr);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testRefreshRateCallbackMixedWithFrameCallbacks() {
        nativeTestRefreshRateCallbackMixedWithFrameCallbacks(mChoreographerPtr);
    }

}
