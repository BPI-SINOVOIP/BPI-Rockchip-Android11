/*
 * Copyright (C) 2013 The Android Open Source Project
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

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.media.cts.R;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Verification test for video encoder and decoder.
 *
 * A raw yv12 stream is encoded at various settings and written to an IVF
 * file. Encoded stream bitrate and key frame interval are checked against target values.
 * The stream is later decoded by video decoder to verify frames are decodable and to
 * calculate PSNR values for various bitrates.
 */
@MediaHeavyPresubmitTest
@AppModeFull(reason = "TODO: evaluate and port to instant")
public class VideoCodecTest extends VideoCodecTestBase {

    private static final String ENCODED_IVF_BASE = "football";
    private static final String INPUT_YUV = null;
    private static final String OUTPUT_YUV = SDCARD_DIR + File.separator +
            ENCODED_IVF_BASE + "_out.yuv";

    // YUV stream properties.
    private static final int WIDTH = 320;
    private static final int HEIGHT = 240;
    private static final int FPS = 30;
    // Default encoding bitrate.
    private static final int BITRATE = 400000;
    // List of bitrates used in quality and basic bitrate tests.
    private static final int[] TEST_BITRATES_SET = { 300000, 500000, 700000, 900000 };
    // Maximum allowed bitrate variation from the target value.
    // Keep in sync with the variation at libmediandkjni/native_media_utils.h
    private static final double MAX_BITRATE_VARIATION = 0.2;
    // Average PSNR values for reference Google Video codec for the above bitrates.
    private static final double[] REFERENCE_AVERAGE_PSNR = { 33.1, 35.2, 36.6, 37.8 };
    // Minimum PSNR values for reference Google Video codec for the above bitrates.
    private static final double[] REFERENCE_MINIMUM_PSNR = { 25.9, 27.5, 28.4, 30.3 };
    // Maximum allowed average PSNR difference of encoder comparing to reference Google encoder.
    private static final double MAX_AVERAGE_PSNR_DIFFERENCE = 2;
    // Maximum allowed minimum PSNR difference of encoder comparing to reference Google encoder.
    private static final double MAX_MINIMUM_PSNR_DIFFERENCE = 4;
    // Maximum allowed average PSNR difference of the encoder running in a looper thread with 0 ms
    // buffer dequeue timeout comparing to the encoder running in a callee's thread with 100 ms
    // buffer dequeue timeout.
    private static final double MAX_ASYNC_AVERAGE_PSNR_DIFFERENCE = 1.5;
    // Maximum allowed minimum PSNR difference of the encoder running in a looper thread
    // comparing to the encoder running in a callee's thread.
    private static final double MAX_ASYNC_MINIMUM_PSNR_DIFFERENCE = 2;
    // Maximum allowed average key frame interval variation from the target value.
    private static final int MAX_AVERAGE_KEYFRAME_INTERVAL_VARIATION = 1;
    // Maximum allowed key frame interval variation from the target value.
    private static final int MAX_KEYFRAME_INTERVAL_VARIATION = 3;

    /**
     * A basic test for Video encoder.
     *
     * Encodes 9 seconds of raw stream with default configuration options,
     * and then decodes it to verify the bitstream.
     * Also checks the average bitrate is within MAX_BITRATE_VARIATION of the target value.
     */
    private void internalTestBasic(String codecMimeType, int bitRateMode) throws Exception {
        int encodeSeconds = 9;
        boolean skipped = true;

        for (int targetBitrate : TEST_BITRATES_SET) {
            EncoderOutputStreamParameters params = getDefaultEncodingParameters(
                    INPUT_YUV,
                    ENCODED_IVF_BASE,
                    codecMimeType,
                    encodeSeconds,
                    WIDTH,
                    HEIGHT,
                    FPS,
                    bitRateMode,
                    targetBitrate,
                    true);
            ArrayList<ByteBuffer> codecConfigs = new ArrayList<>();
            ArrayList<MediaCodec.BufferInfo> bufInfo = encode(params, codecConfigs);
            if (bufInfo == null) {
                continue;
            }
            skipped = false;

            VideoEncodingStatistics statistics = computeEncodingStatistics(bufInfo);

            /* Allow achieved bitrate to be smaller than target bitrate for
             * VIDEO_ControlRateVariable mode */
            if ((params.bitrateType == VIDEO_ControlRateConstant) ||
                (statistics.mAverageBitrate > targetBitrate)) {
                assertEquals("Stream bitrate " + statistics.mAverageBitrate +
                    " is different from the target " + targetBitrate,
                    targetBitrate, statistics.mAverageBitrate,
                    MAX_BITRATE_VARIATION * targetBitrate);
            }

            decode(params.outputIvfFilename, null, codecMimeType, FPS,
                    params.forceGoogleEncoder, codecConfigs);
        }

        if (skipped) {
            Log.i(TAG, "SKIPPING testBasic(): codec is not supported");
        }
    }

