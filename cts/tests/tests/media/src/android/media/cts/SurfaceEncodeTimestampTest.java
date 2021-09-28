/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CodecException;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaFormat;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.RequiresDevice;
import android.test.AndroidTestCase;
import android.util.Log;

import androidx.test.filters.SmallTest;
import androidx.test.filters.SdkSuppress;

import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import java.util.function.Supplier;

@Presubmit
@AppModeFull(reason = "TODO: evaluate and port to instant")
@SmallTest
@RequiresDevice
public class SurfaceEncodeTimestampTest extends AndroidTestCase {
    private static final String TAG = SurfaceEncodeTimestampTest.class.getSimpleName();
    private static final boolean DEBUG = false;

    private static final Color COLOR_BLOCK =
            Color.valueOf(1.0f, 1.0f, 1.0f);
    private static final Color[] COLOR_BARS = {
            Color.valueOf(0.0f, 0.0f, 0.0f),
            Color.valueOf(0.0f, 0.0f, 0.64f),
            Color.valueOf(0.0f, 0.64f, 0.0f),
            Color.valueOf(0.0f, 0.64f, 0.64f),
            Color.valueOf(0.64f, 0.0f, 0.0f),
            Color.valueOf(0.64f, 0.0f, 0.64f),
            Color.valueOf(0.64f, 0.64f, 0.0f),
    };
    private static final int BORDER_WIDTH = 16;
    private static final int OUTPUT_FRAME_RATE = 30;

    private Handler mHandler;
    private HandlerThread mHandlerThread;
    private MediaCodec mEncoder;
    private InputSurface mInputEglSurface;
    private int mInputCount;

    @Override
    public void setUp() throws Exception {
        if (mHandlerThread == null) {
            mHandlerThread = new HandlerThread(
                    "EncoderThread", Process.THREAD_PRIORITY_FOREGROUND);
            mHandlerThread.start();
            mHandler = new Handler(mHandlerThread.getLooper());
        }
    }

    @Override
    public void tearDown() throws Exception {
        mHandler = null;
        if (mHandlerThread != null) {
            mHandlerThread.quit();
            mHandlerThread = null;
        }
    }

    /*
     * Test KEY_MAX_PTS_GAP_TO_ENCODER
     *
     * This key is supposed to cap the gap between any two frames fed to the encoder,
     * and restore the output pts back to the original. Since the pts is not supposed
     * to be modified, we can't really verify that the "capping" actually took place.
     * However, we can at least verify that the pts is preserved.
     */
    public void testMaxPtsGap() throws Throwable {
        long[] inputPts = {1000000, 2000000, 3000000, 4000000};
        long[] expectedOutputPts = {1000000, 2000000, 3000000, 4000000};
        doTest(inputPts, expectedOutputPts, (format) -> {
            format.setLong(MediaFormat.KEY_MAX_PTS_GAP_TO_ENCODER, 33333);
        });
    }

    /*
     * Test that by default backward-going frames get dropped.
     */
    public void testBackwardFrameDroppedWithoutFixedPtsGap() throws Throwable {
        long[] inputPts = {33333, 66667, 66000, 100000};
        long[] expectedOutputPts = {33333, 66667, 100000};
        doTest(inputPts, expectedOutputPts, null);
    }

    /*
     * Test KEY_MAX_PTS_GAP_TO_ENCODER
     *
     * Test that when fixed pts gap is used, backward-going frames are accepted
     * and the pts is preserved.
     */
    public void testBackwardFramePreservedWithFixedPtsGap() throws Throwable {
        long[] inputPts = {33333, 66667, 66000, 100000};
        long[] expectedOutputPts = {33333, 66667, 66000, 100000};
        doTest(inputPts, expectedOutputPts, (format) -> {
            format.setLong(MediaFormat.KEY_MAX_PTS_GAP_TO_ENCODER, -33333);
        });
    }

    /*
     * Test KEY_MAX_FPS_TO_ENCODER
     *
     * Input frames are timestamped at 60fps, the key is supposed to drop
     * one every other frame to maintain 30fps output.
     */
    public void testMaxFps() throws Throwable {
        long[] inputPts = {16667, 33333, 50000, 66667, 83333};
        long[] expectedOutputPts = {16667, 50000, 83333};
        doTest(inputPts, expectedOutputPts, (format) -> {
            format.setFloat(MediaFormat.KEY_MAX_FPS_TO_ENCODER, 30.0f);
        });
    }

