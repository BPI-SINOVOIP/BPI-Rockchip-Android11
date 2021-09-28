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

package com.android.tv.data;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.media.tv.TvContract;
import android.media.tv.TvContract.Programs;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.support.annotation.AnyThread;
import android.support.annotation.MainThread;
import android.support.annotation.VisibleForTesting;
import android.util.ArraySet;
import android.util.Log;
import android.util.LongSparseArray;
import android.util.LruCache;

import com.android.tv.TvSingletons;
import com.android.tv.common.SoftPreconditions;
import com.android.tv.common.memory.MemoryManageable;
import com.android.tv.common.util.Clock;
import com.android.tv.data.api.Channel;
import com.android.tv.data.api.Program;
import com.android.tv.perf.EventNames;
import com.android.tv.perf.PerformanceMonitor;
import com.android.tv.perf.TimerEvent;
import com.android.tv.util.AsyncDbTask;
import com.android.tv.util.MultiLongSparseArray;
import com.android.tv.util.TvInputManagerHelper;
import com.android.tv.util.TvProviderUtils;
import com.android.tv.util.Utils;

import com.android.tv.common.flags.BackendKnobsFlags;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

@MainThread
public class ProgramDataManager implements MemoryManageable {
    private static final String TAG = "ProgramDataManager";
    private static final boolean DEBUG = false;

    // To prevent from too many program update operations at the same time, we give random interval
    // between PERIODIC_PROGRAM_UPDATE_MIN_MS and PERIODIC_PROGRAM_UPDATE_MAX_MS.
    @VisibleForTesting
    static final long PERIODIC_PROGRAM_UPDATE_MIN_MS = TimeUnit.MINUTES.toMillis(5);

    private static final long PERIODIC_PROGRAM_UPDATE_MAX_MS = TimeUnit.MINUTES.toMillis(10);
    private static final long PROGRAM_PREFETCH_UPDATE_WAIT_MS = TimeUnit.SECONDS.toMillis(5);
    // TODO: need to optimize consecutive DB updates.
    private static final long CURRENT_PROGRAM_UPDATE_WAIT_MS = TimeUnit.SECONDS.toMillis(5);
    @VisibleForTesting static final long PROGRAM_GUIDE_SNAP_TIME_MS = TimeUnit.MINUTES.toMillis(30);

    // Default fetch hours
    private static final long FETCH_HOURS_MS = TimeUnit.HOURS.toMillis(24);
    // Load data earlier for smooth scrolling.
    private static final long BUFFER_HOURS_MS = TimeUnit.HOURS.toMillis(6);

    // TODO: Use TvContract constants, once they become public.
    private static final String PARAM_START_TIME = "start_time";
    private static final String PARAM_END_TIME = "end_time";
    // COLUMN_CHANNEL_ID, COLUMN_END_TIME_UTC_MILLIS are added to detect duplicated programs.
    // Duplicated programs are always consecutive by the sorting order.
    private static final String SORT_BY_TIME =
            Programs.COLUMN_START_TIME_UTC_MILLIS
                    + ", "
                    + Programs.COLUMN_CHANNEL_ID
                    + ", "
                    + Programs.COLUMN_END_TIME_UTC_MILLIS;
    private static final String SORT_BY_CHANNEL_ID =
            Programs.COLUMN_CHANNEL_ID
                    + ", "
                    + Programs.COLUMN_START_TIME_UTC_MILLIS
                    + " DESC, "
                    + Programs.COLUMN_END_TIME_UTC_MILLIS
                    + " ASC, "
                    + Programs._ID
                    + " DESC";

    private static final int MSG_UPDATE_CURRENT_PROGRAMS = 1000;
    private static final int MSG_UPDATE_ONE_CURRENT_PROGRAM = 1001;
    private static final int MSG_UPDATE_PREFETCH_PROGRAM = 1002;
    private static final int MSG_UPDATE_CONTENT_RATINGS = 1003;

    private final Context mContext;
    private final Clock mClock;
    private final ContentResolver mContentResolver;
    private final Executor mDbExecutor;
    private final BackendKnobsFlags mBackendKnobsFlags;
    private final PerformanceMonitor mPerformanceMonitor;
    private final ChannelDataManager mChannelDataManager;
    private final TvInputManagerHelper mTvInputManagerHelper;
    private boolean mStarted;
    // Updated only on the main thread.
    private volatile boolean mCurrentProgramsLoadFinished;
    private ProgramsUpdateTask mProgramsUpdateTask;
    private final LongSparseArray<UpdateCurrentProgramForChannelTask> mProgramUpdateTaskMap =
            new LongSparseArray<>();
    private final Map<Long, Program> mChannelIdCurrentProgramMap = new ConcurrentHashMap<>();
    private final MultiLongSparseArray<OnCurrentProgramUpdatedListener>
            mChannelId2ProgramUpdatedListeners = new MultiLongSparseArray<>();
    private final Handler mHandler;
    private final Set<Callback> mCallbacks = new ArraySet<>();
    private Map<Long, ArrayList<Program>> mChannelIdProgramCache = new ConcurrentHashMap<>();
    private final Set<Long> mCompleteInfoChannelIds = new HashSet<>();
    private final ContentObserver mProgramObserver;

