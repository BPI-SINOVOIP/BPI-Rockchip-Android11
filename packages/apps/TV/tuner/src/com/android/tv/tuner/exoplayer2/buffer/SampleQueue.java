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

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.decoder.DecoderInputBuffer;

import java.util.LinkedList;

/** A sample queue which reads from the buffer and passes to player pipeline. */
public class SampleQueue {
    private final LinkedList<DecoderInputBuffer> mQueue = new LinkedList<>();
    private final InputBufferPool mInputBufferPool;
    private Long mLastQueuedPositionUs = null;

    public SampleQueue(InputBufferPool inputBufferPool) {
        mInputBufferPool = inputBufferPool;
    }

    public void queueSample(DecoderInputBuffer sample) {
        mQueue.offer(sample);
        mLastQueuedPositionUs = sample.timeUs;
    }

    public int dequeueSample(DecoderInputBuffer sample) {
        DecoderInputBuffer sampleFromQueue = mQueue.poll();
        if (sampleFromQueue == null) {
            return C.RESULT_NOTHING_READ;
        }
        int size = sampleFromQueue.data.position();
        sample.ensureSpaceForWrite(size);
        sample.setFlags((sampleFromQueue.isKeyFrame() ? C.BUFFER_FLAG_KEY_FRAME : 0)
                | (sampleFromQueue.isDecodeOnly() ? C.BUFFER_FLAG_DECODE_ONLY : 0)
                | (sampleFromQueue.isEncrypted() ? C.BUFFER_FLAG_ENCRYPTED : 0));
        sample.timeUs = sampleFromQueue.timeUs;
        sample.data.clear();
        sampleFromQueue.flip();
        sample.data.put(sampleFromQueue.data);
        mInputBufferPool.releaseSample(sampleFromQueue);
        return C.RESULT_BUFFER_READ;
    }

    public void clear() {
        while (!mQueue.isEmpty()) {
            mInputBufferPool.releaseSample(mQueue.poll());
        }
        mLastQueuedPositionUs = null;
    }

    public Long getLastQueuedPositionUs() {
        return mLastQueuedPositionUs;
    }

    public boolean isDurationGreaterThan(long durationUs) {
        return !mQueue.isEmpty() && mQueue.getLast().timeUs - mQueue.getFirst().timeUs > durationUs;
    }

    public boolean isEmpty() {
        return mQueue.isEmpty();
    }
}
