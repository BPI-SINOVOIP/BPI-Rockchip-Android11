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

package com.android.tv.dvr;

import android.annotation.TargetApi;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.database.ContentObserver;
import android.database.sqlite.SQLiteException;
import android.media.tv.TvContract.RecordedPrograms;
import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager.TvInputCallback;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.MainThread;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.ArraySet;
import android.util.Log;
import android.util.Range;

import com.android.tv.TvSingletons;
import com.android.tv.common.SoftPreconditions;
import com.android.tv.common.dagger.annotations.ApplicationContext;
import com.android.tv.common.recording.RecordingStorageStatusManager;
import com.android.tv.common.recording.RecordingStorageStatusManager.OnStorageMountChangedListener;
import com.android.tv.common.util.Clock;
import com.android.tv.common.util.CommonUtils;
import com.android.tv.dvr.data.IdGenerator;
import com.android.tv.dvr.data.RecordedProgram;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.dvr.data.ScheduledRecording.RecordingState;
import com.android.tv.dvr.data.SeriesRecording;
import com.android.tv.dvr.provider.DvrDatabaseHelper;
import com.android.tv.dvr.provider.DvrDbFuture.AddScheduleFuture;
import com.android.tv.dvr.provider.DvrDbFuture.AddSeriesRecordingFuture;
import com.android.tv.dvr.provider.DvrDbFuture.DeleteScheduleFuture;
import com.android.tv.dvr.provider.DvrDbFuture.DeleteSeriesRecordingFuture;
import com.android.tv.dvr.provider.DvrDbFuture.DvrQueryScheduleFuture;
import com.android.tv.dvr.provider.DvrDbFuture.DvrQuerySeriesRecordingFuture;
import com.android.tv.dvr.provider.DvrDbFuture.UpdateScheduleFuture;
import com.android.tv.dvr.provider.DvrDbFuture.UpdateSeriesRecordingFuture;
import com.android.tv.dvr.provider.DvrDbSync;
import com.android.tv.dvr.recorder.SeriesRecordingScheduler;
import com.android.tv.util.AsyncDbTask;
import com.android.tv.util.AsyncDbTask.AsyncRecordedProgramQueryTask;
import com.android.tv.util.AsyncDbTask.DbExecutor;
import com.android.tv.util.TvInputManagerHelper;
import com.android.tv.util.TvUriMatcher;

import com.google.common.base.Predicate;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.ListenableFuture;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.Future;

import javax.inject.Inject;
import javax.inject.Singleton;

/** DVR Data manager to handle recordings and schedules. */
@MainThread
@TargetApi(Build.VERSION_CODES.N)
@Singleton
public class DvrDataManagerImpl extends BaseDvrDataManager {
    private static final String TAG = "DvrDataManagerImpl";
    private static final boolean DEBUG = false;

    private final TvInputManagerHelper mInputManager;

    private final HashMap<Long, ScheduledRecording> mScheduledRecordings = new HashMap<>();
    private final HashMap<Long, RecordedProgram> mRecordedPrograms = new HashMap<>();
    private final HashMap<Long, SeriesRecording> mSeriesRecordings = new HashMap<>();
    private final HashMap<Long, ScheduledRecording> mProgramId2ScheduledRecordings =
            new HashMap<>();
    private final HashMap<String, SeriesRecording> mSeriesId2SeriesRecordings = new HashMap<>();

    private final HashMap<Long, ScheduledRecording> mScheduledRecordingsForRemovedInput =
            new HashMap<>();
    private final HashMap<Long, RecordedProgram> mRecordedProgramsForRemovedInput = new HashMap<>();
    private final HashMap<Long, SeriesRecording> mSeriesRecordingsForRemovedInput = new HashMap<>();

    private final Context mContext;
    private final DvrDatabaseHelper mDbHelper;
    private final DvrDbSync.Factory mDvrDbSyncFactory;
    private final DvrQueryScheduleFuture.Factory mDvrQueryScheduleFutureFactory;
    private Executor mDbExecutor;
    private final ContentObserver mContentObserver =
            new ContentObserver(new Handler(Looper.getMainLooper())) {
                @Override
                public void onChange(boolean selfChange) {
                    onChange(selfChange, null);
                }

                @Override
                public void onChange(boolean selfChange, final @Nullable Uri uri) {
                    RecordedProgramsQueryTask task = new RecordedProgramsQueryTask(uri);
                    task.executeOnDbThread();
                    mPendingTasks.add(task);
                }
            };

    private boolean mDvrLoadFinished;
    private boolean mRecordedProgramLoadFinished;
    private final Set<AsyncTask> mPendingTasks = new ArraySet<>();
    private final Set<Future> mPendingDvrFuture = new ArraySet<>();
    // TODO(b/79207567) make sure Future is not stopped at writing.
    private final Set<Future> mNoStopFuture = new ArraySet<>();
    private DvrDbSync mDbSync;
    private RecordingStorageStatusManager mStorageStatusManager;

    private final TvInputCallback mInputCallback =
            new TvInputCallback() {
                @Override
                public void onInputAdded(String inputId) {
                    if (DEBUG) Log.d(TAG, "onInputAdded " + inputId);
                    if (!isInputAvailable(inputId)) {
                        if (DEBUG) Log.d(TAG, "Not available for recording");
                        return;
                    }
                    unhideInput(inputId);
                }

                @Override
                public void onInputRemoved(String inputId) {
                    if (DEBUG) Log.d(TAG, "onInputRemoved " + inputId);
                    hideInput(inputId);
                }
            };

