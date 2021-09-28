/*
 * Copyright (C) 2014 The Android Open Source Project
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

import android.content.Context;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraCharacteristics.Key;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.cts.helpers.CameraErrorCollector;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.testcases.Camera2AndroidTestCase;
import android.hardware.camera2.params.BlackLevelPattern;
import android.hardware.camera2.params.ColorSpaceTransform;
import android.hardware.camera2.params.RecommendedStreamConfigurationMap;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.CamcorderProfile;
import android.media.ImageReader;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Rational;
import android.util.Range;
import android.util.Size;
import android.util.Pair;
import android.util.Patterns;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

import com.android.compatibility.common.util.CddTest;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.Set;

import org.junit.runners.Parameterized;
import org.junit.runner.RunWith;
import org.junit.Test;

import static android.hardware.camera2.cts.helpers.AssertHelpers.*;
import static android.hardware.camera2.cts.CameraTestUtils.SimpleCaptureCallback;

import static junit.framework.Assert.*;

import static org.mockito.Mockito.*;

/**
 * Extended tests for static camera characteristics.
 */
@RunWith(Parameterized.class)
public class ExtendedCameraCharacteristicsTest extends Camera2AndroidTestCase {
    private static final String TAG = "ExChrsTest"; // must be short so next line doesn't throw
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    private static final String PREFIX_ANDROID = "android";

    /*
     * Constants for static RAW metadata.
     */
    private static final int MIN_ALLOWABLE_WHITELEVEL = 32; // must have sensor bit depth > 5

    private List<CameraCharacteristics> mCharacteristics;

    private static final Size FULLHD = new Size(1920, 1080);
    private static final Size FULLHD_ALT = new Size(1920, 1088);
    private static final Size HD = new Size(1280, 720);
    private static final Size VGA = new Size(640, 480);
    private static final Size QVGA = new Size(320, 240);

    private static final long MIN_BACK_SENSOR_RESOLUTION = 2000000;
    private static final long MIN_FRONT_SENSOR_RESOLUTION = VGA.getHeight() * VGA.getWidth();
    private static final long LOW_LATENCY_THRESHOLD_MS = 200;
    private static final float LATENCY_TOLERANCE_FACTOR = 1.1f; // 10% tolerance
    private static final float FOCAL_LENGTH_TOLERANCE = .01f;
    private static final int MAX_NUM_IMAGES = 5;
    private static final long PREVIEW_RUN_MS = 500;
    private static final long FRAME_DURATION_30FPS_NSEC = (long) 1e9 / 30;
    /*
     * HW Levels short hand
     */
    private static final int LEGACY = CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    private static final int LIMITED = CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED;
    private static final int FULL = CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    private static final int LEVEL_3 = CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_3;
    private static final int EXTERNAL = CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL;
    private static final int OPT = Integer.MAX_VALUE;  // For keys that are optional on all hardware levels.