    /**
     * Asynchronous encoding test for Video encoder.
     *
     * Encodes 9 seconds of raw stream using synchronous and asynchronous calls.
     * Checks the PSNR difference between the encoded and decoded output and reference yuv input
     * does not change much for two different ways of the encoder call.
     */
    private void internalTestAsyncEncoding(String codecMimeType, int bitRateMode) throws Exception {
        int encodeSeconds = 9;

        // First test the encoder running in a looper thread with buffer callbacks enabled.
        boolean syncEncoding = false;
        EncoderOutputStreamParameters params = getDefaultEncodingParameters(
                INPUT_YUV,
                ENCODED_IVF_BASE,
                codecMimeType,
                encodeSeconds,
                WIDTH,
                HEIGHT,
                FPS,
                bitRateMode,
                BITRATE,
                syncEncoding);
        ArrayList<ByteBuffer> codecConfigs = new ArrayList<>();
        ArrayList<MediaCodec.BufferInfo> bufInfos = encodeAsync(params, codecConfigs);
        if (bufInfos == null) {
            Log.i(TAG, "SKIPPING testAsyncEncoding(): no suitable encoder found");
            return;
        }
        computeEncodingStatistics(bufInfos);
        decode(params.outputIvfFilename, OUTPUT_YUV, codecMimeType, FPS,
                params.forceGoogleEncoder, codecConfigs);
        VideoDecodingStatistics statisticsAsync = computeDecodingStatistics(
                params.inputYuvFilename, R.raw.football_qvga, OUTPUT_YUV,
                params.frameWidth, params.frameHeight);


        // Test the encoder running in a callee's thread.
        syncEncoding = true;
        params = getDefaultEncodingParameters(
                INPUT_YUV,
                ENCODED_IVF_BASE,
                codecMimeType,
                encodeSeconds,
                WIDTH,
                HEIGHT,
                FPS,
                bitRateMode,
                BITRATE,
                syncEncoding);
        codecConfigs.clear();
        bufInfos = encode(params, codecConfigs);
        if (bufInfos == null) {
            Log.i(TAG, "SKIPPING testAsyncEncoding(): no suitable encoder found");
            return;
        }
        computeEncodingStatistics(bufInfos);
        decode(params.outputIvfFilename, OUTPUT_YUV, codecMimeType, FPS,
                params.forceGoogleEncoder, codecConfigs);
        VideoDecodingStatistics statisticsSync = computeDecodingStatistics(
                params.inputYuvFilename, R.raw.football_qvga, OUTPUT_YUV,
                params.frameWidth, params.frameHeight);

        // Check PSNR difference.
        Log.d(TAG, "PSNR Average: Async: " + statisticsAsync.mAveragePSNR +
                ". Sync: " + statisticsSync.mAveragePSNR);
        Log.d(TAG, "PSNR Minimum: Async: " + statisticsAsync.mMinimumPSNR +
                ". Sync: " + statisticsSync.mMinimumPSNR);
        if ((Math.abs(statisticsAsync.mAveragePSNR - statisticsSync.mAveragePSNR) >
            MAX_ASYNC_AVERAGE_PSNR_DIFFERENCE) ||
            (Math.abs(statisticsAsync.mMinimumPSNR - statisticsSync.mMinimumPSNR) >
            MAX_ASYNC_MINIMUM_PSNR_DIFFERENCE)) {
            throw new RuntimeException("Difference between PSNRs for async and sync encoders");
        }
    }

