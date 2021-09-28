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

package com.android.tv.tuner.tvinput;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.media.tv.TvInputService;
import android.net.Uri;
import android.util.Log;

import com.android.tv.common.feature.CommonFeatures;
import com.android.tv.tuner.tvinput.datamanager.ChannelDataManager;
import com.android.tv.tuner.tvinput.factory.TunerRecordingSessionFactory;
import com.android.tv.tuner.tvinput.factory.TunerSessionFactory;

import com.google.common.cache.CacheBuilder;
import com.google.common.cache.CacheLoader;
import com.google.common.cache.LoadingCache;
import com.google.common.cache.RemovalListener;

import dagger.android.AndroidInjection;

import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;
import java.util.concurrent.TimeUnit;

import javax.inject.Inject;

/** {@link BaseTunerTvInputService} serves TV channels coming from a tuner device. */
public class BaseTunerTvInputService extends TvInputService {
    private static final String TAG = "BaseTunerTvInputService";
    private static final boolean DEBUG = false;

    private static final int DVR_STORAGE_CLEANUP_JOB_ID = 100;

    private final Set<Session> mTunerSessions = Collections.newSetFromMap(new WeakHashMap<>());
    private final Set<RecordingSession> mTunerRecordingSession =
            Collections.newSetFromMap(new WeakHashMap<>());
    @Inject TunerSessionFactory mTunerSessionFactory;
    @Inject TunerRecordingSessionFactory mTunerRecordingSessionFactory;

    LoadingCache<String, ChannelDataManager> mChannelDataManagers;
    RemovalListener<String, ChannelDataManager> mChannelDataManagerRemovalListener =
            notification -> {
                ChannelDataManager cdm = notification.getValue();
                if (cdm != null) {
                    cdm.release();
                }
            };

    @Override
    public void onCreate() {
        if (getApplicationContext().getSystemService(Context.TV_INPUT_SERVICE) == null) {
            Log.wtf(TAG, "Stopping because device does not have a TvInputManager");
            this.stopSelf();
            return;
        }
        AndroidInjection.inject(this);
        super.onCreate();
        if (DEBUG) Log.d(TAG, "onCreate");
        mChannelDataManagers =
                CacheBuilder.newBuilder()
                        .weakValues()
                        .removalListener(mChannelDataManagerRemovalListener)
                        .build(
                                new CacheLoader<String, ChannelDataManager>() {
                                    @Override
                                    public ChannelDataManager load(String inputId) {
                                        return createChannelDataManager(inputId);
                                    }
                                });
        if (CommonFeatures.DVR.isEnabled(this)) {
            JobScheduler jobScheduler =
                    (JobScheduler) getSystemService(Context.JOB_SCHEDULER_SERVICE);
            JobInfo pendingJob = jobScheduler.getPendingJob(DVR_STORAGE_CLEANUP_JOB_ID);
            if (pendingJob != null) {
                // storage cleaning job is already scheduled.
            } else {
                JobInfo job =
                        new JobInfo.Builder(
                                        DVR_STORAGE_CLEANUP_JOB_ID,
                                        new ComponentName(this, TunerStorageCleanUpService.class))
                                .setPersisted(true)
                                .setPeriodic(TimeUnit.DAYS.toMillis(1))
                                .build();
                jobScheduler.schedule(job);
            }
        }
    }

    private ChannelDataManager createChannelDataManager(String inputId) {
        return new ChannelDataManager(getApplicationContext(), inputId);
    }

    @Override
    public void onDestroy() {
        if (DEBUG) Log.d(TAG, "onDestroy");
        super.onDestroy();
        mChannelDataManagers.invalidateAll();
    }

    @Override
    public RecordingSession onCreateRecordingSession(String inputId) {
        RecordingSession session =
                mTunerRecordingSessionFactory.create(
                        inputId, this::onReleased, mChannelDataManagers.getUnchecked(inputId));
        mTunerRecordingSession.add(session);
        return session;
    }

    @Override
    public Session onCreateSession(String inputId) {
        if (DEBUG) Log.d(TAG, "onCreateSession");
        try {
            // TODO(b/65445352): Support multiple TunerSessions for multiple tuners
            if (!mTunerSessions.isEmpty()) {
                Log.d(TAG, "abort creating an session");
                return null;
            }

            final Session session =
                    mTunerSessionFactory.create(
                            mChannelDataManagers.getUnchecked(inputId),
                            this::onReleased,
                            this::getRecordingUri);
            mTunerSessions.add(session);
            session.setOverlayViewEnabled(true);
            return session;
        } catch (RuntimeException e) {
            // There are no available DVB devices.
            Log.e(TAG, "Creating a session for " + inputId + " failed.", e);
            return null;
        }
    }

    private Uri getRecordingUri(Uri channelUri) {
        for (RecordingSession session : mTunerRecordingSession) {
            TunerRecordingSession tunerSession = (TunerRecordingSession) session;
            if (tunerSession.getChannelUri().equals(channelUri)) {
                return tunerSession.getRecordingUri();
            }
        }
        return null;
    }

    private void onReleased(Session session) {
        mTunerSessions.remove(session);
        mChannelDataManagers.cleanUp();
    }

    private void onReleased(RecordingSession session) {
        mTunerRecordingSession.remove(session);
    }
}
