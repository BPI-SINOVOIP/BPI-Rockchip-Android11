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

package android.media.cts;

import static androidx.heifwriter.HeifWriter.INPUT_MODE_BITMAP;
import static androidx.heifwriter.HeifWriter.INPUT_MODE_BUFFER;
import static androidx.heifwriter.HeifWriter.INPUT_MODE_SURFACE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.RequiresDevice;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.test.AndroidTestCase;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.heifwriter.HeifWriter;
import androidx.test.filters.SmallTest;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.MediaUtils;

import java.io.Closeable;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.util.Arrays;

@Presubmit
@SmallTest
@RequiresDevice
@AppModeFull(reason = "Instant apps cannot access the SD card")
public class HeifWriterTest extends AndroidTestCase {
    private static final String TAG = HeifWriterTest.class.getSimpleName();
    private static final boolean DEBUG = false;
    private static final boolean DUMP_OUTPUT = false;
    private static final boolean DUMP_YUV_INPUT = false;
    private static final int GRID_WIDTH = 512;
    private static final int GRID_HEIGHT = 512;
    private static final boolean IS_BEFORE_R = ApiLevelUtil.isBefore(Build.VERSION_CODES.R);

    private static byte[][] TEST_YUV_COLORS = {
            {(byte) 255, (byte) 0, (byte) 0},
            {(byte) 255, (byte) 0, (byte) 255},
            {(byte) 255, (byte) 255, (byte) 255},
            {(byte) 255, (byte) 255, (byte) 0},
    };
    private static Color COLOR_BLOCK =
            Color.valueOf(1.0f, 1.0f, 1.0f);
    private static Color[] COLOR_BARS = {
            Color.valueOf(0.0f, 0.0f, 0.0f),
            Color.valueOf(0.0f, 0.0f, 0.64f),
            Color.valueOf(0.0f, 0.64f, 0.0f),
            Color.valueOf(0.0f, 0.64f, 0.64f),
            Color.valueOf(0.64f, 0.0f, 0.0f),
            Color.valueOf(0.64f, 0.0f, 0.64f),
            Color.valueOf(0.64f, 0.64f, 0.0f),
    };
    private static int BORDER_WIDTH = 16;

    private static final String HEIFWRITER_INPUT = "heifwriter_input.heic";
    private static final int[] IMAGE_RESOURCES = new int[] {
            R.raw.heifwriter_input
    };
    private static final String[] IMAGE_FILENAMES = new String[] {
            HEIFWRITER_INPUT
    };
    private static final String OUTPUT_FILENAME = "output.heic";

    private InputSurface mInputEglSurface;
    private Handler mHandler;
    private int mInputIndex;

    @Override
    public void setUp() throws Exception {
        for (int i = 0; i < IMAGE_RESOURCES.length; ++i) {
            String outputPath = new File(Environment.getExternalStorageDirectory(),
                    IMAGE_FILENAMES[i]).getAbsolutePath();

            InputStream inputStream = null;
            FileOutputStream outputStream = null;
            try {
                inputStream = getContext().getResources().openRawResource(IMAGE_RESOURCES[i]);
                outputStream = new FileOutputStream(outputPath);
                copy(inputStream, outputStream);
            } finally {
                closeQuietly(inputStream);
                closeQuietly(outputStream);
            }
        }

        HandlerThread handlerThread = new HandlerThread(
                "HeifEncoderThread", Process.THREAD_PRIORITY_FOREGROUND);
        handlerThread.start();
        mHandler = new Handler(handlerThread.getLooper());
    }

    @Override
    public void tearDown() throws Exception {
        for (int i = 0; i < IMAGE_RESOURCES.length; ++i) {
            String imageFilePath =
                    new File(Environment.getExternalStorageDirectory(), IMAGE_FILENAMES[i])
                            .getAbsolutePath();
            File imageFile = new File(imageFilePath);
            if (imageFile.exists()) {
                imageFile.delete();
            }
        }
    }

