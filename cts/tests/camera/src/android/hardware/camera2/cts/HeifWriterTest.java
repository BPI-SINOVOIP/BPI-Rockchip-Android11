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

package android.camera.cts;

import static android.hardware.camera2.cts.CameraTestUtils.SESSION_CONFIGURE_TIMEOUT_MS;
import static android.hardware.camera2.cts.CameraTestUtils.CAPTURE_RESULT_TIMEOUT_MS;
import static android.hardware.camera2.cts.CameraTestUtils.SimpleCaptureCallback;
import static android.hardware.camera2.cts.CameraTestUtils.getValueNotNull;

import static androidx.heifwriter.HeifWriter.INPUT_MODE_SURFACE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.testcases.Camera2AndroidTestCase;
import android.hardware.camera2.params.OutputConfiguration;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import android.os.Environment;
import android.os.SystemClock;
import android.util.Log;
import android.util.Size;
import android.view.Surface;

import androidx.heifwriter.HeifWriter;

import com.android.compatibility.common.util.MediaUtils;
import com.android.ex.camera2.blocking.BlockingSessionCallback;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.runners.Parameterized;
import org.junit.runner.RunWith;
import org.junit.Test;

@RunWith(Parameterized.class)
public class HeifWriterTest extends Camera2AndroidTestCase {
    private static final String TAG = HeifWriterTest.class.getSimpleName();
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private String mFilePath;
    private static final String OUTPUT_FILENAME = "output.heic";

