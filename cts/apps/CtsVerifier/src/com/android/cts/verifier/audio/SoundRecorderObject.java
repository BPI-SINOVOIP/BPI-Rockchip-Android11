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

package com.android.cts.verifier.audio;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.os.SystemClock;
import android.util.Log;
import com.android.cts.verifier.audio.wavelib.*;

public class SoundRecorderObject implements Runnable,
        AudioRecord.OnRecordPositionUpdateListener {
    private static final String TAG = "SoundRecorderObject";

    private AudioRecord mRecorder;
    private int mMinRecordBufferSizeInSamples = 0;
    private short[] mAudioShortArray;

    private final int mBlockSizeSamples;
    private final int mSamplingRate;
    private final int mSelectedRecordSource;
    private final int mChannelConfig = AudioFormat.CHANNEL_IN_MONO;
    private final int mAudioFormat = AudioFormat.ENCODING_PCM_16BIT;
    private final int mBytesPerSample = 2; //pcm int16
    private Thread mRecordThread;

    public PipeShort mPipe = new PipeShort(65536);

    public SoundRecorderObject(int samplingRate, int blockSize, int recordingSource) {
        mSamplingRate = samplingRate;
        mBlockSizeSamples = blockSize;
        mSelectedRecordSource = recordingSource;
    }

    public int getAudioSessionId() {
        if (mRecorder != null) {
            return mRecorder.getAudioSessionId();
        }
        return -1;
    }

    public void startRecording() {
        boolean successful = initRecord();
        if (successful) {
            startRecordingForReal();
        } else {
            Log.v(TAG, "Recorder initialization error.");
        }
    }

    private void startRecordingForReal() {
        // start streaming
        if (mRecordThread == null) {
            mRecordThread = new Thread(this);
            mRecordThread.setName("recordingThread");
        }
        if (!mRecordThread.isAlive()) {
            mRecordThread.start();
        }

        mPipe.flush();

//        long startTime = SystemClock.uptimeMillis();
        mRecorder.startRecording();
        if (mRecorder.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
            stopRecording();
            return;
        }
        // Log.v(TAG, "Start time: " + (long) (SystemClock.uptimeMillis() - startTime) + " ms");
    }

    public void stopRecording() {
        //TODO: consider addin a lock to protect usage
        stopRecordingForReal();
    }

    private void stopRecordingForReal() {
        // stop streaming
        Thread zeThread = mRecordThread;
        mRecordThread = null;
        if (zeThread != null) {
            zeThread.interrupt();
            try {
                zeThread.join();
            } catch(InterruptedException e) {
                //restore interrupted status of recording thread
                zeThread.interrupt();
            }
        }
        // release recording resources
        if (mRecorder != null) {
            mRecorder.stop();
            mRecorder.release();
            mRecorder = null;
        }
    }

    private boolean initRecord() {
        int minRecordBuffSizeInBytes = AudioRecord.getMinBufferSize(mSamplingRate,
                mChannelConfig, mAudioFormat);
        Log.v(TAG,"FrequencyAnalyzer: min buff size = " + minRecordBuffSizeInBytes + " bytes");
        if (minRecordBuffSizeInBytes <= 0) {
            return false;
        }

        mMinRecordBufferSizeInSamples = minRecordBuffSizeInBytes / mBytesPerSample;
        // allocate the byte array to read the audio data

        mAudioShortArray = new short[mMinRecordBufferSizeInSamples];

        Log.v(TAG, "Initiating record:");
        Log.v(TAG, "using source " + mSelectedRecordSource);
        Log.v(TAG, "at " + mSamplingRate + "Hz");

        try {
            mRecorder = new AudioRecord(mSelectedRecordSource, mSamplingRate,
                    mChannelConfig, mAudioFormat,
                    2 * minRecordBuffSizeInBytes /* double size for padding */);
        } catch (IllegalArgumentException e) {
            return false;
        }
        if (mRecorder.getState() != AudioRecord.STATE_INITIALIZED) {
            mRecorder.release();
            mRecorder = null;
            return false;
        }
        mRecorder.setRecordPositionUpdateListener(this);
        mRecorder.setPositionNotificationPeriod(mBlockSizeSamples / 2);
        return true;
    }

    public void periodicNotification(AudioRecord recorder) {}
    public void markerReached(AudioRecord track) {}

     // ---------------------------------------------------------
     // Implementation of AudioRecord.OnPeriodicNotificationListener
     // --------------------
    @Override
    public void onPeriodicNotification(AudioRecord recorder) {
        periodicNotification(recorder);
    }

    @Override
    public void onMarkerReached(AudioRecord track) {
        markerReached(track);
    }

    // ---------------------------------------------------------
    // Implementation of Runnable for the audio recording + playback
    // --------------------
    public void run() {
        Thread thisThread = Thread.currentThread();
        while (!thisThread.isInterrupted()) {
            // read from native recorder
            int nSamplesRead = mRecorder.read(mAudioShortArray, 0, mMinRecordBufferSizeInSamples);
            if (nSamplesRead > 0) {
                mPipe.write(mAudioShortArray, 0, nSamplesRead);
            }
        }
    }
}