    private final OnStorageMountChangedListener mStorageMountChangedListener =
            new OnStorageMountChangedListener() {
                @Override
                public void onStorageMountChanged(boolean storageMounted) {
                    for (TvInputInfo input : mInputManager.getTvInputInfos(true, true)) {
                        if (CommonUtils.isBundledInput(input.getId())) {
                            if (storageMounted) {
                                unhideInput(input.getId());
                            } else {
                                hideInput(input.getId());
                            }
                        }
                    }
                }
            };

    private final FutureCallback<Void> removeFromSetOnCompletion =
            new FutureCallback<Void>() {
                @Override
                public void onSuccess(Void result) {
                    mNoStopFuture.remove(this);
                }

                @Override
                public void onFailure(Throwable t) {
                    Log.w(TAG, "Failed to execute.", t);
                    mNoStopFuture.remove(this);
                }
            };

    private static <T> List<T> moveElements(
            HashMap<Long, T> from, HashMap<Long, T> to, Predicate<T> filter) {
        List<T> moved = new ArrayList<>();
        Iterator<Entry<Long, T>> iter = from.entrySet().iterator();
        while (iter.hasNext()) {
            Entry<Long, T> entry = iter.next();
            if (filter.apply(entry.getValue())) {
                to.put(entry.getKey(), entry.getValue());
                iter.remove();
                moved.add(entry.getValue());
            }
        }
        return moved;
    }

    @Inject
    public DvrDataManagerImpl(
            @ApplicationContext Context context,
            Clock clock,
            TvInputManagerHelper tvInputManagerHelper,
            @DbExecutor Executor dbExecutor,
            DvrDatabaseHelper dbHelper,
            DvrDbSync.Factory dvrDbSyncFactory,
            DvrQueryScheduleFuture.Factory dvrQueryScheduleFutureFactory) {
        super(context, clock);
        mContext = context;
        TvSingletons tvSingletons = TvSingletons.getSingletons(context);
        mInputManager = tvInputManagerHelper;
        mStorageStatusManager = tvSingletons.getRecordingStorageStatusManager();
        mDbExecutor = dbExecutor;
        mDbHelper = dbHelper;
        mDvrQueryScheduleFutureFactory = dvrQueryScheduleFutureFactory;
        mDvrDbSyncFactory = dvrDbSyncFactory;
        start();
    }

    private void start() {
        mInputManager.addCallback(mInputCallback);
        mStorageStatusManager.addListener(mStorageMountChangedListener);
        DvrQuerySeriesRecordingFuture dvrQuerySeriesRecordingTask =
                new DvrQuerySeriesRecordingFuture(mDbHelper);
        ListenableFuture<List<SeriesRecording>> dvrQuerySeriesRecordingFuture =
                dvrQuerySeriesRecordingTask.executeOnDbThread(
                        new FutureCallback<List<SeriesRecording>>() {
                            @Override
                            public void onSuccess(List<SeriesRecording> seriesRecordings) {
                                mPendingDvrFuture.remove(this);
                                long maxId = 0;
                                HashSet<String> seriesIds = new HashSet<>();
                                for (SeriesRecording r : seriesRecordings) {
                                    if (SoftPreconditions.checkState(
                                            !seriesIds.contains(r.getSeriesId()),
                                            TAG,
                                            "Skip loading series recording with duplicate series"
                                                    + " ID: "
                                                    + r)) {
                                        seriesIds.add(r.getSeriesId());
                                        if (isInputAvailable(r.getInputId())) {
                                            mSeriesRecordings.put(r.getId(), r);
                                            mSeriesId2SeriesRecordings.put(r.getSeriesId(), r);
                                        } else {
                                            mSeriesRecordingsForRemovedInput.put(r.getId(), r);
                                        }
                                    }
                                    if (maxId < r.getId()) {
                                        maxId = r.getId();
                                    }
                                }
                                IdGenerator.SERIES_RECORDING.setMaxId(maxId);
                            }

                            @Override
                            public void onFailure(Throwable t) {
                                Log.w(TAG, "Failed to load series recording.", t);
                                mPendingDvrFuture.remove(this);
                            }
                        });
        mPendingDvrFuture.add(dvrQuerySeriesRecordingFuture);
        DvrQueryScheduleFuture dvrQueryScheduleTask =
                mDvrQueryScheduleFutureFactory.create(mDbHelper);
        ListenableFuture<List<ScheduledRecording>> dvrQueryScheduleFuture =
                dvrQueryScheduleTask.executeOnDbThread(
                        new FutureCallback<List<ScheduledRecording>>() {
                            @Override
                            public void onSuccess(List<ScheduledRecording> result) {
                                mPendingDvrFuture.remove(this);
                                long maxId = 0;
                                int reasonNotStarted =
                                        ScheduledRecording
                                                .FAILED_REASON_PROGRAM_ENDED_BEFORE_RECORDING_STARTED;
                                List<ScheduledRecording> toUpdate = new ArrayList<>();
                                List<ScheduledRecording> toDelete = new ArrayList<>();
                                for (ScheduledRecording r : result) {
                                    if (!isInputAvailable(r.getInputId())) {
                                        mScheduledRecordingsForRemovedInput.put(r.getId(), r);
                                    } else if (r.getState()
                                            == ScheduledRecording.STATE_RECORDING_DELETED) {
                                        getDeletedScheduleMap().put(r.getProgramId(), r);
                                    } else {
                                        mScheduledRecordings.put(r.getId(), r);
                                        if (r.getProgramId() != ScheduledRecording.ID_NOT_SET) {
                                            mProgramId2ScheduledRecordings.put(r.getProgramId(), r);
                                        }
                                        // Adjust the state of the schedules before DB loading is
                                        // finished.
                                        switch (r.getState()) {
                                            case ScheduledRecording.STATE_RECORDING_IN_PROGRESS:
                                                if (r.getEndTimeMs()
                                                        <= mClock.currentTimeMillis()) {
                                                    int reason =
                                                            ScheduledRecording
                                                                    .FAILED_REASON_NOT_FINISHED;
                                                    toUpdate.add(
                                                            ScheduledRecording.buildFrom(r)
                                                                    .setState(
                                                                            ScheduledRecording
                                                                                    .STATE_RECORDING_FAILED)
                                                                    .setFailedReason(reason)
                                                                    .build());
                                                } else {
                                                    toUpdate.add(
                                                            ScheduledRecording.buildFrom(r)
                                                                    .setState(
                                                                            ScheduledRecording
                                                                                    .STATE_RECORDING_NOT_STARTED)
                                                                    .build());
                                                }
                                                break;
                                            case ScheduledRecording.STATE_RECORDING_NOT_STARTED:
                                                if (r.getEndTimeMs()
                                                        <= mClock.currentTimeMillis()) {
                                                    toUpdate.add(
                                                            ScheduledRecording.buildFrom(r)
                                                                    .setState(
                                                                            ScheduledRecording
                                                                                    .STATE_RECORDING_FAILED)
                                                                    .setFailedReason(
                                                                            reasonNotStarted)
                                                                    .build());
                                                }
                                                break;
                                            case ScheduledRecording.STATE_RECORDING_CANCELED:
                                                toDelete.add(r);
                                                break;
                                            default: // fall out
                                        }
                                    }
                                    if (maxId < r.getId()) {
                                        maxId = r.getId();
                                    }
                                }
                                if (!toUpdate.isEmpty()) {
                                    updateScheduledRecording(ScheduledRecording.toArray(toUpdate));
                                }
                                if (!toDelete.isEmpty()) {
                                    removeScheduledRecording(ScheduledRecording.toArray(toDelete));
                                }
                                IdGenerator.SCHEDULED_RECORDING.setMaxId(maxId);
                                if (mRecordedProgramLoadFinished) {
                                    validateSeriesRecordings();
                                }
                                mDvrLoadFinished = true;
                                notifyDvrScheduleLoadFinished();
                                if (isInitialized()) {
                                    mDbSync = mDvrDbSyncFactory.create(
                                                    mContext,
                                                    DvrDataManagerImpl.this);
                                    mDbSync.start();
                                    SeriesRecordingScheduler.getInstance(mContext).start();
                                }
                            }

                            @Override
                            public void onFailure(Throwable t) {
                                Log.w(TAG, "Failed to load scheduled recording.", t);
                                mPendingDvrFuture.remove(this);
                            }
                        });
        mPendingDvrFuture.add(dvrQueryScheduleFuture);
        RecordedProgramsQueryTask mRecordedProgramQueryTask = new RecordedProgramsQueryTask(null);
        mRecordedProgramQueryTask.executeOnDbThread();
        ContentResolver cr = mContext.getContentResolver();
        cr.registerContentObserver(RecordedPrograms.CONTENT_URI, true, mContentObserver);
    }

