/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.tv.settings.system.development.audio;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.Optional;

/** Stores audio recording metrics. Sends metrics updates through a callback. */
public class AudioMetrics {

    private static final String TAG = "AudioMetrics";

    private Optional<Long> mStartTs = Optional.empty();

    private Data mData = new Data();

    private final UpdateMetricsCallback mCallback;

    /** Contains data to be exposed via the callback. */
    public static class Data {

        public Optional<Long> timeToStartReadMs = Optional.empty();
        public Optional<Long> timeToValidAudioMs = Optional.empty();
        public Optional<Long> emptyAudioDurationMs = Optional.empty();

        public Data() {
        }

        public Data(Data data) {
            this.timeToStartReadMs = data.timeToStartReadMs;
            this.timeToValidAudioMs = data.timeToValidAudioMs;
            this.emptyAudioDurationMs = data.emptyAudioDurationMs;
        }
    }

    /** Interface for receiving metrics updates. */
    public interface UpdateMetricsCallback {
        /** Callback for receiving metrics updates. */
        void onUpdateMetrics(AudioMetrics.Data data);
    }

    /**
     * @param callback Callback for updates.
     */
    public AudioMetrics(UpdateMetricsCallback callback) {
        this.mCallback = callback;
    }

    /** Records the beginning of the audio recording process. */
    public void start() {
        mData = new Data();
        mStartTs = Optional.of(System.currentTimeMillis());
        updateMetrics();
    }

    /** Records that we have started monitoring a buffer for incoming audio data. */
    public void startedReading() throws IllegalStateException {
        long startTs = this.mStartTs
                .orElseThrow(() -> new IllegalStateException(
                        "Started reading before recording started"));
        long currentTs = System.currentTimeMillis();
        mData = new Data(mData);
        mData.timeToStartReadMs = Optional.of(currentTs - startTs);
        updateMetrics();
    }

    /** Records that we have started receiving non-zero audio data */
    public void receivedValidAudio() throws IllegalStateException {
        long startTs = this.mStartTs
                .orElseThrow(() -> new IllegalStateException(
                        "Received audio data before recording started"));
        long currentTs = System.currentTimeMillis();
        mData = new Data(mData);
        mData.timeToValidAudioMs = Optional.of(currentTs - startTs);
        updateMetrics();
    }

    /** Records the duration of empty audio received before valid audio */
    public void setEmptyAudioDurationMs(long emptyAudioMs) {
        mData = new Data(mData);
        mData.emptyAudioDurationMs = Optional.of(emptyAudioMs);
        updateMetrics();
    }

    /** Sends updated data through the callback */
    private void updateMetrics() {
        Handler mainHandler = new Handler(Looper.getMainLooper());
        mainHandler.post(() -> mCallback.onUpdateMetrics(mData));
        Log.i(TAG, String.format("Time to start reading: %s",
                msTimestampToString(mData.timeToStartReadMs)));
        Log.i(TAG, String.format("Time to valid audio data: %s",
                msTimestampToString(mData.timeToValidAudioMs)));
        Log.i(TAG, String.format("Empty audio duration: %s",
                msTimestampToString(mData.emptyAudioDurationMs)));
    }

    /** Converts a possible timestamp in milliseconds to its string representation. */
    public static String msTimestampToString(Optional<Long> optL) {
        return optL.map((Long l) -> String.format("%s ms", l)).orElse("");
    }
}

