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

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Trace;
import android.util.Log;
import android.util.SparseArray;
import android.view.Surface;


public class SurfacePixelValidator2 {
    private static final String TAG = "SurfacePixelValidator";

    private static final int MAX_CAPTURED_FAILURES = 5;
    private static final int PIXEL_STRIDE = 4;

    private final int mWidth;
    private final int mHeight;

    private final HandlerThread mWorkerThread;

    private final PixelChecker mPixelChecker;
    private final Rect mBoundsToCheck;
    private ImageReader mImageReader;

    private final Object mResultLock = new Object();
    private int mResultSuccessFrames;
    private int mResultFailureFrames;
    private SparseArray<Bitmap> mFirstFailures = new SparseArray<>(MAX_CAPTURED_FAILURES);

    private ImageReader.OnImageAvailableListener mOnImageAvailable =
            new ImageReader.OnImageAvailableListener() {

        @Override
        public void onImageAvailable(ImageReader reader) {
            Trace.beginSection("Read buffer");
            Image image = reader.acquireNextImage();

            Image.Plane plane = image.getPlanes()[0];
            if (plane.getPixelStride() != PIXEL_STRIDE) {
                throw new IllegalStateException("pixel stride != " + PIXEL_STRIDE + "? "
                                                + plane.getPixelStride());
            }
            Trace.endSection();

            boolean success = mPixelChecker.validatePlane(plane, mBoundsToCheck, mWidth, mHeight);

            synchronized (mResultLock) {
                mResultLock.notifyAll();
                if (success) {
                    mResultSuccessFrames++;
                } else {
                    mResultFailureFrames++;
                    int totalFramesSeen = mResultSuccessFrames + mResultFailureFrames;
                    Log.d(TAG, "Failure (" + mPixelChecker.getLastError() + ") occurred on frame "
                            + totalFramesSeen);

                    if (mFirstFailures.size() < MAX_CAPTURED_FAILURES) {
                        Log.d(TAG, "Capturing bitmap #" + mFirstFailures.size());
                        // error, worth looking at...
                        Bitmap capture = Bitmap.wrapHardwareBuffer(
                                image.getHardwareBuffer(), null)
                                .copy(Bitmap.Config.ARGB_8888, false);
                        mFirstFailures.put(totalFramesSeen, capture);
                    }
                }
            }
            image.close();
        }
    };

    void waitForFrame(int timeoutMs) {
        synchronized (mResultLock) {
            if (mResultSuccessFrames != 0 || mResultFailureFrames != 0) {
                return;
            }
            try {
                mResultLock.wait(timeoutMs);
            } catch (Exception e) {
            }
        }
    }

    private static void getPixels(Image image, int[] dest, Rect bounds) {
        Bitmap hwBitmap = Bitmap.wrapHardwareBuffer(image.getHardwareBuffer(), null);
        Bitmap swBitmap = hwBitmap.copy(Bitmap.Config.ARGB_8888, false);
        hwBitmap.recycle();
        swBitmap.getPixels(dest, 0, bounds.width(),
                bounds.left, bounds.top, bounds.width(), bounds.height());
        swBitmap.recycle();
    }

    public SurfacePixelValidator2(Context context, Point size, Rect boundsToCheck,
            PixelChecker pixelChecker) {
        mWidth = size.x;
        mHeight = size.y;

        mWorkerThread = new HandlerThread("SurfacePixelValidator");
        mWorkerThread.start();

        mPixelChecker = pixelChecker;
        mBoundsToCheck = new Rect(boundsToCheck);

        mImageReader = ImageReader.newInstance(mWidth, mHeight, HardwareBuffer.RGBA_8888, 1,
                HardwareBuffer.USAGE_GPU_COLOR_OUTPUT | HardwareBuffer.USAGE_CPU_READ_OFTEN
                        | HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE);
        mImageReader.setOnImageAvailableListener(mOnImageAvailable,
                new Handler(mWorkerThread.getLooper()));
    }

    public Surface getSurface() {
        return mImageReader.getSurface();
    }

    /**
     * Shuts down processing pipeline, and returns current pass/fail counts.
     *
     * Wait for pipeline to flush before calling this method. If not, frames that are still in
     * flight may be lost.
     */
    public void finish(CapturedActivity.TestResult testResult) {
        synchronized (mResultLock) {
            // could in theory miss results still processing, but only if latency is extremely high.
            // Caller should only call this
            testResult.failFrames = mResultFailureFrames;
            testResult.passFrames = mResultSuccessFrames;

            for (int i = 0; i < mFirstFailures.size(); i++) {
                testResult.failures.put(mFirstFailures.keyAt(i), mFirstFailures.valueAt(i));
            }
        }
        mImageReader.close();
        mWorkerThread.quitSafely();
    }
}
