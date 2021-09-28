/*
 * Copyright 2020 The Android Open Source Project
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

import static android.hardware.camera2.cts.CameraTestUtils.*;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.ex.camera2.blocking.BlockingSessionCallback;
import com.android.ex.camera2.blocking.BlockingOfflineSessionCallback;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.CameraOfflineSession;
import android.hardware.camera2.CameraOfflineSession.CameraOfflineSessionCallback;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.InputConfiguration;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.multiprocess.camera.cts.ErrorLoggingService;
import android.hardware.multiprocess.camera.cts.TestConstants;
import android.media.Image;
import android.media.ImageReader;
import android.os.Bundle;
import android.util.Log;
import android.util.Size;
import android.view.Surface;

import android.hardware.camera2.cts.testcases.Camera2SurfaceViewTestCase;

import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.Test;

import java.lang.IllegalArgumentException;
import java.util.concurrent.TimeoutException;
import java.util.ArrayList;
import java.util.List;

import junit.framework.AssertionFailedError;

@RunWith(Parameterized.class)
public class OfflineSessionTest extends Camera2SurfaceViewTestCase {
    private static final String TAG = "OfflineSessionTest";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);
    private final String REMOTE_PROCESS_NAME = "camera2ActivityProcess";
    private final java.lang.Class<?> REMOTE_PROCESS_CLASS = Camera2OfflineTestActivity.class;

    private static final Size MANDATORY_STREAM_BOUND = new Size(1920, 1080);
    private static final int WAIT_FOR_FRAMES_TIMEOUT_MS = 3000;
    private static final int WAIT_FOR_STATE_TIMEOUT_MS = 5000;
    private static final int WAIT_FOR_REMOTE_ACTIVITY_LAUNCH_MS = 2000;
    private static final int WAIT_FOR_REMOTE_ACTIVITY_DESTROY_MS = 2000;

    private enum OfflineTestSequence {
        /** Regular offline switch without extra calls */
        NoExtraSteps,

        /** Offline switch followed by immediate offline session close */
        CloseOfflineSession,

        /** Offline switch followed in parallel with immediate device close and same device open
         *  in a separate activity
         */
        CloseDeviceAndOpenRemote,

        /** Offline session running in parallel with reinitialized regular capture session */
        InitializeRegularSession,

        /** Trigger repeating sequence abort during offline switch */
        RepeatingSequenceAbort,
    }

    /**
     * Test offline switch behavior in case of invalid/bad input.
     *
     * <p> Verify that clients are not allowed to switch to offline mode
     * by passing invalid outputs. Invalid outputs can be either
     * surfaces not registered with camera or surfaces used in
     * repeating requests.</p>
     */
    @Test
    public void testInvalidOutput() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);

                CaptureRequest.Builder previewRequest =
                        mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                CaptureRequest.Builder stillCaptureRequest =
                        mCamera.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
                Size previewSize = mOrderedPreviewSizes.get(0);
                Size stillSize = mOrderedStillSizes.get(0);
                SimpleCaptureCallback resultListener = new SimpleCaptureCallback();
                SimpleImageReaderListener imageListener = new SimpleImageReaderListener();

                startPreview(previewRequest, previewSize, resultListener);

                CaptureResult result = resultListener.getCaptureResult(WAIT_FOR_FRAMES_TIMEOUT_MS);

                Long timestamp = result.get(CaptureResult.SENSOR_TIMESTAMP);
                assertNotNull("Can't read a capture result timestamp", timestamp);

                CaptureResult result2 = resultListener.getCaptureResult(WAIT_FOR_FRAMES_TIMEOUT_MS);

                Long timestamp2 = result2.get(CaptureResult.SENSOR_TIMESTAMP);
                assertNotNull("Can't read a capture result 2 timestamp", timestamp2);

                assertTrue("Bad timestamps", timestamp2 > timestamp);

                createImageReader(stillSize, ImageFormat.JPEG, MAX_READER_IMAGES, imageListener);

                BlockingOfflineSessionCallback offlineCb = new BlockingOfflineSessionCallback();

                try {
                    ArrayList<Surface> offlineSurfaces = new ArrayList<Surface>();
                    offlineSurfaces.add(mReaderSurface);
                    mSession.switchToOffline(offlineSurfaces, new HandlerExecutor(mHandler),
                            offlineCb);
                    fail("Offline session switch accepts unregistered output surface");
                } catch (IllegalArgumentException e) {
                    //Expected
                }

                if (mSession.supportsOfflineProcessing(mPreviewSurface)) {
                    ArrayList<Surface> offlineSurfaces = new ArrayList<Surface>();
                    offlineSurfaces.add(mPreviewSurface);
                    mSession.switchToOffline(offlineSurfaces, new HandlerExecutor(mHandler),
                            offlineCb);
                    // We only have a single repeating request, in this case the camera
                    // implementation should fail to find any capture requests that can
                    // be migrated to offline mode and notify the failure accordingly.
                    offlineCb.waitForState(BlockingOfflineSessionCallback.STATE_SWITCH_FAILED,
                            WAIT_FOR_STATE_TIMEOUT_MS);
                } else {
                    stopPreview();
                }

                closeImageReader();
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test camera callback sequence during and after offline session switch.
     *
     * <p>Camera clients must receive respective capture results or failures for all
     * non-offline outputs after the offline switch call returns.
     * In case the switch was successful clients must be notified about the
     * remaining offline requests via the registered offline callback.</p>
     */
    @Test
    public void testOfflineCallbacks() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);
                camera2OfflineSessionTest(mCameraIdsUnderTest[i], mOrderedStillSizes.get(0),
                        ImageFormat.JPEG, OfflineTestSequence.NoExtraSteps);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test camera offline session behavior in case of depth jpeg output.
     *
     * <p>Verify that offline session and callbacks behave as expected
     * in case the camera supports offline depth jpeg output.</p>
     */
    @Test
    public void testOfflineDepthJpeg() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isDepthJpegSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support depth jpeg, skipping");
                    continue;
                }

                List<Size> depthJpegSizes = CameraTestUtils.getSortedSizesForFormat(
                        mCameraIdsUnderTest[i], mCameraManager, ImageFormat.DEPTH_JPEG,
                        null /*bound*/);
                openDevice(mCameraIdsUnderTest[i]);
                camera2OfflineSessionTest(mCameraIdsUnderTest[i], depthJpegSizes.get(0),
                        ImageFormat.DEPTH_JPEG, OfflineTestSequence.NoExtraSteps);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test camera offline session behavior in case of HEIC output.
     *
     * <p>Verify that offline session and callbacks behave as expected
     * in case the camera supports offline HEIC output.</p>
     */
    @Test
    public void testOfflineHEIC() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isHeicSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support HEIC, skipping");
                    continue;
                }

                List<Size> heicSizes = CameraTestUtils.getSupportedHeicSizes(
                        mCameraIdsUnderTest[i], mCameraManager, null /*bound*/);
                openDevice(mCameraIdsUnderTest[i]);
                camera2OfflineSessionTest(mCameraIdsUnderTest[i], heicSizes.get(0),
                        ImageFormat.HEIC, OfflineTestSequence.NoExtraSteps);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test camera offline session behavior after close and reopen.
     *
     * <p> Verify that closing the initial camera device and opening the same
     * sensor during offline processing does not have any unexpected side effects.</p>
     */
    @Test
    public void testDeviceCloseAndOpen() throws Exception {
        ErrorLoggingService.ErrorServiceConnection errorConnection =
                new ErrorLoggingService.ErrorServiceConnection(mContext);

        errorConnection.start();
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);
                if (camera2OfflineSessionTest(mCameraIdsUnderTest[i], mOrderedStillSizes.get(0),
                            ImageFormat.JPEG, OfflineTestSequence.CloseDeviceAndOpenRemote)) {
                    // Verify that the remote camera was opened correctly
                    List<ErrorLoggingService.LogEvent> allEvents = null;
                    try {
                        allEvents = errorConnection.getLog(WAIT_FOR_STATE_TIMEOUT_MS,
                                TestConstants.EVENT_CAMERA_CONNECT);
                    } catch (TimeoutException e) {
                        fail("Timed out waiting on remote offline process error log!");
                    }
                    assertNotNull("Failed to connect to camera device in remote offline process!",
                            allEvents);
                }
            } finally {
                closeDevice();

            }
        }

        errorConnection.stop();
    }

    /**
     * Test camera offline session behavior during close.
     *
     * <p>Verify that clients are able to close an offline session and receive
     * all corresponding callbacks according to the documentation.</p>
     */
    @Test
    public void testOfflineSessionClose() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);
                camera2OfflineSessionTest(mCameraIdsUnderTest[i], mOrderedStillSizes.get(0),
                        ImageFormat.JPEG, OfflineTestSequence.CloseOfflineSession);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test camera offline session in case of new capture session
     *
     * <p>Verify that clients are able to initialize a new regular capture session
     * in parallel with the offline session.</p>
     */
    @Test
    public void testOfflineSessionWithRegularSession() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);
                camera2OfflineSessionTest(mCameraIdsUnderTest[i], mOrderedStillSizes.get(0),
                        ImageFormat.JPEG, OfflineTestSequence.InitializeRegularSession);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test for aborted repeating sequences when switching to offline mode
     *
     * <p>Verify that clients receive the expected sequence abort callbacks when switching
     * to offline session.</p>
     */
    @Test
    public void testRepeatingSequenceAbort() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);
                camera2OfflineSessionTest(mCameraIdsUnderTest[i], mOrderedStillSizes.get(0),
                        ImageFormat.JPEG, OfflineTestSequence.RepeatingSequenceAbort);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test that both shared and surface group outputs are not advertised as
     * capable working in offline mode.
     *
     * <p>Both shared and surface group outputs cannot be switched to offline mode.
     * Make sure that both cases are correctly advertised and switching to offline
     * mode is failing as expected.</p>
     */
    @Test
    public void testUnsupportedOfflineSessionOutputs() throws Exception {
        for (int i = 0; i < mCameraIdsUnderTest.length; i++) {
            try {
                Log.i(TAG, "Testing camera2 API for camera device " + mCameraIdsUnderTest[i]);

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support color outputs, skipping");
                    continue;
                }

                if (!mAllStaticInfo.get(mCameraIdsUnderTest[i]).isOfflineProcessingSupported()) {
                    Log.i(TAG, "Camera " + mCameraIdsUnderTest[i] +
                            " does not support offline processing, skipping");
                    continue;
                }

                openDevice(mCameraIdsUnderTest[i]);
                camera2UnsupportedOfflineOutputTest(true /*useSurfaceGroup*/);
                camera2UnsupportedOfflineOutputTest(false /*useSurfaceGroup*/);
            } finally {
                closeDevice();
            }
        }
    }

    private void checkForSequenceAbort(SimpleCaptureCallback resultListener, int sequenceId) {
        ArrayList<Integer> abortedSeq = resultListener.geAbortedSequences(
                1 /*maxNumbAborts*/);
        assertNotNull("No aborted capture sequence ids present", abortedSeq);
        assertTrue("Unexpected number of aborted capture sequence ids : " +
                abortedSeq.size() + " expected 1", abortedSeq.size() == 1);
        assertTrue("Unexpected abort capture sequence id: " +
                abortedSeq.get(0).intValue() + " expected capture sequence id: " +
                sequenceId, abortedSeq.get(0).intValue() == sequenceId);
    }

    private void verifyCaptureResults(SimpleCaptureCallback resultListener,
            SimpleImageReaderListener imageListener, int sequenceId, boolean offlineResults)
            throws Exception {
        long sequenceLastFrameNumber = resultListener.getCaptureSequenceLastFrameNumber(
                sequenceId, 0 /*timeoutMs*/);

        long lastFrameNumberReceived = -1;
        while (resultListener.hasMoreResults()) {
            TotalCaptureResult result = resultListener.getTotalCaptureResult(0 /*timeout*/);
            if (lastFrameNumberReceived < result.getFrameNumber()) {
                lastFrameNumberReceived = result.getFrameNumber();
            }

            if (imageListener != null) {
                long resultTimestamp = result.get(CaptureResult.SENSOR_TIMESTAMP);
                Image offlineImage = imageListener.getImage(CAPTURE_IMAGE_TIMEOUT_MS);
                assertEquals("Offline image timestamp: " + offlineImage.getTimestamp() +
                        " doesn't match with the result timestamp: " + resultTimestamp,
                        offlineImage.getTimestamp(), resultTimestamp);
            }
        }

        while (resultListener.hasMoreFailures()) {
            ArrayList<CaptureFailure> failures = resultListener.getCaptureFailures(
                    /*maxNumFailures*/ 1);
            for (CaptureFailure failure : failures) {
                if (lastFrameNumberReceived < failure.getFrameNumber()) {
                    lastFrameNumberReceived = failure.getFrameNumber();
                }
            }
        }

        String assertString = offlineResults ?
                "Last offline frame number from " +
                "onCaptureSequenceCompleted (%d) doesn't match the last frame number " +
                "received from results/failures (%d)" :
                "Last frame number from onCaptureSequenceCompleted " +
                "(%d) doesn't match the last frame number received from " +
                "results/failures (%d)";
        assertEquals(String.format(assertString, sequenceLastFrameNumber, lastFrameNumberReceived),
                sequenceLastFrameNumber, lastFrameNumberReceived);
    }

    /**
     * Verify offline session behavior during common use cases
     *
     * @param cameraId      Id of the camera device under test
     * @param offlineSize   The offline surface size
     * @param offlineFormat The offline surface pixel format
     * @param testSequence  Specific scenario to be verified
     * @return true if the offline session switch is successful, false if there is any failure.
     */
    private boolean camera2OfflineSessionTest(String cameraId, Size offlineSize, int offlineFormat,
            OfflineTestSequence testSequence) throws Exception {
        boolean ret = false;
        int remoteOfflinePID = -1;
        Size previewSize = mOrderedPreviewSizes.get(0);
        for (Size sz : mOrderedPreviewSizes) {
            if (sz.getWidth() <= MANDATORY_STREAM_BOUND.getWidth() && sz.getHeight() <=
                    MANDATORY_STREAM_BOUND.getHeight()) {
                previewSize = sz;
                break;
            }
        }
        Size privateSize = previewSize;
        if (mAllStaticInfo.get(cameraId).isPrivateReprocessingSupported()) {
            privateSize = mAllStaticInfo.get(cameraId).getSortedSizesForInputFormat(
                    ImageFormat.PRIVATE, MANDATORY_STREAM_BOUND).get(0);
        }

        CaptureRequest.Builder previewRequest =
                mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
        CaptureRequest.Builder stillCaptureRequest =
                mCamera.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
        SimpleCaptureCallback resultListener = new SimpleCaptureCallback();
        SimpleCaptureCallback regularResultListener = new SimpleCaptureCallback();
        SimpleCaptureCallback offlineResultListener = new SimpleCaptureCallback();
        SimpleImageReaderListener imageListener = new SimpleImageReaderListener();
        ImageReader privateReader = null;
        ImageReader yuvCallbackReader = null;
        ImageReader jpegReader = null;
        int repeatingSeqId = -1;

        // Update preview size.
        updatePreviewSurface(previewSize);

        // Create ImageReader.
        createImageReader(offlineSize, offlineFormat, MAX_READER_IMAGES, imageListener);

        // Configure output streams with preview and offline streams.
        ArrayList<Surface> outputSurfaces = new ArrayList<Surface>();
        outputSurfaces.add(mPreviewSurface);
        outputSurfaces.add(mReaderSurface);
        final CameraCaptureSession.StateCallback sessionCb = mock(
                CameraCaptureSession.StateCallback.class);
        mSessionListener = new BlockingSessionCallback(sessionCb);
        mSession = configureCameraSession(mCamera, outputSurfaces, mSessionListener, mHandler);

        if (!mSession.supportsOfflineProcessing(mReaderSurface)) {
            Log.i(TAG, "Camera does not support offline processing for still capture output");
            return false;
        }

        // Configure the requests.
        previewRequest.addTarget(mPreviewSurface);
        stillCaptureRequest.addTarget(mReaderSurface);


        ArrayList<Integer> allowedOfflineStates = new ArrayList<Integer>();
        allowedOfflineStates.add(BlockingOfflineSessionCallback.STATE_READY);
        allowedOfflineStates.add(BlockingOfflineSessionCallback.STATE_SWITCH_FAILED);
        ArrayList<Surface> offlineSurfaces = new ArrayList<Surface>();
        offlineSurfaces.add(mReaderSurface);
        final CameraOfflineSessionCallback mockOfflineCb = mock(CameraOfflineSessionCallback.class);
        BlockingOfflineSessionCallback offlineCb = new BlockingOfflineSessionCallback(
                mockOfflineCb);
        ArrayList<CaptureRequest> offlineRequestList = new ArrayList<CaptureRequest>();
        for (int i = 0; i < MAX_READER_IMAGES; i++) {
            offlineRequestList.add(stillCaptureRequest.build());
        }

        if (testSequence != OfflineTestSequence.RepeatingSequenceAbort) {
            repeatingSeqId = mSession.setRepeatingRequest(previewRequest.build(), resultListener,
                    mHandler);
            checkInitialResults(resultListener);
        }

        int offlineSeqId = mSession.captureBurst(offlineRequestList, offlineResultListener,
                mHandler);

        if (testSequence == OfflineTestSequence.RepeatingSequenceAbort) {
            // Submit the preview repeating request after the offline burst so it can be delayed
            // long enough and fail to reach the camera processing pipeline.
            repeatingSeqId = mSession.setRepeatingRequest(previewRequest.build(), resultListener,
                    mHandler);
        }

        CameraOfflineSession offlineSession = mSession.switchToOffline(offlineSurfaces,
                new HandlerExecutor(mHandler), offlineCb);
        assertNotNull("Invalid offline session", offlineSession);

        // The regular capture session must be closed as well
        verify(sessionCb, times(1)).onClosed(mSession);

        int offlineState = offlineCb.waitForAnyOfStates(allowedOfflineStates,
                WAIT_FOR_STATE_TIMEOUT_MS);
        if (offlineState == BlockingOfflineSessionCallback.STATE_SWITCH_FAILED) {
            // A failure during offline mode switch is only allowed in case the switch gets
            // triggered too late without pending offline requests.
            verify(mockOfflineCb, times(1)).onSwitchFailed(offlineSession);
            verify(mockOfflineCb, times(0)).onReady(offlineSession);
            verify(mockOfflineCb, times(0)).onIdle(offlineSession);
            verify(mockOfflineCb, times(0)).onError(offlineSession,
                    CameraOfflineSessionCallback.STATUS_INTERNAL_ERROR);

            try {
                verifyCaptureResults(resultListener, null /*imageListener*/, repeatingSeqId,
                        false /*offlineResults*/);
            } catch (AssertionFailedError e) {
                if (testSequence == OfflineTestSequence.RepeatingSequenceAbort) {
                    checkForSequenceAbort(resultListener, repeatingSeqId);
                } else {
                    throw e;
                }
            }
            verifyCaptureResults(offlineResultListener, null /*imageListener*/, offlineSeqId,
                    true /*offlineResults*/);
        } else {
            verify(mockOfflineCb, times(1)).onReady(offlineSession);
            verify(mockOfflineCb, times(0)).onSwitchFailed(offlineSession);

            switch (testSequence) {
                case RepeatingSequenceAbort:
                    checkForSequenceAbort(resultListener, repeatingSeqId);
                    break;
                case CloseDeviceAndOpenRemote:
                    // According to the documentation, closing the initial camera device and
                    // re-opening the same device from a different client after successful
                    // offline session switch must not have any noticeable impact on the
                    // offline processing.
                    closeDevice();
                    remoteOfflinePID = startRemoteOfflineTestProcess(cameraId);

                    break;
                case CloseOfflineSession:
                    offlineSession.close();

                    break;
                case InitializeRegularSession:
                    // According to the documentation, initializing a regular capture session
                    // along with the offline session should not have any side effects.
                    // We also don't want to re-use the same offline output surface as part
                    // of the new  regular capture session.
                    outputSurfaces.remove(mReaderSurface);

                    // According to the specification, an active offline session must allow
                    // camera devices to support at least one preview stream, one yuv stream
                    // of size up-to 1080p, one jpeg stream with any supported size and
                    // an extra input/output private pair in case reprocessing is also available.
                    yuvCallbackReader = makeImageReader(previewSize, ImageFormat.YUV_420_888,
                            1 /*maxNumImages*/, new SimpleImageReaderListener(), mHandler);
                    outputSurfaces.add(yuvCallbackReader.getSurface());

                    jpegReader = makeImageReader(offlineSize, ImageFormat.JPEG,
                            1 /*maxNumImages*/, new SimpleImageReaderListener(), mHandler);
                    outputSurfaces.add(jpegReader.getSurface());

                    if (mAllStaticInfo.get(cameraId).isPrivateReprocessingSupported()) {
                        privateReader = makeImageReader(privateSize, ImageFormat.PRIVATE,
                                1 /*maxNumImages*/, new SimpleImageReaderListener(), mHandler);
                        outputSurfaces.add(privateReader.getSurface());

                        InputConfiguration inputConfig = new InputConfiguration(
                                privateSize.getWidth(), privateSize.getHeight(),
                                ImageFormat.PRIVATE);
                        mSession = CameraTestUtils.configureReprocessableCameraSession(mCamera,
                                inputConfig, outputSurfaces, mSessionListener, mHandler);

                    } else {
                        mSession = configureCameraSession(mCamera, outputSurfaces, mSessionListener,
                                mHandler);
                    }

                    mSession.setRepeatingRequest(previewRequest.build(), regularResultListener,
                            mHandler);

                    break;
                case NoExtraSteps:
                default:
            }

            if (testSequence != OfflineTestSequence.RepeatingSequenceAbort) {
                // The repeating non-offline request should be done after the switch returns.
                verifyCaptureResults(resultListener, null /*imageListener*/, repeatingSeqId,
                        false /*offlineResults*/);
            }

            if (testSequence != OfflineTestSequence.CloseOfflineSession) {
                offlineCb.waitForState(BlockingOfflineSessionCallback.STATE_IDLE,
                        WAIT_FOR_STATE_TIMEOUT_MS);
                verify(mockOfflineCb, times(1)).onIdle(offlineSession);
                verify(mockOfflineCb, times(0)).onError(offlineSession,
                        CameraOfflineSessionCallback.STATUS_INTERNAL_ERROR);

                // The offline requests should be done after we reach idle state.
                verifyCaptureResults(offlineResultListener, imageListener, offlineSeqId,
                        true /*offlineResults*/);

                offlineSession.close();
            }

            if (testSequence == OfflineTestSequence.InitializeRegularSession) {
                checkInitialResults(regularResultListener);
                stopPreview();

                if (privateReader != null) {
                    privateReader.close();
                }

                if (yuvCallbackReader != null) {
                    yuvCallbackReader.close();
                }

                if (jpegReader != null) {
                    jpegReader.close();
                }
            }

            offlineCb.waitForState(BlockingOfflineSessionCallback.STATE_CLOSED,
                  WAIT_FOR_STATE_TIMEOUT_MS);
            verify(mockOfflineCb, times(1)).onClosed(offlineSession);

            ret = true;
        }

        closeImageReader();

        stopRemoteOfflineTestProcess(remoteOfflinePID);

        return ret;
    }

    private void checkInitialResults(SimpleCaptureCallback resultListener) {
        CaptureResult result = resultListener.getCaptureResult(WAIT_FOR_FRAMES_TIMEOUT_MS);

        Long timestamp = result.get(CaptureResult.SENSOR_TIMESTAMP);
        assertNotNull("Can't read a capture result timestamp", timestamp);

        CaptureResult result2 = resultListener.getCaptureResult(WAIT_FOR_FRAMES_TIMEOUT_MS);

        Long timestamp2 = result2.get(CaptureResult.SENSOR_TIMESTAMP);
        assertNotNull("Can't read a capture result 2 timestamp", timestamp2);

        assertTrue("Bad timestamps", timestamp2 > timestamp);
    }

    private void camera2UnsupportedOfflineOutputTest(boolean useSurfaceGroup) throws Exception {
        CaptureRequest.Builder previewRequest =
                mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
        Size previewSize = mOrderedPreviewSizes.get(0);
        SimpleCaptureCallback resultListener = new SimpleCaptureCallback();
        updatePreviewSurface(previewSize);

        OutputConfiguration outConfig;
        if (useSurfaceGroup) {
            outConfig = new OutputConfiguration(1 /*surfaceGroupId*/, mPreviewSurface);
        } else {
            outConfig = new OutputConfiguration(mPreviewSurface);
            outConfig.enableSurfaceSharing();
        }

        ArrayList<OutputConfiguration> outputList = new ArrayList<OutputConfiguration>();
        outputList.add(outConfig);
        BlockingSessionCallback sessionListener = new BlockingSessionCallback();
        mCamera.createCaptureSessionByOutputConfigurations(outputList, sessionListener, mHandler);
        CameraCaptureSession session = sessionListener.waitAndGetSession(
                SESSION_CONFIGURE_TIMEOUT_MS);

        assertFalse(useSurfaceGroup ? "Group surface outputs cannot support offline mode" :
                "Shared surface outputs cannot support offline mode",
                session.supportsOfflineProcessing(mPreviewSurface));

        ArrayList<CaptureRequest> offlineRequestList = new ArrayList<CaptureRequest>();
        previewRequest.addTarget(mPreviewSurface);
        for (int i = 0; i < MAX_READER_IMAGES; i++) {
            offlineRequestList.add(previewRequest.build());
        }

        final CameraOfflineSessionCallback offlineCb = mock(CameraOfflineSessionCallback.class);
        ArrayList<Surface> offlineSurfaces = new ArrayList<Surface>();
        offlineSurfaces.add(mPreviewSurface);
        session.captureBurst(offlineRequestList, resultListener, mHandler);
        try {
            session.switchToOffline(offlineSurfaces, new HandlerExecutor(mHandler), offlineCb);
            fail(useSurfaceGroup ? "Group surface outputs cannot be switched to offline mode" :
                "Shared surface outputs cannot be switched to offline mode");
        } catch (IllegalArgumentException e) {
            // Expected
        }

        session.close();
    }

    private int startRemoteOfflineTestProcess(String cameraId) throws InterruptedException {
        // Ensure no running activity process with same name
        String cameraActivityName = mContext.getPackageName() + ":" + REMOTE_PROCESS_NAME;
        ActivityManager activityManager = (ActivityManager) mContext.getSystemService(
                Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> list = activityManager.getRunningAppProcesses();
        for (ActivityManager.RunningAppProcessInfo rai : list) {
            if (cameraActivityName.equals(rai.processName)) {
                fail("Remote offline session test activity already running");
                return -1;
            }
        }

        Activity activity = mActivityRule.getActivity();
        Intent activityIntent = new Intent(activity, REMOTE_PROCESS_CLASS);
        Bundle b = new Bundle();
        b.putString(CameraTestUtils.OFFLINE_CAMERA_ID, cameraId);
        activityIntent.putExtras(b);
        activity.startActivity(activityIntent);
        Thread.sleep(WAIT_FOR_REMOTE_ACTIVITY_LAUNCH_MS);

        // Fail if activity isn't running
        list = activityManager.getRunningAppProcesses();
        for (ActivityManager.RunningAppProcessInfo rai : list) {
            if (cameraActivityName.equals(rai.processName))
                return rai.pid;
        }

        fail("Remote offline session test activity failed to start");

        return -1;
    }

    private void stopRemoteOfflineTestProcess(int remotePID) throws InterruptedException {
        if (remotePID < 0) {
            return;
        }

        android.os.Process.killProcess(remotePID);
        Thread.sleep(WAIT_FOR_REMOTE_ACTIVITY_DESTROY_MS);

        ActivityManager activityManager = (ActivityManager) mContext.getSystemService(
                Context.ACTIVITY_SERVICE);
        String cameraActivityName = mContext.getPackageName() + ":" + REMOTE_PROCESS_NAME;
        List<ActivityManager.RunningAppProcessInfo> list = activityManager.getRunningAppProcesses();
        for (ActivityManager.RunningAppProcessInfo rai : list) {
            if (cameraActivityName.equals(rai.processName))
                fail("Remote offline session test activity is still running");
        }

    }
}