    public void stop() {
        mInputManager.removeCallback(mInputCallback);
        mStorageStatusManager.removeListener(mStorageMountChangedListener);
        SeriesRecordingScheduler.getInstance(mContext).stop();
        if (mDbSync != null) {
            mDbSync.stop();
        }
        ContentResolver cr = mContext.getContentResolver();
        cr.unregisterContentObserver(mContentObserver);
        Iterator<AsyncTask> i = mPendingTasks.iterator();
        while (i.hasNext()) {
            AsyncTask task = i.next();
            i.remove();
            task.cancel(true);
        }
        Iterator<Future> id = mPendingDvrFuture.iterator();
        while (id.hasNext()) {
            Future future = id.next();
            id.remove();
            future.cancel(true);
        }
    }

    private void onRecordedProgramsLoadedFinished(Uri uri, List<RecordedProgram> recordedPrograms) {
        if (uri == null) {
            uri = RecordedPrograms.CONTENT_URI;
        }
        if (recordedPrograms == null) {
            recordedPrograms = Collections.emptyList();
        }
        int match = TvUriMatcher.match(uri);
        if (match == TvUriMatcher.MATCH_RECORDED_PROGRAM) {
            if (!mRecordedProgramLoadFinished) {
                for (RecordedProgram recorded : recordedPrograms) {
                    if (isInputAvailable(recorded.getInputId())) {
                        mRecordedPrograms.put(recorded.getId(), recorded);
                    } else {
                        mRecordedProgramsForRemovedInput.put(recorded.getId(), recorded);
                    }
                }
                mRecordedProgramLoadFinished = true;
                notifyRecordedProgramLoadFinished();
                if (isInitialized()) {
                    mDbSync = mDvrDbSyncFactory.create(mContext, DvrDataManagerImpl.this);
                    mDbSync.start();
                }
            } else if (recordedPrograms.isEmpty()) {
                List<RecordedProgram> oldRecordedPrograms =
                        new ArrayList<>(mRecordedPrograms.values());
                mRecordedPrograms.clear();
                mRecordedProgramsForRemovedInput.clear();
                notifyRecordedProgramsRemoved(RecordedProgram.toArray(oldRecordedPrograms));
            } else {
                HashMap<Long, RecordedProgram> oldRecordedPrograms =
                        new HashMap<>(mRecordedPrograms);
                mRecordedPrograms.clear();
                mRecordedProgramsForRemovedInput.clear();
                List<RecordedProgram> addedRecordedPrograms = new ArrayList<>();
                List<RecordedProgram> changedRecordedPrograms = new ArrayList<>();
                for (RecordedProgram recorded : recordedPrograms) {
                    if (isInputAvailable(recorded.getInputId())) {
                        mRecordedPrograms.put(recorded.getId(), recorded);
                        if (oldRecordedPrograms.remove(recorded.getId()) == null) {
                            addedRecordedPrograms.add(recorded);
                        } else {
                            changedRecordedPrograms.add(recorded);
                        }
                    } else {
                        mRecordedProgramsForRemovedInput.put(recorded.getId(), recorded);
                    }
                }
                if (!addedRecordedPrograms.isEmpty()) {
                    notifyRecordedProgramsAdded(RecordedProgram.toArray(addedRecordedPrograms));
                }
                if (!changedRecordedPrograms.isEmpty()) {
                    notifyRecordedProgramsChanged(RecordedProgram.toArray(changedRecordedPrograms));
                }
                if (!oldRecordedPrograms.isEmpty()) {
                    notifyRecordedProgramsRemoved(
                            RecordedProgram.toArray(oldRecordedPrograms.values()));
                }
            }
            if (isInitialized()) {
                validateSeriesRecordings();
                SeriesRecordingScheduler.getInstance(mContext).start();
            }
        } else if (match == TvUriMatcher.MATCH_RECORDED_PROGRAM_ID) {
            if (!mRecordedProgramLoadFinished) {
                return;
            }
            long id = ContentUris.parseId(uri);
            if (DEBUG) Log.d(TAG, "changed recorded program #" + id + " to " + recordedPrograms);
            if (recordedPrograms.isEmpty()) {
                mRecordedProgramsForRemovedInput.remove(id);
                RecordedProgram old = mRecordedPrograms.remove(id);
                if (old != null) {
                    notifyRecordedProgramsRemoved(old);
                    SeriesRecording r = mSeriesId2SeriesRecordings.get(old.getSeriesId());
                    if (r != null && isEmptySeriesRecording(r)) {
                        removeSeriesRecording(r);
                    }
                }
            } else {
                RecordedProgram recordedProgram = recordedPrograms.get(0);
                if (isInputAvailable(recordedProgram.getInputId())) {
                    RecordedProgram old = mRecordedPrograms.put(id, recordedProgram);
                    if (old == null) {
                        notifyRecordedProgramsAdded(recordedProgram);
                    } else {
                        notifyRecordedProgramsChanged(recordedProgram);
                    }
                } else {
                    mRecordedProgramsForRemovedInput.put(id, recordedProgram);
                }
            }
        }
    }

