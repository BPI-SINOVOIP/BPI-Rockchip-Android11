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

import static android.system.OsConstants.EINVAL;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.Collections;

/**
 * An Activity to help with frame rate testing.
 */
public class FrameRateCtsActivity extends Activity {
    static {
        System.loadLibrary("ctsgraphics_jni");
    }

    private static String TAG = "FrameRateCtsActivity";
    private static final long FRAME_RATE_SWITCH_GRACE_PERIOD_SECONDS = 2;
    private static final long STABLE_FRAME_RATE_WAIT_SECONDS = 1;
    private static final long POST_BUFFER_INTERVAL_MILLIS = 500;
    private static final int PRECONDITION_WAIT_MAX_ATTEMPTS = 5;
    private static final long PRECONDITION_WAIT_TIMEOUT_SECONDS = 20;
    private static final long PRECONDITION_VIOLATION_WAIT_TIMEOUT_SECONDS = 3;

    private DisplayManager mDisplayManager;
    private SurfaceView mSurfaceView;
    private Handler mHandler = new Handler(Looper.getMainLooper());
    private Object mLock = new Object();
    private Surface mSurface = null;
    private float mDeviceFrameRate;
    private ArrayList<Float> mFrameRateChangedEvents = new ArrayList<Float>();

    private enum ActivityState { RUNNING, PAUSED, DESTROYED }

    private ActivityState mActivityState;

    SurfaceHolder.Callback mSurfaceHolderCallback = new SurfaceHolder.Callback() {
        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            synchronized (mLock) {
                mSurface = holder.getSurface();
                mLock.notify();
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            synchronized (mLock) {
                mSurface = null;
                mLock.notify();
            }
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}
    };

    DisplayManager.DisplayListener mDisplayListener = new DisplayManager.DisplayListener() {
        @Override
        public void onDisplayAdded(int displayId) {}

        @Override
        public void onDisplayChanged(int displayId) {
            if (displayId != Display.DEFAULT_DISPLAY) {
                return;
            }
            synchronized (mLock) {
                float frameRate = mDisplayManager.getDisplay(displayId).getMode().getRefreshRate();
                if (frameRate != mDeviceFrameRate) {
                    Log.i(TAG,
                            String.format("Frame rate changed: %.0f --> %.0f", mDeviceFrameRate,
                                    frameRate));
                    mDeviceFrameRate = frameRate;
                    mFrameRateChangedEvents.add(frameRate);
                    mLock.notify();
                }
            }
        }

        @Override
        public void onDisplayRemoved(int displayId) {}
    };

    private static class PreconditionViolatedException extends RuntimeException {
        PreconditionViolatedException() {}
    }

    private static class FrameRateTimeoutException extends RuntimeException {
        FrameRateTimeoutException(float appRequestedFrameRate, float deviceFrameRate) {
            this.appRequestedFrameRate = appRequestedFrameRate;
            this.deviceFrameRate = deviceFrameRate;
        }

        public float appRequestedFrameRate;
        public float deviceFrameRate;
    }

    public enum Api {
        SURFACE("Surface"),
        ANATIVE_WINDOW("ANativeWindow"),
        SURFACE_CONTROL("SurfaceControl"),
        NATIVE_SURFACE_CONTROL("ASurfaceControl");

        private final String mName;
        Api(String name) {
            mName = name;
        }

        public String toString() {
            return mName;
        }
    }

    private static class TestSurface {
        private Api mApi;
        private String mName;
        private SurfaceControl mSurfaceControl;
        private Surface mSurface;
        private long mNativeSurfaceControl;
        private int mColor;
        private boolean mLastBufferPostTimeValid;
        private long mLastBufferPostTime;

