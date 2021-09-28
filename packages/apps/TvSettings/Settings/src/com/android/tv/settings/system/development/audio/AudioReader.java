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

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import java.nio.ShortBuffer;
import java.util.HashSet;
import java.util.Set;

/** Records audio, and passes it to listeners on completion. */
public class AudioReader implements Runnable {

    private static final String TAG = "AudioReader";

    private static final int BUFFER_SIZE = AudioDebug.SAMPLE_RATE * 40;

    private final Set<Listener> mListeners;

    private final short[] mBuffer;
    private final AudioRecord mAudioRecord;

    private final AudioMetrics mMetrics;
    private final int mMinBufferSize;

    private volatile boolean mActive = true;
    private volatile boolean mCancelled = false;

    /** Interface for receiving recorded audio. */
    public interface Listener {
        /** Callback for receiving recorded audio. */
        void onAudioRecorded(ShortBuffer data);
    }

    /**
     * @param metrics Object for storing metrics.
     */
    public AudioReader(AudioMetrics metrics) throws AudioReaderException {
        this.mMetrics = metrics;

        mMinBufferSize =
                AudioRecord.getMinBufferSize(AudioDebug.SAMPLE_RATE, AudioFormat.CHANNEL_IN_DEFAULT,
                        AudioDebug.ENCODING);
        try {
            mAudioRecord =
                    new AudioRecord.Builder()
                            .setAudioFormat(
                                    new AudioFormat.Builder()
                                            .setSampleRate(AudioDebug.SAMPLE_RATE)
                                            .setEncoding(AudioDebug.ENCODING)
                                            .build())
                            .setAudioSource(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                            .setBufferSizeInBytes(2 * mMinBufferSize)
                            .build();
        } catch (UnsupportedOperationException | IllegalArgumentException e) {
            throw new AudioReaderException(e);
        }

        Log.i(TAG, String.format("Constructed AudioRecord with buffer size %d", BUFFER_SIZE));

        mBuffer = new short[BUFFER_SIZE];
        Log.i(TAG, String.format("Allocated audio buffer with size %d", BUFFER_SIZE));

        mListeners = new HashSet<>();
    }

    /** Adds a listener. */
    public void addListener(Listener listener) {
        mListeners.add(listener);
    }

    /** Removes a listener. */
    public void removeListener(Listener listener) {
        mListeners.remove(listener);
    }

    /** Records an audio track and sends it to all listeners. */
    @Override
    public void run() {
        long startTs = System.currentTimeMillis();

        mMetrics.startedReading();
        mAudioRecord.startRecording();

        int samplesRecorded = 0;
        boolean gotValidData = false;
        // Read audio into the buffer
        while (mActive && samplesRecorded < BUFFER_SIZE) {
            int samplesRead = mAudioRecord.read(mBuffer, samplesRecorded,
                    mMinBufferSize / 2, AudioRecord.READ_BLOCKING);
            if (samplesRead > 0) {
                // Check for first non-zero sample and record it in the metrics
                if (!gotValidData) {
                    for (int i = 0; i < samplesRead; i++) {
                        int currentSample = samplesRecorded + i;
                        if (mBuffer[currentSample] != 0) {
                            gotValidData = true;
                            mMetrics.receivedValidAudio();
                            mMetrics.setEmptyAudioDurationMs(samplesToMs(currentSample));
                            break;
                        }
                    }
                }

                samplesRecorded += samplesRead;
                Log.v(TAG,
                        String.format("Time elapsed: %d ms", System.currentTimeMillis() - startTs));
                Log.v(TAG, String.format("Audio recorded: %d ms", samplesToMs(samplesRecorded)));
            } else if (samplesRead == 0) {
                Log.e(TAG, "Read 0 bytes");
            } else if (samplesRead < 0) {
                mActive = false;
                Log.e(TAG, String.format("AudioRecord error: %d", samplesRead));
            }
        }

        mAudioRecord.release();

        if (mCancelled) {
            Log.i(TAG, "Cancelled audio recording");
        } else {
            Log.i(TAG,
                    String.format("Recorded audio buffer with %d samples", samplesRecorded));

            ShortBuffer audioBuffer = ShortBuffer.wrap(mBuffer, samplesRecorded,
                    mBuffer.length - samplesRecorded);

            mListeners.forEach(l -> l.onAudioRecorded(audioBuffer));
        }
    }

    /** Stops audio recording, and sends recorded audio to listeners. */
    public void stop() {
        mActive = false;
    }

    /** Stops audio recording, and discards the recorded audio. */
    public void cancel() {
        mCancelled = true;
        mActive = false;
    }


    private static long samplesToMs(int samples) {
        return Math.round((double) samples / AudioDebug.SAMPLE_RATE / AudioDebug.CHANNELS * 1000);
    }
}
