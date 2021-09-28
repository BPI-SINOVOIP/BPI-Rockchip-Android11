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

package android.mediav2.cts;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.os.Build;
import android.util.Log;
import android.util.Pair;
import android.view.Surface;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

@RunWith(Parameterized.class)
public class CodecEncoderSurfaceTest {
    private static final String LOG_TAG = CodecEncoderSurfaceTest.class.getSimpleName();
    private static final String mInpPrefix = WorkDir.getMediaDirString();
    private static final boolean ENABLE_LOGS = false;

    private final String mMime;
    private final String mTestFile;
    private final int mBitrate;
    private final int mFrameRate;
    private final int mMaxBFrames;

    private MediaExtractor mExtractor;
    private MediaCodec mEncoder;
    private CodecAsyncHandler mAsyncHandleEncoder;
    private MediaCodec mDecoder;
    private CodecAsyncHandler mAsyncHandleDecoder;
    private boolean mIsCodecInAsyncMode;
    private boolean mSignalEOSWithLastFrame;
    private boolean mSawDecInputEOS;
    private boolean mSawDecOutputEOS;
    private boolean mSawEncOutputEOS;
    private int mDecInputCount;
    private int mDecOutputCount;
    private int mEncOutputCount;

    private boolean mSaveToMem;
    private OutputManager mOutputBuff;

    private Surface mSurface;

    private MediaMuxer mMuxer;
    private int mTrackID = -1;

    static {
        android.os.Bundle args = InstrumentationRegistry.getArguments();
        CodecTestBase.codecSelKeys = args.getString(CodecTestBase.CODEC_SEL_KEY);
        if (CodecTestBase.codecSelKeys == null)
            CodecTestBase.codecSelKeys = CodecTestBase.CODEC_SEL_VALUE;
    }

    public CodecEncoderSurfaceTest(String mime, String testFile, int bitrate, int frameRate) {
        mMime = mime;
        mTestFile = testFile;
        mBitrate = bitrate;
        mFrameRate = frameRate;
        mMaxBFrames = 0;
        mAsyncHandleDecoder = new CodecAsyncHandler();
        mAsyncHandleEncoder = new CodecAsyncHandler();
    }