    @Override
    public boolean isInitialized() {
        return mDvrLoadFinished && mRecordedProgramLoadFinished;
    }

    @Override
    public boolean isDvrScheduleLoadFinished() {
        return mDvrLoadFinished;
    }

    @Override
    public boolean isRecordedProgramLoadFinished() {
        return mRecordedProgramLoadFinished;
    }

    private List<ScheduledRecording> getScheduledRecordingsPrograms() {
        if (!mDvrLoadFinished) {
            return Collections.emptyList();
        }
        ArrayList<ScheduledRecording> list = new ArrayList<>(mScheduledRecordings.size());
        list.addAll(mScheduledRecordings.values());
        Collections.sort(list, ScheduledRecording.START_TIME_COMPARATOR);
        return list;
    }

    @Override
    public List<RecordedProgram> getRecordedPrograms() {
        if (!mRecordedProgramLoadFinished) {
            return Collections.emptyList();
        }
        return new ArrayList<>(mRecordedPrograms.values());
    }

    @Override
    public List<RecordedProgram> getRecordedPrograms(long seriesRecordingId) {
        SeriesRecording seriesRecording = getSeriesRecording(seriesRecordingId);
        if (!mRecordedProgramLoadFinished || seriesRecording == null) {
            return Collections.emptyList();
        }
        return super.getRecordedPrograms(seriesRecordingId);
    }

    @Override
    public List<ScheduledRecording> getAllScheduledRecordings() {
        return new ArrayList<>(mScheduledRecordings.values());
    }

    @Override
    protected List<ScheduledRecording> getRecordingsWithState(@RecordingState int... states) {
        List<ScheduledRecording> result = new ArrayList<>();
        for (ScheduledRecording r : mScheduledRecordings.values()) {
            for (int state : states) {
                if (r.getState() == state) {
                    result.add(r);
                    break;
                }
            }
        }
        return result;
    }

    @Override
    public List<SeriesRecording> getSeriesRecordings() {
        if (!mDvrLoadFinished) {
            return Collections.emptyList();
        }
        return new ArrayList<>(mSeriesRecordings.values());
    }

    @Override
    public List<SeriesRecording> getSeriesRecordings(String inputId) {
        List<SeriesRecording> result = new ArrayList<>();
        for (SeriesRecording r : mSeriesRecordings.values()) {
            if (TextUtils.equals(r.getInputId(), inputId)) {
                result.add(r);
            }
        }
        return result;
    }

    @Override
    public long getNextScheduledStartTimeAfter(long startTime) {
        return getNextStartTimeAfter(getScheduledRecordingsPrograms(), startTime);
    }

    @VisibleForTesting
    static long getNextStartTimeAfter(
            List<ScheduledRecording> scheduledRecordings, long startTime) {
        int start = 0;
        int end = scheduledRecordings.size() - 1;
        while (start <= end) {
            int mid = (start + end) / 2;
            if (scheduledRecordings.get(mid).getStartTimeMs() <= startTime) {
                start = mid + 1;
            } else {
                end = mid - 1;
            }
        }
        return start < scheduledRecordings.size()
                ? scheduledRecordings.get(start).getStartTimeMs()
                : NEXT_START_TIME_NOT_FOUND;
    }

    @Override
    public List<ScheduledRecording> getScheduledRecordings(
            Range<Long> period, @RecordingState int state) {
        List<ScheduledRecording> result = new ArrayList<>();
        for (ScheduledRecording r : mScheduledRecordings.values()) {
            if (r.isOverLapping(period) && r.getState() == state) {
                result.add(r);
            }
        }
        return result;
    }