    private boolean mPrefetchEnabled;
    private long mProgramPrefetchUpdateWaitMs;
    private long mLastPrefetchTaskRunMs;
    private ProgramsPrefetchTask mProgramsPrefetchTask;

    // Any program that ends prior to this time will be removed from the cache
    // when a channel's current program is updated.
    // Note that there's no limit for end time.
    private long mPrefetchTimeRangeStartMs;

    private boolean mPauseProgramUpdate = false;
    private final LruCache<Long, Program> mZeroLengthProgramCache = new LruCache<>(10);
    // Current tuned channel.
    private long mTunedChannelId;
    // Hours of data to be fetched, it is updated during horizontal scroll.
    // Note that it should never exceed programGuideMaxHours.
    private long mMaxFetchHoursMs = FETCH_HOURS_MS;

    @MainThread
    public ProgramDataManager(Context context) {
        this(
                context,
                TvSingletons.getSingletons(context).getDbExecutor(),
                context.getContentResolver(),
                Clock.SYSTEM,
                Looper.myLooper(),
                TvSingletons.getSingletons(context).getBackendKnobs(),
                TvSingletons.getSingletons(context).getPerformanceMonitor(),
                TvSingletons.getSingletons(context).getChannelDataManager(),
                TvSingletons.getSingletons(context).getTvInputManagerHelper());
    }

    @VisibleForTesting
    ProgramDataManager(
            Context context,
            Executor executor,
            ContentResolver contentResolver,
            Clock time,
            Looper looper,
            BackendKnobsFlags backendKnobsFlags,
            PerformanceMonitor performanceMonitor,
            ChannelDataManager channelDataManager,
            TvInputManagerHelper tvInputManagerHelper) {
        mContext = context;
        mDbExecutor = executor;
        mClock = time;
        mContentResolver = contentResolver;
        mHandler = new MyHandler(looper);
        mBackendKnobsFlags = backendKnobsFlags;
        mPerformanceMonitor = performanceMonitor;
        mChannelDataManager = channelDataManager;
        mTvInputManagerHelper = tvInputManagerHelper;
        mProgramObserver =
                new ContentObserver(mHandler) {
                    @Override
                    public void onChange(boolean selfChange) {
                        if (!mHandler.hasMessages(MSG_UPDATE_CURRENT_PROGRAMS)) {
                            mHandler.sendEmptyMessage(MSG_UPDATE_CURRENT_PROGRAMS);
                        }
                        if (isProgramUpdatePaused()) {
                            return;
                        }
                        if (mPrefetchEnabled) {
                            // The delay time of an existing MSG_UPDATE_PREFETCH_PROGRAM could be
                            // quite long
                            // up to PROGRAM_GUIDE_SNAP_TIME_MS. So we need to remove the existing
                            // message
                            // and send MSG_UPDATE_PREFETCH_PROGRAM again.
                            mHandler.removeMessages(MSG_UPDATE_PREFETCH_PROGRAM);
                            mHandler.sendEmptyMessage(MSG_UPDATE_PREFETCH_PROGRAM);
                        }
                    }
                };
        mProgramPrefetchUpdateWaitMs = PROGRAM_PREFETCH_UPDATE_WAIT_MS;
    }

    @VisibleForTesting
    ContentObserver getContentObserver() {
        return mProgramObserver;
    }

    /**
     * Set the program prefetch update wait which gives the delay to query all programs from DB to
     * prevent from too frequent DB queries. Default value is {@link
     * #PROGRAM_PREFETCH_UPDATE_WAIT_MS}
     */
    @VisibleForTesting
    void setProgramPrefetchUpdateWait(long programPrefetchUpdateWaitMs) {
        mProgramPrefetchUpdateWaitMs = programPrefetchUpdateWaitMs;
    }

    /** Starts the manager. */
    public void start() {
        if (mStarted) {
            return;
        }
        mStarted = true;
        // Should be called directly instead of posting MSG_UPDATE_CURRENT_PROGRAMS message
        // to the handler. If not, another DB task can be executed before loading current programs.
        handleUpdateCurrentPrograms();
        mHandler.sendEmptyMessage(MSG_UPDATE_CONTENT_RATINGS);
        if (mPrefetchEnabled) {
            mHandler.sendEmptyMessage(MSG_UPDATE_PREFETCH_PROGRAM);
        }
        mContentResolver.registerContentObserver(Programs.CONTENT_URI, true, mProgramObserver);
    }

