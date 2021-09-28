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
package com.android.tradefed.cluster;

import com.android.tradefed.log.LogUtil.CLog;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

/** ClusterEventUploader class, which uploads {@link IClusterEvent} to TFC. */
public abstract class ClusterEventUploader<T extends IClusterEvent>
        implements IClusterEventUploader<T> {

    // Default maximum event batch size
    private static final int DEFAULT_MAX_BATCH_SIZE = 200;

    // Default event upload interval in ms.
    private static final long DEFAULT_EVENT_UPLOAD_INTERVAL = 60 * 1000;

    private int mMaxBatchSize = DEFAULT_MAX_BATCH_SIZE;
    private long mEventUploadInterval = DEFAULT_EVENT_UPLOAD_INTERVAL;
    private long mLastEventUploadTime = 0;
    private Queue<T> mEventQueue = new ConcurrentLinkedQueue<T>();

    /** {@inheritDoc} */
    @Override
    public void setMaxBatchSize(int batchSize) {
        mMaxBatchSize = batchSize;
    }

    /** {@inheritDoc} */
    @Override
    public int getMaxBatchSize() {
        return mMaxBatchSize;
    }

    /** {@inheritDoc} */
    @Override
    public void setEventUploadInterval(long interval) {
        mEventUploadInterval = interval;
    }

    /** {@inheritDoc} */
    @Override
    public long getEventUploadInterval() {
        return mEventUploadInterval;
    }

    /** {@inheritDoc} */
    @Override
    public void postEvent(final T event) {
        mEventQueue.add(event);
        uploadEvents(false);
    }

    /** {@inheritDoc} */
    @Override
    public void flush() {
        uploadEvents(true);
    }

    /**
     * Upload events.
     *
     * @param uploadNow upload now or wait for uploading with other events.
     */
    private void uploadEvents(final boolean uploadNow) {
        final long now = System.currentTimeMillis();
        if (!uploadNow && now - mLastEventUploadTime < getEventUploadInterval()) {
            return;
        }
        uploadEvents();
    }

    /** Synchronized actually upload events. Only one thread will upload the events. */
    private synchronized void uploadEvents() {
        // Tradefed Cluster is unable to process command events larger than 100 KiB (b/72104215).
        // The ClusterCommandEvent Builder provides some protection against this by limiting data
        // fields to 1 KiB, but the number of data fields is not bounded, so we're not entirely
        // protected against this.

        mLastEventUploadTime = System.currentTimeMillis();
        List<T> events = new ArrayList<T>();
        try {
            // Upload batches of events until there are no more left
            while (!mEventQueue.isEmpty()) {
                // Limit the number of events to upload at once
                int batchSize = getMaxBatchSize();
                while (!mEventQueue.isEmpty() && events.size() < batchSize) {
                    events.add(mEventQueue.poll());
                }
                doUploadEvents(events);
                events.clear();
            }
        } catch (IOException e) {
            CLog.w("failed to upload events: %s", e);
            CLog.w("events will be uploaded with the next event.");
            mEventQueue.addAll(events);
        }
    }

    protected abstract void doUploadEvents(List<T> events) throws IOException;
}