    @Override
    public List<ScheduledRecording> getScheduledRecordings(long seriesRecordingId) {
        List<ScheduledRecording> result = new ArrayList<>();
        for (ScheduledRecording r : mScheduledRecordings.values()) {
            if (r.getSeriesRecordingId() == seriesRecordingId) {
                result.add(r);
            }
        }
        return result;
    }

    @Override
    public List<ScheduledRecording> getScheduledRecordings(String inputId) {
        List<ScheduledRecording> result = new ArrayList<>();
        for (ScheduledRecording r : mScheduledRecordings.values()) {
            if (TextUtils.equals(r.getInputId(), inputId)) {
                result.add(r);
            }
        }
        return result;
    }

    @Nullable
    @Override
    public ScheduledRecording getScheduledRecording(long recordingId) {
        return mScheduledRecordings.get(recordingId);
    }

    @Nullable
    @Override
    public ScheduledRecording getScheduledRecordingForProgramId(long programId) {
        return mProgramId2ScheduledRecordings.get(programId);
    }

    @Nullable
    @Override
    public RecordedProgram getRecordedProgram(long recordingId) {
        return mRecordedPrograms.get(recordingId);
    }

    @Nullable
    @Override
    public SeriesRecording getSeriesRecording(long seriesRecordingId) {
        return mSeriesRecordings.get(seriesRecordingId);
    }

    @Nullable
    @Override
    public SeriesRecording getSeriesRecording(String seriesId) {
        return mSeriesId2SeriesRecordings.get(seriesId);
    }

    @Override
    public void addScheduledRecording(ScheduledRecording... schedules) {
        for (ScheduledRecording r : schedules) {
            if (r.getId() == ScheduledRecording.ID_NOT_SET) {
                r.setId(IdGenerator.SCHEDULED_RECORDING.newId());
            }
            mScheduledRecordings.put(r.getId(), r);
            if (r.getProgramId() != ScheduledRecording.ID_NOT_SET) {
                mProgramId2ScheduledRecordings.put(r.getProgramId(), r);
            }
        }
        if (mDvrLoadFinished) {
            notifyScheduledRecordingAdded(schedules);
        }
        ListenableFuture addScheduleFuture =
                new AddScheduleFuture(mDbHelper)
                        .executeOnDbThread(removeFromSetOnCompletion, schedules);
        mNoStopFuture.add(addScheduleFuture);
        removeDeletedSchedules(schedules);
    }

    @Override
    public void addSeriesRecording(SeriesRecording... seriesRecordings) {
        for (SeriesRecording r : seriesRecordings) {
            r.setId(IdGenerator.SERIES_RECORDING.newId());
            mSeriesRecordings.put(r.getId(), r);
            SeriesRecording previousSeries = mSeriesId2SeriesRecordings.put(r.getSeriesId(), r);
            SoftPreconditions.checkArgument(
                    previousSeries == null,
                    TAG,
                    "Attempt to add series" + " recording with the duplicate series ID: %s",
                    r.getSeriesId());
        }
        if (mDvrLoadFinished) {
            notifySeriesRecordingAdded(seriesRecordings);
        }
        ListenableFuture addSeriesRecordingFuture =
                new AddSeriesRecordingFuture(mDbHelper)
                        .executeOnDbThread(removeFromSetOnCompletion, seriesRecordings);
        mNoStopFuture.add(addSeriesRecordingFuture);
    }

    @Override
    public void removeScheduledRecording(ScheduledRecording... schedules) {
        removeScheduledRecording(false, schedules);
    }

    @Override
    public void removeScheduledRecording(boolean forceRemove, ScheduledRecording... schedules) {
        List<ScheduledRecording> schedulesToDelete = new ArrayList<>();
        List<ScheduledRecording> schedulesNotToDelete = new ArrayList<>();
        Set<Long> seriesRecordingIdsToCheck = new HashSet<>();
        for (ScheduledRecording r : schedules) {
            mScheduledRecordings.remove(r.getId());
            getDeletedScheduleMap().remove(r.getProgramId());
            mProgramId2ScheduledRecordings.remove(r.getProgramId());
            if (r.getSeriesRecordingId() != SeriesRecording.ID_NOT_SET
                    && (r.getState() == ScheduledRecording.STATE_RECORDING_NOT_STARTED
                            || r.getState() == ScheduledRecording.STATE_RECORDING_IN_PROGRESS)) {
                seriesRecordingIdsToCheck.add(r.getSeriesRecordingId());
            }
            boolean isScheduleForRemovedInput =
                    mScheduledRecordingsForRemovedInput.remove(r.getProgramId()) != null;
            // If it belongs to the series recording and it's not started yet, just mark delete
            // instead of deleting it.
            if (!isScheduleForRemovedInput
                    && !forceRemove
                    && r.getSeriesRecordingId() != SeriesRecording.ID_NOT_SET
                    && (r.getState() == ScheduledRecording.STATE_RECORDING_NOT_STARTED
                            || r.getState() == ScheduledRecording.STATE_RECORDING_CANCELED)) {
                SoftPreconditions.checkState(r.getProgramId() != ScheduledRecording.ID_NOT_SET);
                ScheduledRecording deleted =
                        ScheduledRecording.buildFrom(r)
                                .setState(ScheduledRecording.STATE_RECORDING_DELETED)
                                .build();
                getDeletedScheduleMap().put(deleted.getProgramId(), deleted);
                schedulesNotToDelete.add(deleted);
            } else {
                schedulesToDelete.add(r);
            }
        }
        if (mDvrLoadFinished) {
            if (mRecordedProgramLoadFinished) {
                checkAndRemoveEmptySeriesRecording(seriesRecordingIdsToCheck);
            }
            notifyScheduledRecordingRemoved(schedules);
        }
        Iterator<ScheduledRecording> iterator = schedulesNotToDelete.iterator();
        while (iterator.hasNext()) {
            ScheduledRecording r = iterator.next();
            if (!mSeriesRecordings.containsKey(r.getSeriesRecordingId())) {
                iterator.remove();
                schedulesToDelete.add(r);
            }
        }
        if (!schedulesToDelete.isEmpty()) {
            ListenableFuture deleteScheduleFuture =
                    new DeleteScheduleFuture(mDbHelper)
                            .executeOnDbThread(
                                    removeFromSetOnCompletion,
                                    ScheduledRecording.toArray(schedulesToDelete));
            mNoStopFuture.add(deleteScheduleFuture);
        }
        if (!schedulesNotToDelete.isEmpty()) {
            ListenableFuture updateScheduleFuture =
                    new UpdateScheduleFuture(mDbHelper)
                            .executeOnDbThread(
                                    removeFromSetOnCompletion,
                                    ScheduledRecording.toArray(schedulesNotToDelete));
            mNoStopFuture.add(updateScheduleFuture);
        }
    }