    /**
     * Check if MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME is honored.
     *
     * Encodes 9 seconds of raw stream and requests a sync frame every second (30 frames).
     * The test does not verify the output stream.
     */
    private void internalTestSyncFrame(
            String codecMimeType, int bitRateMode, boolean useNdk) throws Exception {
        int encodeSeconds = 9;

        EncoderOutputStreamParameters params = getDefaultEncodingParameters(
                INPUT_YUV,
                ENCODED_IVF_BASE,
                codecMimeType,
                encodeSeconds,
                WIDTH,
                HEIGHT,
                FPS,
                bitRateMode,
                BITRATE,
                true);
        params.syncFrameInterval = encodeSeconds * FPS;
        params.syncForceFrameInterval = FPS;
        params.useNdk = useNdk;
        ArrayList<MediaCodec.BufferInfo> bufInfo = encode(params);
        if (bufInfo == null) {
            Log.i(TAG, "SKIPPING testSyncFrame(): no suitable encoder found");
            return;
        }

        VideoEncodingStatistics statistics = computeEncodingStatistics(bufInfo);

        // First check if we got expected number of key frames.
        int actualKeyFrames = statistics.mKeyFrames.size();
        if (actualKeyFrames != encodeSeconds) {
            throw new RuntimeException("Number of key frames " + actualKeyFrames +
                    " is different from the expected " + encodeSeconds);
        }

        // Check key frame intervals:
        // Average value should be within +/- 1 frame of the target value,
        // maximum value should not be greater than target value + 3,
        // and minimum value should not be less that target value - 3.
        if (Math.abs(statistics.mAverageKeyFrameInterval - FPS) >
            MAX_AVERAGE_KEYFRAME_INTERVAL_VARIATION ||
            (statistics.mMaximumKeyFrameInterval - FPS > MAX_KEYFRAME_INTERVAL_VARIATION) ||
            (FPS - statistics.mMinimumKeyFrameInterval > MAX_KEYFRAME_INTERVAL_VARIATION)) {
            throw new RuntimeException(
                    "Key frame intervals are different from the expected " + FPS);
        }
    }

    /**
     * Check if MediaCodec.PARAMETER_KEY_VIDEO_BITRATE is honored.
     *
     * Run the the encoder for 12 seconds. Request changes to the
     * bitrate after 6 seconds and ensure the encoder responds.
     */
    private void internalTestDynamicBitrateChange(
            String codecMimeType, int bitRateMode, boolean useNdk) throws Exception {
        int encodeSeconds = 12;    // Encoding sequence duration in seconds.
        int[] bitrateTargetValues = { 400000, 800000 };  // List of bitrates to test.

        EncoderOutputStreamParameters params = getDefaultEncodingParameters(
                INPUT_YUV,
                ENCODED_IVF_BASE,
                codecMimeType,
                encodeSeconds,
                WIDTH,
                HEIGHT,
                FPS,
                bitRateMode,
                bitrateTargetValues[0],
                true);

        // Number of seconds for each bitrate
        int stepSeconds = encodeSeconds / bitrateTargetValues.length;
        // Fill the bitrates values.
        params.bitrateSet = new int[encodeSeconds * FPS];
        for (int i = 0; i < bitrateTargetValues.length ; i++) {
            Arrays.fill(params.bitrateSet,
                    i * encodeSeconds * FPS / bitrateTargetValues.length,
                    (i + 1) * encodeSeconds * FPS / bitrateTargetValues.length,
                    bitrateTargetValues[i]);
        }

        params.useNdk = useNdk;
        ArrayList<MediaCodec.BufferInfo> bufInfo = encode(params);
        if (bufInfo == null) {
            Log.i(TAG, "SKIPPING testDynamicBitrateChange(): no suitable encoder found");
            return;
        }

        VideoEncodingStatistics statistics = computeEncodingStatistics(bufInfo);

        // Calculate actual average bitrates  for every [stepSeconds] second.
        int[] bitrateActualValues = new int[bitrateTargetValues.length];
        for (int i = 0; i < bitrateTargetValues.length ; i++) {
            bitrateActualValues[i] = 0;
            for (int j = i * stepSeconds; j < (i + 1) * stepSeconds; j++) {
                bitrateActualValues[i] += statistics.mBitrates.get(j);
            }
            bitrateActualValues[i] /= stepSeconds;
            Log.d(TAG, "Actual bitrate for interval #" + i + " : " + bitrateActualValues[i] +
                    ". Target: " + bitrateTargetValues[i]);

            // Compare actual bitrate values to make sure at least same increasing/decreasing
            // order as the target bitrate values.
            for (int j = 0; j < i; j++) {
                long differenceTarget = bitrateTargetValues[i] - bitrateTargetValues[j];
                long differenceActual = bitrateActualValues[i] - bitrateActualValues[j];
                if (differenceTarget * differenceActual < 0) {
                    throw new RuntimeException("Target bitrates: " +
                            bitrateTargetValues[j] + " , " + bitrateTargetValues[i] +
                            ". Actual bitrates: "
                            + bitrateActualValues[j] + " , " + bitrateActualValues[i]);
                }
            }
        }
    }

