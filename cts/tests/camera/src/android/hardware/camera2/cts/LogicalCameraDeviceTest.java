/*
 * Copyright 2018 The Android Open Source Project
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

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.ImageFormat;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.cts.CaptureResultTest;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.helpers.StaticMetadata.CheckLevel;
import android.hardware.camera2.cts.testcases.Camera2SurfaceViewTestCase;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.CamcorderProfile;
import android.media.Image;
import android.media.ImageReader;
import android.os.BatteryManager;
import android.util.ArraySet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Pair;
import android.util.Range;
import android.util.Size;
import android.util.SizeF;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.Stat;
import com.android.ex.camera2.blocking.BlockingSessionCallback;
import com.android.ex.camera2.utils.StateWaiter;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.junit.runners.Parameterized;
import org.junit.runner.RunWith;
import org.junit.Test;

import static org.mockito.Mockito.*;

/**
 * Tests exercising logical camera setup, configuration, and usage.
 */
@RunWith(Parameterized.class)
public final class LogicalCameraDeviceTest extends Camera2SurfaceViewTestCase {
    private static final String TAG = "LogicalCameraDeviceTest";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    private static final int CONFIGURE_TIMEOUT = 5000; //ms

    private static final double NS_PER_MS = 1000000.0;
    private static final int MAX_IMAGE_COUNT = 3;
    private static final int NUM_FRAMES_CHECKED = 300;

    private static final double FRAME_DURATION_THRESHOLD = 0.03;
    private static final double FOV_THRESHOLD = 0.03;
    private static final long MAX_TIMESTAMP_DIFFERENCE_THRESHOLD = 10000000; // 10ms

    private StateWaiter mSessionWaiter;

    private final int[] sTemplates = new int[] {
        CameraDevice.TEMPLATE_PREVIEW,
        CameraDevice.TEMPLATE_RECORD,
        CameraDevice.TEMPLATE_STILL_CAPTURE,
        CameraDevice.TEMPLATE_VIDEO_SNAPSHOT,
        CameraDevice.TEMPLATE_ZERO_SHUTTER_LAG,
        CameraDevice.TEMPLATE_MANUAL,
    };

    @Override
    public void setUp() throws Exception {
        super.setUp();
    }

