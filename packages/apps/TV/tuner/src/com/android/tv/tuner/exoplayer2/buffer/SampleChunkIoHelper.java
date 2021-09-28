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

package com.android.tv.tuner.exoplayer2.buffer;

import android.media.MediaFormat;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.util.ArraySet;
import android.util.Log;
import android.util.Pair;

import com.android.tv.common.SoftPreconditions;
import com.android.tv.tuner.exoplayer2.buffer.RecordingSampleBuffer.BufferReason;

import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.decoder.DecoderInputBuffer;
import com.google.android.exoplayer2.mediacodec.MediaFormatUtil;
import com.google.android.exoplayer2.util.MimeTypes;
import com.google.android.exoplayer2.util.Util;
import com.google.auto.factory.AutoFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Handles all {@link SampleChunk} I/O operations. An I/O dedicated thread handles all I/O
 * operations for synchronization.
 */
public class SampleChunkIoHelper implements Handler.Callback {
    private static final String TAG = "SampleChunkIoHelper";

    private static final int MAX_READ_BUFFER_SAMPLES = 3;
    private static final int READ_RESCHEDULING_DELAY_MS = 10;

    private static final int MSG_OPEN_READ = 1;
    private static final int MSG_OPEN_WRITE = 2;
    private static final int MSG_CLOSE_READ = 3;
    private static final int MSG_CLOSE_WRITE = 4;
    private static final int MSG_READ = 5;
    private static final int MSG_WRITE = 6;
    private static final int MSG_RELEASE = 7;
    private static final int MSG_UPDATE_INDEX = 8;

    private final long mSampleChunkDurationUs;
    private final int mTrackCount;
    private final List<String> mIds;
    private final List<Format> mFormats;
    private final @BufferReason int mBufferReason;
    private final BufferManager mBufferManager;
    private final InputBufferPool mInputBufferPool;
    private final IoCallback mIoCallback;

    private Handler mIoHandler;
    private final ConcurrentLinkedQueue<DecoderInputBuffer>[] mReadSampleBuffers;
    private final ConcurrentLinkedQueue<DecoderInputBuffer>[] mHandlerReadSampleBuffers;
    private final long[] mWriteIndexEndPositionUs;
    private final long[] mWriteChunkEndPositionUs;
    private final SampleChunk.IoState[] mReadIoStates;
    private final SampleChunk.IoState[] mWriteIoStates;
    private final Set<Integer> mSelectedTracks = new ArraySet<>();
    private final long[] mReadChunkOffset;
    private final long[] mReadChunkPositionUs;
    private long mBufferDurationUs = 0;
    private boolean mWriteEnded;
    private boolean mErrorNotified;
    private boolean mFinished;

    /** A Callback for I/O events. */
    public abstract static class IoCallback {

        /** Called when there is no sample to read. */
        public void onIoReachedEos() {}

        /** Called when there is an irrecoverable error during I/O. */
        public void onIoError() {}
    }

    private static class IoParams {
        private final int index;
        private final long positionUs;
        private final DecoderInputBuffer sample;
        private final ConditionVariable conditionVariable;
        private final ConcurrentLinkedQueue<DecoderInputBuffer> readSampleBuffer;

        private IoParams(
                int index,
                long positionUs,
                DecoderInputBuffer sample,
                ConditionVariable conditionVariable,
                ConcurrentLinkedQueue<DecoderInputBuffer> readSampleBuffer) {
            this.index = index;
            this.positionUs = positionUs;
            this.sample = sample;
            this.conditionVariable = conditionVariable;
            this.readSampleBuffer = readSampleBuffer;
        }
    }

    /**
     * Factory for {@link SampleChunkIoHelper}.
     *
     * <p>This wrapper class keeps other classes from needing to reference the {@link AutoFactory}
     * generated class.
     */
    public interface Factory {
        SampleChunkIoHelper create(
                List<String> ids,
                List<Format> formats,
                @BufferReason int bufferReason,
                BufferManager bufferManager,
                InputBufferPool inputBufferPool,
                IoCallback ioCallback);
    }

