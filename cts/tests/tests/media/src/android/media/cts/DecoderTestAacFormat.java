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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.content.res.AssetFileDescriptor;
import android.content.res.Resources;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.cts.DecoderTest.AudioParameter;
import android.media.cts.R;
import android.os.Build;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.MediaUtils;

import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class DecoderTestAacFormat {
    private static final String TAG = "DecoderTestAacFormat";

    private static final boolean sIsAndroidRAndAbove =
            ApiLevelUtil.isAtLeast(Build.VERSION_CODES.R);

    private Resources mResources;

    @Before
    public void setUp() throws Exception {
        final Instrumentation inst = InstrumentationRegistry.getInstrumentation();
        assertNotNull(inst);
        mResources = inst.getContext().getResources();
    }

    /**
     * Verify downmixing to stereo at decoding of MPEG-4 HE-AAC 5.0 and 5.1 channel streams
     */
    @Test
    public void testHeAacM4aMultichannelDownmix() throws Exception {
        Log.i(TAG, "START testDecodeHeAacMcM4a");

        if (!MediaUtils.check(sIsAndroidRAndAbove, "M-chan downmix fixed in Android R"))
            return;

        // array of multichannel resources with their expected number of channels without downmixing
        int[][] samples = {
                //  {resourceId, numChannels},
                {R.raw.noise_5ch_48khz_aot5_dr_sbr_sig1_mp4, 5},
                {R.raw.noise_6ch_44khz_aot5_dr_sbr_sig2_mp4, 6},
        };
        for (int[] sample: samples) {
            for (String codecName : DecoderTest.codecsFor(sample[0] /* resource */, mResources)) {
                // verify correct number of channels is observed without downmixing
                AudioParameter chanParams = new AudioParameter();
                decodeUpdateFormat(codecName, sample[0] /*resource*/, chanParams, 0 /*no downmix*/);
                assertEquals("Number of channels differs for codec:" + codecName,
                        sample[1], chanParams.getNumChannels());

                // verify correct number of channels is observed when downmixing to stereo
                AudioParameter downmixParams = new AudioParameter();
                decodeUpdateFormat(codecName, sample[0] /* resource */, downmixParams,
                        2 /*stereo downmix*/);
                assertEquals("Number of channels differs for codec:" + codecName,
                        2, downmixParams.getNumChannels());

            }
        }
    }

    /**
     *
     * @param decoderName
     * @param testInput
     * @param audioParams
     * @param downmixChannelCount 0 if no downmix requested,
     *                           positive number for number of channels in requested downmix
     * @throws IOException
     */
    private void decodeUpdateFormat(String decoderName, int testInput, AudioParameter audioParams,
            int downmixChannelCount)
            throws IOException
    {
        AssetFileDescriptor testFd = mResources.openRawResourceFd(testInput);

        MediaExtractor extractor = new MediaExtractor();
        extractor.setDataSource(testFd.getFileDescriptor(), testFd.getStartOffset(),
                testFd.getLength());
        testFd.close();

        assertEquals("wrong number of tracks", 1, extractor.getTrackCount());
        MediaFormat format = extractor.getTrackFormat(0);
        String mime = format.getString(MediaFormat.KEY_MIME);
        assertTrue("not an audio file", mime.startsWith("audio/"));

        MediaCodec decoder;
        if (decoderName == null) {
            decoder = MediaCodec.createDecoderByType(mime);
        } else {
            decoder = MediaCodec.createByCodecName(decoderName);
        }

        MediaFormat configFormat = format;
        if (downmixChannelCount > 0) {
            configFormat.setInteger(
                    MediaFormat.KEY_AAC_MAX_OUTPUT_CHANNEL_COUNT, downmixChannelCount);
        }

        Log.v(TAG, "configuring with " + configFormat);
        decoder.configure(configFormat, null /* surface */, null /* crypto */, 0 /* flags */);

        decoder.start();
        ByteBuffer[] codecInputBuffers = decoder.getInputBuffers();
        ByteBuffer[] codecOutputBuffers = decoder.getOutputBuffers();

        extractor.selectTrack(0);

        // start decoding
        final long kTimeOutUs = 5000;
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        boolean sawInputEOS = false;
        boolean sawOutputEOS = false;
        int noOutputCounter = 0;
        int samplecounter = 0;
        short[] decoded = new short[0];
        int decodedIdx = 0;
        while (!sawOutputEOS && noOutputCounter < 50) {
            noOutputCounter++;
            if (!sawInputEOS) {
                int inputBufIndex = decoder.dequeueInputBuffer(kTimeOutUs);

                if (inputBufIndex >= 0) {
                    ByteBuffer dstBuf = codecInputBuffers[inputBufIndex];

                    int sampleSize =
                            extractor.readSampleData(dstBuf, 0 /* offset */);

                    long presentationTimeUs = 0;

                    if (sampleSize < 0) {
                        Log.d(TAG, "saw input EOS.");
                        sawInputEOS = true;
                        sampleSize = 0;
                    } else {
                        samplecounter++;
                        presentationTimeUs = extractor.getSampleTime();
                    }
                    decoder.queueInputBuffer(
                            inputBufIndex,
                            0 /* offset */,
                            sampleSize,
                            presentationTimeUs,
                            sawInputEOS ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);

                    if (!sawInputEOS) {
                        extractor.advance();
                    }
                }
            }

            int res = decoder.dequeueOutputBuffer(info, kTimeOutUs);

            if (res >= 0) {
                if (info.size > 0) {
                    noOutputCounter = 0;
                }

                int outputBufIndex = res;
                ByteBuffer buf = codecOutputBuffers[outputBufIndex];

                if (decodedIdx + (info.size / 2) >= decoded.length) {
                    decoded = Arrays.copyOf(decoded, decodedIdx + (info.size / 2));
                }

                buf.position(info.offset);
                for (int i = 0; i < info.size; i += 2) {
                    decoded[decodedIdx++] = buf.getShort();
                }

                decoder.releaseOutputBuffer(outputBufIndex, false /* render */);

                if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    Log.d(TAG, "saw output EOS.");
                    sawOutputEOS = true;
                }
            } else if (res == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                codecOutputBuffers = decoder.getOutputBuffers();
                Log.d(TAG, "output buffers have changed.");
            } else if (res == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                MediaFormat outputFormat = decoder.getOutputFormat();
                audioParams.setNumChannels(outputFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
                audioParams.setSamplingRate(outputFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE));
                Log.i(TAG, "output format has changed to " + outputFormat);
            } else {
                Log.d(TAG, "dequeueOutputBuffer returned " + res);
            }
        }
        if (noOutputCounter >= 50) {
            fail("decoder stopped outputing data");
        }
        decoder.stop();
        decoder.release();
        extractor.release();
    }
}