    /**
     * Stops the manager. It clears manager states and runs pending DB operations. Added listeners
     * aren't automatically removed by this method.
     */
    @VisibleForTesting
    public void stop() {
        if (!mStarted) {
            return;
        }
        mStarted = false;
        mContentResolver.unregisterContentObserver(mProgramObserver);
        mHandler.removeCallbacksAndMessages(null);

        clearTask(mProgramUpdateTaskMap);
        cancelPrefetchTask();
        if (mProgramsUpdateTask != null) {
            mProgramsUpdateTask.cancel(true);
            mProgramsUpdateTask = null;
        }
    }

    @AnyThread
    public boolean isCurrentProgramsLoadFinished() {
        return mCurrentProgramsLoadFinished;
    }

    /** Returns the current program at the specified channel. */
    @AnyThread
    public Program getCurrentProgram(long channelId) {
        return mChannelIdCurrentProgramMap.get(channelId);
    }

    /** Returns all the current programs. */
    @AnyThread
    public List<Program> getCurrentPrograms() {
        return new ArrayList<>(mChannelIdCurrentProgramMap.values());
    }

    /** Reloads program data. */
    public void reload() {
        if (!mHandler.hasMessages(MSG_UPDATE_CURRENT_PROGRAMS)) {
            mHandler.sendEmptyMessage(MSG_UPDATE_CURRENT_PROGRAMS);
        }
        if (mPrefetchEnabled && !mHandler.hasMessages(MSG_UPDATE_PREFETCH_PROGRAM)) {
            mHandler.sendEmptyMessage(MSG_UPDATE_PREFETCH_PROGRAM);
        }
    }

    /**
     * Prefetch program data if needed.
     *
     * @param channelId ID of the channel to prefetch
     * @param selectedProgramIndex index of selected program.
     */
    public void prefetchChannel(long channelId, int selectedProgramIndex) {
        long startTimeMs =
                Utils.floorTime(
                        mClock.currentTimeMillis() - PROGRAM_GUIDE_SNAP_TIME_MS,
                        PROGRAM_GUIDE_SNAP_TIME_MS);
        long programGuideMaxHoursMs =
                TimeUnit.HOURS.toMillis(mBackendKnobsFlags.programGuideMaxHours());
        long endTimeMs = 0;
        if (mMaxFetchHoursMs < programGuideMaxHoursMs
                && isHorizontalLoadNeeded(startTimeMs, channelId, selectedProgramIndex)) {
            // Horizontal scrolling needs to load data of further days.
            mMaxFetchHoursMs = Math.min(programGuideMaxHoursMs, mMaxFetchHoursMs + FETCH_HOURS_MS);
            mCompleteInfoChannelIds.clear();
        }
        // Load max hours complete data for first channel.
        if (mCompleteInfoChannelIds.isEmpty()) {
            endTimeMs = startTimeMs + programGuideMaxHoursMs;
        } else if (!mCompleteInfoChannelIds.contains(channelId)) {
            endTimeMs = startTimeMs + mMaxFetchHoursMs;
        }
        if (endTimeMs > 0) {
            mCompleteInfoChannelIds.add(channelId);
            new SingleChannelPrefetchTask(channelId, startTimeMs, endTimeMs).executeOnDbThread();
        }
    }

    public void prefetchChannel(long channelId) {
        prefetchChannel(channelId, 0);
    }

    /**
     * Check if enough data is present for horizontal scroll, otherwise prefetch programs.
     *
     * <p>If end time of current program is past {@code BUFFER_HOURS_MS} less than the fetched time
     * we need to prefetch proceeding programs.
     *
     * @param startTimeMs Fetch start time, it is used to get fetch end time.
     * @param channelId
     * @param selectedProgramIndex
     * @return {@code true} If data load is needed, else {@code false}.
     */
    private boolean isHorizontalLoadNeeded(
            long startTimeMs, long channelId, int selectedProgramIndex) {
        if (mChannelIdProgramCache.containsKey(channelId)) {
            ArrayList<Program> programs = mChannelIdProgramCache.get(channelId);
            long marginEndTime = startTimeMs + mMaxFetchHoursMs - BUFFER_HOURS_MS;
            return programs.size() > selectedProgramIndex &&
                    programs.get(selectedProgramIndex).getEndTimeUtcMillis() > marginEndTime;
        }
        return false;
    }

    public void onChannelTuned(long channelId) {
        mTunedChannelId = channelId;
        prefetchChannel(channelId);
    }

    /** A Callback interface to receive notification on program data retrieval from DB. */
    public interface Callback {
        /**
         * Called when a Program data is now available through getProgram() after the DB operation
         * is done which wasn't before. This would be called only if fetched data is around the
         * selected program.
         */
        void onProgramUpdated();

        /**
         * Called when we update program data during scrolling. Data is loaded from DB on request
         * basis. It loads data based on horizontal scrolling as well.
         */
        void onChannelUpdated();
    }