     /**
      * Check if encoder and decoder can run simultaneously on different threads.
      *
      * Encodes and decodes 9 seconds of raw stream sequentially in CBR mode,
      * and then run parallel encoding and decoding of the same streams.
      * Compares average bitrate and PSNR for sequential and parallel runs.
      */
     private void internalTestParallelEncodingAndDecoding(String codecMimeType) throws Exception {
         // check for encoder up front, as by the time we detect lack of
         // encoder support, we may have already started decoding.
         MediaCodecList mcl = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
         MediaFormat format = MediaFormat.createVideoFormat(codecMimeType, WIDTH, HEIGHT);
         if (mcl.findEncoderForFormat(format) == null) {
             Log.i(TAG, "SKIPPING testParallelEncodingAndDecoding(): no suitable encoder found");
             return;
         }

         int encodeSeconds = 9;
         final int[] bitrate = new int[1];
         final double[] psnr = new double[1];
         final Exception[] exceptionEncoder = new Exception[1];
         final Exception[] exceptionDecoder = new Exception[1];
         final EncoderOutputStreamParameters params = getDefaultEncodingParameters(
                 INPUT_YUV,
                 ENCODED_IVF_BASE,
                 codecMimeType,
                 encodeSeconds,
                 WIDTH,
                 HEIGHT,
                 FPS,
                 VIDEO_ControlRateConstant,
                 BITRATE,
                 true);
         final String inputIvfFilename = params.outputIvfFilename;
         final ArrayList<ByteBuffer> codecConfigs = new ArrayList<>();

         Runnable runEncoder = new Runnable() {
             public void run() {
                 try {
                     ArrayList<MediaCodec.BufferInfo> bufInfo;
                     if (codecConfigs.isEmpty()) {
                         bufInfo = encode(params, codecConfigs);
                     } else {
                         bufInfo = encode(params);
                     }
                     VideoEncodingStatistics statistics = computeEncodingStatistics(bufInfo);
                     bitrate[0] = statistics.mAverageBitrate;
                 } catch (Exception e) {
                     Log.e(TAG, "Encoder error: " + e.toString());
                     exceptionEncoder[0] = e;
                 }
             }
         };
         Runnable runDecoder = new Runnable() {
             public void run() {
                 try {
                     decode(inputIvfFilename, OUTPUT_YUV, codecMimeType, FPS,
                            params.forceGoogleEncoder, codecConfigs);
                     VideoDecodingStatistics statistics = computeDecodingStatistics(
                            params.inputYuvFilename, R.raw.football_qvga, OUTPUT_YUV,
                            params.frameWidth, params.frameHeight);
                     psnr[0] = statistics.mAveragePSNR;
                 } catch (Exception e) {
                     Log.e(TAG, "Decoder error: " + e.toString());
                     exceptionDecoder[0] = e;
                 }
             }
         };

         // Sequential encoding and decoding.
         runEncoder.run();
         if (exceptionEncoder[0] != null) {
             throw exceptionEncoder[0];
         }
         int referenceBitrate = bitrate[0];
         runDecoder.run();
         if (exceptionDecoder[0] != null) {
             throw exceptionDecoder[0];
         }
         double referencePsnr = psnr[0];

         // Parallel encoding and decoding.
         params.outputIvfFilename = SDCARD_DIR + File.separator + ENCODED_IVF_BASE + "_copy.ivf";
         Thread threadEncoder = new Thread(runEncoder);
         Thread threadDecoder = new Thread(runDecoder);
         threadEncoder.start();
         threadDecoder.start();
         threadEncoder.join();
         threadDecoder.join();
         if (exceptionEncoder[0] != null) {
             throw exceptionEncoder[0];
         }
         if (exceptionDecoder[0] != null) {
             throw exceptionDecoder[0];
         }

         // Compare bitrates and PSNRs for sequential and parallel cases.
         Log.d(TAG, "Sequential bitrate: " + referenceBitrate + ". PSNR: " + referencePsnr);
         Log.d(TAG, "Parallel bitrate: " + bitrate[0] + ". PSNR: " + psnr[0]);
         assertEquals("Bitrate for sequenatial encoding" + referenceBitrate +
                 " is different from parallel encoding " + bitrate[0],
                 referenceBitrate, bitrate[0], MAX_BITRATE_VARIATION * referenceBitrate);
         assertEquals("PSNR for sequenatial encoding" + referencePsnr +
                 " is different from parallel encoding " + psnr[0],
                 referencePsnr, psnr[0], MAX_ASYNC_AVERAGE_PSNR_DIFFERENCE);
     }