    @Override
    public void removeSeriesRecording(final SeriesRecording... seriesRecordings) {
        HashSet<Long> ids = new HashSet<>();
        for (SeriesRecording r : seriesRecordings) {
            mSeriesRecordings.remove(r.getId());
            mSeriesId2SeriesRecordings.remove(r.getSeriesId());
            ids.add(r.getId());
        }
        // Reset series recording ID of the scheduled recording.
        List<ScheduledRecording> toUpdate = new ArrayList<>();
        List<ScheduledRecording> toDelete = new ArrayList<>();
        for (ScheduledRecording r : mScheduledRecordings.values()) {
            if (ids.contains(r.getSeriesRecordingId())) {
                if (r.getState() == ScheduledRecording.STATE_RECORDING_NOT_STARTED) {
                    toDelete.add(r);
                } else {
                    toUpdate.add(
                            ScheduledRecording.buildFrom(r)
                                    .setSeriesRecordingId(SeriesRecording.ID_NOT_SET)
                                    .build());
                }
            }
        }
        if (!toUpdate.isEmpty()) {
            // No need to update DB. It's handled in database automatically when the series
            // recording is deleted.
            updateScheduledRecording(false, ScheduledRecording.toArray(toUpdate));
        }
        if (!toDelete.isEmpty()) {
            removeScheduledRecording(true, ScheduledRecording.toArray(toDelete));
        }
        if (mDvrLoadFinished) {
            notifySeriesRecordingRemoved(seriesRecordings);
        }
        ListenableFuture deleteSeriesRecordingFuture =
                new DeleteSeriesRecordingFuture(mDbHelper)
                        .executeOnDbThread(removeFromSetOnCompletion, seriesRecordings);
        mNoStopFuture.add(deleteSeriesRecordingFuture);
        removeDeletedSchedules(seriesRecordings);
    }

    @Override
    public void updateScheduledRecording(final ScheduledRecording... schedules) {
        updateScheduledRecording(true, schedules);
    }

    private void updateScheduledRecording(boolean updateDb, final ScheduledRecording... schedules) {
        List<ScheduledRecording> toUpdate = new ArrayList<>();
        Set<Long> seriesRecordingIdsToCheck = new HashSet<>();
        for (ScheduledRecording r : schedules) {
            if (!SoftPreconditions.checkState(
                    mScheduledRecordings.containsKey(r.getId()),
                    TAG,
                    "Recording not found for: " + r)) {
                continue;
            }
            toUpdate.add(r);
            ScheduledRecording oldScheduledRecording = mScheduledRecordings.put(r.getId(), r);
            // The channel ID should not be changed.
            SoftPreconditions.checkState(r.getChannelId() == oldScheduledRecording.getChannelId());
            long programId = r.getProgramId();
            if (oldScheduledRecording.getProgramId() != programId
                    && oldScheduledRecording.getProgramId() != ScheduledRecording.ID_NOT_SET) {
                ScheduledRecording oldValueForProgramId =
                        mProgramId2ScheduledRecordings.get(oldScheduledRecording.getProgramId());
                if (oldValueForProgramId.getId() == r.getId()) {
                    // Only remove the old ScheduledRecording if it has the same ID as the new one.
                    mProgramId2ScheduledRecordings.remove(oldScheduledRecording.getProgramId());
                }
            }
            if (programId != ScheduledRecording.ID_NOT_SET) {
                mProgramId2ScheduledRecordings.put(programId, r);
            }
            if (r.getState() == ScheduledRecording.STATE_RECORDING_FAILED
                    && r.getSeriesRecordingId() != SeriesRecording.ID_NOT_SET) {
                // If the scheduled recording is failed, it may cause the automatically generated
                // series recording for this schedule becomes invalid (with no future schedules and
                // past recordings.) We should check and remove these series recordings.
                seriesRecordingIdsToCheck.add(r.getSeriesRecordingId());
            }
        }
        if (toUpdate.isEmpty()) {
            return;
        }
        ScheduledRecording[] scheduleArray = ScheduledRecording.toArray(toUpdate);
        if (mDvrLoadFinished) {
            notifyScheduledRecordingStatusChanged(scheduleArray);
        }
        if (updateDb) {
            ListenableFuture updateScheduleFuture =
                    new UpdateScheduleFuture(mDbHelper)
                            .executeOnDbThread(removeFromSetOnCompletion, scheduleArray);
            mNoStopFuture.add(updateScheduleFuture);
        }
        checkAndRemoveEmptySeriesRecording(seriesRecordingIdsToCheck);
        removeDeletedSchedules(schedules);
    }