    /** Adds the {@link Callback}. */
    public void addCallback(Callback callback) {
        mCallbacks.add(callback);
    }

    /** Removes the {@link Callback}. */
    public void removeCallback(Callback callback) {
        mCallbacks.remove(callback);
    }

    /** Enables or Disables program prefetch. */
    public void setPrefetchEnabled(boolean enable) {
        if (mPrefetchEnabled == enable) {
            return;
        }
        if (enable) {
            mPrefetchEnabled = true;
            mLastPrefetchTaskRunMs = 0;
            if (mStarted) {
                mHandler.sendEmptyMessage(MSG_UPDATE_PREFETCH_PROGRAM);
            }
        } else {
            mPrefetchEnabled = false;
            cancelPrefetchTask();
            clearChannelInfoMap();
            mHandler.removeMessages(MSG_UPDATE_PREFETCH_PROGRAM);
        }
    }

    /**
     * Returns the programs for the given channel which ends after the given start time.
     *
     * <p>Prefetch should be enabled to call it.
     *
     * @return {@link List} with Programs. It may includes dummy program if the entry needs DB
     *     operations to get.
     */
    public List<Program> getPrograms(long channelId, long startTime) {
        SoftPreconditions.checkState(mPrefetchEnabled, TAG, "Prefetch is disabled.");
        ArrayList<Program> cachedPrograms = mChannelIdProgramCache.get(channelId);
        if (cachedPrograms == null) {
            return Collections.emptyList();
        }
        int startIndex = getProgramIndexAt(cachedPrograms, startTime);
        return Collections.unmodifiableList(
                cachedPrograms.subList(startIndex, cachedPrograms.size()));
    }

    /**
     * Returns the index of program that is played at the specified time.
     *
     * <p>If there isn't, return the first program among programs that starts after the given time
     * if returnNextProgram is {@code true}.
     */
    private int getProgramIndexAt(List<Program> programs, long time) {
        Program key = mZeroLengthProgramCache.get(time);
        if (key == null) {
            key = createDummyProgram(time, time);
            mZeroLengthProgramCache.put(time, key);
        }
        int index = Collections.binarySearch(programs, key);
        if (index < 0) {
            index = -(index + 1); // change it to index to be added.
            if (index > 0 && isProgramPlayedAt(programs.get(index - 1), time)) {
                // A program is played at that time.
                return index - 1;
            }
            return index;
        }
        return index;
    }

    private boolean isProgramPlayedAt(Program program, long time) {
        return program.getStartTimeUtcMillis() <= time && time <= program.getEndTimeUtcMillis();
    }

    /**
     * Adds the listener to be notified if current program is updated for a channel.
     *
     * @param channelId A channel ID to get notified. If it's {@link Channel#INVALID_ID}, the
     *     listener would be called whenever a current program is updated.
     */
    public void addOnCurrentProgramUpdatedListener(
            long channelId, OnCurrentProgramUpdatedListener listener) {
        mChannelId2ProgramUpdatedListeners.put(channelId, listener);
    }

    /**
     * Removes the listener previously added by {@link #addOnCurrentProgramUpdatedListener(long,
     * OnCurrentProgramUpdatedListener)}.
     */
    public void removeOnCurrentProgramUpdatedListener(
            long channelId, OnCurrentProgramUpdatedListener listener) {
        mChannelId2ProgramUpdatedListeners.remove(channelId, listener);
    }

    private void notifyCurrentProgramUpdate(long channelId, Program program) {
        for (OnCurrentProgramUpdatedListener listener :
                mChannelId2ProgramUpdatedListeners.get(channelId)) {
            listener.onCurrentProgramUpdated(channelId, program);
        }
        for (OnCurrentProgramUpdatedListener listener :
                mChannelId2ProgramUpdatedListeners.get(Channel.INVALID_ID)) {
            listener.onCurrentProgramUpdated(channelId, program);
        }
    }

    private void updateCurrentProgram(long channelId, Program program) {
        Program previousProgram =
                program == null
                        ? mChannelIdCurrentProgramMap.remove(channelId)
                        : mChannelIdCurrentProgramMap.put(channelId, program);
        if (!Objects.equals(program, previousProgram)) {
            if (mPrefetchEnabled) {
                removePreviousProgramsAndUpdateCurrentProgramInCache(channelId, program);
            }
            notifyCurrentProgramUpdate(channelId, program);
        }

        long delayedTime;
        if (program == null) {
            delayedTime =
                    PERIODIC_PROGRAM_UPDATE_MIN_MS
                            + (long)
                                    (Math.random()
                                            * (PERIODIC_PROGRAM_UPDATE_MAX_MS
                                                    - PERIODIC_PROGRAM_UPDATE_MIN_MS));
        } else {
            delayedTime = program.getEndTimeUtcMillis() - mClock.currentTimeMillis();
        }
        mHandler.sendMessageDelayed(
                mHandler.obtainMessage(MSG_UPDATE_ONE_CURRENT_PROGRAM, channelId), delayedTime);
    }