    @Override
    public void setUp() throws Exception {
        super.setUp();

        File filesDir = mContext.getPackageManager().isInstantApp()
                ? mContext.getFilesDir()
                : mContext.getExternalFilesDir(null);

        mFilePath = filesDir.getPath();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    public void testHeif() throws Exception {
        final int NUM_SINGLE_CAPTURE_TESTED = 3;
        final int NUM_HEIC_CAPTURE_TESTED = 2;
        final int SESSION_WARMUP_MS = 1000;
        final int HEIF_STOP_TIMEOUT = 3000 * NUM_SINGLE_CAPTURE_TESTED;

        if (!canEncodeHeic()) {
            MediaUtils.skipTest("heic encoding is not supported on this device");
            return;
        }

        boolean sessionFailure = false;
        Integer[] sessionStates = {BlockingSessionCallback.SESSION_READY,
                BlockingSessionCallback.SESSION_CONFIGURE_FAILED};
        for (String id : mCameraIdsUnderTest) {
            try {
                Log.v(TAG, "Testing HEIF capture for Camera " + id);
                openDevice(id);

                Size[] availableSizes = mStaticInfo.getAvailableSizesForFormatChecked(
                        ImageFormat.PRIVATE,
                        StaticMetadata.StreamDirection.Output);

                // for each resolution, test imageReader:
                for (Size sz : availableSizes) {
                    HeifWriter heifWriter = null;
                    OutputConfiguration outConfig = null;
                    Surface latestSurface = null;
                    CaptureRequest.Builder reqStill = null;
                    int width = sz.getWidth();
                    int height = sz.getHeight();
                    for (int cap = 0; cap < NUM_HEIC_CAPTURE_TESTED; cap++) {
                        if (VERBOSE) {
                            Log.v(TAG, "Testing size " + sz.toString() + " format PRIVATE"
                                    + " for camera " + mCamera.getId() + ". Iteration:" + cap);
                        }

                        try {
                            TestConfig.Builder builder = new TestConfig.Builder(/*useGrid*/false);
                            builder.setNumImages(NUM_SINGLE_CAPTURE_TESTED);
                            builder.setSize(sz);
                            String filename = "Cam" + id + "_" + width + "x" + height +
                                    "_" + cap + ".heic";
                            builder.setOutputPath(
                                    new File(mFilePath, filename).getAbsolutePath());
                            TestConfig config = builder.build();

                            try {
                                heifWriter = new HeifWriter.Builder(
                                        config.mOutputPath,
                                        width, height, INPUT_MODE_SURFACE)
                                    .setGridEnabled(config.mUseGrid)
                                    .setMaxImages(config.mMaxNumImages)
                                    .setQuality(config.mQuality)
                                    .setPrimaryIndex(config.mNumImages - 1)
                                    .setHandler(mHandler)
                                    .build();
                            } catch (IOException e) {
                                // Continue in case the size is not supported
                                sessionFailure = true;
                                Log.i(TAG, "Skip due to heifWriter creation failure: "
                                        + e.getMessage());
                                continue;
                            }

                            // First capture. Start capture session
                            latestSurface = heifWriter.getInputSurface();
                            outConfig = new OutputConfiguration(latestSurface);
                            List<OutputConfiguration> configs =
                                new ArrayList<OutputConfiguration>();
                            configs.add(outConfig);

                            SurfaceTexture preview = new SurfaceTexture(/*random int*/ 1);
                            Surface previewSurface = new Surface(preview);
                            preview.setDefaultBufferSize(640, 480);
                            configs.add(new OutputConfiguration(previewSurface));

                            CaptureRequest.Builder reqPreview = mCamera.createCaptureRequest(
                                    CameraDevice.TEMPLATE_PREVIEW);
                            reqPreview.addTarget(previewSurface);

                            reqStill = mCamera.createCaptureRequest(
                                    CameraDevice.TEMPLATE_STILL_CAPTURE);
                            reqStill.addTarget(previewSurface);
                            reqStill.addTarget(latestSurface);

                            // Start capture session and preview
                            createSessionByConfigs(configs);
                            int state = mCameraSessionListener.getStateWaiter().waitForAnyOfStates(
                                    Arrays.asList(sessionStates), SESSION_CONFIGURE_TIMEOUT_MS);
                            if (state == BlockingSessionCallback.SESSION_CONFIGURE_FAILED) {
                                // session configuration failure. Bail out due to known issue of
                                // HeifWriter INPUT_SURFACE mode support for camera. b/79699819
                                sessionFailure = true;
                                break;
                            }
                            startCapture(reqPreview.build(), /*repeating*/true, null, null);

                            SystemClock.sleep(SESSION_WARMUP_MS);

                            heifWriter.start();

                            // Start capture.
                            CaptureRequest request = reqStill.build();
                            SimpleCaptureCallback listener = new SimpleCaptureCallback();

                            int numImages = config.mNumImages;

                            for (int i = 0; i < numImages; i++) {
                                startCapture(request, /*repeating*/false, listener, mHandler);
                            }

                            // Validate capture result.
                            CaptureResult result = validateCaptureResult(
                                    ImageFormat.PRIVATE, sz, listener, numImages);

                            // TODO: convert capture results into EXIF and send to heifwriter

                            heifWriter.stop(HEIF_STOP_TIMEOUT);

                            verifyResult(config.mOutputPath, width, height,
                                    config.mRotation, config.mUseGrid,
                                    Math.min(numImages, config.mMaxNumImages));
                        } finally {
                            if (heifWriter != null) {
                                heifWriter.close();
                                heifWriter = null;
                            }
                            if (!sessionFailure) {
                                stopCapture(/*fast*/false);
                            }
                        }
                    }

                    if (sessionFailure) {
                        break;
                    }
                }
            } finally {
                closeDevice(id);
            }
        }
    }

    private static boolean canEncodeHeic() {
        return MediaUtils.hasEncoder(MediaFormat.MIMETYPE_VIDEO_HEVC)
            || MediaUtils.hasEncoder(MediaFormat.MIMETYPE_IMAGE_ANDROID_HEIC);
    }

    private static class TestConfig {
        final boolean mUseGrid;
        final int mMaxNumImages;
        final int mNumImages;
        final int mWidth;
        final int mHeight;
        final int mRotation;
        final int mQuality;
        final String mOutputPath;

        TestConfig(boolean useGrid, int maxNumImages, int numImages,
                   int width, int height, int rotation, int quality,
                   String outputPath) {
            mUseGrid = useGrid;
            mMaxNumImages = maxNumImages;
            mNumImages = numImages;
            mWidth = width;
            mHeight = height;
            mRotation = rotation;
            mQuality = quality;
            mOutputPath = outputPath;
        }

        static class Builder {
            final boolean mUseGrid;
            int mMaxNumImages;
            int mNumImages;
            int mWidth;
            int mHeight;
            int mRotation;
            final int mQuality;
            String mOutputPath;

            Builder(boolean useGrids) {
                mUseGrid = useGrids;
                mMaxNumImages = mNumImages = 4;
                mWidth = 1920;
                mHeight = 1080;
                mRotation = 0;
                mQuality = 100;
                mOutputPath = new File(Environment.getExternalStorageDirectory(),
                        OUTPUT_FILENAME).getAbsolutePath();
            }

            Builder setNumImages(int numImages) {
                mMaxNumImages = mNumImages = numImages;
                return this;
            }

            Builder setRotation(int rotation) {
                mRotation = rotation;
                return this;
            }

            Builder setSize(Size sz) {
                mWidth = sz.getWidth();
                mHeight = sz.getHeight();
                return this;
            }

            Builder setOutputPath(String path) {
                mOutputPath = path;
                return this;
            }

            private void cleanupStaleOutputs() {
                File outputFile = new File(mOutputPath);
                if (outputFile.exists()) {
                    outputFile.delete();
                }
            }

            TestConfig build() {
                cleanupStaleOutputs();
                return new TestConfig(mUseGrid, mMaxNumImages, mNumImages,
                        mWidth, mHeight, mRotation, mQuality, mOutputPath);
            }
        }

        @Override
        public String toString() {
            return "TestConfig"
                    + ": mUseGrid " + mUseGrid
                    + ", mMaxNumImages " + mMaxNumImages
                    + ", mNumImages " + mNumImages
                    + ", mWidth " + mWidth
                    + ", mHeight " + mHeight
                    + ", mRotation " + mRotation
                    + ", mQuality " + mQuality
                    + ", mOutputPath " + mOutputPath;
        }
    }

    private void verifyResult(
            String filename, int width, int height, int rotation, boolean useGrid, int numImages)
            throws Exception {
        MediaMetadataRetriever retriever = new MediaMetadataRetriever();
        retriever.setDataSource(filename);
        String hasImage = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_HAS_IMAGE);
        if (!"yes".equals(hasImage)) {
            throw new Exception("No images found in file " + filename);
        }
        assertEquals("Wrong image count", numImages,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_COUNT)));
        assertEquals("Wrong width", width,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_WIDTH)));
        assertEquals("Wrong height", height,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_HEIGHT)));
        assertEquals("Wrong rotation", rotation,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_ROTATION)));
        retriever.release();

        if (useGrid) {
            MediaExtractor extractor = new MediaExtractor();
            extractor.setDataSource(filename);
            MediaFormat format = extractor.getTrackFormat(0);
            int tileWidth = format.getInteger(MediaFormat.KEY_TILE_WIDTH);
            int tileHeight = format.getInteger(MediaFormat.KEY_TILE_HEIGHT);
            int gridRows = format.getInteger(MediaFormat.KEY_GRID_ROWS);
            int gridCols = format.getInteger(MediaFormat.KEY_GRID_COLUMNS);
            assertTrue("Wrong tile width or grid cols",
                    ((width + tileWidth - 1) / tileWidth) == gridCols);
            assertTrue("Wrong tile height or grid rows",
                    ((height + tileHeight - 1) / tileHeight) == gridRows);
            extractor.release();
        }
    }

    /**
     * Validate capture results.
     *
     * @param format The format of this capture.
     * @param size The capture size.
     * @param listener The capture listener to get capture result callbacks.
     * @return the last verified CaptureResult
     */
    private CaptureResult validateCaptureResult(
            int format, Size size, SimpleCaptureCallback listener, int numFrameVerified) {
        CaptureResult result = null;
        for (int i = 0; i < numFrameVerified; i++) {
            result = listener.getCaptureResult(CAPTURE_RESULT_TIMEOUT_MS);
            if (mStaticInfo.isCapabilitySupported(
                    CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS)) {
                Long exposureTime = getValueNotNull(result, CaptureResult.SENSOR_EXPOSURE_TIME);
                Integer sensitivity = getValueNotNull(result, CaptureResult.SENSOR_SENSITIVITY);
                mCollector.expectInRange(
                        String.format(
                                "Capture for format %d, size %s exposure time is invalid.",
                                format, size.toString()),
                        exposureTime,
                        mStaticInfo.getExposureMinimumOrDefault(),
                        mStaticInfo.getExposureMaximumOrDefault()
                );
                mCollector.expectInRange(
                        String.format("Capture for format %d, size %s sensitivity is invalid.",
                                format, size.toString()),
                        sensitivity,
                        mStaticInfo.getSensitivityMinimumOrDefault(),
                        mStaticInfo.getSensitivityMaximumOrDefault()
                );
            }
            // TODO: add more key validations.
        }
        return result;
    }
}
