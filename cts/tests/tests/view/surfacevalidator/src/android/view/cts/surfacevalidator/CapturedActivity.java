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
package android.view.cts.surfacevalidator;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.provider.Settings;
import android.server.wm.settings.SettingsSession;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseArray;
import android.view.Display;
import android.view.PointerIcon;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import androidx.test.InstrumentationRegistry;

import org.junit.rules.TestName;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class CapturedActivity extends Activity {
    public static class TestResult {
        public int passFrames;
        public int failFrames;
        public final SparseArray<Bitmap> failures = new SparseArray<>();
    }

    private static class ImmersiveConfirmationSetting extends SettingsSession<String> {
        ImmersiveConfirmationSetting() {
            super(Settings.Secure.getUriFor(
                Settings.Secure.IMMERSIVE_MODE_CONFIRMATIONS),
                Settings.Secure::getString, Settings.Secure::putString);
        }
    }

    private ImmersiveConfirmationSetting mSettingsSession;

    private static final String TAG = "CapturedActivity";
    private static final int PERMISSION_CODE = 1;
    private MediaProjectionManager mProjectionManager;
    private MediaProjection mMediaProjection;
    private VirtualDisplay mVirtualDisplay;

    private SurfacePixelValidator2 mSurfacePixelValidator;

    private static final int PERMISSION_DIALOG_WAIT_MS = 1000;
    private static final int RETRY_COUNT = 2;

    private static final long START_CAPTURE_DELAY_MS = 4000;

    private static final String ACCEPT_RESOURCE_ID = "android:id/button1";

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private volatile boolean mOnEmbedded;
    private volatile boolean mOnWatch;
    private CountDownLatch mCountDownLatch;
    private boolean mProjectionServiceBound = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final PackageManager packageManager = getPackageManager();
        mOnWatch = packageManager.hasSystemFeature(PackageManager.FEATURE_WATCH);
        if (mOnWatch) {
            // Don't try and set up test/capture infrastructure - they're not supported
            return;
        }
        // Embedded devices are significantly slower, and are given
        // longer duration to capture the expected number of frames
        mOnEmbedded = packageManager.hasSystemFeature(PackageManager.FEATURE_EMBEDDED);

        mSettingsSession = new ImmersiveConfirmationSetting();
        mSettingsSession.set("confirmed");

        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN);
        // Set the NULL pointer icon so that it won't obstruct the captured image.
        getWindow().getDecorView().setPointerIcon(
                PointerIcon.getSystemIcon(this, PointerIcon.TYPE_NULL));
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);


        mProjectionManager =
                (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        mCountDownLatch = new CountDownLatch(1);
        bindMediaProjectionService();
    }

    public void dismissPermissionDialog() {
        // The permission dialog will be auto-opened by the activity - find it and accept
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        UiObject2 acceptButton = uiDevice.wait(Until.findObject(By.res(ACCEPT_RESOURCE_ID)),
                PERMISSION_DIALOG_WAIT_MS);
        if (acceptButton != null) {
            Log.d(TAG, "found permission dialog after searching all windows, clicked");
            acceptButton.click();
        }
    }

    private ServiceConnection mConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            startActivityForResult(mProjectionManager.createScreenCaptureIntent(), PERMISSION_CODE);
            dismissPermissionDialog();
            mProjectionServiceBound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            mProjectionServiceBound = false;
        }
    };

    private void bindMediaProjectionService() {
        Intent intent = new Intent(this, LocalMediaProjectionService.class);
        startService(intent);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
        if (mMediaProjection != null) {
            mMediaProjection.stop();
            mMediaProjection = null;
        }
        if (mProjectionServiceBound) {
            unbindService(mConnection);
            mProjectionServiceBound = false;
        }
        mSettingsSession.close();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mOnWatch) return;
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN);

        if (requestCode != PERMISSION_CODE) {
            throw new IllegalStateException("Unknown request code: " + requestCode);
        }
        if (resultCode != RESULT_OK) {
            throw new IllegalStateException("User denied screen sharing permission");
        }
        Log.d(TAG, "onActivityResult");
        mMediaProjection = mProjectionManager.getMediaProjection(resultCode, data);
        mMediaProjection.registerCallback(new MediaProjectionCallback(), null);
        mCountDownLatch.countDown();
    }

    public long getCaptureDurationMs() {
        return mOnEmbedded ? 100000 : 50000;
    }

    public TestResult runTest(ISurfaceValidatorTestCase animationTestCase) throws Throwable {
        TestResult testResult = new TestResult();
        if (mOnWatch) {
            /**
             * Watch devices not supported, since they may not support:
             *    1) displaying unmasked windows
             *    2) RenderScript
             *    3) Video playback
             */
            Log.d(TAG, "Skipping test on watch.");
            testResult.passFrames = 1000;
            testResult.failFrames = 0;
            return testResult;
        }

        final long timeOutMs = mOnEmbedded ? 125000 : 62500;
        final long captureDuration = animationTestCase.hasAnimation() ?
            getCaptureDurationMs() : 0;
        final long endCaptureDelayMs = START_CAPTURE_DELAY_MS + captureDuration;
        final long endDelayMs = endCaptureDelayMs + 1000;

        int count = 0;
        // Sometimes system decides to rotate the permission activity to another orientation
        // right after showing it. This results in: uiautomation thinks that accept button appears,
        // we successfully click it in terms of uiautomation, but nothing happens,
        // because permission activity is already recreated.
        // Thus, we try to click that button multiple times.
        do {
            assertTrue("Can't get the permission", count <= RETRY_COUNT);
            dismissPermissionDialog();
            count++;
        } while (!mCountDownLatch.await(timeOutMs, TimeUnit.MILLISECONDS));

        mHandler.post(() -> {
            Log.d(TAG, "Setting up test case");

            // shouldn't be necessary, since we've already done this in #create,
            // but ensure status/nav are hidden for test
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN);

            animationTestCase.start(getApplicationContext(),
                    (FrameLayout) findViewById(android.R.id.content));
        });

        mHandler.postDelayed(() -> {
            Log.d(TAG, "Starting capture");

            Display display = getWindow().getDecorView().getDisplay();
            Point size = new Point();
            DisplayMetrics metrics = new DisplayMetrics();
            display.getMetrics(metrics);

            final DisplayManager displayManager =
                    (DisplayManager) CapturedActivity.this.getSystemService(
                    Context.DISPLAY_SERVICE);
            final Display defaultDisplay = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
            final int rotation = defaultDisplay.getRotation();
            Display.Mode mode = defaultDisplay.getMode();
            size = new Point(mode.getPhysicalWidth(), mode.getPhysicalHeight());

            View testAreaView = findViewById(android.R.id.content);
            Rect boundsToCheck = new Rect(0, 0, testAreaView.getWidth(), testAreaView.getHeight());
            int[] topLeft = new int[2];
            testAreaView.getLocationOnScreen(topLeft);
            boundsToCheck.offset(topLeft[0], topLeft[1]);

            if (boundsToCheck.width() < 90 || boundsToCheck.height() < 90) {
                fail("capture bounds too small to be a fullscreen activity: " + boundsToCheck);
            }

            mSurfacePixelValidator = new SurfacePixelValidator2(CapturedActivity.this,
                    size, boundsToCheck, animationTestCase.getChecker());
            Log.d("MediaProjection", "Size is " + size.toString()
                    + ", bounds are " + boundsToCheck.toShortString());
            mVirtualDisplay = mMediaProjection.createVirtualDisplay("CtsCapturedActivity",
                    size.x, size.y,
                    metrics.densityDpi,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                    mSurfacePixelValidator.getSurface(),
                    null /*Callbacks*/,
                    null /*Handler*/);
        }, START_CAPTURE_DELAY_MS);

        final int SINGLE_FRAME_TIMEOUT_MS = 1000;
        mHandler.postDelayed(() -> {
            Log.d(TAG, "Stopping capture");
            mSurfacePixelValidator.waitForFrame(SINGLE_FRAME_TIMEOUT_MS);
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }, endCaptureDelayMs);

        final CountDownLatch latch = new CountDownLatch(1);
        mHandler.postDelayed(() -> {
            Log.d(TAG, "Ending test case");
            animationTestCase.end();
            mSurfacePixelValidator.finish(testResult);
            latch.countDown();
            mSurfacePixelValidator = null;
        }, endDelayMs);

        boolean latchResult = latch.await(timeOutMs, TimeUnit.MILLISECONDS);
        if (!latchResult) {
            testResult.passFrames = 0;
            testResult.failFrames = 1000;
        }
        Log.d(TAG, "Test finished, passFrames " + testResult.passFrames
                + ", failFrames " + testResult.failFrames);
        return testResult;
    }

    private void saveFailureCaptures(SparseArray<Bitmap> failFrames, TestName name) {
        if (failFrames.size() == 0) return;

        String directoryName = Environment.getExternalStorageDirectory()
                + "/" + getClass().getSimpleName()
                + "/" + name.getMethodName();
        File testDirectory = new File(directoryName);
        if (testDirectory.exists()) {
            String[] children = testDirectory.list();
            if (children == null) {
                return;
            }
            for (String file : children) {
                new File(testDirectory, file).delete();
            }
        } else {
            testDirectory.mkdirs();
        }

        for (int i = 0; i < failFrames.size(); i++) {
            int frameNr = failFrames.keyAt(i);
            Bitmap bitmap = failFrames.valueAt(i);

            String bitmapName =  "frame_" + frameNr + ".png";
            Log.d(TAG, "Saving file : " + bitmapName + " in directory : " + directoryName);

            File file = new File(directoryName, bitmapName);
            try (FileOutputStream fileStream = new FileOutputStream(file)) {
                bitmap.compress(Bitmap.CompressFormat.PNG, 85, fileStream);
                fileStream.flush();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public void verifyTest(ISurfaceValidatorTestCase testCase, TestName name) throws Throwable {
        CapturedActivity.TestResult result = runTest(testCase);
        saveFailureCaptures(result.failures, name);

        float failRatio = 1.0f * result.failFrames / (result.failFrames + result.passFrames);
        assertTrue("Error: " + failRatio + " fail ratio - extremely high, is activity obstructed?",
                failRatio < 0.95f);
        assertTrue("Error: " + result.failFrames
                        + " incorrect frames observed - incorrect positioning",
                result.failFrames == 0);

        if (testCase.hasAnimation()) {
            float framesPerSecond = 1.0f * result.passFrames
                    / TimeUnit.MILLISECONDS.toSeconds(getCaptureDurationMs());
            assertTrue("Error, only " + result.passFrames
                    + " frames observed, virtual display only capturing at "
                    + framesPerSecond + " frames per second",
                    result.passFrames > 100);
        }
    }

    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onStop() {
            Log.d(TAG, "MediaProjectionCallback#onStop");
            if (mVirtualDisplay != null) {
                mVirtualDisplay.release();
                mVirtualDisplay = null;
            }
        }
    }
}