    /**
     * Test that passing in invalid physical camera ids in OutputConfiguragtion behaves as expected
     * for logical multi-camera and non-logical multi-camera.
     */
    @Test
    public void testInvalidPhysicalCameraIdInOutputConfiguration() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);
                if (mAllStaticInfo.get(id).isHardwareLevelLegacy()) {
                    Log.i(TAG, "Camera " + id + " is legacy, skipping");
                    continue;
                }
                if (!mAllStaticInfo.get(id).isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                Size yuvSize = mOrderedPreviewSizes.get(0);
                // Create a YUV image reader.
                ImageReader imageReader = ImageReader.newInstance(yuvSize.getWidth(),
                        yuvSize.getHeight(), ImageFormat.YUV_420_888, /*maxImages*/1);

                CameraCaptureSession.StateCallback sessionListener =
                        mock(CameraCaptureSession.StateCallback.class);
                List<OutputConfiguration> outputs = new ArrayList<>();
                OutputConfiguration outputConfig = new OutputConfiguration(
                        imageReader.getSurface());
                OutputConfiguration outputConfigCopy =
                        new OutputConfiguration(imageReader.getSurface());
                assertTrue("OutputConfiguration must be equal to its copy",
                        outputConfig.equals(outputConfigCopy));
                outputConfig.setPhysicalCameraId(id);
                assertFalse("OutputConfigurations with different physical Ids must be different",
                        outputConfig.equals(outputConfigCopy));
                String idCopy = new String(id);
                outputConfigCopy.setPhysicalCameraId(idCopy);
                assertTrue("OutputConfigurations with same physical Ids must be equal",
                        outputConfig.equals(outputConfigCopy));

                // Regardless of logical camera or non-logical camera, create a session of an
                // output configuration with invalid physical camera id, verify that the
                // createCaptureSession fails.
                outputs.add(outputConfig);
                CameraCaptureSession session =
                        CameraTestUtils.configureCameraSessionWithConfig(mCamera, outputs,
                                sessionListener, mHandler);
                verify(sessionListener, timeout(CONFIGURE_TIMEOUT).atLeastOnce()).
                        onConfigureFailed(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onConfigured(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onReady(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onActive(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onClosed(any(CameraCaptureSession.class));
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test for making sure that streaming from physical streams work as expected.
     */
    @Test
    public void testBasicPhysicalStreaming() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                if (!staticInfo.isLogicalMultiCamera()) {
                    Log.i(TAG, "Camera " + id + " is not a logical multi-camera, skipping");
                    continue;
                }

                openDevice(id);
                assertTrue("Logical multi-camera must be LIMITED or higher",
                        mStaticInfo.isHardwareLevelAtLeastLimited());

                // Figure out preview size and physical cameras to use.
                ArrayList<String> dualPhysicalCameraIds = new ArrayList<String>();
                Size previewSize= findCommonPreviewSize(id, dualPhysicalCameraIds);
                if (previewSize == null) {
                    Log.i(TAG, "Camera " + id + ": No matching physical preview streams, skipping");
                    continue;
                }

                testBasicPhysicalStreamingForCamera(
                        id, dualPhysicalCameraIds, previewSize);
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test for making sure that logical/physical stream requests work when both logical stream
     * and physical stream are configured.
     */
    @Test
    public void testBasicLogicalPhysicalStreamCombination() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                if (!staticInfo.isLogicalMultiCamera()) {
                    Log.i(TAG, "Camera " + id + " is not a logical multi-camera, skipping");
                    continue;
                }

                openDevice(id);
                assertTrue("Logical multi-camera must be LIMITED or higher",
                        mStaticInfo.isHardwareLevelAtLeastLimited());

                // Figure out yuv size and physical cameras to use.
                List<String> dualPhysicalCameraIds = new ArrayList<String>();
                Size yuvSize= findCommonPreviewSize(id, dualPhysicalCameraIds);
                if (yuvSize == null) {
                    Log.i(TAG, "Camera " + id + ": No matching physical YUV streams, skipping");
                    continue;
                }

                if (VERBOSE) {
                    Log.v(TAG, "Camera " + id + ": Testing YUV size of " + yuvSize.getWidth() +
                        " x " + yuvSize.getHeight());
                }

                List<OutputConfiguration> outputConfigs = new ArrayList<>();
                List<SimpleImageReaderListener> imageReaderListeners = new ArrayList<>();
                SimpleImageReaderListener readerListenerPhysical = new SimpleImageReaderListener();
                ImageReader yuvTargetPhysical = CameraTestUtils.makeImageReader(yuvSize,
                        ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                        readerListenerPhysical, mHandler);
                OutputConfiguration config = new OutputConfiguration(yuvTargetPhysical.getSurface());
                config.setPhysicalCameraId(dualPhysicalCameraIds.get(0));
                outputConfigs.add(config);

                SimpleImageReaderListener readerListenerLogical = new SimpleImageReaderListener();
                ImageReader yuvTargetLogical = CameraTestUtils.makeImageReader(yuvSize,
                        ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                        readerListenerLogical, mHandler);
                outputConfigs.add(new OutputConfiguration(yuvTargetLogical.getSurface()));
                imageReaderListeners.add(readerListenerLogical);

                SessionConfigSupport sessionConfigSupport = isSessionConfigSupported(
                        mCamera, mHandler, outputConfigs, /*inputConfig*/ null,
                        SessionConfiguration.SESSION_REGULAR, false/*defaultSupport*/);
                assertTrue("Session configuration query for logical camera failed with error",
                        !sessionConfigSupport.error);
                if (!sessionConfigSupport.callSupported) {
                    continue;
                }

                mSessionListener = new BlockingSessionCallback();
                mSessionWaiter = mSessionListener.getStateWaiter();
                mSession = configureCameraSessionWithConfig(mCamera, outputConfigs,
                        mSessionListener, mHandler);
                if (!sessionConfigSupport.configSupported) {
                    mSessionWaiter.waitForState(BlockingSessionCallback.SESSION_CONFIGURE_FAILED,
                            SESSION_CONFIGURE_TIMEOUT_MS);
                    continue;
                }

                // Test request logical stream with an idle physical stream.
                CaptureRequest.Builder requestBuilder =
                    mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                requestBuilder.addTarget(yuvTargetLogical.getSurface());

                for (int i = 0; i < MAX_IMAGE_COUNT; i++) {
                    mSession.capture(requestBuilder.build(), new SimpleCaptureCallback(), mHandler);
                    for (SimpleImageReaderListener readerListener : imageReaderListeners) {
                        Image image = readerListener.getImage(WAIT_FOR_RESULT_TIMEOUT_MS);
                        image.close();
                    }
                }

                // Test request physical stream with an idle logical stream.
                imageReaderListeners.clear();
                imageReaderListeners.add(readerListenerPhysical);

                requestBuilder.removeTarget(yuvTargetLogical.getSurface());
                requestBuilder.addTarget(yuvTargetPhysical.getSurface());

                for (int i = 0; i < MAX_IMAGE_COUNT; i++) {
                    mSession.capture(requestBuilder.build(), new SimpleCaptureCallback(), mHandler);
                    for (SimpleImageReaderListener readerListener : imageReaderListeners) {
                        Image image = readerListener.getImage(WAIT_FOR_RESULT_TIMEOUT_MS);
                        image.close();
                    }
                }

                // Test request logical and physical streams at the same time
                imageReaderListeners.clear();
                readerListenerLogical.drain();
                imageReaderListeners.add(readerListenerLogical);
                readerListenerPhysical.drain();
                imageReaderListeners.add(readerListenerPhysical);

                requestBuilder.addTarget(yuvTargetLogical.getSurface());
                for (int i = 0; i < MAX_IMAGE_COUNT; i++) {
                    mSession.capture(requestBuilder.build(), new SimpleCaptureCallback(), mHandler);
                    for (SimpleImageReaderListener readerListener : imageReaderListeners) {
                        Image image = readerListener.getImage(WAIT_FOR_RESULT_TIMEOUT_MS);
                        image.close();
                    }
                }

                if (mSession != null) {
                    mSession.close();
                }

            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test for making sure that multiple requests for physical cameras work as expected.
     */
    @Test
    public void testBasicPhysicalRequests() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                if (!staticInfo.isLogicalMultiCamera()) {
                    Log.i(TAG, "Camera " + id + " is not a logical multi-camera, skipping");
                    continue;
                }

                openDevice(id);
                assertTrue("Logical multi-camera must be LIMITED or higher",
                        mStaticInfo.isHardwareLevelAtLeastLimited());

                // Figure out yuv size and physical cameras to use.
                List<String> dualPhysicalCameraIds = new ArrayList<String>();
                Size yuvSize= findCommonPreviewSize(id, dualPhysicalCameraIds);
                if (yuvSize == null) {
                    Log.i(TAG, "Camera " + id + ": No matching physical YUV streams, skipping");
                    continue;
                }
                ArraySet<String> physicalIdSet = new ArraySet<String>(dualPhysicalCameraIds.size());
                physicalIdSet.addAll(dualPhysicalCameraIds);

                if (VERBOSE) {
                    Log.v(TAG, "Camera " + id + ": Testing YUV size of " + yuvSize.getWidth() +
                        " x " + yuvSize.getHeight());
                }
                List<CaptureRequest.Key<?>> physicalRequestKeys =
                    mStaticInfo.getCharacteristics().getAvailablePhysicalCameraRequestKeys();
                if (physicalRequestKeys == null) {
                    Log.i(TAG, "Camera " + id + ": no available physical request keys, skipping");
                    continue;
                }

                List<OutputConfiguration> outputConfigs = new ArrayList<>();
                List<ImageReader> imageReaders = new ArrayList<>();
                List<SimpleImageReaderListener> imageReaderListeners = new ArrayList<>();
                for (String physicalCameraId : dualPhysicalCameraIds) {
                    SimpleImageReaderListener readerListener = new SimpleImageReaderListener();
                    ImageReader yuvTarget = CameraTestUtils.makeImageReader(yuvSize,
                            ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                            readerListener, mHandler);
                    OutputConfiguration config = new OutputConfiguration(yuvTarget.getSurface());
                    config.setPhysicalCameraId(physicalCameraId);
                    outputConfigs.add(config);
                    imageReaders.add(yuvTarget);
                    imageReaderListeners.add(readerListener);
                }

                SessionConfigSupport sessionConfigSupport = isSessionConfigSupported(
                        mCamera, mHandler, outputConfigs, /*inputConfig*/ null,
                        SessionConfiguration.SESSION_REGULAR, false/*defaultSupport*/);
                assertTrue("Session configuration query for logical camera failed with error",
                        !sessionConfigSupport.error);
                if (!sessionConfigSupport.callSupported) {
                    continue;
                }

                CaptureRequest.Builder requestBuilder =
                    mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW, physicalIdSet);
                for (OutputConfiguration c : outputConfigs) {
                    requestBuilder.addTarget(c.getSurface());
                }

                mSessionListener = new BlockingSessionCallback();
                mSessionWaiter = mSessionListener.getStateWaiter();
                mSession = configureCameraSessionWithConfig(mCamera, outputConfigs,
                        mSessionListener, mHandler);
                if (!sessionConfigSupport.configSupported) {
                    mSessionWaiter.waitForState(BlockingSessionCallback.SESSION_CONFIGURE_FAILED,
                            SESSION_CONFIGURE_TIMEOUT_MS);
                    continue;
                }

                for (int i = 0; i < MAX_IMAGE_COUNT; i++) {
                    mSession.capture(requestBuilder.build(), new SimpleCaptureCallback(), mHandler);
                    for (SimpleImageReaderListener readerListener : imageReaderListeners) {
                        readerListener.getImage(WAIT_FOR_RESULT_TIMEOUT_MS);
                    }
                }

                // Test streaming requests
                imageReaders.clear();
                outputConfigs.clear();

                // Add physical YUV streams
                List<ImageReader> physicalTargets = new ArrayList<>();
                for (String physicalCameraId : dualPhysicalCameraIds) {
                    ImageReader physicalTarget = CameraTestUtils.makeImageReader(yuvSize,
                            ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                            new ImageDropperListener(), mHandler);
                    OutputConfiguration config = new OutputConfiguration(
                            physicalTarget.getSurface());
                    config.setPhysicalCameraId(physicalCameraId);
                    outputConfigs.add(config);
                    physicalTargets.add(physicalTarget);
                }

                mSessionListener = new BlockingSessionCallback();
                mSession = configureCameraSessionWithConfig(mCamera, outputConfigs,
                        mSessionListener, mHandler);

                requestBuilder = mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW,
                            physicalIdSet);
                for (OutputConfiguration c : outputConfigs) {
                    requestBuilder.addTarget(c.getSurface());
                }

                SimpleCaptureCallback simpleResultListener = new SimpleCaptureCallback();
                mSession.setRepeatingRequest(requestBuilder.build(), simpleResultListener,
                        mHandler);

                // Converge AE
                waitForAeStable(simpleResultListener, NUM_FRAMES_WAITED_FOR_UNKNOWN_LATENCY);

                if (mSession != null) {
                    mSession.close();
                }

            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Tests invalid/incorrect multiple physical capture request cases.
     */
    @Test
    public void testInvalidPhysicalCameraRequests() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (staticInfo.isHardwareLevelLegacy()) {
                    Log.i(TAG, "Camera " + id + " is legacy, skipping");
                    continue;
                }
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                Size yuvSize = mOrderedPreviewSizes.get(0);
                List<OutputConfiguration> outputConfigs = new ArrayList<>();
                List<ImageReader> imageReaders = new ArrayList<>();
                SimpleImageReaderListener readerListener = new SimpleImageReaderListener();
                ImageReader yuvTarget = CameraTestUtils.makeImageReader(yuvSize,
                        ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                        readerListener, mHandler);
                imageReaders.add(yuvTarget);
                OutputConfiguration config = new OutputConfiguration(yuvTarget.getSurface());
                outputConfigs.add(new OutputConfiguration(yuvTarget.getSurface()));

                ArraySet<String> physicalIdSet = new ArraySet<String>();
                // Invalid physical id
                physicalIdSet.add("-2");

                CaptureRequest.Builder requestBuilder =
                    mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW, physicalIdSet);
                requestBuilder.addTarget(config.getSurface());

                // Check for invalid setting get/set
                try {
                    requestBuilder.getPhysicalCameraKey(CaptureRequest.CONTROL_CAPTURE_INTENT, "-1");
                    fail("No exception for invalid physical camera id");
                } catch (IllegalArgumentException e) {
                    //expected
                }

                try {
                    requestBuilder.setPhysicalCameraKey(CaptureRequest.CONTROL_CAPTURE_INTENT,
                            new Integer(0), "-1");
                    fail("No exception for invalid physical camera id");
                } catch (IllegalArgumentException e) {
                    //expected
                }

                mSessionListener = new BlockingSessionCallback();
                mSession = configureCameraSessionWithConfig(mCamera, outputConfigs,
                        mSessionListener, mHandler);

                try {
                    mSession.capture(requestBuilder.build(), new SimpleCaptureCallback(),
                            mHandler);
                    fail("No exception for invalid physical camera id");
                } catch (IllegalArgumentException e) {
                    //expected
                }

                if (mStaticInfo.isLogicalMultiCamera()) {
                    // Figure out yuv size to use.
                    List<String> dualPhysicalCameraIds = new ArrayList<String>();
                    Size sharedSize= findCommonPreviewSize(id, dualPhysicalCameraIds);
                    if (sharedSize == null) {
                        Log.i(TAG, "Camera " + id + ": No matching physical YUV streams, skipping");
                        continue;
                    }

                    physicalIdSet.clear();
                    physicalIdSet.addAll(dualPhysicalCameraIds);
                    requestBuilder =
                        mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW, physicalIdSet);
                    requestBuilder.addTarget(config.getSurface());
                    CaptureRequest request = requestBuilder.build();

                    try {
                        mSession.capture(request, new SimpleCaptureCallback(), mHandler);
                        fail("Capture requests that don't configure outputs with corresponding " +
                                "physical camera id should fail");
                    } catch (IllegalArgumentException e) {
                        //expected
                    }
                }

                if (mSession != null) {
                    mSession.close();
                }
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test for physical camera switch based on focal length (optical zoom) and crop region
     * (digital zoom).
     *
     * - Focal length and crop region change must be synchronized to not have sudden jump in field
     *   of view.
     * - Main physical id must be valid.
     */
    @Test
    public void testLogicalCameraZoomSwitch() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                if (!staticInfo.isLogicalMultiCamera()) {
                    Log.i(TAG, "Camera " + id + " is not a logical multi-camera, skipping");
                    continue;
                }

                openDevice(id);
                Size yuvSize = mOrderedPreviewSizes.get(0);
                // Create a YUV image reader.
                ImageReader imageReader = CameraTestUtils.makeImageReader(yuvSize,
                        ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                        new ImageDropperListener(), mHandler);

                List<OutputConfiguration> outputConfigs = new ArrayList<>();
                OutputConfiguration config = new OutputConfiguration(imageReader.getSurface());
                outputConfigs.add(config);

                mSessionListener = new BlockingSessionCallback();
                mSession = configureCameraSessionWithConfig(mCamera, outputConfigs,
                        mSessionListener, mHandler);

                final float FOV_MARGIN = 0.01f;
                final float[] focalLengths = staticInfo.getAvailableFocalLengthsChecked();
                final int zoomSteps = focalLengths.length;
                final float maxZoom = staticInfo.getAvailableMaxDigitalZoomChecked();
                final Rect activeArraySize = staticInfo.getActiveArraySizeChecked();
                final Set<String> physicalIds =
                        staticInfo.getCharacteristics().getPhysicalCameraIds();

                CaptureRequest.Builder requestBuilder =
                    mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                requestBuilder.addTarget(imageReader.getSurface());

                // For each adjacent focal lengths, set different crop region such that the
                // resulting angle of view is the same. This is to make sure that no sudden FOV
                // (field of view) jump when switching between different focal lengths.
                for (int i = 0; i < zoomSteps-1; i++) {
                    // Start with larger focal length + full active array crop region.
                    requestBuilder.set(CaptureRequest.LENS_FOCAL_LENGTH, focalLengths[i+1]);
                    requestBuilder.set(CaptureRequest.SCALER_CROP_REGION, activeArraySize);
                    SimpleCaptureCallback simpleResultListener = new SimpleCaptureCallback();
                    mSession.setRepeatingRequest(requestBuilder.build(), simpleResultListener,
                            mHandler);
                    waitForAeStable(simpleResultListener, NUM_FRAMES_WAITED_FOR_UNKNOWN_LATENCY);

                    // This is an approximate, assuming that subject distance >> focal length.
                    float zoomFactor = focalLengths[i+1]/focalLengths[i];
                    PointF zoomCenter = new PointF(0.5f, 0.5f);
                    Rect requestCropRegion = getCropRegionForZoom(zoomFactor,
                            zoomCenter, maxZoom, activeArraySize);
                    if (VERBOSE) {
                        Log.v(TAG, "Switching from crop region " + activeArraySize + ", focal " +
                            "length " + focalLengths[i+1] + " to crop region " + requestCropRegion +
                            ", focal length " + focalLengths[i]);
                    }

                    // Create a burst capture to switch between different focal_length/crop_region
                    // combination with same field of view.
                    List<CaptureRequest> requests = new ArrayList<CaptureRequest>();
                    SimpleCaptureCallback listener = new SimpleCaptureCallback();

                    requestBuilder.set(CaptureRequest.LENS_FOCAL_LENGTH, focalLengths[i]);
                    requestBuilder.set(CaptureRequest.SCALER_CROP_REGION, requestCropRegion);
                    requests.add(requestBuilder.build());
                    requests.add(requestBuilder.build());

                    requestBuilder.set(CaptureRequest.LENS_FOCAL_LENGTH, focalLengths[i+1]);
                    requestBuilder.set(CaptureRequest.SCALER_CROP_REGION, activeArraySize);
                    requests.add(requestBuilder.build());
                    requests.add(requestBuilder.build());

                    requestBuilder.set(CaptureRequest.LENS_FOCAL_LENGTH, focalLengths[i]);
                    requestBuilder.set(CaptureRequest.SCALER_CROP_REGION, requestCropRegion);
                    requests.add(requestBuilder.build());
                    requests.add(requestBuilder.build());

                    mSession.captureBurst(requests, listener, mHandler);
                    TotalCaptureResult[] results = listener.getTotalCaptureResultsForRequests(
                            requests, WAIT_FOR_RESULT_TIMEOUT_MS);

                    // Verify result metadata to produce similar field of view.
                    float fov = activeArraySize.width()/(2*focalLengths[i+1]);
                    for (int j = 0; j < results.length; j++) {
                        TotalCaptureResult result = results[j];
                        Float resultFocalLength = result.get(CaptureResult.LENS_FOCAL_LENGTH);
                        Rect resultCropRegion = result.get(CaptureResult.SCALER_CROP_REGION);

                        if (VERBOSE) {
                            Log.v(TAG, "Result crop region " + resultCropRegion + ", focal length "
                                    + resultFocalLength + " for result " + j);
                        }
                        float newFov = resultCropRegion.width()/(2*resultFocalLength);

                        mCollector.expectTrue("Field of view must be consistent with focal " +
                                "length and crop region change cancelling out each other.",
                                Math.abs(newFov - fov)/fov < FOV_MARGIN);

                        if (staticInfo.isActivePhysicalCameraIdSupported()) {
                            String activePhysicalId = result.get(
                                    CaptureResult.LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID);
                            assertTrue(physicalIds.contains(activePhysicalId));

                            StaticMetadata physicalCameraStaticInfo =
                                    mAllStaticInfo.get(activePhysicalId);
                            float[] physicalCameraFocalLengths =
                                    physicalCameraStaticInfo.getAvailableFocalLengthsChecked();
                            mCollector.expectTrue("Current focal length " + resultFocalLength
                                    + " must be supported by active physical camera "
                                    + activePhysicalId, Arrays.asList(CameraTestUtils.toObject(
                                    physicalCameraFocalLengths)).contains(resultFocalLength));
                        }
                    }
                }

                if (mSession != null) {
                    mSession.close();
                }

            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Test that for logical multi-camera, the activePhysicalId is valid, and is the same
     * for all capture templates.
     */
    @Test
    public void testActivePhysicalId() throws Exception {
        final int AVAILABILITY_TIMEOUT_MS = 10;
        final LinkedBlockingQueue<Pair<String, String>> unavailablePhysicalCamEventQueue =
                new LinkedBlockingQueue<>();
        CameraManager.AvailabilityCallback ac = new CameraManager.AvailabilityCallback() {
             @Override
            public void onPhysicalCameraUnavailable(String cameraId, String physicalCameraId) {
                unavailablePhysicalCamEventQueue.offer(new Pair<>(cameraId, physicalCameraId));
            }
        };

        mCameraManager.registerAvailabilityCallback(ac, mHandler);
        Set<Pair<String, String>> unavailablePhysicalCameras = new HashSet<Pair<String, String>>();
        Pair<String, String> candidatePhysicalIds =
                unavailablePhysicalCamEventQueue.poll(AVAILABILITY_TIMEOUT_MS,
                java.util.concurrent.TimeUnit.MILLISECONDS);
        while (candidatePhysicalIds != null) {
            unavailablePhysicalCameras.add(candidatePhysicalIds);
            candidatePhysicalIds =
                unavailablePhysicalCamEventQueue.poll(AVAILABILITY_TIMEOUT_MS,
                java.util.concurrent.TimeUnit.MILLISECONDS);
        }
        mCameraManager.unregisterAvailabilityCallback(ac);

        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                if (!staticInfo.isLogicalMultiCamera()) {
                    Log.i(TAG, "Camera " + id + " is not a logical multi-camera, skipping");
                    continue;
                }

                if (!staticInfo.isActivePhysicalCameraIdSupported()) {
                    continue;
                }

                final Set<String> physicalIds =
                        staticInfo.getCharacteristics().getPhysicalCameraIds();
                openDevice(id);
                Size previewSz =
                        getMaxPreviewSize(mCamera.getId(), mCameraManager,
                        getPreviewSizeBound(mWindowManager, PREVIEW_SIZE_BOUND));

                String storedActiveId = null;
                for (int template : sTemplates) {
                    try {
                        CaptureRequest.Builder requestBuilder =
                                mCamera.createCaptureRequest(template);
                        SimpleCaptureCallback listener = new SimpleCaptureCallback();
                        startPreview(requestBuilder, previewSz, listener);
                        waitForSettingsApplied(listener, NUM_FRAMES_WAITED_FOR_UNKNOWN_LATENCY);

                        CaptureResult result = listener.getCaptureResult(WAIT_FOR_RESULT_TIMEOUT_MS);
                        String activePhysicalId = result.get(
                                CaptureResult.LOGICAL_MULTI_CAMERA_ACTIVE_PHYSICAL_ID);

                        assertNotNull("activePhysicalId must not be null", activePhysicalId);
                        if (storedActiveId == null) {
                            storedActiveId = activePhysicalId;
                            assertTrue(
                                  "Camera device reported invalid activePhysicalId: " +
                                  activePhysicalId, physicalIds.contains(activePhysicalId));
                        } else {
                            assertTrue(
                                  "Camera device reported different activePhysicalId " +
                                  activePhysicalId + " vs " + storedActiveId +
                                  " for different capture templates",
                                  storedActiveId.equals(activePhysicalId));
                        }
                    } catch (IllegalArgumentException e) {
                        if (template == CameraDevice.TEMPLATE_MANUAL &&
                                !staticInfo.isCapabilitySupported(CameraCharacteristics.
                                REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
                            // OK
                        } else if (template == CameraDevice.TEMPLATE_ZERO_SHUTTER_LAG &&
                                !staticInfo.isCapabilitySupported(CameraCharacteristics.
                                REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING)) {
                            // OK.
                        } else {
                            throw e; // rethrow
                        }
                    }
                }

                // Query unavailable physical cameras, and make sure the active physical id
                // isn't an unavailable physical camera.
                for (Pair<String, String> unavailPhysicalCamera: unavailablePhysicalCameras) {
                    assertFalse(unavailPhysicalCamera.first.equals(id) &&
                           unavailPhysicalCamera.second.equals(storedActiveId));
                }
            } finally {
                closeDevice();
            }
        }

    }

    /**
     * Test that for logical multi-camera of a Handheld device, the default FOV is
     * between 50 and 90 degrees for all capture templates.
     */
    @Test
    @CddTest(requirement="7.5.4/C-1-1")
    public void testDefaultFov() throws Exception {
        final double MIN_FOV = 50;
        final double MAX_FOV = 90;
        if (!isHandheldDevice()) {
            return;
        }
        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);

                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                if (!staticInfo.isLogicalMultiCamera()) {
                    Log.i(TAG, "Camera " + id + " is not a logical multi-camera, skipping");
                    continue;
                }

                SizeF physicalSize = staticInfo.getCharacteristics().get(
                        CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE);
                double physicalDiag = Math.sqrt(Math.pow(physicalSize.getWidth(), 2)
                        + Math.pow(physicalSize.getHeight(), 2));
                Rect activeArraySize = staticInfo.getCharacteristics().get(
                        CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);

                openDevice(id);
                for (int template : sTemplates) {
                    try {
                        CaptureRequest.Builder requestBuilder =
                                mCamera.createCaptureRequest(template);
                        Float requestFocalLength = requestBuilder.get(
                                CaptureRequest.LENS_FOCAL_LENGTH);
                        assertNotNull("LENS_FOCAL_LENGTH must not be null", requestFocalLength);

                        Float requestZoomRatio = requestBuilder.get(
                                CaptureRequest.CONTROL_ZOOM_RATIO);
                        assertNotNull("CONTROL_ZOOM_RATIO must not be null", requestZoomRatio);
                        Rect requestCropRegion = requestBuilder.get(
                                CaptureRequest.SCALER_CROP_REGION);
                        assertNotNull("SCALER_CROP_REGION must not be null", requestCropRegion);
                        float totalZoomRatio = Math.min(
                                1.0f * activeArraySize.width() / requestCropRegion.width(),
                                1.0f * activeArraySize.height() / requestCropRegion.height()) *
                                requestZoomRatio;

                        double fov = 2 *
                                Math.toDegrees(Math.atan2(physicalDiag/(2 * totalZoomRatio),
                                requestFocalLength));

                        Log.v(TAG, "Camera " +  id + " template " + template +
                                "'s default FOV is " + fov);
                        mCollector.expectInRange("Camera " +  id + " template " + template +
                                "'s default FOV must fall between [50, 90] degrees",
                                fov, MIN_FOV, MAX_FOV);
                    } catch (IllegalArgumentException e) {
                        if (template == CameraDevice.TEMPLATE_MANUAL &&
                                !staticInfo.isCapabilitySupported(CameraCharacteristics.
                                REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
                            // OK
                        } else if (template == CameraDevice.TEMPLATE_ZERO_SHUTTER_LAG &&
                                !staticInfo.isCapabilitySupported(CameraCharacteristics.
                                REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING)) {
                            // OK.
                        } else {
                            throw e; // rethrow
                        }
                    }
                }
            } finally {
                closeDevice();
            }
        }
    }

    /**
     * Find a common preview size that's supported by both the logical camera and
     * two of the underlying physical cameras.
     */
    private Size findCommonPreviewSize(String cameraId,
            List<String> dualPhysicalCameraIds) throws Exception {

        Set<String> physicalCameraIds =
                mStaticInfo.getCharacteristics().getPhysicalCameraIds();
        assertTrue("Logical camera must contain at least 2 physical camera ids",
                physicalCameraIds.size() >= 2);

        List<Size> previewSizes = getSupportedPreviewSizes(
                cameraId, mCameraManager, PREVIEW_SIZE_BOUND);
        HashMap<String, List<Size>> physicalPreviewSizesMap = new HashMap<String, List<Size>>();
        HashMap<String, StreamConfigurationMap> physicalConfigs = new HashMap<>();
        for (String physicalCameraId : physicalCameraIds) {
            CameraCharacteristics properties =
                    mCameraManager.getCameraCharacteristics(physicalCameraId);
            assertNotNull("Can't get camera characteristics!", properties);
            if (!mAllStaticInfo.get(physicalCameraId).isColorOutputSupported()) {
                // No color output support, skip.
                continue;
            }
            StreamConfigurationMap configMap =
                properties.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            physicalConfigs.put(physicalCameraId, configMap);
            physicalPreviewSizesMap.put(physicalCameraId,
                    getSupportedPreviewSizes(physicalCameraId, mCameraManager, PREVIEW_SIZE_BOUND));
        }

        // Find display size from window service.
        Context context = mActivityRule.getActivity().getApplicationContext();
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();

        int displayWidth = display.getWidth();
        int displayHeight = display.getHeight();

        if (displayHeight > displayWidth) {
            displayHeight = displayWidth;
            displayWidth = display.getHeight();
        }

        StreamConfigurationMap config = mStaticInfo.getCharacteristics().get(
                CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        for (Size previewSize : previewSizes) {
            dualPhysicalCameraIds.clear();
            // Skip preview sizes larger than screen size
            if (previewSize.getWidth() > displayWidth ||
                    previewSize.getHeight() > displayHeight) {
                continue;
            }

            final long minFrameDuration = config.getOutputMinFrameDuration(
                   ImageFormat.YUV_420_888, previewSize);

            ArrayList<String> supportedPhysicalCameras = new ArrayList<String>();
            for (String physicalCameraId : physicalCameraIds) {
                List<Size> physicalPreviewSizes = physicalPreviewSizesMap.get(physicalCameraId);
                if (physicalPreviewSizes != null && physicalPreviewSizes.contains(previewSize)) {
                   long minDurationPhysical =
                           physicalConfigs.get(physicalCameraId).getOutputMinFrameDuration(
                           ImageFormat.YUV_420_888, previewSize);
                   if (minDurationPhysical <= minFrameDuration) {
                        dualPhysicalCameraIds.add(physicalCameraId);
                        if (dualPhysicalCameraIds.size() == 2) {
                            return previewSize;
                        }
                   }
                }
            }
        }
        return null;
    }

    /**
     * Validate that physical cameras are not cropping too much.
     *
     * This is to make sure physical processed streams have at least the same field of view as
     * the logical stream, or the maximum field of view of the physical camera, whichever is
     * smaller.
     *
     * Note that the FOV is calculated in the directio of sensor width.
     */
    private void validatePhysicalCamerasFov(TotalCaptureResult totalCaptureResult,
            List<String> physicalCameraIds) {
        Rect cropRegion = totalCaptureResult.get(CaptureResult.SCALER_CROP_REGION);
        Float focalLength = totalCaptureResult.get(CaptureResult.LENS_FOCAL_LENGTH);
        Float zoomRatio = totalCaptureResult.get(CaptureResult.CONTROL_ZOOM_RATIO);
        Rect activeArraySize = mStaticInfo.getActiveArraySizeChecked();
        SizeF sensorSize = mStaticInfo.getValueFromKeyNonNull(
                CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE);

        // Assume subject distance >> focal length, and subject distance >> camera baseline.
        double fov = 2 * Math.toDegrees(Math.atan2(sensorSize.getWidth() * cropRegion.width() /
                (2 * zoomRatio * activeArraySize.width()),  focalLength));

        Map<String, CaptureResult> physicalResultsDual =
                    totalCaptureResult.getPhysicalCameraResults();
        for (String physicalId : physicalCameraIds) {
            StaticMetadata physicalStaticInfo = mAllStaticInfo.get(physicalId);
            CaptureResult physicalResult = physicalResultsDual.get(physicalId);
            Rect physicalCropRegion = physicalResult.get(CaptureResult.SCALER_CROP_REGION);
            Float physicalFocalLength = physicalResult.get(CaptureResult.LENS_FOCAL_LENGTH);
            Float physicalZoomRatio = physicalResult.get(CaptureResult.CONTROL_ZOOM_RATIO);
            Rect physicalActiveArraySize = physicalStaticInfo.getActiveArraySizeChecked();
            SizeF physicalSensorSize = mStaticInfo.getValueFromKeyNonNull(
                    CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE);

            double physicalFov = 2 * Math.toDegrees(Math.atan2(
                    physicalSensorSize.getWidth() * physicalCropRegion.width() /
                    (2 * physicalZoomRatio * physicalActiveArraySize.width()), physicalFocalLength));

            double maxPhysicalFov = 2 * Math.toDegrees(Math.atan2(physicalSensorSize.getWidth() / 2,
                    physicalFocalLength));
            double expectedPhysicalFov = Math.min(maxPhysicalFov, fov);

            if (VERBOSE) {
                Log.v(TAG, "Logical camera Fov: " + fov + ", maxPhyiscalFov: " + maxPhysicalFov +
                        ", physicalFov: " + physicalFov);
            }
            assertTrue("Physical stream FOV (Field of view) should be greater or equal to"
                    + " min(logical stream FOV, max physical stream FOV). Physical FOV: "
                    + physicalFov + " vs min(" + fov + ", " + maxPhysicalFov,
                    physicalFov - expectedPhysicalFov > -FOV_THRESHOLD);
        }
    }

    /**
     * Test physical camera YUV streaming within a particular logical camera.
     *
     * Use 2 YUV streams with PREVIEW or smaller size.
     */
    private void testBasicPhysicalStreamingForCamera(String logicalCameraId,
            List<String> physicalCameraIds, Size previewSize) throws Exception {
        List<OutputConfiguration> outputConfigs = new ArrayList<>();
        List<ImageReader> imageReaders = new ArrayList<>();

        // Add 1 logical YUV stream
        ImageReader logicalTarget = CameraTestUtils.makeImageReader(previewSize,
                ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                new ImageDropperListener(), mHandler);
        imageReaders.add(logicalTarget);
        outputConfigs.add(new OutputConfiguration(logicalTarget.getSurface()));

        // Add physical YUV streams
        if (physicalCameraIds.size() != 2) {
            throw new IllegalArgumentException("phyiscalCameraIds must contain 2 camera ids");
        }
        List<ImageReader> physicalTargets = new ArrayList<>();
        for (String physicalCameraId : physicalCameraIds) {
            ImageReader physicalTarget = CameraTestUtils.makeImageReader(previewSize,
                    ImageFormat.YUV_420_888, MAX_IMAGE_COUNT,
                    new ImageDropperListener(), mHandler);
            OutputConfiguration config = new OutputConfiguration(physicalTarget.getSurface());
            config.setPhysicalCameraId(physicalCameraId);
            outputConfigs.add(config);
            physicalTargets.add(physicalTarget);
        }

        SessionConfigSupport sessionConfigSupport = isSessionConfigSupported(
                mCamera, mHandler, outputConfigs, /*inputConfig*/ null,
                SessionConfiguration.SESSION_REGULAR, false/*defaultSupport*/);
        assertTrue("Session configuration query for logical camera failed with error",
                !sessionConfigSupport.error);
        if (!sessionConfigSupport.callSupported) {
            return;
        }

        mSessionListener = new BlockingSessionCallback();
        mSessionWaiter = mSessionListener.getStateWaiter();
        mSession = configureCameraSessionWithConfig(mCamera, outputConfigs,
                mSessionListener, mHandler);
        if (!sessionConfigSupport.configSupported) {
            mSessionWaiter.waitForState(BlockingSessionCallback.SESSION_CONFIGURE_FAILED,
                    SESSION_CONFIGURE_TIMEOUT_MS);
            return;
        }

        // Stream logical YUV stream and note down the FPS
        CaptureRequest.Builder requestBuilder =
                mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
        requestBuilder.addTarget(logicalTarget.getSurface());

        SimpleCaptureCallback simpleResultListener =
                new SimpleCaptureCallback();
        StreamConfigurationMap config = mStaticInfo.getCharacteristics().get(
                CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        final long minFrameDuration = config.getOutputMinFrameDuration(
                ImageFormat.YUV_420_888, previewSize);
        if (minFrameDuration > 0) {
            Range<Integer> targetRange = getSuitableFpsRangeForDuration(logicalCameraId,
                    minFrameDuration);
            requestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, targetRange);
        }
        mSession.setRepeatingRequest(requestBuilder.build(),
                simpleResultListener, mHandler);

        // Converge AE
        waitForAeStable(simpleResultListener, NUM_FRAMES_WAITED_FOR_UNKNOWN_LATENCY);

        if (mStaticInfo.isAeLockSupported()) {
            // Lock AE if supported.
            requestBuilder.set(CaptureRequest.CONTROL_AE_LOCK, true);
            mSession.setRepeatingRequest(requestBuilder.build(), simpleResultListener,
                    mHandler);
            waitForResultValue(simpleResultListener, CaptureResult.CONTROL_AE_STATE,
                    CaptureResult.CONTROL_AE_STATE_LOCKED, NUM_RESULTS_WAIT_TIMEOUT);
        }

        // Verify results
        CaptureResultTest.validateCaptureResult(mCollector, simpleResultListener,
                mStaticInfo, mAllStaticInfo, null/*requestedPhysicalIds*/,
                requestBuilder, NUM_FRAMES_CHECKED);

        // Collect timestamps for one logical stream only.
        long prevTimestamp = -1;
        long[] logicalTimestamps = new long[NUM_FRAMES_CHECKED];
        for (int i = 0; i < NUM_FRAMES_CHECKED; i++) {
            TotalCaptureResult totalCaptureResult =
                    simpleResultListener.getTotalCaptureResult(
                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
            logicalTimestamps[i] = totalCaptureResult.get(CaptureResult.SENSOR_TIMESTAMP);
        }

        double logicalAvgDurationMs = (logicalTimestamps[NUM_FRAMES_CHECKED-1] -
                logicalTimestamps[0])/(NS_PER_MS*(NUM_FRAMES_CHECKED-1));

        // Request one logical stream and one physical stream
        simpleResultListener = new SimpleCaptureCallback();
        requestBuilder.addTarget(physicalTargets.get(1).getSurface());
        mSession.setRepeatingRequest(requestBuilder.build(), simpleResultListener,
                mHandler);

        // Verify results for physical streams request.
        CaptureResultTest.validateCaptureResult(mCollector, simpleResultListener,
                mStaticInfo, mAllStaticInfo, physicalCameraIds.subList(1, 2), requestBuilder,
                NUM_FRAMES_CHECKED);


        // Start requesting on both logical and physical streams
        SimpleCaptureCallback simpleResultListenerDual =
                new SimpleCaptureCallback();
        for (ImageReader physicalTarget : physicalTargets) {
            requestBuilder.addTarget(physicalTarget.getSurface());
        }
        mSession.setRepeatingRequest(requestBuilder.build(), simpleResultListenerDual,
                mHandler);

        // Verify results for physical streams request.
        CaptureResultTest.validateCaptureResult(mCollector, simpleResultListenerDual,
                mStaticInfo, mAllStaticInfo, physicalCameraIds, requestBuilder,
                NUM_FRAMES_CHECKED);

        // Acquire the timestamps of the physical camera.
        long[] logicalTimestamps2 = new long[NUM_FRAMES_CHECKED];
        long [][] physicalTimestamps = new long[physicalTargets.size()][];
        for (int i = 0; i < physicalTargets.size(); i++) {
            physicalTimestamps[i] = new long[NUM_FRAMES_CHECKED];
        }
        for (int i = 0; i < NUM_FRAMES_CHECKED; i++) {
            TotalCaptureResult totalCaptureResultDual =
                    simpleResultListenerDual.getTotalCaptureResult(
                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
            logicalTimestamps2[i] = totalCaptureResultDual.get(CaptureResult.SENSOR_TIMESTAMP);

            int index = 0;
            Map<String, CaptureResult> physicalResultsDual =
                    totalCaptureResultDual.getPhysicalCameraResults();
            for (String physicalId : physicalCameraIds) {
                 if (physicalResultsDual.containsKey(physicalId)) {
                     physicalTimestamps[index][i] = physicalResultsDual.get(physicalId).get(
                             CaptureResult.SENSOR_TIMESTAMP);
                 } else {
                     physicalTimestamps[index][i] = -1;
                 }
                 index++;
            }
        }

        // Check both logical and physical streams' crop region, and make sure their FOVs
        // are similar.
        TotalCaptureResult totalCaptureResult =
                simpleResultListenerDual.getTotalCaptureResult(
                CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
        validatePhysicalCamerasFov(totalCaptureResult, physicalCameraIds);

        // Check timestamp monolithity for individual camera and across cameras
        for (int i = 0; i < NUM_FRAMES_CHECKED-1; i++) {
            assertTrue("Logical camera timestamp must monolithically increase",
                    logicalTimestamps2[i] < logicalTimestamps2[i+1]);
        }
        for (int i = 0; i < physicalCameraIds.size(); i++) {
            for (int j = 0 ; j < NUM_FRAMES_CHECKED-1; j++) {
                if (physicalTimestamps[i][j] != -1 && physicalTimestamps[i][j+1] != -1) {
                    assertTrue("Physical camera timestamp must monolithically increase",
                            physicalTimestamps[i][j] < physicalTimestamps[i][j+1]);
                }
                if (j > 0 && physicalTimestamps[i][j] != -1) {
                    assertTrue("Physical camera's timestamp N must be greater than logical " +
                            "camera's timestamp N-1",
                            physicalTimestamps[i][j] > logicalTimestamps[j-1]);
                }
                if (physicalTimestamps[i][j] != -1) {
                    assertTrue("Physical camera's timestamp N must be less than logical camera's " +
                            "timestamp N+1", physicalTimestamps[i][j] > logicalTimestamps[j+1]);
                }
            }
        }

        double logicalAvgDurationMs2 = (logicalTimestamps2[NUM_FRAMES_CHECKED-1] -
                logicalTimestamps2[0])/(NS_PER_MS*(NUM_FRAMES_CHECKED-1));
        // Check CALIBRATED synchronization between physical cameras
        Integer syncType = mStaticInfo.getCharacteristics().get(
                CameraCharacteristics.LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE);
        double fpsRatio = (logicalAvgDurationMs2 - logicalAvgDurationMs)/logicalAvgDurationMs;
        if (syncType == CameraCharacteristics.LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE_CALIBRATED) {
            // Check framerate doesn't slow down with physical streams
            mCollector.expectTrue(
                    "The average frame duration with concurrent physical streams is" +
                    logicalAvgDurationMs2 + " ms vs " + logicalAvgDurationMs +
                    " ms for logical streams only", fpsRatio <= FRAME_DURATION_THRESHOLD);

            long maxTimestampDelta = 0;
            for (int i = 0; i < NUM_FRAMES_CHECKED; i++) {
                long delta = Math.abs(physicalTimestamps[0][i] - physicalTimestamps[1][i]);
                if (delta > maxTimestampDelta) {
                    maxTimestampDelta = delta;
                }
            }

            Log.i(TAG, "Maximum difference between physical camera timestamps: "
                    + maxTimestampDelta);

            // The maximum timestamp difference should not be larger than the threshold.
            mCollector.expectTrue(
                    "The maximum timestamp deltas between the physical cameras "
                    + maxTimestampDelta + " is larger than " + MAX_TIMESTAMP_DIFFERENCE_THRESHOLD,
                    maxTimestampDelta <= MAX_TIMESTAMP_DIFFERENCE_THRESHOLD);
        } else {
            // Do not enforce fps check for APPROXIMATE synced device.
            if (fpsRatio > FRAME_DURATION_THRESHOLD) {
                Log.w(TAG, "The average frame duration with concurrent physical streams is" +
                        logicalAvgDurationMs2 + " ms vs " + logicalAvgDurationMs +
                        " ms for logical streams only");
            }
        }

        if (VERBOSE) {
            while (simpleResultListenerDual.hasMoreFailures()) {
                ArrayList<CaptureFailure> failures =
                    simpleResultListenerDual.getCaptureFailures(/*maxNumFailures*/ 1);
                for (CaptureFailure failure : failures) {
                    String physicalCameraId = failure.getPhysicalCameraId();
                    if (physicalCameraId != null) {
                        Log.v(TAG, "Capture result failure for physical camera id: " +
                                physicalCameraId);
                    }
                }
            }
        }

        // Stop preview
        if (mSession != null) {
            mSession.close();
        }
    }

    /**
     * The CDD defines a handheld device as one that has a battery and a screen size between
     * 2.5 and 8 inches.
     */
    private boolean isHandheldDevice() throws Exception {
        double screenInches = getScreenSizeInInches();
        return deviceHasBattery() && screenInches >= 2.5 && screenInches <= 8.0;
    }

    private boolean deviceHasBattery() {
        final Intent batteryInfo = mContext.registerReceiver(null,
                new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        return batteryInfo != null && batteryInfo.getBooleanExtra(BatteryManager.EXTRA_PRESENT, true);
    }

    private double getScreenSizeInInches() {
        DisplayMetrics dm = new DisplayMetrics();
        mWindowManager.getDefaultDisplay().getMetrics(dm);
        double widthInInchesSquared = Math.pow(dm.widthPixels/dm.xdpi,2);
        double heightInInchesSquared = Math.pow(dm.heightPixels/dm.ydpi,2);
        return Math.sqrt(widthInInchesSquared + heightInInchesSquared);
    }
}
