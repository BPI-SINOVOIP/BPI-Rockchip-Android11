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

import android.annotation.Nullable;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Handler;
import android.util.Log;

import java.nio.ShortBuffer;

/** Manages audio recording, audio metrics, and audio playback for debugging purposes. */
public class AudioDebug {

    private static final String TAG = "AudioDebug";

    public static final int CHANNELS = 1;
    public static final int SAMPLE_RATE = 16000;
    public static final int BITRATE = 16;
    public static final int ENCODING = AudioFormat.ENCODING_PCM_16BIT;

    private final Context mContext;

    private final AudioRecordedCallback mAudioRecordedCallback;

    private final AudioMetrics mMetrics;

    @Nullable
    private AudioReader mAudioReader;

    @Nullable
    private ShortBuffer mAudioBuffer;

    @Nullable
    private AudioTrack mAudioTrack;

    /** Interface for receiving a notification when audio recording finishes. */
    public interface AudioRecordedCallback {
        /** Callback for receiving a notification when audio recording finishes. */
        void onAudioRecorded(boolean successful);
    }

    /**
     * @param context               The parent context
     * @param audioRecordedCallback Callback for notification on audio recording completion
     * @param metricsCallback       Callback for metrics updates
     */
    public AudioDebug(Context context, AudioRecordedCallback audioRecordedCallback,
            AudioMetrics.UpdateMetricsCallback metricsCallback) {
        this.mContext = context;
        this.mAudioRecordedCallback = audioRecordedCallback;

        mMetrics = new AudioMetrics(metricsCallback);
    }

    /** Starts recording audio. */
    public void startRecording() throws AudioReaderException {
        if (mAudioReader != null) {
            mAudioReader.stop();
        }

        mMetrics.start();

        mAudioReader = new AudioReader(mMetrics);
        mAudioReader.addListener((ShortBuffer buffer) -> onAudioRecorded(buffer));

        Thread audioReaderThread = new Thread(mAudioReader);
        audioReaderThread.setPriority(10);
        audioReaderThread.start();
    }

    /**
     * Stores a buffer containing recorded audio in an AudioTrack. Overwrites any previously
     * recorded audio.
     *
     * @param audioBuffer The buffer containing the recorded audio.
     */
    private void onAudioRecorded(ShortBuffer audioBuffer) {
        if (audioBuffer.position() == 0) {
            Log.e(TAG, "Empty buffer recorded");
            return;
        }

        this.mAudioBuffer = audioBuffer;

        int numShorts = audioBuffer.position();
        int numBytes = numShorts * 2;

        Handler mainHandler = new Handler(mContext.getMainLooper());

        try {
            mAudioTrack =
                    new AudioTrack.Builder()
                            .setAudioFormat(
                                    new AudioFormat.Builder()
                                            .setSampleRate(SAMPLE_RATE)
                                            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                                            .setEncoding(ENCODING)
                                            .build()
                            )
                            .setTransferMode(AudioTrack.MODE_STATIC)
                            .setBufferSizeInBytes(numBytes)
                            .build();
        } catch (UnsupportedOperationException | IllegalArgumentException e) {
            Log.e(TAG, "Failed to create AudioTrack", e);
            mainHandler.post(() -> mAudioRecordedCallback.onAudioRecorded(false));
            return;
        }

        Log.i(TAG, String.format("AudioTrack state: %d", mAudioTrack.getState()));

        int writeStatus = mAudioTrack.write(audioBuffer.array(), 0, numShorts,
                AudioTrack.WRITE_BLOCKING);
        if (writeStatus > 0) {
            Log.i(TAG, String.format("Wrote %d bytes to an AudioTrack", numBytes));
            mainHandler.post(() -> mAudioRecordedCallback.onAudioRecorded(true));
        } else if (writeStatus == 0) {
            Log.e(TAG, "Received empty audio buffer");
        } else {
            Log.e(TAG, String.format("Error calling AudioTrack.write(): %d", writeStatus));
            mainHandler.post(() -> mAudioRecordedCallback.onAudioRecorded(false));
        }
    }


    /** Stops recording audio. */
    public void stopRecording() {
        if (mAudioReader != null) {
            mAudioReader.stop();
            mAudioReader = null;
        }
    }

    /** Stops recording audio, and discards the recorded audio. */
    public void cancelRecording() {
        if (mAudioReader != null) {
            mAudioReader.cancel();
            mAudioReader = null;
        }
    }


    /** Plays the recorded audio. */
    public void playAudio() {
        if (mAudioTrack == null) {
            Log.e(TAG, "No audio track recorded");
        } else {
            mAudioTrack.stop();
            mAudioTrack.reloadStaticData();
            mAudioTrack.setPlaybackHeadPosition(0);
            mAudioTrack.setVolume(1.0f);
            mAudioTrack.play();
        }
    }

    /** Writes the recorded audio to a WAV file. */
    public void writeAudioToFile() {
        WavWriter.writeToFile(mContext.getExternalFilesDir(null), mAudioBuffer);
    }
}