    @Override
    public void updateSeriesRecording(final SeriesRecording... seriesRecordings) {
        for (SeriesRecording r : seriesRecordings) {
            if (!SoftPreconditions.checkArgument(
                    mSeriesRecordings.containsKey(r.getId()),
                    TAG,
                    "Non Existing Series ID: %s",
                    r)) {
                continue;
            }
            SeriesRecording old1 = mSeriesRecordings.put(r.getId(), r);
            SeriesRecording old2 = mSeriesId2SeriesRecordings.put(r.getSeriesId(), r);
            SoftPreconditions.checkArgument(
                    old1.equals(old2), TAG, "Series ID cannot be updated: %s", r);
        }
        if (mDvrLoadFinished) {
            notifySeriesRecordingChanged(seriesRecordings);
        }
        ListenableFuture updateSeriesRecordingFuture =
                new UpdateSeriesRecordingFuture(mDbHelper)
                        .executeOnDbThread(removeFromSetOnCompletion, seriesRecordings);
        mNoStopFuture.add(updateSeriesRecordingFuture);
    }

    private boolean isInputAvailable(String inputId) {
        return mInputManager.hasTvInputInfo(inputId)
                && (!CommonUtils.isBundledInput(inputId)
                        || mStorageStatusManager.isStorageMounted());
    }

    private void removeDeletedSchedules(ScheduledRecording... addedSchedules) {
        List<ScheduledRecording> schedulesToDelete = new ArrayList<>();
        for (ScheduledRecording r : addedSchedules) {
            ScheduledRecording deleted = getDeletedScheduleMap().remove(r.getProgramId());
            if (deleted != null) {
                schedulesToDelete.add(deleted);
            }
        }
        if (!schedulesToDelete.isEmpty()) {
            ListenableFuture deleteScheduleFuture =
                    new DeleteScheduleFuture(mDbHelper)
                            .executeOnDbThread(
                                    removeFromSetOnCompletion,
                                    ScheduledRecording.toArray(schedulesToDelete));
            mNoStopFuture.add(deleteScheduleFuture);
        }
    }

    private void removeDeletedSchedules(SeriesRecording... removedSeriesRecordings) {
        Set<Long> seriesRecordingIds = new HashSet<>();
        for (SeriesRecording r : removedSeriesRecordings) {
            seriesRecordingIds.add(r.getId());
        }
        List<ScheduledRecording> schedulesToDelete = new ArrayList<>();
        Iterator<Entry<Long, ScheduledRecording>> iter =
                getDeletedScheduleMap().entrySet().iterator();
        while (iter.hasNext()) {
            Entry<Long, ScheduledRecording> entry = iter.next();
            if (seriesRecordingIds.contains(entry.getValue().getSeriesRecordingId())) {
                schedulesToDelete.add(entry.getValue());
                iter.remove();
            }
        }
        if (!schedulesToDelete.isEmpty()) {
            ListenableFuture deleteScheduleFuture =
                    new DeleteScheduleFuture(mDbHelper)
                            .executeOnDbThread(
                                    removeFromSetOnCompletion,
                                    ScheduledRecording.toArray(schedulesToDelete));
            mNoStopFuture.add(deleteScheduleFuture);
        }
    }

    private void unhideInput(String inputId) {
        if (DEBUG) Log.d(TAG, "unhideInput " + inputId);
        List<ScheduledRecording> movedSchedules =
                moveElements(
                        mScheduledRecordingsForRemovedInput,
                        mScheduledRecordings,
                        r -> r.getInputId().equals(inputId));
        List<RecordedProgram> movedRecordedPrograms =
                moveElements(
                        mRecordedProgramsForRemovedInput,
                        mRecordedPrograms,
                        r -> r.getInputId().equals(inputId));
        List<SeriesRecording> removedSeriesRecordings = new ArrayList<>();
        List<SeriesRecording> movedSeriesRecordings =
                moveElements(
                        mSeriesRecordingsForRemovedInput,
                        mSeriesRecordings,
                        r -> {
                            if (r.getInputId().equals(inputId)) {
                                if (!isEmptySeriesRecording(r)) {
                                    return true;
                                }
                                removedSeriesRecordings.add(r);
                            }
                            return false;
                        });
        if (!movedSchedules.isEmpty()) {
            for (ScheduledRecording schedule : movedSchedules) {
                mProgramId2ScheduledRecordings.put(schedule.getProgramId(), schedule);
            }
        }
        if (!movedSeriesRecordings.isEmpty()) {
            for (SeriesRecording seriesRecording : movedSeriesRecordings) {
                mSeriesId2SeriesRecordings.put(seriesRecording.getSeriesId(), seriesRecording);
            }
        }
        for (SeriesRecording r : removedSeriesRecordings) {
            mSeriesRecordingsForRemovedInput.remove(r.getId());
        }
        ListenableFuture deleteSeriesRecordingFuture =
                new DeleteSeriesRecordingFuture(mDbHelper)
                        .executeOnDbThread(
                                removeFromSetOnCompletion,
                                SeriesRecording.toArray(removedSeriesRecordings));
        mNoStopFuture.add(deleteSeriesRecordingFuture);
        // Notify after all the data are moved.
        if (!movedSchedules.isEmpty()) {
            notifyScheduledRecordingAdded(ScheduledRecording.toArray(movedSchedules));
        }
        if (!movedSeriesRecordings.isEmpty()) {
            notifySeriesRecordingAdded(SeriesRecording.toArray(movedSeriesRecordings));
        }
        if (!movedRecordedPrograms.isEmpty()) {
            notifyRecordedProgramsAdded(RecordedProgram.toArray(movedRecordedPrograms));
        }
    }

