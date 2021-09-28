/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.tuner.exoplayer;

import android.content.Context;
import android.support.annotation.Nullable;

import com.android.tv.tuner.exoplayer.MpegTsPlayer.RendererBuilder;
import com.android.tv.tuner.exoplayer.MpegTsPlayer.RendererBuilderCallback;
import com.android.tv.tuner.exoplayer.audio.MpegTsDefaultAudioTrackRenderer;
import com.android.tv.tuner.exoplayer.buffer.BufferManager;
import com.android.tv.tuner.exoplayer.buffer.PlaybackBufferListener;

import com.google.android.exoplayer.MediaCodecSelector;
import com.google.android.exoplayer.SampleSource;
import com.google.android.exoplayer.TrackRenderer;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.auto.factory.AutoFactory;
import com.google.auto.factory.Provided;

/** Builder for renderer objects for {@link MpegTsPlayer}. */
public class MpegTsRendererBuilder implements RendererBuilder {
    private final Context mContext;
    private final BufferManager mBufferManager;
    private final PlaybackBufferListener mBufferListener;
    private final MpegTsSampleExtractor.Factory mMpegTsSampleExtractorFactory;

    /**
     * Factory for {@link MpegTsRendererBuilder}.
     *
     * <p>This wrapper class keeps other classes from needing to reference the {@link AutoFactory}
     * generated class.
     */
    public interface Factory {
        public MpegTsRendererBuilder create(
                Context context,
                @Nullable BufferManager bufferManager,
                PlaybackBufferListener bufferListener);
    }

    @AutoFactory(implementing = Factory.class)
    public MpegTsRendererBuilder(
            Context context,
            @Nullable BufferManager bufferManager,
            PlaybackBufferListener bufferListener,
            @Provided MpegTsSampleExtractor.Factory mpegTsSampleExtractorFactory) {
        mContext = context;
        mBufferManager = bufferManager;
        mBufferListener = bufferListener;
        mMpegTsSampleExtractorFactory = mpegTsSampleExtractorFactory;
    }

    @Override
    public void buildRenderers(
            MpegTsPlayer mpegTsPlayer, DataSource dataSource, RendererBuilderCallback callback) {
        // Build the video and audio renderers.
        SampleExtractor extractor =
                dataSource == null
                        ? mMpegTsSampleExtractorFactory.create(mBufferManager, mBufferListener)
                        : mMpegTsSampleExtractorFactory.create(
                                dataSource, mBufferManager, mBufferListener);
        SampleSource sampleSource = new MpegTsSampleSource(extractor);
        MpegTsVideoTrackRenderer videoRenderer =
                new MpegTsVideoTrackRenderer(
                        mContext, sampleSource, mpegTsPlayer.getMainHandler(), mpegTsPlayer);
        // TODO: Only using MpegTsDefaultAudioTrackRenderer for A/V sync issue. We will use
        // {@link MpegTsMediaCodecAudioTrackRenderer} when we use ExoPlayer's extractor.
        TrackRenderer audioRenderer =
                new MpegTsDefaultAudioTrackRenderer(
                        sampleSource,
                        MediaCodecSelector.DEFAULT,
                        mpegTsPlayer.getMainHandler(),
                        mpegTsPlayer);
        Cea708TextTrackRenderer textRenderer = new Cea708TextTrackRenderer(sampleSource);

        TrackRenderer[] renderers = new TrackRenderer[MpegTsPlayer.RENDERER_COUNT];
        renderers[MpegTsPlayer.TRACK_TYPE_VIDEO] = videoRenderer;
        renderers[MpegTsPlayer.TRACK_TYPE_AUDIO] = audioRenderer;
        renderers[MpegTsPlayer.TRACK_TYPE_TEXT] = textRenderer;
        callback.onRenderers(null, renderers);
    }
}