    private void removePreviousProgramsAndUpdateCurrentProgramInCache(
            long channelId, Program currentProgram) {
        SoftPreconditions.checkState(mPrefetchEnabled, TAG, "Prefetch is disabled.");
        if (!Program.isProgramValid(currentProgram)) {
            return;
        }
        ArrayList<Program> cachedPrograms = mChannelIdProgramCache.remove(channelId);
        if (cachedPrograms == null) {
            return;
        }
        ListIterator<Program> i = cachedPrograms.listIterator();
        while (i.hasNext()) {
            Program cachedProgram = i.next();
            if (cachedProgram.getEndTimeUtcMillis() <= mPrefetchTimeRangeStartMs) {
                // Remove previous programs which will not be shown in program guide.
                i.remove();
                continue;
            }

            if (cachedProgram.getEndTimeUtcMillis() <= currentProgram.getStartTimeUtcMillis()) {
                // Keep the programs that ends earlier than current program
                // but later than mPrefetchTimeRangeStartMs.
                continue;
            }

            // Update dummy program around current program if any.
            if (cachedProgram.getStartTimeUtcMillis() < currentProgram.getStartTimeUtcMillis()) {
                // The dummy program starts earlier than the current program. Adjust its end time.
                i.set(
                        createDummyProgram(
                                cachedProgram.getStartTimeUtcMillis(),
                                currentProgram.getStartTimeUtcMillis()));
                i.add(currentProgram);
            } else {
                i.set(currentProgram);
            }
            if (currentProgram.getEndTimeUtcMillis() < cachedProgram.getEndTimeUtcMillis()) {
                // The dummy program ends later than the current program. Adjust its start time.
                i.add(
                        createDummyProgram(
                                currentProgram.getEndTimeUtcMillis(),
                                cachedProgram.getEndTimeUtcMillis()));
            }
            break;
        }
        if (cachedPrograms.isEmpty()) {
            // If all the cached programs finish before mPrefetchTimeRangeStartMs, the
            // currentProgram would not have a chance to be inserted to the cache.
            cachedPrograms.add(currentProgram);
        }
        mChannelIdProgramCache.put(channelId, cachedPrograms);
    }

    private void handleUpdateCurrentPrograms() {
        if (mProgramsUpdateTask != null) {
            mHandler.sendEmptyMessageDelayed(
                    MSG_UPDATE_CURRENT_PROGRAMS, CURRENT_PROGRAM_UPDATE_WAIT_MS);
            return;
        }
        clearTask(mProgramUpdateTaskMap);
        mHandler.removeMessages(MSG_UPDATE_ONE_CURRENT_PROGRAM);
        mProgramsUpdateTask = new ProgramsUpdateTask(mClock.currentTimeMillis());
        mProgramsUpdateTask.executeOnDbThread();
    }