    private void hideInput(String inputId) {
        if (DEBUG) Log.d(TAG, "hideInput " + inputId);
        List<ScheduledRecording> movedSchedules =
                moveElements(
                        mScheduledRecordings,
                        mScheduledRecordingsForRemovedInput,
                        r -> r.getInputId().equals(inputId));
        List<SeriesRecording> movedSeriesRecordings =
                moveElements(
                        mSeriesRecordings,
                        mSeriesRecordingsForRemovedInput,
                        r -> r.getInputId().equals(inputId));
        List<RecordedProgram> movedRecordedPrograms =
                moveElements(
                        mRecordedPrograms,
                        mRecordedProgramsForRemovedInput,
                        r -> r.getInputId().equals(inputId));
        if (!movedSchedules.isEmpty()) {
            for (ScheduledRecording schedule : movedSchedules) {
                mProgramId2ScheduledRecordings.remove(schedule.getProgramId());
            }
        }
        if (!movedSeriesRecordings.isEmpty()) {
            for (SeriesRecording seriesRecording : movedSeriesRecordings) {
                mSeriesId2SeriesRecordings.remove(seriesRecording.getSeriesId());
            }
        }
        // Notify after all the data are moved.
        if (!movedSchedules.isEmpty()) {
            notifyScheduledRecordingRemoved(ScheduledRecording.toArray(movedSchedules));
        }
        if (!movedSeriesRecordings.isEmpty()) {
            notifySeriesRecordingRemoved(SeriesRecording.toArray(movedSeriesRecordings));
        }
        if (!movedRecordedPrograms.isEmpty()) {
            notifyRecordedProgramsRemoved(RecordedProgram.toArray(movedRecordedPrograms));
        }
    }

    private void checkAndRemoveEmptySeriesRecording(Set<Long> seriesRecordingIds) {
        int i = 0;
        long[] rIds = new long[seriesRecordingIds.size()];
        for (long rId : seriesRecordingIds) {
            rIds[i++] = rId;
        }
        checkAndRemoveEmptySeriesRecording(rIds);
    }

    @Override
    public void forgetStorage(String inputId) {
        List<ScheduledRecording> schedulesToDelete = new ArrayList<>();
        for (Iterator<ScheduledRecording> i =
                        mScheduledRecordingsForRemovedInput.values().iterator();
                i.hasNext(); ) {
            ScheduledRecording r = i.next();
            if (inputId.equals(r.getInputId())) {
                schedulesToDelete.add(r);
                i.remove();
            }
        }
        List<SeriesRecording> seriesRecordingsToDelete = new ArrayList<>();
        for (Iterator<SeriesRecording> i = mSeriesRecordingsForRemovedInput.values().iterator();
                i.hasNext(); ) {
            SeriesRecording r = i.next();
            if (inputId.equals(r.getInputId())) {
                seriesRecordingsToDelete.add(r);
                i.remove();
            }
        }
        for (Iterator<RecordedProgram> i = mRecordedProgramsForRemovedInput.values().iterator();
                i.hasNext(); ) {
            if (inputId.equals(i.next().getInputId())) {
                i.remove();
            }
        }
        ListenableFuture deleteScheduleFuture =
                new DeleteScheduleFuture(mDbHelper)
                        .executeOnDbThread(
                                removeFromSetOnCompletion,
                                ScheduledRecording.toArray(schedulesToDelete));
        mNoStopFuture.add(deleteScheduleFuture);
        ListenableFuture deleteSeriesRecordingFuture =
                new DeleteSeriesRecordingFuture(mDbHelper)
                        .executeOnDbThread(
                                removeFromSetOnCompletion,
                                SeriesRecording.toArray(seriesRecordingsToDelete));
        mNoStopFuture.add(deleteSeriesRecordingFuture);
        new AsyncDbTask<Void, Void, Void>(mDbExecutor) {
            @Override
            protected Void doInBackground(Void... params) {
                ContentResolver resolver = mContext.getContentResolver();
                String[] args = {inputId};
                try {
                    resolver.delete(
                            RecordedPrograms.CONTENT_URI,
                            RecordedPrograms.COLUMN_INPUT_ID + " = ?",
                            args);
                } catch (SQLiteException e) {
                    Log.e(TAG, "Failed to delete recorded programs for inputId: " + inputId, e);
                }
                return null;
            }
        }.executeOnDbThread();
    }

    private void validateSeriesRecordings() {
        Iterator<SeriesRecording> iter = mSeriesRecordings.values().iterator();
        List<SeriesRecording> removedSeriesRecordings = new ArrayList<>();
        while (iter.hasNext()) {
            SeriesRecording r = iter.next();
            if (isEmptySeriesRecording(r)) {
                iter.remove();
                removedSeriesRecordings.add(r);
            }
        }
        if (!removedSeriesRecordings.isEmpty()) {
            SeriesRecording[] removed = SeriesRecording.toArray(removedSeriesRecordings);
            ListenableFuture deleteSeriesRecordingFuture =
                    new DeleteSeriesRecordingFuture(mDbHelper)
                            .executeOnDbThread(removeFromSetOnCompletion, removed);
            mNoStopFuture.add(deleteSeriesRecordingFuture);
            if (mDvrLoadFinished) {
                notifySeriesRecordingRemoved(removed);
            }
        }
    }

    private final class RecordedProgramsQueryTask extends AsyncRecordedProgramQueryTask {
        private final Uri mUri;

        public RecordedProgramsQueryTask(Uri uri) {
            super(mDbExecutor, mContext, uri == null ? RecordedPrograms.CONTENT_URI : uri);
            mUri = uri;
        }

        @Override
        protected void onCancelled(List<RecordedProgram> scheduledRecordings) {
            mPendingTasks.remove(this);
        }

        @Override
        protected void onPostExecute(List<RecordedProgram> result) {
            mPendingTasks.remove(this);
            onRecordedProgramsLoadedFinished(mUri, result);
        }
    }
}
