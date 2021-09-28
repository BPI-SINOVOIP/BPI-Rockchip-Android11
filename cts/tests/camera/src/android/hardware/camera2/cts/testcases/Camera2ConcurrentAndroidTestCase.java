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

package android.hardware.camera2.cts.testcases;

import static android.hardware.camera2.cts.CameraTestUtils.*;
import static com.android.ex.camera2.blocking.BlockingStateCallback.*;

import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCaptureSession.CaptureCallback;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.MandatoryStreamCombination;
import android.hardware.camera2.params.MandatoryStreamCombination.MandatoryStreamInformation;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.cts.Camera2ParameterizedTestCase;
import android.hardware.camera2.cts.CameraTestUtils;
import android.hardware.camera2.cts.helpers.CameraErrorCollector;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.helpers.StaticMetadata.CheckLevel;
import android.os.Handler;
import android.os.HandlerThread;
import android.test.AndroidTestCase;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import com.android.ex.camera2.blocking.BlockingSessionCallback;
import com.android.ex.camera2.blocking.BlockingStateCallback;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

public class Camera2ConcurrentAndroidTestCase extends Camera2ParameterizedTestCase {
    private static final String TAG = "Camera2ConcurrentAndroidTestCase";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    public static class CameraTestInfo {
        public String mCameraId;
        public CameraDevice mCamera;
        public StaticMetadata mStaticInfo;
        public MandatoryStreamCombination[] mMandatoryStreamCombinations;
        public CameraCaptureSession mCameraSession;
        public BlockingSessionCallback mCameraSessionListener;
        public BlockingStateCallback mCameraListener;
        public CameraTestInfo(String cameraId, StaticMetadata staticInfo,
                MandatoryStreamCombination[] mandatoryStreamCombinations,
                BlockingStateCallback cameraListener) {
            mCameraId = cameraId;
            mStaticInfo = staticInfo;
            mMandatoryStreamCombinations = mandatoryStreamCombinations;
            mCameraListener = cameraListener;
        }
    };
    protected Set<Set<String>> mConcurrentCameraIdCombinations;
    protected HashMap<String, CameraTestInfo> mCameraTestInfos;
    // include both standalone camera IDs and "hidden" physical camera IDs
    protected String[] mAllCameraIds;
    protected HashMap<String, StaticMetadata> mAllStaticInfo;
    protected Handler mHandler;
    protected HandlerThread mHandlerThread;
    protected CameraErrorCollector mCollector;
    protected String mDebugFileNameBase;

    protected WindowManager mWindowManager;

    /**
     * Set up the camera2 test case required environments, including CameraManager,
     * HandlerThread, Camera IDs, and CameraStateCallback etc.
     */
    @Override
    public void setUp() throws Exception {
        super.setUp();
        mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
        mCollector = new CameraErrorCollector();

        File filesDir = mContext.getPackageManager().isInstantApp()
                ? mContext.getFilesDir()
                : mContext.getExternalFilesDir(null);

        mDebugFileNameBase = filesDir.getPath();
        mAllStaticInfo = new HashMap<String, StaticMetadata>();
        List<String> hiddenPhysicalIds = new ArrayList<>();
        for (String cameraId : mCameraIdsUnderTest) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(cameraId);
            StaticMetadata staticMetadata = new StaticMetadata(props,
                    CheckLevel.ASSERT, /*collector*/null);
            mAllStaticInfo.put(cameraId, staticMetadata);
            for (String physicalId : props.getPhysicalCameraIds()) {
                if (!Arrays.asList(mCameraIdsUnderTest).contains(physicalId) &&
                        !hiddenPhysicalIds.contains(physicalId)) {
                    hiddenPhysicalIds.add(physicalId);
                    props = mCameraManager.getCameraCharacteristics(physicalId);
                    staticMetadata = new StaticMetadata(
                            mCameraManager.getCameraCharacteristics(physicalId),
                            CheckLevel.ASSERT, /*collector*/null);
                    mAllStaticInfo.put(physicalId, staticMetadata);
                }
            }
        }
        mConcurrentCameraIdCombinations =
                CameraTestUtils.getConcurrentCameraIds(mCameraManager, mAdoptShellPerm);
        assertNotNull("Unable to get concurrent camera combinations",
                mConcurrentCameraIdCombinations);
        mCameraTestInfos = new HashMap<String, CameraTestInfo>();
        for (Set<String> cameraIdComb : mConcurrentCameraIdCombinations) {
            for (String cameraId : cameraIdComb) {
                if (!mCameraTestInfos.containsKey(cameraId)) {
                    StaticMetadata staticMetadata = mAllStaticInfo.get(cameraId);
                    assertTrue("camera id" + cameraId + "'s metadata not found in mAllStaticInfo",
                            staticMetadata != null);
                    CameraCharacteristics.Key<MandatoryStreamCombination[]> mandatoryStreamsKey =
                            CameraCharacteristics.SCALER_MANDATORY_CONCURRENT_STREAM_COMBINATIONS;
                    MandatoryStreamCombination[] combinations =
                            staticMetadata.getCharacteristics().get(mandatoryStreamsKey);
                    assertTrue("Concurrent streaming camera id " + cameraId +
                            "  MUST have mandatory stream combinations",
                            (combinations != null) && (combinations.length > 0));
                    mCameraTestInfos.put(cameraId,
                            new CameraTestInfo(cameraId, staticMetadata, combinations,
                                  new BlockingStateCallback()));
                }
            }
        }