    private class ProgramsPrefetchTask
            extends AsyncDbTask<Void, Void, Map<Long, ArrayList<Program>>> {
        private final long mStartTimeMs;
        private final long mEndTimeMs;

        private boolean mSuccess;
        private TimerEvent mFromEmptyCacheTimeEvent;

        public ProgramsPrefetchTask() {
            super(mDbExecutor);
            long time = mClock.currentTimeMillis();
            mStartTimeMs =
                    Utils.floorTime(time - PROGRAM_GUIDE_SNAP_TIME_MS, PROGRAM_GUIDE_SNAP_TIME_MS);
            mEndTimeMs = mStartTimeMs + TimeUnit.HOURS.toMillis(getFetchDuration());
            mSuccess = false;
        }

        @Override
        protected void onPreExecute() {
            if (mChannelIdCurrentProgramMap.isEmpty()) {
                // No current program guide is shown.
                // Measure the delay before users can see program guides.
                mFromEmptyCacheTimeEvent = mPerformanceMonitor.startTimer();
            }
        }

        @Override
        protected Map<Long, ArrayList<Program>> doInBackground(Void... params) {
            TimerEvent asyncTimeEvent = mPerformanceMonitor.startTimer();
            Map<Long, ArrayList<Program>> programMap = new HashMap<>();
            if (DEBUG) {
                Log.d(
                        TAG,
                        "Starts programs prefetch. "
                                + Utils.toTimeString(mStartTimeMs)
                                + "-"
                                + Utils.toTimeString(mEndTimeMs));
            }
            Uri uri =
                    Programs.CONTENT_URI
                            .buildUpon()
                            .appendQueryParameter(PARAM_START_TIME, String.valueOf(mStartTimeMs))
                            .appendQueryParameter(PARAM_END_TIME, String.valueOf(mEndTimeMs))
                            .build();
            final int RETRY_COUNT = 3;
            Program lastReadProgram = null;
            for (int retryCount = RETRY_COUNT; retryCount > 0; retryCount--) {
                if (isProgramUpdatePaused()) {
                    return null;
                }
                programMap.clear();

                String[] projection = ProgramImpl.PARTIAL_PROJECTION;
                if (TvProviderUtils.checkSeriesIdColumn(mContext, Programs.CONTENT_URI)) {
                    if (Utils.isProgramsUri(uri)) {
                        projection =
                                TvProviderUtils.addExtraColumnsToProjection(
                                        projection, TvProviderUtils.EXTRA_PROGRAM_COLUMN_SERIES_ID);
                    }
                }
                try (Cursor c = mContentResolver.query(uri, projection, null, null, SORT_BY_TIME)) {
                    if (c == null) {
                        continue;
                    }
                    while (c.moveToNext()) {
                        int duplicateCount = 0;
                        if (isCancelled()) {
                            if (DEBUG) {
                                Log.d(TAG, "ProgramsPrefetchTask canceled.");
                            }
                            return null;
                        }
                        Program program = ProgramImpl.fromCursorPartialProjection(c);
                        if (Program.isDuplicate(program, lastReadProgram)) {
                            duplicateCount++;
                            continue;
                        } else {
                            lastReadProgram = program;
                        }
                        ArrayList<Program> programs = programMap.get(program.getChannelId());
                        if (programs == null) {
                            programs = new ArrayList<>();
                            // To skip already loaded complete data.
                            Program currentProgramInfo =
                                    mChannelIdCurrentProgramMap.get(program.getChannelId());
                            if (currentProgramInfo != null
                                    && Program.isDuplicate(program, currentProgramInfo)) {
                                program = currentProgramInfo;
                            }

                            programMap.put(program.getChannelId(), programs);
                        }
                        programs.add(program);
                        if (duplicateCount > 0) {
                            Log.w(TAG, "Found " + duplicateCount + " duplicate programs");
                        }
                    }
                    mSuccess = true;
                    break;
                } catch (IllegalStateException e) {
                    if (DEBUG) {
                        Log.d(TAG, "Database is changed while querying. Will retry.");
                    }
                } catch (SecurityException e) {
                    Log.w(TAG, "Security exception during program data query", e);
                } catch (Exception e) {
                    Log.w(TAG, "Error during program data query", e);
                }
            }
            if (DEBUG) {
                Log.d(TAG, "Ends programs prefetch for " + programMap.size() + " channels");
            }
            mPerformanceMonitor.stopTimer(
                    asyncTimeEvent,
                    EventNames.PROGRAM_DATA_MANAGER_PROGRAMS_PREFETCH_TASK_DO_IN_BACKGROUND);
            return programMap;
        }

        @Override
        protected void onPostExecute(Map<Long, ArrayList<Program>> programs) {
            mProgramsPrefetchTask = null;
            if (isProgramUpdatePaused()) {
                // ProgramsPrefetchTask will run again once setPauseProgramUpdate(false) is called.
                return;
            }
            long nextMessageDelayedTime;
            if (mSuccess) {
                long currentTime = mClock.currentTimeMillis();
                mLastPrefetchTaskRunMs = currentTime;
                nextMessageDelayedTime =
                        Utils.floorTime(
                                        mLastPrefetchTaskRunMs + PROGRAM_GUIDE_SNAP_TIME_MS,
                                        PROGRAM_GUIDE_SNAP_TIME_MS)
                                - currentTime;
                mChannelIdProgramCache = programs;
                // Since cache has partial data we need to reset the map of complete data.
                clearChannelInfoMap();
                // Get complete projection of tuned channel.
                prefetchChannel(mTunedChannelId);

                notifyProgramUpdated();
                if (mFromEmptyCacheTimeEvent != null) {
                    mPerformanceMonitor.stopTimer(
                            mFromEmptyCacheTimeEvent,
                            EventNames.PROGRAM_GUIDE_SHOW_FROM_EMPTY_CACHE);
                    mFromEmptyCacheTimeEvent = null;
                }
            } else {
                nextMessageDelayedTime = PERIODIC_PROGRAM_UPDATE_MIN_MS;
            }
            if (!mHandler.hasMessages(MSG_UPDATE_PREFETCH_PROGRAM)) {
                mHandler.sendEmptyMessageDelayed(
                        MSG_UPDATE_PREFETCH_PROGRAM, nextMessageDelayedTime);
            }
        }
    }

    private void clearChannelInfoMap() {
        mCompleteInfoChannelIds.clear();
        mMaxFetchHoursMs = FETCH_HOURS_MS;
    }