    /**
     * Check the encoder quality for various bitrates by calculating PSNR
     *
     * Run the the encoder for 9 seconds for each bitrate and calculate PSNR
     * for each encoded stream.
     * Video streams with higher bitrates should have higher PSNRs.
     * Also compares average and minimum PSNR of codec with PSNR values of reference Google codec.
     */
    private void internalTestEncoderQuality(String codecMimeType, int bitRateMode)
            throws Exception {
        int encodeSeconds = 9;      // Encoding sequence duration in seconds for each bitrate.
        double[] psnrPlatformCodecAverage = new double[TEST_BITRATES_SET.length];
        double[] psnrPlatformCodecMin = new double[TEST_BITRATES_SET.length];
        boolean[] completed = new boolean[TEST_BITRATES_SET.length];
        boolean skipped = true;

        // Run platform specific encoder for different bitrates
        // and compare PSNR of codec with PSNR of reference Google codec.
        for (int i = 0; i < TEST_BITRATES_SET.length; i++) {
            EncoderOutputStreamParameters params = getDefaultEncodingParameters(
                    INPUT_YUV,
                    ENCODED_IVF_BASE,
                    codecMimeType,
                    encodeSeconds,
                    WIDTH,
                    HEIGHT,
                    FPS,
                    bitRateMode,
                    TEST_BITRATES_SET[i],
                    true);
            ArrayList<ByteBuffer> codecConfigs = new ArrayList<>();
            if (encode(params, codecConfigs) == null) {
                // parameters not supported, try other bitrates
                completed[i] = false;
                continue;
            }
            completed[i] = true;
            skipped = false;

            decode(params.outputIvfFilename, OUTPUT_YUV, codecMimeType, FPS,
                    params.forceGoogleEncoder, codecConfigs);
            VideoDecodingStatistics statistics = computeDecodingStatistics(
                    params.inputYuvFilename, R.raw.football_qvga, OUTPUT_YUV,
                    params.frameWidth, params.frameHeight);
            psnrPlatformCodecAverage[i] = statistics.mAveragePSNR;
            psnrPlatformCodecMin[i] = statistics.mMinimumPSNR;
        }

        if (skipped) {
            Log.i(TAG, "SKIPPING testEncoderQuality(): no bitrates supported");
            return;
        }

        // First do a sanity check - higher bitrates should results in higher PSNR.
        for (int i = 1; i < TEST_BITRATES_SET.length ; i++) {
            if (!completed[i]) {
                continue;
            }
            for (int j = 0; j < i; j++) {
                if (!completed[j]) {
                    continue;
                }
                double differenceBitrate = TEST_BITRATES_SET[i] - TEST_BITRATES_SET[j];
                double differencePSNR = psnrPlatformCodecAverage[i] - psnrPlatformCodecAverage[j];
                if (differenceBitrate * differencePSNR < 0) {
                    throw new RuntimeException("Target bitrates: " +
                            TEST_BITRATES_SET[j] + ", " + TEST_BITRATES_SET[i] +
                            ". Actual PSNRs: "
                            + psnrPlatformCodecAverage[j] + ", " + psnrPlatformCodecAverage[i]);
                }
            }
        }

        // Then compare average and minimum PSNR of platform codec with reference Google codec -
        // average PSNR for platform codec should be no more than 2 dB less than reference PSNR
        // and minumum PSNR - no more than 4 dB less than reference minimum PSNR.
        // These PSNR difference numbers are arbitrary for now, will need further estimation
        // when more devices with HW video codec will appear.
        for (int i = 0; i < TEST_BITRATES_SET.length ; i++) {
            if (!completed[i]) {
                continue;
            }

            Log.d(TAG, "Bitrate " + TEST_BITRATES_SET[i]);
            Log.d(TAG, "Reference: Average: " + REFERENCE_AVERAGE_PSNR[i] + ". Minimum: " +
                    REFERENCE_MINIMUM_PSNR[i]);
            Log.d(TAG, "Platform:  Average: " + psnrPlatformCodecAverage[i] + ". Minimum: " +
                    psnrPlatformCodecMin[i]);
            if (psnrPlatformCodecAverage[i] < REFERENCE_AVERAGE_PSNR[i] -
                    MAX_AVERAGE_PSNR_DIFFERENCE) {
                throw new RuntimeException("Low average PSNR " + psnrPlatformCodecAverage[i] +
                        " comparing to reference PSNR " + REFERENCE_AVERAGE_PSNR[i] +
                        " for bitrate " + TEST_BITRATES_SET[i]);
            }
            if (psnrPlatformCodecMin[i] < REFERENCE_MINIMUM_PSNR[i] -
                    MAX_MINIMUM_PSNR_DIFFERENCE) {
                throw new RuntimeException("Low minimum PSNR " + psnrPlatformCodecMin[i] +
                        " comparing to reference PSNR " + REFERENCE_MINIMUM_PSNR[i] +
                        " for bitrate " + TEST_BITRATES_SET[i]);
            }
        }
    }