        mAllCameraIds = new String[mCameraIdsUnderTest.length + hiddenPhysicalIds.size()];
        System.arraycopy(mCameraIdsUnderTest, 0, mAllCameraIds, 0, mCameraIdsUnderTest.length);
        for (int i = 0; i < hiddenPhysicalIds.size(); i++) {
            mAllCameraIds[mCameraIdsUnderTest.length + i] = hiddenPhysicalIds.get(i);
        }
    }

    @Override
    public void tearDown() throws Exception {
        try {
            if (mHandlerThread != null) {
                mHandlerThread.quitSafely();
            }
            mHandler = null;

            if (mCollector != null) {
                mCollector.verify();
            }
        } catch (Throwable e) {
            // When new Exception(e) is used, exception info will be printed twice.
            throw new Exception(e.getMessage());
        } finally {
            super.tearDown();
        }
    }

    /**
     * Start capture with given {@link #CaptureRequest}.
     *
     * @param request The {@link #CaptureRequest} to be captured.
     * @param repeating If the capture is single capture or repeating.
     * @param listener The {@link #CaptureCallback} camera device used to notify callbacks.
     * @param handler The handler camera device used to post callbacks.
     */
    protected void startCapture(String cameraId, CaptureRequest request, boolean repeating,
            CaptureCallback listener, Handler handler) throws Exception {
        if (VERBOSE) Log.v(TAG, "Starting capture from device");
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTestInfo not found for camera id " + cameraId, info != null);
        if (repeating) {
            info.mCameraSession.setRepeatingRequest(request, listener, handler);
        } else {
            info.mCameraSession.capture(request, listener, handler);
        }
    }

    /**
     * Stop the current active capture.
     *
     * @param fast When it is true, {@link CameraDevice#flush} is called, the stop capture
     * could be faster.
     */
    protected void stopCapture(String cameraId, boolean fast) throws Exception {
        if (VERBOSE) Log.v(TAG, "Stopping capture");

        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTest info not found for camera id " + cameraId, info != null);
        if (fast) {
            /**
             * Flush is useful for canceling long exposure single capture, it also could help
             * to make the streaming capture stop sooner.
             */
            info.mCameraSession.abortCaptures();
            info.mCameraSessionListener.getStateWaiter().
                    waitForState(BlockingSessionCallback.SESSION_READY, CAMERA_IDLE_TIMEOUT_MS);
        } else {
            info.mCameraSession.close();
            info.mCameraSessionListener.getStateWaiter().
                    waitForState(BlockingSessionCallback.SESSION_CLOSED, CAMERA_IDLE_TIMEOUT_MS);
        }
    }

    /**
     * Open a {@link #CameraDevice camera device} and get the StaticMetadata for a given camera id.
     * The default mCameraListener is used to wait for states.
     *
     * @param cameraId The id of the camera device to be opened.
     */
    protected void openDevice(String cameraId) throws Exception {
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTest info not found for camera id " + cameraId, info != null);
        openDevice(cameraId, info.mCameraListener);
    }

    /**
     * Open a {@link #CameraDevice} and get the StaticMetadata for a given camera id and listener.
     *
     * @param cameraId The id of the camera device to be opened.
     * @param listener The {@link #BlockingStateCallback} used to wait for states.
     */
    protected void openDevice(String cameraId, BlockingStateCallback listener) throws Exception {
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTest info not found for camera id " + cameraId, info != null);

        info.mCamera = CameraTestUtils.openCamera(
                mCameraManager, cameraId, listener, mHandler);
        mCollector.setCameraId(cameraId);
        if (VERBOSE) {
            Log.v(TAG, "Camera " + cameraId + " is opened");
        }
    }

    /**
     * Create a {@link #CameraCaptureSession} using the currently open camera with
     * OutputConfigurations.
     *
     * @param outputSurfaces The set of output surfaces to configure for this session
     */
    protected void createSessionByConfigs(String cameraId,
            List<OutputConfiguration> outputConfigs) throws Exception {
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTest info not found for camera id " + cameraId, info != null);

        info.mCameraSessionListener = new BlockingSessionCallback();
        info.mCameraSession = CameraTestUtils.configureCameraSessionWithConfig(info.mCamera,
                outputConfigs, info.mCameraSessionListener, mHandler);
    }

    /**
     * Close a {@link #CameraDevice camera device} and clear the associated StaticInfo field for a
     * given camera id. The default mCameraListener is used to wait for states.
     * <p>
     * This function must be used along with the {@link #openDevice} for the
     * same camera id.
     * </p>
     *
     * @param cameraId The id of the {@link #CameraDevice camera device} to be closed.
     */
    protected void closeDevice(String cameraId) {
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTest info not found for camera id " + cameraId, info != null);
        closeDevice(cameraId, info.mCameraListener);
    }

    /**
     * Close a {@link #CameraDevice camera device} and clear the associated StaticInfo field for a
     * given camera id and listener.
     * <p>
     * This function must be used along with the {@link #openDevice} for the
     * same camera id.
     * </p>
     *
     * @param cameraId The id of the camera device to be closed.
     * @param listener The BlockingStateCallback used to wait for states.
     */
    protected void closeDevice(String cameraId, BlockingStateCallback listener) {
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTest info not found for camera id " + cameraId, info != null);

        if (info.mCamera != null) {
            info.mCamera.close();
            listener.waitForState(STATE_CLOSED, CAMERA_CLOSE_TIMEOUT_MS);
            info.mCamera = null;
            info.mCameraSession = null;
            info.mCameraSessionListener = null;
            if (VERBOSE) {
                Log.v(TAG, "Camera " + cameraId + " is closed");
            }
        }
    }

}
