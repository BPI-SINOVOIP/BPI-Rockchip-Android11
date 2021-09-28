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

package android.hardware.camera2.cts;

import static android.hardware.camera2.cts.CameraTestUtils.OFFLINE_CAMERA_ID;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.multiprocess.camera.cts.ErrorLoggingService;
import android.hardware.multiprocess.camera.cts.TestConstants;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

/**
 * Activity implementing basic access of the Camera2 API.
 *
 * <p />
 * This will log all errors to {@link android.hardware.multiprocess.camera.cts.ErrorLoggingService}.
 */
public class Camera2OfflineTestActivity extends Activity {
    private static final String TAG = "Camera2OfflineTestActivity";

    ErrorLoggingService.ErrorServiceConnection mErrorServiceConnection;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "onCreate called.");
        super.onCreate(savedInstanceState);
        mErrorServiceConnection = new ErrorLoggingService.ErrorServiceConnection(this);
        mErrorServiceConnection.start();
    }

    @Override
    protected void onPause() {
        Log.i(TAG, "onPause called.");
        super.onPause();
    }

    @Override
    protected void onResume() {
        Log.i(TAG, "onResume called.");
        super.onResume();

        try {
            CameraManager manager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);

            if (manager == null) {
                mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_ERROR, TAG +
                        " could not connect camera service");
                return;
            }
            Intent intent = getIntent();
            Bundle bundledExtras = intent.getExtras();
            if (null == bundledExtras) {
                mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_ERROR, TAG +
                        " not bundled intent extras");
                return;
            }

            String cameraId = bundledExtras.getString(OFFLINE_CAMERA_ID);
            if (null == cameraId) {
                mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_ERROR, TAG +
                        " no camera id present in bundled extra");
                return;
            }

            manager.registerAvailabilityCallback(new CameraManager.AvailabilityCallback() {
                @Override
                public void onCameraAvailable(String cameraId) {
                    super.onCameraAvailable(cameraId);
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_AVAILABLE,
                            cameraId);
                    Log.i(TAG, "Camera " + cameraId + " is available");
                }

                @Override
                public void onCameraUnavailable(String cameraId) {
                    super.onCameraUnavailable(cameraId);
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_UNAVAILABLE,
                            cameraId);
                    Log.i(TAG, "Camera " + cameraId + " is unavailable");
                }

                @Override
                public void onPhysicalCameraAvailable(String cameraId, String physicalCameraId) {
                    super.onPhysicalCameraAvailable(cameraId, physicalCameraId);
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_AVAILABLE,
                            cameraId + " : " + physicalCameraId);
                    Log.i(TAG, "Camera " + cameraId + " : " + physicalCameraId + " is available");
                }

                @Override
                public void onPhysicalCameraUnavailable(String cameraId, String physicalCameraId) {
                    super.onPhysicalCameraUnavailable(cameraId, physicalCameraId);
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_UNAVAILABLE,
                            cameraId + " : " + physicalCameraId);
                    Log.i(TAG, "Camera " + cameraId + " : " + physicalCameraId + " is unavailable");
                }
            }, null);

            manager.openCamera(cameraId, new CameraDevice.StateCallback() {
                @Override
                public void onOpened(CameraDevice cameraDevice) {
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_CONNECT,
                            cameraId);
                    Log.i(TAG, "Camera " + cameraId + " is opened");
                }

                @Override
                public void onDisconnected(CameraDevice cameraDevice) {
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_EVICTED,
                            cameraId);
                    Log.i(TAG, "Camera " + cameraId + " is disconnected");
                }

                @Override
                public void onError(CameraDevice cameraDevice, int i) {
                    mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_ERROR, TAG +
                            " Camera " + cameraId + " experienced error " + i);
                    Log.e(TAG, "Camera " + cameraId + " onError called with error " + i);
                }
            }, null);
        } catch (CameraAccessException e) {
            mErrorServiceConnection.logAsync(TestConstants.EVENT_CAMERA_ERROR, TAG +
                    " camera exception during connection: " + e);
            Log.e(TAG, "Access exception: " + e);
        }
    }

    @Override
    protected void onDestroy() {
        Log.i(TAG, "onDestroy called.");
        super.onDestroy();
        if (mErrorServiceConnection != null) {
            mErrorServiceConnection.stop();
            mErrorServiceConnection = null;
        }
    }
}
