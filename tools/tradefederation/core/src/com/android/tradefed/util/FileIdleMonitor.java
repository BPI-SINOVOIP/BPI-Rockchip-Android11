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
package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil.CLog;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.Sets;

import java.io.File;
import java.time.Duration;
import java.time.Instant;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/**
 * Monitors files and executes a callback if they have been idle (i.e. none of them have been
 * modified) for too long.
 */
public class FileIdleMonitor {

    private final Duration mTimeout;
    private final Runnable mCallback;
    private final Set<File> mFiles;

    private final Supplier<ScheduledExecutorService> mExecutorSupplier;
    private ScheduledExecutorService mExecutor;

    /**
     * Constructs a file monitor with a default executor.
     *
     * @param timeout maximum idle time
     * @param callback callback to execute if idle for too long
     * @param files files to monitor
     */
    public FileIdleMonitor(Duration timeout, Runnable callback, File... files) {
        this(Executors::newSingleThreadScheduledExecutor, timeout, callback, files);
    }

    @VisibleForTesting
    FileIdleMonitor(
            Supplier<ScheduledExecutorService> executorSupplier,
            Duration timeout,
            Runnable callback,
            File... files) {
        mExecutorSupplier = executorSupplier;
        mTimeout = timeout;
        mCallback = callback;
        mFiles = Sets.newHashSet(files);
    }

    /** Starts monitoring files. No-op if already started. */
    public void start() {
        if (mExecutor == null) {
            mExecutor = mExecutorSupplier.get();
            checkIfIdle();
        }
    }

    /** Stops monitoring files. No-op if already stopped. */
    public void stop() {
        if (mExecutor != null) {
            mExecutor.shutdownNow();
            mExecutor = null;
        }
    }

    /**
     * Fetch the latest modification timestamp of all monitored files.
     *
     * @return latest modification time (epoch milliseconds) or 0L if the files were not found.
     */
    private long getTimestamp() {
        return mFiles.stream().map(File::lastModified).max(Long::compare).orElse(0L);
    }

    /** Check whether the monitored files have been idle for too long. */
    private void checkIfIdle() {
        long timestamp = getTimestamp();
        if (timestamp == 0L) {
            // files not found, check again after timeout
            scheduleCheck(mTimeout);
            return;
        }

        Duration elapsed = Duration.between(Instant.ofEpochMilli(timestamp), Instant.now());
        Duration remaining = mTimeout.minus(elapsed);
        if (remaining.isNegative()) {
            // timeout was exceeded, execute callback and check again after the timeout
            executeCallback();
            scheduleCheck(mTimeout);
        } else {
            // timeout not reached, check again after the remaining time
            scheduleCheck(remaining);
        }
    }

    /** Schedule an idle check after the specified delay. */
    private void scheduleCheck(Duration delay) {
        mExecutor.schedule(this::checkIfIdle, delay.toMillis(), TimeUnit.MILLISECONDS);
    }

    /** Run callback method, catching all exceptions. */
    private void executeCallback() {
        try {
            mCallback.run();
        } catch (RuntimeException e) {
            CLog.e("Failed to execute callback");
            CLog.e(e);
        }
    }
}
