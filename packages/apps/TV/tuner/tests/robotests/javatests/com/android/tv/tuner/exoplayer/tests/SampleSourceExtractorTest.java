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
package com.android.tv.tuner.exoplayer.tests;

import static com.google.common.truth.Truth.assertWithMessage;

import static junit.framework.Assert.fail;

import android.content.Context;
import android.net.Uri;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Pair;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.tuner.exoplayer.ExoPlayerSampleExtractor;
import com.android.tv.tuner.exoplayer.buffer.BufferManager;
import com.android.tv.tuner.exoplayer.buffer.BufferManager.StorageManager;
import com.android.tv.tuner.exoplayer.buffer.PlaybackBufferListener;
import com.android.tv.tuner.exoplayer.buffer.SampleChunk;
import com.android.tv.tuner.testing.buffer.VerySlowSampleChunk;

import com.google.android.exoplayer.MediaFormat;
import com.google.android.exoplayer.SampleHolder;
import com.google.android.exoplayer.SampleSource;
import com.google.android.exoplayer2.upstream.DataSource;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.SortedMap;

/** Tests for {@link ExoPlayerSampleExtractor} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class SampleSourceExtractorTest {
    // Maximum bandwidth of 1080p channel is about 2.2MB/s. 2MB for a sample will suffice.
    private static final int SAMPLE_BUFFER_SIZE = 1024 * 1024 * 2;
    private static final int CONSUMING_SAMPLES_PERIOD = 100;
    private Uri testStreamUri;
    private HandlerThread handlerThread;
    private DataSource dataSource;

    @Before
    public void setUp() {
        testStreamUri = Uri.parse("asset:///capture_stream.ts");
        handlerThread = new HandlerThread("test");
        dataSource = new AssetDataSource(RuntimeEnvironment.application);
    }

    @Test
    public void testTrickplayDisabled() throws Throwable {
        DataSource source = new AssetDataSource(RuntimeEnvironment.application);
        MockPlaybackBufferListener listener = new MockPlaybackBufferListener();
        ExoPlayerSampleExtractor extractor =
                new ExoPlayerSampleExtractor(
                        testStreamUri,
                        source,
                        null,
                        listener,
                        false,
                        Looper.getMainLooper(),
                        handlerThread,
                        (bufferManager, bufferListener, enableTrickplay, bufferReason) -> null);
        assertWithMessage("Trickplay should be disabled").that(listener.getLastState()).isFalse();
        // Prepares the extractor.
        extractor.prepare();
        // Looper is nat available until prepare is called at least once
        Looper handlerLooper = handlerThread.getLooper();
        try {
            while (!extractor.prepare()) {

                ShadowLooper.getShadowMainLooper().runOneTask();
                Shadows.shadowOf(handlerLooper).runOneTask();
            }
        } catch (IOException e) {
            fail("Exception occurred while preparing: " + e.getMessage());
        }
        // Selects all tracks.
        List<MediaFormat> trackFormats = extractor.getTrackFormats();
        for (int i = 0; i < trackFormats.size(); ++i) {
            extractor.selectTrack(i);
        }
        // Consumes over some period.
        SampleHolder sampleHolder = new SampleHolder(SampleHolder.BUFFER_REPLACEMENT_MODE_NORMAL);
        sampleHolder.ensureSpaceForWrite(SAMPLE_BUFFER_SIZE);

        Shadows.shadowOf(handlerLooper).idle();
        for (int i = 0; i < CONSUMING_SAMPLES_PERIOD; ++i) {
            boolean found = false;
            while (!found) {
                for (int j = 0; j < trackFormats.size(); ++j) {
                    int result = extractor.readSample(j, sampleHolder);
                    switch (result) {
                        case SampleSource.SAMPLE_READ:
                            found = true;
                            break;
                        case SampleSource.END_OF_STREAM:
                            fail("Failed to read samples");
                            break;
                        default:
                    }
                    if (found) {
                        break;
                    }
                }
                Shadows.shadowOf(handlerLooper).runOneTask();
                ShadowLooper.getShadowMainLooper().runOneTask();
            }
        }
        extractor.release();
    }

    @Ignore("b/70338667")
    @Test
    public void testDiskTooSlowTrickplayDisabled() throws Throwable {
        StorageManager storageManager = new StubStorageManager(RuntimeEnvironment.application);
        BufferManager bufferManager =
                new BufferManager(
                        storageManager, new VerySlowSampleChunk.VerySlowSampleChunkCreator());
        bufferManager.setMinimumSampleSizeForSpeedCheck(0);
        MockPlaybackBufferListener listener = new MockPlaybackBufferListener();
        ExoPlayerSampleExtractor extractor =
                new ExoPlayerSampleExtractor(
                        testStreamUri,
                        dataSource,
                        bufferManager,
                        listener,
                        false,
                        Looper.getMainLooper(),
                        handlerThread,
                        (bufferManager2, bufferListener, enableTrickplay, bufferReason) -> null);

        assertWithMessage("Trickplay should be enabled at the first")
                .that(Boolean.TRUE)
                .isEqualTo(listener.getLastState());
        // Prepares the extractor.
        extractor.prepare();
        // Looper is nat available until prepare is called at least once
        Looper handlerLooper = handlerThread.getLooper();
        try {
            while (!extractor.prepare()) {

                ShadowLooper.getShadowMainLooper().runOneTask();
                Shadows.shadowOf(handlerLooper).runOneTask();
            }
        } catch (IOException e) {
            fail("Exception occurred while preparing: " + e.getMessage());
        }
        // Selects all tracks.
        List<MediaFormat> trackFormats = extractor.getTrackFormats();
        for (int i = 0; i < trackFormats.size(); ++i) {
            extractor.selectTrack(i);
        }
        // Consumes until once speed check is done.
        SampleHolder sampleHolder = new SampleHolder(SampleHolder.BUFFER_REPLACEMENT_MODE_NORMAL);
        sampleHolder.ensureSpaceForWrite(SAMPLE_BUFFER_SIZE);
        while (!bufferManager.hasSpeedCheckDone()) {
            boolean found = false;
            while (!found) {
                for (int j = 0; j < trackFormats.size(); ++j) {
                    int result = extractor.readSample(j, sampleHolder);
                    switch (result) {
                        case SampleSource.SAMPLE_READ:
                            found = true;
                            break;
                        case SampleSource.END_OF_STREAM:
                            fail("Failed to read samples");
                            break;
                        default:
                    }
                    if (found) {
                        break;
                    }
                }
                ShadowLooper.getShadowMainLooper().runOneTask();
                Shadows.shadowOf(handlerLooper).runOneTask();
            }
        }
        extractor.release();
        ShadowLooper.getShadowMainLooper().idle();
        Shadows.shadowOf(handlerLooper).idle();
        assertWithMessage("Disk too slow event should be reported")
                .that(listener.isReportedDiskTooSlow())
                .isTrue();
    }

    private static class StubStorageManager implements StorageManager {
        private final Context mContext;

        StubStorageManager(Context context) {
            mContext = context;
        }

        @Override
        public File getBufferDir() {
            return mContext.getCacheDir();
        }

        @Override
        public boolean isPersistent() {
            return false;
        }

        @Override
        public boolean reachedStorageMax(long bufferSize, long pendingDelete) {
            return false;
        }

        @Override
        public boolean hasEnoughBuffer(long pendingDelete) {
            return true;
        }

        @Override
        public List<BufferManager.TrackFormat> readTrackInfoFiles(boolean isAudio) {
            return null;
        }

        @Override
        public ArrayList<BufferManager.PositionHolder> readIndexFile(String trackId)
                throws IOException {
            return null;
        }

        @Override
        public void writeTrackInfoFiles(List<BufferManager.TrackFormat> formatList, boolean isAudio)
                throws IOException {
            // No-op.
        }

        @Override
        public void writeIndexFile(
                String trackName, SortedMap<Long, Pair<SampleChunk, Integer>> index)
                throws IOException {
            // No-op.
        }

        @Override
        public void updateIndexFile(
                String trackName, int size, long position, SampleChunk sampleChunk, int offset)
                throws IOException {
            // No-op
        }
    }

    public static class MockPlaybackBufferListener implements PlaybackBufferListener {
        private Boolean mLastState;
        private boolean mIsReportedDiskTooSlow;

        public Boolean getLastState() {
            return mLastState;
        }

        public boolean isReportedDiskTooSlow() {
            return mIsReportedDiskTooSlow;
        }
        // PlaybackBufferListener
        @Override
        public void onBufferStartTimeChanged(long startTimeMs) {
            // No-op.
        }

        @Override
        public void onBufferStateChanged(boolean available) {
            mLastState = available;
        }

        @Override
        public void onDiskTooSlow() {
            mIsReportedDiskTooSlow = true;
        }
    }
}
