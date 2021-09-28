/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.hardware.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.hardware.Camera;
import android.hardware.Camera.AutoFocusCallback;
import android.hardware.Camera.ErrorCallback;
import android.hardware.Camera.PictureCallback;
import android.hardware.Camera.PreviewCallback;
import android.os.Looper;
import android.util.Log;

import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class CameraPerformanceTestHelper {
    private static final String TAG = "CameraTestCase";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    public static final int NO_ERROR = -1;
    public static final long WAIT_FOR_COMMAND_TO_COMPLETE_NS = 5000000000L;
    public static final long WAIT_FOR_FOCUS_TO_COMPLETE_NS = 5000000000L;
    public static final long WAIT_FOR_SNAPSHOT_TO_COMPLETE_NS = 5000000000L;

    private Looper mLooper = null;
    private int mCameraErrorCode;
    private Camera mCamera;

    /**
     * Initializes the message looper so that the Camera object can
     * receive the callback messages.
     */
    public void initializeMessageLooper(final int cameraId) throws InterruptedException {
        Lock startLock = new ReentrantLock();
        Condition startDone = startLock.newCondition();
        mCameraErrorCode = NO_ERROR;
        new Thread() {
            @Override
            public void run() {
                // Set up a looper to be used by camera.
                Looper.prepare();
                // Save the looper so that we can terminate this thread
                // after we are done with it.
                mLooper = Looper.myLooper();
                try {
                    mCamera = Camera.open(cameraId);
                    mCamera.setErrorCallback(new ErrorCallback() {
                        @Override
                        public void onError(int error, Camera camera) {
                            mCameraErrorCode = error;
                        }
                    });
                } catch (RuntimeException e) {
                    Log.e(TAG, "Fail to open camera." + e);
                }
                startLock.lock();
                startDone.signal();
                startLock.unlock();
                Looper.loop(); // Blocks forever until Looper.quit() is called.
                if (VERBOSE) Log.v(TAG, "initializeMessageLooper: quit.");
            }
        }.start();

        startLock.lock();
        try {
            assertTrue(
                    "initializeMessageLooper: start timeout",
                    startDone.awaitNanos(WAIT_FOR_COMMAND_TO_COMPLETE_NS) > 0L);
        } finally {
            startLock.unlock();
        }

        assertNotNull("Fail to open camera.", mCamera);
    }

    /**
     * Terminates the message looper thread, optionally allowing evict error
     */
    public void terminateMessageLooper() throws Exception {
        mLooper.quit();
        // Looper.quit() is asynchronous. The looper may still has some
        // preview callbacks in the queue after quit is called. The preview
        // callback still uses the camera object (setHasPreviewCallback).
        // After camera is released, RuntimeException will be thrown from
        // the method. So we need to join the looper thread here.
        mLooper.getThread().join();
        mCamera.release();
        mCamera = null;
        assertEquals("Got camera error callback.", NO_ERROR, mCameraErrorCode);
    }

    /**
     * Start preview and wait for the first preview callback, which indicates the
     * preview becomes active.
     */
    public void startPreview() throws InterruptedException {
        Lock previewLock = new ReentrantLock();
        Condition previewDone = previewLock.newCondition();

        mCamera.setPreviewCallback(new PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, android.hardware.Camera camera) {
                previewLock.lock();
                previewDone.signal();
                previewLock.unlock();
            }
        });
        mCamera.startPreview();

        previewLock.lock();
        try {
            assertTrue(
                    "Preview done timeout",
                    previewDone.awaitNanos(WAIT_FOR_COMMAND_TO_COMPLETE_NS) > 0L);
        } finally {
            previewLock.unlock();
        }

        mCamera.setPreviewCallback(null);
    }

    /**
     * Trigger and wait for autofocus to complete.
     */
    public void autoFocus() throws InterruptedException {
        Lock focusLock = new ReentrantLock();
        Condition focusDone = focusLock.newCondition();

        mCamera.autoFocus(new AutoFocusCallback() {
            @Override
            public void onAutoFocus(boolean success, Camera camera) {
                focusLock.lock();
                focusDone.signal();
                focusLock.unlock();
            }
        });

        focusLock.lock();
        try {
            assertTrue(
                    "Autofocus timeout",
                    focusDone.awaitNanos(WAIT_FOR_FOCUS_TO_COMPLETE_NS) > 0L);
        } finally {
            focusLock.unlock();
        }
    }

    /**
     * Trigger and wait for snapshot to finish.
     */
    public void takePicture() throws InterruptedException {
        Lock snapshotLock = new ReentrantLock();
        Condition snapshotDone = snapshotLock.newCondition();

        mCamera.takePicture(/*shutterCallback*/ null, /*rawPictureCallback*/ null,
                new PictureCallback() {
                    @Override
                    public void onPictureTaken(byte[] rawData, Camera camera) {
                        snapshotLock.lock();
                        try {
                            assertNotNull("Empty jpeg data", rawData);
                            snapshotDone.signal();
                        } finally {
                            snapshotLock.unlock();
                        }
                    }
                });

        snapshotLock.lock();
        try {
            assertTrue(
                    "TakePicture timeout",
                    snapshotDone.awaitNanos(WAIT_FOR_SNAPSHOT_TO_COMPLETE_NS) > 0L);
        } finally {
            snapshotLock.unlock();
        }
    }

    public Camera getCamera() {
        return mCamera;
    }
}