    /**
     * Creates {@link SampleChunk} I/O handler.
     *
     * @param ids track names
     * @param formats {@link Format} for each track
     * @param bufferReason reason to be buffered
     * @param bufferManager manager of {@link SampleChunk} collections
     * @param inputBufferPool allocator for a sample
     * @param ioCallback listeners for I/O events
     */
    @AutoFactory(implementing = Factory.class)
    public SampleChunkIoHelper(
            List<String> ids,
            List<Format> formats,
            @BufferReason int bufferReason,
            BufferManager bufferManager,
            InputBufferPool inputBufferPool,
            IoCallback ioCallback) {
        mTrackCount = ids.size();
        mIds = ids;
        mFormats = formats;
        mBufferReason = bufferReason;
        mBufferManager = bufferManager;
        mInputBufferPool = inputBufferPool;
        mIoCallback = ioCallback;

        mReadSampleBuffers = new ConcurrentLinkedQueue[mTrackCount];
        mHandlerReadSampleBuffers = new ConcurrentLinkedQueue[mTrackCount];
        mWriteIndexEndPositionUs = new long[mTrackCount];
        mWriteChunkEndPositionUs = new long[mTrackCount];
        mReadChunkOffset = new long[mTrackCount];
        mReadChunkPositionUs = new long[mTrackCount];
        mReadIoStates = new SampleChunk.IoState[mTrackCount];
        mWriteIoStates = new SampleChunk.IoState[mTrackCount];

        // Small chunk duration for live playback will give more fine grained storage usage
        // and eviction handling for trickplay.
        mSampleChunkDurationUs =
                bufferReason == RecordingSampleBuffer.BUFFER_REASON_LIVE_PLAYBACK
                        ? RecordingSampleBuffer.MIN_SEEK_DURATION_US
                        : RecordingSampleBuffer.RECORDING_CHUNK_DURATION_US;
        for (int i = 0; i < mTrackCount; ++i) {
            mWriteIndexEndPositionUs[i] = RecordingSampleBuffer.MIN_SEEK_DURATION_US;
            mWriteChunkEndPositionUs[i] = mSampleChunkDurationUs;
            mReadIoStates[i] = new SampleChunk.IoState();
            mWriteIoStates[i] = new SampleChunk.IoState();
        }
    }

