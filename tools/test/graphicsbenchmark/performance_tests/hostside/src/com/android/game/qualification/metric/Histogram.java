/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.metric;

import com.android.annotations.VisibleForTesting;

import com.google.common.collect.ImmutableSet;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collection;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.SortedMap;
import java.util.TreeMap;

import javax.annotation.Nullable;

/**
 * Create a histogram from a set of data.
 */
public class Histogram {
    private Long mBucketSize;
    private SortedMap<Long, Integer> mCounts = new TreeMap<>(); // need sorted keys for plotting.
    @Nullable private Long mMinCutoff;
    @Nullable private Long mMaxCutoff;

    /**
     * Create a Histogram
     *
     * @param data data to be graphed.
     * @param bucketSize Size of each bucket.  The first bucket will have a range of
     *                  [-bucketSize / 2, bucketSize/2).
     * @param minCutoff All data value < minCutoff will be categorize into the same bucket.
     * @param maxCutoff All data value > maxCutoff will be categorize into the same bucket.
     */
    public Histogram(
            Collection<Long> data,
            Long bucketSize,
            @Nullable Long minCutoff,
            @Nullable Long maxCutoff) {
        mBucketSize = bucketSize;
        mMinCutoff = minCutoff;
        mMaxCutoff = maxCutoff;

        for (Long value : data) {
            long bucket = findBucket(value);
            mCounts.put(bucket, mCounts.getOrDefault(bucket, 0) + 1);
        }

        // Add some buckets for padding to clarify the range of value represented by each bar of the
        // histogram.
        Set<Long> keys = ImmutableSet.copyOf(mCounts.keySet());
        for (long bucket : keys) {
            if (bucket == Long.MIN_VALUE) {
                mCounts.putIfAbsent(mMinCutoff, 0);
            } else if (mMaxCutoff != null && bucket > mMaxCutoff - bucketSize){
                mCounts.putIfAbsent(Long.MAX_VALUE, 0);
            } else {
                mCounts.putIfAbsent(bucket + bucketSize, 0);
            }
        }
    }

    @VisibleForTesting
    SortedMap<Long, Integer> getCounts() {
        return mCounts;
    }

    /**
     * Plot the histogram as text.
     *
     * Use '=' to represent the count of each bucket.
     *
     * @param output OutputStream to print the histogram to.
     * @param maxBarLength Maximum number of '=' to be printed for each bar.  If a bucket contains
     *                     a larger count than maxBarLength, all the other bars are scaled
     *                     accordingly.
     */
    public void plotAscii(OutputStream output, int maxBarLength) throws IOException {
        if (mCounts.isEmpty()) {
            return;
        }
        int maxCount = 0;
        int total = 0;
        for (int count : mCounts.values()) {
            if (count > maxCount) {
                maxCount = count;
            }
            total += count;
        }
        int cumulative = 0;
        int maxKeyLength =
                (int) Math.log10(
                        mCounts.lastKey() == Long.MAX_VALUE ? mMaxCutoff : mCounts.lastKey()) + 1;
        for (Map.Entry<Long, Integer> entry : mCounts.entrySet()) {
            cumulative += entry.getValue();
            long key = entry.getKey();
            if (mMinCutoff != null && Long.MIN_VALUE == key) {
                output.write("<".getBytes(StandardCharsets.UTF_8));
                key = mMinCutoff;
            } else if (mMaxCutoff != null && Long.MAX_VALUE == key) {
                output.write(">".getBytes(StandardCharsets.UTF_8));
                key = mMaxCutoff;
            } else {
                output.write(" ".getBytes(StandardCharsets.UTF_8));
            }
            float percentage = entry.getValue() * 100f / total;
            int barLength =
                    (maxCount > maxBarLength)
                            ? (entry.getValue() * maxBarLength + maxCount - 1) / maxCount
                            : entry.getValue();
            String bar = String.join("", Collections.nCopies(barLength, "="));
            output.write(
                    String.format(
                            "%" + maxKeyLength + "d| %-" + maxBarLength + "s (%d = %.1f%%) {%.1f%%}\n",
                            key,
                            bar,
                            entry.getValue(),
                            percentage,
                            cumulative * 100f / total) .getBytes());
        }
    }

    // Returns the lowest value of the bucket for the specified value.
    private Long findBucket(Long value) {
        if (mMinCutoff != null && value < mMinCutoff) {
            return Long.MIN_VALUE;
        }
        if (mMaxCutoff != null && value > mMaxCutoff) {
            return Long.MAX_VALUE;
        }
        long index = (value + mBucketSize / 2) / mBucketSize;
        return mBucketSize * index - mBucketSize / 2;
    }
}
