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
package com.google.android.exoplayer2.video;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.SurfaceTexture;
import android.os.Handler;
import android.os.SystemClock;
import android.view.Surface;
import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.RendererCapabilities;
import com.google.android.exoplayer2.RendererConfiguration;
import com.google.android.exoplayer2.decoder.DecoderException;
import com.google.android.exoplayer2.decoder.SimpleDecoder;
import com.google.android.exoplayer2.drm.ExoMediaCrypto;
import com.google.android.exoplayer2.testutil.FakeSampleStream;
import com.google.android.exoplayer2.testutil.FakeSampleStream.FakeSampleStreamItem;
import com.google.android.exoplayer2.util.MimeTypes;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

/** Unit test for {@link DecoderVideoRenderer}. */
@LooperMode(LooperMode.Mode.PAUSED)
@RunWith(AndroidJUnit4.class)
public final class DecoderVideoRendererTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final Format H264_FORMAT =
      new Format.Builder()
          .setSampleMimeType(MimeTypes.VIDEO_H264)
          .setWidth(1920)
          .setHeight(1080)
          .build();

  private DecoderVideoRenderer renderer;
  @Mock private VideoRendererEventListener eventListener;

  @Before
  public void setUp() {
    renderer =
        new DecoderVideoRenderer(
            /* allowedJoiningTimeMs= */ 0,
            new Handler(),
            eventListener,
            /* maxDroppedFramesToNotify= */ -1) {

          private final Object pendingDecodeCallLock = new Object();

          @GuardedBy("pendingDecodeCallLock")
          private int pendingDecodeCalls;

          @C.VideoOutputMode private int outputMode;

          @Override
          public String getName() {
            return "TestVideoRenderer";
          }

          @Override
          @Capabilities
          public int supportsFormat(Format format) {
            return RendererCapabilities.create(FORMAT_HANDLED);
          }

          @Override
          protected void setDecoderOutputMode(@C.VideoOutputMode int outputMode) {
            this.outputMode = outputMode;
          }

          @Override
          protected void renderOutputBufferToSurface(
              VideoDecoderOutputBuffer outputBuffer, Surface surface) {
            // Do nothing.
          }

          @Override
          protected void onQueueInputBuffer(VideoDecoderInputBuffer buffer) {
            // SimpleDecoder.decode() is called on a background thread we have no control about from
            // the test. Ensure the background calls are predictably serialized by waiting for them
            // to finish:
            //  1. Mark decode calls as "pending" here.
            //  2. Send a message on the test thread to wait for all pending decode calls.
            //  3. Decrement the pending counter in decode calls and wake up the waiting test.
            //  4. The tests need to call ShadowLooper.idleMainThread() to wait for pending calls.
            synchronized (pendingDecodeCallLock) {
              pendingDecodeCalls++;
            }
            new Handler()
                .post(
                    () -> {
                      synchronized (pendingDecodeCallLock) {
                        while (pendingDecodeCalls > 0) {
                          try {
                            pendingDecodeCallLock.wait();
                          } catch (InterruptedException e) {
                            // Ignore.
                          }
                        }
                      }
                    });
            super.onQueueInputBuffer(buffer);
          }

          @Override
          protected SimpleDecoder<
                  VideoDecoderInputBuffer,
                  ? extends VideoDecoderOutputBuffer,
                  ? extends DecoderException>
              createDecoder(Format format, @Nullable ExoMediaCrypto mediaCrypto) {
            return new SimpleDecoder<
                VideoDecoderInputBuffer, VideoDecoderOutputBuffer, DecoderException>(
                new VideoDecoderInputBuffer[10], new VideoDecoderOutputBuffer[10]) {
              @Override
              protected VideoDecoderInputBuffer createInputBuffer() {
                return new VideoDecoderInputBuffer();
              }

              @Override
              protected VideoDecoderOutputBuffer createOutputBuffer() {
                return new VideoDecoderOutputBuffer(this::releaseOutputBuffer);
              }

              @Override
              protected DecoderException createUnexpectedDecodeException(Throwable error) {
                return new DecoderException("error", error);
              }

              @Nullable
              @Override
              protected DecoderException decode(
                  VideoDecoderInputBuffer inputBuffer,
                  VideoDecoderOutputBuffer outputBuffer,
                  boolean reset) {
                outputBuffer.init(inputBuffer.timeUs, outputMode, /* supplementalData= */ null);
                synchronized (pendingDecodeCallLock) {
                  pendingDecodeCalls--;
                  pendingDecodeCallLock.notify();
                }
                return null;
              }

              @Override
              public String getName() {
                return "TestDecoder";
              }
            };
          }
        };
    renderer.setOutputSurface(new Surface(new SurfaceTexture(/* texName= */ 0)));
  }

  @Test
  public void enable_withMayRenderStartOfStream_rendersFirstFrameBeforeStart() throws Exception {
    FakeSampleStream fakeSampleStream =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME));

    renderer.enable(
        RendererConfiguration.DEFAULT,
        new Format[] {H264_FORMAT},
        fakeSampleStream,
        /* positionUs= */ 0,
        /* joining= */ false,
        /* mayRenderStartOfStream= */ true,
        /* offsetUs */ 0);
    for (int i = 0; i < 10; i++) {
      renderer.render(/* positionUs= */ 0, SystemClock.elapsedRealtime() * 1000);
      // Ensure pending messages are delivered.
      ShadowLooper.idleMainLooper();
    }

    verify(eventListener).onRenderedFirstFrame(any());
  }

  @Test
  public void enable_withoutMayRenderStartOfStream_doesNotRenderFirstFrameBeforeStart()
      throws Exception {
    FakeSampleStream fakeSampleStream =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME));

    renderer.enable(
        RendererConfiguration.DEFAULT,
        new Format[] {H264_FORMAT},
        fakeSampleStream,
        /* positionUs= */ 0,
        /* joining= */ false,
        /* mayRenderStartOfStream= */ false,
        /* offsetUs */ 0);
    for (int i = 0; i < 10; i++) {
      renderer.render(/* positionUs= */ 0, SystemClock.elapsedRealtime() * 1000);
      // Ensure pending messages are delivered.
      ShadowLooper.idleMainLooper();
    }

    verify(eventListener, never()).onRenderedFirstFrame(any());
  }

  @Test
  public void enable_withoutMayRenderStartOfStream_rendersFirstFrameAfterStart() throws Exception {
    FakeSampleStream fakeSampleStream =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME));

    renderer.enable(
        RendererConfiguration.DEFAULT,
        new Format[] {H264_FORMAT},
        fakeSampleStream,
        /* positionUs= */ 0,
        /* joining= */ false,
        /* mayRenderStartOfStream= */ false,
        /* offsetUs */ 0);
    renderer.start();
    for (int i = 0; i < 10; i++) {
      renderer.render(/* positionUs= */ 0, SystemClock.elapsedRealtime() * 1000);
      // Ensure pending messages are delivered.
      ShadowLooper.idleMainLooper();
    }

    verify(eventListener).onRenderedFirstFrame(any());
  }

  // TODO: Fix rendering of first frame at stream transition.
  @Ignore
  @Test
  public void replaceStream_whenStarted_rendersFirstFrameOfNewStream() throws Exception {
    FakeSampleStream fakeSampleStream1 =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME),
            FakeSampleStreamItem.END_OF_STREAM_ITEM);
    FakeSampleStream fakeSampleStream2 =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME),
            FakeSampleStreamItem.END_OF_STREAM_ITEM);
    renderer.enable(
        RendererConfiguration.DEFAULT,
        new Format[] {H264_FORMAT},
        fakeSampleStream1,
        /* positionUs= */ 0,
        /* joining= */ false,
        /* mayRenderStartOfStream= */ true,
        /* offsetUs */ 0);
    renderer.start();

    boolean replacedStream = false;
    for (int i = 0; i <= 10; i++) {
      renderer.render(/* positionUs= */ i * 10, SystemClock.elapsedRealtime() * 1000);
      if (!replacedStream && renderer.hasReadStreamToEnd()) {
        renderer.replaceStream(new Format[] {H264_FORMAT}, fakeSampleStream2, /* offsetUs= */ 100);
        replacedStream = true;
      }
      // Ensure pending messages are delivered.
      ShadowLooper.idleMainLooper();
    }

    verify(eventListener, times(2)).onRenderedFirstFrame(any());
  }

  // TODO: Fix rendering of first frame at stream transition.
  @Ignore
  @Test
  public void replaceStream_whenNotStarted_doesNotRenderFirstFrameOfNewStream() throws Exception {
    FakeSampleStream fakeSampleStream1 =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME),
            FakeSampleStreamItem.END_OF_STREAM_ITEM);
    FakeSampleStream fakeSampleStream2 =
        new FakeSampleStream(
            /* format= */ H264_FORMAT,
            /* eventDispatcher= */ null,
            /* firstSampleTimeUs= */ 0,
            /* timeUsIncrement= */ 50,
            new FakeSampleStreamItem(new byte[] {0}, C.BUFFER_FLAG_KEY_FRAME),
            FakeSampleStreamItem.END_OF_STREAM_ITEM);
    renderer.enable(
        RendererConfiguration.DEFAULT,
        new Format[] {H264_FORMAT},
        fakeSampleStream1,
        /* positionUs= */ 0,
        /* joining= */ false,
        /* mayRenderStartOfStream= */ true,
        /* offsetUs */ 0);

    boolean replacedStream = false;
    for (int i = 0; i < 10; i++) {
      renderer.render(/* positionUs= */ i * 10, SystemClock.elapsedRealtime() * 1000);
      if (!replacedStream && renderer.hasReadStreamToEnd()) {
        renderer.replaceStream(new Format[] {H264_FORMAT}, fakeSampleStream2, /* offsetUs= */ 100);
        replacedStream = true;
      }
      // Ensure pending messages are delivered.
      ShadowLooper.idleMainLooper();
    }

    verify(eventListener).onRenderedFirstFrame(any());

    // Render to streamOffsetUs and verify the new first frame gets rendered.
    renderer.render(/* positionUs= */ 100, SystemClock.elapsedRealtime() * 1000);

    verify(eventListener, times(2)).onRenderedFirstFrame(any());
  }
}