    private long getFetchDuration() {
        if (mChannelIdProgramCache.isEmpty()) {
            return Math.max(1L, mBackendKnobsFlags.programGuideInitialFetchHours());
        } else {
            long durationHours;
            int channelCount = mChannelDataManager.getChannelCount();
            long knobsMaxHours = mBackendKnobsFlags.programGuideMaxHours();
            long targetChannelCount = mBackendKnobsFlags.epgTargetChannelCount();
            if (channelCount <= targetChannelCount) {
                durationHours = Math.max(48L, knobsMaxHours);
            } else {
                // 2 days <= duration <= 14 days (336 hours)
                durationHours = knobsMaxHours * targetChannelCount / channelCount;
                if (durationHours < 48L) {
                    durationHours = 48L;
                } else if (durationHours > 336L) {
                    durationHours = 336L;
                }
            }
            return durationHours;
        }
    }

    private class SingleChannelPrefetchTask extends AsyncDbTask.AsyncQueryTask<ArrayList<Program>> {
        long mChannelId;

        public SingleChannelPrefetchTask(long channelId, long startTimeMs, long endTimeMs) {
            super(
                    mDbExecutor,
                    mContext,
                    TvContract.buildProgramsUriForChannel(channelId, startTimeMs, endTimeMs),
                    ProgramImpl.PROJECTION,
                    null,
                    null,
                    SORT_BY_TIME);
            mChannelId = channelId;
        }

        @Override
        protected ArrayList<Program> onQuery(Cursor c) {
            ArrayList<Program> programMap = new ArrayList<>();
            while (c.moveToNext()) {
                Program program = ProgramImpl.fromCursor(c);
                programMap.add(program);
            }
            return programMap;
        }

        @Override
        protected void onPostExecute(ArrayList<Program> programs) {
            mChannelIdProgramCache.put(mChannelId, programs);
            notifyChannelUpdated();
        }
    }

    private void notifyProgramUpdated() {
        for (Callback callback : mCallbacks) {
            callback.onProgramUpdated();
        }
    }

    private void notifyChannelUpdated() {
        for (Callback callback : mCallbacks) {
            callback.onChannelUpdated();
        }
    }

    private class ProgramsUpdateTask extends AsyncDbTask.AsyncQueryTask<List<Program>> {
        public ProgramsUpdateTask(long time) {
            super(
                    mDbExecutor,
                    mContext,
                    Programs.CONTENT_URI
                            .buildUpon()
                            .appendQueryParameter(PARAM_START_TIME, String.valueOf(time))
                            .appendQueryParameter(PARAM_END_TIME, String.valueOf(time))
                            .build(),
                    ProgramImpl.PROJECTION,
                    null,
                    null,
                    SORT_BY_CHANNEL_ID);
        }

        @Override
        public List<Program> onQuery(Cursor c) {
            final List<Program> programs = new ArrayList<>();
            if (c != null) {
                int duplicateCount = 0;
                Program lastReadProgram = null;
                while (c.moveToNext()) {
                    if (isCancelled()) {
                        return programs;
                    }
                    Program program = ProgramImpl.fromCursor(c);
                    // Only one program is expected per channel for this query
                    // However, skip overlapping programs from same channel
                    if (Program.sameChannel(program, lastReadProgram)
                            && Program.isOverlapping(program, lastReadProgram)) {
                        duplicateCount++;
                        continue;
                    } else {
                        lastReadProgram = program;
                    }

                    programs.add(program);
                }
                if (duplicateCount > 0) {
                    Log.w(TAG, "Found " + duplicateCount + " overlapping programs");
                }
            }
            return programs;
        }

        @Override
        protected void onPostExecute(List<Program> programs) {
            if (DEBUG) Log.d(TAG, "ProgramsUpdateTask done");
            mProgramsUpdateTask = null;
            if (programs != null) {
                Set<Long> removedChannelIds = new HashSet<>(mChannelIdCurrentProgramMap.keySet());
                for (Program program : programs) {
                    long channelId = program.getChannelId();
                    updateCurrentProgram(channelId, program);
                    removedChannelIds.remove(channelId);
                }
                for (Long channelId : removedChannelIds) {
                    if (mPrefetchEnabled) {
                        mChannelIdProgramCache.remove(channelId);
                        mCompleteInfoChannelIds.remove(channelId);
                    }
                    mChannelIdCurrentProgramMap.remove(channelId);
                    notifyCurrentProgramUpdate(channelId, null);
                }
            }
            mCurrentProgramsLoadFinished = true;
        }
    }

    private class UpdateCurrentProgramForChannelTask extends AsyncDbTask.AsyncQueryTask<Program> {
        private final long mChannelId;

        private UpdateCurrentProgramForChannelTask(long channelId, long time) {
            super(
                    mDbExecutor,
                    mContext,
                    TvContract.buildProgramsUriForChannel(channelId, time, time),
                    ProgramImpl.PROJECTION,
                    null,
                    null,
                    SORT_BY_TIME);
            mChannelId = channelId;
        }

        @Override
        public Program onQuery(Cursor c) {
            Program program = null;
            if (c != null && c.moveToNext()) {
                program = ProgramImpl.fromCursor(c);
            }
            return program;
        }