        TestSurface(Api api, SurfaceControl parentSurfaceControl, Surface parentSurface,
                String name, Rect destFrame, boolean visible, int color) {
            mApi = api;
            mName = name;
            mColor = color;

            if (mApi == Api.SURFACE || mApi == Api.ANATIVE_WINDOW || mApi == Api.SURFACE_CONTROL) {
                assertTrue("No parent surface", parentSurfaceControl != null);
                mSurfaceControl = new SurfaceControl.Builder()
                                          .setParent(parentSurfaceControl)
                                          .setName(mName)
                                          .setBufferSize(destFrame.right - destFrame.left,
                                                  destFrame.bottom - destFrame.top)
                                          .build();
                SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
                try {
                    transaction.setGeometry(mSurfaceControl, null, destFrame, Surface.ROTATION_0)
                            .apply();
                } finally {
                    transaction.close();
                }
                mSurface = new Surface(mSurfaceControl);
            } else if (mApi == Api.NATIVE_SURFACE_CONTROL) {
                assertTrue("No parent surface", parentSurface != null);
                mNativeSurfaceControl = nativeSurfaceControlCreate(parentSurface, mName,
                        destFrame.left, destFrame.top, destFrame.right, destFrame.bottom);
                assertTrue("Failed to create a native SurfaceControl", mNativeSurfaceControl != 0);
            }

            setVisibility(visible);
            postBuffer();
        }

        public int setFrameRate(float frameRate, int compatibility) {
            Log.i(TAG,
                    String.format("Setting frame rate for %s: fps=%.0f compatibility=%s", mName,
                            frameRate, frameRateCompatibilityToString(compatibility)));

            int rc = 0;
            if (mApi == Api.SURFACE) {
                mSurface.setFrameRate(frameRate, compatibility);
            } else if (mApi == Api.ANATIVE_WINDOW) {
                rc = nativeWindowSetFrameRate(mSurface, frameRate, compatibility);
            } else if (mApi == Api.SURFACE_CONTROL) {
                SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
                try {
                    transaction.setFrameRate(mSurfaceControl, frameRate, compatibility).apply();
                } finally {
                    transaction.close();
                }
            } else if (mApi == Api.NATIVE_SURFACE_CONTROL) {
                nativeSurfaceControlSetFrameRate(mNativeSurfaceControl, frameRate, compatibility);
            }
            return rc;
        }

        public void setInvalidFrameRate(float frameRate, int compatibility) {
            if (mApi == Api.SURFACE) {
                boolean caughtIllegalArgException = false;
                try {
                    setFrameRate(frameRate, compatibility);
                } catch (IllegalArgumentException exc) {
                    caughtIllegalArgException = true;
                }
                assertTrue("Expected an IllegalArgumentException from invalid call to"
                                + " Surface.setFrameRate()",
                        caughtIllegalArgException);
            } else {
                int rc = setFrameRate(frameRate, compatibility);
                if (mApi == Api.ANATIVE_WINDOW) {
                    assertTrue("Expected -EINVAL return value from invalid call to"
                                    + " ANativeWindow_setFrameRate()",
                            rc == -EINVAL);
                }
            }
        }

        public void setVisibility(boolean visible) {
            Log.i(TAG,
                    String.format("Setting visibility for %s: %s", mName,
                            visible ? "visible" : "hidden"));
            if (mApi == Api.SURFACE || mApi == Api.ANATIVE_WINDOW || mApi == Api.SURFACE_CONTROL) {
                SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
                try {
                    transaction.setVisibility(mSurfaceControl, visible).apply();
                } finally {
                    transaction.close();
                }
            } else if (mApi == Api.NATIVE_SURFACE_CONTROL) {
                nativeSurfaceControlSetVisibility(mNativeSurfaceControl, visible);
            }
        }

        public void postBuffer() {
            mLastBufferPostTimeValid = true;
            mLastBufferPostTime = System.nanoTime();
            if (mApi == Api.SURFACE || mApi == Api.ANATIVE_WINDOW || mApi == Api.SURFACE_CONTROL) {
                Canvas canvas = mSurface.lockHardwareCanvas();
                canvas.drawColor(mColor);
                mSurface.unlockCanvasAndPost(canvas);
            } else if (mApi == Api.NATIVE_SURFACE_CONTROL) {
                assertTrue("Posting a buffer failed",
                        nativeSurfaceControlPostBuffer(mNativeSurfaceControl, mColor));
            }
        }

        public long getLastBufferPostTime() {
            assertTrue("No buffer posted yet", mLastBufferPostTimeValid);
            return mLastBufferPostTime;
        }