    /*
     * Test KEY_CAPTURE_RATE
     *
     * Input frames are timestamped at various capture fps to simulate slow-motion
     * or timelapse recording scenarios. The key is supposed to adjust (stretch or
     * compress) the output timestamp so that the output fps becomes that specified
     * by  KEY_FRAME_RATE.
     */
    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.R)
    public void testCaptureFps() throws Throwable {
        // test slow motion
        testCaptureFps(120, false /*useFloatKey*/);
        testCaptureFps(240, true /*useFloatKey*/);

        // test timelapse
        testCaptureFps(1, false /*useFloatKey*/);
    }

    private void testCaptureFps(int captureFps, final boolean useFloatKey) throws Throwable {
        long[] inputPts = new long[4];
        long[] expectedOutputPts = new long[4];
        final long bastPts = 1000000;
        for (int i = 0; i < inputPts.length; i++) {
            inputPts[i] = bastPts + (int)(i * 1000000.0f / captureFps + 0.5f);
            expectedOutputPts[i] = inputPts[i] * captureFps / OUTPUT_FRAME_RATE;
        }

        doTest(inputPts, expectedOutputPts, (format) -> {
            if (useFloatKey) {
                format.setFloat(MediaFormat.KEY_CAPTURE_RATE, captureFps);
            } else {
                format.setInteger(MediaFormat.KEY_CAPTURE_RATE, captureFps);
            }
        });
    }

    /*
     * Test KEY_REPEAT_PREVIOUS_FRAME_AFTER
     *
     * Test that the frame is repeated at least once if no new frame arrives after
     * the specified amount of time.
     */
    public void testRepeatPreviousFrameAfter() throws Throwable {
        long[] inputPts = {16667, 33333, -100000, 133333};
        long[] expectedOutputPts = {16667, 33333, 103333};
        doTest(inputPts, expectedOutputPts, (format) -> {
            format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, 70000);
        });
    }

    /*
     * Test KEY_CREATE_INPUT_SURFACE_SUSPENDED and PARAMETER_KEY_SUSPEND
     *
     * Start the encoder with KEY_CREATE_INPUT_SURFACE_SUSPENDED set, then resume
     * by PARAMETER_KEY_SUSPEND. Verify only frames after resume are captured.
     */
    public void testCreateInputSurfaceSuspendedResume() throws Throwable {
        // Using PARAMETER_KEY_SUSPEND (instead of PARAMETER_KEY_SUSPEND +
        // PARAMETER_KEY_SUSPEND_TIME) to resume doesn't enforce a time
        // for the action to take effect. Due to the asynchronous operation
        // between the MediaCodec's parameters and the input surface, frames
        // rendered before the resume call may reach encoder input side after
        // the resume. Here we do a slight wait (100000us) to make sure that
        // the resume only takes effect on the frame with timestamp 100000.
        long[] inputPts = {33333, 66667, -100000, 100000, 133333};
        long[] expectedOutputPts = {100000, 133333};
        doTest(inputPts, expectedOutputPts, (format) -> {
            format.setInteger(MediaFormat.KEY_CREATE_INPUT_SURFACE_SUSPENDED, 1);
        }, () -> {
            Bundle params = new Bundle();
            params.putInt(MediaCodec.PARAMETER_KEY_SUSPEND, 0);
            return params;
        });
    }

    /*
     * Test KEY_CREATE_INPUT_SURFACE_SUSPENDED,
     * PARAMETER_KEY_SUSPEND and PARAMETER_KEY_SUSPEND_TIME
     *
     * Start the encoder with KEY_CREATE_INPUT_SURFACE_SUSPENDED set, then request resume
     * at specific time using PARAMETER_KEY_SUSPEND + PARAMETER_KEY_SUSPEND_TIME.
     * Verify only frames after the specified time are captured.
     */
     public void testCreateInputSurfaceSuspendedResumeWithTime() throws Throwable {
         // Unlike using PARAMETER_KEY_SUSPEND alone to resume, using PARAMETER_KEY_SUSPEND
         // + PARAMETER_KEY_SUSPEND_TIME to resume can be scheduled any time before the
         // frame with the specified time arrives. Here we do it immediately after start.
         long[] inputPts = {-1, 33333, 66667, 100000, 133333};
         long[] expectedOutputPts = {100000, 133333};
         doTest(inputPts, expectedOutputPts, (format) -> {
             format.setInteger(MediaFormat.KEY_CREATE_INPUT_SURFACE_SUSPENDED, 1);
         }, () -> {
             Bundle params = new Bundle();
             params.putInt(MediaCodec.PARAMETER_KEY_SUSPEND, 0);
             params.putLong(MediaCodec.PARAMETER_KEY_SUSPEND_TIME, 100000);
             return params;
         });
    }

     /*
      * Test PARAMETER_KEY_SUSPEND.
      *
      * Suspend/resume during capture, and verify that frames during the suspension
      * period are dropped.
      */
    public void testSuspendedResume() throws Throwable {
        // Using PARAMETER_KEY_SUSPEND (instead of PARAMETER_KEY_SUSPEND +
        // PARAMETER_KEY_SUSPEND_TIME) to suspend/resume doesn't enforce a time
        // for the action to take effect. Due to the asynchronous operation
        // between the MediaCodec's parameters and the input surface, frames
        // rendered before the request may reach encoder input side after
        // the request. Here we do a slight wait (100000us) to make sure that
        // the suspend/resume only takes effect on the next frame.
        long[] inputPts = {33333, 66667, -100000, 100000, 133333, -100000, 166667};
        long[] expectedOutputPts = {33333, 66667, 166667};
        doTest(inputPts, expectedOutputPts, null, () -> {
            Bundle params = new Bundle();
            params.putInt(MediaCodec.PARAMETER_KEY_SUSPEND, 1);
            return params;
        }, () -> {
            Bundle params = new Bundle();
            params.putInt(MediaCodec.PARAMETER_KEY_SUSPEND, 0);
            return params;
        });
    }

    /*
     * Test PARAMETER_KEY_SUSPEND + PARAMETER_KEY_SUSPEND_TIME.
     *
     * Suspend/resume with specified time during capture, and verify that frames during
     * the suspension period are dropped.
     */
    public void testSuspendedResumeWithTime() throws Throwable {
        // Unlike using PARAMETER_KEY_SUSPEND alone to suspend/resume, requests using
        // PARAMETER_KEY_SUSPEND + PARAMETER_KEY_SUSPEND_TIME can be scheduled any time
        // before the frame with the specified time arrives. Queue both requests shortly
        // after start and test that they take place at the proper frames.
        long[] inputPts = {-1, 33333, -1, 66667, 100000, 133333, 166667};
        long[] expectedOutputPts = {33333, 66667, 166667};
        doTest(inputPts, expectedOutputPts, null, () -> {
            Bundle params = new Bundle();
            params.putInt(MediaCodec.PARAMETER_KEY_SUSPEND, 1);
            params.putLong(MediaCodec.PARAMETER_KEY_SUSPEND_TIME, 100000);
            return params;
        }, () -> {
            Bundle params = new Bundle();
            params.putInt(MediaCodec.PARAMETER_KEY_SUSPEND, 0);
            params.putLong(MediaCodec.PARAMETER_KEY_SUSPEND_TIME, 166667);
            return params;
        });
    }

    /*
     * Test PARAMETER_KEY_OFFSET_TIME.
     *
     * Apply PARAMETER_KEY_OFFSET_TIME during capture, and verify that the pts
     * of frames after the request are adjusted by the offset correctly.
     */
    public void testOffsetTime() throws Throwable {
        long[] inputPts = {33333, 66667, -100000, 100000, 133333};
        long[] expectedOutputPts = {33333, 66667, 83333, 116666};
        doTest(inputPts, expectedOutputPts, null, () -> {
            Bundle params = new Bundle();
            params.putLong(MediaCodec.PARAMETER_KEY_OFFSET_TIME, -16667);
            return params;
        });
    }

    private void doTest(long[] inputPtsUs, long[] expectedOutputPtsUs,
            Consumer<MediaFormat> configSetter, Supplier<Bundle>... paramGetter) throws Exception {

        try {
            if (DEBUG) Log.d(TAG, "started");

            // setup surface encoder format
            mEncoder = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            MediaFormat codecFormat = MediaFormat.createVideoFormat(
                    MediaFormat.MIMETYPE_VIDEO_AVC, 1280, 720);
            codecFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 0);
            codecFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    CodecCapabilities.COLOR_FormatSurface);
            codecFormat.setInteger(MediaFormat.KEY_FRAME_RATE, OUTPUT_FRAME_RATE);
            codecFormat.setInteger(MediaFormat.KEY_BITRATE_MODE,
                    MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR);
            codecFormat.setInteger(MediaFormat.KEY_BIT_RATE, 6000000);

            if (configSetter != null) {
                configSetter.accept(codecFormat);
            }

            CountDownLatch latch = new CountDownLatch(1);

            // configure and start encoder
            long[] actualOutputPtsUs = new long[expectedOutputPtsUs.length];
            mEncoder.setCallback(new EncoderCallback(latch, actualOutputPtsUs), mHandler);
            mEncoder.configure(codecFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

            mInputEglSurface = new InputSurface(mEncoder.createInputSurface());

            mEncoder.start();

            mInputCount = 0;
            int paramIndex = 0;
            // perform input operations
            for (int i = 0; i < inputPtsUs.length; i++) {
                if (DEBUG) Log.d(TAG, "drawFrame: " + i + ", pts " + inputPtsUs[i]);

                if (inputPtsUs[i] < 0) {
                    if (inputPtsUs[i] < -1) {
                        // larger negative number means that a sleep is required
                        // before the parameter test.
                        Thread.sleep(-inputPtsUs[i]/1000);
                    }
                    if (paramIndex < paramGetter.length && paramGetter[paramIndex] != null) {
                        // this means a pause to apply parameter to be tested
                        mEncoder.setParameters(paramGetter[paramIndex].get());
                    }
                    paramIndex++;
                } else {
                    drawFrame(1280, 720, inputPtsUs[i]);
                }
            }

            // if it worked there is really no reason to take longer..
            latch.await(1000, TimeUnit.MILLISECONDS);

            // verify output timestamps
            assertTrue("mismatch in output timestamp",
                    Arrays.equals(expectedOutputPtsUs, actualOutputPtsUs));

            if (DEBUG) Log.d(TAG, "stopped");
        } finally {
            if (mEncoder != null) {
                mEncoder.stop();
                mEncoder.release();
                mEncoder = null;
            }
            if (mInputEglSurface != null) {
                // This also releases the surface from encoder.
                mInputEglSurface.release();
                mInputEglSurface = null;
            }
        }
    }

    class EncoderCallback extends MediaCodec.Callback {
        private boolean mOutputEOS;
        private int mOutputCount;
        private final CountDownLatch mLatch;
        private final long[] mActualOutputPts;
        private final int mMaxOutput;

        EncoderCallback(CountDownLatch latch, long[] actualOutputPts) {
            mLatch = latch;
            mActualOutputPts = actualOutputPts;
            mMaxOutput = actualOutputPts.length;
        }

        @Override
        public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
            if (codec != mEncoder) return;
            if (DEBUG) Log.d(TAG, "onOutputFormatChanged: " + format);
        }

        @Override
        public void onInputBufferAvailable(MediaCodec codec, int index) {
            if (codec != mEncoder) return;
            if (DEBUG) Log.d(TAG, "onInputBufferAvailable: " + index);
        }

        @Override
        public void onOutputBufferAvailable(MediaCodec codec, int index, BufferInfo info) {
            if (codec != mEncoder || mOutputEOS) return;

            if (DEBUG) {
                Log.d(TAG, "onOutputBufferAvailable: " + index
                        + ", time " + info.presentationTimeUs
                        + ", size " + info.size
                        + ", flags " + info.flags);
            }

            if ((info.size > 0) && ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0)) {
                mActualOutputPts[mOutputCount++] = info.presentationTimeUs;
            }

            mOutputEOS |= ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) ||
                    (mOutputCount == mMaxOutput);

            codec.releaseOutputBuffer(index, false);

            if (mOutputEOS) {
                stopAndNotify(null);
            }
        }

        @Override
        public void onError(MediaCodec codec, CodecException e) {
            if (codec != mEncoder) return;

            Log.e(TAG, "onError: " + e);
            stopAndNotify(e);
        }

        private void stopAndNotify(CodecException e) {
            mLatch.countDown();
        }
    }

    private void drawFrame(int width, int height, long ptsUs) {
        mInputEglSurface.makeCurrent();
        generateSurfaceFrame(mInputCount, width, height);
        mInputEglSurface.setPresentationTime(1000 * ptsUs);
        mInputEglSurface.swapBuffers();
        mInputCount++;
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
}