    @Parameterized.Parameters(name = "{index}({0})")
    public static Collection<Object[]> input() {
        ArrayList<String> cddRequiredMimeList = new ArrayList<>();
        if (CodecTestBase.isHandheld() || CodecTestBase.isTv() || CodecTestBase.isAutomotive()) {
            // sec 2.2.2, 2.3.2, 2.5.2
            cddRequiredMimeList.add(MediaFormat.MIMETYPE_VIDEO_AVC);
            cddRequiredMimeList.add(MediaFormat.MIMETYPE_VIDEO_VP8);
        }
        final Object[][] exhaustiveArgsList = new Object[][]{
                // Video - CodecMime, test file, bit rate, frame rate
                {MediaFormat.MIMETYPE_VIDEO_H263, "bbb_176x144_128kbps_15fps_h263.3gp", 128000, 15},
                {MediaFormat.MIMETYPE_VIDEO_MPEG4, "bbb_128x96_64kbps_12fps_mpeg4.mp4", 64000, 12},
                {MediaFormat.MIMETYPE_VIDEO_AVC, "bbb_cif_768kbps_30fps_avc.mp4", 512000, 30},
                {MediaFormat.MIMETYPE_VIDEO_HEVC, "bbb_cif_768kbps_30fps_avc.mp4", 512000, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP8, "bbb_cif_768kbps_30fps_avc.mp4", 512000, 30},
                {MediaFormat.MIMETYPE_VIDEO_VP9, "bbb_cif_768kbps_30fps_avc.mp4", 512000, 30},
                {MediaFormat.MIMETYPE_VIDEO_AV1, "bbb_cif_768kbps_30fps_avc.mp4", 512000, 30},
        };
        ArrayList<String> mimes = new ArrayList<>();
        if (CodecTestBase.codecSelKeys.contains(CodecTestBase.CODEC_SEL_VALUE)) {
            MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            MediaCodecInfo[] codecInfos = codecList.getCodecInfos();
            for (MediaCodecInfo codecInfo : codecInfos) {
                if (!codecInfo.isEncoder()) continue;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && codecInfo.isAlias()) continue;
                String[] types = codecInfo.getSupportedTypes();
                for (String type : types) {
                    if (!mimes.contains(type) && type.startsWith("video/")) {
                        mimes.add(type);
                    }
                }
            }
            // TODO(b/154423708): add checks for video o/p port and display length >= 2.5"
            /* sec 5.2: device implementations include an embedded screen display with the diagonal
            length of at least 2.5inches or include a video output port or declare the support of a
            camera */
            if (CodecTestBase.hasCamera() && !mimes.contains(MediaFormat.MIMETYPE_VIDEO_AVC) &&
                    !mimes.contains(MediaFormat.MIMETYPE_VIDEO_VP8)) {
                fail("device must support at least one of VP8 or AVC video encoders");
            }
            for (String mime : cddRequiredMimeList) {
                if (!mimes.contains(mime)) {
                    fail("no codec found for mime " + mime + " as required by cdd");
                }
            }
        } else {
            for (Map.Entry<String, String> entry : CodecTestBase.codecSelKeyMimeMap.entrySet()) {
                String key = entry.getKey();
                String value = entry.getValue();
                if (CodecTestBase.codecSelKeys.contains(key) && !mimes.contains(value)) {
                    mimes.add(value);
                }
            }
        }
        final List<Object[]> argsList = new ArrayList<>();
        for (String mime : mimes) {
            boolean miss = true;
            for (Object[] arg : exhaustiveArgsList) {
                if (mime.equals(arg[0])) {
                    argsList.add(arg);
                    miss = false;
                }
            }
            if (miss) {
                if (cddRequiredMimeList.contains(mime)) {
                    fail("no test vectors for required mimetype " + mime);
                }
                Log.w(LOG_TAG, "no test vectors available for optional mime type " + mime);
            }
        }
        return argsList;
    }

    private boolean hasSeenError() {
        return mAsyncHandleDecoder.hasSeenError() || mAsyncHandleEncoder.hasSeenError();
    }

    private MediaFormat setUpSource(String srcFile) throws IOException {
        mExtractor = new MediaExtractor();
        mExtractor.setDataSource(mInpPrefix + srcFile);
        for (int trackID = 0; trackID < mExtractor.getTrackCount(); trackID++) {
            MediaFormat format = mExtractor.getTrackFormat(trackID);
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (mime.startsWith("video/")) {
                mExtractor.selectTrack(trackID);
                // COLOR_FormatYUV420Flexible by default should be supported by all components
                // This call shouldn't effect configure() call for any codec
                format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                        MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
                return format;
            }
        }
        mExtractor.release();
        fail("No video track found in file: " + srcFile);
        return null;
    }

    private void resetContext(boolean isAsync, boolean signalEOSWithLastFrame) {
        mAsyncHandleDecoder.resetContext();
        mAsyncHandleEncoder.resetContext();
        mIsCodecInAsyncMode = isAsync;
        mSignalEOSWithLastFrame = signalEOSWithLastFrame;
        mSawDecInputEOS = false;
        mSawDecOutputEOS = false;
        mSawEncOutputEOS = false;
        mDecInputCount = 0;
        mDecOutputCount = 0;
        mEncOutputCount = 0;
    }

    private void configureCodec(MediaFormat decFormat, MediaFormat encFormat, boolean isAsync,
            boolean signalEOSWithLastFrame) {
        resetContext(isAsync, signalEOSWithLastFrame);
        mAsyncHandleEncoder.setCallBack(mEncoder, isAsync);
        mEncoder.configure(encFormat, null, MediaCodec.CONFIGURE_FLAG_ENCODE, null);
        mSurface = mEncoder.createInputSurface();
        assertTrue("Surface is not valid", mSurface.isValid());
        mAsyncHandleDecoder.setCallBack(mDecoder, isAsync);
        mDecoder.configure(decFormat, mSurface, null, 0);
        if (ENABLE_LOGS) {
            Log.v(LOG_TAG, "codec configured");
        }
    }

    private void enqueueDecoderEOS(int bufferIndex) {
        if (!mSawDecInputEOS) {
            mDecoder.queueInputBuffer(bufferIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
            mSawDecInputEOS = true;
            if (ENABLE_LOGS) {
                Log.v(LOG_TAG, "Queued End of Stream");
            }
        }
    }

    private void enqueueDecoderInput(int bufferIndex) {
        if (mExtractor.getSampleSize() < 0) {
            enqueueDecoderEOS(bufferIndex);
        } else {
            ByteBuffer inputBuffer = mDecoder.getInputBuffer(bufferIndex);
            mExtractor.readSampleData(inputBuffer, 0);
            int size = (int) mExtractor.getSampleSize();
            long pts = mExtractor.getSampleTime();
            int extractorFlags = mExtractor.getSampleFlags();
            int codecFlags = 0;
            if ((extractorFlags & MediaExtractor.SAMPLE_FLAG_SYNC) != 0) {
                codecFlags |= MediaCodec.BUFFER_FLAG_KEY_FRAME;
            }
            if ((extractorFlags & MediaExtractor.SAMPLE_FLAG_PARTIAL_FRAME) != 0) {
                codecFlags |= MediaCodec.BUFFER_FLAG_PARTIAL_FRAME;
            }
            if (!mExtractor.advance() && mSignalEOSWithLastFrame) {
                codecFlags |= MediaCodec.BUFFER_FLAG_END_OF_STREAM;
                mSawDecInputEOS = true;
            }
            if (ENABLE_LOGS) {
                Log.v(LOG_TAG, "input: id: " + bufferIndex + " size: " + size + " pts: " + pts +
                        " flags: " + codecFlags);
            }
            mDecoder.queueInputBuffer(bufferIndex, 0, size, pts, codecFlags);
            if (size > 0 && (codecFlags & (MediaCodec.BUFFER_FLAG_CODEC_CONFIG |
                    MediaCodec.BUFFER_FLAG_PARTIAL_FRAME)) == 0) {
                mOutputBuff.saveInPTS(pts);
                mDecInputCount++;
            }
        }
    }

    private void dequeueDecoderOutput(int bufferIndex, MediaCodec.BufferInfo info) {
        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
            mSawDecOutputEOS = true;
        }
        if (ENABLE_LOGS) {
            Log.v(LOG_TAG, "output: id: " + bufferIndex + " flags: " + info.flags + " size: " +
                    info.size + " timestamp: " + info.presentationTimeUs);
        }
        if (info.size > 0 && (info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
            mDecOutputCount++;
        }
        mDecoder.releaseOutputBuffer(bufferIndex, mSurface != null);
    }

    private void dequeueEncoderOutput(int bufferIndex, MediaCodec.BufferInfo info) {
        if (ENABLE_LOGS) {
            Log.v(LOG_TAG, "encoder output: id: " + bufferIndex + " flags: " + info.flags +
                    " size: " + info.size + " timestamp: " + info.presentationTimeUs);
        }
        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
            mSawEncOutputEOS = true;
        }
        if (info.size > 0) {
            ByteBuffer buf = mEncoder.getOutputBuffer(bufferIndex);
            if (mSaveToMem) {
                mOutputBuff.saveToMemory(buf, info);
            }
            if (mMuxer != null) {
                if (mTrackID == -1) {
                    mTrackID = mMuxer.addTrack(mEncoder.getOutputFormat());
                    mMuxer.start();
                }
                mMuxer.writeSampleData(mTrackID, buf, info);
            }
            if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                mOutputBuff.saveOutPTS(info.presentationTimeUs);
                mEncOutputCount++;
            }
        }
        mEncoder.releaseOutputBuffer(bufferIndex, false);
    }

    private void tryEncoderOutput(long timeOutUs) throws InterruptedException {
        if (mIsCodecInAsyncMode) {
            if (!hasSeenError() && !mSawEncOutputEOS) {
                Pair<Integer, MediaCodec.BufferInfo> element = mAsyncHandleEncoder.getOutput();
                if (element != null) {
                    dequeueEncoderOutput(element.first, element.second);
                }
            }
        } else {
            MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();
            if (!mSawEncOutputEOS) {
                int outputBufferId = mEncoder.dequeueOutputBuffer(outInfo, timeOutUs);
                if (outputBufferId >= 0) {
                    dequeueEncoderOutput(outputBufferId, outInfo);
                }
            }
        }
    }

    private void waitForAllEncoderOutputs() throws InterruptedException {
        if (mIsCodecInAsyncMode) {
            while (!hasSeenError() && !mSawEncOutputEOS) {
                tryEncoderOutput(CodecTestBase.Q_DEQ_TIMEOUT_US);
            }
        } else {
            while (!mSawEncOutputEOS) {
                tryEncoderOutput(CodecTestBase.Q_DEQ_TIMEOUT_US);
            }
        }
    }

    private void queueEOS() throws InterruptedException {
        if (mIsCodecInAsyncMode) {
            while (!mAsyncHandleDecoder.hasSeenError() && !mSawDecInputEOS) {
                Pair<Integer, MediaCodec.BufferInfo> element = mAsyncHandleDecoder.getWork();
                if (element != null) {
                    int bufferID = element.first;
                    MediaCodec.BufferInfo info = element.second;
                    if (info != null) {
                        dequeueDecoderOutput(bufferID, info);
                    } else {
                        enqueueDecoderEOS(element.first);
                    }
                }
            }
        } else if (!mSawDecInputEOS) {
            enqueueDecoderEOS(mDecoder.dequeueInputBuffer(-1));
        }
        if (mIsCodecInAsyncMode) {
            while (!hasSeenError() && !mSawDecOutputEOS) {
                Pair<Integer, MediaCodec.BufferInfo> decOp = mAsyncHandleDecoder.getOutput();
                if (decOp != null) dequeueDecoderOutput(decOp.first, decOp.second);
                if (mSawDecOutputEOS) mEncoder.signalEndOfInputStream();
                // TODO: remove fixed constant and change it according to encoder latency
                if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                    tryEncoderOutput(-1);
                }
            }
        } else {
            MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();
            while (!mSawDecOutputEOS) {
                int outputBufferId =
                        mDecoder.dequeueOutputBuffer(outInfo, CodecTestBase.Q_DEQ_TIMEOUT_US);
                if (outputBufferId >= 0) {
                    dequeueDecoderOutput(outputBufferId, outInfo);
                }
                if (mSawDecOutputEOS) mEncoder.signalEndOfInputStream();
                // TODO: remove fixed constant and change it according to encoder latency
                if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                    tryEncoderOutput(-1);
                }
            }
        }
    }

    private void doWork(int frameLimit) throws InterruptedException {
        int frameCnt = 0;
        if (mIsCodecInAsyncMode) {
            // dequeue output after inputEOS is expected to be done in waitForAllOutputs()
            while (!hasSeenError() && !mSawDecInputEOS && frameCnt < frameLimit) {
                Pair<Integer, MediaCodec.BufferInfo> element = mAsyncHandleDecoder.getWork();
                if (element != null) {
                    int bufferID = element.first;
                    MediaCodec.BufferInfo info = element.second;
                    if (info != null) {
                        // <id, info> corresponds to output callback. Handle it accordingly
                        dequeueDecoderOutput(bufferID, info);
                    } else {
                        // <id, null> corresponds to input callback. Handle it accordingly
                        enqueueDecoderInput(bufferID);
                        frameCnt++;
                    }
                }
                // check decoder EOS
                if (mSawDecOutputEOS) mEncoder.signalEndOfInputStream();
                // encoder output
                // TODO: remove fixed constant and change it according to encoder latency
                if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                    tryEncoderOutput(-1);
                }
            }
        } else {
            MediaCodec.BufferInfo outInfo = new MediaCodec.BufferInfo();
            while (!mSawDecInputEOS && frameCnt < frameLimit) {
                // decoder input
                int inputBufferId = mDecoder.dequeueInputBuffer(CodecTestBase.Q_DEQ_TIMEOUT_US);
                if (inputBufferId != -1) {
                    enqueueDecoderInput(inputBufferId);
                    frameCnt++;
                }
                // decoder output
                int outputBufferId =
                        mDecoder.dequeueOutputBuffer(outInfo, CodecTestBase.Q_DEQ_TIMEOUT_US);
                if (outputBufferId >= 0) {
                    dequeueDecoderOutput(outputBufferId, outInfo);
                }
                // check decoder EOS
                if (mSawDecOutputEOS) mEncoder.signalEndOfInputStream();
                // encoder output
                // TODO: remove fixed constant and change it according to encoder latency
                if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                    tryEncoderOutput(-1);
                }
            }
        }
    }

    private MediaFormat setUpEncoderFormat(MediaFormat decoderFormat) {
        MediaFormat encoderFormat = new MediaFormat();
        encoderFormat.setString(MediaFormat.KEY_MIME, mMime);
        encoderFormat.setInteger(MediaFormat.KEY_WIDTH,
                decoderFormat.getInteger(MediaFormat.KEY_WIDTH));
        encoderFormat.setInteger(MediaFormat.KEY_HEIGHT,
                decoderFormat.getInteger(MediaFormat.KEY_HEIGHT));
        encoderFormat.setInteger(MediaFormat.KEY_FRAME_RATE, mFrameRate);
        encoderFormat.setInteger(MediaFormat.KEY_BIT_RATE, mBitrate);
        encoderFormat.setFloat(MediaFormat.KEY_I_FRAME_INTERVAL, 1.0f);
        encoderFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        encoderFormat.setInteger(MediaFormat.KEY_MAX_B_FRAMES, mMaxBFrames);
        return encoderFormat;
    }

    /**
     * Tests listed encoder components for sync and async mode in surface mode.The output has to
     * be consistent (not flaky) in all runs.
     */
    @LargeTest
    @Test(timeout = CodecTestBase.PER_TEST_TIMEOUT_LARGE_TEST_MS)
    public void testSimpleEncodeFromSurface() throws IOException, InterruptedException {
        MediaFormat decoderFormat = setUpSource(mTestFile);
        MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        String decoder = codecList.findDecoderForFormat(decoderFormat);
        if (decoder == null) {
            mExtractor.release();
            fail("no suitable decoder found for format: " + decoderFormat.toString());
        }
        mDecoder = MediaCodec.createByCodecName(decoder);
        MediaFormat encoderFormat = setUpEncoderFormat(decoderFormat);
        ArrayList<String> listOfEncoders = CodecTestBase.selectCodecs(mMime, null, null, true);
        assertFalse("no suitable codecs found for mime: " + mMime, listOfEncoders.isEmpty());
        boolean muxOutput = true;
        for (String encoder : listOfEncoders) {
            mEncoder = MediaCodec.createByCodecName(encoder);
            /* TODO(b/149027258) */
            mSaveToMem = false;
            OutputManager ref = new OutputManager();
            OutputManager test = new OutputManager();
            int loopCounter = 0;
            boolean[] boolStates = {true, false};
            for (boolean isAsync : boolStates) {
                mExtractor.seekTo(0, MediaExtractor.SEEK_TO_CLOSEST_SYNC);
                mOutputBuff = loopCounter == 0 ? ref : test;
                mOutputBuff.reset();
                if (muxOutput && loopCounter == 0) {
                    String tmpPath;
                    int muxerFormat;
                    if (mMime.equals(MediaFormat.MIMETYPE_VIDEO_VP8) ||
                            mMime.equals(MediaFormat.MIMETYPE_VIDEO_VP9)) {
                        muxerFormat = MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM;
                        tmpPath = File.createTempFile("tmp", ".webm").getAbsolutePath();
                    } else {
                        muxerFormat = MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4;
                        tmpPath = File.createTempFile("tmp", ".mp4").getAbsolutePath();
                    }
                    mMuxer = new MediaMuxer(tmpPath, muxerFormat);
                }
                configureCodec(decoderFormat, encoderFormat, isAsync, false);
                mEncoder.start();
                mDecoder.start();
                doWork(Integer.MAX_VALUE);
                queueEOS();
                waitForAllEncoderOutputs();
                if (muxOutput) {
                    if (mTrackID != -1) {
                        mMuxer.stop();
                        mTrackID = -1;
                    }
                    if (mMuxer != null) {
                        mMuxer.release();
                        mMuxer = null;
                    }
                }
                /* TODO(b/147348711) */
                if (false) mDecoder.stop();
                else mDecoder.reset();
                /* TODO(b/147348711) */
                if (false) mEncoder.stop();
                else mEncoder.reset();
                String log = String.format(
                        "format: %s \n codec: %s, file: %s, mode: %s:: ",
                        encoderFormat, encoder, mTestFile, (isAsync ? "async" : "sync"));
                assertTrue(log + " unexpected error", !hasSeenError());
                assertTrue(log + "no input sent", 0 != mDecInputCount);
                assertTrue(log + "no decoder output received", 0 != mDecOutputCount);
                assertTrue(log + "no encoder output received", 0 != mEncOutputCount);
                assertTrue(log + "decoder input count != output count, act/exp: " +
                        mDecOutputCount +
                        " / " + mDecInputCount, mDecInputCount == mDecOutputCount);
                /* TODO(b/153127506)
                 *  Currently disabling all encoder output checks. Added checks only for encoder
                 *  timeStamp is in increasing order or not.
                 *  Once issue is fixed remove increasing timestamp check and enable encoder checks.
                 */
                /*assertTrue(log + "encoder output count != decoder output count, act/exp: " +
                                mEncOutputCount + " / " + mDecOutputCount,
                        mEncOutputCount == mDecOutputCount);
                if (loopCounter != 0) {
                    assertTrue(log + "encoder output is flaky", ref.equals(test));
                } else {
                    assertTrue(log + " input pts list and output pts list are not identical",
                            ref.isOutPtsListIdenticalToInpPtsList((false)));
                }*/
                if (loopCounter != 0) {
                    assertTrue("test output pts is not strictly increasing",
                            test.isPtsStrictlyIncreasing(Long.MIN_VALUE));
                } else {
                    assertTrue("ref output pts is not strictly increasing",
                            ref.isPtsStrictlyIncreasing(Long.MIN_VALUE));
                }
                loopCounter++;
                mSurface.release();
                mSurface = null;
            }
            mEncoder.release();
        }
        mDecoder.release();
        mExtractor.release();
    }

    private native boolean nativeTestSimpleEncode(String encoder, String decoder, String mime,
            String testFile, String muxFile, int bitrate, int framerate);

    @LargeTest
    @Test(timeout = CodecTestBase.PER_TEST_TIMEOUT_LARGE_TEST_MS)
    public void testSimpleEncodeFromSurfaceNative() throws IOException {
        MediaFormat decoderFormat = setUpSource(mTestFile);
        MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        String decoder = codecList.findDecoderForFormat(decoderFormat);
        if (decoder == null) {
            mExtractor.release();
            fail("no suitable decoder found for format: " + decoderFormat.toString());
        }
        ArrayList<String> listOfEncoders = CodecTestBase.selectCodecs(mMime, null, null, true);
        assertFalse("no suitable codecs found for mime: " + mMime, listOfEncoders.isEmpty());
        for (String encoder : listOfEncoders) {
            String tmpPath = null;
            if (mMime.equals(MediaFormat.MIMETYPE_VIDEO_VP8) ||
                    mMime.equals(MediaFormat.MIMETYPE_VIDEO_VP9)) {
                tmpPath = File.createTempFile("tmp", ".webm").getAbsolutePath();
            } else {
                tmpPath = File.createTempFile("tmp", ".mp4").getAbsolutePath();
            }
            assertTrue(
                    nativeTestSimpleEncode(encoder, decoder, mMime, mInpPrefix + mTestFile, tmpPath,
                            mBitrate, mFrameRate));
        }
    }
}