    public void testBasicVP8CBR() throws Exception {
        internalTestBasic(VP8_MIME, VIDEO_ControlRateConstant);
    }
    public void testBasicVP8VBR() throws Exception {
        internalTestBasic(VP8_MIME, VIDEO_ControlRateVariable);
    }

    public void testBasicVP9CBR() throws Exception {
        internalTestBasic(VP9_MIME, VIDEO_ControlRateConstant);
    }
    public void testBasicVP9VBR() throws Exception {
        internalTestBasic(VP9_MIME, VIDEO_ControlRateVariable);
    }

    public void testBasicAVCCBR() throws Exception {
        internalTestBasic(AVC_MIME, VIDEO_ControlRateConstant);
    }
    public void testBasicAVCVBR() throws Exception {
        internalTestBasic(AVC_MIME, VIDEO_ControlRateVariable);
    }
    public void testBasicHEVCCBR() throws Exception {
        internalTestBasic(HEVC_MIME, VIDEO_ControlRateConstant);
    }
    public void testBasicHEVCVBR() throws Exception {
        internalTestBasic(HEVC_MIME, VIDEO_ControlRateVariable);
    }
    public void testAsyncEncodingVP8CBR() throws Exception {
        internalTestAsyncEncoding(VP8_MIME, VIDEO_ControlRateConstant);
    }
    public void testAsyncEncodingVP8VBR() throws Exception {
        internalTestAsyncEncoding(VP8_MIME, VIDEO_ControlRateVariable);
    }

