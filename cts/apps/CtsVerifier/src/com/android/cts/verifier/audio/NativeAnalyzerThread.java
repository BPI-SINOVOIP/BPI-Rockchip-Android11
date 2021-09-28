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


package com.android.cts.verifier.audio;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import android.os.Handler;
import  android.os.Message;

/**
 * A thread that runs a native audio loopback analyzer.
 */
public class NativeAnalyzerThread {
    private final int mSecondsToRun = 5;
    private Handler mMessageHandler;
    private Thread mThread;
    private volatile boolean mEnabled = false;
    private volatile double mLatencyMillis = 0.0;
    private volatile double mConfidence = 0.0;
    private volatile int mSampleRate = 0;

    private int mInputPreset = 0;

    static final int NATIVE_AUDIO_THREAD_MESSAGE_REC_STARTED = 892;
    static final int NATIVE_AUDIO_THREAD_MESSAGE_OPEN_ERROR = 893;
    static final int NATIVE_AUDIO_THREAD_MESSAGE_REC_ERROR = 894;
    static final int NATIVE_AUDIO_THREAD_MESSAGE_REC_COMPLETE = 895;
    static final int NATIVE_AUDIO_THREAD_MESSAGE_REC_COMPLETE_ERRORS = 896;

    public void setInputPreset(int inputPreset) {
        mInputPreset = inputPreset;
    }

    //JNI load
    static {
        try {
            System.loadLibrary("audioloopback_jni");
        } catch (UnsatisfiedLinkError e) {
            log("Error loading loopback JNI library");
            e.printStackTrace();
        }

        /* TODO: gracefully fail/notify if the library can't be loaded */
    }

    /**
     * @return native audio context
     */
    private native long openAudio(int micSource);
    private native int startAudio(long audio_context);
    private native int stopAudio(long audio_context);
    private native int closeAudio(long audio_context);
    private native int getError(long audio_context);
    private native boolean isRecordingComplete(long audio_context);
    private native int analyze(long audio_context);
    private native double getLatencyMillis(long audio_context);
    private native double getConfidence(long audio_context);
    private native int getSampleRate(long audio_context);

    public double getLatencyMillis() {
        return mLatencyMillis;
    }

    public double getConfidence() {
        return mConfidence;
    }

    public int getSampleRate() { return mSampleRate; }

    public synchronized void startTest() {
        if (mThread == null) {
            mEnabled = true;
            mThread = new Thread(mBackGroundTask);
            mThread.start();
        }
    }

    public synchronized void stopTest(int millis) throws InterruptedException {
        mEnabled = false;
        if (mThread != null) {
            mThread.interrupt();
            mThread.join(millis);
            mThread = null;
        }
    }

    private void sendMessage(int what) {
        if (mMessageHandler != null) {
            Message msg = Message.obtain();
            msg.what = what;
            mMessageHandler.sendMessage(msg);
        }
    }

    private Runnable mBackGroundTask = () -> {
        mLatencyMillis = 0.0;
        mConfidence = 0.0;
        mSampleRate = 0;

        boolean analysisComplete = false;

        log(" Started capture test");
        sendMessage(NATIVE_AUDIO_THREAD_MESSAGE_REC_STARTED);

        long audioContext = openAudio(mInputPreset);
        log(String.format("audioContext = 0x%X",audioContext));

        if (audioContext == 0 ) {
            log(" ERROR at JNI initialization");
            sendMessage(NATIVE_AUDIO_THREAD_MESSAGE_OPEN_ERROR);
        }  else if (mEnabled) {
            int result = startAudio(audioContext);
            if (result < 0) {
                sendMessage(NATIVE_AUDIO_THREAD_MESSAGE_REC_ERROR);
                mEnabled = false;
            }

            final long timeoutMillis = mSecondsToRun * 1000;
            final long startedAtMillis = System.currentTimeMillis();
            boolean timedOut = false;
            int loopCounter = 0;
            while (mEnabled && !timedOut) {
                result = getError(audioContext);
                if (result < 0) {
                    sendMessage(NATIVE_AUDIO_THREAD_MESSAGE_REC_ERROR);
                    break;
                } else if (isRecordingComplete(audioContext)) {
                    result = stopAudio(audioContext);
                    if (result < 0) {
                        sendMessage(NATIVE_AUDIO_THREAD_MESSAGE_REC_ERROR);
                        break;
                    }

                    // Analyze the recording and measure latency.
                    mThread.setPriority(Thread.MAX_PRIORITY);
                    result = analyze(audioContext);
                    if (result < 0) {
                        break;
                    } else {
                        analysisComplete = true;
                    }
                    mLatencyMillis = getLatencyMillis(audioContext);
                    mConfidence = getConfidence(audioContext);
                    mSampleRate = getSampleRate(audioContext);
                    break;
                } else {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
                long now = System.currentTimeMillis();
                timedOut = (now - startedAtMillis) > timeoutMillis;
            }
            log("latency: analyze returns " + result);
            closeAudio(audioContext);

            int what = (analysisComplete && result == 0)
                    ? NATIVE_AUDIO_THREAD_MESSAGE_REC_COMPLETE
                    : NATIVE_AUDIO_THREAD_MESSAGE_REC_COMPLETE_ERRORS;
            sendMessage(what);
        }
    };

    public void setMessageHandler(Handler messageHandler) {
        mMessageHandler = messageHandler;
    }

    private static void log(String msg) {
        Log.v("Loopback", msg);
    }

}  //end thread.
