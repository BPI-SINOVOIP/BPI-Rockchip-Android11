/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.car.storagemonitoring;

import android.car.storagemonitoring.IoStatsEntry;
import android.car.storagemonitoring.UidIoRecord;
import android.util.SparseArray;

import androidx.annotation.GuardedBy;

import com.android.car.SparseArrayStream;
import com.android.car.procfsinspector.ProcessInfo;
import com.android.car.systeminterface.SystemStateInterface;

import java.util.List;
import java.util.Optional;

public class IoStatsTracker {

    // NOTE: this class is not thread safe
    private abstract class Lazy<T> {
        protected Optional<T> mLazy = Optional.empty();

        protected abstract T supply();

        public T get() {
            if (!mLazy.isPresent()) {
                mLazy = Optional.of(supply());
            }
            return mLazy.get();
        }
    }

    private final Object mLock = new Object();
    private final long mSampleWindowMs;
    private final SystemStateInterface mSystemStateInterface;
    @GuardedBy("mLock")
    private SparseArray<IoStatsEntry> mTotal;
    @GuardedBy("mLock")
    private SparseArray<IoStatsEntry> mCurrentSample;

    public IoStatsTracker(List<IoStatsEntry> initialValue,
            long sampleWindowMs, SystemStateInterface systemStateInterface) {
        mTotal = new SparseArray<>(initialValue.size());
        initialValue.forEach(uidIoStats -> mTotal.append(uidIoStats.uid, uidIoStats));
        mCurrentSample = mTotal.clone();
        mSampleWindowMs = sampleWindowMs;
        mSystemStateInterface = systemStateInterface;
    }

    /**
     * Updates the tracker information with new metrics.
     */
    public void update(SparseArray<UidIoRecord> newMetrics) {
        final Lazy<List<ProcessInfo>> processTable = new Lazy<List<ProcessInfo>>() {
            @Override
            protected List<ProcessInfo> supply() {
                return mSystemStateInterface.getRunningProcesses();
            }
        };

        SparseArray<IoStatsEntry> newSample = new SparseArray<>();
        SparseArray<IoStatsEntry> newTotal = new SparseArray<>();

        synchronized (mLock) {
            // prepare the new values
            SparseArrayStream.valueStream(newMetrics).forEach(newRecord -> {
                final int uid = newRecord.uid;
                final IoStatsEntry oldRecord = mTotal.get(uid);

                IoStatsEntry newStats = null;

                if (oldRecord == null) {
                    // this user id has just showed up, so just add it to the current sample
                    // and its runtime is the size of our sample window
                    newStats = new IoStatsEntry(newRecord, mSampleWindowMs);
                } else {
                    // this user id has already been detected

                    if (oldRecord.representsSameMetrics(newRecord)) {
                        // if no new I/O happened, try to figure out if any process on behalf
                        // of this user has happened, and use that to update the runtime metrics
                        if (processTable.get().stream().anyMatch(pi -> pi.uid == uid)) {
                            newStats = new IoStatsEntry(newRecord.delta(oldRecord),
                                    oldRecord.runtimeMillis + mSampleWindowMs);
                        }
                        // if no new I/O happened and no process is running for this user
                        // then do not prepare a new sample, as nothing has changed
                    } else {
                        // but if new I/O happened, assume something was running for the entire
                        // sample window and compute the delta
                        newStats = new IoStatsEntry(newRecord.delta(oldRecord),
                                oldRecord.runtimeMillis + mSampleWindowMs);
                    }
                }

                if (newStats != null) {
                    newSample.put(uid, newStats);
                    newTotal.append(uid, new IoStatsEntry(newRecord, newStats.runtimeMillis));
                } else {
                    // if oldRecord were null, newStats would be != null and we wouldn't be here
                    newTotal.append(uid, oldRecord);
                }
            });

            // now update the stored values
            mCurrentSample = newSample;
            mTotal = newTotal;
        }
    }

    /**
     * Returns all IO stats entries.
     */
    public SparseArray<IoStatsEntry> getTotal() {
        synchronized (mLock) {
            return mTotal.clone();
        }
    }

    /**
     * Return newly added IO stats entries.
     */
    public SparseArray<IoStatsEntry> getCurrentSample() {
        synchronized (mLock) {
            return mCurrentSample.clone();
        }
    }
}
