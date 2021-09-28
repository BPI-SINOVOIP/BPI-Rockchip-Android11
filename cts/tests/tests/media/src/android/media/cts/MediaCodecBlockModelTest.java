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

package android.media.cts;

import android.content.res.AssetFileDescriptor;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CodecException;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.cts.R;
import android.net.Uri;
import android.os.Build;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.RequiresDevice;
import android.test.AndroidTestCase;
import android.util.Log;
import android.view.Surface;

import androidx.test.filters.SmallTest;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.MediaUtils;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;;
import java.util.UUID;;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.BooleanSupplier;

/**
 * MediaCodec tests with CONFIGURE_FLAG_USE_BLOCK_MODEL.
 */
@NonMediaMainlineTest
public class MediaCodecBlockModelTest extends AndroidTestCase {
    private static final String TAG = "MediaCodecBlockModelTest";
    private static final boolean VERBOSE = false;           // lots of logging

                                                            // H.264 Advanced Video Coding
    private static final String MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC;

    private static final int APP_BUFFER_SIZE = 1024 * 1024;  // 1 MB

    // Input buffers from this input video are queued up to and including the video frame with
    // timestamp LAST_BUFFER_TIMESTAMP_US.
    private static final int INPUT_RESOURCE_ID =
            R.raw.video_480x360_mp4_h264_1350kbps_30fps_aac_stereo_192kbps_44100hz;
    private static final long LAST_BUFFER_TIMESTAMP_US = 166666;

    // The test should fail if the codec never produces output frames for the truncated input.
    // Time out processing, as we have no way to query whether the decoder will produce output.
    private static final int TIMEOUT_MS = 60000;  // 1 minute

    private boolean mIsAtLeastR = ApiLevelUtil.isAtLeast(Build.VERSION_CODES.R);
    /**
     * Tests whether decoding a short group-of-pictures succeeds. The test queues a few video frames
     * then signals end-of-stream. The test fails if the decoder doesn't output the queued frames.
     */
    @Presubmit
    @SmallTest
    @RequiresDevice
    public void testDecodeShortVideo() throws InterruptedException {
        if (!MediaUtils.check(mIsAtLeastR, "test needs Android 11")) return;
        runThread(() -> runDecodeShortVideo(
                INPUT_RESOURCE_ID,
                LAST_BUFFER_TIMESTAMP_US,
                true /* obtainBlockForEachBuffer */));
        runThread(() -> runDecodeShortVideo(
                INPUT_RESOURCE_ID,
                LAST_BUFFER_TIMESTAMP_US,
                false /* obtainBlockForEachBuffer */));
    }

    /**
     * Tests whether decoding a short encrypted group-of-pictures succeeds.
     * The test queues a few encrypted video frames
     * then signals end-of-stream. The test fails if the decoder doesn't output the queued frames.
     */
    public void testDecodeShortEncryptedVideo() throws InterruptedException {
        if (!MediaUtils.check(mIsAtLeastR, "test needs Android 11")) return;
        runThread(() -> runDecodeShortEncryptedVideo(
                true /* obtainBlockForEachBuffer */));
        runThread(() -> runDecodeShortEncryptedVideo(
                false /* obtainBlockForEachBuffer */));
    }

    /**
     * Tests whether decoding a short audio succeeds. The test queues a few audio frames
     * then signals end-of-stream. The test fails if the decoder doesn't output the queued frames.
     */
    @Presubmit
    @SmallTest
    @RequiresDevice
    public void testDecodeShortAudio() throws InterruptedException {
        if (!MediaUtils.check(mIsAtLeastR, "test needs Android 11")) return;
        runThread(() -> runDecodeShortAudio(
                INPUT_RESOURCE_ID,
                LAST_BUFFER_TIMESTAMP_US,
                true /* obtainBlockForEachBuffer */));
        runThread(() -> runDecodeShortAudio(
                INPUT_RESOURCE_ID,
                LAST_BUFFER_TIMESTAMP_US,
                false /* obtainBlockForEachBuffer */));
    }

    /**
     * Tests whether encoding a short audio succeeds. The test queues a few audio frames
     * then signals end-of-stream. The test fails if the encoder doesn't output the queued frames.
     */
    @Presubmit
    @SmallTest
    @RequiresDevice
    public void testEncodeShortAudio() throws InterruptedException {
        if (!MediaUtils.check(mIsAtLeastR, "test needs Android 11")) return;
        runThread(() -> runEncodeShortAudio());
    }

