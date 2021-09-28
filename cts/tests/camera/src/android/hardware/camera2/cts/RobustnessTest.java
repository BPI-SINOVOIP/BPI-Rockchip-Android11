/*
 * Copyright 2014 The Android Open Source Project
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
import static android.hardware.camera2.cts.RobustnessTest.MaxStreamSizes.*;

import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.testcases.Camera2AndroidTestCase;
import android.hardware.camera2.params.InputConfiguration;
import android.hardware.camera2.params.OisSample;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.MandatoryStreamCombination;
import android.hardware.camera2.params.MandatoryStreamCombination.MandatoryStreamInformation;
import android.hardware.camera2.params.SessionConfiguration;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.CamcorderProfile;
import android.media.Image;
import android.media.ImageReader;
import android.media.ImageWriter;
import android.util.Log;
import android.util.Pair;
import android.util.Size;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

import com.android.ex.camera2.blocking.BlockingSessionCallback;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.junit.runners.Parameterized;
import org.junit.runner.RunWith;
import org.junit.Test;

import static junit.framework.Assert.assertTrue;
import static org.mockito.Mockito.*;

/**
 * Tests exercising edge cases in camera setup, configuration, and usage.
 */

@RunWith(Parameterized.class)
public class RobustnessTest extends Camera2AndroidTestCase {
    private static final String TAG = "RobustnessTest";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    private static final int CONFIGURE_TIMEOUT = 5000; //ms
    private static final int CAPTURE_TIMEOUT = 1500; //ms

    // For testTriggerInteractions
    private static final int PREVIEW_WARMUP_FRAMES = 60;
    private static final int MAX_RESULT_STATE_CHANGE_WAIT_FRAMES = 100;
    private static final int MAX_TRIGGER_SEQUENCE_FRAMES = 180; // 6 sec at 30 fps
    private static final int MAX_RESULT_STATE_POSTCHANGE_WAIT_FRAMES = 10;

    /**
     * Test that a {@link CameraCaptureSession} can be configured with a {@link Surface} containing
     * a dimension other than one of the supported output dimensions.  The buffers produced into
     * this surface are expected have the dimensions of the closest possible buffer size in the
     * available stream configurations for a surface with this format.
     */
    @Test
    public void testBadSurfaceDimensions() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            try {
                Log.i(TAG, "Testing Camera " + id);
                openDevice(id);

                List<Size> testSizes = null;
                int format = mStaticInfo.isColorOutputSupported() ?
                    ImageFormat.YUV_420_888 : ImageFormat.DEPTH16;

                testSizes = CameraTestUtils.getSortedSizesForFormat(id, mCameraManager,
                        format, null);

                // Find some size not supported by the camera
                Size weirdSize = new Size(643, 577);
                int count = 0;
                while(testSizes.contains(weirdSize)) {
                    // Really, they can't all be supported...
                    weirdSize = new Size(weirdSize.getWidth() + 1, weirdSize.getHeight() + 1);
                    count++;
                    assertTrue("Too many exotic YUV_420_888 resolutions supported.", count < 100);
                }

                // Setup imageReader with invalid dimension
                ImageReader imageReader = ImageReader.newInstance(weirdSize.getWidth(),
                        weirdSize.getHeight(), format, 3);

                // Setup ImageReaderListener
                SimpleImageReaderListener imageListener = new SimpleImageReaderListener();
                imageReader.setOnImageAvailableListener(imageListener, mHandler);

                Surface surface = imageReader.getSurface();
                List<Surface> surfaces = new ArrayList<>();
                surfaces.add(surface);

                // Setup a capture request and listener
                CaptureRequest.Builder request =
                        mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                request.addTarget(surface);

                // Check that correct session callback is hit.
                CameraCaptureSession.StateCallback sessionListener =
                        mock(CameraCaptureSession.StateCallback.class);
                CameraCaptureSession session = CameraTestUtils.configureCameraSession(mCamera,
                        surfaces, sessionListener, mHandler);

                verify(sessionListener, timeout(CONFIGURE_TIMEOUT).atLeastOnce()).
                        onConfigured(any(CameraCaptureSession.class));
                verify(sessionListener, timeout(CONFIGURE_TIMEOUT).atLeastOnce()).
                        onReady(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onConfigureFailed(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onActive(any(CameraCaptureSession.class));
                verify(sessionListener, never()).onClosed(any(CameraCaptureSession.class));

                CameraCaptureSession.CaptureCallback captureListener =
                        mock(CameraCaptureSession.CaptureCallback.class);
                session.capture(request.build(), captureListener, mHandler);

                verify(captureListener, timeout(CAPTURE_TIMEOUT).atLeastOnce()).
                        onCaptureCompleted(any(CameraCaptureSession.class),
                                any(CaptureRequest.class), any(TotalCaptureResult.class));
                verify(captureListener, never()).onCaptureFailed(any(CameraCaptureSession.class),
                        any(CaptureRequest.class), any(CaptureFailure.class));

                Image image = imageListener.getImage(CAPTURE_TIMEOUT);
                int imageWidth = image.getWidth();
                int imageHeight = image.getHeight();
                Size actualSize = new Size(imageWidth, imageHeight);

                assertTrue("Camera does not contain outputted image resolution " + actualSize,
                        testSizes.contains(actualSize));
                imageReader.close();
            } finally {
                closeDevice(id);
            }
        }
    }