        public void release() {
            if (mSurface != null) {
                mSurface.release();
                mSurface = null;
            }
            if (mSurfaceControl != null) {
                SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
                try {
                    transaction.reparent(mSurfaceControl, null).apply();
                } finally {
                    transaction.close();
                }
                mSurfaceControl.release();
                mSurfaceControl = null;
            }
            if (mNativeSurfaceControl != 0) {
                nativeSurfaceControlDestroy(mNativeSurfaceControl);
                mNativeSurfaceControl = 0;
            }
        }

        @Override
        protected void finalize() throws Throwable {
            try {
                release();
            } finally {
                super.finalize();
            }
        }
    }

    private static String frameRateCompatibilityToString(int compatibility) {
        switch (compatibility) {
            case Surface.FRAME_RATE_COMPATIBILITY_DEFAULT:
                return "default";
            case Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE:
                return "fixed_source";
            default:
                return "invalid(" + compatibility + ")";
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        synchronized (mLock) {
            mDisplayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
            mDeviceFrameRate =
                    mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY).getMode().getRefreshRate();
            mDisplayManager.registerDisplayListener(mDisplayListener, mHandler);
            mSurfaceView = new SurfaceView(this);
            mSurfaceView.setWillNotDraw(false);
            mSurfaceView.setZOrderOnTop(true);
            setContentView(mSurfaceView,
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
            mSurfaceView.getHolder().addCallback(mSurfaceHolderCallback);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mDisplayManager.unregisterDisplayListener(mDisplayListener);
        synchronized (mLock) {
            mActivityState = ActivityState.DESTROYED;
            mLock.notify();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        synchronized (mLock) {
            mActivityState = ActivityState.PAUSED;
            mLock.notify();
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        synchronized (mLock) {
            mActivityState = ActivityState.RUNNING;
            mLock.notify();
        }
    }

    private ArrayList<Float> getFrameRatesToTest() {
        Display display = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
        Display.Mode[] modes = display.getSupportedModes();
        Display.Mode currentMode = display.getMode();
        ArrayList<Float> frameRates = new ArrayList<Float>();
        for (Display.Mode mode : modes) {
            if (mode.getPhysicalWidth() == currentMode.getPhysicalWidth()
                    && mode.getPhysicalHeight() == currentMode.getPhysicalHeight()) {
                frameRates.add(mode.getRefreshRate());
            }
        }
        Collections.sort(frameRates);
        ArrayList<Float> uniqueFrameRates = new ArrayList<Float>();
        for (float frameRate : frameRates) {
            if (uniqueFrameRates.isEmpty()
                    || frameRate - uniqueFrameRates.get(uniqueFrameRates.size() - 1) >= 1.0f) {
                uniqueFrameRates.add(frameRate);
            }
        }
        return uniqueFrameRates;
    }

    private boolean isFrameRateMultiple(float higherFrameRate, float lowerFrameRate) {
        float multiple = higherFrameRate / lowerFrameRate;
        int roundedMultiple = Math.round(multiple);
        return roundedMultiple > 0
                && Math.abs(roundedMultiple * lowerFrameRate - higherFrameRate) <= 0.1f;
    }

    // Returns two device-supported frame rates that aren't multiples of each other, or null if no
    // such incompatible frame rates are available. This is useful for testing behavior where we
    // have layers with conflicting frame rates.
    private float[] getIncompatibleFrameRates() {
        ArrayList<Float> frameRates = getFrameRatesToTest();
        for (int i = 0; i < frameRates.size(); i++) {
            for (int j = i + 1; j < frameRates.size(); j++) {
                if (!isFrameRateMultiple(Math.max(frameRates.get(i), frameRates.get(j)),
                            Math.min(frameRates.get(i), frameRates.get(j)))) {
                    return new float[] {frameRates.get(i), frameRates.get(j)};
                }
            }
        }
        return null;
    }

    // Waits until our SurfaceHolder has a surface and the activity is resumed.
    private void waitForPreconditions() throws InterruptedException {
        assertTrue(
                "Activity was unexpectedly destroyed", mActivityState != ActivityState.DESTROYED);
        if (mSurface == null || mActivityState != ActivityState.RUNNING) {
            Log.i(TAG,
                    String.format(
                            "Waiting for preconditions. Have surface? %b. Activity resumed? %b.",
                            mSurface != null, mActivityState == ActivityState.RUNNING));
        }
        long nowNanos = System.nanoTime();
        long endTimeNanos = nowNanos + PRECONDITION_WAIT_TIMEOUT_SECONDS * 1_000_000_000L;
        while (mSurface == null || mActivityState != ActivityState.RUNNING) {
            long timeRemainingMillis = (endTimeNanos - nowNanos) / 1_000_000;
            assertTrue(String.format("Timed out waiting for preconditions. Have surface? %b."
                                       + " Activity resumed? %b.",
                               mSurface != null, mActivityState == ActivityState.RUNNING),
                    timeRemainingMillis > 0);
            mLock.wait(timeRemainingMillis);
            assertTrue("Activity was unexpectedly destroyed",
                    mActivityState != ActivityState.DESTROYED);
            nowNanos = System.nanoTime();
        }
    }

    // Returns true if we encounter a precondition violation, false otherwise.
    private boolean waitForPreconditionViolation() throws InterruptedException {
        assertTrue(
                "Activity was unexpectedly destroyed", mActivityState != ActivityState.DESTROYED);
        long nowNanos = System.nanoTime();
        long endTimeNanos = nowNanos + PRECONDITION_VIOLATION_WAIT_TIMEOUT_SECONDS * 1_000_000_000L;
        while (mSurface != null && mActivityState == ActivityState.RUNNING) {
            long timeRemainingMillis = (endTimeNanos - nowNanos) / 1_000_000;
            if (timeRemainingMillis <= 0) {
                break;
            }
            mLock.wait(timeRemainingMillis);
            assertTrue("Activity was unexpectedly destroyed",
                    mActivityState != ActivityState.DESTROYED);
            nowNanos = System.nanoTime();
        }
        return mSurface == null || mActivityState != ActivityState.RUNNING;
    }

    private void verifyPreconditions() {
        if (mSurface == null || mActivityState != ActivityState.RUNNING) {
            throw new PreconditionViolatedException();
        }
    }

    // Returns true if we reached waitUntilNanos, false if some other event occurred.
    private boolean waitForEvents(long waitUntilNanos, ArrayList<TestSurface> surfaces)
            throws InterruptedException {
        mFrameRateChangedEvents.clear();
        long nowNanos = System.nanoTime();
        while (nowNanos < waitUntilNanos) {
            long surfacePostTime = Long.MAX_VALUE;
            for (TestSurface surface : surfaces) {
                surfacePostTime = Math.min(surfacePostTime,
                        surface.getLastBufferPostTime()
                                + (POST_BUFFER_INTERVAL_MILLIS * 1_000_000L));
            }
            long timeoutNs = Math.min(waitUntilNanos, surfacePostTime) - nowNanos;
            long timeoutMs = timeoutNs / 1_000_000L;
            int remainderNs = (int) (timeoutNs % 1_000_000L);
            // Don't call wait(0, 0) - it blocks indefinitely.
            if (timeoutMs > 0 || remainderNs > 0) {
                mLock.wait(timeoutMs, remainderNs);
            }
            nowNanos = System.nanoTime();
            verifyPreconditions();
            if (!mFrameRateChangedEvents.isEmpty()) {
                return false;
            }
            if (nowNanos >= surfacePostTime) {
                for (TestSurface surface : surfaces) {
                    surface.postBuffer();
                }
            }
        }
        return true;
    }

    private void verifyCompatibleAndStableFrameRate(float appRequestedFrameRate,
            ArrayList<TestSurface> surfaces) throws InterruptedException {
        Log.i(TAG, "Verifying compatible and stable frame rate");
        long nowNanos = System.nanoTime();
        long gracePeriodEndTimeNanos =
                nowNanos + FRAME_RATE_SWITCH_GRACE_PERIOD_SECONDS * 1_000_000_000L;
        while (true) {
            // Wait until we switch to a compatible frame rate.
            while (!isFrameRateMultiple(mDeviceFrameRate, appRequestedFrameRate)
                    && !waitForEvents(gracePeriodEndTimeNanos, surfaces)) {
                // Empty
            }
            nowNanos = System.nanoTime();
            if (nowNanos >= gracePeriodEndTimeNanos) {
                throw new FrameRateTimeoutException(appRequestedFrameRate, mDeviceFrameRate);
            }

            // We've switched to a compatible frame rate. Now wait for a while to see if we stay at
            // that frame rate.
            long endTimeNanos = nowNanos + STABLE_FRAME_RATE_WAIT_SECONDS * 1_000_000_000L;
            while (endTimeNanos > nowNanos) {
                if (waitForEvents(endTimeNanos, surfaces)) {
                    Log.i(TAG, String.format("Stable frame rate %.0f verified", mDeviceFrameRate));
                    return;
                }
                nowNanos = System.nanoTime();
                if (!mFrameRateChangedEvents.isEmpty()) {
                    break;
                }
            }
        }
    }

    // Unfortunately, we can't just use Consumer<Api> for this, because we need to declare that it
    // throws InterruptedException.
    private interface TestInterface {
        void run(Api api) throws InterruptedException;
    }

    // Runs the given test for each api, waiting for the preconditions to be satisfied before
    // running the test. Includes retry logic when the test fails because the preconditions are
    // violated. E.g. if we lose the SurfaceHolder's surface, or the activity is paused/resumed,
    // we'll retry the test. The activity being intermittently paused/resumed has been observed to
    // cause test failures in practice.
    private void runTestsWithPreconditions(TestInterface test, String testName)
            throws InterruptedException {
        synchronized (mLock) {
            for (Api api : Api.values()) {
                Log.i(TAG, String.format("Testing %s %s", api, testName));
                int attempts = 0;
                boolean testPassed = false;
                try {
                    while (!testPassed) {
                        waitForPreconditions();
                        try {
                            test.run(api);
                            testPassed = true;
                        } catch (PreconditionViolatedException exc) {
                            // The logic below will retry if we're below max attempts.
                        } catch (FrameRateTimeoutException exc) {
                            // Sometimes we get a test timeout failure before we get the
                            // notification that the activity was paused, and it was the pause that
                            // caused the timeout failure. Wait for a bit to see if we get notified
                            // of a precondition violation, and if so, retry the test. Otherwise
                            // fail.
                            assertTrue(
                                    String.format(
                                            "Timed out waiting for a stable and compatible frame"
                                                    + " rate. requested=%.0f received=%.0f.",
                                            exc.appRequestedFrameRate, exc.deviceFrameRate),
                                    waitForPreconditionViolation());
                        }

                        if (!testPassed) {
                            Log.i(TAG,
                                    String.format("Preconditions violated while running the test."
                                                    + " Have surface? %b. Activity resumed? %b.",
                                            mSurface != null,
                                            mActivityState == ActivityState.RUNNING));
                            attempts++;
                            assertTrue(String.format(
                                               "Exceeded %d precondition wait attempts. Giving up.",
                                               PRECONDITION_WAIT_MAX_ATTEMPTS),
                                    attempts < PRECONDITION_WAIT_MAX_ATTEMPTS);
                        }
                    }
                } finally {
                    String passFailMessage = String.format(
                            "%s %s %s", testPassed ? "Passed" : "Failed", api, testName);
                    if (testPassed) {
                        Log.i(TAG, passFailMessage);
                    } else {
                        Log.e(TAG, passFailMessage);
                    }
                }
            }
        }
    }

    private void testExactFrameRateMatch(Api api) throws InterruptedException {
        ArrayList<Float> frameRatesToTest = getFrameRatesToTest();
        TestSurface surface = null;
        try {
            surface = new TestSurface(api, mSurfaceView.getSurfaceControl(), mSurface,
                    "testSurface", mSurfaceView.getHolder().getSurfaceFrame(),
                    /*visible=*/true, Color.RED);
            ArrayList<TestSurface> surfaces = new ArrayList<>();
            surfaces.add(surface);
            for (float frameRate : frameRatesToTest) {
                surface.setFrameRate(frameRate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
                verifyCompatibleAndStableFrameRate(frameRate, surfaces);
            }
            surface.setFrameRate(0.f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
        } finally {
            if (surface != null) {
                surface.release();
            }
        }
    }

    public void testExactFrameRateMatch() throws InterruptedException {
        runTestsWithPreconditions(api -> testExactFrameRateMatch(api), "exact frame rate match");
    }

    private void testFixedSource(Api api) throws InterruptedException {
        float[] incompatibleFrameRates = getIncompatibleFrameRates();
        if (incompatibleFrameRates == null) {
            Log.i(TAG, "No incompatible frame rates to use for testing fixed_source behavior");
            return;
        }

        float frameRateA = incompatibleFrameRates[0];
        float frameRateB = incompatibleFrameRates[1];
        Log.i(TAG,
                String.format("Testing with incompatible frame rates: surfaceA=%.0f surfaceB=%.0f",
                        frameRateA, frameRateB));
        TestSurface surfaceA = null;
        TestSurface surfaceB = null;
        try {
            int width = mSurfaceView.getHolder().getSurfaceFrame().width();
            int height = mSurfaceView.getHolder().getSurfaceFrame().height() / 2;
            Rect destFrameA = new Rect(/*left=*/0, /*top=*/0, /*right=*/width, /*bottom=*/height);
            surfaceA = new TestSurface(api, mSurfaceView.getSurfaceControl(), mSurface, "surfaceA",
                    destFrameA, /*visible=*/true, Color.RED);
            Rect destFrameB = new Rect(
                    /*left=*/0, /*top=*/height, /*right=*/width, /*bottom=*/height * 2);
            surfaceB = new TestSurface(api, mSurfaceView.getSurfaceControl(), mSurface, "surfaceB",
                    destFrameB, /*visible=*/false, Color.GREEN);

            surfaceA.setFrameRate(frameRateA, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
            surfaceB.setFrameRate(frameRateB, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);

            ArrayList<TestSurface> surfaces = new ArrayList<>();
            surfaces.add(surfaceA);
            surfaces.add(surfaceB);

            verifyCompatibleAndStableFrameRate(frameRateA, surfaces);
            surfaceB.setVisibility(true);
            verifyCompatibleAndStableFrameRate(frameRateB, surfaces);
        } finally {
            if (surfaceA != null) {
                surfaceA.release();
            }
            if (surfaceB != null) {
                surfaceB.release();
            }
        }
    }

    public void testFixedSource() throws InterruptedException {
        runTestsWithPreconditions(api -> testFixedSource(api), "fixed source behavior");
    }

    private void testInvalidParams(Api api) throws InterruptedException {
        TestSurface surface = null;
        try {
            surface = new TestSurface(api, mSurfaceView.getSurfaceControl(), mSurface,
                    "testSurface", mSurfaceView.getHolder().getSurfaceFrame(),
                    /*visible=*/true, Color.RED);
            surface.setInvalidFrameRate(-100.f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
            surface.setInvalidFrameRate(
                    Float.POSITIVE_INFINITY, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
            surface.setInvalidFrameRate(Float.NaN, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
            surface.setInvalidFrameRate(0.f, -10);
            surface.setInvalidFrameRate(0.f, 50);
        } finally {
            if (surface != null) {
                surface.release();
            }
        }
    }

    public void testInvalidParams() throws InterruptedException {
        runTestsWithPreconditions(api -> testInvalidParams(api), "invalid params behavior");
    }

    private static native int nativeWindowSetFrameRate(
            Surface surface, float frameRate, int compatibility);
    private static native long nativeSurfaceControlCreate(
            Surface parentSurface, String name, int left, int top, int right, int bottom);
    private static native void nativeSurfaceControlDestroy(long surfaceControl);
    private static native void nativeSurfaceControlSetFrameRate(
            long surfaceControl, float frameRate, int compatibility);
    private static native void nativeSurfaceControlSetVisibility(
            long surfaceControl, boolean visible);
    private static native boolean nativeSurfaceControlPostBuffer(long surfaceControl, int color);
}