    /**
     * Tests whether encoding a short video succeeds. The test queues a few video frames
     * then signals end-of-stream. The test fails if the encoder doesn't output the queued frames.
     */
    @Presubmit
    @SmallTest
    @RequiresDevice
    public void testEncodeShortVideo() throws InterruptedException {
        if (!MediaUtils.check(mIsAtLeastR, "test needs Android 11")) return;
        runThread(() -> runEncodeShortVideo());
    }

    /**
     * Test whether format change happens as expected in block model mode.
     */
    @Presubmit
    @SmallTest
    @RequiresDevice
    public void testFormatChange() throws InterruptedException {
        if (!MediaUtils.check(mIsAtLeastR, "test needs Android 11")) return;
        List<FormatChangeEvent> events = new ArrayList<>();
        runThread(() -> runDecodeShortVideo(
                getMediaExtractorForMimeType(INPUT_RESOURCE_ID, "video/"),
                LAST_BUFFER_TIMESTAMP_US,
                true /* obtainBlockForEachBuffer */,
                MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 320, 240),
                events,
                null /* sessionId */));
        int width = 320;
        int height = 240;
        for (FormatChangeEvent event : events) {
            if (event.changedKeys.contains(MediaFormat.KEY_WIDTH)) {
                width = event.format.getInteger(MediaFormat.KEY_WIDTH);
            }
            if (event.changedKeys.contains(MediaFormat.KEY_HEIGHT)) {
                height = event.format.getInteger(MediaFormat.KEY_HEIGHT);
            }
        }
        assertEquals("Width should have been updated", 480, width);
        assertEquals("Height should have been updated", 360, height);
    }

    private void runThread(BooleanSupplier supplier) throws InterruptedException {
        final AtomicBoolean completed = new AtomicBoolean(false);
        Thread thread = new Thread(new Runnable() {
            public void run() {
                completed.set(supplier.getAsBoolean());
            }
        });
        final AtomicReference<Throwable> throwable = new AtomicReference<>();
        thread.setUncaughtExceptionHandler((Thread t, Throwable e) -> {
            throwable.set(e);
        });
        thread.start();
        thread.join(TIMEOUT_MS);
        Throwable t = throwable.get();
        if (t != null) {
            throw new AssertionError("There was an error while running the thread", t);
        }
        assertTrue("timed out decoding to end-of-stream", completed.get());
    }

    private static class LinearInputBlock {
        MediaCodec.LinearBlock block;
        ByteBuffer buffer;
        int offset;
    }

    private static interface InputSlotListener {
        public void onInputSlot(MediaCodec codec, int index, LinearInputBlock input) throws Exception;
    }

    private static class ExtractorInputSlotListener implements InputSlotListener {
        public static class Builder {
            public Builder setExtractor(MediaExtractor extractor) {
                mExtractor = extractor;
                return this;
            }

            public Builder setLastBufferTimestampUs(Long timestampUs) {
                mLastBufferTimestampUs = timestampUs;
                return this;
            }

            public Builder setObtainBlockForEachBuffer(boolean enabled) {
                mObtainBlockForEachBuffer = enabled;
                return this;
            }

            public Builder setTimestampQueue(List<Long> list) {
                mTimestampList = list;
                return this;
            }

            public Builder setContentEncrypted(boolean encrypted) {
                mContentEncrypted = encrypted;
                return this;
            }

            public ExtractorInputSlotListener build() {
                if (mExtractor == null) {
                    throw new IllegalStateException("Extractor must be set");
                }
                return new ExtractorInputSlotListener(
                        mExtractor, mLastBufferTimestampUs,
                        mObtainBlockForEachBuffer, mTimestampList,
                        mContentEncrypted);
            }

            private MediaExtractor mExtractor = null;
            private Long mLastBufferTimestampUs = null;
            private boolean mObtainBlockForEachBuffer = false;
            private List<Long> mTimestampList = null;
            private boolean mContentEncrypted = false;
        }

        private ExtractorInputSlotListener(
                MediaExtractor extractor,
                Long lastBufferTimestampUs,
                boolean obtainBlockForEachBuffer,
                List<Long> timestampList,
                boolean contentEncrypted) {
            mExtractor = extractor;
            mLastBufferTimestampUs = lastBufferTimestampUs;
            mObtainBlockForEachBuffer = obtainBlockForEachBuffer;
            mTimestampList = timestampList;
            mContentEncrypted = contentEncrypted;
        }

        @Override
        public void onInputSlot(MediaCodec codec, int index, LinearInputBlock input) throws Exception {
            // Try to feed more data into the codec.
            if (mExtractor.getSampleTrackIndex() == -1 || mSignaledEos) {
                return;
            }
            long size = mExtractor.getSampleSize();
            String[] codecNames = new String[]{ codec.getName() };
            if (mContentEncrypted) {
                codecNames[0] = codecNames[0] + ".secure";
            }
            if (mObtainBlockForEachBuffer) {
                input.block.recycle();
                input.block = MediaCodec.LinearBlock.obtain(Math.toIntExact(size), codecNames);
                assertTrue("Blocks obtained through LinearBlock.obtain must be mappable",
                        input.block.isMappable());
                input.buffer = input.block.map();
                input.offset = 0;
            }
            if (input.buffer.capacity() < size) {
                input.block.recycle();
                input.block = MediaCodec.LinearBlock.obtain(
                        Math.toIntExact(size * 2), codecNames);
                assertTrue("Blocks obtained through LinearBlock.obtain must be mappable",
                        input.block.isMappable());
                input.buffer = input.block.map();
                input.offset = 0;
            } else if (input.buffer.capacity() - input.offset < size) {
                long capacity = input.buffer.capacity();
                input.block.recycle();
                input.block = MediaCodec.LinearBlock.obtain(
                        Math.toIntExact(capacity), codecNames);
                assertTrue("Blocks obtained through LinearBlock.obtain must be mappable",
                        input.block.isMappable());
                input.buffer = input.block.map();
                input.offset = 0;
            }
            long timestampUs = mExtractor.getSampleTime();
            int written = mExtractor.readSampleData(input.buffer, input.offset);
            boolean encrypted =
                    (mExtractor.getSampleFlags() & MediaExtractor.SAMPLE_FLAG_ENCRYPTED) != 0;
            if (encrypted) {
                mExtractor.getSampleCryptoInfo(mCryptoInfo);
            }
            mExtractor.advance();
            mSignaledEos = mExtractor.getSampleTrackIndex() == -1
                    || (mLastBufferTimestampUs != null && timestampUs >= mLastBufferTimestampUs);
            MediaCodec.QueueRequest request = codec.getQueueRequest(index);
            if (encrypted) {
                request.setEncryptedLinearBlock(
                        input.block, input.offset, written, mCryptoInfo);
            } else {
                request.setLinearBlock(input.block, input.offset, written);
            }
            request.setPresentationTimeUs(timestampUs);
            request.setFlags(mSignaledEos ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
            if (mSetParams) {
                request.setIntegerParameter("vendor.int", 0);
                request.setLongParameter("vendor.long", 0);
                request.setFloatParameter("vendor.float", (float)0);
                request.setStringParameter("vendor.string", "str");
                request.setByteBufferParameter("vendor.buffer", ByteBuffer.allocate(1));
                mSetParams = false;
            }
            request.queue();
            input.offset += written;
            if (mTimestampList != null) {
                mTimestampList.add(timestampUs);
            }
        }

        private final MediaExtractor mExtractor;
        private final Long mLastBufferTimestampUs;
        private final boolean mObtainBlockForEachBuffer;
        private final List<Long> mTimestampList;
        private boolean mSignaledEos = false;
        private boolean mSetParams = true;
        private final MediaCodec.CryptoInfo mCryptoInfo = new MediaCodec.CryptoInfo();
        private final boolean mContentEncrypted;
    }

    private static interface OutputSlotListener {
        // Returns true if EOS is met
        public boolean onOutputSlot(MediaCodec codec, int index) throws Exception;
    }

    private static class DummyOutputSlotListener implements OutputSlotListener {
        public DummyOutputSlotListener(boolean graphic, List<Long> timestampList) {
            mGraphic = graphic;
            mTimestampList = timestampList;
        }

        @Override
        public boolean onOutputSlot(MediaCodec codec, int index) throws Exception {
            MediaCodec.OutputFrame frame = codec.getOutputFrame(index);
            boolean eos = (frame.getFlags() & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;

            if (mGraphic && frame.getHardwareBuffer() != null) {
                frame.getHardwareBuffer().close();
            }
            if (!mGraphic && frame.getLinearBlock() != null) {
                frame.getLinearBlock().recycle();
            }

            mTimestampList.remove(frame.getPresentationTimeUs());

            codec.releaseOutputBuffer(index, false);

            return eos;
        }

        private final boolean mGraphic;
        private final List<Long> mTimestampList;
    }

    private static class SurfaceOutputSlotListener implements OutputSlotListener {
        public SurfaceOutputSlotListener(
                OutputSurface surface,
                List<Long> timestampList,
                List<FormatChangeEvent> events) {
            mOutputSurface = surface;
            mTimestampList = timestampList;
            mEvents = (events != null) ? events : new ArrayList<>();
        }

        @Override
        public boolean onOutputSlot(MediaCodec codec, int index) throws Exception {
            MediaCodec.OutputFrame frame = codec.getOutputFrame(index);
            boolean eos = (frame.getFlags() & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;

            boolean render = false;
            if (frame.getHardwareBuffer() != null) {
                frame.getHardwareBuffer().close();
                render = true;
            }

            mTimestampList.remove(frame.getPresentationTimeUs());

            if (!frame.getChangedKeys().isEmpty()) {
                mEvents.add(new FormatChangeEvent(
                        frame.getPresentationTimeUs(), frame.getChangedKeys(), frame.getFormat()));
            }

            codec.releaseOutputBuffer(index, render);
            if (render) {
                mOutputSurface.awaitNewImage();
            }

            return eos;
        }

        private final OutputSurface mOutputSurface;
        private final List<Long> mTimestampList;
        private final List<FormatChangeEvent> mEvents;
    }

    private static class SlotEvent {
        SlotEvent(boolean input, int index) {
            this.input = input;
            this.index = index;
        }
        final boolean input;
        final int index;
    }

    private boolean runDecodeShortVideo(
            int inputResourceId,
            long lastBufferTimestampUs,
            boolean obtainBlockForEachBuffer) {
        return runDecodeShortVideo(
                getMediaExtractorForMimeType(inputResourceId, "video/"),
                lastBufferTimestampUs, obtainBlockForEachBuffer, null, null, null);
    }

    private static final UUID CLEARKEY_SCHEME_UUID =
            new UUID(0x1077efecc0b24d02L, 0xace33c1e52e2fb4bL);

    private static final byte[] CLEAR_KEY_CENC = convert(new int[] {
            0x3f, 0x0a, 0x33, 0xf3, 0x40, 0x98, 0xb9, 0xe2,
            0x2b, 0xc0, 0x78, 0xe0, 0xa1, 0xb5, 0xe8, 0x54 });

    private static final byte[] DRM_INIT_DATA = convert(new int[] {
            // BMFF box header (4 bytes size + 'pssh')
            0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68,
            // Full box header (version = 1 flags = 0)
            0x01, 0x00, 0x00, 0x00,
            // SystemID
            0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c,
            0x1e, 0x52, 0xe2, 0xfb, 0x4b,
            // Number of key ids
            0x00, 0x00, 0x00, 0x01,
            // Key id
            0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
            0x30, 0x30, 0x30, 0x30, 0x30,
            // size of data, must be zero
            0x00, 0x00, 0x00, 0x00 });

    private static final long ENCRYPTED_CONTENT_FIRST_BUFFER_TIMESTAMP_US = 12083333;
    private static final long ENCRYPTED_CONTENT_LAST_BUFFER_TIMESTAMP_US = 15041666;

    private static byte[] convert(int[] intArray) {
        byte[] byteArray = new byte[intArray.length];
        for (int i = 0; i < intArray.length; ++i) {
            byteArray[i] = (byte)intArray[i];
        }
        return byteArray;
    }

    private boolean runDecodeShortEncryptedVideo(boolean obtainBlockForEachBuffer) {
        MediaExtractor extractor = new MediaExtractor();

        try (final MediaDrm drm = new MediaDrm(CLEARKEY_SCHEME_UUID)) {
            Uri uri = Uri.parse(Utils.getMediaPath() + "/clearkey/llama_h264_main_720p_8000.mp4");
            extractor.setDataSource(uri.toString(), null);
            extractor.selectTrack(0);
            extractor.seekTo(12083333, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
            drm.setOnEventListener(
                    (MediaDrm mediaDrm, byte[] sessionId, int event, int extra, byte[] data) -> {
                        if (event == MediaDrm.EVENT_KEY_REQUIRED
                                || event == MediaDrm.EVENT_KEY_EXPIRED) {
                            MediaDrmClearkeyTest.retrieveKeys(
                                    mediaDrm, "cenc", sessionId, DRM_INIT_DATA,
                                    MediaDrm.KEY_TYPE_STREAMING,
                                    new byte[][] { CLEAR_KEY_CENC });
                        }
                    });
            byte[] sessionId = drm.openSession();
            MediaDrmClearkeyTest.retrieveKeys(
                    drm, "cenc", sessionId, DRM_INIT_DATA, MediaDrm.KEY_TYPE_STREAMING,
                    new byte[][] { CLEAR_KEY_CENC });
            boolean result = runDecodeShortVideo(
                    extractor, ENCRYPTED_CONTENT_LAST_BUFFER_TIMESTAMP_US,
                    obtainBlockForEachBuffer, null /* format */, null /* events */, sessionId);
            drm.closeSession(sessionId);
            return result;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static class FormatChangeEvent {
        FormatChangeEvent(long ts, Set<String> keys, MediaFormat fmt) {
            timestampUs = ts;
            changedKeys = new HashSet<>(keys);
            format = new MediaFormat(fmt);
        }

        long timestampUs;
        Set<String> changedKeys;
        MediaFormat format;

        @Override
        public String toString() {
            return Long.toString(timestampUs) + "us: changed keys=" + changedKeys
                + " format=" + format;
        }
    }

    private boolean runDecodeShortVideo(
            MediaExtractor mediaExtractor,
            Long lastBufferTimestampUs,
            boolean obtainBlockForEachBuffer,
            MediaFormat format,
            List<FormatChangeEvent> events,
            byte[] sessionId) {
        OutputSurface outputSurface = null;
        MediaCodec mediaCodec = null;
        MediaCrypto crypto = null;
        try {
            outputSurface = new OutputSurface(1, 1);
            MediaFormat mediaFormat = mediaExtractor.getTrackFormat(
                    mediaExtractor.getSampleTrackIndex());
            if (format != null) {
                // copy CSD
                for (int i = 0; i < 3; ++i) {
                    String key = "csd-" + i;
                    if (mediaFormat.containsKey(key)) {
                        format.setByteBuffer(key, mediaFormat.getByteBuffer(key));
                    }
                }
                mediaFormat = format;
            }
            // TODO: b/147748978
            String[] codecs = MediaUtils.getDecoderNames(true /* isGoog */, mediaFormat);
            if (codecs.length == 0) {
                Log.i(TAG, "No decoder found for format= " + mediaFormat);
                return true;
            }
            mediaCodec = MediaCodec.createByCodecName(codecs[0]);

            if (sessionId != null) {
                crypto = new MediaCrypto(CLEARKEY_SCHEME_UUID, new byte[0] /* initData */);
                crypto.setMediaDrmSession(sessionId);
            }
            List<Long> timestampList = Collections.synchronizedList(new ArrayList<>());
            boolean result = runComponentWithLinearInput(
                    mediaCodec,
                    crypto,
                    mediaFormat,
                    outputSurface.getSurface(),
                    false,  // encoder
                    new ExtractorInputSlotListener.Builder()
                            .setExtractor(mediaExtractor)
                            .setLastBufferTimestampUs(lastBufferTimestampUs)
                            .setObtainBlockForEachBuffer(obtainBlockForEachBuffer)
                            .setTimestampQueue(timestampList)
                            .setContentEncrypted(sessionId != null)
                            .build(),
                    new SurfaceOutputSlotListener(outputSurface, timestampList, events));
            if (result) {
                assertTrue("Timestamp should match between input / output: " + timestampList,
                        timestampList.isEmpty());
            }
            return result;
        } catch (IOException e) {
            throw new RuntimeException("error reading input resource", e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            if (mediaCodec != null) {
                mediaCodec.stop();
                mediaCodec.release();
            }
            if (mediaExtractor != null) {
                mediaExtractor.release();
            }
            if (outputSurface != null) {
                outputSurface.release();
            }
            if (crypto != null) {
                crypto.release();
            }
        }
    }

    private boolean runDecodeShortAudio(
            int inputResourceId,
            long lastBufferTimestampUs,
            boolean obtainBlockForEachBuffer) {
        MediaExtractor mediaExtractor = null;
        MediaCodec mediaCodec = null;
        try {
            mediaExtractor = getMediaExtractorForMimeType(inputResourceId, "audio/");
            MediaFormat mediaFormat =
                    mediaExtractor.getTrackFormat(mediaExtractor.getSampleTrackIndex());
            // TODO: b/147748978
            String[] codecs = MediaUtils.getDecoderNames(true /* isGoog */, mediaFormat);
            if (codecs.length == 0) {
                Log.i(TAG, "No decoder found for format= " + mediaFormat);
                return true;
            }
            mediaCodec = MediaCodec.createByCodecName(codecs[0]);

            List<Long> timestampList = Collections.synchronizedList(new ArrayList<>());
            boolean result = runComponentWithLinearInput(
                    mediaCodec,
                    null,  // crypto
                    mediaFormat,
                    null,  // surface
                    false,  // encoder
                    new ExtractorInputSlotListener.Builder()
                            .setExtractor(mediaExtractor)
                            .setLastBufferTimestampUs(lastBufferTimestampUs)
                            .setObtainBlockForEachBuffer(obtainBlockForEachBuffer)
                            .setTimestampQueue(timestampList)
                            .build(),
                    new DummyOutputSlotListener(false /* graphic */, timestampList));
            if (result) {
                assertTrue("Timestamp should match between input / output: " + timestampList,
                        timestampList.isEmpty());
            }
            return result;
        } catch (IOException e) {
            throw new RuntimeException("error reading input resource", e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            if (mediaCodec != null) {
                mediaCodec.stop();
                mediaCodec.release();
            }
            if (mediaExtractor != null) {
                mediaExtractor.release();
            }
        }
    }

    private boolean runEncodeShortAudio() {
        MediaExtractor mediaExtractor = null;
        MediaCodec mediaCodec = null;
        try {
            mediaExtractor = getMediaExtractorForMimeType(
                    R.raw.okgoogle123_good, MediaFormat.MIMETYPE_AUDIO_RAW);
            MediaFormat mediaFormat = new MediaFormat(
                    mediaExtractor.getTrackFormat(mediaExtractor.getSampleTrackIndex()));
            mediaFormat.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_AUDIO_AAC);
            mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, 128000);
            // TODO: b/147748978
            String[] codecs = MediaUtils.getEncoderNames(true /* isGoog */, mediaFormat);
            if (codecs.length == 0) {
                Log.i(TAG, "No encoder found for format= " + mediaFormat);
                return true;
            }
            mediaCodec = MediaCodec.createByCodecName(codecs[0]);

            List<Long> timestampList = Collections.synchronizedList(new ArrayList<>());
            boolean result = runComponentWithLinearInput(
                    mediaCodec,
                    null,  // crypto
                    mediaFormat,
                    null,  // surface
                    true,  // encoder
                    new ExtractorInputSlotListener.Builder()
                            .setExtractor(mediaExtractor)
                            .setLastBufferTimestampUs(LAST_BUFFER_TIMESTAMP_US)
                            .setTimestampQueue(timestampList)
                            .build(),
                    new DummyOutputSlotListener(false /* graphic */, timestampList));
            if (result) {
                assertTrue("Timestamp should match between input / output: " + timestampList,
                        timestampList.isEmpty());
            }
            return result;
        } catch (IOException e) {
            throw new RuntimeException("error reading input resource", e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            if (mediaCodec != null) {
                mediaCodec.stop();
                mediaCodec.release();
            }
            if (mediaExtractor != null) {
                mediaExtractor.release();
            }
        }
    }

    private boolean runEncodeShortVideo() {
        final int kWidth = 176;
        final int kHeight = 144;
        final int kFrameRate = 15;
        MediaCodec mediaCodec = null;
        ArrayList<HardwareBuffer> hardwareBuffers = new ArrayList<>();
        try {
            MediaFormat mediaFormat = MediaFormat.createVideoFormat(
                    MediaFormat.MIMETYPE_VIDEO_AVC, kWidth, kHeight);
            mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, kFrameRate);
            mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, 1000000);
            mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            mediaFormat.setInteger(
                    MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
            // TODO: b/147748978
            String[] codecs = MediaUtils.getEncoderNames(true /* isGoog */, mediaFormat);
            if (codecs.length == 0) {
                Log.i(TAG, "No encoder found for format= " + mediaFormat);
                return true;
            }
            mediaCodec = MediaCodec.createByCodecName(codecs[0]);

            long usage = HardwareBuffer.USAGE_CPU_READ_OFTEN;
            usage |= HardwareBuffer.USAGE_CPU_WRITE_OFTEN;
            if (mediaCodec.getCodecInfo().isHardwareAccelerated()) {
                usage |= HardwareBuffer.USAGE_VIDEO_ENCODE;
            }
            if (!HardwareBuffer.isSupported(
                        kWidth, kHeight, HardwareBuffer.YCBCR_420_888, 1 /* layer */, usage)) {
                Log.i(TAG, "HardwareBuffer doesn't support " + kWidth + "x" + kHeight
                        + "; YCBCR_420_888; usage(" + Long.toHexString(usage) + ")");
                return true;
            }

            List<Long> timestampList = Collections.synchronizedList(new ArrayList<>());

            final LinkedBlockingQueue<SlotEvent> queue = new LinkedBlockingQueue<>();
            mediaCodec.setCallback(new MediaCodec.Callback() {
                @Override
                public void onInputBufferAvailable(MediaCodec codec, int index) {
                    queue.offer(new SlotEvent(true, index));
                }

                @Override
                public void onOutputBufferAvailable(
                        MediaCodec codec, int index, MediaCodec.BufferInfo info) {
                    queue.offer(new SlotEvent(false, index));
                }

                @Override
                public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
                }

                @Override
                public void onError(MediaCodec codec, CodecException e) {
                }
            });

            int flags = MediaCodec.CONFIGURE_FLAG_USE_BLOCK_MODEL;
            flags |= MediaCodec.CONFIGURE_FLAG_ENCODE;

            mediaCodec.configure(mediaFormat, null, null, flags);
            mediaCodec.start();
            boolean eos = false;
            boolean signaledEos = false;
            int frameIndex = 0;
            while (!eos && !Thread.interrupted()) {
                SlotEvent event;
                try {
                    event = queue.take();
                } catch (InterruptedException e) {
                    return false;
                }

                if (event.input) {
                    if (signaledEos) {
                        continue;
                    }
                    while (hardwareBuffers.size() <= event.index) {
                        hardwareBuffers.add(null);
                    }
                    HardwareBuffer buffer = hardwareBuffers.get(event.index);
                    if (buffer == null) {
                        buffer = HardwareBuffer.create(
                                kWidth, kHeight, HardwareBuffer.YCBCR_420_888, 1, usage);
                        hardwareBuffers.set(event.index, buffer);
                    }
                    try (Image image = MediaCodec.mapHardwareBuffer(buffer)) {
                        assertNotNull("CPU readable/writable image must be mappable", image);
                        assertEquals(kWidth, image.getWidth());
                        assertEquals(kHeight, image.getHeight());
                        // For Y plane
                        int rowSampling = 1;
                        int colSampling = 1;
                        for (Image.Plane plane : image.getPlanes()) {
                            ByteBuffer planeBuffer = plane.getBuffer();
                            for (int row = 0; row < kHeight / rowSampling; ++row) {
                                int rowOffset = row * plane.getRowStride();
                                for (int col = 0; col < kWidth / rowSampling; ++col) {
                                    planeBuffer.put(
                                            rowOffset + col * plane.getPixelStride(),
                                            (byte)(frameIndex * 4));
                                }
                            }
                            // For Cb and Cr planes
                            rowSampling = 2;
                            colSampling = 2;
                        }
                        long timestampUs = 1000000l * frameIndex / kFrameRate;
                        ++frameIndex;
                        if (frameIndex >= 32) {
                            signaledEos = true;
                        }
                        timestampList.add(timestampUs);
                        mediaCodec.getQueueRequest(event.index)
                                .setHardwareBuffer(buffer)
                                .setPresentationTimeUs(timestampUs)
                                .setFlags(signaledEos ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0)
                                .queue();
                    }
                } else {
                    MediaCodec.OutputFrame frame = mediaCodec.getOutputFrame(event.index);
                    eos = (frame.getFlags() & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;

                    if (!eos) {
                        assertNotNull(frame.getLinearBlock());
                        frame.getLinearBlock().recycle();
                    }

                    timestampList.remove(frame.getPresentationTimeUs());

                    mediaCodec.releaseOutputBuffer(event.index, false);
                }
            }

            if (!timestampList.isEmpty()) {
                assertTrue("Timestamp should match between input / output: " + timestampList,
                        timestampList.isEmpty());
            }
            return eos;
        } catch (IOException e) {
            throw new RuntimeException("error reading input resource", e);
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            if (mediaCodec != null) {
                mediaCodec.stop();
                mediaCodec.release();
            }
            for (HardwareBuffer buffer : hardwareBuffers) {
                if (buffer != null) {
                    buffer.close();
                }
            }
        }
    }

    private boolean runComponentWithLinearInput(
            MediaCodec mediaCodec,
            MediaCrypto crypto,
            MediaFormat mediaFormat,
            Surface surface,
            boolean encoder,
            InputSlotListener inputListener,
            OutputSlotListener outputListener) throws Exception {
        final LinkedBlockingQueue<SlotEvent> queue = new LinkedBlockingQueue<>();
        mediaCodec.setCallback(new MediaCodec.Callback() {
            @Override
            public void onInputBufferAvailable(MediaCodec codec, int index) {
                queue.offer(new SlotEvent(true, index));
            }

            @Override
            public void onOutputBufferAvailable(
                    MediaCodec codec, int index, MediaCodec.BufferInfo info) {
                queue.offer(new SlotEvent(false, index));
            }

            @Override
            public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
            }

            @Override
            public void onError(MediaCodec codec, CodecException e) {
            }
        });
        String[] codecNames = new String[]{ mediaCodec.getName() };
        LinearInputBlock input = new LinearInputBlock();
        if (!mediaCodec.getCodecInfo().isVendor() && mediaCodec.getName().startsWith("c2.")) {
            assertTrue("Google default c2.* codecs are copy-free compatible with LinearBlocks",
                    MediaCodec.LinearBlock.isCodecCopyFreeCompatible(codecNames));
        }
        if (crypto != null) {
            codecNames[0] = codecNames[0] + ".secure";
        }
        input.block = MediaCodec.LinearBlock.obtain(
                APP_BUFFER_SIZE, codecNames);
        assertTrue("Blocks obtained through LinearBlock.obtain must be mappable",
                input.block.isMappable());
        input.buffer = input.block.map();
        input.offset = 0;

        int flags = MediaCodec.CONFIGURE_FLAG_USE_BLOCK_MODEL;
        if (encoder) {
            flags |= MediaCodec.CONFIGURE_FLAG_ENCODE;
        }
        mediaCodec.configure(mediaFormat, surface, crypto, flags);
        mediaCodec.start();
        boolean eos = false;
        boolean signaledEos = false;
        while (!eos && !Thread.interrupted()) {
            SlotEvent event;
            try {
                event = queue.take();
            } catch (InterruptedException e) {
                return false;
            }

            if (event.input) {
                inputListener.onInputSlot(mediaCodec, event.index, input);
            } else {
                eos = outputListener.onOutputSlot(mediaCodec, event.index);
            }
        }

        input.block.recycle();
        return eos;
    }

    private MediaExtractor getMediaExtractorForMimeType(int resourceId, String mimeTypePrefix) {
        MediaExtractor mediaExtractor = new MediaExtractor();
        try (AssetFileDescriptor afd = mContext.getResources().openRawResourceFd(resourceId)) {
            mediaExtractor.setDataSource(
                    afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        int trackIndex;
        for (trackIndex = 0; trackIndex < mediaExtractor.getTrackCount(); trackIndex++) {
            MediaFormat trackMediaFormat = mediaExtractor.getTrackFormat(trackIndex);
            if (trackMediaFormat.getString(MediaFormat.KEY_MIME).startsWith(mimeTypePrefix)) {
                mediaExtractor.selectTrack(trackIndex);
                break;
            }
        }
        if (trackIndex == mediaExtractor.getTrackCount()) {
            throw new IllegalStateException("couldn't get a video track");
        }

        return mediaExtractor;
    }

    private static boolean supportsCodec(String mimeType, boolean encoder) {
        MediaCodecList list = new MediaCodecList(MediaCodecList.ALL_CODECS);
        for (MediaCodecInfo info : list.getCodecInfos()) {
            if (encoder != info.isEncoder()) {
                continue;
            }

            for (String type : info.getSupportedTypes()) {
                if (type.equalsIgnoreCase(mimeType)) {
                    return true;
                }
            }
        }
        return false;
    }
}