    /**
     * Test for making sure the mandatory stream combinations work as expected.
     */
    @Test
    public void testMandatoryOutputCombinations() throws Exception {
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
            openDevice(id);
            MandatoryStreamCombination[] combinations =
                    mStaticInfo.getCharacteristics().get(
                            CameraCharacteristics.SCALER_MANDATORY_STREAM_COMBINATIONS);
            if (combinations == null) {
                Log.i(TAG, "No mandatory stream combinations for camera: " + id + " skip test");
                closeDevice(id);
                continue;
            }

            try {
                for (MandatoryStreamCombination combination : combinations) {
                    if (!combination.isReprocessable()) {
                        testMandatoryStreamCombination(id, mStaticInfo,
                                null/*physicalCameraId*/, combination);
                    }
                }

                // Make sure mandatory stream combinations for each physical camera work
                // as expected.
                if (mStaticInfo.isLogicalMultiCamera()) {
                    Set<String> physicalCameraIds =
                            mStaticInfo.getCharacteristics().getPhysicalCameraIds();
                    for (String physicalId : physicalCameraIds) {
                        if (Arrays.asList(mCameraIdsUnderTest).contains(physicalId)) {
                            // If physicalId is advertised in camera ID list, do not need to test
                            // its stream combination through logical camera.
                            continue;
                        }
                        for (Pair<String, String> unavailPhysicalCam : unavailablePhysicalCameras) {
                            if (unavailPhysicalCam.first.equals(id) ||
                                    unavailPhysicalCam.second.equals(physicalId)) {
                                // This particular physical camera isn't available. Skip.
                                continue;
                            }
                        }
                        StaticMetadata physicalStaticInfo = mAllStaticInfo.get(physicalId);
                        MandatoryStreamCombination[] phyCombinations =
                                physicalStaticInfo.getCharacteristics().get(
                                        CameraCharacteristics.SCALER_MANDATORY_STREAM_COMBINATIONS);

                        if (phyCombinations == null) {
                            Log.i(TAG, "No mandatory stream combinations for physical camera device: " + id + " skip test");
                            continue;
                        }

                        for (MandatoryStreamCombination combination : phyCombinations) {
                            if (!combination.isReprocessable()) {
                                testMandatoryStreamCombination(id, physicalStaticInfo,
                                        physicalId, combination);
                            }
                        }
                    }
                }

            } finally {
                closeDevice(id);
            }
        }
    }

    private void testMandatoryStreamCombination(String cameraId, StaticMetadata staticInfo,
            String physicalCameraId, MandatoryStreamCombination combination) throws Exception {
        // Check whether substituting YUV_888 format with Y8 format
        boolean substituteY8 = false;
        if (staticInfo.isMonochromeWithY8()) {
            List<MandatoryStreamInformation> streamsInfo = combination.getStreamsInformation();
            for (MandatoryStreamInformation streamInfo : streamsInfo) {
                if (streamInfo.getFormat() == ImageFormat.YUV_420_888) {
                    substituteY8 = true;
                    break;
                }
            }
        }

        // Check whether substituting JPEG format with HEIC format
        boolean substituteHeic = false;
        if (staticInfo.isHeicSupported()) {
            List<MandatoryStreamInformation> streamsInfo = combination.getStreamsInformation();
            for (MandatoryStreamInformation streamInfo : streamsInfo) {
                if (streamInfo.getFormat() == ImageFormat.JPEG) {
                    substituteHeic = true;
                    break;
                }
            }
        }

        // Test camera output combination
        String log = "Testing mandatory stream combination: " + combination.getDescription() +
                " on camera: " + cameraId;
        if (physicalCameraId != null) {
            log += ", physical sub-camera: " + physicalCameraId;
        }
        Log.i(TAG, log);
        testMandatoryStreamCombination(cameraId, staticInfo, physicalCameraId, combination,
                /*substituteY8*/false, /*substituteHeic*/false);

        if (substituteY8) {
            Log.i(TAG, log + " with Y8");
            testMandatoryStreamCombination(cameraId, staticInfo, physicalCameraId, combination,
                    /*substituteY8*/true, /*substituteHeic*/false);
        }

        if (substituteHeic) {
            Log.i(TAG, log + " with HEIC");
            testMandatoryStreamCombination(cameraId, staticInfo, physicalCameraId, combination,
                    /*substituteY8*/false, /*substituteHeic*/true);
        }
    }

    private void testMandatoryStreamCombination(String cameraId,
            StaticMetadata staticInfo, String physicalCameraId,
            MandatoryStreamCombination combination,
            boolean substituteY8, boolean substituteHeic) throws Exception {

        // Timeout is relaxed by 1 second for LEGACY devices to reduce false positive rate in CTS
        final int TIMEOUT_FOR_RESULT_MS = (staticInfo.isHardwareLevelLegacy()) ? 2000 : 1000;
        final int MIN_RESULT_COUNT = 3;

        // Set up outputs
        List<OutputConfiguration> outputConfigs = new ArrayList<OutputConfiguration>();
        List<SurfaceTexture> privTargets = new ArrayList<SurfaceTexture>();
        List<ImageReader> jpegTargets = new ArrayList<ImageReader>();
        List<ImageReader> yuvTargets = new ArrayList<ImageReader>();
        List<ImageReader> y8Targets = new ArrayList<ImageReader>();
        List<ImageReader> rawTargets = new ArrayList<ImageReader>();
        List<ImageReader> heicTargets = new ArrayList<ImageReader>();
        List<ImageReader> depth16Targets = new ArrayList<ImageReader>();

        CameraTestUtils.setupConfigurationTargets(combination.getStreamsInformation(), privTargets,
                jpegTargets, yuvTargets, y8Targets, rawTargets, heicTargets, depth16Targets,
                outputConfigs, MIN_RESULT_COUNT, substituteY8, substituteHeic, physicalCameraId,
                mHandler);

        boolean haveSession = false;
        try {
            CaptureRequest.Builder requestBuilder =
                    mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);

            for (OutputConfiguration c : outputConfigs) {
                requestBuilder.addTarget(c.getSurface());
            }

            CameraCaptureSession.CaptureCallback mockCaptureCallback =
                    mock(CameraCaptureSession.CaptureCallback.class);

            if (physicalCameraId == null) {
                checkSessionConfigurationSupported(mCamera, mHandler, outputConfigs,
                        /*inputConfig*/ null, SessionConfiguration.SESSION_REGULAR,
                        true/*defaultSupport*/, String.format(
                        "Session configuration query from combination: %s failed",
                        combination.getDescription()));
            } else {
                SessionConfigSupport sessionConfigSupport = isSessionConfigSupported(
                        mCamera, mHandler, outputConfigs, /*inputConfig*/ null,
                        SessionConfiguration.SESSION_REGULAR, false/*defaultSupport*/);
                assertTrue(
                        String.format("Session configuration query from combination: %s failed",
                        combination.getDescription()), !sessionConfigSupport.error);
                if (!sessionConfigSupport.callSupported) {
                    return;
                }
                assertTrue(
                        String.format("Session configuration must be supported for combination: " +
                        "%s", combination.getDescription()), sessionConfigSupport.configSupported);
            }

            createSessionByConfigs(outputConfigs);
            haveSession = true;
            CaptureRequest request = requestBuilder.build();
            mCameraSession.setRepeatingRequest(request, mockCaptureCallback, mHandler);

            verify(mockCaptureCallback,
                    timeout(TIMEOUT_FOR_RESULT_MS * MIN_RESULT_COUNT).atLeast(MIN_RESULT_COUNT))
                    .onCaptureCompleted(
                        eq(mCameraSession),
                        eq(request),
                        isA(TotalCaptureResult.class));
            verify(mockCaptureCallback, never()).
                    onCaptureFailed(
                        eq(mCameraSession),
                        eq(request),
                        isA(CaptureFailure.class));

        } catch (Throwable e) {
            mCollector.addMessage(String.format("Mandatory stream combination: %s failed due: %s",
                    combination.getDescription(), e.getMessage()));
        }
        if (haveSession) {
            try {
                Log.i(TAG, String.format("Done with camera %s, combination: %s, closing session",
                                cameraId, combination.getDescription()));
                stopCapture(/*fast*/false);
            } catch (Throwable e) {
                mCollector.addMessage(
                    String.format("Closing down for combination: %s failed due to: %s",
                            combination.getDescription(), e.getMessage()));
            }
        }

        for (SurfaceTexture target : privTargets) {
            target.release();
        }
        for (ImageReader target : jpegTargets) {
            target.close();
        }
        for (ImageReader target : yuvTargets) {
            target.close();
        }
        for (ImageReader target : y8Targets) {
            target.close();
        }
        for (ImageReader target : rawTargets) {
            target.close();
        }
        for (ImageReader target : heicTargets) {
            target.close();
        }
        for (ImageReader target : depth16Targets) {
            target.close();
        }
    }

    /**
     * Test for making sure the required reprocess input/output combinations for each hardware
     * level and capability work as expected.
     */
    @Test
    public void testMandatoryReprocessConfigurations() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            openDevice(id);
            MandatoryStreamCombination[] combinations =
                    mStaticInfo.getCharacteristics().get(
                            CameraCharacteristics.SCALER_MANDATORY_STREAM_COMBINATIONS);
            if (combinations == null) {
                Log.i(TAG, "No mandatory stream combinations for camera: " + id + " skip test");
                closeDevice(id);
                continue;
            }

            try {
                for (MandatoryStreamCombination combination : combinations) {
                    if (combination.isReprocessable()) {
                        Log.i(TAG, "Testing mandatory reprocessable stream combination: " +
                                combination.getDescription() + " on camera: " + id);
                        testMandatoryReprocessableStreamCombination(id, combination);
                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    private void testMandatoryReprocessableStreamCombination(String cameraId,
            MandatoryStreamCombination combination) {
        // Test reprocess stream combination
        testMandatoryReprocessableStreamCombination(cameraId, combination,
                /*substituteY8*/false, /*substituteHeic*/false);

        // Test substituting YUV_888 format with Y8 format in reprocess stream combination.
        if (mStaticInfo.isMonochromeWithY8()) {
            List<MandatoryStreamInformation> streamsInfo = combination.getStreamsInformation();
            boolean substituteY8 = false;
            for (MandatoryStreamInformation streamInfo : streamsInfo) {
                if (streamInfo.getFormat() == ImageFormat.YUV_420_888) {
                    substituteY8 = true;
                }
            }
            if (substituteY8) {
                testMandatoryReprocessableStreamCombination(cameraId, combination,
                        /*substituteY8*/true, /*substituteHeic*/false);
            }
        }

        if (mStaticInfo.isHeicSupported()) {
            List<MandatoryStreamInformation> streamsInfo = combination.getStreamsInformation();
            boolean substituteHeic = false;
            for (MandatoryStreamInformation streamInfo : streamsInfo) {
                if (streamInfo.getFormat() == ImageFormat.JPEG) {
                    substituteHeic = true;
                }
            }
            if (substituteHeic) {
                testMandatoryReprocessableStreamCombination(cameraId, combination,
                        /*substituteY8*/false, /*substituteHeic*/true);
            }
        }
    }

    private void testMandatoryReprocessableStreamCombination(String cameraId,
            MandatoryStreamCombination combination, boolean substituteY8,
            boolean substituteHeic) {

        final int TIMEOUT_FOR_RESULT_MS = 5000;
        final int NUM_REPROCESS_CAPTURES_PER_CONFIG = 3;

        List<SurfaceTexture> privTargets = new ArrayList<>();
        List<ImageReader> jpegTargets = new ArrayList<>();
        List<ImageReader> yuvTargets = new ArrayList<>();
        List<ImageReader> y8Targets = new ArrayList<>();
        List<ImageReader> rawTargets = new ArrayList<>();
        List<ImageReader> heicTargets = new ArrayList<>();
        List<ImageReader> depth16Targets = new ArrayList<>();
        ArrayList<Surface> outputSurfaces = new ArrayList<>();
        List<OutputConfiguration> outputConfigs = new ArrayList<OutputConfiguration>();
        ImageReader inputReader = null;
        ImageWriter inputWriter = null;
        SimpleImageReaderListener inputReaderListener = new SimpleImageReaderListener();
        SimpleCaptureCallback inputCaptureListener = new SimpleCaptureCallback();
        SimpleCaptureCallback reprocessOutputCaptureListener = new SimpleCaptureCallback();

        List<MandatoryStreamInformation> streamInfo = combination.getStreamsInformation();
        assertTrue("Reprocessable stream combinations should have at least 3 or more streams",
                    (streamInfo != null) && (streamInfo.size() >= 3));

        assertTrue("The first mandatory stream information in a reprocessable combination must " +
                "always be input", streamInfo.get(0).isInput());

        List<Size> inputSizes = streamInfo.get(0).getAvailableSizes();
        int inputFormat = streamInfo.get(0).getFormat();
        if (substituteY8 && (inputFormat == ImageFormat.YUV_420_888)) {
            inputFormat = ImageFormat.Y8;
        }

        Log.i(TAG, "testMandatoryReprocessableStreamCombination: " +
                combination.getDescription() + ", substituteY8 = " + substituteY8 +
                ", substituteHeic = " + substituteHeic);
        try {
            // The second stream information entry is the ZSL stream, which is configured
            // separately.
            CameraTestUtils.setupConfigurationTargets(streamInfo.subList(2, streamInfo.size()),
                    privTargets, jpegTargets, yuvTargets, y8Targets, rawTargets, heicTargets,
                    depth16Targets, outputConfigs, NUM_REPROCESS_CAPTURES_PER_CONFIG, substituteY8,
                    substituteHeic, null/*overridePhysicalCameraId*/, mHandler);

            outputSurfaces.ensureCapacity(outputConfigs.size());
            for (OutputConfiguration config : outputConfigs) {
                outputSurfaces.add(config.getSurface());
            }

            InputConfiguration inputConfig = new InputConfiguration(inputSizes.get(0).getWidth(),
                    inputSizes.get(0).getHeight(), inputFormat);

            // For each config, YUV and JPEG outputs will be tested. (For YUV/Y8 reprocessing,
            // the YUV/Y8 ImageReader for input is also used for output.)
            final boolean inputIsYuv = inputConfig.getFormat() == ImageFormat.YUV_420_888;
            final boolean inputIsY8 = inputConfig.getFormat() == ImageFormat.Y8;
            final boolean useYuv = inputIsYuv || yuvTargets.size() > 0;
            final boolean useY8 = inputIsY8 || y8Targets.size() > 0;
            final int totalNumReprocessCaptures =  NUM_REPROCESS_CAPTURES_PER_CONFIG * (
                    ((inputIsYuv || inputIsY8) ? 1 : 0) +
                    (substituteHeic ? heicTargets.size() : jpegTargets.size()) +
                    (useYuv ? yuvTargets.size() : y8Targets.size()));

            // It needs 1 input buffer for each reprocess capture + the number of buffers
            // that will be used as outputs.
            inputReader = ImageReader.newInstance(inputConfig.getWidth(), inputConfig.getHeight(),
                    inputConfig.getFormat(),
                    totalNumReprocessCaptures + NUM_REPROCESS_CAPTURES_PER_CONFIG);
            inputReader.setOnImageAvailableListener(inputReaderListener, mHandler);
            outputSurfaces.add(inputReader.getSurface());

            checkSessionConfigurationWithSurfaces(mCamera, mHandler, outputSurfaces,
                    inputConfig, SessionConfiguration.SESSION_REGULAR, /*defaultSupport*/ true,
                    String.format("Session configuration query %s failed",
                    combination.getDescription()));

            // Verify we can create a reprocessable session with the input and all outputs.
            BlockingSessionCallback sessionListener = new BlockingSessionCallback();
            CameraCaptureSession session = configureReprocessableCameraSession(mCamera,
                    inputConfig, outputSurfaces, sessionListener, mHandler);
            inputWriter = ImageWriter.newInstance(session.getInputSurface(),
                    totalNumReprocessCaptures);

            // Prepare a request for reprocess input
            CaptureRequest.Builder builder = mCamera.createCaptureRequest(
                    CameraDevice.TEMPLATE_ZERO_SHUTTER_LAG);
            builder.addTarget(inputReader.getSurface());

            for (int i = 0; i < totalNumReprocessCaptures; i++) {
                session.capture(builder.build(), inputCaptureListener, mHandler);
            }

            List<CaptureRequest> reprocessRequests = new ArrayList<>();
            List<Surface> reprocessOutputs = new ArrayList<>();
            if (inputIsYuv || inputIsY8) {
                reprocessOutputs.add(inputReader.getSurface());
            }

            for (ImageReader reader : jpegTargets) {
                reprocessOutputs.add(reader.getSurface());
            }

            for (ImageReader reader : heicTargets) {
                reprocessOutputs.add(reader.getSurface());
            }

            for (ImageReader reader : yuvTargets) {
                reprocessOutputs.add(reader.getSurface());
            }

            for (ImageReader reader : y8Targets) {
                reprocessOutputs.add(reader.getSurface());
            }

            for (int i = 0; i < NUM_REPROCESS_CAPTURES_PER_CONFIG; i++) {
                for (Surface output : reprocessOutputs) {
                    TotalCaptureResult result = inputCaptureListener.getTotalCaptureResult(
                            TIMEOUT_FOR_RESULT_MS);
                    builder =  mCamera.createReprocessCaptureRequest(result);
                    inputWriter.queueInputImage(
                            inputReaderListener.getImage(TIMEOUT_FOR_RESULT_MS));
                    builder.addTarget(output);
                    reprocessRequests.add(builder.build());
                }
            }

            session.captureBurst(reprocessRequests, reprocessOutputCaptureListener, mHandler);

            for (int i = 0; i < reprocessOutputs.size() * NUM_REPROCESS_CAPTURES_PER_CONFIG; i++) {
                TotalCaptureResult result = reprocessOutputCaptureListener.getTotalCaptureResult(
                        TIMEOUT_FOR_RESULT_MS);
            }
        } catch (Throwable e) {
            mCollector.addMessage(String.format("Reprocess stream combination %s failed due to: %s",
                    combination.getDescription(), e.getMessage()));
        } finally {
            inputReaderListener.drain();
            reprocessOutputCaptureListener.drain();

            for (SurfaceTexture target : privTargets) {
                target.release();
            }

            for (ImageReader target : jpegTargets) {
                target.close();
            }

            for (ImageReader target : yuvTargets) {
                target.close();
            }

            for (ImageReader target : y8Targets) {
                target.close();
            }

            for (ImageReader target : rawTargets) {
                target.close();
            }

            for (ImageReader target : heicTargets) {
                target.close();
            }

            for (ImageReader target : depth16Targets) {
                target.close();
            }

            if (inputReader != null) {
                inputReader.close();
            }

            if (inputWriter != null) {
                inputWriter.close();
            }
        }
    }

    @Test
    public void testBasicTriggerSequence() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s", id));

            try {
                // Legacy devices do not support precapture trigger; don't test devices that
                // can't focus
                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (staticInfo.isHardwareLevelLegacy() || !staticInfo.hasFocuser()) {
                    continue;
                }
                // Depth-only devices won't support AE
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                int[] availableAfModes = mStaticInfo.getAfAvailableModesChecked();
                int[] availableAeModes = mStaticInfo.getAeAvailableModesChecked();

                for (int afMode : availableAfModes) {
                    if (afMode == CameraCharacteristics.CONTROL_AF_MODE_OFF ||
                            afMode == CameraCharacteristics.CONTROL_AF_MODE_EDOF) {
                        // Only test AF modes that have meaningful trigger behavior
                        continue;
                    }

                    for (int aeMode : availableAeModes) {
                        if (aeMode ==  CameraCharacteristics.CONTROL_AE_MODE_OFF) {
                            // Only test AE modes that have meaningful trigger behavior
                            continue;
                        }

                        SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);

                        CaptureRequest.Builder previewRequest =
                                prepareTriggerTestSession(preview, aeMode, afMode);

                        SimpleCaptureCallback captureListener =
                                new CameraTestUtils.SimpleCaptureCallback();

                        mCameraSession.setRepeatingRequest(previewRequest.build(), captureListener,
                                mHandler);

                        // Cancel triggers

                        cancelTriggersAndWait(previewRequest, captureListener, afMode);

                        //
                        // Standard sequence - AF trigger then AE trigger

                        if (VERBOSE) {
                            Log.v(TAG, String.format("Triggering AF"));
                        }

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_START);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_IDLE);

                        CaptureRequest triggerRequest = previewRequest.build();
                        mCameraSession.capture(triggerRequest, captureListener, mHandler);

                        CaptureResult triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        int afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);
                        boolean focusComplete = false;

                        for (int i = 0;
                             i < MAX_TRIGGER_SEQUENCE_FRAMES && !focusComplete;
                             i++) {

                            focusComplete = verifyAfSequence(afMode, afState, focusComplete);

                            CaptureResult focusResult = captureListener.getCaptureResult(
                                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            afState = focusResult.get(CaptureResult.CONTROL_AF_STATE);
                        }

                        assertTrue("Focusing never completed!", focusComplete);

                        // Standard sequence - Part 2 AE trigger

                        if (VERBOSE) {
                            Log.v(TAG, String.format("Triggering AE"));
                        }

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);

                        triggerRequest = previewRequest.build();
                        mCameraSession.capture(triggerRequest, captureListener, mHandler);

                        triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);

                        int aeState = triggerResult.get(CaptureResult.CONTROL_AE_STATE);

                        boolean precaptureComplete = false;

                        for (int i = 0;
                             i < MAX_TRIGGER_SEQUENCE_FRAMES && !precaptureComplete;
                             i++) {

                            precaptureComplete = verifyAeSequence(aeState, precaptureComplete);

                            CaptureResult precaptureResult = captureListener.getCaptureResult(
                                CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            aeState = precaptureResult.get(CaptureResult.CONTROL_AE_STATE);
                        }

                        assertTrue("Precapture sequence never completed!", precaptureComplete);

                        for (int i = 0; i < MAX_RESULT_STATE_POSTCHANGE_WAIT_FRAMES; i++) {
                            CaptureResult postPrecaptureResult = captureListener.getCaptureResult(
                                CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            aeState = postPrecaptureResult.get(CaptureResult.CONTROL_AE_STATE);
                            assertTrue("Late transition to PRECAPTURE state seen",
                                    aeState != CaptureResult.CONTROL_AE_STATE_PRECAPTURE);
                        }

                        // Done

                        stopCapture(/*fast*/ false);
                        preview.release();
                    }

                }

            } finally {
                closeDevice(id);
            }
        }

    }

    @Test
    public void testSimultaneousTriggers() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s", id));

            try {
                // Legacy devices do not support precapture trigger; don't test devices that
                // can't focus
                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (staticInfo.isHardwareLevelLegacy() || !staticInfo.hasFocuser()) {
                    continue;
                }
                // Depth-only devices won't support AE
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                int[] availableAfModes = mStaticInfo.getAfAvailableModesChecked();
                int[] availableAeModes = mStaticInfo.getAeAvailableModesChecked();

                for (int afMode : availableAfModes) {
                    if (afMode == CameraCharacteristics.CONTROL_AF_MODE_OFF ||
                            afMode == CameraCharacteristics.CONTROL_AF_MODE_EDOF) {
                        // Only test AF modes that have meaningful trigger behavior
                        continue;
                    }

                    for (int aeMode : availableAeModes) {
                        if (aeMode ==  CameraCharacteristics.CONTROL_AE_MODE_OFF) {
                            // Only test AE modes that have meaningful trigger behavior
                            continue;
                        }

                        SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);

                        CaptureRequest.Builder previewRequest =
                                prepareTriggerTestSession(preview, aeMode, afMode);

                        SimpleCaptureCallback captureListener =
                                new CameraTestUtils.SimpleCaptureCallback();

                        mCameraSession.setRepeatingRequest(previewRequest.build(), captureListener,
                                mHandler);

                        // Cancel triggers

                        cancelTriggersAndWait(previewRequest, captureListener, afMode);

                        //
                        // Trigger AF and AE together

                        if (VERBOSE) {
                            Log.v(TAG, String.format("Triggering AF and AE together"));
                        }

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_START);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);

                        CaptureRequest triggerRequest = previewRequest.build();
                        mCameraSession.capture(triggerRequest, captureListener, mHandler);

                        CaptureResult triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        int aeState = triggerResult.get(CaptureResult.CONTROL_AE_STATE);
                        int afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);

                        boolean precaptureComplete = false;
                        boolean focusComplete = false;

                        for (int i = 0;
                             i < MAX_TRIGGER_SEQUENCE_FRAMES &&
                                     !(focusComplete && precaptureComplete);
                             i++) {

                            focusComplete = verifyAfSequence(afMode, afState, focusComplete);
                            precaptureComplete = verifyAeSequence(aeState, precaptureComplete);

                            CaptureResult sequenceResult = captureListener.getCaptureResult(
                                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            afState = sequenceResult.get(CaptureResult.CONTROL_AF_STATE);
                            aeState = sequenceResult.get(CaptureResult.CONTROL_AE_STATE);
                        }

                        assertTrue("Precapture sequence never completed!", precaptureComplete);
                        assertTrue("Focus sequence never completed!", focusComplete);

                        // Done

                        stopCapture(/*fast*/ false);
                        preview.release();

                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    @Test
    public void testAfThenAeTrigger() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s", id));

            try {
                // Legacy devices do not support precapture trigger; don't test devices that
                // can't focus
                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (staticInfo.isHardwareLevelLegacy() || !staticInfo.hasFocuser()) {
                    continue;
                }
                // Depth-only devices won't support AE
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                int[] availableAfModes = mStaticInfo.getAfAvailableModesChecked();
                int[] availableAeModes = mStaticInfo.getAeAvailableModesChecked();

                for (int afMode : availableAfModes) {
                    if (afMode == CameraCharacteristics.CONTROL_AF_MODE_OFF ||
                            afMode == CameraCharacteristics.CONTROL_AF_MODE_EDOF) {
                        // Only test AF modes that have meaningful trigger behavior
                        continue;
                    }

                    for (int aeMode : availableAeModes) {
                        if (aeMode ==  CameraCharacteristics.CONTROL_AE_MODE_OFF) {
                            // Only test AE modes that have meaningful trigger behavior
                            continue;
                        }

                        SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);

                        CaptureRequest.Builder previewRequest =
                                prepareTriggerTestSession(preview, aeMode, afMode);

                        SimpleCaptureCallback captureListener =
                                new CameraTestUtils.SimpleCaptureCallback();

                        mCameraSession.setRepeatingRequest(previewRequest.build(), captureListener,
                                mHandler);

                        // Cancel triggers

                        cancelTriggersAndWait(previewRequest, captureListener, afMode);

                        //
                        // AF with AE a request later

                        if (VERBOSE) {
                            Log.v(TAG, "Trigger AF, then AE trigger on next request");
                        }

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_START);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_IDLE);

                        CaptureRequest triggerRequest = previewRequest.build();
                        mCameraSession.capture(triggerRequest, captureListener, mHandler);

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);

                        CaptureRequest triggerRequest2 = previewRequest.build();
                        mCameraSession.capture(triggerRequest2, captureListener, mHandler);

                        CaptureResult triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        int afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);

                        boolean precaptureComplete = false;
                        boolean focusComplete = false;

                        focusComplete = verifyAfSequence(afMode, afState, focusComplete);

                        triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest2, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);
                        int aeState = triggerResult.get(CaptureResult.CONTROL_AE_STATE);

                        for (int i = 0;
                             i < MAX_TRIGGER_SEQUENCE_FRAMES &&
                                     !(focusComplete && precaptureComplete);
                             i++) {

                            focusComplete = verifyAfSequence(afMode, afState, focusComplete);
                            precaptureComplete = verifyAeSequence(aeState, precaptureComplete);

                            CaptureResult sequenceResult = captureListener.getCaptureResult(
                                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            afState = sequenceResult.get(CaptureResult.CONTROL_AF_STATE);
                            aeState = sequenceResult.get(CaptureResult.CONTROL_AE_STATE);
                        }

                        assertTrue("Precapture sequence never completed!", precaptureComplete);
                        assertTrue("Focus sequence never completed!", focusComplete);

                        // Done

                        stopCapture(/*fast*/ false);
                        preview.release();

                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    @Test
    public void testAeThenAfTrigger() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s", id));

            try {
                // Legacy devices do not support precapture trigger; don't test devices that
                // can't focus
                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (staticInfo.isHardwareLevelLegacy() || !staticInfo.hasFocuser()) {
                    continue;
                }
                // Depth-only devices won't support AE
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                int[] availableAfModes = mStaticInfo.getAfAvailableModesChecked();
                int[] availableAeModes = mStaticInfo.getAeAvailableModesChecked();

                for (int afMode : availableAfModes) {
                    if (afMode == CameraCharacteristics.CONTROL_AF_MODE_OFF ||
                            afMode == CameraCharacteristics.CONTROL_AF_MODE_EDOF) {
                        // Only test AF modes that have meaningful trigger behavior
                        continue;
                    }

                    for (int aeMode : availableAeModes) {
                        if (aeMode ==  CameraCharacteristics.CONTROL_AE_MODE_OFF) {
                            // Only test AE modes that have meaningful trigger behavior
                            continue;
                        }

                        SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);

                        CaptureRequest.Builder previewRequest =
                                prepareTriggerTestSession(preview, aeMode, afMode);

                        SimpleCaptureCallback captureListener =
                                new CameraTestUtils.SimpleCaptureCallback();

                        mCameraSession.setRepeatingRequest(previewRequest.build(), captureListener,
                                mHandler);

                        // Cancel triggers

                        cancelTriggersAndWait(previewRequest, captureListener, afMode);

                        //
                        // AE with AF a request later

                        if (VERBOSE) {
                            Log.v(TAG, "Trigger AE, then AF trigger on next request");
                        }

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);

                        CaptureRequest triggerRequest = previewRequest.build();
                        mCameraSession.capture(triggerRequest, captureListener, mHandler);

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_START);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_IDLE);

                        CaptureRequest triggerRequest2 = previewRequest.build();
                        mCameraSession.capture(triggerRequest2, captureListener, mHandler);

                        CaptureResult triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        int aeState = triggerResult.get(CaptureResult.CONTROL_AE_STATE);

                        boolean precaptureComplete = false;
                        boolean focusComplete = false;

                        precaptureComplete = verifyAeSequence(aeState, precaptureComplete);

                        triggerResult = captureListener.getCaptureResultForRequest(
                                triggerRequest2, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        int afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);
                        aeState = triggerResult.get(CaptureResult.CONTROL_AE_STATE);

                        for (int i = 0;
                             i < MAX_TRIGGER_SEQUENCE_FRAMES &&
                                     !(focusComplete && precaptureComplete);
                             i++) {

                            focusComplete = verifyAfSequence(afMode, afState, focusComplete);
                            precaptureComplete = verifyAeSequence(aeState, precaptureComplete);

                            CaptureResult sequenceResult = captureListener.getCaptureResult(
                                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            afState = sequenceResult.get(CaptureResult.CONTROL_AF_STATE);
                            aeState = sequenceResult.get(CaptureResult.CONTROL_AE_STATE);
                        }

                        assertTrue("Precapture sequence never completed!", precaptureComplete);
                        assertTrue("Focus sequence never completed!", focusComplete);

                        // Done

                        stopCapture(/*fast*/ false);
                        preview.release();

                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    @Test
    public void testAeAndAfCausality() throws Exception {

        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s", id));

            try {
                // Legacy devices do not support precapture trigger; don't test devices that
                // can't focus
                StaticMetadata staticInfo = mAllStaticInfo.get(id);
                if (staticInfo.isHardwareLevelLegacy() || !staticInfo.hasFocuser()) {
                    continue;
                }
                // Depth-only devices won't support AE
                if (!staticInfo.isColorOutputSupported()) {
                    Log.i(TAG, "Camera " + id + " does not support color outputs, skipping");
                    continue;
                }

                openDevice(id);
                int[] availableAfModes = mStaticInfo.getAfAvailableModesChecked();
                int[] availableAeModes = mStaticInfo.getAeAvailableModesChecked();
                final int maxPipelineDepth = mStaticInfo.getCharacteristics().get(
                        CameraCharacteristics.REQUEST_PIPELINE_MAX_DEPTH);

                for (int afMode : availableAfModes) {
                    if (afMode == CameraCharacteristics.CONTROL_AF_MODE_OFF ||
                            afMode == CameraCharacteristics.CONTROL_AF_MODE_EDOF) {
                        // Only test AF modes that have meaningful trigger behavior
                        continue;
                    }
                    for (int aeMode : availableAeModes) {
                        if (aeMode ==  CameraCharacteristics.CONTROL_AE_MODE_OFF) {
                            // Only test AE modes that have meaningful trigger behavior
                            continue;
                        }

                        SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);

                        CaptureRequest.Builder previewRequest =
                                prepareTriggerTestSession(preview, aeMode, afMode);

                        SimpleCaptureCallback captureListener =
                                new CameraTestUtils.SimpleCaptureCallback();

                        mCameraSession.setRepeatingRequest(previewRequest.build(), captureListener,
                                mHandler);

                        List<CaptureRequest> triggerRequests =
                                new ArrayList<CaptureRequest>(maxPipelineDepth+1);
                        for (int i = 0; i < maxPipelineDepth; i++) {
                            triggerRequests.add(previewRequest.build());
                        }

                        // Cancel triggers
                        cancelTriggersAndWait(previewRequest, captureListener, afMode);

                        //
                        // Standard sequence - Part 1 AF trigger

                        if (VERBOSE) {
                            Log.v(TAG, String.format("Triggering AF"));
                        }

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_START);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_IDLE);
                        triggerRequests.add(previewRequest.build());

                        mCameraSession.captureBurst(triggerRequests, captureListener, mHandler);

                        TotalCaptureResult[] triggerResults =
                                captureListener.getTotalCaptureResultsForRequests(
                                triggerRequests, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        for (int i = 0; i < maxPipelineDepth; i++) {
                            TotalCaptureResult triggerResult = triggerResults[i];
                            int afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);
                            int afTrigger = triggerResult.get(CaptureResult.CONTROL_AF_TRIGGER);

                            verifyStartingAfState(afMode, afState);
                            assertTrue(String.format("In AF mode %s, previous AF_TRIGGER must not "
                                    + "be START before TRIGGER_START",
                                    StaticMetadata.getAfModeName(afMode)),
                                    afTrigger != CaptureResult.CONTROL_AF_TRIGGER_START);
                        }

                        int afState =
                                triggerResults[maxPipelineDepth].get(CaptureResult.CONTROL_AF_STATE);
                        boolean focusComplete = false;
                        for (int i = 0;
                             i < MAX_TRIGGER_SEQUENCE_FRAMES && !focusComplete;
                             i++) {

                            focusComplete = verifyAfSequence(afMode, afState, focusComplete);

                            CaptureResult focusResult = captureListener.getCaptureResult(
                                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
                            afState = focusResult.get(CaptureResult.CONTROL_AF_STATE);
                        }

                        assertTrue("Focusing never completed!", focusComplete);

                        // Standard sequence - Part 2 AE trigger

                        if (VERBOSE) {
                            Log.v(TAG, String.format("Triggering AE"));
                        }
                        // Remove AF trigger request
                        triggerRequests.remove(maxPipelineDepth);

                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_IDLE);
                        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START);
                        triggerRequests.add(previewRequest.build());

                        mCameraSession.captureBurst(triggerRequests, captureListener, mHandler);

                        triggerResults = captureListener.getTotalCaptureResultsForRequests(
                                triggerRequests, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);

                        for (int i = 0; i < maxPipelineDepth; i++) {
                            TotalCaptureResult triggerResult = triggerResults[i];
                            int aeState = triggerResult.get(CaptureResult.CONTROL_AE_STATE);
                            int aeTrigger = triggerResult.get(
                                    CaptureResult.CONTROL_AE_PRECAPTURE_TRIGGER);

                            assertTrue(String.format("In AE mode %s, previous AE_TRIGGER must not "
                                    + "be START before TRIGGER_START",
                                    StaticMetadata.getAeModeName(aeMode)),
                                    aeTrigger != CaptureResult.CONTROL_AE_PRECAPTURE_TRIGGER_START);
                            assertTrue(String.format("In AE mode %s, previous AE_STATE must not be"
                                    + " PRECAPTURE_TRIGGER before TRIGGER_START",
                                    StaticMetadata.getAeModeName(aeMode)),
                                    aeState != CaptureResult.CONTROL_AE_STATE_PRECAPTURE);
                        }

                        // Stand sequence - Part 3 Cancel AF trigger
                        if (VERBOSE) {
                            Log.v(TAG, String.format("Cancel AF trigger"));
                        }
                        // Remove AE trigger request
                        triggerRequests.remove(maxPipelineDepth);
                        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                                CaptureRequest.CONTROL_AF_TRIGGER_CANCEL);
                        triggerRequests.add(previewRequest.build());

                        mCameraSession.captureBurst(triggerRequests, captureListener, mHandler);
                        triggerResults = captureListener.getTotalCaptureResultsForRequests(
                                triggerRequests, MAX_RESULT_STATE_CHANGE_WAIT_FRAMES);
                        for (int i = 0; i < maxPipelineDepth; i++) {
                            TotalCaptureResult triggerResult = triggerResults[i];
                            afState = triggerResult.get(CaptureResult.CONTROL_AF_STATE);
                            int afTrigger = triggerResult.get(CaptureResult.CONTROL_AF_TRIGGER);

                            assertTrue(
                                    String.format("In AF mode %s, previous AF_TRIGGER must not " +
                                    "be CANCEL before TRIGGER_CANCEL",
                                    StaticMetadata.getAfModeName(afMode)),
                                    afTrigger != CaptureResult.CONTROL_AF_TRIGGER_CANCEL);
                            assertTrue(
                                    String.format("In AF mode %s, previous AF_STATE must be LOCKED"
                                    + " before CANCEL, but is %s",
                                    StaticMetadata.getAfModeName(afMode),
                                    StaticMetadata.AF_STATE_NAMES[afState]),
                                    afState == CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED ||
                                    afState == CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED);
                        }

                        stopCapture(/*fast*/ false);
                        preview.release();
                    }

                }

            } finally {
                closeDevice(id);
            }
        }

    }

    @Test
    public void testAbandonRepeatingRequestSurface() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format(
                    "Testing Camera %s for abandoning surface of a repeating request", id));

            StaticMetadata staticInfo = mAllStaticInfo.get(id);
            if (!staticInfo.isColorOutputSupported()) {
                Log.i(TAG, "Camera " + id + " does not support color output, skipping");
                continue;
            }

            openDevice(id);

            try {

                SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);
                Surface previewSurface = new Surface(preview);

                CaptureRequest.Builder previewRequest = preparePreviewTestSession(preview);
                SimpleCaptureCallback captureListener = new CameraTestUtils.SimpleCaptureCallback();

                int sequenceId = mCameraSession.setRepeatingRequest(previewRequest.build(),
                        captureListener, mHandler);

                for (int i = 0; i < PREVIEW_WARMUP_FRAMES; i++) {
                    captureListener.getTotalCaptureResult(CAPTURE_TIMEOUT);
                }

                // Abandon preview surface.
                preview.release();

                // Check onCaptureSequenceCompleted is received.
                long sequenceLastFrameNumber = captureListener.getCaptureSequenceLastFrameNumber(
                        sequenceId, CAPTURE_TIMEOUT);

                mCameraSession.stopRepeating();

                // Find the last frame number received in results and failures.
                long lastFrameNumber = -1;
                while (captureListener.hasMoreResults()) {
                    TotalCaptureResult result = captureListener.getTotalCaptureResult(
                            CAPTURE_TIMEOUT);
                    if (lastFrameNumber < result.getFrameNumber()) {
                        lastFrameNumber = result.getFrameNumber();
                    }
                }

                while (captureListener.hasMoreFailures()) {
                    ArrayList<CaptureFailure> failures = captureListener.getCaptureFailures(
                            /*maxNumFailures*/ 1);
                    for (CaptureFailure failure : failures) {
                        if (lastFrameNumber < failure.getFrameNumber()) {
                            lastFrameNumber = failure.getFrameNumber();
                        }
                    }
                }

                // Verify the last frame number received from capture sequence completed matches the
                // the last frame number of the results and failures.
                assertEquals(String.format("Last frame number from onCaptureSequenceCompleted " +
                        "(%d) doesn't match the last frame number received from " +
                        "results/failures (%d)", sequenceLastFrameNumber, lastFrameNumber),
                        sequenceLastFrameNumber, lastFrameNumber);
            } finally {
                closeDevice(id);
            }
        }
    }

    @Test
    public void testConfigureAbandonedSurface() throws Exception {
        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format(
                    "Testing Camera %s for configuring abandoned surface", id));

            openDevice(id);
            try {
                SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);
                Surface previewSurface = new Surface(preview);

                // Abandon preview SurfaceTexture.
                preview.release();

                try {
                    CaptureRequest.Builder previewRequest = preparePreviewTestSession(preview);
                    fail("Configuring abandoned surfaces must fail!");
                } catch (IllegalArgumentException e) {
                    // expected
                    Log.i(TAG, "normal session check passed");
                }

                // Try constrained high speed session/requests
                if (!mStaticInfo.isConstrainedHighSpeedVideoSupported()) {
                    continue;
                }

                List<Surface> surfaces = new ArrayList<>();
                surfaces.add(previewSurface);
                CameraCaptureSession.StateCallback sessionListener =
                        mock(CameraCaptureSession.StateCallback.class);

                try {
                    mCamera.createConstrainedHighSpeedCaptureSession(surfaces,
                            sessionListener, mHandler);
                    fail("Configuring abandoned surfaces in high speed session must fail!");
                } catch (IllegalArgumentException e) {
                    // expected
                    Log.i(TAG, "high speed session check 1 passed");
                }

                // Also try abandone the Surface directly
                previewSurface.release();

                try {
                    mCamera.createConstrainedHighSpeedCaptureSession(surfaces,
                            sessionListener, mHandler);
                    fail("Configuring abandoned surfaces in high speed session must fail!");
                } catch (IllegalArgumentException e) {
                    // expected
                    Log.i(TAG, "high speed session check 2 passed");
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    @Test
    public void testAfSceneChange() throws Exception {
        final int NUM_FRAMES_VERIFIED = 3;

        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s for AF scene change", id));

            StaticMetadata staticInfo =
                    new StaticMetadata(mCameraManager.getCameraCharacteristics(id));
            if (!staticInfo.isAfSceneChangeSupported()) {
                continue;
            }

            openDevice(id);

            try {
                SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);
                Surface previewSurface = new Surface(preview);

                CaptureRequest.Builder previewRequest = preparePreviewTestSession(preview);
                SimpleCaptureCallback previewListener = new CameraTestUtils.SimpleCaptureCallback();

                int[] availableAfModes = mStaticInfo.getAfAvailableModesChecked();

                // Test AF scene change in each AF mode.
                for (int afMode : availableAfModes) {
                    previewRequest.set(CaptureRequest.CONTROL_AF_MODE, afMode);

                    int sequenceId = mCameraSession.setRepeatingRequest(previewRequest.build(),
                            previewListener, mHandler);

                    // Verify that AF scene change is NOT_DETECTED or DETECTED.
                    for (int i = 0; i < NUM_FRAMES_VERIFIED; i++) {
                        TotalCaptureResult result =
                            previewListener.getTotalCaptureResult(CAPTURE_TIMEOUT);
                        mCollector.expectKeyValueIsIn(result,
                                CaptureResult.CONTROL_AF_SCENE_CHANGE,
                                CaptureResult.CONTROL_AF_SCENE_CHANGE_DETECTED,
                                CaptureResult.CONTROL_AF_SCENE_CHANGE_NOT_DETECTED);
                    }

                    mCameraSession.stopRepeating();
                    previewListener.getCaptureSequenceLastFrameNumber(sequenceId, CAPTURE_TIMEOUT);
                    previewListener.drain();
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    @Test
    public void testOisDataMode() throws Exception {
        final int NUM_FRAMES_VERIFIED = 3;

        for (String id : mCameraIdsUnderTest) {
            Log.i(TAG, String.format("Testing Camera %s for OIS mode", id));

            StaticMetadata staticInfo =
                    new StaticMetadata(mCameraManager.getCameraCharacteristics(id));
            if (!staticInfo.isOisDataModeSupported()) {
                continue;
            }

            openDevice(id);

            try {
                SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);
                Surface previewSurface = new Surface(preview);

                CaptureRequest.Builder previewRequest = preparePreviewTestSession(preview);
                SimpleCaptureCallback previewListener = new CameraTestUtils.SimpleCaptureCallback();

                int[] availableOisDataModes = staticInfo.getCharacteristics().get(
                        CameraCharacteristics.STATISTICS_INFO_AVAILABLE_OIS_DATA_MODES);

                // Test each OIS data mode
                for (int oisMode : availableOisDataModes) {
                    previewRequest.set(CaptureRequest.STATISTICS_OIS_DATA_MODE, oisMode);

                    int sequenceId = mCameraSession.setRepeatingRequest(previewRequest.build(),
                            previewListener, mHandler);

                    // Check OIS data in each mode.
                    for (int i = 0; i < NUM_FRAMES_VERIFIED; i++) {
                        TotalCaptureResult result =
                            previewListener.getTotalCaptureResult(CAPTURE_TIMEOUT);

                        OisSample[] oisSamples = result.get(CaptureResult.STATISTICS_OIS_SAMPLES);

                        if (oisMode == CameraCharacteristics.STATISTICS_OIS_DATA_MODE_OFF) {
                            mCollector.expectKeyValueEquals(result,
                                    CaptureResult.STATISTICS_OIS_DATA_MODE,
                                    CaptureResult.STATISTICS_OIS_DATA_MODE_OFF);
                            mCollector.expectTrue("OIS samples reported in OIS_DATA_MODE_OFF",
                                    oisSamples == null || oisSamples.length == 0);

                        } else if (oisMode == CameraCharacteristics.STATISTICS_OIS_DATA_MODE_ON) {
                            mCollector.expectKeyValueEquals(result,
                                    CaptureResult.STATISTICS_OIS_DATA_MODE,
                                    CaptureResult.STATISTICS_OIS_DATA_MODE_ON);
                            mCollector.expectTrue("OIS samples not reported in OIS_DATA_MODE_ON",
                                    oisSamples != null && oisSamples.length != 0);
                        } else {
                            mCollector.addMessage(String.format("Invalid OIS mode: %d", oisMode));
                        }
                    }

                    mCameraSession.stopRepeating();
                    previewListener.getCaptureSequenceLastFrameNumber(sequenceId, CAPTURE_TIMEOUT);
                    previewListener.drain();
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    private CaptureRequest.Builder preparePreviewTestSession(SurfaceTexture preview)
            throws Exception {
        Surface previewSurface = new Surface(preview);

        preview.setDefaultBufferSize(640, 480);

        ArrayList<Surface> sessionOutputs = new ArrayList<>();
        sessionOutputs.add(previewSurface);

        createSession(sessionOutputs);

        CaptureRequest.Builder previewRequest =
                mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);

        previewRequest.addTarget(previewSurface);

        return previewRequest;
    }

    private CaptureRequest.Builder prepareTriggerTestSession(
            SurfaceTexture preview, int aeMode, int afMode) throws Exception {
        Log.i(TAG, String.format("Testing AE mode %s, AF mode %s",
                        StaticMetadata.getAeModeName(aeMode),
                        StaticMetadata.getAfModeName(afMode)));

        CaptureRequest.Builder previewRequest = preparePreviewTestSession(preview);
        previewRequest.set(CaptureRequest.CONTROL_AE_MODE, aeMode);
        previewRequest.set(CaptureRequest.CONTROL_AF_MODE, afMode);

        return previewRequest;
    }

    private void cancelTriggersAndWait(CaptureRequest.Builder previewRequest,
            SimpleCaptureCallback captureListener, int afMode) throws Exception {
        previewRequest.set(CaptureRequest.CONTROL_AF_TRIGGER,
                CaptureRequest.CONTROL_AF_TRIGGER_CANCEL);
        previewRequest.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER,
                CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL);

        CaptureRequest triggerRequest = previewRequest.build();
        mCameraSession.capture(triggerRequest, captureListener, mHandler);

        // Wait for a few frames to initialize 3A

        CaptureResult previewResult = null;
        int afState;
        int aeState;

        for (int i = 0; i < PREVIEW_WARMUP_FRAMES; i++) {
            previewResult = captureListener.getCaptureResult(
                    CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS);
            if (VERBOSE) {
                afState = previewResult.get(CaptureResult.CONTROL_AF_STATE);
                aeState = previewResult.get(CaptureResult.CONTROL_AE_STATE);
                Log.v(TAG, String.format("AF state: %s, AE state: %s",
                                StaticMetadata.AF_STATE_NAMES[afState],
                                StaticMetadata.AE_STATE_NAMES[aeState]));
            }
        }

        // Verify starting states

        afState = previewResult.get(CaptureResult.CONTROL_AF_STATE);
        aeState = previewResult.get(CaptureResult.CONTROL_AE_STATE);

        verifyStartingAfState(afMode, afState);

        // After several frames, AE must no longer be in INACTIVE state
        assertTrue(String.format("AE state must be SEARCHING, CONVERGED, " +
                        "or FLASH_REQUIRED, is %s", StaticMetadata.AE_STATE_NAMES[aeState]),
                aeState == CaptureResult.CONTROL_AE_STATE_SEARCHING ||
                aeState == CaptureResult.CONTROL_AE_STATE_CONVERGED ||
                aeState == CaptureResult.CONTROL_AE_STATE_FLASH_REQUIRED);
    }

    private void verifyStartingAfState(int afMode, int afState) {
        switch (afMode) {
            case CaptureResult.CONTROL_AF_MODE_AUTO:
            case CaptureResult.CONTROL_AF_MODE_MACRO:
                assertTrue(String.format("AF state not INACTIVE, is %s",
                                StaticMetadata.AF_STATE_NAMES[afState]),
                        afState == CaptureResult.CONTROL_AF_STATE_INACTIVE);
                break;
            case CaptureResult.CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            case CaptureResult.CONTROL_AF_MODE_CONTINUOUS_VIDEO:
                // After several frames, AF must no longer be in INACTIVE state
                assertTrue(String.format("In AF mode %s, AF state not PASSIVE_SCAN" +
                                ", PASSIVE_FOCUSED, or PASSIVE_UNFOCUSED, is %s",
                                StaticMetadata.getAfModeName(afMode),
                                StaticMetadata.AF_STATE_NAMES[afState]),
                        afState == CaptureResult.CONTROL_AF_STATE_PASSIVE_SCAN ||
                        afState == CaptureResult.CONTROL_AF_STATE_PASSIVE_FOCUSED ||
                        afState == CaptureResult.CONTROL_AF_STATE_PASSIVE_UNFOCUSED);
                break;
            default:
                fail("unexpected af mode");
        }
    }

    private boolean verifyAfSequence(int afMode, int afState, boolean focusComplete) {
        if (focusComplete) {
            assertTrue(String.format("AF Mode %s: Focus lock lost after convergence: AF state: %s",
                            StaticMetadata.getAfModeName(afMode),
                            StaticMetadata.AF_STATE_NAMES[afState]),
                    afState == CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED ||
                    afState ==CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED);
            return focusComplete;
        }
        if (VERBOSE) {
            Log.v(TAG, String.format("AF mode: %s, AF state: %s",
                            StaticMetadata.getAfModeName(afMode),
                            StaticMetadata.AF_STATE_NAMES[afState]));
        }
        switch (afMode) {
            case CaptureResult.CONTROL_AF_MODE_AUTO:
            case CaptureResult.CONTROL_AF_MODE_MACRO:
                assertTrue(String.format("AF mode %s: Unexpected AF state %s",
                                StaticMetadata.getAfModeName(afMode),
                                StaticMetadata.AF_STATE_NAMES[afState]),
                        afState == CaptureResult.CONTROL_AF_STATE_ACTIVE_SCAN ||
                        afState == CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED ||
                        afState == CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED);
                focusComplete =
                        (afState != CaptureResult.CONTROL_AF_STATE_ACTIVE_SCAN);
                break;
            case CaptureResult.CONTROL_AF_MODE_CONTINUOUS_PICTURE:
                assertTrue(String.format("AF mode %s: Unexpected AF state %s",
                                StaticMetadata.getAfModeName(afMode),
                                StaticMetadata.AF_STATE_NAMES[afState]),
                        afState == CaptureResult.CONTROL_AF_STATE_PASSIVE_SCAN ||
                        afState == CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED ||
                        afState == CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED);
                focusComplete =
                        (afState != CaptureResult.CONTROL_AF_STATE_PASSIVE_SCAN);
                break;
            case CaptureResult.CONTROL_AF_MODE_CONTINUOUS_VIDEO:
                assertTrue(String.format("AF mode %s: Unexpected AF state %s",
                                StaticMetadata.getAfModeName(afMode),
                                StaticMetadata.AF_STATE_NAMES[afState]),
                        afState == CaptureResult.CONTROL_AF_STATE_FOCUSED_LOCKED ||
                        afState == CaptureResult.CONTROL_AF_STATE_NOT_FOCUSED_LOCKED);
                focusComplete = true;
                break;
            default:
                fail("Unexpected AF mode: " + StaticMetadata.getAfModeName(afMode));
        }
        return focusComplete;
    }

    private boolean verifyAeSequence(int aeState, boolean precaptureComplete) {
        if (precaptureComplete) {
            assertTrue("Precapture state seen after convergence",
                    aeState != CaptureResult.CONTROL_AE_STATE_PRECAPTURE);
            return precaptureComplete;
        }
        if (VERBOSE) {
            Log.v(TAG, String.format("AE state: %s", StaticMetadata.AE_STATE_NAMES[aeState]));
        }
        switch (aeState) {
            case CaptureResult.CONTROL_AE_STATE_PRECAPTURE:
                // scan still continuing
                break;
            case CaptureResult.CONTROL_AE_STATE_CONVERGED:
            case CaptureResult.CONTROL_AE_STATE_FLASH_REQUIRED:
                // completed
                precaptureComplete = true;
                break;
            default:
                fail(String.format("Precapture sequence transitioned to "
                                + "state %s incorrectly!", StaticMetadata.AE_STATE_NAMES[aeState]));
                break;
        }
        return precaptureComplete;
    }

    /**
     * Test for making sure that all expected mandatory stream combinations are present and
     * advertised accordingly.
     */
    @Test
    public void testVerifyMandatoryOutputCombinationTables() throws Exception {
       final int[][] LEGACY_COMBINATIONS = {
            // Simple preview, GPU video processing, or no-preview video recording
            {PRIV, MAXIMUM},
            // No-viewfinder still image capture
            {JPEG, MAXIMUM},
            // In-application video/image processing
            {YUV,  MAXIMUM},
            // Standard still imaging.
            {PRIV, PREVIEW,  JPEG, MAXIMUM},
            // In-app processing plus still capture.
            {YUV,  PREVIEW,  JPEG, MAXIMUM},
            // Standard recording.
            {PRIV, PREVIEW,  PRIV, PREVIEW},
            // Preview plus in-app processing.
            {PRIV, PREVIEW,  YUV,  PREVIEW},
            // Still capture plus in-app processing.
            {PRIV, PREVIEW,  YUV,  PREVIEW,  JPEG, MAXIMUM}
        };

        final int[][] LIMITED_COMBINATIONS = {
            // High-resolution video recording with preview.
            {PRIV, PREVIEW,  PRIV, RECORD },
            // High-resolution in-app video processing with preview.
            {PRIV, PREVIEW,  YUV , RECORD },
            // Two-input in-app video processing.
            {YUV , PREVIEW,  YUV , RECORD },
            // High-resolution recording with video snapshot.
            {PRIV, PREVIEW,  PRIV, RECORD,   JPEG, RECORD  },
            // High-resolution in-app processing with video snapshot.
            {PRIV, PREVIEW,  YUV,  RECORD,   JPEG, RECORD  },
            // Two-input in-app processing with still capture.
            {YUV , PREVIEW,  YUV,  PREVIEW,  JPEG, MAXIMUM }
        };

        final int[][] BURST_COMBINATIONS = {
            // Maximum-resolution GPU processing with preview.
            {PRIV, PREVIEW,  PRIV, MAXIMUM },
            // Maximum-resolution in-app processing with preview.
            {PRIV, PREVIEW,  YUV,  MAXIMUM },
            // Maximum-resolution two-input in-app processing.
            {YUV,  PREVIEW,  YUV,  MAXIMUM },
        };

        final int[][] FULL_COMBINATIONS = {
            // Video recording with maximum-size video snapshot.
            {PRIV, PREVIEW,  PRIV, PREVIEW,  JPEG, MAXIMUM },
            // Standard video recording plus maximum-resolution in-app processing.
            {YUV,  VGA,      PRIV, PREVIEW,  YUV,  MAXIMUM },
            // Preview plus two-input maximum-resolution in-app processing.
            {YUV,  VGA,      YUV,  PREVIEW,  YUV,  MAXIMUM }
        };

        final int[][] RAW_COMBINATIONS = {
            // No-preview DNG capture.
            {RAW,  MAXIMUM },
            // Standard DNG capture.
            {PRIV, PREVIEW,  RAW,  MAXIMUM },
            // In-app processing plus DNG capture.
            {YUV,  PREVIEW,  RAW,  MAXIMUM },
            // Video recording with DNG capture.
            {PRIV, PREVIEW,  PRIV, PREVIEW,  RAW, MAXIMUM},
            // Preview with in-app processing and DNG capture.
            {PRIV, PREVIEW,  YUV,  PREVIEW,  RAW, MAXIMUM},
            // Two-input in-app processing plus DNG capture.
            {YUV,  PREVIEW,  YUV,  PREVIEW,  RAW, MAXIMUM},
            // Still capture with simultaneous JPEG and DNG.
            {PRIV, PREVIEW,  JPEG, MAXIMUM,  RAW, MAXIMUM},
            // In-app processing with simultaneous JPEG and DNG.
            {YUV,  PREVIEW,  JPEG, MAXIMUM,  RAW, MAXIMUM}
        };

        final int[][] LEVEL_3_COMBINATIONS = {
            // In-app viewfinder analysis with dynamic selection of output format
            {PRIV, PREVIEW, PRIV, VGA, YUV, MAXIMUM, RAW, MAXIMUM},
            // In-app viewfinder analysis with dynamic selection of output format
            {PRIV, PREVIEW, PRIV, VGA, JPEG, MAXIMUM, RAW, MAXIMUM}
        };

        final int[][][] TABLES =
                { LEGACY_COMBINATIONS, LIMITED_COMBINATIONS, BURST_COMBINATIONS, FULL_COMBINATIONS,
                  RAW_COMBINATIONS, LEVEL_3_COMBINATIONS };

        validityCheckConfigurationTables(TABLES);

        for (String id : mCameraIdsUnderTest) {
            openDevice(id);
            MandatoryStreamCombination[] combinations =
                    mStaticInfo.getCharacteristics().get(
                            CameraCharacteristics.SCALER_MANDATORY_STREAM_COMBINATIONS);
            if ((combinations == null) || (combinations.length == 0)) {
                Log.i(TAG, "No mandatory stream combinations for camera: " + id + " skip test");
                closeDevice(id);
                continue;
            }

            MaxStreamSizes maxSizes = new MaxStreamSizes(mStaticInfo, id, mContext);
            try {
                if (mStaticInfo.isColorOutputSupported()) {
                    for (int[] c : LEGACY_COMBINATIONS) {
                        assertTrue(String.format("Expected static stream combination: %s not " +
                                    "found among the available mandatory combinations",
                                    maxSizes.combinationToString(c)),
                                isMandatoryCombinationAvailable(c, maxSizes, combinations));
                    }
                }

                if (!mStaticInfo.isHardwareLevelLegacy()) {
                    if (mStaticInfo.isColorOutputSupported()) {
                        for (int[] c : LIMITED_COMBINATIONS) {
                            assertTrue(String.format("Expected static stream combination: %s not " +
                                        "found among the available mandatory combinations",
                                        maxSizes.combinationToString(c)),
                                    isMandatoryCombinationAvailable(c, maxSizes, combinations));
                        }
                    }

                    if (mStaticInfo.isCapabilitySupported(
                            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE)) {
                        for (int[] c : BURST_COMBINATIONS) {
                            assertTrue(String.format("Expected static stream combination: %s not " +
                                        "found among the available mandatory combinations",
                                        maxSizes.combinationToString(c)),
                                    isMandatoryCombinationAvailable(c, maxSizes, combinations));
                        }
                    }

                    if (mStaticInfo.isHardwareLevelAtLeastFull()) {
                        for (int[] c : FULL_COMBINATIONS) {
                            assertTrue(String.format("Expected static stream combination: %s not " +
                                        "found among the available mandatory combinations",
                                        maxSizes.combinationToString(c)),
                                    isMandatoryCombinationAvailable(c, maxSizes, combinations));
                        }
                    }

                    if (mStaticInfo.isCapabilitySupported(
                            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
                        for (int[] c : RAW_COMBINATIONS) {
                            assertTrue(String.format("Expected static stream combination: %s not " +
                                        "found among the available mandatory combinations",
                                        maxSizes.combinationToString(c)),
                                    isMandatoryCombinationAvailable(c, maxSizes, combinations));
                        }
                    }

                    if (mStaticInfo.isHardwareLevelAtLeast(
                            CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_3)) {
                        for (int[] c: LEVEL_3_COMBINATIONS) {
                            assertTrue(String.format("Expected static stream combination: %s not " +
                                        "found among the available mandatory combinations",
                                        maxSizes.combinationToString(c)),
                                    isMandatoryCombinationAvailable(c, maxSizes, combinations));
                        }
                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    /**
     * Test for making sure that all expected reprocessable mandatory stream combinations are
     * present and advertised accordingly.
     */
    @Test
    public void testVerifyReprocessMandatoryOutputCombinationTables() throws Exception {
        final int[][] LIMITED_COMBINATIONS = {
            // Input           Outputs
            {PRIV, MAXIMUM,    JPEG, MAXIMUM},
            {YUV , MAXIMUM,    JPEG, MAXIMUM},
            {PRIV, MAXIMUM,    PRIV, PREVIEW, JPEG, MAXIMUM},
            {YUV , MAXIMUM,    PRIV, PREVIEW, JPEG, MAXIMUM},
            {PRIV, MAXIMUM,    YUV , PREVIEW, JPEG, MAXIMUM},
            {YUV , MAXIMUM,    YUV , PREVIEW, JPEG, MAXIMUM},
            {PRIV, MAXIMUM,    YUV , PREVIEW, YUV , PREVIEW, JPEG, MAXIMUM},
            {YUV,  MAXIMUM,    YUV , PREVIEW, YUV , PREVIEW, JPEG, MAXIMUM},
        };

        final int[][] FULL_COMBINATIONS = {
            // Input           Outputs
            {YUV , MAXIMUM,    PRIV, PREVIEW},
            {YUV , MAXIMUM,    YUV , PREVIEW},
            {PRIV, MAXIMUM,    PRIV, PREVIEW, YUV , RECORD},
            {YUV , MAXIMUM,    PRIV, PREVIEW, YUV , RECORD},
            {PRIV, MAXIMUM,    PRIV, PREVIEW, YUV , MAXIMUM},
            {PRIV, MAXIMUM,    YUV , PREVIEW, YUV , MAXIMUM},
            {PRIV, MAXIMUM,    PRIV, PREVIEW, YUV , PREVIEW, JPEG, MAXIMUM},
            {YUV , MAXIMUM,    PRIV, PREVIEW, YUV , PREVIEW, JPEG, MAXIMUM},
        };

        final int[][] RAW_COMBINATIONS = {
            // Input           Outputs
            {PRIV, MAXIMUM,    YUV , PREVIEW, RAW , MAXIMUM},
            {YUV , MAXIMUM,    YUV , PREVIEW, RAW , MAXIMUM},
            {PRIV, MAXIMUM,    PRIV, PREVIEW, YUV , PREVIEW, RAW , MAXIMUM},
            {YUV , MAXIMUM,    PRIV, PREVIEW, YUV , PREVIEW, RAW , MAXIMUM},
            {PRIV, MAXIMUM,    YUV , PREVIEW, YUV , PREVIEW, RAW , MAXIMUM},
            {YUV , MAXIMUM,    YUV , PREVIEW, YUV , PREVIEW, RAW , MAXIMUM},
            {PRIV, MAXIMUM,    PRIV, PREVIEW, JPEG, MAXIMUM, RAW , MAXIMUM},
            {YUV , MAXIMUM,    PRIV, PREVIEW, JPEG, MAXIMUM, RAW , MAXIMUM},
            {PRIV, MAXIMUM,    YUV , PREVIEW, JPEG, MAXIMUM, RAW , MAXIMUM},
            {YUV , MAXIMUM,    YUV , PREVIEW, JPEG, MAXIMUM, RAW , MAXIMUM},
        };

        final int[][] LEVEL_3_COMBINATIONS = {
            // Input          Outputs
            // In-app viewfinder analysis with YUV->YUV ZSL and RAW
            {YUV , MAXIMUM,   PRIV, PREVIEW, PRIV, VGA, RAW, MAXIMUM},
            // In-app viewfinder analysis with PRIV->JPEG ZSL and RAW
            {PRIV, MAXIMUM,   PRIV, PREVIEW, PRIV, VGA, RAW, MAXIMUM, JPEG, MAXIMUM},
            // In-app viewfinder analysis with YUV->JPEG ZSL and RAW
            {YUV , MAXIMUM,   PRIV, PREVIEW, PRIV, VGA, RAW, MAXIMUM, JPEG, MAXIMUM},
        };

        final int[][][] TABLES =
                { LIMITED_COMBINATIONS, FULL_COMBINATIONS, RAW_COMBINATIONS, LEVEL_3_COMBINATIONS };

        validityCheckConfigurationTables(TABLES);

        for (String id : mCameraIdsUnderTest) {
            openDevice(id);
            MandatoryStreamCombination[] cs = mStaticInfo.getCharacteristics().get(
                    CameraCharacteristics.SCALER_MANDATORY_STREAM_COMBINATIONS);
            if ((cs == null) || (cs.length == 0)) {
                Log.i(TAG, "No mandatory stream combinations for camera: " + id + " skip test");
                closeDevice(id);
                continue;
            }

            boolean supportYuvReprocess = mStaticInfo.isCapabilitySupported(
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
            boolean supportOpaqueReprocess = mStaticInfo.isCapabilitySupported(
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING);
            if (!supportYuvReprocess && !supportOpaqueReprocess) {
                Log.i(TAG, "No reprocess support for camera: " + id + " skip test");
                closeDevice(id);
                continue;
            }

            MaxStreamSizes maxSizes = new MaxStreamSizes(mStaticInfo, id, mContext);
            try {
                for (int[] c : LIMITED_COMBINATIONS) {
                    assertTrue(String.format("Expected static reprocessable stream combination:" +
                                "%s not found among the available mandatory combinations",
                                maxSizes.reprocessCombinationToString(c)),
                            isMandatoryCombinationAvailable(c, maxSizes, /*isInput*/ true, cs));
                }

                if (mStaticInfo.isHardwareLevelAtLeastFull()) {
                    for (int[] c : FULL_COMBINATIONS) {
                        assertTrue(String.format(
                                    "Expected static reprocessable stream combination:" +
                                    "%s not found among the available mandatory combinations",
                                    maxSizes.reprocessCombinationToString(c)),
                                isMandatoryCombinationAvailable(c, maxSizes, /*isInput*/ true, cs));
                    }
                }

                if (mStaticInfo.isCapabilitySupported(
                        CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
                    for (int[] c : RAW_COMBINATIONS) {
                        assertTrue(String.format(
                                    "Expected static reprocessable stream combination:" +
                                    "%s not found among the available mandatory combinations",
                                    maxSizes.reprocessCombinationToString(c)),
                                isMandatoryCombinationAvailable(c, maxSizes, /*isInput*/ true, cs));
                    }
                }

                if (mStaticInfo.isHardwareLevelAtLeast(
                            CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_3)) {
                    for (int[] c : LEVEL_3_COMBINATIONS) {
                        assertTrue(String.format(
                                    "Expected static reprocessable stream combination:" +
                                    "%s not found among the available mandatory combinations",
                                    maxSizes.reprocessCombinationToString(c)),
                                isMandatoryCombinationAvailable(c, maxSizes, /*isInput*/ true, cs));
                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    private boolean isMandatoryCombinationAvailable(final int[] combination,
            final MaxStreamSizes maxSizes,
            final MandatoryStreamCombination[] availableCombinations) {
        return isMandatoryCombinationAvailable(combination, maxSizes, /*isInput*/ false,
                availableCombinations);
    }

    private boolean isMandatoryCombinationAvailable(final int[] combination,
            final MaxStreamSizes maxSizes, boolean isInput,
            final MandatoryStreamCombination[] availableCombinations) {
        boolean supportYuvReprocess = mStaticInfo.isCapabilitySupported(
                CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
        boolean supportOpaqueReprocess = mStaticInfo.isCapabilitySupported(
                CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING);
        // Static combinations to be verified can be composed of multiple entries
        // that have the following layout (format, size). In case "isInput" is set,
        // the first stream configuration entry will contain the input format and size
        // as well as the first matching output.
        int streamCount = combination.length / 2;
        ArrayList<Pair<Pair<Integer, Boolean>, Size>> currentCombination =
                new ArrayList<Pair<Pair<Integer, Boolean>, Size>>(streamCount);
        for (int i = 0; i < combination.length; i += 2) {
            if (isInput && (i == 0)) {
                // Skip the combination if the format is not supported for reprocessing.
                if ((combination[i] == YUV && !supportYuvReprocess) ||
                        (combination[i] == PRIV && !supportOpaqueReprocess)) {
                    return true;
                }
                Size sz = maxSizes.getMaxInputSizeForFormat(combination[i]);
                currentCombination.add(Pair.create(Pair.create(new Integer(combination[i]),
                            new Boolean(true)), sz));
                currentCombination.add(Pair.create(Pair.create(new Integer(combination[i]),
                            new Boolean(false)), sz));
            } else {
                Size sz = maxSizes.getOutputSizeForFormat(combination[i], combination[i+1]);
                currentCombination.add(Pair.create(Pair.create(new Integer(combination[i]),
                            new Boolean(false)), sz));
            }
        }

        for (MandatoryStreamCombination c : availableCombinations) {
            List<MandatoryStreamInformation> streamInfoList = c.getStreamsInformation();
            if ((streamInfoList.size() == currentCombination.size()) &&
                    (isInput == c.isReprocessable())) {
                ArrayList<Pair<Pair<Integer, Boolean>, Size>> expected =
                        new ArrayList<Pair<Pair<Integer, Boolean>, Size>>(currentCombination);

                for (MandatoryStreamInformation streamInfo : streamInfoList) {
                    Size maxSize = CameraTestUtils.getMaxSize(
                            streamInfo.getAvailableSizes().toArray(new Size[0]));
                    Pair p = Pair.create(Pair.create(new Integer(streamInfo.getFormat()),
                            new Boolean(streamInfo.isInput())), maxSize);
                    if (expected.contains(p)) {
                        expected.remove(p);
                    }
                }

                if (expected.isEmpty()) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Verify correctness of the configuration tables.
     */
    private void validityCheckConfigurationTables(final int[][][] tables) throws Exception {
        int tableIdx = 0;
        for (int[][] table : tables) {
            int rowIdx = 0;
            for (int[] row : table) {
                assertTrue(String.format("Odd number of entries for table %d row %d: %s ",
                                tableIdx, rowIdx, Arrays.toString(row)),
                        (row.length % 2) == 0);
                for (int i = 0; i < row.length; i += 2) {
                    int format = row[i];
                    int maxSize = row[i + 1];
                    assertTrue(String.format("table %d row %d index %d format not valid: %d",
                                    tableIdx, rowIdx, i, format),
                            format == PRIV || format == JPEG || format == YUV || format == RAW);
                    assertTrue(String.format("table %d row %d index %d max size not valid: %d",
                                    tableIdx, rowIdx, i + 1, maxSize),
                            maxSize == PREVIEW || maxSize == RECORD ||
                            maxSize == MAXIMUM || maxSize == VGA);
                }
                rowIdx++;
            }
            tableIdx++;
        }
    }

    /**
     * Simple holder for resolutions to use for different camera outputs and size limits.
     */
    static class MaxStreamSizes {
        // Format shorthands
        static final int PRIV = ImageFormat.PRIVATE;
        static final int JPEG = ImageFormat.JPEG;
        static final int YUV  = ImageFormat.YUV_420_888;
        static final int RAW  = ImageFormat.RAW_SENSOR;
        static final int Y8   = ImageFormat.Y8;
        static final int HEIC = ImageFormat.HEIC;

        // Max resolution indices
        static final int PREVIEW = 0;
        static final int RECORD  = 1;
        static final int MAXIMUM = 2;
        static final int VGA = 3;
        static final int VGA_FULL_FOV = 4;
        static final int MAX_30FPS = 5;
        static final int RESOLUTION_COUNT = 6;

        static final long FRAME_DURATION_30FPS_NSEC = (long) 1e9 / 30;

        public MaxStreamSizes(StaticMetadata sm, String cameraId, Context context) {
            Size[] privSizes = sm.getAvailableSizesForFormatChecked(ImageFormat.PRIVATE,
                    StaticMetadata.StreamDirection.Output, /*fastSizes*/true, /*slowSizes*/false);
            Size[] yuvSizes = sm.getAvailableSizesForFormatChecked(ImageFormat.YUV_420_888,
                    StaticMetadata.StreamDirection.Output, /*fastSizes*/true, /*slowSizes*/false);

            Size[] y8Sizes = sm.getAvailableSizesForFormatChecked(ImageFormat.Y8,
                    StaticMetadata.StreamDirection.Output, /*fastSizes*/true, /*slowSizes*/false);
            Size[] jpegSizes = sm.getAvailableSizesForFormatChecked(ImageFormat.JPEG,
                    StaticMetadata.StreamDirection.Output, /*fastSizes*/true, /*slowSizes*/false);
            Size[] rawSizes = sm.getAvailableSizesForFormatChecked(ImageFormat.RAW_SENSOR,
                    StaticMetadata.StreamDirection.Output, /*fastSizes*/true, /*slowSizes*/false);
            Size[] heicSizes = sm.getAvailableSizesForFormatChecked(ImageFormat.HEIC,
                    StaticMetadata.StreamDirection.Output, /*fastSizes*/true, /*slowSizes*/false);

            Size maxPreviewSize = getMaxPreviewSize(context, cameraId);

            maxRawSize = (rawSizes.length != 0) ? CameraTestUtils.getMaxSize(rawSizes) : null;

            StreamConfigurationMap configs = sm.getCharacteristics().get(
                    CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            if (sm.isColorOutputSupported()) {
                maxPrivSizes[PREVIEW] = getMaxSize(privSizes, maxPreviewSize);
                maxYuvSizes[PREVIEW]  = getMaxSize(yuvSizes, maxPreviewSize);
                maxJpegSizes[PREVIEW] = getMaxSize(jpegSizes, maxPreviewSize);

                if (sm.isExternalCamera()) {
                    maxPrivSizes[RECORD] = getMaxExternalRecordingSize(cameraId, configs);
                    maxYuvSizes[RECORD]  = getMaxExternalRecordingSize(cameraId, configs);
                    maxJpegSizes[RECORD] = getMaxExternalRecordingSize(cameraId, configs);
                } else {
                    maxPrivSizes[RECORD] = getMaxRecordingSize(cameraId);
                    maxYuvSizes[RECORD]  = getMaxRecordingSize(cameraId);
                    maxJpegSizes[RECORD] = getMaxRecordingSize(cameraId);
                }

                maxPrivSizes[MAXIMUM] = CameraTestUtils.getMaxSize(privSizes);
                maxYuvSizes[MAXIMUM] = CameraTestUtils.getMaxSize(yuvSizes);
                maxJpegSizes[MAXIMUM] = CameraTestUtils.getMaxSize(jpegSizes);

                // Must always be supported, add unconditionally
                final Size vgaSize = new Size(640, 480);
                maxPrivSizes[VGA] = vgaSize;
                maxYuvSizes[VGA] = vgaSize;
                maxJpegSizes[VGA] = vgaSize;

                if (sm.isMonochromeWithY8()) {
                    maxY8Sizes[PREVIEW]  = getMaxSize(y8Sizes, maxPreviewSize);
                    if (sm.isExternalCamera()) {
                        maxY8Sizes[RECORD]  = getMaxExternalRecordingSize(cameraId, configs);
                    } else {
                        maxY8Sizes[RECORD]  = getMaxRecordingSize(cameraId);
                    }
                    maxY8Sizes[MAXIMUM] = CameraTestUtils.getMaxSize(y8Sizes);
                    maxY8Sizes[VGA] = vgaSize;
                }

                if (sm.isHeicSupported()) {
                    maxHeicSizes[PREVIEW] = getMaxSize(heicSizes, maxPreviewSize);
                    maxHeicSizes[RECORD] = getMaxRecordingSize(cameraId);
                    maxHeicSizes[MAXIMUM] = CameraTestUtils.getMaxSize(heicSizes);
                    maxHeicSizes[VGA] = vgaSize;
                }
            }
            if (sm.isColorOutputSupported() && !sm.isHardwareLevelLegacy()) {
                // VGA resolution, but with aspect ratio matching full res FOV
                float fullFovAspect = maxYuvSizes[MAXIMUM].getWidth() /
                    (float) maxYuvSizes[MAXIMUM].getHeight();
                Size vgaFullFovSize = new Size(640, (int) (640 / fullFovAspect));

                maxPrivSizes[VGA_FULL_FOV] = vgaFullFovSize;
                maxYuvSizes[VGA_FULL_FOV] = vgaFullFovSize;
                maxJpegSizes[VGA_FULL_FOV] = vgaFullFovSize;
                if (sm.isMonochromeWithY8()) {
                    maxY8Sizes[VGA_FULL_FOV] = vgaFullFovSize;
                }

                // Max resolution that runs at 30fps

                Size maxPriv30fpsSize = null;
                Size maxYuv30fpsSize = null;
                Size maxY830fpsSize = null;
                Size maxJpeg30fpsSize = null;
                Comparator<Size> comparator = new SizeComparator();
                for (Map.Entry<Size, Long> e :
                             sm.getAvailableMinFrameDurationsForFormatChecked(ImageFormat.PRIVATE).
                             entrySet()) {
                    Size s = e.getKey();
                    Long minDuration = e.getValue();
                    Log.d(TAG, String.format("Priv Size: %s, duration %d limit %d", s, minDuration,
                                FRAME_DURATION_30FPS_NSEC));
                    if (minDuration <= FRAME_DURATION_30FPS_NSEC) {
                        if (maxPriv30fpsSize == null ||
                                comparator.compare(maxPriv30fpsSize, s) < 0) {
                            maxPriv30fpsSize = s;
                        }
                    }
                }
                assertTrue("No PRIVATE resolution available at 30fps!", maxPriv30fpsSize != null);

                for (Map.Entry<Size, Long> e :
                             sm.getAvailableMinFrameDurationsForFormatChecked(
                                     ImageFormat.YUV_420_888).
                             entrySet()) {
                    Size s = e.getKey();
                    Long minDuration = e.getValue();
                    Log.d(TAG, String.format("YUV Size: %s, duration %d limit %d", s, minDuration,
                                FRAME_DURATION_30FPS_NSEC));
                    if (minDuration <= FRAME_DURATION_30FPS_NSEC) {
                        if (maxYuv30fpsSize == null ||
                                comparator.compare(maxYuv30fpsSize, s) < 0) {
                            maxYuv30fpsSize = s;
                        }
                    }
                }
                assertTrue("No YUV_420_888 resolution available at 30fps!",
                        maxYuv30fpsSize != null);

                if (sm.isMonochromeWithY8()) {
                    for (Map.Entry<Size, Long> e :
                                 sm.getAvailableMinFrameDurationsForFormatChecked(
                                         ImageFormat.Y8).
                                 entrySet()) {
                        Size s = e.getKey();
                        Long minDuration = e.getValue();
                        Log.d(TAG, String.format("Y8 Size: %s, duration %d limit %d",
                                s, minDuration, FRAME_DURATION_30FPS_NSEC));
                        if (minDuration <= FRAME_DURATION_30FPS_NSEC) {
                            if (maxY830fpsSize == null ||
                                    comparator.compare(maxY830fpsSize, s) < 0) {
                                maxY830fpsSize = s;
                            }
                        }
                    }
                    assertTrue("No Y8 resolution available at 30fps!", maxY830fpsSize != null);
                }

                for (Map.Entry<Size, Long> e :
                             sm.getAvailableMinFrameDurationsForFormatChecked(ImageFormat.JPEG).
                             entrySet()) {
                    Size s = e.getKey();
                    Long minDuration = e.getValue();
                    Log.d(TAG, String.format("JPEG Size: %s, duration %d limit %d", s, minDuration,
                                FRAME_DURATION_30FPS_NSEC));
                    if (minDuration <= FRAME_DURATION_30FPS_NSEC) {
                        if (maxJpeg30fpsSize == null ||
                                comparator.compare(maxJpeg30fpsSize, s) < 0) {
                            maxJpeg30fpsSize = s;
                        }
                    }
                }
                assertTrue("No JPEG resolution available at 30fps!", maxJpeg30fpsSize != null);

                maxPrivSizes[MAX_30FPS] = maxPriv30fpsSize;
                maxYuvSizes[MAX_30FPS] = maxYuv30fpsSize;
                maxY8Sizes[MAX_30FPS] = maxY830fpsSize;
                maxJpegSizes[MAX_30FPS] = maxJpeg30fpsSize;
            }

            Size[] privInputSizes = configs.getInputSizes(ImageFormat.PRIVATE);
            maxInputPrivSize = privInputSizes != null ?
                    CameraTestUtils.getMaxSize(privInputSizes) : null;
            Size[] yuvInputSizes = configs.getInputSizes(ImageFormat.YUV_420_888);
            maxInputYuvSize = yuvInputSizes != null ?
                    CameraTestUtils.getMaxSize(yuvInputSizes) : null;
            Size[] y8InputSizes = configs.getInputSizes(ImageFormat.Y8);
            maxInputY8Size = y8InputSizes != null ?
                    CameraTestUtils.getMaxSize(y8InputSizes) : null;
        }

        private final Size[] maxPrivSizes = new Size[RESOLUTION_COUNT];
        private final Size[] maxJpegSizes = new Size[RESOLUTION_COUNT];
        private final Size[] maxYuvSizes = new Size[RESOLUTION_COUNT];
        private final Size[] maxY8Sizes = new Size[RESOLUTION_COUNT];
        private final Size[] maxHeicSizes = new Size[RESOLUTION_COUNT];
        private final Size maxRawSize;
        // TODO: support non maximum reprocess input.
        private final Size maxInputPrivSize;
        private final Size maxInputYuvSize;
        private final Size maxInputY8Size;

        public final Size getOutputSizeForFormat(int format, int resolutionIndex) {
            if (resolutionIndex >= RESOLUTION_COUNT) {
                return new Size(0, 0);
            }

            switch (format) {
                case PRIV:
                    return maxPrivSizes[resolutionIndex];
                case YUV:
                    return maxYuvSizes[resolutionIndex];
                case JPEG:
                    return maxJpegSizes[resolutionIndex];
                case Y8:
                    return maxY8Sizes[resolutionIndex];
                case HEIC:
                    return maxHeicSizes[resolutionIndex];
                case RAW:
                    return maxRawSize;
                default:
                    return new Size(0, 0);
            }
        }

        public final Size getMaxInputSizeForFormat(int format) {
            switch (format) {
                case PRIV:
                    return maxInputPrivSize;
                case YUV:
                    return maxInputYuvSize;
                case Y8:
                    return maxInputY8Size;
                default:
                    return new Size(0, 0);
            }
        }

        static public String combinationToString(int[] combination) {
            StringBuilder b = new StringBuilder("{ ");
            for (int i = 0; i < combination.length; i += 2) {
                int format = combination[i];
                int sizeLimit = combination[i + 1];

                appendFormatSize(b, format, sizeLimit);
                b.append(" ");
            }
            b.append("}");
            return b.toString();
        }

        static public String reprocessCombinationToString(int[] reprocessCombination) {
            // reprocessConfig[0..1] is the input configuration
            StringBuilder b = new StringBuilder("Input: ");
            appendFormatSize(b, reprocessCombination[0], reprocessCombination[1]);

            // reprocessCombnation[0..1] is also output combination to be captured as reprocess
            // input.
            b.append(", Outputs: { ");
            for (int i = 0; i < reprocessCombination.length; i += 2) {
                int format = reprocessCombination[i];
                int sizeLimit = reprocessCombination[i + 1];

                appendFormatSize(b, format, sizeLimit);
                b.append(" ");
            }
            b.append("}");
            return b.toString();
        }

        static private void appendFormatSize(StringBuilder b, int format, int Size) {
            switch (format) {
                case PRIV:
                    b.append("[PRIV, ");
                    break;
                case JPEG:
                    b.append("[JPEG, ");
                    break;
                case YUV:
                    b.append("[YUV, ");
                    break;
                case Y8:
                    b.append("[Y8, ");
                    break;
                case RAW:
                    b.append("[RAW, ");
                    break;
                default:
                    b.append("[UNK, ");
                    break;
            }

            switch (Size) {
                case PREVIEW:
                    b.append("PREVIEW]");
                    break;
                case RECORD:
                    b.append("RECORD]");
                    break;
                case MAXIMUM:
                    b.append("MAXIMUM]");
                    break;
                case VGA:
                    b.append("VGA]");
                    break;
                case VGA_FULL_FOV:
                    b.append("VGA_FULL_FOV]");
                    break;
                case MAX_30FPS:
                    b.append("MAX_30FPS]");
                    break;
                default:
                    b.append("UNK]");
                    break;
            }
        }
    }

    private static Size getMaxRecordingSize(String cameraId) {
        int id = Integer.valueOf(cameraId);

        int quality =
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_2160P) ?
                    CamcorderProfile.QUALITY_2160P :
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_1080P) ?
                    CamcorderProfile.QUALITY_1080P :
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_720P) ?
                    CamcorderProfile.QUALITY_720P :
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_480P) ?
                    CamcorderProfile.QUALITY_480P :
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_QVGA) ?
                    CamcorderProfile.QUALITY_QVGA :
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_CIF) ?
                    CamcorderProfile.QUALITY_CIF :
                CamcorderProfile.hasProfile(id, CamcorderProfile.QUALITY_QCIF) ?
                    CamcorderProfile.QUALITY_QCIF :
                    -1;

        assertTrue("No recording supported for camera id " + cameraId, quality != -1);

        CamcorderProfile maxProfile = CamcorderProfile.get(id, quality);
        return new Size(maxProfile.videoFrameWidth, maxProfile.videoFrameHeight);
    }

    private static Size getMaxExternalRecordingSize(
            String cameraId, StreamConfigurationMap config) {
        final Size FULLHD = new Size(1920, 1080);

        Size[] videoSizeArr = config.getOutputSizes(android.media.MediaRecorder.class);
        List<Size> sizes = new ArrayList<Size>();
        for (Size sz: videoSizeArr) {
            if (sz.getWidth() <= FULLHD.getWidth() && sz.getHeight() <= FULLHD.getHeight()) {
                sizes.add(sz);
            }
        }
        List<Size> videoSizes = getAscendingOrderSizes(sizes, /*ascending*/false);
        for (Size sz : videoSizes) {
            long minFrameDuration = config.getOutputMinFrameDuration(
                    android.media.MediaRecorder.class, sz);
            // Give some margin for rounding error
            if (minFrameDuration > (1e9 / 30.1)) {
                Log.i(TAG, "External camera " + cameraId + " has max video size:" + sz);
                return sz;
            }
        }
        fail("Camera " + cameraId + " does not support any 30fps video output");
        return FULLHD; // doesn't matter what size is returned here
    }

    /**
     * Get maximum size in list that's equal or smaller to than the bound.
     * Returns null if no size is smaller than or equal to the bound.
     */
    private static Size getMaxSize(Size[] sizes, Size bound) {
        if (sizes == null || sizes.length == 0) {
            throw new IllegalArgumentException("sizes was empty");
        }

        Size sz = null;
        for (Size size : sizes) {
            if (size.getWidth() <= bound.getWidth() && size.getHeight() <= bound.getHeight()) {

                if (sz == null) {
                    sz = size;
                } else {
                    long curArea = sz.getWidth() * (long) sz.getHeight();
                    long newArea = size.getWidth() * (long) size.getHeight();
                    if ( newArea > curArea ) {
                        sz = size;
                    }
                }
            }
        }

        assertTrue("No size under bound found: " + Arrays.toString(sizes) + " bound " + bound,
                sz != null);

        return sz;
    }

    private static Size getMaxPreviewSize(Context context, String cameraId) {
        try {
            WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
            Display display = windowManager.getDefaultDisplay();

            int width = display.getWidth();
            int height = display.getHeight();

            if (height > width) {
                height = width;
                width = display.getHeight();
            }

            CameraManager camMgr =
                (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
            List<Size> orderedPreviewSizes = CameraTestUtils.getSupportedPreviewSizes(
                cameraId, camMgr, PREVIEW_SIZE_BOUND);

            if (orderedPreviewSizes != null) {
                for (Size size : orderedPreviewSizes) {
                    if (width >= size.getWidth() &&
                        height >= size.getHeight())
                        return size;
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "getMaxPreviewSize Failed. "+e.toString());
        }
        return PREVIEW_SIZE_BOUND;
    }
}