    public void testInputBuffer_NoGrid_NoHandler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BUFFER, false, false);
        doTestForVariousNumberImages(builder);
    }

    public void testInputBuffer_Grid_NoHandler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BUFFER, true, false);
        doTestForVariousNumberImages(builder);
    }

    public void testInputBuffer_NoGrid_Handler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BUFFER, false, true);
        doTestForVariousNumberImages(builder);
    }

    public void testInputBuffer_Grid_Handler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BUFFER, true, true);
        doTestForVariousNumberImages(builder);
    }

    public void testInputSurface_NoGrid_NoHandler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_SURFACE, false, false);
        doTestForVariousNumberImages(builder);
    }

    public void testInputSurface_Grid_NoHandler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_SURFACE, true, false);
        doTestForVariousNumberImages(builder);
    }

    public void testInputSurface_NoGrid_Handler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_SURFACE, false, true);
        doTestForVariousNumberImages(builder);
    }

    public void testInputSurface_Grid_Handler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_SURFACE, true, true);
        doTestForVariousNumberImages(builder);
    }

    public void testInputBitmap_NoGrid_NoHandler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BITMAP, false, false);
        for (int i = 0; i < IMAGE_RESOURCES.length; ++i) {
            String inputPath = new File(Environment.getExternalStorageDirectory(),
                    IMAGE_FILENAMES[i]).getAbsolutePath();
            doTestForVariousNumberImages(builder.setInputPath(inputPath));
        }
    }

    public void testInputBitmap_Grid_NoHandler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BITMAP, true, false);
        for (int i = 0; i < IMAGE_RESOURCES.length; ++i) {
            String inputPath = new File(Environment.getExternalStorageDirectory(),
                    IMAGE_FILENAMES[i]).getAbsolutePath();
            doTestForVariousNumberImages(builder.setInputPath(inputPath));
        }
    }

    public void testInputBitmap_NoGrid_Handler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BITMAP, false, true);
        for (int i = 0; i < IMAGE_RESOURCES.length; ++i) {
            String inputPath = new File(Environment.getExternalStorageDirectory(),
                    IMAGE_FILENAMES[i]).getAbsolutePath();
            doTestForVariousNumberImages(builder.setInputPath(inputPath));
        }
    }

    public void testInputBitmap_Grid_Handler() throws Throwable {
        TestConfig.Builder builder = new TestConfig.Builder(INPUT_MODE_BITMAP, true, true);
        for (int i = 0; i < IMAGE_RESOURCES.length; ++i) {
            String inputPath = new File(Environment.getExternalStorageDirectory(),
                    IMAGE_FILENAMES[i]).getAbsolutePath();
            doTestForVariousNumberImages(builder.setInputPath(inputPath));
        }
    }

    /**
     * This test is to ensure that if the device advertises support for {@link
     * MediaFormat#MIMETYPE_IMAGE_ANDROID_HEIC} (which encodes full-frame image
     * with tiling), it must also support {@link MediaFormat#MIMETYPE_VIDEO_HEVC}
     * at a specific tile size (512x512) with bitrate control mode {@link
     * MediaCodecInfo.EncoderCapabilities#BITRATE_MODE_CQ}, so that a fallback
     * could be implemented for image resolutions that's not supported by the
     * {@link MediaFormat#MIMETYPE_IMAGE_ANDROID_HEIC} encoder.
     */
    @CddTest(requirement="5.1.4/C-1-1")
    public void testHeicFallbackAvailable() throws Throwable {
        if (!MediaUtils.hasEncoder(MediaFormat.MIMETYPE_IMAGE_ANDROID_HEIC)) {
            MediaUtils.skipTest("HEIC full-frame image encoder is not supported on this device");
            return;
        }

        final MediaCodecList mcl = new MediaCodecList(MediaCodecList.REGULAR_CODECS);

        boolean fallbackFound = false;
        for (MediaCodecInfo info : mcl.getCodecInfos()) {
            if (!info.isEncoder() || !info.isHardwareAccelerated()) {
                continue;
            }
            MediaCodecInfo.CodecCapabilities caps = null;
            try {
                caps = info.getCapabilitiesForType(MediaFormat.MIMETYPE_VIDEO_HEVC);
            } catch (IllegalArgumentException e) { // mime is not supported
                continue;
            }
            if (caps.getVideoCapabilities().isSizeSupported(GRID_WIDTH, GRID_HEIGHT) &&
                    caps.getEncoderCapabilities().isBitrateModeSupported(
                            MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CQ)) {
                fallbackFound = true;
                Log.d(TAG, "found fallback on " + info.getName());
                // not breaking here so that we can log what's available by running this test
            }
        }
        assertTrue("HEIC full-frame image encoder without HEVC fallback", fallbackFound);
    }

    private static boolean canEncodeHeic() {
        return MediaUtils.hasEncoder(MediaFormat.MIMETYPE_VIDEO_HEVC)
            || MediaUtils.hasEncoder(MediaFormat.MIMETYPE_IMAGE_ANDROID_HEIC);
    }

    private void doTestForVariousNumberImages(TestConfig.Builder builder) throws Exception {
        if (!canEncodeHeic()) {
            MediaUtils.skipTest("heic encoding is not supported on this device");
            return;
        }

        builder.setNumImages(4);
        doTest(builder.setRotation(270).build());
        doTest(builder.setRotation(180).build());
        doTest(builder.setRotation(90).build());
        doTest(builder.setRotation(0).build());
        doTest(builder.setNumImages(1).build());
        doTest(builder.setNumImages(8).build());
    }

    private void closeQuietly(Closeable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (RuntimeException rethrown) {
                throw rethrown;
            } catch (Exception ignored) {
            }
        }
    }

    private int copy(InputStream in, OutputStream out) throws IOException {
        int total = 0;
        byte[] buffer = new byte[8192];
        int c;
        while ((c = in.read(buffer)) != -1) {
            total += c;
            out.write(buffer, 0, c);
        }
        return total;
    }

    private static class TestConfig {
        final int mInputMode;
        final boolean mUseGrid;
        final boolean mUseHandler;
        final int mMaxNumImages;
        final int mActualNumImages;
        final int mWidth;
        final int mHeight;
        final int mRotation;
        final int mQuality;
        final String mInputPath;
        final String mOutputPath;
        final Bitmap[] mBitmaps;

        TestConfig(int inputMode, boolean useGrid, boolean useHandler,
                   int maxNumImages, int actualNumImages, int width, int height,
                   int rotation, int quality,
                   String inputPath, String outputPath, Bitmap[] bitmaps) {
            mInputMode = inputMode;
            mUseGrid = useGrid;
            mUseHandler = useHandler;
            mMaxNumImages = maxNumImages;
            mActualNumImages = actualNumImages;
            mWidth = width;
            mHeight = height;
            mRotation = rotation;
            mQuality = quality;
            mInputPath = inputPath;
            mOutputPath = outputPath;
            mBitmaps = bitmaps;
        }

        static class Builder {
            final int mInputMode;
            final boolean mUseGrid;
            final boolean mUseHandler;
            int mMaxNumImages;
            int mNumImages;
            int mWidth;
            int mHeight;
            int mRotation;
            final int mQuality;
            String mInputPath;
            final String mOutputPath;
            Bitmap[] mBitmaps;
            boolean mNumImagesSetExplicitly;


            Builder(int inputMode, boolean useGrids, boolean useHandler) {
                mInputMode = inputMode;
                mUseGrid = useGrids;
                mUseHandler = useHandler;
                mMaxNumImages = mNumImages = 4;
                mWidth = 1920;
                mHeight = 1080;
                mRotation = 0;
                mQuality = 100;
                // use memfd by default
                if (DUMP_OUTPUT || IS_BEFORE_R) {
                    mOutputPath = new File(Environment.getExternalStorageDirectory(),
                            OUTPUT_FILENAME).getAbsolutePath();
                } else {
                    mOutputPath = null;
                }
            }

            Builder setInputPath(String inputPath) {
                mInputPath = (mInputMode == INPUT_MODE_BITMAP) ? inputPath : null;
                return this;
            }

            Builder setNumImages(int numImages) {
                mNumImagesSetExplicitly = true;
                mNumImages = numImages;
                return this;
            }

            Builder setRotation(int rotation) {
                mRotation = rotation;
                return this;
            }

            private void loadBitmapInputs() {
                if (mInputMode != INPUT_MODE_BITMAP) {
                    return;
                }
                MediaMetadataRetriever retriever = new MediaMetadataRetriever();
                retriever.setDataSource(mInputPath);
                String hasImage = retriever.extractMetadata(
                        MediaMetadataRetriever.METADATA_KEY_HAS_IMAGE);
                if (!"yes".equals(hasImage)) {
                    throw new IllegalArgumentException("no bitmap found!");
                }
                mMaxNumImages = Math.min(mMaxNumImages, Integer.parseInt(retriever.extractMetadata(
                        MediaMetadataRetriever.METADATA_KEY_IMAGE_COUNT)));
                if (!mNumImagesSetExplicitly) {
                    mNumImages = mMaxNumImages;
                }
                mBitmaps = new Bitmap[mMaxNumImages];
                for (int i = 0; i < mBitmaps.length; i++) {
                    mBitmaps[i] = retriever.getImageAtIndex(i);
                }
                mWidth = mBitmaps[0].getWidth();
                mHeight = mBitmaps[0].getHeight();
                retriever.release();
            }

            private void cleanupStaleOutputs() {
                if (mOutputPath != null) {
                    File outputFile = new File(mOutputPath);
                    if (outputFile.exists()) {
                        outputFile.delete();
                    }
                }
            }

            TestConfig build() {
                cleanupStaleOutputs();
                loadBitmapInputs();

                return new TestConfig(mInputMode, mUseGrid, mUseHandler, mMaxNumImages, mNumImages,
                        mWidth, mHeight, mRotation, mQuality, mInputPath, mOutputPath, mBitmaps);
            }
        }

        @Override
        public String toString() {
            return "TestConfig"
                    + ": mInputMode " + mInputMode
                    + ", mUseGrid " + mUseGrid
                    + ", mUseHandler " + mUseHandler
                    + ", mMaxNumImages " + mMaxNumImages
                    + ", mNumImages " + mActualNumImages
                    + ", mWidth " + mWidth
                    + ", mHeight " + mHeight
                    + ", mRotation " + mRotation
                    + ", mQuality " + mQuality
                    + ", mInputPath " + mInputPath
                    + ", mOutputPath " + mOutputPath;
        }
    }

    private void doTest(final TestConfig config) throws Exception {
        final int width = config.mWidth;
        final int height = config.mHeight;
        final int actualNumImages = config.mActualNumImages;

        mInputIndex = 0;
        HeifWriter heifWriter = null;
        FileInputStream inputYuvStream = null;
        FileOutputStream outputYuvStream = null;
        FileDescriptor outputFd = null;
        RandomAccessFile outputFile = null;
        try {
            if (DEBUG) Log.d(TAG, "started: " + config);
            if (config.mOutputPath != null) {
                outputFile = new RandomAccessFile(config.mOutputPath, "rws");
                outputFile.setLength(0);
                outputFd = outputFile.getFD();
            } else {
                outputFd = Os.memfd_create("temp", OsConstants.MFD_CLOEXEC);
            }

            heifWriter = new HeifWriter.Builder(
                    outputFd, width, height, config.mInputMode)
                    .setRotation(config.mRotation)
                    .setGridEnabled(config.mUseGrid)
                    .setMaxImages(config.mMaxNumImages)
                    .setQuality(config.mQuality)
                    .setPrimaryIndex(config.mMaxNumImages - 1)
                    .setHandler(config.mUseHandler ? mHandler : null)
                    .build();

            if (config.mInputMode == INPUT_MODE_SURFACE) {
                mInputEglSurface = new InputSurface(heifWriter.getInputSurface());
            }

            heifWriter.start();

            if (config.mInputMode == INPUT_MODE_BUFFER) {
                byte[] data = new byte[width * height * 3 / 2];

                if (config.mInputPath != null) {
                    inputYuvStream = new FileInputStream(config.mInputPath);
                }

                if (DUMP_YUV_INPUT) {
                    File outputYuvFile = new File("/sdcard/input.yuv");
                    outputYuvFile.createNewFile();
                    outputYuvStream = new FileOutputStream(outputYuvFile);
                }

                for (int i = 0; i < actualNumImages; i++) {
                    if (DEBUG) Log.d(TAG, "fillYuvBuffer: " + i);
                    fillYuvBuffer(i, data, width, height, inputYuvStream);
                    if (DUMP_YUV_INPUT) {
                        Log.d(TAG, "@@@ dumping input YUV");
                        outputYuvStream.write(data);
                    }
                    heifWriter.addYuvBuffer(ImageFormat.YUV_420_888, data);
                }
            } else if (config.mInputMode == INPUT_MODE_SURFACE) {
                // The input surface is a surface texture using single buffer mode, draws will be
                // blocked until onFrameAvailable is done with the buffer, which is dependent on
                // how fast MediaCodec processes them, which is further dependent on how fast the
                // MediaCodec callbacks are handled. We can't put draws on the same looper that
                // handles MediaCodec callback, it will cause deadlock.
                for (int i = 0; i < actualNumImages; i++) {
                    if (DEBUG) Log.d(TAG, "drawFrame: " + i);
                    drawFrame(width, height);
                }
                heifWriter.setInputEndOfStreamTimestamp(
                        1000 * computePresentationTime(actualNumImages - 1));
            } else if (config.mInputMode == INPUT_MODE_BITMAP) {
                Bitmap[] bitmaps = config.mBitmaps;
                for (int i = 0; i < Math.min(bitmaps.length, actualNumImages); i++) {
                    if (DEBUG) Log.d(TAG, "addBitmap: " + i);
                    heifWriter.addBitmap(bitmaps[i]);
                    bitmaps[i].recycle();
                }
            }

            heifWriter.stop(3000);
            // The test sets the primary index to the last image.
            // However, if we're testing early abort, the last image will not be
            // present and the muxer is supposed to set it to 0 by default.
            int expectedPrimary = config.mMaxNumImages - 1;
            int expectedImageCount = config.mMaxNumImages;
            if (actualNumImages < config.mMaxNumImages) {
                expectedPrimary = 0;
                expectedImageCount = actualNumImages;
            }
            verifyResult(outputFd, width, height, config.mRotation,
                    expectedImageCount, expectedPrimary, config.mUseGrid,
                    config.mInputMode == INPUT_MODE_SURFACE);
            if (DEBUG) Log.d(TAG, "finished: PASS");
        } finally {
            try {
                if (outputYuvStream != null) {
                    outputYuvStream.close();
                }
                if (inputYuvStream != null) {
                    inputYuvStream.close();
                }
                if (outputFile != null) {
                    outputFile.close();
                }
                if (outputFd != null) {
                    Os.close(outputFd);
                }
            } catch (IOException|ErrnoException e) {}

            if (heifWriter != null) {
                heifWriter.close();
                heifWriter = null;
            }
            if (mInputEglSurface != null) {
                // This also releases the surface from encoder.
                mInputEglSurface.release();
                mInputEglSurface = null;
            }
        }
    }

    private long computePresentationTime(int frameIndex) {
        return 132 + (long)frameIndex * 1000000;
    }

    private void fillYuvBuffer(int frameIndex, @NonNull byte[] data, int width, int height,
                               @Nullable FileInputStream inputStream) throws IOException {
        if (inputStream != null) {
            inputStream.read(data);
        } else {
            byte[] color = TEST_YUV_COLORS[frameIndex % TEST_YUV_COLORS.length];
            int sizeY = width * height;
            Arrays.fill(data, 0, sizeY, color[0]);
            Arrays.fill(data, sizeY, sizeY * 5 / 4, color[1]);
            Arrays.fill(data, sizeY * 5 / 4, sizeY * 3 / 2, color[2]);
        }
    }

    private void drawFrame(int width, int height) {
        mInputEglSurface.makeCurrent();
        generateSurfaceFrame(mInputIndex, width, height);
        mInputEglSurface.setPresentationTime(1000 * computePresentationTime(mInputIndex));
        mInputEglSurface.swapBuffers();
        mInputIndex++;
    }

    private static Rect getColorBarRect(int index, int width, int height) {
        int barWidth = (width - BORDER_WIDTH * 2) / COLOR_BARS.length;
        return new Rect(BORDER_WIDTH + barWidth * index, BORDER_WIDTH,
                BORDER_WIDTH + barWidth * (index + 1), height - BORDER_WIDTH);
    }

    private static Rect getColorBlockRect(int index, int width, int height) {
        int blockCenterX = (width / 5) * (index % 4 + 1);
        return new Rect(blockCenterX - width / 10, height / 6,
                        blockCenterX + width / 10, height / 3);
    }

    private void generateSurfaceFrame(int frameIndex, int width, int height) {
        GLES20.glViewport(0, 0, width, height);
        GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
        GLES20.glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        GLES20.glEnable(GLES20.GL_SCISSOR_TEST);

        for (int i = 0; i < COLOR_BARS.length; i++) {
            Rect r = getColorBarRect(i, width, height);

            GLES20.glScissor(r.left, r.top, r.width(), r.height());
            final Color color = COLOR_BARS[i];
            GLES20.glClearColor(color.red(), color.green(), color.blue(), 1.0f);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        }

        Rect r = getColorBlockRect(frameIndex, width, height);
        GLES20.glScissor(r.left, r.top, r.width(), r.height());
        GLES20.glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        r.inset(BORDER_WIDTH, BORDER_WIDTH);
        GLES20.glScissor(r.left, r.top, r.width(), r.height());
        GLES20.glClearColor(COLOR_BLOCK.red(), COLOR_BLOCK.green(), COLOR_BLOCK.blue(), 1.0f);
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    }

    /**
     * Determines if two color values are approximately equal.
     */
    private static boolean approxEquals(Color expected, Color actual) {
        final float MAX_DELTA = 0.025f;
        return (Math.abs(expected.red() - actual.red()) <= MAX_DELTA)
            && (Math.abs(expected.green() - actual.green()) <= MAX_DELTA)
            && (Math.abs(expected.blue() - actual.blue()) <= MAX_DELTA);
    }

    private void verifyResult(
            FileDescriptor fd, int width, int height, int rotation,
            int imageCount, int primary, boolean useGrid, boolean checkColor)
            throws Exception {
        MediaMetadataRetriever retriever = new MediaMetadataRetriever();
        retriever.setDataSource(fd);
        String hasImage = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_HAS_IMAGE);
        if (!"yes".equals(hasImage)) {
            throw new Exception("No images found in file descriptor");
        }
        assertEquals("Wrong width", width,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_WIDTH)));
        assertEquals("Wrong height", height,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_HEIGHT)));
        assertEquals("Wrong rotation", rotation,
                Integer.parseInt(retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_IMAGE_ROTATION)));
        assertEquals("Wrong image count", imageCount,
                Integer.parseInt(retriever.extractMetadata(
                        MediaMetadataRetriever.METADATA_KEY_IMAGE_COUNT)));
        assertEquals("Wrong primary index", primary,
                Integer.parseInt(retriever.extractMetadata(
                        MediaMetadataRetriever.METADATA_KEY_IMAGE_PRIMARY)));
        retriever.release();

        if (useGrid) {
            MediaExtractor extractor = new MediaExtractor();
            extractor.setDataSource(fd);
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

        if (checkColor) {
            Os.lseek(fd, 0, OsConstants.SEEK_SET);
            Bitmap bitmap = BitmapFactory.decodeFileDescriptor(fd);

            for (int i = 0; i < COLOR_BARS.length; i++) {
                Rect r = getColorBarRect(i, width, height);
                assertTrue("Color bar " + i + " doesn't match", approxEquals(COLOR_BARS[i],
                        Color.valueOf(bitmap.getPixel(r.centerX(), r.centerY()))));
            }

            Rect r = getColorBlockRect(primary, width, height);
            assertTrue("Color block doesn't match", approxEquals(COLOR_BLOCK,
                    Color.valueOf(bitmap.getPixel(r.centerX(), height - r.centerY()))));
        }
    }
}