        @Override
        protected void onPostExecute(Program program) {
            mProgramUpdateTaskMap.remove(mChannelId);
            updateCurrentProgram(mChannelId, program);
        }
    }

    private class MyHandler extends Handler {
        public MyHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_UPDATE_CURRENT_PROGRAMS:
                    handleUpdateCurrentPrograms();
                    break;
                case MSG_UPDATE_ONE_CURRENT_PROGRAM:
                    {
                        long channelId = (Long) msg.obj;
                        UpdateCurrentProgramForChannelTask oldTask =
                                mProgramUpdateTaskMap.get(channelId);
                        if (oldTask != null) {
                            oldTask.cancel(true);
                        }
                        UpdateCurrentProgramForChannelTask task =
                                new UpdateCurrentProgramForChannelTask(
                                        channelId, mClock.currentTimeMillis());
                        mProgramUpdateTaskMap.put(channelId, task);
                        task.executeOnDbThread();
                        break;
                    }
                case MSG_UPDATE_PREFETCH_PROGRAM:
                    {
                        if (isProgramUpdatePaused()) {
                            return;
                        }
                        if (mProgramsPrefetchTask != null) {
                            mHandler.sendEmptyMessageDelayed(
                                    msg.what, mProgramPrefetchUpdateWaitMs);
                            return;
                        }
                        long delayMillis =
                                mLastPrefetchTaskRunMs
                                        + mProgramPrefetchUpdateWaitMs
                                        - mClock.currentTimeMillis();
                        if (delayMillis > 0) {
                            mHandler.sendEmptyMessageDelayed(
                                    MSG_UPDATE_PREFETCH_PROGRAM, delayMillis);
                        } else {
                            mProgramsPrefetchTask = new ProgramsPrefetchTask();
                            mProgramsPrefetchTask.executeOnDbThread();
                        }
                        break;
                    }
                case MSG_UPDATE_CONTENT_RATINGS:
                    mTvInputManagerHelper.getContentRatingsManager().update();
                    break;
                default:
                    // Do nothing
            }
        }
    }

    /**
     * Pause program update. Updating program data will result in UI refresh, but UI is fragile to
     * handle it so we'd better disable it for a while.
     *
     * <p>Prefetch should be enabled to call it.
     */
    public void setPauseProgramUpdate(boolean pauseProgramUpdate) {
        SoftPreconditions.checkState(mPrefetchEnabled, TAG, "Prefetch is disabled.");
        if (mPauseProgramUpdate && !pauseProgramUpdate) {
            if (!mHandler.hasMessages(MSG_UPDATE_PREFETCH_PROGRAM)) {
                // MSG_UPDATE_PRFETCH_PROGRAM can be empty
                // if prefetch task is launched while program update is paused.
                // Update immediately in that case.
                mHandler.sendEmptyMessage(MSG_UPDATE_PREFETCH_PROGRAM);
            }
        }
        mPauseProgramUpdate = pauseProgramUpdate;
    }

    private boolean isProgramUpdatePaused() {
        // Although pause is requested, we need to keep updating if cache is empty.
        return mPauseProgramUpdate && !mChannelIdProgramCache.isEmpty();
    }

    /**
     * Sets program data prefetch time range. Any program data that ends before the start time will
     * be removed from the cache later. Note that there's no limit for end time.
     *
     * <p>Prefetch should be enabled to call it.
     */
    public void setPrefetchTimeRange(long startTimeMs) {
        SoftPreconditions.checkState(mPrefetchEnabled, TAG, "Prefetch is disabled.");
        if (mPrefetchTimeRangeStartMs > startTimeMs) {
            // Fetch the programs immediately to re-create the cache.
            if (!mHandler.hasMessages(MSG_UPDATE_PREFETCH_PROGRAM)) {
                mHandler.sendEmptyMessage(MSG_UPDATE_PREFETCH_PROGRAM);
            }
        }
        mPrefetchTimeRangeStartMs = startTimeMs;
    }

    private void clearTask(LongSparseArray<UpdateCurrentProgramForChannelTask> tasks) {
        for (int i = 0; i < tasks.size(); i++) {
            tasks.valueAt(i).cancel(true);
        }
        tasks.clear();
    }

    private void cancelPrefetchTask() {
        if (mProgramsPrefetchTask != null) {
            mProgramsPrefetchTask.cancel(true);
            mProgramsPrefetchTask = null;
        }
    }

    // Create dummy program which indicates data isn't loaded yet so DB query is required.
    private Program createDummyProgram(long startTimeMs, long endTimeMs) {
        return new ProgramImpl.Builder()
                .setChannelId(Channel.INVALID_ID)
                .setStartTimeUtcMillis(startTimeMs)
                .setEndTimeUtcMillis(endTimeMs)
                .build();
    }

    @Override
    public void performTrimMemory(int level) {
        mChannelId2ProgramUpdatedListeners.clearEmptyCache();
    }
}
