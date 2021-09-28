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

package com.android.tv.dvr.provider;

import android.database.Cursor;
import android.support.annotation.Nullable;
import android.util.Log;

import com.android.tv.common.concurrent.NamedThreadFactory;
import com.android.tv.common.flags.DvrFlags;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.dvr.data.SeriesRecording;
import com.android.tv.dvr.provider.DvrContract.Schedules;
import com.android.tv.dvr.provider.DvrContract.SeriesRecordings;
import com.android.tv.util.MainThreadExecutor;

import com.google.auto.factory.AutoFactory;
import com.google.auto.factory.Provided;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.ListeningExecutorService;
import com.google.common.util.concurrent.MoreExecutors;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executors;

/** {@link DvrDbFuture} that defaults to executing on its own single threaded Executor Service. */
public abstract class DvrDbFuture<ParamsT, ResultT> {
    private static final NamedThreadFactory THREAD_FACTORY =
            new NamedThreadFactory(DvrDbFuture.class.getSimpleName());
    private static final ListeningExecutorService DB_EXECUTOR =
            MoreExecutors.listeningDecorator(Executors.newSingleThreadExecutor(THREAD_FACTORY));

    final DvrDatabaseHelper mDbHelper;
    private ListenableFuture<ResultT> mFuture;

    private DvrDbFuture(DvrDatabaseHelper mDbHelper) {
        this.mDbHelper = mDbHelper;
    }

    /** Execute the task on the {@link #DB_EXECUTOR} thread and return Future */
    @SafeVarargs
    public final ListenableFuture<ResultT> executeOnDbThread(
            FutureCallback<ResultT> callback, ParamsT... params) {
        mFuture = DB_EXECUTOR.submit(() -> dbHelperInBackground(params));
        Futures.addCallback(mFuture, callback, MainThreadExecutor.getInstance());
        return mFuture;
    }

    /** Executes in the background after initializing DbHelper} */
    @Nullable
    protected abstract ResultT dbHelperInBackground(ParamsT... params);

    public final boolean isCancelled() {
        return mFuture.isCancelled();
    }

    /** Inserts schedules. */
    public static class AddScheduleFuture extends DvrDbFuture<ScheduledRecording, Void> {
        public AddScheduleFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        protected final Void dbHelperInBackground(ScheduledRecording... params) {
            mDbHelper.insertSchedules(params);
            return null;
        }
    }

    /** Update schedules. */
    public static class UpdateScheduleFuture extends DvrDbFuture<ScheduledRecording, Void> {
        public UpdateScheduleFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        protected final Void dbHelperInBackground(ScheduledRecording... params) {
            mDbHelper.updateSchedules(params);
            return null;
        }
    }

    /** Delete schedules. */
    public static class DeleteScheduleFuture extends DvrDbFuture<ScheduledRecording, Void> {
        public DeleteScheduleFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        protected final Void dbHelperInBackground(ScheduledRecording... params) {
            mDbHelper.deleteSchedules(params);
            return null;
        }
    }

    /** Returns all {@link ScheduledRecording}s. */
    public static class DvrQueryScheduleFuture extends DvrDbFuture<Void, List<ScheduledRecording>> {

        private final DvrFlags mDvrFlags;

        /**
         * Factory for {@link DvrQueryScheduleFuture}.
         *
         * <p>This wrapper class keeps other classes from needing to reference the
         * {@link AutoFactory} generated class.
         */
        public interface Factory {
            public DvrQueryScheduleFuture create(DvrDatabaseHelper dbHelper);
        }

        @AutoFactory(implementing = Factory.class, className = "DvrQueryScheduleFutureFactory")
        public DvrQueryScheduleFuture(DvrDatabaseHelper dbHelper, @Provided DvrFlags dvrFlags) {
            super(dbHelper);
            mDvrFlags = dvrFlags;
        }

        @Override
        @Nullable
        protected final List<ScheduledRecording> dbHelperInBackground(Void... params) {
            if (isCancelled()) {
                return null;
            }
            List<ScheduledRecording> scheduledRecordings = new ArrayList<>();
            if (mDvrFlags.startEarlyEndLateEnabled()) {
                try (Cursor c = mDbHelper.query(Schedules.TABLE_NAME,
                        ScheduledRecording.PROJECTION_WITH_TIME_OFFSET)) {
                    while (c.moveToNext() && !isCancelled()) {
                        scheduledRecordings.add(ScheduledRecording.fromCursorWithTimeOffset(c));
                    }
                }
            } else {
                try (Cursor c = mDbHelper.query(Schedules.TABLE_NAME,
                        ScheduledRecording.PROJECTION)) {
                    while (c.moveToNext() && !isCancelled()) {
                        scheduledRecordings.add(ScheduledRecording.fromCursor(c));
                    }
                }
            }
            return scheduledRecordings;
        }
    }

    /** Inserts series recordings. */
    public static class AddSeriesRecordingFuture extends DvrDbFuture<SeriesRecording, Void> {
        public AddSeriesRecordingFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        protected final Void dbHelperInBackground(SeriesRecording... params) {
            mDbHelper.insertSeriesRecordings(params);
            return null;
        }
    }

    /** Update series recordings. */
    public static class UpdateSeriesRecordingFuture extends DvrDbFuture<SeriesRecording, Void> {
        public UpdateSeriesRecordingFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        protected final Void dbHelperInBackground(SeriesRecording... params) {
            mDbHelper.updateSeriesRecordings(params);
            return null;
        }
    }

    /** Delete series recordings. */
    public static class DeleteSeriesRecordingFuture extends DvrDbFuture<SeriesRecording, Void> {
        public DeleteSeriesRecordingFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        protected final Void dbHelperInBackground(SeriesRecording... params) {
            mDbHelper.deleteSeriesRecordings(params);
            return null;
        }
    }

    /** Returns all {@link SeriesRecording}s. */
    public static class DvrQuerySeriesRecordingFuture
            extends DvrDbFuture<Void, List<SeriesRecording>> {
        private static final String TAG = "DvrQuerySeriesRecording";

        public DvrQuerySeriesRecordingFuture(DvrDatabaseHelper dbHelper) {
            super(dbHelper);
        }

        @Override
        @Nullable
        protected final List<SeriesRecording> dbHelperInBackground(Void... params) {
            if (isCancelled()) {
                return null;
            }
            List<SeriesRecording> scheduledRecordings = new ArrayList<>();
            try (Cursor c =
                    mDbHelper.query(SeriesRecordings.TABLE_NAME, SeriesRecording.PROJECTION)) {
                while (c.moveToNext() && !isCancelled()) {
                    scheduledRecordings.add(SeriesRecording.fromCursor(c));
                }
            } catch (Exception e) {
                Log.w(TAG, "Can't query dvr series recording data", e);
            }
            return scheduledRecordings;
        }
    }
}