    public void testAsyncEncodingVP9CBR() throws Exception {
        internalTestAsyncEncoding(VP9_MIME, VIDEO_ControlRateConstant);
    }
    public void testAsyncEncodingVP9VBR() throws Exception {
        internalTestAsyncEncoding(VP9_MIME, VIDEO_ControlRateVariable);
    }

    public void testAsyncEncodingAVCCBR() throws Exception {
        internalTestAsyncEncoding(AVC_MIME, VIDEO_ControlRateConstant);
    }
    public void testAsyncEncodingAVCVBR() throws Exception {
        internalTestAsyncEncoding(AVC_MIME, VIDEO_ControlRateVariable);
    }
    public void testAsyncEncodingHEVCCBR() throws Exception {
        internalTestAsyncEncoding(HEVC_MIME, VIDEO_ControlRateConstant);
    }
    public void testAsyncEncodingHEVCVBR() throws Exception {
        internalTestAsyncEncoding(HEVC_MIME, VIDEO_ControlRateVariable);
    }

    public void testSyncFrameVP8CBR() throws Exception {
        internalTestSyncFrame(VP8_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testSyncFrameVP8VBR() throws Exception {
        internalTestSyncFrame(VP8_MIME, VIDEO_ControlRateVariable, false);
    }

    public void testSyncFrameVP8NdkCBR() throws Exception {
        internalTestSyncFrame(VP8_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testSyncFrameVP8NdkVBR() throws Exception {
        internalTestSyncFrame(VP8_MIME, VIDEO_ControlRateVariable, true);
    }

    public void testSyncFrameVP9CBR() throws Exception {
        internalTestSyncFrame(VP9_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testSyncFrameVP9VBR() throws Exception {
        internalTestSyncFrame(VP9_MIME, VIDEO_ControlRateVariable, false);
    }

    public void testSyncFrameVP9NdkCBR() throws Exception {
        internalTestSyncFrame(VP9_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testSyncFrameVP9NdkVBR() throws Exception {
        internalTestSyncFrame(VP9_MIME, VIDEO_ControlRateVariable, true);
    }

    public void testSyncFrameAVCCBR() throws Exception {
        internalTestSyncFrame(AVC_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testSyncFrameAVCVBR() throws Exception {
        internalTestSyncFrame(AVC_MIME, VIDEO_ControlRateVariable, false);
    }

    public void testSyncFrameAVCNdkCBR() throws Exception {
        internalTestSyncFrame(AVC_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testSyncFrameAVCNdkVBR() throws Exception {
        internalTestSyncFrame(AVC_MIME, VIDEO_ControlRateVariable, true);
    }

    public void testSyncFrameHEVCCBR() throws Exception {
        internalTestSyncFrame(HEVC_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testSyncFrameHEVCVBR() throws Exception {
        internalTestSyncFrame(HEVC_MIME, VIDEO_ControlRateVariable, false);
    }

    public void testSyncFrameHEVCNdkCBR() throws Exception {
        internalTestSyncFrame(HEVC_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testSyncFrameHEVCNdkVBR() throws Exception {
        internalTestSyncFrame(HEVC_MIME, VIDEO_ControlRateVariable, true);
    }

    public void testDynamicBitrateChangeVP8CBR() throws Exception {
        internalTestDynamicBitrateChange(VP8_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testDynamicBitrateChangeVP8VBR() throws Exception {
        internalTestDynamicBitrateChange(VP8_MIME, VIDEO_ControlRateVariable, false);
    }
    public void testDynamicBitrateChangeVP8NdkCBR() throws Exception {
        internalTestDynamicBitrateChange(VP8_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testDynamicBitrateChangeVP8NdkVBR() throws Exception {
        internalTestDynamicBitrateChange(VP8_MIME, VIDEO_ControlRateVariable, true);
    }
    public void testDynamicBitrateChangeVP9CBR() throws Exception {
        internalTestDynamicBitrateChange(VP9_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testDynamicBitrateChangeVP9VBR() throws Exception {
        internalTestDynamicBitrateChange(VP9_MIME, VIDEO_ControlRateVariable, false);
    }
    public void testDynamicBitrateChangeVP9NdkCBR() throws Exception {
        internalTestDynamicBitrateChange(VP9_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testDynamicBitrateChangeVP9NdkVBR() throws Exception {
        internalTestDynamicBitrateChange(VP9_MIME, VIDEO_ControlRateVariable, true);
    }
    public void testDynamicBitrateChangeAVCCBR() throws Exception {
        internalTestDynamicBitrateChange(AVC_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testDynamicBitrateChangeAVCVBR() throws Exception {
        internalTestDynamicBitrateChange(AVC_MIME, VIDEO_ControlRateVariable, false);
    }
    public void testDynamicBitrateChangeAVCNdkCBR() throws Exception {
        internalTestDynamicBitrateChange(AVC_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testDynamicBitrateChangeAVCNdkVBR() throws Exception {
        internalTestDynamicBitrateChange(AVC_MIME, VIDEO_ControlRateVariable, true);
    }
    public void testDynamicBitrateChangeHEVCCBR() throws Exception {
        internalTestDynamicBitrateChange(HEVC_MIME, VIDEO_ControlRateConstant, false);
    }
    public void testDynamicBitrateChangeHEVCVBR() throws Exception {
        internalTestDynamicBitrateChange(HEVC_MIME, VIDEO_ControlRateVariable, false);
    }
    public void testDynamicBitrateChangeHEVCNdkCBR() throws Exception {
        internalTestDynamicBitrateChange(HEVC_MIME, VIDEO_ControlRateConstant, true);
    }
    public void testDynamicBitrateChangeHEVCNdkVBR() throws Exception {
        internalTestDynamicBitrateChange(HEVC_MIME, VIDEO_ControlRateVariable, true);
    }

    public void testEncoderQualityVP8CBR() throws Exception {
        internalTestEncoderQuality(VP8_MIME, VIDEO_ControlRateConstant);
    }
    public void testEncoderQualityVP8VBR() throws Exception {
        internalTestEncoderQuality(VP8_MIME, VIDEO_ControlRateVariable);
    }

    public void testEncoderQualityVP9CBR() throws Exception {
        internalTestEncoderQuality(VP9_MIME, VIDEO_ControlRateConstant);
    }
    public void testEncoderQualityVP9VBR() throws Exception {
        internalTestEncoderQuality(VP9_MIME, VIDEO_ControlRateVariable);
    }

    public void testEncoderQualityAVCCBR() throws Exception {
        internalTestEncoderQuality(AVC_MIME, VIDEO_ControlRateConstant);
    }
    public void testEncoderQualityAVCVBR() throws Exception {
        internalTestEncoderQuality(AVC_MIME, VIDEO_ControlRateVariable);
    }

    public void testEncoderQualityHEVCCBR() throws Exception {
        internalTestEncoderQuality(HEVC_MIME, VIDEO_ControlRateConstant);
    }
    public void testEncoderQualityHEVCVBR() throws Exception {
        internalTestEncoderQuality(HEVC_MIME, VIDEO_ControlRateVariable);
    }

    public void testParallelEncodingAndDecodingVP8() throws Exception {
        internalTestParallelEncodingAndDecoding(VP8_MIME);
    }
    public void testParallelEncodingAndDecodingVP9() throws Exception {
        internalTestParallelEncodingAndDecoding(VP9_MIME);
    }
    public void testParallelEncodingAndDecodingAVC() throws Exception {
        internalTestParallelEncodingAndDecoding(AVC_MIME);
    }
    public void testParallelEncodingAndDecodingHEVC() throws Exception {
        internalTestParallelEncodingAndDecoding(HEVC_MIME);
    }
}