    /**
     * Prepares and initializes for I/O operations.
     *
     * @throws IOException if an I/O error occurs.
     */
    public void init() throws IOException {
        HandlerThread handlerThread = new HandlerThread(TAG);
        handlerThread.start();
        mIoHandler = new Handler(handlerThread.getLooper(), this);
        if (mBufferReason == RecordingSampleBuffer.BUFFER_REASON_RECORDED_PLAYBACK) {
            for (int i = 0; i < mTrackCount; ++i) {
                mBufferManager.loadTrackFromStorage(mIds.get(i), mInputBufferPool);
            }
            mWriteEnded = true;
        } else {
            for (int i = 0; i < mTrackCount; ++i) {
                mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_OPEN_WRITE, i));
            }
        }

        try {
            if (mBufferReason == RecordingSampleBuffer.BUFFER_REASON_RECORDING && mTrackCount > 0) {
                // Saves meta information for recording.
                List<BufferManager.TrackFormat> audios = new ArrayList<>(mTrackCount);
                List<BufferManager.TrackFormat> videos = new ArrayList<>(mTrackCount);
                for (int i = 0; i < mTrackCount; ++i) {
                    if (MimeTypes.isAudio(mFormats.get(i).sampleMimeType)) {
                        MediaFormat mediaFormat = getAudioMediaFormat(mFormats.get(i));
                        mediaFormat.setLong(MediaFormat.KEY_DURATION, mBufferDurationUs);
                        audios.add(new BufferManager.TrackFormat(mIds.get(i), mediaFormat));
                    } else if (MimeTypes.isVideo(mFormats.get(i).sampleMimeType)) {
                        MediaFormat mediaFormat = getVideoMediaFormat(mFormats.get(i));
                        mediaFormat.setLong(MediaFormat.KEY_DURATION, mBufferDurationUs);
                        if (mFormats.get(i).pixelWidthHeightRatio != Format.NO_VALUE) {
                            // MediaFormats doesn't store aspect ratio so updating the width
                            // to maintain aspect ratio.
                            mediaFormat.setInteger(
                                    MediaFormat.KEY_WIDTH,
                                    (int)
                                            (mFormats.get(i).width
                                                    * mFormats.get(i).pixelWidthHeightRatio));
                        }
                        videos.add(new BufferManager.TrackFormat(mIds.get(i), mediaFormat));
                    }
                }
                mBufferManager.writeMetaFilesOnly(audios, videos);
            }
        } catch (Exception e) {
            Log.e(TAG, "Unable to write Meta files for DVR recording.", e);
        }
    }

    private MediaFormat getVideoMediaFormat(Format format) {
        MediaFormat mediaFormat = new MediaFormat();
        // Set mediaFormat parameters that should always be set.
        mediaFormat.setString(MediaFormat.KEY_MIME, format.sampleMimeType);
        mediaFormat.setInteger(MediaFormat.KEY_WIDTH, format.width);
        mediaFormat.setInteger(MediaFormat.KEY_HEIGHT, format.height);
        MediaFormatUtil.setCsdBuffers(mediaFormat, format.initializationData);
        // Set mediaFormat parameters that may be unset.
        MediaFormatUtil.maybeSetFloat(mediaFormat, MediaFormat.KEY_FRAME_RATE, format.frameRate);
        MediaFormatUtil.maybeSetInteger(
                mediaFormat, MediaFormat.KEY_ROTATION, format.rotationDegrees);
        MediaFormatUtil.maybeSetColorInfo(mediaFormat, format.colorInfo);
        MediaFormatUtil.maybeSetInteger(
                mediaFormat, MediaFormat.KEY_MAX_INPUT_SIZE, format.maxInputSize);
        if (Util.SDK_INT >= 23) {
            mediaFormat.setInteger(MediaFormat.KEY_PRIORITY, 0 /* realtime priority */);
        }
        return mediaFormat;
    }

    private MediaFormat getAudioMediaFormat(Format format) {
        MediaFormat mediaFormat = new MediaFormat();
        // Set mediaFormat parameters that should always be set.
        mediaFormat.setString(MediaFormat.KEY_MIME, format.sampleMimeType);
        mediaFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, format.channelCount);
        mediaFormat.setInteger(MediaFormat.KEY_SAMPLE_RATE, format.sampleRate);
        // Set mediaFormat parameters that may be unset.
        MediaFormatUtil.setCsdBuffers(mediaFormat, format.initializationData);
        MediaFormatUtil.maybeSetInteger(
                mediaFormat, MediaFormat.KEY_MAX_INPUT_SIZE, format.maxInputSize);
        if (Util.SDK_INT >= 23) {
            mediaFormat.setInteger(MediaFormat.KEY_PRIORITY, 0 /* realtime priority */);
        }
        return mediaFormat;
    }

    /**
     * Reads a sample if it is available.
     *
     * @param index track index
     * @return {@code null} if a sample is not available, otherwise returns a sample
     */
    public DecoderInputBuffer readSample(int index) {
        DecoderInputBuffer sample = mReadSampleBuffers[index].poll();
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_READ, index));
        return sample;
    }

    /**
     * Writes a sample.
     *
     * @param index track index
     * @param sample to write
     * @param conditionVariable which will be wait until the write is finished
     * @throws IOException if an I/O error occurs.
     */
    public void writeSample(
            int index,
            DecoderInputBuffer sample,
            ConditionVariable conditionVariable) throws IOException {
        if (mErrorNotified) {
            throw new IOException("Storage I/O error happened");
        }
        conditionVariable.close();
        IoParams params = new IoParams(index, 0, sample, conditionVariable, null);
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_WRITE, params));
    }

    /**
     * Starts read from the specified position.
     *
     * @param index track index
     * @param positionUs the specified position
     */
    public void openRead(int index, long positionUs) {
        // Old mReadSampleBuffers may have a pending read.
        mReadSampleBuffers[index] = new ConcurrentLinkedQueue<>();
        IoParams params = new IoParams(index, positionUs, null, null, mReadSampleBuffers[index]);
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_OPEN_READ, params));
    }

    /**
     * Update Index from the specified offset.
     *
     * @param index track index
     * @param offset of the specified position
     */
    private void updateIndex(int index, long offset) {
        IoParams params =
                new IoParams(index, offset, null, null, null); // mReadSampleBuffers[index]);
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_UPDATE_INDEX, params));
    }

    /**
     * Closes read from the specified track.
     *
     * @param index track index
     */
    public void closeRead(int index) {
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_CLOSE_READ, index));
    }

    /** Notifies writes are finished. */
    public void closeWrite() {
        mIoHandler.sendEmptyMessage(MSG_CLOSE_WRITE);
    }

    /**
     * Finishes I/O operations and releases all the resources.
     *
     * @throws IOException if an I/O error occurs.
     */
    public void release() throws IOException {
        if (mIoHandler == null) {
            return;
        }
        // Finishes all I/O operations.
        ConditionVariable conditionVariable = new ConditionVariable();
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_RELEASE, conditionVariable));
        conditionVariable.block();

        for (int i = 0; i < mTrackCount; ++i) {
            mBufferManager.unregisterChunkEvictedListener(mIds.get(i));
        }
        try {
            if (mBufferReason == RecordingSampleBuffer.BUFFER_REASON_RECORDING && mTrackCount > 0) {
                // Saves meta information for recording.
                List<BufferManager.TrackFormat> audios = new LinkedList<>();
                List<BufferManager.TrackFormat> videos = new LinkedList<>();
                for (int i = 0; i < mTrackCount; ++i) {
                    if (MimeTypes.isAudio(mFormats.get(i).sampleMimeType)) {
                        MediaFormat mediaFormat = getAudioMediaFormat(mFormats.get(i));
                        mediaFormat.setLong(MediaFormat.KEY_DURATION, mBufferDurationUs);
                        audios.add(new BufferManager.TrackFormat(mIds.get(i), mediaFormat));
                    } else if (MimeTypes.isVideo(mFormats.get(i).sampleMimeType)) {
                        MediaFormat mediaFormat = getVideoMediaFormat(mFormats.get(i));
                        mediaFormat.setLong(MediaFormat.KEY_DURATION, mBufferDurationUs);
                        if (mFormats.get(i).pixelWidthHeightRatio != Format.NO_VALUE) {
                            // MediaFormats doesn't store aspect ratio so updating the width
                            // to maintain aspect ratio.
                            mediaFormat.setInteger(
                                    MediaFormat.KEY_WIDTH,
                                    (int)
                                            (mFormats.get(i).width
                                                    * mFormats.get(i).pixelWidthHeightRatio));
                        }
                        videos.add(new BufferManager.TrackFormat(mIds.get(i), mediaFormat));
                    }
                }
                mBufferManager.writeMetaFiles(audios, videos);
            }
        } finally {
            mBufferManager.release();
            mIoHandler.getLooper().quitSafely();
        }
    }

    @Override
    public boolean handleMessage(Message message) {
        if (mFinished) {
            return true;
        }
        releaseEvictedChunks();
        try {
            switch (message.what) {
                case MSG_OPEN_READ:
                    doOpenRead((IoParams) message.obj);
                    return true;
                case MSG_OPEN_WRITE:
                    doOpenWrite((int) message.obj);
                    return true;
                case MSG_CLOSE_READ:
                    doCloseRead((int) message.obj);
                    return true;
                case MSG_CLOSE_WRITE:
                    doCloseWrite();
                    return true;
                case MSG_READ:
                    doRead((int) message.obj);
                    return true;
                case MSG_WRITE:
                    doWrite((IoParams) message.obj);
                    // Since only write will increase storage, eviction will be handled here.
                    return true;
                case MSG_RELEASE:
                    doRelease((ConditionVariable) message.obj);
                    return true;
                case MSG_UPDATE_INDEX:
                    doUpdateIndex((IoParams) message.obj);
                    return true;
            }
        } catch (IOException e) {
            mIoCallback.onIoError();
            mErrorNotified = true;
            Log.e(TAG, "IoException happened", e);
            return true;
        }
        return false;
    }

    private void doOpenRead(IoParams params) throws IOException {
        int index = params.index;
        mIoHandler.removeMessages(MSG_READ, index);
        Pair<SampleChunk, Integer> readPosition =
                mBufferManager.getReadFile(mIds.get(index), params.positionUs);
        if (readPosition == null) {
            String errorMessage =
                    "Chunk ID:" + mIds.get(index) + " pos:" + params.positionUs + "is not found";
            SoftPreconditions.checkNotNull(readPosition, TAG, errorMessage);
            throw new IOException(errorMessage);
        }
        mSelectedTracks.add(index);
        mReadIoStates[index].openRead(readPosition.first, (long) readPosition.second);
        if (mHandlerReadSampleBuffers[index] != null) {
            DecoderInputBuffer sample;
            while ((sample = mHandlerReadSampleBuffers[index].poll()) != null) {
                mInputBufferPool.releaseSample(sample);
            }
        }
        mHandlerReadSampleBuffers[index] = params.readSampleBuffer;
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_READ, index));
    }

    private void doOpenWrite(int index) throws IOException {
        boolean updateIndexFile =
                (mBufferReason == RecordingSampleBuffer.BUFFER_REASON_RECORDING)
                        && (MimeTypes.isVideo(mFormats.get(index).sampleMimeType)
                                || MimeTypes.isAudio(mFormats.get(index).sampleMimeType));

        SampleChunk chunk =
                mBufferManager.createNewWriteFileIfNeeded(
                        mIds.get(index), 0, mInputBufferPool, null, 0, updateIndexFile);
        mWriteIoStates[index].openWrite(chunk);
    }

    private void doCloseRead(int index) {
        mSelectedTracks.remove(index);
        if (mHandlerReadSampleBuffers[index] != null) {
            DecoderInputBuffer sample;
            while ((sample = mHandlerReadSampleBuffers[index].poll()) != null) {
                mInputBufferPool.releaseSample(sample);
            }
        }
        mIoHandler.removeMessages(MSG_READ, index);
    }

    private void doRead(int index) throws IOException {
        mIoHandler.removeMessages(MSG_READ, index);
        if (mHandlerReadSampleBuffers[index].size() >= MAX_READ_BUFFER_SAMPLES) {
            // If enough samples are buffered, try again few moments later hoping that
            // buffered samples are consumed.
            mIoHandler.sendMessageDelayed(
                    mIoHandler.obtainMessage(MSG_READ, index), READ_RESCHEDULING_DELAY_MS);
        } else {
            if (mReadIoStates[index].isReadFinished()) {
                for (int i = 0; i < mTrackCount; ++i) {
                    if (!mReadIoStates[i].isReadFinished()) {
                        return;
                    }
                }
                mIoCallback.onIoReachedEos();
                return;
            }
            DecoderInputBuffer sample = mReadIoStates[index].read();
            if (sample != null) {
                mHandlerReadSampleBuffers[index].offer(sample);
                mReadChunkOffset[index] = mReadIoStates[index].getOffset();
                mReadChunkPositionUs[index] = sample.timeUs;
            } else {
                if (mBufferReason == RecordingSampleBuffer.BUFFER_REASON_RECORDED_PLAYBACK) {
                    // Update Index, to load new Samples
                    updateIndex(index, mReadChunkOffset[index]);
                }
                // Read reached write but write is not finished yet --- wait a few moments to
                // see if another sample is written.
                mIoHandler.sendMessageDelayed(
                        mIoHandler.obtainMessage(MSG_READ, index), READ_RESCHEDULING_DELAY_MS);
            }
        }
    }

    private void doUpdateIndex(IoParams params) throws IOException {
        int index = params.index;
        mIoHandler.removeMessages(MSG_READ, index);
        // Update Track from Storage to load new Samples
        mBufferManager.loadTrackFromStorage(mIds.get(index), mInputBufferPool);
        Pair<SampleChunk, Integer> readPosition =
                mBufferManager.getReadFile(mIds.get(index), mReadChunkPositionUs[index]);
        if (readPosition == null) {
            String errorMessage =
                    "Chunk ID:"
                            + mIds.get(index)
                            + " pos:"
                            + mReadChunkPositionUs[index]
                            + "is not found";
            SoftPreconditions.checkNotNull(readPosition, TAG, errorMessage);
            throw new IOException(errorMessage);
        }
        mReadIoStates[index].openRead(readPosition.first, params.positionUs);
        mIoHandler.sendMessage(mIoHandler.obtainMessage(MSG_READ, index));
    }

    private void doWrite(IoParams params) throws IOException {
        try {
            if (mWriteEnded) {
                SoftPreconditions.checkState(false);
                return;
            }
            int index = params.index;
            DecoderInputBuffer sample = params.sample;
            SampleChunk nextChunk = null;
            if (sample.isKeyFrame()) {
                if (sample.timeUs > mBufferDurationUs) {
                    mBufferDurationUs = sample.timeUs;
                }
                if (sample.timeUs >= mWriteIndexEndPositionUs[index]) {
                    SampleChunk currentChunk =
                            sample.timeUs >= mWriteChunkEndPositionUs[index]
                                    ? null
                                    : mWriteIoStates[params.index].getChunk();
                    int currentOffset = (int) mWriteIoStates[params.index].getOffset();
                    boolean updateIndexFile =
                            (mBufferReason == RecordingSampleBuffer.BUFFER_REASON_RECORDING)
                                    && (MimeTypes.isVideo(mFormats.get(index).sampleMimeType)
                                            || MimeTypes.isAudio(
                                                    mFormats.get(index).sampleMimeType));

                    nextChunk =
                            mBufferManager.createNewWriteFileIfNeeded(
                                    mIds.get(index),
                                    mWriteIndexEndPositionUs[index],
                                    mInputBufferPool,
                                    currentChunk,
                                    currentOffset,
                                    updateIndexFile);
                    mWriteIndexEndPositionUs[index] =
                            ((sample.timeUs / RecordingSampleBuffer.MIN_SEEK_DURATION_US) + 1)
                                    * RecordingSampleBuffer.MIN_SEEK_DURATION_US;
                    if (nextChunk != null) {
                        mWriteChunkEndPositionUs[index] =
                                ((sample.timeUs / mSampleChunkDurationUs) + 1)
                                        * mSampleChunkDurationUs;
                    }
                }
            }
            mWriteIoStates[params.index].write(params.sample, nextChunk);
        } finally {
            params.conditionVariable.open();
        }
    }

    private void doCloseWrite() throws IOException {
        if (mWriteEnded) {
            return;
        }
        mWriteEnded = true;
        boolean readFinished = true;
        for (int i = 0; i < mTrackCount; ++i) {
            readFinished = readFinished && mReadIoStates[i].isReadFinished();
            mWriteIoStates[i].closeWrite();
        }
        if (readFinished) {
            mIoCallback.onIoReachedEos();
        }
    }

    private void doRelease(ConditionVariable conditionVariable) {
        mIoHandler.removeCallbacksAndMessages(null);
        mFinished = true;
        conditionVariable.open();
        mSelectedTracks.clear();
    }

    private void releaseEvictedChunks() {
        if (mBufferReason != RecordingSampleBuffer.BUFFER_REASON_LIVE_PLAYBACK
                || mSelectedTracks.isEmpty()) {
            return;
        }
        long currentStartPositionUs = Long.MAX_VALUE;
        for (int trackIndex : mSelectedTracks) {
            currentStartPositionUs =
                    Math.min(
                            currentStartPositionUs, mReadIoStates[trackIndex].getStartPositionUs());
        }
        for (int i = 0; i < mTrackCount; ++i) {
            long evictEndPositionUs =
                    Math.min(
                            mBufferManager.getStartPositionUs(mIds.get(i)), currentStartPositionUs);
            mBufferManager.evictChunks(mIds.get(i), evictEndPositionUs);
        }
    }
}
