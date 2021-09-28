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

import com.google.android.exoplayer2.decoder.DecoderInputBuffer;

import java.util.LinkedList;

/** Pool of samples to recycle ByteBuffers as much as possible. */
public class InputBufferPool {
    private final LinkedList<DecoderInputBuffer> mInputBufferPool = new LinkedList<>();

    /**
     * Acquires a sample with a buffer larger than size from the pool. Allocate new one or resize an
     * existing buffer if necessary.
     */
    public synchronized DecoderInputBuffer acquireSample(int size) {
        if (mInputBufferPool.isEmpty()) {
            DecoderInputBuffer sample =
                    new DecoderInputBuffer(DecoderInputBuffer.BUFFER_REPLACEMENT_MODE_NORMAL);
            sample.ensureSpaceForWrite(size);
            return sample;
        }
        DecoderInputBuffer smallestSufficientSample = null;
        DecoderInputBuffer maxSample = mInputBufferPool.getFirst();
        for (DecoderInputBuffer sample : mInputBufferPool) {
            // Grab the smallest sufficient sample.
            if (sample.data.capacity() >= size
                    && (smallestSufficientSample == null
                            || smallestSufficientSample.data.capacity() > sample.data.capacity())) {
                smallestSufficientSample = sample;
            }

            // Grab the max size sample.
            if (maxSample.data.capacity() < sample.data.capacity()) {
                maxSample = sample;
            }
        }
        DecoderInputBuffer sampleFromPool = smallestSufficientSample;

        // If there's no sufficient sample, grab the maximum sample and resize it to size.
        if (sampleFromPool == null) {
            sampleFromPool = maxSample;
            sampleFromPool.ensureSpaceForWrite(size);
        }
        mInputBufferPool.remove(sampleFromPool);
        return sampleFromPool;
    }

    /** Releases the sample back to the pool. */
    public synchronized void releaseSample(DecoderInputBuffer sample) {
        sample.clear();
        mInputBufferPool.offerLast(sample);
    }
}