    /*
     * Capabilities short hand
     */
    private static final int NONE = -1;
    private static final int BC =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE;
    private static final int MANUAL_SENSOR =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR;
    private static final int MANUAL_POSTPROC =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING;
    private static final int RAW =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_RAW;
    private static final int YUV_REPROCESS =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING;
    private static final int OPAQUE_REPROCESS =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING;
    private static final int CONSTRAINED_HIGH_SPEED =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO;
    private static final int MONOCHROME =
            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MONOCHROME;
    private static final int HIGH_SPEED_FPS_LOWER_MIN = 30;
    private static final int HIGH_SPEED_FPS_UPPER_MIN = 120;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mCharacteristics = new ArrayList<>();
        for (int i = 0; i < mAllCameraIds.length; i++) {
            mCharacteristics.add(mAllStaticInfo.get(mAllCameraIds[i]).getCharacteristics());
        }
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        mCharacteristics = null;
    }

    /**
     * Test that the available stream configurations contain a few required formats and sizes.
     */
    @CddTest(requirement="7.5.1/C-1-2")
    @Test
    public void testAvailableStreamConfigs() throws Exception {
        boolean firstBackFacingCamera = true;
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            StreamConfigurationMap config =
                    c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            assertNotNull(String.format("No stream configuration map found for: ID %s",
                    mAllCameraIds[i]), config);
            int[] outputFormats = config.getOutputFormats();

            int[] actualCapabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    actualCapabilities);

            // Check required formats exist (JPEG, and YUV_420_888).
            if (!arrayContains(actualCapabilities, BC)) {
                Log.i(TAG, "Camera " + mAllCameraIds[i] +
                    ": BACKWARD_COMPATIBLE capability not supported, skipping test");
                continue;
            }

            boolean isMonochromeWithY8 = arrayContains(actualCapabilities, MONOCHROME)
                    && arrayContains(outputFormats, ImageFormat.Y8);
            boolean isHiddenPhysicalCamera = !arrayContains(mCameraIdsUnderTest, mAllCameraIds[i]);
            boolean supportHeic = arrayContains(outputFormats, ImageFormat.HEIC);

            assertArrayContains(
                    String.format("No valid YUV_420_888 preview formats found for: ID %s",
                            mAllCameraIds[i]), outputFormats, ImageFormat.YUV_420_888);
            if (isMonochromeWithY8) {
                assertArrayContains(
                        String.format("No valid Y8 preview formats found for: ID %s",
                                mAllCameraIds[i]), outputFormats, ImageFormat.Y8);
            }
            assertArrayContains(String.format("No JPEG image format for: ID %s",
                    mAllCameraIds[i]), outputFormats, ImageFormat.JPEG);

            Size[] yuvSizes = config.getOutputSizes(ImageFormat.YUV_420_888);
            Size[] y8Sizes = config.getOutputSizes(ImageFormat.Y8);
            Size[] jpegSizes = config.getOutputSizes(ImageFormat.JPEG);
            Size[] heicSizes = config.getOutputSizes(ImageFormat.HEIC);
            Size[] privateSizes = config.getOutputSizes(ImageFormat.PRIVATE);

            CameraTestUtils.assertArrayNotEmpty(yuvSizes,
                    String.format("No sizes for preview format %x for: ID %s",
                            ImageFormat.YUV_420_888, mAllCameraIds[i]));
            if (isMonochromeWithY8) {
                CameraTestUtils.assertArrayNotEmpty(y8Sizes,
                    String.format("No sizes for preview format %x for: ID %s",
                            ImageFormat.Y8, mAllCameraIds[i]));
            }

            Rect activeRect = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            Size pixelArraySize = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);

            int activeArrayHeight = activeRect.height();
            int activeArrayWidth = activeRect.width();
            long sensorResolution = pixelArraySize.getHeight() * pixelArraySize.getWidth() ;
            Integer lensFacing = c.get(CameraCharacteristics.LENS_FACING);
            assertNotNull("Can't get lens facing info for camera id: " + mAllCameraIds[i],
                    lensFacing);

            // Check that the sensor sizes are atleast what the CDD specifies
            switch(lensFacing) {
                case CameraCharacteristics.LENS_FACING_FRONT:
                    assertTrue("Front Sensor resolution should be at least " +
                            MIN_FRONT_SENSOR_RESOLUTION + " pixels, is "+ sensorResolution,
                            sensorResolution >= MIN_FRONT_SENSOR_RESOLUTION);
                    break;
                case CameraCharacteristics.LENS_FACING_BACK:
                    if (firstBackFacingCamera) {
                        assertTrue("Back Sensor resolution should be at least "
                                + MIN_BACK_SENSOR_RESOLUTION +
                                " pixels, is "+ sensorResolution,
                                sensorResolution >= MIN_BACK_SENSOR_RESOLUTION);
                        firstBackFacingCamera = false;
                    }
                    break;
                default:
                    break;
            }

            Integer hwLevel = c.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);

            if (activeArrayWidth >= FULLHD.getWidth() &&
                    activeArrayHeight >= FULLHD.getHeight()) {
                assertArrayContainsAnyOf(String.format(
                        "Required FULLHD size not found for format %x for: ID %s",
                        ImageFormat.JPEG, mAllCameraIds[i]), jpegSizes,
                        new Size[] {FULLHD, FULLHD_ALT});
                if (supportHeic) {
                    assertArrayContainsAnyOf(String.format(
                            "Required FULLHD size not found for format %x for: ID %s",
                            ImageFormat.HEIC, mAllCameraIds[i]), heicSizes,
                            new Size[] {FULLHD, FULLHD_ALT});
                }
            }

            if (activeArrayWidth >= HD.getWidth() &&
                    activeArrayHeight >= HD.getHeight()) {
                assertArrayContains(String.format(
                        "Required HD size not found for format %x for: ID %s",
                        ImageFormat.JPEG, mAllCameraIds[i]), jpegSizes, HD);
                if (supportHeic) {
                    assertArrayContains(String.format(
                            "Required HD size not found for format %x for: ID %s",
                            ImageFormat.HEIC, mAllCameraIds[i]), heicSizes, HD);
                }
            }

            if (activeArrayWidth >= VGA.getWidth() &&
                    activeArrayHeight >= VGA.getHeight()) {
                assertArrayContains(String.format(
                        "Required VGA size not found for format %x for: ID %s",
                        ImageFormat.JPEG, mAllCameraIds[i]), jpegSizes, VGA);
                if (supportHeic) {
                    assertArrayContains(String.format(
                            "Required VGA size not found for format %x for: ID %s",
                            ImageFormat.HEIC, mAllCameraIds[i]), heicSizes, VGA);
                }
            }

            if (activeArrayWidth >= QVGA.getWidth() &&
                    activeArrayHeight >= QVGA.getHeight()) {
                assertArrayContains(String.format(
                        "Required QVGA size not found for format %x for: ID %s",
                        ImageFormat.JPEG, mAllCameraIds[i]), jpegSizes, QVGA);
                if (supportHeic) {
                    assertArrayContains(String.format(
                            "Required QVGA size not found for format %x for: ID %s",
                            ImageFormat.HEIC, mAllCameraIds[i]), heicSizes, QVGA);
                }

            }

            ArrayList<Size> jpegSizesList = new ArrayList<>(Arrays.asList(jpegSizes));
            ArrayList<Size> yuvSizesList = new ArrayList<>(Arrays.asList(yuvSizes));
            ArrayList<Size> privateSizesList = new ArrayList<>(Arrays.asList(privateSizes));
            boolean isExternalCamera = (hwLevel ==
                    CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL);
            Size maxVideoSize = null;
            if (isExternalCamera || isHiddenPhysicalCamera) {
                // TODO: for now, use FULLHD 30 as largest possible video size for external camera.
                // For hidden physical camera, since we don't require CamcorderProfile to be
                // available, use FULLHD 30 as maximum video size as well.
                List<Size> videoSizes = CameraTestUtils.getSupportedVideoSizes(
                        mAllCameraIds[i], mCameraManager, FULLHD);
                for (Size sz : videoSizes) {
                    long minFrameDuration = config.getOutputMinFrameDuration(
                            android.media.MediaRecorder.class, sz);
                    // Give some margin for rounding error
                    if (minFrameDuration < (1e9 / 29.9)) {
                        maxVideoSize = sz;
                        break;
                    }
                }
            } else {
                int cameraId = Integer.valueOf(mAllCameraIds[i]);
                CamcorderProfile maxVideoProfile = CamcorderProfile.get(
                        cameraId, CamcorderProfile.QUALITY_HIGH);
                maxVideoSize = new Size(
                        maxVideoProfile.videoFrameWidth, maxVideoProfile.videoFrameHeight);
            }
            if (maxVideoSize == null) {
                fail("Camera " + mAllCameraIds[i] + " does not support any 30fps video output");
            }

            // Handle FullHD special case first
            if (jpegSizesList.contains(FULLHD)) {
                if (compareHardwareLevel(hwLevel, LEVEL_3) >= 0 || hwLevel == FULL ||
                        (hwLevel == LIMITED &&
                        maxVideoSize.getWidth() >= FULLHD.getWidth() &&
                        maxVideoSize.getHeight() >= FULLHD.getHeight())) {
                    boolean yuvSupportFullHD = yuvSizesList.contains(FULLHD) ||
                            yuvSizesList.contains(FULLHD_ALT);
                    boolean privateSupportFullHD = privateSizesList.contains(FULLHD) ||
                            privateSizesList.contains(FULLHD_ALT);
                    assertTrue("Full device FullHD YUV size not found", yuvSupportFullHD);
                    assertTrue("Full device FullHD PRIVATE size not found", privateSupportFullHD);

                    if (isMonochromeWithY8) {
                        ArrayList<Size> y8SizesList = new ArrayList<>(Arrays.asList(y8Sizes));
                        boolean y8SupportFullHD = y8SizesList.contains(FULLHD) ||
                                y8SizesList.contains(FULLHD_ALT);
                        assertTrue("Full device FullHD Y8 size not found", y8SupportFullHD);
                    }
                }
                // remove all FullHD or FullHD_Alt sizes for the remaining of the test
                jpegSizesList.remove(FULLHD);
                jpegSizesList.remove(FULLHD_ALT);
            }

            // Check all sizes other than FullHD
            if (hwLevel == LIMITED) {
                // Remove all jpeg sizes larger than max video size
                ArrayList<Size> toBeRemoved = new ArrayList<>();
                for (Size size : jpegSizesList) {
                    if (size.getWidth() >= maxVideoSize.getWidth() &&
                            size.getHeight() >= maxVideoSize.getHeight()) {
                        toBeRemoved.add(size);
                    }
                }
                jpegSizesList.removeAll(toBeRemoved);
            }

            if (compareHardwareLevel(hwLevel, LEVEL_3) >= 0 || hwLevel == FULL ||
                    hwLevel == LIMITED) {
                if (!yuvSizesList.containsAll(jpegSizesList)) {
                    for (Size s : jpegSizesList) {
                        if (!yuvSizesList.contains(s)) {
                            fail("Size " + s + " not found in YUV format");
                        }
                    }
                }

                if (isMonochromeWithY8) {
                    ArrayList<Size> y8SizesList = new ArrayList<>(Arrays.asList(y8Sizes));
                    if (!y8SizesList.containsAll(jpegSizesList)) {
                        for (Size s : jpegSizesList) {
                            if (!y8SizesList.contains(s)) {
                                fail("Size " + s + " not found in Y8 format");
                            }
                        }
                    }
                }
            }

            if (!privateSizesList.containsAll(yuvSizesList)) {
                for (Size s : yuvSizesList) {
                    if (!privateSizesList.contains(s)) {
                        fail("Size " + s + " not found in PRIVATE format");
                    }
                }
            }
        }
    }

    private void verifyCommonRecommendedConfiguration(String id, CameraCharacteristics c,
            RecommendedStreamConfigurationMap config, boolean checkNoInput,
            boolean checkNoHighRes, boolean checkNoHighSpeed, boolean checkNoPrivate,
            boolean checkNoDepth) {
        StreamConfigurationMap fullConfig = c.get(
                CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        assertNotNull(String.format("No stream configuration map found for ID: %s!", id),
                fullConfig);

        Set<Integer> recommendedOutputFormats = config.getOutputFormats();

        if (checkNoInput) {
            Set<Integer> inputFormats = config.getInputFormats();
            assertTrue(String.format("Recommended configuration must not include any input " +
                    "streams for ID: %s", id),
                    ((inputFormats == null) || (inputFormats.size() == 0)));
        }

        if (checkNoHighRes) {
            for (int format : recommendedOutputFormats) {
                Set<Size> highResSizes = config.getHighResolutionOutputSizes(format);
                assertTrue(String.format("Recommended configuration should not include any " +
                        "high resolution sizes, which cannot operate at full " +
                        "BURST_CAPTURE rate for ID: %s", id),
                        ((highResSizes == null) || (highResSizes.size() == 0)));
            }
        }

        if (checkNoHighSpeed) {
            Set<Size> highSpeedSizes = config.getHighSpeedVideoSizes();
            assertTrue(String.format("Recommended configuration must not include any high " +
                    "speed configurations for ID: %s", id),
                    ((highSpeedSizes == null) || (highSpeedSizes.size() == 0)));
        }

        int[] exhaustiveOutputFormats = fullConfig.getOutputFormats();
        for (Integer formatInteger : recommendedOutputFormats) {
            int format = formatInteger.intValue();
            assertArrayContains(String.format("Unsupported recommended output format: %d for " +
                    "ID: %s ", format, id), exhaustiveOutputFormats, format);
            Set<Size> recommendedSizes = config.getOutputSizes(format);

            switch (format) {
                case ImageFormat.PRIVATE:
                    if (checkNoPrivate) {
                        fail(String.format("Recommended configuration must not include " +
                                "PRIVATE format entries for ID: %s", id));
                    }

                    Set<Size> classOutputSizes = config.getOutputSizes(ImageReader.class);
                    assertCollectionContainsAnyOf(String.format("Recommended output sizes for " +
                            "ImageReader class don't match the output sizes for the " +
                            "corresponding format for ID: %s", id), classOutputSizes,
                            recommendedSizes);
                    break;
                case ImageFormat.DEPTH16:
                case ImageFormat.DEPTH_POINT_CLOUD:
                    if (checkNoDepth) {
                        fail(String.format("Recommended configuration must not include any DEPTH " +
                                "formats for ID: %s", id));
                    }
                    break;
                default:
            }
            Size [] exhaustiveSizes = fullConfig.getOutputSizes(format);
            for (Size sz : recommendedSizes) {
                assertArrayContains(String.format("Unsupported recommended size %s for " +
                        "format: %d for ID: %s", sz.toString(), format, id),
                        exhaustiveSizes, sz);

                long recommendedMinDuration = config.getOutputMinFrameDuration(format, sz);
                long availableMinDuration = fullConfig.getOutputMinFrameDuration(format, sz);
                assertTrue(String.format("Recommended minimum frame duration %d for size " +
                        "%s format: %d doesn't match with currently available minimum" +
                        " frame duration of %d for ID: %s", recommendedMinDuration,
                        sz.toString(), format, availableMinDuration, id),
                        (recommendedMinDuration == availableMinDuration));
                long recommendedStallDuration = config.getOutputStallDuration(format, sz);
                long availableStallDuration = fullConfig.getOutputStallDuration(format, sz);
                assertTrue(String.format("Recommended stall duration %d for size %s" +
                        " format: %d doesn't match with currently available stall " +
                        "duration of %d for ID: %s", recommendedStallDuration,
                        sz.toString(), format, availableStallDuration, id),
                        (recommendedStallDuration == availableStallDuration));

                ImageReader reader = ImageReader.newInstance(sz.getWidth(), sz.getHeight(), format,
                        /*maxImages*/1);
                Surface readerSurface = reader.getSurface();
                assertTrue(String.format("ImageReader surface using format %d and size %s is not" +
                        " supported for ID: %s", format, sz.toString(), id),
                        config.isOutputSupportedFor(readerSurface));
                if (format == ImageFormat.PRIVATE) {
                    long classMinDuration = config.getOutputMinFrameDuration(ImageReader.class, sz);
                    assertTrue(String.format("Recommended minimum frame duration %d for size " +
                            "%s format: %d doesn't match with the duration %d for " +
                            "ImageReader class of the same size", recommendedMinDuration,
                            sz.toString(), format, classMinDuration),
                            classMinDuration == recommendedMinDuration);
                    long classStallDuration = config.getOutputStallDuration(ImageReader.class, sz);
                    assertTrue(String.format("Recommended stall duration %d for size " +
                            "%s format: %d doesn't match with the stall duration %d for " +
                            "ImageReader class of the same size", recommendedStallDuration,
                            sz.toString(), format, classStallDuration),
                            classStallDuration == recommendedStallDuration);
                }
            }
        }
    }

    private void verifyRecommendedPreviewConfiguration(String cameraId, CameraCharacteristics c,
            RecommendedStreamConfigurationMap previewConfig) {
        verifyCommonRecommendedConfiguration(cameraId, c, previewConfig, /*checkNoInput*/ true,
                /*checkNoHighRes*/ true, /*checkNoHighSpeed*/ true, /*checkNoPrivate*/ false,
                /*checkNoDepth*/ true);

        Set<Integer> outputFormats = previewConfig.getOutputFormats();
        assertTrue(String.format("No valid YUV_420_888 and PRIVATE preview " +
                "formats found in recommended preview configuration for ID: %s", cameraId),
                outputFormats.containsAll(Arrays.asList(new Integer(ImageFormat.YUV_420_888),
                        new Integer(ImageFormat.PRIVATE))));
    }

    private void verifyRecommendedVideoConfiguration(String cameraId, CameraCharacteristics c,
            RecommendedStreamConfigurationMap videoConfig) {
        verifyCommonRecommendedConfiguration(cameraId, c, videoConfig, /*checkNoInput*/ true,
                /*checkNoHighRes*/ true, /*checkNoHighSpeed*/ false, /*checkNoPrivate*/false,
                /*checkNoDepth*/ true);

        Set<Size> highSpeedSizes = videoConfig.getHighSpeedVideoSizes();
        StreamConfigurationMap fullConfig = c.get(
                CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        assertNotNull("No stream configuration map found!", fullConfig);
        Size [] availableHighSpeedSizes = fullConfig.getHighSpeedVideoSizes();
        if ((highSpeedSizes != null) && (highSpeedSizes.size() > 0)) {
            for (Size sz : highSpeedSizes) {
                assertArrayContains(String.format("Recommended video configuration includes " +
                        "unsupported high speed configuration with size %s for ID: %s",
                        sz.toString(), cameraId), availableHighSpeedSizes, sz);
                Set<Range<Integer>>  highSpeedFpsRanges =
                    videoConfig.getHighSpeedVideoFpsRangesFor(sz);
                Range<Integer> [] availableHighSpeedFpsRanges =
                    fullConfig.getHighSpeedVideoFpsRangesFor(sz);
                for (Range<Integer> fpsRange : highSpeedFpsRanges) {
                    assertArrayContains(String.format("Recommended video configuration includes " +
                            "unsupported high speed fps range [%d %d] for ID: %s",
                            fpsRange.getLower().intValue(), fpsRange.getUpper().intValue(),
                            cameraId), availableHighSpeedFpsRanges, fpsRange);
                }
            }
        }

        final int[] profileList = {
            CamcorderProfile.QUALITY_2160P,
            CamcorderProfile.QUALITY_1080P,
            CamcorderProfile.QUALITY_480P,
            CamcorderProfile.QUALITY_720P,
            CamcorderProfile.QUALITY_CIF,
            CamcorderProfile.QUALITY_HIGH,
            CamcorderProfile.QUALITY_LOW,
            CamcorderProfile.QUALITY_QCIF,
            CamcorderProfile.QUALITY_QVGA,
        };
        Set<Size> privateSizeSet = videoConfig.getOutputSizes(ImageFormat.PRIVATE);
        for (int profile : profileList) {
            int idx = Integer.valueOf(cameraId);
            if (CamcorderProfile.hasProfile(idx, profile)) {
                CamcorderProfile videoProfile = CamcorderProfile.get(idx, profile);
                Size profileSize  = new Size(videoProfile.videoFrameWidth,
                        videoProfile.videoFrameHeight);
                assertCollectionContainsAnyOf(String.format("Recommended video configuration " +
                        "doesn't include supported video profile size %s with Private format " +
                        "for ID: %s", profileSize.toString(), cameraId), privateSizeSet,
                        Arrays.asList(profileSize));
            }
        }
    }

    private Pair<Boolean, Size> isSizeWithinSensorMargin(Size sz, Size sensorSize) {
        final float SIZE_ERROR_MARGIN = 0.03f;
        float croppedWidth = (float)sensorSize.getWidth();
        float croppedHeight = (float)sensorSize.getHeight();
        float sensorAspectRatio = (float)sensorSize.getWidth() / (float)sensorSize.getHeight();
        float maxAspectRatio = (float)sz.getWidth() / (float)sz.getHeight();
        if (sensorAspectRatio < maxAspectRatio) {
            croppedHeight = (float)sensorSize.getWidth() / maxAspectRatio;
        } else if (sensorAspectRatio > maxAspectRatio) {
            croppedWidth = (float)sensorSize.getHeight() * maxAspectRatio;
        }
        Size croppedSensorSize = new Size((int)croppedWidth, (int)croppedHeight);

        Boolean match = new Boolean(
            (sz.getWidth() <= croppedSensorSize.getWidth() * (1.0 + SIZE_ERROR_MARGIN) &&
             sz.getWidth() >= croppedSensorSize.getWidth() * (1.0 - SIZE_ERROR_MARGIN) &&
             sz.getHeight() <= croppedSensorSize.getHeight() * (1.0 + SIZE_ERROR_MARGIN) &&
             sz.getHeight() >= croppedSensorSize.getHeight() * (1.0 - SIZE_ERROR_MARGIN)));

        return Pair.create(match, croppedSensorSize);
    }

    private void verifyRecommendedSnapshotConfiguration(String cameraId, CameraCharacteristics c,
            RecommendedStreamConfigurationMap snapshotConfig) {
        verifyCommonRecommendedConfiguration(cameraId, c, snapshotConfig, /*checkNoInput*/ true,
                /*checkNoHighRes*/ false, /*checkNoHighSpeed*/ true, /*checkNoPrivate*/false,
                /*checkNoDepth*/ false);
        Rect activeRect = CameraTestUtils.getValueNotNull(
                c, CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
        Size arraySize = new Size(activeRect.width(), activeRect.height());

        Set<Size> snapshotSizeSet = snapshotConfig.getOutputSizes(ImageFormat.JPEG);
        Size[] snapshotSizes = new Size[snapshotSizeSet.size()];
        snapshotSizes = snapshotSizeSet.toArray(snapshotSizes);
        Size maxJpegSize = CameraTestUtils.getMaxSize(snapshotSizes);
        assertTrue(String.format("Maximum recommended Jpeg size %s should be within 3 percent " +
                "of the area of the advertised array size %s for ID: %s",
                maxJpegSize.toString(), arraySize.toString(), cameraId),
                isSizeWithinSensorMargin(maxJpegSize, arraySize).first.booleanValue());
    }

    private void verifyRecommendedVideoSnapshotConfiguration(String cameraId,
            CameraCharacteristics c,
            RecommendedStreamConfigurationMap videoSnapshotConfig,
            RecommendedStreamConfigurationMap videoConfig) {
        verifyCommonRecommendedConfiguration(cameraId, c, videoSnapshotConfig,
                /*checkNoInput*/ true, /*checkNoHighRes*/ false, /*checkNoHighSpeed*/ true,
                /*checkNoPrivate*/ true, /*checkNoDepth*/ true);

        Set<Integer> outputFormats = videoSnapshotConfig.getOutputFormats();
        assertCollectionContainsAnyOf(String.format("No valid JPEG format found " +
                "in recommended video snapshot configuration for ID: %s", cameraId),
                outputFormats, Arrays.asList(new Integer(ImageFormat.JPEG)));
        assertTrue(String.format("Recommended video snapshot configuration must only advertise " +
                "JPEG format for ID: %s", cameraId), outputFormats.size() == 1);

        Set<Size> privateVideoSizeSet = videoConfig.getOutputSizes(ImageFormat.PRIVATE);
        Size[] privateVideoSizes = new Size[privateVideoSizeSet.size()];
        privateVideoSizes = privateVideoSizeSet.toArray(privateVideoSizes);
        Size maxVideoSize = CameraTestUtils.getMaxSize(privateVideoSizes);
        Set<Size> outputSizes = videoSnapshotConfig.getOutputSizes(ImageFormat.JPEG);
        assertCollectionContainsAnyOf(String.format("The maximum recommended video size %s " +
                "should be present in the recommended video snapshot configurations for ID: %s",
                maxVideoSize.toString(), cameraId), outputSizes, Arrays.asList(maxVideoSize));
    }

    private void verifyRecommendedRawConfiguration(String cameraId,
            CameraCharacteristics c, RecommendedStreamConfigurationMap rawConfig) {
        verifyCommonRecommendedConfiguration(cameraId, c, rawConfig, /*checkNoInput*/ true,
                /*checkNoHighRes*/ false, /*checkNoHighSpeed*/ true, /*checkNoPrivate*/ true,
                /*checkNoDepth*/ true);

        Set<Integer> outputFormats = rawConfig.getOutputFormats();
        for (Integer outputFormatInteger : outputFormats) {
            int outputFormat = outputFormatInteger.intValue();
            switch (outputFormat) {
                case ImageFormat.RAW10:
                case ImageFormat.RAW12:
                case ImageFormat.RAW_PRIVATE:
                case ImageFormat.RAW_SENSOR:
                    break;
                default:
                    fail(String.format("Recommended raw configuration map must not contain " +
                            " non-RAW formats like: %d for ID: %s", outputFormat, cameraId));

            }
        }
    }

    private void verifyRecommendedZSLConfiguration(String cameraId, CameraCharacteristics c,
            RecommendedStreamConfigurationMap zslConfig) {
        verifyCommonRecommendedConfiguration(cameraId, c, zslConfig, /*checkNoInput*/ false,
                /*checkNoHighRes*/ false, /*checkNoHighSpeed*/ true, /*checkNoPrivate*/ false,
                /*checkNoDepth*/ false);

        StreamConfigurationMap fullConfig =
            c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        assertNotNull(String.format("No stream configuration map found for ID: %s!", cameraId),
                fullConfig);
        Set<Integer> inputFormats = zslConfig.getInputFormats();
        int [] availableInputFormats = fullConfig.getInputFormats();
        for (Integer inputFormatInteger : inputFormats) {
            int inputFormat = inputFormatInteger.intValue();
            assertArrayContains(String.format("Recommended ZSL configuration includes " +
                    "unsupported input format %d for ID: %s", inputFormat, cameraId),
                    availableInputFormats, inputFormat);

            Set<Size> inputSizes = zslConfig.getInputSizes(inputFormat);
            Size [] availableInputSizes = fullConfig.getInputSizes(inputFormat);
            assertTrue(String.format("Recommended ZSL configuration input format %d includes " +
                    "invalid input sizes for ID: %s", inputFormat, cameraId),
                    ((inputSizes != null) && (inputSizes.size() > 0)));
            for (Size inputSize : inputSizes) {
                assertArrayContains(String.format("Recommended ZSL configuration includes " +
                        "unsupported input format %d with size %s ID: %s", inputFormat,
                        inputSize.toString(), cameraId), availableInputSizes, inputSize);
            }
            Set<Integer> validOutputFormats = zslConfig.getValidOutputFormatsForInput(inputFormat);
            int [] availableValidOutputFormats = fullConfig.getValidOutputFormatsForInput(
                    inputFormat);
            for (Integer outputFormatInteger : validOutputFormats) {
                int outputFormat = outputFormatInteger.intValue();
                assertArrayContains(String.format("Recommended ZSL configuration includes " +
                        "unsupported output format %d for input %s ID: %s", outputFormat,
                        inputFormat, cameraId), availableValidOutputFormats, outputFormat);
            }
        }
    }

    private void checkFormatLatency(int format, long latencyThresholdMs,
            RecommendedStreamConfigurationMap configMap) throws Exception {
        Set<Size> availableSizes = configMap.getOutputSizes(format);
        assertNotNull(String.format("No available sizes for output format: %d", format),
                availableSizes);

        ImageReader previewReader = null;
        long threshold = (long) (latencyThresholdMs * LATENCY_TOLERANCE_FACTOR);
        // for each resolution, check that the end-to-end latency doesn't exceed the given threshold
        for (Size sz : availableSizes) {
            try {
                // Create ImageReaders, capture session and requests
                final ImageReader.OnImageAvailableListener mockListener = mock(
                        ImageReader.OnImageAvailableListener.class);
                createDefaultImageReader(sz, format, MAX_NUM_IMAGES, mockListener);
                Size previewSize = mOrderedPreviewSizes.get(0);
                previewReader = createImageReader(previewSize, ImageFormat.YUV_420_888,
                        MAX_NUM_IMAGES, new CameraTestUtils.ImageDropperListener());
                Surface previewSurface = previewReader.getSurface();
                List<Surface> surfaces = new ArrayList<Surface>();
                surfaces.add(previewSurface);
                surfaces.add(mReaderSurface);
                createSession(surfaces);
                CaptureRequest.Builder captureBuilder =
                    mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                captureBuilder.addTarget(previewSurface);
                CaptureRequest request = captureBuilder.build();

                // Let preview run for a while
                startCapture(request, /*repeating*/ true, new SimpleCaptureCallback(), mHandler);
                Thread.sleep(PREVIEW_RUN_MS);

                // Start capture.
                captureBuilder = mCamera.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
                captureBuilder.addTarget(mReaderSurface);
                request = captureBuilder.build();

                for (int i = 0; i < MAX_NUM_IMAGES; i++) {
                    startCapture(request, /*repeating*/ false, new SimpleCaptureCallback(),
                            mHandler);
                    verify(mockListener, timeout(threshold).times(1)).onImageAvailable(
                            any(ImageReader.class));
                    reset(mockListener);
                }

                // stop capture.
                stopCapture(/*fast*/ false);
            } finally {
                closeDefaultImageReader();

                if (previewReader != null) {
                    previewReader.close();
                    previewReader = null;
                }
            }

        }
    }

    private void verifyRecommendedLowLatencyConfiguration(String cameraId, CameraCharacteristics c,
            RecommendedStreamConfigurationMap lowLatencyConfig) throws Exception {
        verifyCommonRecommendedConfiguration(cameraId, c, lowLatencyConfig, /*checkNoInput*/ true,
                /*checkNoHighRes*/ false, /*checkNoHighSpeed*/ true, /*checkNoPrivate*/ false,
                /*checkNoDepth*/ true);

        try {
            openDevice(cameraId);

            Set<Integer> formats = lowLatencyConfig.getOutputFormats();
            for (Integer format : formats) {
                checkFormatLatency(format.intValue(), LOW_LATENCY_THRESHOLD_MS, lowLatencyConfig);
            }
        } finally {
            closeDevice(cameraId);
        }

    }

    @Test
    public void testRecommendedStreamConfigurations() throws Exception {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            int[] actualCapabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    actualCapabilities);

            if (!arrayContains(actualCapabilities, BC)) {
                Log.i(TAG, "Camera " + mAllCameraIds[i] +
                        ": BACKWARD_COMPATIBLE capability not supported, skipping test");
                continue;
            }

            try {
                RecommendedStreamConfigurationMap map = c.getRecommendedStreamConfigurationMap(
                        RecommendedStreamConfigurationMap.USECASE_PREVIEW - 1);
                fail("Recommended configuration map shouldn't be available for invalid " +
                        "use case!");
            } catch (IllegalArgumentException e) {
                //Expected continue
            }

            try {
                RecommendedStreamConfigurationMap map = c.getRecommendedStreamConfigurationMap(
                        RecommendedStreamConfigurationMap.USECASE_LOW_LATENCY_SNAPSHOT + 1);
                fail("Recommended configuration map shouldn't be available for invalid " +
                        "use case!");
            } catch (IllegalArgumentException e) {
                //Expected continue
            }

            RecommendedStreamConfigurationMap previewConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_PREVIEW);
            RecommendedStreamConfigurationMap videoRecordingConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_RECORD);
            RecommendedStreamConfigurationMap videoSnapshotConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_VIDEO_SNAPSHOT);
            RecommendedStreamConfigurationMap snapshotConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_SNAPSHOT);
            RecommendedStreamConfigurationMap rawConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_RAW);
            RecommendedStreamConfigurationMap zslConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_ZSL);
            RecommendedStreamConfigurationMap lowLatencyConfig =
                    c.getRecommendedStreamConfigurationMap(
                    RecommendedStreamConfigurationMap.USECASE_LOW_LATENCY_SNAPSHOT);
            if ((previewConfig == null) && (videoRecordingConfig == null) &&
                    (videoSnapshotConfig == null) && (snapshotConfig == null) &&
                    (rawConfig == null) && (zslConfig == null) && (lowLatencyConfig == null)) {
                Log.i(TAG, "Camera " + mAllCameraIds[i] +
                        " doesn't support recommended configurations, skipping test");
                continue;
            }

            assertNotNull(String.format("Mandatory recommended preview configuration map not " +
                    "found for: ID %s", mAllCameraIds[i]), previewConfig);
            verifyRecommendedPreviewConfiguration(mAllCameraIds[i], c, previewConfig);

            assertNotNull(String.format("Mandatory recommended video recording configuration map " +
                    "not found for: ID %s", mAllCameraIds[i]), videoRecordingConfig);
            verifyRecommendedVideoConfiguration(mAllCameraIds[i], c, videoRecordingConfig);

            assertNotNull(String.format("Mandatory recommended video snapshot configuration map " +
                    "not found for: ID %s", mAllCameraIds[i]), videoSnapshotConfig);
            verifyRecommendedVideoSnapshotConfiguration(mAllCameraIds[i], c, videoSnapshotConfig,
                    videoRecordingConfig);

            assertNotNull(String.format("Mandatory recommended snapshot configuration map not " +
                    "found for: ID %s", mAllCameraIds[i]), snapshotConfig);
            verifyRecommendedSnapshotConfiguration(mAllCameraIds[i], c, snapshotConfig);

            if (arrayContains(actualCapabilities, RAW)) {
                assertNotNull(String.format("Mandatory recommended raw configuration map not " +
                        "found for: ID %s", mAllCameraIds[i]), rawConfig);
                verifyRecommendedRawConfiguration(mAllCameraIds[i], c, rawConfig);
            }

            if (arrayContains(actualCapabilities, OPAQUE_REPROCESS) ||
                    arrayContains(actualCapabilities, YUV_REPROCESS)) {
                assertNotNull(String.format("Mandatory recommended ZSL configuration map not " +
                        "found for: ID %s", mAllCameraIds[i]), zslConfig);
                verifyRecommendedZSLConfiguration(mAllCameraIds[i], c, zslConfig);
            }

            if (lowLatencyConfig != null) {
                verifyRecommendedLowLatencyConfiguration(mAllCameraIds[i], c, lowLatencyConfig);
            }
        }
    }

    /**
     * Test {@link CameraCharacteristics#getKeys}
     */
    @Test
    public void testKeys() {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            mCollector.setCameraId(mAllCameraIds[i]);

            if (VERBOSE) {
                Log.v(TAG, "testKeys - testing characteristics for camera " + mAllCameraIds[i]);
            }

            List<CameraCharacteristics.Key<?>> allKeys = c.getKeys();
            assertNotNull("Camera characteristics keys must not be null", allKeys);
            assertFalse("Camera characteristics keys must have at least 1 key",
                    allKeys.isEmpty());

            for (CameraCharacteristics.Key<?> key : allKeys) {
                assertKeyPrefixValid(key.getName());

                // All characteristics keys listed must never be null
                mCollector.expectKeyValueNotNull(c, key);

                // TODO: add a check that key must not be @hide
            }

            /*
             * List of keys that must be present in camera characteristics (not null).
             *
             * Keys for LIMITED, FULL devices might be available despite lacking either
             * the hardware level or the capability. This is *OK*. This only lists the
             * *minimal* requirements for a key to be listed.
             *
             * LEGACY devices are a bit special since they map to api1 devices, so we know
             * for a fact most keys are going to be illegal there so they should never be
             * available.
             *
             * For LIMITED-level keys, if the level is >= LIMITED, then the capabilities are used to
             * do the actual checking.
             */
            {
                //                                           (Key Name)                                     (HW Level)  (Capabilities <Var-Arg>)
                expectKeyAvailable(c, CameraCharacteristics.COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES     , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AVAILABLE_MODES                         , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AE_AVAILABLE_ANTIBANDING_MODES          , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AE_AVAILABLE_MODES                      , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES          , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AE_COMPENSATION_RANGE                   , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AE_COMPENSATION_STEP                    , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AE_LOCK_AVAILABLE                       , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES                      , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AVAILABLE_EFFECTS                       , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AVAILABLE_SCENE_MODES                   , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES     , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES                     , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_AWB_LOCK_AVAILABLE                      , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_MAX_REGIONS_AE                          , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_MAX_REGIONS_AF                          , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.CONTROL_MAX_REGIONS_AWB                         , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.EDGE_AVAILABLE_EDGE_MODES                       , FULL     ,   NONE                 );
                expectKeyAvailable(c, CameraCharacteristics.FLASH_INFO_AVAILABLE                            , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES             , OPT      ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL                   , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.INFO_VERSION                                    , OPT      ,   NONE                 );
                expectKeyAvailable(c, CameraCharacteristics.JPEG_AVAILABLE_THUMBNAIL_SIZES                  , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.LENS_FACING                                     , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_AVAILABLE_APERTURES                   , FULL     ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_AVAILABLE_FILTER_DENSITIES            , FULL     ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION       , LIMITED  ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_FOCUS_DISTANCE_CALIBRATION            , LIMITED  ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_HYPERFOCAL_DISTANCE                   , LIMITED  ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_MINIMUM_FOCUS_DISTANCE                , LIMITED  ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES                  , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_MAX_NUM_INPUT_STREAMS                   , OPT      ,   YUV_REPROCESS, OPAQUE_REPROCESS);
                expectKeyAvailable(c, CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP                 , OPT      ,   CONSTRAINED_HIGH_SPEED);
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_MAX_NUM_OUTPUT_PROC                     , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_MAX_NUM_OUTPUT_PROC_STALLING            , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_MAX_NUM_OUTPUT_RAW                      , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_PARTIAL_RESULT_COUNT                    , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.REQUEST_PIPELINE_MAX_DEPTH                      , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM               , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP                 , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SCALER_CROPPING_TYPE                            , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_BLACK_LEVEL_PATTERN                      , FULL     ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE                   , OPT      ,   BC, RAW              );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT            , FULL     ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_EXPOSURE_TIME_RANGE                 , FULL     ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_MAX_FRAME_DURATION                  , FULL     ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE                    , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_SENSITIVITY_RANGE                   , FULL     ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_WHITE_LEVEL                         , OPT      ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_TIMESTAMP_SOURCE                    , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_MAX_ANALOG_SENSITIVITY                   , FULL     ,   MANUAL_SENSOR        );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_ORIENTATION                              , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SHADING_AVAILABLE_MODES                         , LIMITED  ,   MANUAL_POSTPROC, RAW );
                expectKeyAvailable(c, CameraCharacteristics.STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES     , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES   , OPT      ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES, LIMITED  ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.STATISTICS_INFO_MAX_FACE_COUNT                  , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SYNC_MAX_LATENCY                                , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.TONEMAP_AVAILABLE_TONE_MAP_MODES                , FULL     ,   MANUAL_POSTPROC      );
                expectKeyAvailable(c, CameraCharacteristics.TONEMAP_MAX_CURVE_POINTS                        , FULL     ,   MANUAL_POSTPROC      );

                // Future: Use column editors for modifying above, ignore line length to keep 1 key per line

                // TODO: check that no other 'android' keys are listed in #getKeys if they aren't in the above list
            }

            int[] actualCapabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    actualCapabilities);
            boolean isMonochrome = arrayContains(actualCapabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MONOCHROME);
            if (!isMonochrome) {
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_CALIBRATION_TRANSFORM1                   , OPT      ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_COLOR_TRANSFORM1                         , OPT      ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_FORWARD_MATRIX1                          , OPT      ,   RAW                  );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1                    , OPT      ,   RAW                  );


                // Only check for these if the second reference illuminant is included
                if (allKeys.contains(CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT2)) {
                    expectKeyAvailable(c, CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT2                    , OPT      ,   RAW                  );
                    expectKeyAvailable(c, CameraCharacteristics.SENSOR_COLOR_TRANSFORM2                         , OPT      ,   RAW                  );
                    expectKeyAvailable(c, CameraCharacteristics.SENSOR_CALIBRATION_TRANSFORM2                   , OPT      ,   RAW                  );
                    expectKeyAvailable(c, CameraCharacteristics.SENSOR_FORWARD_MATRIX2                          , OPT      ,   RAW                  );
                }
            }

            // Required key if any of RAW format output is supported
            StreamConfigurationMap config =
                    c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            assertNotNull(String.format("No stream configuration map found for: ID %s",
                    mAllCameraIds[i]), config);
            if (config.isOutputSupportedFor(ImageFormat.RAW_SENSOR) ||
                    config.isOutputSupportedFor(ImageFormat.RAW10)  ||
                    config.isOutputSupportedFor(ImageFormat.RAW12)  ||
                    config.isOutputSupportedFor(ImageFormat.RAW_PRIVATE)) {
                expectKeyAvailable(c,
                        CameraCharacteristics.CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE, OPT, BC);
            }

            // External Camera exceptional keys
            Integer hwLevel = c.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            boolean isExternalCamera = (hwLevel ==
                    CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL);
            if (!isExternalCamera) {
                expectKeyAvailable(c, CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS               , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_AVAILABLE_TEST_PATTERN_MODES             , OPT      ,   BC                   );
                expectKeyAvailable(c, CameraCharacteristics.SENSOR_INFO_PHYSICAL_SIZE                       , OPT      ,   BC                   );
            }


            // Verify version is a short text string.
            if (allKeys.contains(CameraCharacteristics.INFO_VERSION)) {
                final String TEXT_REGEX = "[\\p{Alnum}\\p{Punct}\\p{Space}]*";
                final int MAX_VERSION_LENGTH = 256;

                String version = c.get(CameraCharacteristics.INFO_VERSION);
                mCollector.expectTrue("Version contains non-text characters: " + version,
                        version.matches(TEXT_REGEX));
                mCollector.expectLessOrEqual("Version too long: " + version, MAX_VERSION_LENGTH,
                        version.length());
            }
        }
    }

    /**
     * Test values for static metadata used by the RAW capability.
     */
    @Test
    public void testStaticRawCharacteristics() {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            int[] actualCapabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    actualCapabilities);
            if (!arrayContains(actualCapabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
                Log.i(TAG, "RAW capability is not supported in camera " + mAllCameraIds[i] +
                        ". Skip the test.");
                continue;
            }

            Integer actualHwLevel = c.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            if (actualHwLevel != null && actualHwLevel == FULL) {
                mCollector.expectKeyValueContains(c,
                        CameraCharacteristics.HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
                        CameraCharacteristics.HOT_PIXEL_MODE_FAST);
            }
            mCollector.expectKeyValueContains(c,
                    CameraCharacteristics.STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES, false);
            mCollector.expectKeyValueGreaterThan(c, CameraCharacteristics.SENSOR_INFO_WHITE_LEVEL,
                    MIN_ALLOWABLE_WHITELEVEL);


            boolean isMonochrome = arrayContains(actualCapabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MONOCHROME);
            if (!isMonochrome) {
                mCollector.expectKeyValueIsIn(c,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GBRG,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR);
                // TODO: SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGB isn't supported yet.

                mCollector.expectKeyValueInRange(c,
                        CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1,
                        CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT,
                        CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1_ISO_STUDIO_TUNGSTEN);
                // Only check the range if the second reference illuminant is avaliable
                if (c.get(CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT2) != null) {
                        mCollector.expectKeyValueInRange(c,
                        CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT2,
                        (byte) CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT,
                        (byte) CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1_ISO_STUDIO_TUNGSTEN);
                }

                Rational[] zeroes = new Rational[9];
                Arrays.fill(zeroes, Rational.ZERO);

                ColorSpaceTransform zeroed = new ColorSpaceTransform(zeroes);
                mCollector.expectNotEquals("Forward Matrix1 should not contain all zeroes.", zeroed,
                        c.get(CameraCharacteristics.SENSOR_FORWARD_MATRIX1));
                mCollector.expectNotEquals("Forward Matrix2 should not contain all zeroes.", zeroed,
                        c.get(CameraCharacteristics.SENSOR_FORWARD_MATRIX2));
                mCollector.expectNotEquals("Calibration Transform1 should not contain all zeroes.",
                        zeroed, c.get(CameraCharacteristics.SENSOR_CALIBRATION_TRANSFORM1));
                mCollector.expectNotEquals("Calibration Transform2 should not contain all zeroes.",
                        zeroed, c.get(CameraCharacteristics.SENSOR_CALIBRATION_TRANSFORM2));
                mCollector.expectNotEquals("Color Transform1 should not contain all zeroes.",
                        zeroed, c.get(CameraCharacteristics.SENSOR_COLOR_TRANSFORM1));
                mCollector.expectNotEquals("Color Transform2 should not contain all zeroes.",
                        zeroed, c.get(CameraCharacteristics.SENSOR_COLOR_TRANSFORM2));
            } else {
                mCollector.expectKeyValueIsIn(c,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_MONO,
                        CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_NIR);
                // TODO: SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGB isn't supported yet.
            }

            BlackLevelPattern blackLevel = mCollector.expectKeyValueNotNull(c,
                    CameraCharacteristics.SENSOR_BLACK_LEVEL_PATTERN);
            if (blackLevel != null) {
                String blackLevelPatternString = blackLevel.toString();
                if (VERBOSE) {
                    Log.v(TAG, "Black level pattern: " + blackLevelPatternString);
                }
                int[] blackLevelPattern = new int[BlackLevelPattern.COUNT];
                blackLevel.copyTo(blackLevelPattern, /*offset*/0);
                if (isMonochrome) {
                    for (int index = 1; index < BlackLevelPattern.COUNT; index++) {
                        mCollector.expectEquals(
                                "Monochrome camera 2x2 channels blacklevel value must be the same.",
                                blackLevelPattern[index], blackLevelPattern[0]);
                    }
                }

                Integer whitelevel = c.get(CameraCharacteristics.SENSOR_INFO_WHITE_LEVEL);
                if (whitelevel != null) {
                    mCollector.expectValuesInRange("BlackLevelPattern", blackLevelPattern, 0,
                            whitelevel);
                } else {
                    mCollector.addMessage(
                            "No WhiteLevel available, cannot check BlackLevelPattern range.");
                }
            }

            // TODO: profileHueSatMap, and profileToneCurve aren't supported yet.
        }
    }

    /**
     * Test values for the available session keys.
     */
    @Test
    public void testStaticSessionKeys() throws Exception {
        for (CameraCharacteristics c : mCharacteristics) {
            List<CaptureRequest.Key<?>> availableSessionKeys = c.getAvailableSessionKeys();
            if (availableSessionKeys == null) {
                continue;
            }
            List<CaptureRequest.Key<?>> availableRequestKeys = c.getAvailableCaptureRequestKeys();

            //Every session key should be part of the available request keys
            for (CaptureRequest.Key<?> key : availableSessionKeys) {
                assertTrue("Session key:" + key.getName() + " not present in the available capture "
                        + "request keys!", availableRequestKeys.contains(key));
            }
        }
    }

    /**
     * Test values for static metadata used by the BURST capability.
     */
    @Test
    public void testStaticBurstCharacteristics() throws Exception {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            int[] actualCapabilities = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);

            // Check if the burst capability is defined
            boolean haveBurstCapability = arrayContains(actualCapabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE);
            boolean haveBC = arrayContains(actualCapabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);

            if(haveBurstCapability && !haveBC) {
                fail("Must have BACKWARD_COMPATIBLE capability if BURST_CAPTURE capability is defined");
            }

            if (!haveBC) continue;

            StreamConfigurationMap config =
                    c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            assertNotNull(String.format("No stream configuration map found for: ID %s",
                    mAllCameraIds[i]), config);
            Rect activeRect = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            Size sensorSize = new Size(activeRect.width(), activeRect.height());

            // Ensure that max YUV size matches max JPEG size
            Size maxYuvSize = CameraTestUtils.getMaxSize(
                    config.getOutputSizes(ImageFormat.YUV_420_888));
            Size maxFastYuvSize = maxYuvSize;

            Size[] slowYuvSizes = config.getHighResolutionOutputSizes(ImageFormat.YUV_420_888);
            Size maxSlowYuvSizeLessThan24M = null;
            if (haveBurstCapability && slowYuvSizes != null && slowYuvSizes.length > 0) {
                Size maxSlowYuvSize = CameraTestUtils.getMaxSize(slowYuvSizes);
                final int SIZE_24MP_BOUND = 24000000;
                maxSlowYuvSizeLessThan24M =
                        CameraTestUtils.getMaxSizeWithBound(slowYuvSizes, SIZE_24MP_BOUND);
                maxYuvSize = CameraTestUtils.getMaxSize(new Size[]{maxYuvSize, maxSlowYuvSize});
            }

            Size maxJpegSize = CameraTestUtils.getMaxSize(CameraTestUtils.getSupportedSizeForFormat(
                    ImageFormat.JPEG, mAllCameraIds[i], mCameraManager));

            boolean haveMaxYuv = maxYuvSize != null ?
                (maxJpegSize.getWidth() <= maxYuvSize.getWidth() &&
                        maxJpegSize.getHeight() <= maxYuvSize.getHeight()) : false;

            Pair<Boolean, Size> maxYuvMatchSensorPair = isSizeWithinSensorMargin(maxYuvSize,
                    sensorSize);

            // No need to do null check since framework will generate the key if HAL don't supply
            boolean haveAeLock = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.CONTROL_AE_LOCK_AVAILABLE);
            boolean haveAwbLock = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.CONTROL_AWB_LOCK_AVAILABLE);

            // Ensure that some >=8MP YUV output is fast enough - needs to be at least 20 fps

            long maxFastYuvRate =
                    config.getOutputMinFrameDuration(ImageFormat.YUV_420_888, maxFastYuvSize);
            final long MIN_8MP_DURATION_BOUND_NS = 50000000; // 50 ms, 20 fps
            boolean haveFastYuvRate = maxFastYuvRate <= MIN_8MP_DURATION_BOUND_NS;

            final int SIZE_8MP_BOUND = 8000000;
            boolean havefast8MPYuv = (maxFastYuvSize.getWidth() * maxFastYuvSize.getHeight()) >
                    SIZE_8MP_BOUND;

            // Ensure that max YUV output smaller than 24MP is fast enough
            // - needs to be at least 10 fps
            final long MIN_MAXSIZE_DURATION_BOUND_NS = 100000000; // 100 ms, 10 fps
            long maxYuvRate = maxFastYuvRate;
            if (maxSlowYuvSizeLessThan24M != null) {
                maxYuvRate = config.getOutputMinFrameDuration(
                        ImageFormat.YUV_420_888, maxSlowYuvSizeLessThan24M);
            }
            boolean haveMaxYuvRate = maxYuvRate <= MIN_MAXSIZE_DURATION_BOUND_NS;

            // Ensure that there's an FPS range that's fast enough to capture at above
            // minFrameDuration, for full-auto bursts at the fast resolutions
            Range[] fpsRanges = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
            float minYuvFps = 1.f / maxFastYuvRate;

            boolean haveFastAeTargetFps = false;
            for (Range<Integer> r : fpsRanges) {
                if (r.getLower() >= minYuvFps) {
                    haveFastAeTargetFps = true;
                    break;
                }
            }

            // Ensure that maximum sync latency is small enough for fast setting changes, even if
            // it's not quite per-frame

            Integer maxSyncLatencyValue = c.get(CameraCharacteristics.SYNC_MAX_LATENCY);
            assertNotNull(String.format("No sync latency declared for ID %s", mAllCameraIds[i]),
                    maxSyncLatencyValue);

            int maxSyncLatency = maxSyncLatencyValue;
            final long MAX_LATENCY_BOUND = 4;
            boolean haveFastSyncLatency =
                (maxSyncLatency <= MAX_LATENCY_BOUND) && (maxSyncLatency >= 0);

            if (haveBurstCapability) {
                assertTrue("Must have slow YUV size array when BURST_CAPTURE capability is defined!",
                        slowYuvSizes != null);
                assertTrue(
                        String.format("BURST-capable camera device %s does not have maximum YUV " +
                                "size that is at least max JPEG size",
                                mAllCameraIds[i]),
                        haveMaxYuv);
                assertTrue(
                        String.format("BURST-capable camera device %s max-resolution " +
                                "YUV frame rate is too slow" +
                                "(%d ns min frame duration reported, less than %d ns expected)",
                                mAllCameraIds[i], maxYuvRate, MIN_MAXSIZE_DURATION_BOUND_NS),
                        haveMaxYuvRate);
                assertTrue(
                        String.format("BURST-capable camera device %s >= 8MP YUV output " +
                                "frame rate is too slow" +
                                "(%d ns min frame duration reported, less than %d ns expected)",
                                mAllCameraIds[i], maxYuvRate, MIN_8MP_DURATION_BOUND_NS),
                        haveFastYuvRate);
                assertTrue(
                        String.format("BURST-capable camera device %s does not list an AE target " +
                                " FPS range with min FPS >= %f, for full-AUTO bursts",
                                mAllCameraIds[i], minYuvFps),
                        haveFastAeTargetFps);
                assertTrue(
                        String.format("BURST-capable camera device %s YUV sync latency is too long" +
                                "(%d frames reported, [0, %d] frames expected)",
                                mAllCameraIds[i], maxSyncLatency, MAX_LATENCY_BOUND),
                        haveFastSyncLatency);
                assertTrue(
                        String.format("BURST-capable camera device %s max YUV size %s should be" +
                                "close to active array size %s or cropped active array size %s",
                                mAllCameraIds[i], maxYuvSize.toString(), sensorSize.toString(),
                                maxYuvMatchSensorPair.second.toString()),
                        maxYuvMatchSensorPair.first.booleanValue());
                assertTrue(
                        String.format("BURST-capable camera device %s does not support AE lock",
                                mAllCameraIds[i]),
                        haveAeLock);
                assertTrue(
                        String.format("BURST-capable camera device %s does not support AWB lock",
                                mAllCameraIds[i]),
                        haveAwbLock);
            } else {
                assertTrue("Must have null slow YUV size array when no BURST_CAPTURE capability!",
                        slowYuvSizes == null);
                assertTrue(
                        String.format("Camera device %s has all the requirements for BURST" +
                                " capability but does not report it!", mAllCameraIds[i]),
                        !(haveMaxYuv && haveMaxYuvRate && haveFastYuvRate && haveFastAeTargetFps &&
                                haveFastSyncLatency && maxYuvMatchSensorPair.first.booleanValue() &&
                                haveAeLock && haveAwbLock));
            }
        }
    }

    /**
     * Check reprocessing capabilities.
     */
    @Test
    public void testReprocessingCharacteristics() {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            Log.i(TAG, "testReprocessingCharacteristics: Testing camera ID " + mAllCameraIds[i]);

            CameraCharacteristics c = mCharacteristics.get(i);
            int[] capabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    capabilities);
            boolean supportYUV = arrayContains(capabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
            boolean supportOpaque = arrayContains(capabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING);
            StreamConfigurationMap configs =
                    c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            Integer maxNumInputStreams =
                    c.get(CameraCharacteristics.REQUEST_MAX_NUM_INPUT_STREAMS);
            int[] availableEdgeModes = c.get(CameraCharacteristics.EDGE_AVAILABLE_EDGE_MODES);
            int[] availableNoiseReductionModes = c.get(
                    CameraCharacteristics.NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES);

            int[] inputFormats = configs.getInputFormats();
            int[] outputFormats = configs.getOutputFormats();
            boolean isMonochromeWithY8 = arrayContains(capabilities, MONOCHROME)
                    && arrayContains(outputFormats, ImageFormat.Y8);

            boolean supportZslEdgeMode = false;
            boolean supportZslNoiseReductionMode = false;
            boolean supportHiQNoiseReductionMode = false;
            boolean supportHiQEdgeMode = false;

            if (availableEdgeModes != null) {
                supportZslEdgeMode = Arrays.asList(CameraTestUtils.toObject(availableEdgeModes)).
                        contains(CaptureRequest.EDGE_MODE_ZERO_SHUTTER_LAG);
                supportHiQEdgeMode = Arrays.asList(CameraTestUtils.toObject(availableEdgeModes)).
                        contains(CaptureRequest.EDGE_MODE_HIGH_QUALITY);
            }

            if (availableNoiseReductionModes != null) {
                supportZslNoiseReductionMode = Arrays.asList(
                        CameraTestUtils.toObject(availableNoiseReductionModes)).contains(
                        CaptureRequest.NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG);
                supportHiQNoiseReductionMode = Arrays.asList(
                        CameraTestUtils.toObject(availableNoiseReductionModes)).contains(
                        CaptureRequest.NOISE_REDUCTION_MODE_HIGH_QUALITY);
            }

            if (supportYUV || supportOpaque) {
                mCollector.expectTrue("Support reprocessing but max number of input stream is " +
                        maxNumInputStreams, maxNumInputStreams != null && maxNumInputStreams > 0);
                mCollector.expectTrue("Support reprocessing but EDGE_MODE_ZERO_SHUTTER_LAG is " +
                        "not supported", supportZslEdgeMode);
                mCollector.expectTrue("Support reprocessing but " +
                        "NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG is not supported",
                        supportZslNoiseReductionMode);

                // For reprocessing, if we only require OFF and ZSL mode, it will be just like jpeg
                // encoding. We implicitly require FAST to make reprocessing meaningful, which means
                // that we also require HIGH_QUALITY.
                mCollector.expectTrue("Support reprocessing but EDGE_MODE_HIGH_QUALITY is " +
                        "not supported", supportHiQEdgeMode);
                mCollector.expectTrue("Support reprocessing but " +
                        "NOISE_REDUCTION_MODE_HIGH_QUALITY is not supported",
                        supportHiQNoiseReductionMode);

                // Verify mandatory input formats are supported
                mCollector.expectTrue("YUV_420_888 input must be supported for YUV reprocessing",
                        !supportYUV || arrayContains(inputFormats, ImageFormat.YUV_420_888));
                mCollector.expectTrue("Y8 input must be supported for YUV reprocessing on " +
                        "MONOCHROME devices with Y8 support", !supportYUV || !isMonochromeWithY8
                        || arrayContains(inputFormats, ImageFormat.Y8));
                mCollector.expectTrue("PRIVATE input must be supported for OPAQUE reprocessing",
                        !supportOpaque || arrayContains(inputFormats, ImageFormat.PRIVATE));

                // max capture stall must be reported if one of the reprocessing is supported.
                final int MAX_ALLOWED_STALL_FRAMES = 4;
                Integer maxCaptureStall = c.get(CameraCharacteristics.REPROCESS_MAX_CAPTURE_STALL);
                mCollector.expectTrue("max capture stall must be non-null and no larger than "
                        + MAX_ALLOWED_STALL_FRAMES,
                        maxCaptureStall != null && maxCaptureStall <= MAX_ALLOWED_STALL_FRAMES);

                for (int input : inputFormats) {
                    // Verify mandatory output formats are supported
                    int[] outputFormatsForInput = configs.getValidOutputFormatsForInput(input);
                    mCollector.expectTrue(
                        "YUV_420_888 output must be supported for reprocessing",
                        input == ImageFormat.Y8
                        || arrayContains(outputFormatsForInput, ImageFormat.YUV_420_888));
                    mCollector.expectTrue(
                        "Y8 output must be supported for reprocessing on MONOCHROME devices with"
                        + " Y8 support", !isMonochromeWithY8 || input == ImageFormat.YUV_420_888
                        || arrayContains(outputFormatsForInput, ImageFormat.Y8));
                    mCollector.expectTrue("JPEG output must be supported for reprocessing",
                            arrayContains(outputFormatsForInput, ImageFormat.JPEG));

                    // Verify camera can output the reprocess input formats and sizes.
                    Size[] inputSizes = configs.getInputSizes(input);
                    Size[] outputSizes = configs.getOutputSizes(input);
                    Size[] highResOutputSizes = configs.getHighResolutionOutputSizes(input);
                    mCollector.expectTrue("no input size supported for format " + input,
                            inputSizes.length > 0);
                    mCollector.expectTrue("no output size supported for format " + input,
                            outputSizes.length > 0);

                    for (Size inputSize : inputSizes) {
                        mCollector.expectTrue("Camera must be able to output the supported " +
                                "reprocessing input size",
                                arrayContains(outputSizes, inputSize) ||
                                arrayContains(highResOutputSizes, inputSize));
                    }
                }
            } else {
                mCollector.expectTrue("Doesn't support reprocessing but report input format: " +
                        Arrays.toString(inputFormats), inputFormats.length == 0);
                mCollector.expectTrue("Doesn't support reprocessing but max number of input " +
                        "stream is " + maxNumInputStreams,
                        maxNumInputStreams == null || maxNumInputStreams == 0);
                mCollector.expectTrue("Doesn't support reprocessing but " +
                        "EDGE_MODE_ZERO_SHUTTER_LAG is supported", !supportZslEdgeMode);
                mCollector.expectTrue("Doesn't support reprocessing but " +
                        "NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG is supported",
                        !supportZslNoiseReductionMode);
            }
        }
    }

    /**
     * Check depth output capability
     */
    @Test
    public void testDepthOutputCharacteristics() {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            Log.i(TAG, "testDepthOutputCharacteristics: Testing camera ID " + mAllCameraIds[i]);

            CameraCharacteristics c = mCharacteristics.get(i);
            int[] capabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    capabilities);
            boolean supportDepth = arrayContains(capabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT);
            StreamConfigurationMap configs =
                    c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

            int[] outputFormats = configs.getOutputFormats();
            boolean hasDepth16 = arrayContains(outputFormats, ImageFormat.DEPTH16);

            Boolean depthIsExclusive = c.get(CameraCharacteristics.DEPTH_DEPTH_IS_EXCLUSIVE);

            float[] poseRotation = c.get(CameraCharacteristics.LENS_POSE_ROTATION);
            float[] poseTranslation = c.get(CameraCharacteristics.LENS_POSE_TRANSLATION);
            Integer poseReference = c.get(CameraCharacteristics.LENS_POSE_REFERENCE);
            float[] cameraIntrinsics = c.get(CameraCharacteristics.LENS_INTRINSIC_CALIBRATION);
            float[] distortion = getLensDistortion(c);
            Size pixelArraySize = c.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
            Rect precorrectionArray = c.get(
                CameraCharacteristics.SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE);
            Rect activeArray = c.get(
                CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            Integer facing = c.get(CameraCharacteristics.LENS_FACING);
            float jpegAspectRatioThreshold = .01f;
            boolean jpegSizeMatch = false;

            // Verify pre-correction array encloses active array
            mCollector.expectTrue("preCorrectionArray [" + precorrectionArray.left + ", " +
                    precorrectionArray.top + ", " + precorrectionArray.right + ", " +
                    precorrectionArray.bottom + "] does not enclose activeArray[" +
                    activeArray.left + ", " + activeArray.top + ", " + activeArray.right +
                    ", " + activeArray.bottom,
                    precorrectionArray.contains(activeArray.left, activeArray.top) &&
                    precorrectionArray.contains(activeArray.right-1, activeArray.bottom-1));

            // Verify pixel array encloses pre-correction array
            mCollector.expectTrue("preCorrectionArray [" + precorrectionArray.left + ", " +
                    precorrectionArray.top + ", " + precorrectionArray.right + ", " +
                    precorrectionArray.bottom + "] isn't enclosed by pixelArray[" +
                    pixelArraySize.getWidth() + ", " + pixelArraySize.getHeight() + "]",
                    precorrectionArray.left >= 0 &&
                    precorrectionArray.left < pixelArraySize.getWidth() &&
                    precorrectionArray.right > 0 &&
                    precorrectionArray.right <= pixelArraySize.getWidth() &&
                    precorrectionArray.top >= 0 &&
                    precorrectionArray.top < pixelArraySize.getHeight() &&
                    precorrectionArray.bottom > 0 &&
                    precorrectionArray.bottom <= pixelArraySize.getHeight());

            if (supportDepth) {
                mCollector.expectTrue("Supports DEPTH_OUTPUT but does not support DEPTH16",
                        hasDepth16);
                if (hasDepth16) {
                    Size[] depthSizes = configs.getOutputSizes(ImageFormat.DEPTH16);
                    Size[] jpegSizes = configs.getOutputSizes(ImageFormat.JPEG);
                    mCollector.expectTrue("Supports DEPTH_OUTPUT but no sizes for DEPTH16 supported!",
                            depthSizes != null && depthSizes.length > 0);
                    if (depthSizes != null) {
                        for (Size depthSize : depthSizes) {
                            mCollector.expectTrue("All depth16 sizes must be positive",
                                    depthSize.getWidth() > 0 && depthSize.getHeight() > 0);
                            long minFrameDuration = configs.getOutputMinFrameDuration(
                                    ImageFormat.DEPTH16, depthSize);
                            mCollector.expectTrue("Non-negative min frame duration for depth size "
                                    + depthSize + " expected, got " + minFrameDuration,
                                    minFrameDuration >= 0);
                            long stallDuration = configs.getOutputStallDuration(
                                    ImageFormat.DEPTH16, depthSize);
                            mCollector.expectTrue("Non-negative stall duration for depth size "
                                    + depthSize + " expected, got " + stallDuration,
                                    stallDuration >= 0);
                            if ((jpegSizes != null) && (!jpegSizeMatch)) {
                                for (Size jpegSize : jpegSizes) {
                                    if (jpegSize.equals(depthSize)) {
                                        jpegSizeMatch = true;
                                        break;
                                    } else {
                                        float depthAR = (float) depthSize.getWidth() /
                                                (float) depthSize.getHeight();
                                        float jpegAR = (float) jpegSize.getWidth() /
                                                (float) jpegSize.getHeight();
                                        if (Math.abs(depthAR - jpegAR) <=
                                                jpegAspectRatioThreshold) {
                                            jpegSizeMatch = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (arrayContains(outputFormats, ImageFormat.DEPTH_POINT_CLOUD)) {
                    Size[] depthCloudSizes = configs.getOutputSizes(ImageFormat.DEPTH_POINT_CLOUD);
                    mCollector.expectTrue("Supports DEPTH_POINT_CLOUD " +
                            "but no sizes for DEPTH_POINT_CLOUD supported!",
                            depthCloudSizes != null && depthCloudSizes.length > 0);
                    if (depthCloudSizes != null) {
                        for (Size depthCloudSize : depthCloudSizes) {
                            mCollector.expectTrue("All depth point cloud sizes must be nonzero",
                                    depthCloudSize.getWidth() > 0);
                            mCollector.expectTrue("All depth point cloud sizes must be N x 1",
                                    depthCloudSize.getHeight() == 1);
                            long minFrameDuration = configs.getOutputMinFrameDuration(
                                    ImageFormat.DEPTH_POINT_CLOUD, depthCloudSize);
                            mCollector.expectTrue("Non-negative min frame duration for depth size "
                                    + depthCloudSize + " expected, got " + minFrameDuration,
                                    minFrameDuration >= 0);
                            long stallDuration = configs.getOutputStallDuration(
                                    ImageFormat.DEPTH_POINT_CLOUD, depthCloudSize);
                            mCollector.expectTrue("Non-negative stall duration for depth size "
                                    + depthCloudSize + " expected, got " + stallDuration,
                                    stallDuration >= 0);
                        }
                    }
                }
                if (arrayContains(outputFormats, ImageFormat.DEPTH_JPEG)) {
                    mCollector.expectTrue("Supports DEPTH_JPEG but has no DEPTH16 support!",
                            hasDepth16);
                    mCollector.expectTrue("Supports DEPTH_JPEG but DEPTH_IS_EXCLUSIVE is not " +
                            "defined", depthIsExclusive != null);
                    mCollector.expectTrue("Supports DEPTH_JPEG but DEPTH_IS_EXCLUSIVE is true",
                            !depthIsExclusive.booleanValue());
                    Size[] depthJpegSizes = configs.getOutputSizes(ImageFormat.DEPTH_JPEG);
                    mCollector.expectTrue("Supports DEPTH_JPEG " +
                            "but no sizes for DEPTH_JPEG supported!",
                            depthJpegSizes != null && depthJpegSizes.length > 0);
                    mCollector.expectTrue("Supports DEPTH_JPEG but there are no JPEG sizes with" +
                            " matching DEPTH16 aspect ratio", jpegSizeMatch);
                    if (depthJpegSizes != null) {
                        for (Size depthJpegSize : depthJpegSizes) {
                            mCollector.expectTrue("All depth jpeg sizes must be nonzero",
                                    depthJpegSize.getWidth() > 0 && depthJpegSize.getHeight() > 0);
                            long minFrameDuration = configs.getOutputMinFrameDuration(
                                    ImageFormat.DEPTH_JPEG, depthJpegSize);
                            mCollector.expectTrue("Non-negative min frame duration for depth jpeg" +
                                   " size " + depthJpegSize + " expected, got " + minFrameDuration,
                                    minFrameDuration >= 0);
                            long stallDuration = configs.getOutputStallDuration(
                                    ImageFormat.DEPTH_JPEG, depthJpegSize);
                            mCollector.expectTrue("Non-negative stall duration for depth jpeg size "
                                    + depthJpegSize + " expected, got " + stallDuration,
                                    stallDuration >= 0);
                        }
                    }
                } else {
                    boolean canSupportDynamicDepth = jpegSizeMatch && !depthIsExclusive;
                    mCollector.expectTrue("Device must support DEPTH_JPEG, please check whether " +
                            "library libdepthphoto.so is part of the device PRODUCT_PACKAGES",
                            !canSupportDynamicDepth);
                }


                mCollector.expectTrue("Supports DEPTH_OUTPUT but DEPTH_IS_EXCLUSIVE is not defined",
                        depthIsExclusive != null);

                verifyLensCalibration(poseRotation, poseTranslation, poseReference,
                        cameraIntrinsics, distortion, precorrectionArray, facing);

            } else {
                boolean hasFields =
                    hasDepth16 && (poseTranslation != null) &&
                    (poseRotation != null) && (cameraIntrinsics != null) &&
                    (distortion != null) && (depthIsExclusive != null);

                mCollector.expectTrue(
                        "All necessary depth fields defined, but DEPTH_OUTPUT capability is not listed",
                        !hasFields);

                boolean reportCalibration = poseTranslation != null ||
                        poseRotation != null || cameraIntrinsics !=null;
                // Verify calibration keys are co-existing
                if (reportCalibration) {
                    mCollector.expectTrue(
                            "Calibration keys must be co-existing",
                            poseTranslation != null && poseRotation != null &&
                            cameraIntrinsics !=null);
                }

                boolean reportDistortion = distortion != null;
                if (reportDistortion) {
                    mCollector.expectTrue(
                            "Calibration keys must present where distortion is reported",
                            reportCalibration);
                }
            }
        }
    }

    private void verifyLensCalibration(float[] poseRotation, float[] poseTranslation,
            Integer poseReference, float[] cameraIntrinsics, float[] distortion,
            Rect precorrectionArray, Integer facing) {

        mCollector.expectTrue(
            "LENS_POSE_ROTATION not right size",
            poseRotation != null && poseRotation.length == 4);
        mCollector.expectTrue(
            "LENS_POSE_TRANSLATION not right size",
            poseTranslation != null && poseTranslation.length == 3);
        mCollector.expectTrue(
            "LENS_POSE_REFERENCE is not defined",
            poseReference != null);
        mCollector.expectTrue(
            "LENS_INTRINSIC_CALIBRATION not right size",
            cameraIntrinsics != null && cameraIntrinsics.length == 5);
        mCollector.expectTrue(
            "LENS_DISTORTION not right size",
            distortion != null && distortion.length == 6);

        if (poseRotation != null && poseRotation.length == 4) {
            float normSq =
                    poseRotation[0] * poseRotation[0] +
                    poseRotation[1] * poseRotation[1] +
                    poseRotation[2] * poseRotation[2] +
                    poseRotation[3] * poseRotation[3];
            mCollector.expectTrue(
                "LENS_POSE_ROTATION quarternion must be unit-length",
                0.9999f < normSq && normSq < 1.0001f);

            if (facing.intValue() == CameraMetadata.LENS_FACING_FRONT ||
                    facing.intValue() == CameraMetadata.LENS_FACING_BACK) {
                // Use the screen's natural facing to test pose rotation
                int[] facingSensor = new int[]{0, 0, 1};
                float[][] r = new float[][] {
                        { 1.0f - 2 * poseRotation[1] * poseRotation[1]
                              - 2 * poseRotation[2] * poseRotation[2],
                          2 * poseRotation[0] * poseRotation[1]
                              - 2 * poseRotation[2] * poseRotation[3],
                          2 * poseRotation[0] * poseRotation[2]
                              + 2 * poseRotation[1] * poseRotation[3] },
                        { 2 * poseRotation[0] * poseRotation[1]
                              + 2 * poseRotation[2] * poseRotation[3],
                          1.0f - 2 * poseRotation[0] * poseRotation[0]
                              - 2 * poseRotation[2] * poseRotation[2],
                          2 * poseRotation[1] * poseRotation[2]
                              - 2 * poseRotation[0] * poseRotation[3] },
                        { 2 * poseRotation[0] * poseRotation[2]
                              - 2 * poseRotation[1] * poseRotation[3],
                          2 * poseRotation[1] * poseRotation[2]
                              + 2 * poseRotation[0] * poseRotation[3],
                          1.0f - 2 * poseRotation[0] * poseRotation[0]
                              - 2 * poseRotation[1] * poseRotation[1] }
                      };
                // The screen natural facing in camera's coordinate system
                float facingCameraX = r[0][0] * facingSensor[0] + r[0][1] * facingSensor[1] +
                        r[0][2] * facingSensor[2];
                float facingCameraY = r[1][0] * facingSensor[0] + r[1][1] * facingSensor[1] +
                        r[1][2] * facingSensor[2];
                float facingCameraZ = r[2][0] * facingSensor[0] + r[2][1] * facingSensor[1] +
                        r[2][2] * facingSensor[2];

                mCollector.expectTrue("LENS_POSE_ROTATION must be consistent with lens facing",
                        (facingCameraZ > 0) ^
                        (facing.intValue() == CameraMetadata.LENS_FACING_BACK));

                if (poseReference == CameraCharacteristics.LENS_POSE_REFERENCE_UNDEFINED) {
                    mCollector.expectTrue(
                            "LENS_POSE_ROTATION quarternion must be consistent with camera's " +
                            "default facing",
                            Math.abs(facingCameraX) < 0.00001f &&
                            Math.abs(facingCameraY) < 0.00001f &&
                            Math.abs(facingCameraZ) > 0.99999f &&
                            Math.abs(facingCameraZ) < 1.00001f);
                }
            }

            // TODO: Cross-validate orientation and poseRotation
        }

        if (poseTranslation != null && poseTranslation.length == 3) {
            float normSq =
                    poseTranslation[0] * poseTranslation[0] +
                    poseTranslation[1] * poseTranslation[1] +
                    poseTranslation[2] * poseTranslation[2];
            mCollector.expectTrue("Pose translation is larger than 1 m",
                    normSq < 1.f);

            // Pose translation should be all 0s for UNDEFINED pose reference.
            if (poseReference != null && poseReference ==
                    CameraCharacteristics.LENS_POSE_REFERENCE_UNDEFINED) {
                mCollector.expectTrue("Pose translation aren't all 0s ",
                        normSq < 0.00001f);
            }
        }

        if (poseReference != null) {
            int ref = poseReference;
            boolean validReference = false;
            switch (ref) {
                case CameraCharacteristics.LENS_POSE_REFERENCE_PRIMARY_CAMERA:
                case CameraCharacteristics.LENS_POSE_REFERENCE_GYROSCOPE:
                case CameraCharacteristics.LENS_POSE_REFERENCE_UNDEFINED:
                    // Allowed values
                    validReference = true;
                    break;
                default:
            }
            mCollector.expectTrue("POSE_REFERENCE has unknown value", validReference);
        }

        mCollector.expectTrue("Does not have precorrection active array defined",
                precorrectionArray != null);

        if (cameraIntrinsics != null && precorrectionArray != null) {
            float fx = cameraIntrinsics[0];
            float fy = cameraIntrinsics[1];
            float cx = cameraIntrinsics[2];
            float cy = cameraIntrinsics[3];
            float s = cameraIntrinsics[4];
            mCollector.expectTrue("Optical center expected to be within precorrection array",
                    0 <= cx && cx < precorrectionArray.width() &&
                    0 <= cy && cy < precorrectionArray.height());

            // TODO: Verify focal lengths and skew are reasonable
        }

        if (distortion != null) {
            // TODO: Verify radial distortion
        }

    }

    /**
     * Cross-check StreamConfigurationMap output
     */
    @Test
    public void testStreamConfigurationMap() throws Exception {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            Log.i(TAG, "testStreamConfigurationMap: Testing camera ID " + mAllCameraIds[i]);
            CameraCharacteristics c = mCharacteristics.get(i);
            StreamConfigurationMap config =
                    c.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            assertNotNull(String.format("No stream configuration map found for: ID %s",
                            mAllCameraIds[i]), config);

            int[] actualCapabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    actualCapabilities);

            if (arrayContains(actualCapabilities, BC)) {
                assertTrue("ImageReader must be supported",
                    config.isOutputSupportedFor(android.media.ImageReader.class));
                assertTrue("MediaRecorder must be supported",
                    config.isOutputSupportedFor(android.media.MediaRecorder.class));
                assertTrue("MediaCodec must be supported",
                    config.isOutputSupportedFor(android.media.MediaCodec.class));
                assertTrue("Allocation must be supported",
                    config.isOutputSupportedFor(android.renderscript.Allocation.class));
                assertTrue("SurfaceHolder must be supported",
                    config.isOutputSupportedFor(android.view.SurfaceHolder.class));
                assertTrue("SurfaceTexture must be supported",
                    config.isOutputSupportedFor(android.graphics.SurfaceTexture.class));

                assertTrue("YUV_420_888 must be supported",
                    config.isOutputSupportedFor(ImageFormat.YUV_420_888));
                assertTrue("JPEG must be supported",
                    config.isOutputSupportedFor(ImageFormat.JPEG));
            } else {
                assertTrue("YUV_420_88 may not be supported if BACKWARD_COMPATIBLE capability is not listed",
                    !config.isOutputSupportedFor(ImageFormat.YUV_420_888));
                assertTrue("JPEG may not be supported if BACKWARD_COMPATIBLE capability is not listed",
                    !config.isOutputSupportedFor(ImageFormat.JPEG));
            }

            // Check RAW

            if (arrayContains(actualCapabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
                assertTrue("RAW_SENSOR must be supported if RAW capability is advertised",
                    config.isOutputSupportedFor(ImageFormat.RAW_SENSOR));
            }

            // Cross check public formats and sizes

            int[] supportedFormats = config.getOutputFormats();
            for (int format : supportedFormats) {
                assertTrue("Format " + format + " fails cross check",
                        config.isOutputSupportedFor(format));
                List<Size> supportedSizes = CameraTestUtils.getAscendingOrderSizes(
                        Arrays.asList(config.getOutputSizes(format)), /*ascending*/true);
                if (arrayContains(actualCapabilities,
                        CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE)) {
                    supportedSizes.addAll(
                        Arrays.asList(config.getHighResolutionOutputSizes(format)));
                    supportedSizes = CameraTestUtils.getAscendingOrderSizes(
                        supportedSizes, /*ascending*/true);
                }
                assertTrue("Supported format " + format + " has no sizes listed",
                        supportedSizes.size() > 0);
                for (int j = 0; j < supportedSizes.size(); j++) {
                    Size size = supportedSizes.get(j);
                    if (VERBOSE) {
                        Log.v(TAG,
                                String.format("Testing camera %s, format %d, size %s",
                                        mAllCameraIds[i], format, size.toString()));
                    }

                    long stallDuration = config.getOutputStallDuration(format, size);
                    switch(format) {
                        case ImageFormat.YUV_420_888:
                            assertTrue("YUV_420_888 may not have a non-zero stall duration",
                                    stallDuration == 0);
                            break;
                        case ImageFormat.JPEG:
                        case ImageFormat.RAW_SENSOR:
                            final float TOLERANCE_FACTOR = 2.0f;
                            long prevDuration = 0;
                            if (j > 0) {
                                prevDuration = config.getOutputStallDuration(
                                        format, supportedSizes.get(j - 1));
                            }
                            long nextDuration = Long.MAX_VALUE;
                            if (j < (supportedSizes.size() - 1)) {
                                nextDuration = config.getOutputStallDuration(
                                        format, supportedSizes.get(j + 1));
                            }
                            long curStallDuration = config.getOutputStallDuration(format, size);
                            // Stall duration should be in a reasonable range: larger size should
                            // normally have larger stall duration.
                            mCollector.expectInRange("Stall duration (format " + format +
                                    " and size " + size + ") is not in the right range",
                                    curStallDuration,
                                    (long) (prevDuration / TOLERANCE_FACTOR),
                                    (long) (nextDuration * TOLERANCE_FACTOR));
                            break;
                        default:
                            assertTrue("Negative stall duration for format " + format,
                                    stallDuration >= 0);
                            break;
                    }
                    long minDuration = config.getOutputMinFrameDuration(format, size);
                    if (arrayContains(actualCapabilities,
                            CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
                        assertTrue("MANUAL_SENSOR capability, need positive min frame duration for"
                                + "format " + format + " for size " + size + " minDuration " +
                                minDuration,
                                minDuration > 0);
                    } else {
                        assertTrue("Need non-negative min frame duration for format " + format,
                                minDuration >= 0);
                    }

                    // todo: test opaque image reader when it's supported.
                    if (format != ImageFormat.PRIVATE) {
                        ImageReader testReader = ImageReader.newInstance(
                            size.getWidth(),
                            size.getHeight(),
                            format,
                            1);
                        Surface testSurface = testReader.getSurface();

                        assertTrue(
                            String.format("isOutputSupportedFor fails for config %s, format %d",
                                    size.toString(), format),
                            config.isOutputSupportedFor(testSurface));

                        testReader.close();
                    }
                } // sizes

                // Try an invalid size in this format, should round
                Size invalidSize = findInvalidSize(supportedSizes);
                int MAX_ROUNDING_WIDTH = 1920;
                // todo: test opaque image reader when it's supported.
                if (format != ImageFormat.PRIVATE &&
                        invalidSize.getWidth() <= MAX_ROUNDING_WIDTH) {
                    ImageReader testReader = ImageReader.newInstance(
                                                                     invalidSize.getWidth(),
                                                                     invalidSize.getHeight(),
                                                                     format,
                                                                     1);
                    Surface testSurface = testReader.getSurface();

                    assertTrue(
                               String.format("isOutputSupportedFor fails for config %s, %d",
                                       invalidSize.toString(), format),
                               config.isOutputSupportedFor(testSurface));

                    testReader.close();
                }
            } // formats

            // Cross-check opaque format and sizes
            if (arrayContains(actualCapabilities, BC)) {
                SurfaceTexture st = new SurfaceTexture(1);
                Surface surf = new Surface(st);

                Size[] opaqueSizes = CameraTestUtils.getSupportedSizeForClass(SurfaceTexture.class,
                        mAllCameraIds[i], mCameraManager);
                assertTrue("Opaque format has no sizes listed",
                        opaqueSizes.length > 0);
                for (Size size : opaqueSizes) {
                    long stallDuration = config.getOutputStallDuration(SurfaceTexture.class, size);
                    assertTrue("Opaque output may not have a non-zero stall duration",
                            stallDuration == 0);

                    long minDuration = config.getOutputMinFrameDuration(SurfaceTexture.class, size);
                    if (arrayContains(actualCapabilities,
                                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
                        assertTrue("MANUAL_SENSOR capability, need positive min frame duration for"
                                + "opaque format",
                                minDuration > 0);
                    } else {
                        assertTrue("Need non-negative min frame duration for opaque format ",
                                minDuration >= 0);
                    }
                    st.setDefaultBufferSize(size.getWidth(), size.getHeight());

                    assertTrue(
                            String.format("isOutputSupportedFor fails for SurfaceTexture config %s",
                                    size.toString()),
                            config.isOutputSupportedFor(surf));

                } // opaque sizes

                // Try invalid opaque size, should get rounded
                Size invalidSize = findInvalidSize(opaqueSizes);
                st.setDefaultBufferSize(invalidSize.getWidth(), invalidSize.getHeight());
                assertTrue(
                        String.format("isOutputSupportedFor fails for SurfaceTexture config %s",
                                invalidSize.toString()),
                        config.isOutputSupportedFor(surf));

            }
        } // mCharacteristics
    }

    /**
     * Test high speed capability and cross-check the high speed sizes and fps ranges from
     * the StreamConfigurationMap.
     */
    @Test
    public void testConstrainedHighSpeedCapability() throws Exception {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            int[] capabilities = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            boolean supportHighSpeed = arrayContains(capabilities, CONSTRAINED_HIGH_SPEED);
            if (supportHighSpeed) {
                StreamConfigurationMap config =
                        CameraTestUtils.getValueNotNull(
                                c, CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                List<Size> highSpeedSizes = Arrays.asList(config.getHighSpeedVideoSizes());
                assertTrue("High speed sizes shouldn't be empty", highSpeedSizes.size() > 0);
                Size[] allSizes = CameraTestUtils.getSupportedSizeForFormat(ImageFormat.PRIVATE,
                        mAllCameraIds[i], mCameraManager);
                assertTrue("Normal size for PRIVATE format shouldn't be null or empty",
                        allSizes != null && allSizes.length > 0);
                for (Size size: highSpeedSizes) {
                    // The sizes must be a subset of the normal sizes
                    assertTrue("High speed size " + size +
                            " must be part of normal sizes " + Arrays.toString(allSizes),
                            Arrays.asList(allSizes).contains(size));

                    // Sanitize the high speed FPS ranges for each size
                    List<Range<Integer>> ranges =
                            Arrays.asList(config.getHighSpeedVideoFpsRangesFor(size));
                    for (Range<Integer> range : ranges) {
                        assertTrue("The range " + range + " doesn't satisfy the"
                                + " min/max boundary requirements.",
                                range.getLower() >= HIGH_SPEED_FPS_LOWER_MIN &&
                                range.getUpper() >= HIGH_SPEED_FPS_UPPER_MIN);
                        assertTrue("The range " + range + " should be multiple of 30fps",
                                range.getLower() % 30 == 0 && range.getUpper() % 30 == 0);
                        // If the range is fixed high speed range, it should contain the
                        // [30, fps_max] in the high speed range list; if it's variable FPS range,
                        // the corresponding fixed FPS Range must be included in the range list.
                        if (range.getLower() == range.getUpper()) {
                            Range<Integer> variableRange = new Range<Integer>(30, range.getUpper());
                            assertTrue("The variable FPS range " + variableRange +
                                    " shoould be included in the high speed ranges for size " +
                                    size, ranges.contains(variableRange));
                        } else {
                            Range<Integer> fixedRange =
                                    new Range<Integer>(range.getUpper(), range.getUpper());
                            assertTrue("The fixed FPS range " + fixedRange +
                                    " shoould be included in the high speed ranges for size " +
                                    size, ranges.contains(fixedRange));
                        }
                    }
                }
                // If the device advertise some high speed profiles, the sizes and FPS ranges
                // should be advertise by the camera.
                for (int quality = CamcorderProfile.QUALITY_HIGH_SPEED_480P;
                        quality <= CamcorderProfile.QUALITY_HIGH_SPEED_2160P; quality++) {
                    int cameraId = Integer.valueOf(mAllCameraIds[i]);
                    if (CamcorderProfile.hasProfile(cameraId, quality)) {
                        CamcorderProfile profile = CamcorderProfile.get(cameraId, quality);
                        Size camcorderProfileSize =
                                new Size(profile.videoFrameWidth, profile.videoFrameHeight);
                        assertTrue("CamcorderPrfile size " + camcorderProfileSize +
                                " must be included in the high speed sizes " +
                                Arrays.toString(highSpeedSizes.toArray()),
                                highSpeedSizes.contains(camcorderProfileSize));
                        Range<Integer> camcorderFpsRange =
                                new Range<Integer>(profile.videoFrameRate, profile.videoFrameRate);
                        List<Range<Integer>> allRanges =
                                Arrays.asList(config.getHighSpeedVideoFpsRangesFor(
                                        camcorderProfileSize));
                        assertTrue("Camcorder fps range " + camcorderFpsRange +
                                " should be included by high speed fps ranges " +
                                Arrays.toString(allRanges.toArray()),
                                allRanges.contains(camcorderFpsRange));
                    }
                }
            }
        }
    }

    /**
     * Correctness check of optical black regions.
     */
    @Test
    public void testOpticalBlackRegions() {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            List<CaptureResult.Key<?>> resultKeys = c.getAvailableCaptureResultKeys();
            boolean hasDynamicBlackLevel =
                    resultKeys.contains(CaptureResult.SENSOR_DYNAMIC_BLACK_LEVEL);
            boolean hasDynamicWhiteLevel =
                    resultKeys.contains(CaptureResult.SENSOR_DYNAMIC_WHITE_LEVEL);
            boolean hasFixedBlackLevel =
                    c.getKeys().contains(CameraCharacteristics.SENSOR_BLACK_LEVEL_PATTERN);
            boolean hasFixedWhiteLevel =
                    c.getKeys().contains(CameraCharacteristics.SENSOR_INFO_WHITE_LEVEL);
            // The black and white levels should be either all supported or none of them is
            // supported.
            mCollector.expectTrue("Dynamic black and white level should be all or none of them"
                    + " be supported", hasDynamicWhiteLevel == hasDynamicBlackLevel);
            mCollector.expectTrue("Fixed black and white level should be all or none of them"
                    + " be supported", hasFixedBlackLevel == hasFixedWhiteLevel);
            mCollector.expectTrue("Fixed black level should be supported if dynamic black"
                    + " level is supported", !hasDynamicBlackLevel || hasFixedBlackLevel);

            if (c.getKeys().contains(CameraCharacteristics.SENSOR_OPTICAL_BLACK_REGIONS)) {
                // Regions shouldn't be null or empty.
                Rect[] regions = CameraTestUtils.getValueNotNull(c,
                        CameraCharacteristics.SENSOR_OPTICAL_BLACK_REGIONS);
                CameraTestUtils.assertArrayNotEmpty(regions, "Optical back region arrays must not"
                        + " be empty");

                // Dynamic black level should be supported if the optical black region is
                // advertised.
                mCollector.expectTrue("Dynamic black and white level keys should be advertised in "
                        + "available capture result key list", hasDynamicWhiteLevel);

                // Range check.
                for (Rect region : regions) {
                    mCollector.expectTrue("Camera " + mAllCameraIds[i] + ": optical black region" +
                            " shouldn't be empty!", !region.isEmpty());
                    mCollector.expectGreaterOrEqual("Optical black region left", 0/*expected*/,
                            region.left/*actual*/);
                    mCollector.expectGreaterOrEqual("Optical black region top", 0/*expected*/,
                            region.top/*actual*/);
                    mCollector.expectTrue("Optical black region left/right/width/height must be"
                            + " even number, otherwise, the bayer CFA pattern in this region will"
                            + " be messed up",
                            region.left % 2 == 0 && region.top % 2 == 0 &&
                            region.width() % 2 == 0 && region.height() % 2 == 0);
                    mCollector.expectGreaterOrEqual("Optical black region top", 0/*expected*/,
                            region.top/*actual*/);
                    Size size = CameraTestUtils.getValueNotNull(c,
                            CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
                    mCollector.expectLessOrEqual("Optical black region width",
                            size.getWidth()/*expected*/, region.width());
                    mCollector.expectLessOrEqual("Optical black region height",
                            size.getHeight()/*expected*/, region.height());
                    Rect activeArray = CameraTestUtils.getValueNotNull(c,
                            CameraCharacteristics.SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE);
                    mCollector.expectTrue("Optical black region" + region + " should be outside of"
                            + " active array " + activeArray,
                            !region.intersect(activeArray));
                    // Region need to be disjoint:
                    for (Rect region2 : regions) {
                        mCollector.expectTrue("Optical black region" + region + " should have no "
                                + "overlap with " + region2,
                                region == region2 || !region.intersect(region2));
                    }
                }
            } else {
                Log.i(TAG, "Camera " + mAllCameraIds[i] + " doesn't support optical black regions,"
                        + " skip the region test");
            }
        }
    }

    /**
     * Check Logical camera capability
     */
    @Test
    public void testLogicalCameraCharacteristics() throws Exception {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            int[] capabilities = CameraTestUtils.getValueNotNull(
                    c, CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            boolean supportLogicalCamera = arrayContains(capabilities,
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA);
            if (supportLogicalCamera) {
                Set<String> physicalCameraIds = c.getPhysicalCameraIds();
                assertNotNull("android.logicalCam.physicalCameraIds shouldn't be null",
                    physicalCameraIds);
                assertTrue("Logical camera must contain at least 2 physical camera ids",
                    physicalCameraIds.size() >= 2);

                mCollector.expectKeyValueInRange(c,
                        CameraCharacteristics.LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE,
                        CameraCharacteristics.LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE_APPROXIMATE,
                        CameraCharacteristics.LOGICAL_MULTI_CAMERA_SENSOR_SYNC_TYPE_CALIBRATED);

                Integer timestampSource = c.get(CameraCharacteristics.SENSOR_INFO_TIMESTAMP_SOURCE);
                for (String physicalCameraId : physicalCameraIds) {
                    assertNotNull("Physical camera id shouldn't be null", physicalCameraId);
                    assertTrue(
                            String.format("Physical camera id %s shouldn't be the same as logical"
                                    + " camera id %s", physicalCameraId, mAllCameraIds[i]),
                            physicalCameraId != mAllCameraIds[i]);

                    //validation for depth static metadata of physical cameras
                    CameraCharacteristics pc =
                            mCameraManager.getCameraCharacteristics(physicalCameraId);

                    float[] poseRotation = pc.get(CameraCharacteristics.LENS_POSE_ROTATION);
                    float[] poseTranslation = pc.get(CameraCharacteristics.LENS_POSE_TRANSLATION);
                    Integer poseReference = pc.get(CameraCharacteristics.LENS_POSE_REFERENCE);
                    float[] cameraIntrinsics = pc.get(
                            CameraCharacteristics.LENS_INTRINSIC_CALIBRATION);
                    float[] distortion = getLensDistortion(pc);
                    Integer facing = pc.get(CameraCharacteristics.LENS_FACING);
                    Rect precorrectionArray = pc.get(
                            CameraCharacteristics.SENSOR_INFO_PRE_CORRECTION_ACTIVE_ARRAY_SIZE);

                    verifyLensCalibration(poseRotation, poseTranslation, poseReference,
                            cameraIntrinsics, distortion, precorrectionArray, facing);

                    Integer timestampSourcePhysical =
                            pc.get(CameraCharacteristics.SENSOR_INFO_TIMESTAMP_SOURCE);
                    mCollector.expectEquals("Logical camera and physical cameras must have same " +
                            "timestamp source", timestampSource, timestampSourcePhysical);
                }
            }

            // Verify that if multiple focal lengths or apertures are supported, they are in
            // ascending order.
            Integer hwLevel = c.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            boolean isExternalCamera = (hwLevel ==
                    CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_EXTERNAL);
            if (!isExternalCamera) {
                float[] focalLengths = c.get(
                        CameraCharacteristics.LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
                for (int j = 0; j < focalLengths.length-1; j++) {
                    mCollector.expectTrue("Camera's available focal lengths must be ascending!",
                            focalLengths[j] < focalLengths[j+1]);
                }
                float[] apertures = c.get(CameraCharacteristics.LENS_INFO_AVAILABLE_APERTURES);
                for (int j = 0; j < apertures.length-1; j++) {
                    mCollector.expectTrue("Camera's available apertures must be ascending!",
                            apertures[j] < apertures[j+1]);
                }
            }
        }
    }

    /**
     * Check monochrome camera capability
     */
    @Test
    public void testMonochromeCharacteristics() {
        for (int i = 0; i < mAllCameraIds.length; i++) {
            Log.i(TAG, "testMonochromeCharacteristics: Testing camera ID " + mAllCameraIds[i]);

            CameraCharacteristics c = mCharacteristics.get(i);
            int[] capabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
            assertNotNull("android.request.availableCapabilities must never be null",
                    capabilities);
            boolean supportMonochrome = arrayContains(capabilities, MONOCHROME);

            if (!supportMonochrome) {
                continue;
            }

            List<Key<?>> allKeys = c.getKeys();
            List<CaptureRequest.Key<?>> requestKeys = c.getAvailableCaptureRequestKeys();
            List<CaptureResult.Key<?>> resultKeys = c.getAvailableCaptureResultKeys();

            assertTrue("Monochrome camera must have BACKWARD_COMPATIBLE capability",
                    arrayContains(capabilities, BC));
            int colorFilterArrangement = c.get(
                    CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT);
            assertTrue("Monochrome camera must have either MONO or NIR color filter pattern",
                    colorFilterArrangement ==
                            CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_MONO
                    || colorFilterArrangement ==
                            CameraCharacteristics.SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_NIR);

            assertFalse("Monochrome camera must not contain SENSOR_CALIBRATION_TRANSFORM1 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_CALIBRATION_TRANSFORM1));
            assertFalse("Monochrome camera must not contain SENSOR_COLOR_TRANSFORM1 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_COLOR_TRANSFORM1));
            assertFalse("Monochrome camera must not contain SENSOR_FORWARD_MATRIX1 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_FORWARD_MATRIX1));
            assertFalse("Monochrome camera must not contain SENSOR_REFERENCE_ILLUMINANT1 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT1));
            assertFalse("Monochrome camera must not contain SENSOR_CALIBRATION_TRANSFORM2 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_CALIBRATION_TRANSFORM2));
            assertFalse("Monochrome camera must not contain SENSOR_COLOR_TRANSFORM2 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_COLOR_TRANSFORM2));
            assertFalse("Monochrome camera must not contain SENSOR_FORWARD_MATRIX2 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_FORWARD_MATRIX2));
            assertFalse("Monochrome camera must not contain SENSOR_REFERENCE_ILLUMINANT2 key",
                    allKeys.contains(CameraCharacteristics.SENSOR_REFERENCE_ILLUMINANT2));

            assertFalse(
                    "Monochrome capture result must not contain SENSOR_NEUTRAL_COLOR_POINT key",
                    resultKeys.contains(CaptureResult.SENSOR_NEUTRAL_COLOR_POINT));
            assertFalse("Monochrome capture result must not contain SENSOR_GREEN_SPLIT key",
                    resultKeys.contains(CaptureResult.SENSOR_GREEN_SPLIT));

            // Check that color correction tags are not available for monochrome cameras
            assertTrue("Monochrome camera must not have MANUAL_POST_PROCESSING capability",
                    !arrayContains(capabilities, MANUAL_POSTPROC));
            assertTrue("Monochrome camera must not have COLOR_CORRECTION_MODE in request keys",
                    !requestKeys.contains(CaptureRequest.COLOR_CORRECTION_MODE));
            assertTrue("Monochrome camera must not have COLOR_CORRECTION_MODE in result keys",
                    !resultKeys.contains(CaptureResult.COLOR_CORRECTION_MODE));
            assertTrue("Monochrome camera must not have COLOR_CORRECTION_TRANSFORM in request keys",
                    !requestKeys.contains(CaptureRequest.COLOR_CORRECTION_TRANSFORM));
            assertTrue("Monochrome camera must not have COLOR_CORRECTION_TRANSFORM in result keys",
                    !resultKeys.contains(CaptureResult.COLOR_CORRECTION_TRANSFORM));
            assertTrue("Monochrome camera must not have COLOR_CORRECTION_GAINS in request keys",
                    !requestKeys.contains(CaptureRequest.COLOR_CORRECTION_GAINS));
            assertTrue("Monochrome camera must not have COLOR_CORRECTION_GAINS in result keys",
                    !resultKeys.contains(CaptureResult.COLOR_CORRECTION_GAINS));

            // Check that awbSupportedModes only contains AUTO
            int[] awbAvailableModes = c.get(CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES);
            assertTrue("availableAwbModes must not be null", awbAvailableModes != null);
            assertTrue("availableAwbModes must contain only AUTO", awbAvailableModes.length == 1 &&
                    awbAvailableModes[0] == CaptureRequest.CONTROL_AWB_MODE_AUTO);
        }
    }

    private boolean matchParametersToCharacteritics(Camera.Parameters params,
            Camera.CameraInfo info, CameraCharacteristics ch) {
        Integer facing = ch.get(CameraCharacteristics.LENS_FACING);
        switch (facing.intValue()) {
            case CameraMetadata.LENS_FACING_EXTERNAL:
            case CameraMetadata.LENS_FACING_FRONT:
                if (info.facing != Camera.CameraInfo.CAMERA_FACING_FRONT) {
                    return false;
                }
                break;
            case CameraMetadata.LENS_FACING_BACK:
                if (info.facing != Camera.CameraInfo.CAMERA_FACING_BACK) {
                    return false;
                }
                break;
            default:
                return false;
        }

        Integer orientation = ch.get(CameraCharacteristics.SENSOR_ORIENTATION);
        if (orientation.intValue() != info.orientation) {
            return false;
        }

        StaticMetadata staticMeta = new StaticMetadata(ch);
        boolean legacyHasFlash = params.getSupportedFlashModes() != null;
        if (staticMeta.hasFlash() != legacyHasFlash) {
            return false;
        }

        List<String> legacyFocusModes = params.getSupportedFocusModes();
        boolean legacyHasFocuser = !((legacyFocusModes.size() == 1) &&
                (legacyFocusModes.contains(Camera.Parameters.FOCUS_MODE_FIXED)));
        if (staticMeta.hasFocuser() != legacyHasFocuser) {
            return false;
        }

        if (staticMeta.isVideoStabilizationSupported() != params.isVideoStabilizationSupported()) {
            return false;
        }

        float legacyFocalLength = params.getFocalLength();
        float [] focalLengths = staticMeta.getAvailableFocalLengthsChecked();
        boolean found = false;
        for (float focalLength : focalLengths) {
            if (Math.abs(focalLength - legacyFocalLength) <= FOCAL_LENGTH_TOLERANCE) {
                found = true;
                break;
            }
        }

        return found;
    }

    /**
     * Check that all devices available through the legacy API are also
     * accessible via Camera2.
     */
    @CddTest(requirement="7.5.4/C-0-11")
    @Test
    public void testLegacyCameraDeviceParity() {
        if (mAdoptShellPerm) {
            // There is no current way to determine in camera1 api if a device is a system camera
            // Skip test, http://b/141496896
            return;
        }
        if (mOverrideCameraId != null) {
            // A single camera is being tested. Skip test.
            return;
        }
        int legacyDeviceCount = Camera.getNumberOfCameras();
        assertTrue("More legacy devices: " + legacyDeviceCount + " compared to Camera2 devices: " +
                mCharacteristics.size(), legacyDeviceCount <= mCharacteristics.size());

        ArrayList<CameraCharacteristics> chars = new ArrayList<> (mCharacteristics);
        for (int i = 0; i < legacyDeviceCount; i++) {
            Camera camera = null;
            Camera.Parameters legacyParams = null;
            Camera.CameraInfo legacyInfo = new Camera.CameraInfo();
            try {
                Camera.getCameraInfo(i, legacyInfo);
                camera = Camera.open(i);
                legacyParams = camera.getParameters();

                assertNotNull("Camera parameters for device: " + i + "  must not be null",
                        legacyParams);
            } finally {
                if (camera != null) {
                    camera.release();
                }
            }

            // Camera Ids between legacy devices and Camera2 device could be
            // different try to match devices by using other common traits.
            CameraCharacteristics found = null;
            for (CameraCharacteristics ch : chars) {
                if (matchParametersToCharacteritics(legacyParams, legacyInfo, ch)) {
                    found = ch;
                    break;
                }
            }
            assertNotNull("No matching Camera2 device for legacy device id: " + i, found);

            chars.remove(found);
        }
    }

    /**
     * Check camera orientation against device orientation
     */
    @CddTest(requirement="7.5.5/C-1-1")
    @Test
    public void testCameraOrientationAlignedWithDevice() {
        WindowManager windowManager =
                (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        DisplayMetrics metrics = new DisplayMetrics();
        display.getMetrics(metrics);

        // For square screen, test is guaranteed to pass
        if (metrics.widthPixels == metrics.heightPixels) {
            return;
        }

        // Handle display rotation
        int displayRotation = display.getRotation();
        if (displayRotation == Surface.ROTATION_90 || displayRotation == Surface.ROTATION_270) {
            int tmp = metrics.widthPixels;
            metrics.widthPixels = metrics.heightPixels;
            metrics.heightPixels = tmp;
        }
        boolean isDevicePortrait = metrics.widthPixels < metrics.heightPixels;

        for (int i = 0; i < mAllCameraIds.length; i++) {
            CameraCharacteristics c = mCharacteristics.get(i);
            // Camera size
            Size pixelArraySize = c.get(CameraCharacteristics.SENSOR_INFO_PIXEL_ARRAY_SIZE);
            // Camera orientation
            int sensorOrientation = c.get(CameraCharacteristics.SENSOR_ORIENTATION);

            // For square sensor, test is guaranteed to pass
            if (pixelArraySize.getWidth() == pixelArraySize.getHeight()) {
                continue;
            }

            // Camera size adjusted for device native orientation.
            Size adjustedSensorSize;
            if (sensorOrientation == 90 || sensorOrientation == 270) {
                adjustedSensorSize = new Size(
                        pixelArraySize.getHeight(), pixelArraySize.getWidth());
            } else {
                adjustedSensorSize = pixelArraySize;
            }

            boolean isCameraPortrait =
                    adjustedSensorSize.getWidth() < adjustedSensorSize.getHeight();
            assertFalse("Camera " + mAllCameraIds[i] + "'s long dimension must "
                    + "align with screen's long dimension", isDevicePortrait^isCameraPortrait);
        }
    }

    /**
     * Get lens distortion coefficients, as a list of 6 floats; returns null if no valid
     * distortion field is available
     */
    private float[] getLensDistortion(CameraCharacteristics c) {
        float[] distortion = null;
        float[] newDistortion = c.get(CameraCharacteristics.LENS_DISTORTION);
        if (Build.VERSION.FIRST_SDK_INT > Build.VERSION_CODES.O_MR1 || newDistortion != null) {
            // New devices need to use fixed radial distortion definition; old devices can
            // opt-in to it
            if (newDistortion != null && newDistortion.length == 5) {
                distortion = new float[6];
                distortion[0] = 1.0f;
                for (int i = 1; i < 6; i++) {
                    distortion[i] = newDistortion[i-1];
                }
            }
        } else {
            // Select old field only if on older first SDK and new definition not available
            distortion = c.get(CameraCharacteristics.LENS_RADIAL_DISTORTION);
        }
        return distortion;
    }

    /**
     * Create an invalid size that's close to one of the good sizes in the list, but not one of them
     */
    private Size findInvalidSize(Size[] goodSizes) {
        return findInvalidSize(Arrays.asList(goodSizes));
    }

    /**
     * Create an invalid size that's close to one of the good sizes in the list, but not one of them
     */
    private Size findInvalidSize(List<Size> goodSizes) {
        Size invalidSize = new Size(goodSizes.get(0).getWidth() + 1, goodSizes.get(0).getHeight());
        while(goodSizes.contains(invalidSize)) {
            invalidSize = new Size(invalidSize.getWidth() + 1, invalidSize.getHeight());
        }
        return invalidSize;
    }

    /**
     * Check key is present in characteristics if the hardware level is at least {@code hwLevel};
     * check that the key is present if the actual capabilities are one of {@code capabilities}.
     *
     * @return value of the {@code key} from {@code c}
     */
    private <T> T expectKeyAvailable(CameraCharacteristics c, CameraCharacteristics.Key<T> key,
            int hwLevel, int... capabilities) {

        Integer actualHwLevel = c.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
        assertNotNull("android.info.supportedHardwareLevel must never be null", actualHwLevel);

        int[] actualCapabilities = c.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
        assertNotNull("android.request.availableCapabilities must never be null",
                actualCapabilities);

        List<Key<?>> allKeys = c.getKeys();

        T value = c.get(key);

        // For LIMITED-level targeted keys, rely on capability check, not level
        if ((compareHardwareLevel(actualHwLevel, hwLevel) >= 0) && (hwLevel != LIMITED)) {
            mCollector.expectTrue(
                    String.format("Key (%s) must be in characteristics for this hardware level " +
                            "(required minimal HW level %s, actual HW level %s)",
                            key.getName(), toStringHardwareLevel(hwLevel),
                            toStringHardwareLevel(actualHwLevel)),
                    value != null);
            mCollector.expectTrue(
                    String.format("Key (%s) must be in characteristics list of keys for this " +
                            "hardware level (required minimal HW level %s, actual HW level %s)",
                            key.getName(), toStringHardwareLevel(hwLevel),
                            toStringHardwareLevel(actualHwLevel)),
                    allKeys.contains(key));
        } else if (arrayContainsAnyOf(actualCapabilities, capabilities)) {
            if (!(hwLevel == LIMITED && compareHardwareLevel(actualHwLevel, hwLevel) < 0)) {
                // Don't enforce LIMITED-starting keys on LEGACY level, even if cap is defined
                mCollector.expectTrue(
                    String.format("Key (%s) must be in characteristics for these capabilities " +
                            "(required capabilities %s, actual capabilities %s)",
                            key.getName(), Arrays.toString(capabilities),
                            Arrays.toString(actualCapabilities)),
                    value != null);
                mCollector.expectTrue(
                    String.format("Key (%s) must be in characteristics list of keys for " +
                            "these capabilities (required capabilities %s, actual capabilities %s)",
                            key.getName(), Arrays.toString(capabilities),
                            Arrays.toString(actualCapabilities)),
                    allKeys.contains(key));
            }
        } else {
            if (actualHwLevel == LEGACY && hwLevel != OPT) {
                if (value != null || allKeys.contains(key)) {
                    Log.w(TAG, String.format(
                            "Key (%s) is not required for LEGACY devices but still appears",
                            key.getName()));
                }
            }
            // OK: Key may or may not be present.
        }
        return value;
    }

    private static boolean arrayContains(int[] arr, int needle) {
        if (arr == null) {
            return false;
        }

        for (int elem : arr) {
            if (elem == needle) {
                return true;
            }
        }

        return false;
    }

    private static <T> boolean arrayContains(T[] arr, T needle) {
        if (arr == null) {
            return false;
        }

        for (T elem : arr) {
            if (elem.equals(needle)) {
                return true;
            }
        }

        return false;
    }

    private static boolean arrayContainsAnyOf(int[] arr, int[] needles) {
        for (int needle : needles) {
            if (arrayContains(arr, needle)) {
                return true;
            }
        }
        return false;
    }

    /**
     * The key name has a prefix of either "android." or a valid TLD; other prefixes are not valid.
     */
    private static void assertKeyPrefixValid(String keyName) {
        assertStartsWithAndroidOrTLD(
                "All metadata keys must start with 'android.' (built-in keys) " +
                "or valid TLD (vendor-extended keys)", keyName);
    }

    private static void assertTrueForKey(String msg, CameraCharacteristics.Key<?> key,
            boolean actual) {
        assertTrue(msg + " (key = '" + key.getName() + "')", actual);
    }

    private static <T> void assertOneOf(String msg, T[] expected, T actual) {
        for (int i = 0; i < expected.length; ++i) {
            if (Objects.equals(expected[i], actual)) {
                return;
            }
        }

        fail(String.format("%s: (expected one of %s, actual %s)",
                msg, Arrays.toString(expected), actual));
    }

    private static <T> void assertStartsWithAndroidOrTLD(String msg, String keyName) {
        String delimiter = ".";
        if (keyName.startsWith(PREFIX_ANDROID + delimiter)) {
            return;
        }
        Pattern tldPattern = Pattern.compile(Patterns.TOP_LEVEL_DOMAIN_STR);
        Matcher match = tldPattern.matcher(keyName);
        if (match.find(0) && (0 == match.start()) && (!match.hitEnd())) {
            if (keyName.regionMatches(match.end(), delimiter, 0, delimiter.length())) {
                return;
            }
        }

        fail(String.format("%s: (expected to start with %s or valid TLD, but value was %s)",
                msg, PREFIX_ANDROID + delimiter, keyName));
    }

    /** Return a positive int if left > right, 0 if left==right, negative int if left < right */
    private static int compareHardwareLevel(int left, int right) {
        return remapHardwareLevel(left) - remapHardwareLevel(right);
    }

    /** Remap HW levels worst<->best, 0 = LEGACY, 1 = LIMITED, 2 = FULL, ..., N = LEVEL_N */
    private static int remapHardwareLevel(int level) {
        switch (level) {
            case OPT:
                return Integer.MAX_VALUE;
            case LEGACY:
                return 0; // lowest
            case EXTERNAL:
                return 1; // second lowest
            case LIMITED:
                return 2;
            case FULL:
                return 3; // good
            case LEVEL_3:
                return 4;
            default:
                fail("Unknown HW level: " + level);
        }
        return -1;
    }

    private static String toStringHardwareLevel(int level) {
        switch (level) {
            case LEGACY:
                return "LEGACY";
            case LIMITED:
                return "LIMITED";
            case FULL:
                return "FULL";
            case EXTERNAL:
                return "EXTERNAL";
            default:
                if (level >= LEVEL_3) {
                    return String.format("LEVEL_%d", level);
                }
        }

        // unknown
        Log.w(TAG, "Unknown hardware level " + level);
        return Integer.toString(level);
    }
}
