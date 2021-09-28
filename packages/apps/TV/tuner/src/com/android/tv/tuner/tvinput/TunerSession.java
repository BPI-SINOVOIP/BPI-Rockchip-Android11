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

import android.annotation.TargetApi;
import android.content.Context;
import android.media.PlaybackParams;
import android.media.tv.TvContentRating;
import android.media.tv.TvInputManager;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;
import android.view.Surface;
import android.view.View;

import com.android.tv.common.CommonPreferences.CommonPreferencesChangedListener;
import com.android.tv.common.compat.TisSessionCompat;
import com.android.tv.common.dagger.annotations.ApplicationContext;
import com.android.tv.tuner.prefs.TunerPreferences;
import com.android.tv.tuner.tvinput.datamanager.ChannelDataManager;
import com.android.tv.tuner.tvinput.factory.TunerSessionFactory;
import com.android.tv.tuner.tvinput.factory.TunerSessionFactory.SessionRecordingCallback;
import com.android.tv.tuner.tvinput.factory.TunerSessionFactory.SessionReleasedCallback;

import com.google.auto.factory.AutoFactory;
import com.google.auto.factory.Provided;

/**
 * Provides a tuner TV input session. Main tuner input functions are implemented in {@link
 * TunerSessionWorker}.
 */
@AutoFactory(className = "TunerSessionV1Factory", implementing = TunerSessionFactory.class)
public class TunerSession extends TisSessionCompat implements CommonPreferencesChangedListener {

    private static final String TAG = "TunerSession";
    private static final boolean DEBUG = false;

    private final TunerSessionOverlay mTunerSessionOverlay;
    private final TunerSessionWorker mSessionWorker;
    private final SessionReleasedCallback mReleasedCallback;
    private final SessionRecordingCallback mRecordingCallback;
    private boolean mPlayPaused;
    private long mTuneStartTimestamp;

    public TunerSession(
            @Provided @ApplicationContext Context context,
            ChannelDataManager channelDataManager,
            SessionReleasedCallback releasedCallback,
            SessionRecordingCallback recordingCallback,
            @Provided TunerSessionWorker.Factory tunerSessionWorkerFactory,
            @Provided TunerSessionOverlay.Factory tunerSessionOverlayFactory) {
        super(context);
        mReleasedCallback = releasedCallback;
        mRecordingCallback = recordingCallback;
        mTunerSessionOverlay = tunerSessionOverlayFactory.create(context);
        mSessionWorker =
                tunerSessionWorkerFactory.create(
                        context,
                        channelDataManager,
                        this,
                        mTunerSessionOverlay);
        TunerPreferences.setCommonPreferencesChangedListener(this);
    }

    @Override
    public View onCreateOverlayView() {
        return mTunerSessionOverlay.getOverlayView();
    }

    @Override
    public boolean onSelectTrack(int type, String trackId) {
        mSessionWorker.sendMessage(TunerSessionWorker.MSG_SELECT_TRACK, type, 0, trackId);
        return false;
    }

    @Override
    public void onSetCaptionEnabled(boolean enabled) {
        mSessionWorker.setCaptionEnabled(enabled);
    }

    @Override
    public void onSetStreamVolume(float volume) {
        mSessionWorker.setStreamVolume(volume);
    }

    @Override
    public boolean onSetSurface(Surface surface) {
        mSessionWorker.setSurface(surface);
        return true;
    }

    @Override
    public void onTimeShiftPause() {
        mSessionWorker.sendMessage(TunerSessionWorker.MSG_TIMESHIFT_PAUSE);
        mPlayPaused = true;
    }

    @Override
    public void onTimeShiftResume() {
        mSessionWorker.sendMessage(TunerSessionWorker.MSG_TIMESHIFT_RESUME);
        mPlayPaused = false;
    }

    @Override
    public void onTimeShiftSeekTo(long timeMs) {
        if (DEBUG) Log.d(TAG, "Timeshift seekTo requested position: " + timeMs / 1000);
        mSessionWorker.sendMessage(
                TunerSessionWorker.MSG_TIMESHIFT_SEEK_TO, mPlayPaused ? 1 : 0, 0, timeMs);
    }

    @Override
    public void onTimeShiftSetPlaybackParams(PlaybackParams params) {
        mSessionWorker.sendMessage(TunerSessionWorker.MSG_TIMESHIFT_SET_PLAYBACKPARAMS, params);
    }

    @Override
    public long onTimeShiftGetStartPosition() {
        return mSessionWorker.getStartPosition();
    }

    @Override
    public long onTimeShiftGetCurrentPosition() {
        return mSessionWorker.getCurrentPosition();
    }

    @Override
    public boolean onTune(Uri channelUri) {
        if (DEBUG) {
            Log.d(TAG, "onTune to " + channelUri != null ? channelUri.toString() : "");
        }
        if (channelUri == null) {
            Log.w(TAG, "onTune() is failed due to null channelUri.");
            mSessionWorker.stopTune();
            return false;
        }
        mTuneStartTimestamp = SystemClock.elapsedRealtime();
        mSessionWorker.tune(channelUri);
        mPlayPaused = false;
        return true;
    }

    @TargetApi(Build.VERSION_CODES.N)
    @Override
    public void onTimeShiftPlay(Uri recordUri) {
        if (recordUri == null) {
            Log.w(TAG, "onTimeShiftPlay() is failed due to null channelUri.");
            mSessionWorker.stopTune();
            return;
        }
        mTuneStartTimestamp = SystemClock.elapsedRealtime();
        mSessionWorker.tune(recordUri);
        mPlayPaused = false;
    }

    @Override
    public void onUnblockContent(TvContentRating unblockedRating) {
        mSessionWorker.sendMessage(TunerSessionWorker.MSG_UNBLOCKED_RATING, unblockedRating);
    }

    @Override
    public void onRelease() {
        if (DEBUG) {
            Log.d(TAG, "onRelease");
        }
        // The session worker needs to be released before the overlay to ensure no messages are
        // added by the worker after releasing the overlay.
        mSessionWorker.release();
        mTunerSessionOverlay.release();
        TunerPreferences.setCommonPreferencesChangedListener(null);
        mReleasedCallback.onReleased(this);
    }

    @Override
    public void notifyVideoAvailable() {
        super.notifyVideoAvailable();
        if (mTuneStartTimestamp != 0) {
            Log.i(
                    TAG,
                    "[Profiler] Video available in "
                            + (SystemClock.elapsedRealtime() - mTuneStartTimestamp)
                            + " ms");
            mTuneStartTimestamp = 0;
        }
    }

    @Override
    public void notifyVideoUnavailable(int reason) {
        super.notifyVideoUnavailable(reason);
        if (reason != TvInputManager.VIDEO_UNAVAILABLE_REASON_BUFFERING
                && reason != TvInputManager.VIDEO_UNAVAILABLE_REASON_WEAK_SIGNAL) {
            notifyTimeShiftStatusChanged(TvInputManager.TIME_SHIFT_STATUS_UNAVAILABLE);
        }
    }

    @Override
    public void onCommonPreferencesChanged() {
        mSessionWorker.sendMessage(TunerSessionWorker.MSG_TUNER_PREFERENCES_CHANGED);
    }

    public Uri getRecordingUri(Uri channelUri) {
        return mRecordingCallback.getRecordingUri(channelUri);
    }
}
