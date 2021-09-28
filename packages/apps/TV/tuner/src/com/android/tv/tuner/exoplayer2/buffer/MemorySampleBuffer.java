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

package com.android.tv.tuner.exoplayer2.buffer;

import android.os.ConditionVariable;
import android.support.annotation.NonNull;
import com.android.tv.common.SoftPreconditions;
import com.android.tv.tuner.exoplayer2.SampleExtractor;

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.decoder.DecoderInputBuffer;

import java.util.List;

/**
 * Handles I/O for {@link SampleExtractor} when Trickplay is disabled. Memory storage based buffer
 * is used instead of physical storage based buffer.
 */
public class MemorySampleBuffer implements BufferManager.SampleBuffer {
    private final InputBufferPool mInputBufferPool = new InputBufferPool();
    private SampleQueue[] mPlayingSampleQueues;

    private volatile boolean mEos;

    public MemorySampleBuffer(PlaybackBufferListener bufferListener) {
        if (bufferListener != null) {
            // Disables trickplay.
            bufferListener.onBufferStateChanged(false);
        }
    }

    @Override
    public synchronized void init(@NonNull List<String> ids, @NonNull List<Format> formats) {
        int trackCount = ids.size();
        mPlayingSampleQueues = new SampleQueue[trackCount];
        for (int i = 0; i < trackCount; i++) {
            mPlayingSampleQueues[i] = null;
        }
    }

    @Override
    public void setEos() {
        mEos = true;
    }

    private boolean reachedEos() {
        return mEos;
    }

    @Override
    public void selectTrack(int index) {
        synchronized (this) {
            if (mPlayingSampleQueues[index] == null) {
                mPlayingSampleQueues[index] = new SampleQueue(mInputBufferPool);
            } else {
                mPlayingSampleQueues[index].clear();
            }
        }
    }

    @Override
    public synchronized void deselectTrack(int index) {
        if (mPlayingSampleQueues[index] != null) {
            mPlayingSampleQueues[index].clear();
            mPlayingSampleQueues[index] = null;
        }
    }

    @Override
    public synchronized int readSample(int track, DecoderInputBuffer sampleHolder) {
        SampleQueue queue = mPlayingSampleQueues[track];
        SoftPreconditions.checkNotNull(queue);
        int result = queue == null ? C.RESULT_NOTHING_READ : queue.dequeueSample(sampleHolder);
        if (result != C.RESULT_BUFFER_READ && reachedEos()) {
            return C.RESULT_END_OF_INPUT;
        }
        return result;
    }

    @Override
    public void writeSample(
            int index, DecoderInputBuffer sample, ConditionVariable conditionVariable) {
        int size = sample.data.position();
        sample.data.position(0).limit(size);
        DecoderInputBuffer sampleToQueue = mInputBufferPool.acquireSample(size);
        sampleToQueue.data.clear();
        sampleToQueue.data.put(sample.data);
        sampleToQueue.timeUs = sample.timeUs;
        sampleToQueue.setFlags((sample.isKeyFrame() ? C.BUFFER_FLAG_KEY_FRAME : 0)
                | (sample.isDecodeOnly() ? C.BUFFER_FLAG_DECODE_ONLY : 0)
                | (sample.isEncrypted() ? C.BUFFER_FLAG_ENCRYPTED : 0));

        synchronized (this) {
            if (mPlayingSampleQueues[index] != null) {
                mPlayingSampleQueues[index].queueSample(sampleToQueue);
            }
        }
    }

    @Override
    public boolean isWriteSpeedSlow(int sampleSize, long durationNs) {
        // Since MemorySampleBuffer write samples only to memory (not to physical storage),
        // write speed is always fine.
        return false;
    }

    @Override
    public void handleWriteSpeedSlow() {
        // no-op
    }

    @Override
    public synchronized boolean continueLoading(long positionUs) {
        for (SampleQueue queue : mPlayingSampleQueues) {
            if (queue == null) {
                continue;
            }
            if (queue.getLastQueuedPositionUs() == null
                    || positionUs > queue.getLastQueuedPositionUs()) {
                // No more buffered data.
                return false;
            }
        }
        return true;
    }

    @Override
    public void seekTo(long positionUs) {
        // Not used.
    }

    @Override
    public void release() {
        // Not used.
    }
}
