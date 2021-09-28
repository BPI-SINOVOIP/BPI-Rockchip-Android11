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
package com.android.tv.tuner.modules;

import com.android.tv.tuner.cc.CaptionTrackRenderer;
import com.android.tv.tuner.cc.CaptionTrackRendererFactory;
import com.android.tv.tuner.cc.CaptionWindowLayout;
import com.android.tv.tuner.cc.CaptionWindowLayoutFactory;
import com.android.tv.tuner.exoplayer.ExoPlayerSampleExtractor;
import com.android.tv.tuner.exoplayer.ExoPlayerSampleExtractorFactory;
import com.android.tv.tuner.exoplayer.FileSampleExtractor;
import com.android.tv.tuner.exoplayer.FileSampleExtractorFactory;
import com.android.tv.tuner.exoplayer.MpegTsRendererBuilder;
import com.android.tv.tuner.exoplayer.MpegTsRendererBuilderFactory;
import com.android.tv.tuner.exoplayer.MpegTsSampleExtractor;
import com.android.tv.tuner.exoplayer.MpegTsSampleExtractorFactory;
import com.android.tv.tuner.exoplayer.buffer.RecordingSampleBuffer;
import com.android.tv.tuner.exoplayer.buffer.RecordingSampleBufferFactory;
import com.android.tv.tuner.exoplayer.buffer.SampleChunkIoHelper;
import com.android.tv.tuner.exoplayer.buffer.SampleChunkIoHelperFactory;
import com.android.tv.tuner.source.TunerSourceModule;
import com.android.tv.tuner.tvinput.TunerRecordingSessionFactoryImpl;
import com.android.tv.tuner.tvinput.TunerRecordingSessionWorker;
import com.android.tv.tuner.tvinput.TunerRecordingSessionWorkerFactory;
import com.android.tv.tuner.tvinput.TunerSessionExoV2Factory;
import com.android.tv.tuner.tvinput.TunerSessionOverlay;
import com.android.tv.tuner.tvinput.TunerSessionOverlayFactory;
import com.android.tv.tuner.tvinput.TunerSessionV1Factory;
import com.android.tv.tuner.tvinput.TunerSessionWorker;
import com.android.tv.tuner.tvinput.TunerSessionWorkerExoV2;
import com.android.tv.tuner.tvinput.TunerSessionWorkerExoV2Factory;
import com.android.tv.tuner.tvinput.TunerSessionWorkerFactory;
import com.android.tv.tuner.tvinput.factory.TunerRecordingSessionFactory;
import com.android.tv.tuner.tvinput.factory.TunerSessionFactory;

import dagger.Binds;
import dagger.Module;
import dagger.Provides;

import com.android.tv.common.flags.TunerFlags;

/** Dagger module for TV Tuners. */
@Module(includes = {TunerSingletonsModule.class, TunerSourceModule.class})
public abstract class TunerModule {

    @Provides
    static TunerSessionFactory tunerSessionFactory(
            TunerFlags tunerFlags,
            TunerSessionV1Factory tunerSessionFactory,
            TunerSessionExoV2Factory tunerSessionExoV2Factory) {
        return tunerFlags.useExoplayerV2() ? tunerSessionExoV2Factory : tunerSessionFactory;
    }

    @Binds
    abstract TunerRecordingSessionWorker.Factory tunerRecordingSessionWorkerFactory(
            TunerRecordingSessionWorkerFactory tunerRecordingSessionWorkerFactory);

    @Binds
    abstract TunerSessionWorker.Factory tunerSessionWorkerFactory(
            TunerSessionWorkerFactory tunerSessionWorkerFactory);

    @Binds
    abstract TunerSessionOverlay.Factory tunerSessionOverlayFactory(
            TunerSessionOverlayFactory tunerSessionOverlayFactory);

    @Binds
    abstract CaptionTrackRenderer.Factory captionTrackRendererFactory(
            CaptionTrackRendererFactory captionTrackRendererFactory);

    @Binds
    abstract CaptionWindowLayout.Factory captionWindowLayoutFactory(
            CaptionWindowLayoutFactory captionWindowLayoutFactory);

    @Binds
    abstract TunerSessionWorkerExoV2.Factory tunerSessionWorkerExoV2Factory(
            TunerSessionWorkerExoV2Factory tunerSessionWorkerExoV2Factory);

    @Binds
    abstract TunerRecordingSessionFactory tunerRecordingSessionFactory(
            TunerRecordingSessionFactoryImpl impl);

    @Binds
    abstract MpegTsRendererBuilder.Factory mpegTsRendererBuilderFactory(
            MpegTsRendererBuilderFactory mpegTsRendererBuilderFactory);

    @Binds
    abstract MpegTsSampleExtractor.Factory mpegTsSampleExtractorFactory(
            MpegTsSampleExtractorFactory mpegTsSampleExtractorFactory);

    @Binds
    abstract FileSampleExtractor.Factory fileSampleExtractorFactory(
            FileSampleExtractorFactory fileSampleExtractorFactory);

    @Binds
    abstract RecordingSampleBuffer.Factory recordingSampleBufferFactory(
            RecordingSampleBufferFactory recordingSampleBufferFactory);

    @Binds
    abstract ExoPlayerSampleExtractor.Factory exoPlayerSampleExtractorFactory(
            ExoPlayerSampleExtractorFactory exoPlayerSampleExtractorFactory);

    @Binds
    abstract SampleChunkIoHelper.Factory sampleChunkIoHelperFactory(
            SampleChunkIoHelperFactory sampleChunkIoHelperFactory);
}
