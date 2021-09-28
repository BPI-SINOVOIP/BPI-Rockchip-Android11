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
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.os.Build;
import android.util.Log;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/**
 * Validate ColorAspects configuration for listed encoder components
 */
@RunWith(Parameterized.class)
public class EncoderColorAspectsTest extends CodecEncoderTestBase {
    private static final String LOG_TAG = EncoderColorAspectsTest.class.getSimpleName();
    private static final int UNSPECIFIED = 0;

    private int mRange;
    private int mStandard;
    private int mTransferCurve;
    private MediaFormat mConfigFormat;

    private MediaMuxer mMuxer;
    private int mTrackID = -1;

    private ArrayList<MediaCodec.BufferInfo> mInfoList = new ArrayList<>();

    private ArrayList<String> mCheckESList = new ArrayList<>();

    public EncoderColorAspectsTest(String mime, int width, int height, int range, int standard,
            int transferCurve) {
        super(mime);
        mRange = range;
        mStandard = standard;
        mTransferCurve = transferCurve;
        mConfigFormat = new MediaFormat();
        mConfigFormat.setString(MediaFormat.KEY_MIME, mMime);
        mConfigFormat.setInteger(MediaFormat.KEY_BIT_RATE, 64000);
        mWidth = width;
        mHeight = height;
        mConfigFormat.setInteger(MediaFormat.KEY_WIDTH, mWidth);
        mConfigFormat.setInteger(MediaFormat.KEY_HEIGHT, mHeight);
        mConfigFormat.setInteger(MediaFormat.KEY_FRAME_RATE, mFrameRate);
        mConfigFormat.setFloat(MediaFormat.KEY_I_FRAME_INTERVAL, 1.0f);
        mConfigFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible);
        if (mRange >= 0) mConfigFormat.setInteger(MediaFormat.KEY_COLOR_RANGE, mRange);
        else mRange = 0;
        if (mStandard >= 0) mConfigFormat.setInteger(MediaFormat.KEY_COLOR_STANDARD, mStandard);
        else mStandard = 0;
        if (mTransferCurve >= 0)
            mConfigFormat.setInteger(MediaFormat.KEY_COLOR_TRANSFER, mTransferCurve);
        else mTransferCurve = 0;
        mCheckESList.add(MediaFormat.MIMETYPE_VIDEO_AVC);
        mCheckESList.add(MediaFormat.MIMETYPE_VIDEO_HEVC);
    }

    void dequeueOutput(int bufferIndex, MediaCodec.BufferInfo info) {
        if (info.size > 0) {
            ByteBuffer buf = mCodec.getOutputBuffer(bufferIndex);
            if (mMuxer != null) {
                if (mTrackID == -1) {
                    mTrackID = mMuxer.addTrack(mCodec.getOutputFormat());
                    mMuxer.start();
                }
                mMuxer.writeSampleData(mTrackID, buf, info);
            }
            MediaCodec.BufferInfo copy = new MediaCodec.BufferInfo();
            copy.set(mOutputBuff.getOutStreamSize(), info.size, info.presentationTimeUs,
                    info.flags);
            mInfoList.add(copy);
        }
        super.dequeueOutput(bufferIndex, info);
    }

    @Parameterized.Parameters(name = "{index}({0}{3}{4}{5})")
    public static Collection<Object[]> input() {
        ArrayList<String> testMimeList = new ArrayList<>();
        testMimeList.add(MediaFormat.MIMETYPE_VIDEO_AVC);
        testMimeList.add(MediaFormat.MIMETYPE_VIDEO_HEVC);
        testMimeList.add(MediaFormat.MIMETYPE_VIDEO_VP8);
        testMimeList.add(MediaFormat.MIMETYPE_VIDEO_VP9);
        ArrayList<String> mimes = new ArrayList<>();
        if (CodecTestBase.codecSelKeys.contains(CodecTestBase.CODEC_SEL_VALUE)) {
            MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            MediaCodecInfo[] codecInfos = codecList.getCodecInfos();
            for (MediaCodecInfo codecInfo : codecInfos) {
                if (!codecInfo.isEncoder()) continue;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && codecInfo.isAlias()) continue;
                String[] types = codecInfo.getSupportedTypes();
                for (String type : types) {
                    if (testMimeList.contains(type) && !mimes.contains(type)) {
                        mimes.add(type);
                    }
                }
            }
        }
        int[] ranges =
                {-1, UNSPECIFIED, MediaFormat.COLOR_RANGE_FULL, MediaFormat.COLOR_RANGE_LIMITED};
        int[] standards =
                {-1, UNSPECIFIED, MediaFormat.COLOR_STANDARD_BT709,
                        MediaFormat.COLOR_STANDARD_BT601_PAL,
                        MediaFormat.COLOR_STANDARD_BT601_NTSC, MediaFormat.COLOR_STANDARD_BT2020};
        int[] transfers =
                {-1, UNSPECIFIED, MediaFormat.COLOR_TRANSFER_LINEAR, MediaFormat.COLOR_TRANSFER_SDR_VIDEO};
        // TODO: COLOR_TRANSFER_ST2084, COLOR_TRANSFER_HLG are for 10 bit and above. Should these
        //  be tested as well?
        List<Object[]> exhaustiveArgsList = new ArrayList<>();
        // Assumes all combinations are supported by the standard
        for (String mime : mimes) {
            for (int range : ranges) {
                for (int standard : standards) {
                    for (int transfer : transfers) {
                        exhaustiveArgsList
                                .add(new Object[]{mime, 176, 144, range, standard, transfer});
                    }
                }
            }
        }
        return exhaustiveArgsList;
    }

    @SmallTest
    @Test(timeout = PER_TEST_TIMEOUT_SMALL_TEST_MS)
    public void testColorAspects() throws IOException, InterruptedException {
        ArrayList<String> listOfEncoders = selectCodecs(mMime, null, null, true);
        assertFalse("no suitable codecs found for mime: " + mMime, listOfEncoders.isEmpty());
        setUpSource(mInputFile);
        mSaveToMem = true;
        mOutputBuff = new OutputManager();
        for (String encoder : listOfEncoders) {
            mCodec = MediaCodec.createByCodecName(encoder);
            mOutputBuff.reset();
            mInfoList.clear();
            /* TODO(b/157523045) */
            if (mRange <= UNSPECIFIED || mStandard <= UNSPECIFIED ||
                    mTransferCurve <= UNSPECIFIED) {
                Log.d(LOG_TAG, "test skipped due to b/157523045");
                mCodec.release();
                continue;
            }
            /* TODO(b/156571486) */
            if (encoder.equals("c2.android.hevc.encoder") ||
                    encoder.equals("OMX.google.h264.encoder") ||
                    encoder.equals("c2.android.avc.encoder")) {
                Log.d(LOG_TAG, "test skipped due to b/156571486");
                mCodec.release();
                continue;
            }
            String log = String.format("format: %s \n codec: %s:: ", mConfigFormat, encoder);
            File tmpFile;
            int muxerFormat;
            if (mMime.equals(MediaFormat.MIMETYPE_VIDEO_VP8) ||
                    mMime.equals(MediaFormat.MIMETYPE_VIDEO_VP9)) {
                muxerFormat = MediaMuxer.OutputFormat.MUXER_OUTPUT_WEBM;
                tmpFile = File.createTempFile("tmp", ".webm");
            } else {
                muxerFormat = MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4;
                tmpFile = File.createTempFile("tmp", ".mp4");
            }
            mMuxer = new MediaMuxer(tmpFile.getAbsolutePath(), muxerFormat);
            configureCodec(mConfigFormat, true, true, true);
            mCodec.start();
            doWork(4);
            queueEOS();
            waitForAllOutputs();
            if (mTrackID != -1) {
                mMuxer.stop();
                mTrackID = -1;
            }
            if (mMuxer != null) {
                mMuxer.release();
                mMuxer = null;
            }
            assertTrue(log + "unexpected error", !mAsyncHandle.hasSeenError());
            assertTrue(log + "no input sent", 0 != mInputCount);
            assertTrue(log + "output received", 0 != mOutputCount);
            // verify if the out fmt contains color aspects as expected
            MediaFormat fmt = mCodec.getOutputFormat();
            validateColorAspects(fmt, mRange, mStandard, mTransferCurve);
            mCodec.stop();
            mCodec.release();

            // verify if the muxed file contains color aspects as expected
            CodecDecoderTestBase cdtb = new CodecDecoderTestBase(mMime, null);
            String parent = tmpFile.getParent();
            if (parent != null) parent += File.separator;
            else parent = "";
            cdtb.validateColorAspects(null, parent, tmpFile.getName(), mRange, mStandard,
                    mTransferCurve);

            // if color metadata can also be signalled via elementary stream then verify if the
            // elementary stream contains color aspects as expected
            if (mCheckESList.contains(mMime)) {
                fmt.removeKey(MediaFormat.KEY_COLOR_RANGE);
                fmt.removeKey(MediaFormat.KEY_COLOR_STANDARD);
                fmt.removeKey(MediaFormat.KEY_COLOR_TRANSFER);
                cdtb.validateColorAspects(null, fmt, mOutputBuff.getBuffer(), mInfoList, mRange,
                        mStandard, mTransferCurve);
            }
        }
    }
}
