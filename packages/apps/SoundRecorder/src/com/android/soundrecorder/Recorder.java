/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.soundrecorder;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;

import android.content.Context;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaRecorder;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.MediaPlayer.OnErrorListener;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

public class Recorder implements OnCompletionListener, OnErrorListener {
    static final String TAG = "Recorder";
    static final String SAMPLE_PREFIX = "recording";
    static final String SAMPLE_PATH_KEY = "sample_path";
    static final String SAMPLE_LENGTH_KEY = "sample_length";
    public static final String RECORD_FOLDER = "Records";

    public static final int IDLE_STATE = 0;
    public static final int RECORDING_STATE = 1;
    public static final int PLAYING_STATE = 2;
    
    int mState = IDLE_STATE;

    public static final int NO_ERROR = 0;
    public static final int SDCARD_ACCESS_ERROR = 1;
    public static final int INTERNAL_ERROR = 2;
    public static final int IN_CALL_RECORD_ERROR = 3;

    public static final String TEMP_SUFFIX = ".tmp";
    
    public interface OnStateChangedListener {
        public void onStateChanged(int state);
        public void onError(int error);
    }
    OnStateChangedListener mOnStateChangedListener = null;
    
    long mSampleStart = 0;       // time at which latest record or play operation started
    int mSampleLength = 0;      // length of current sample
    File mSampleFile = null;
    
    MediaRecorder mRecorder = null;
    MediaPlayer mPlayer = null;

    int mSamplingRate = 48000;

    public Recorder() {
    }

    public void setAudioSamplingRate(int rate) {
        mSamplingRate = rate;
    }

    public void saveState(Bundle recorderState) {
        recorderState.putString(SAMPLE_PATH_KEY, mSampleFile.getAbsolutePath());
        recorderState.putInt(SAMPLE_LENGTH_KEY, mSampleLength);
    }
    
    public int getMaxAmplitude() {
        if (mState != RECORDING_STATE)
            return 0;
        return mRecorder.getMaxAmplitude();
    }
    
    public void restoreState(Bundle recorderState) {
        String samplePath = recorderState.getString(SAMPLE_PATH_KEY);
        if (samplePath == null)
            return;
        int sampleLength = recorderState.getInt(SAMPLE_LENGTH_KEY, -1);
        if (sampleLength == -1)
            return;

        File file = new File(samplePath);
        if (!file.exists())
            return;
        if (mSampleFile != null
                && mSampleFile.getAbsolutePath().compareTo(file.getAbsolutePath()) == 0)
            return;
        
        delete();
        mSampleFile = file;
        mSampleLength = sampleLength;

        signalStateChanged(IDLE_STATE);
    }
    
    public void setOnStateChangedListener(OnStateChangedListener listener) {
        mOnStateChangedListener = listener;
    }
    
    public int state() {
        return mState;
    }
    
    public int progress() {
        if (mState == RECORDING_STATE || mState == PLAYING_STATE)
            return (int) ((System.currentTimeMillis() - mSampleStart)/1000);
        return 0;
    }
    
    public int sampleLength() {
        return mSampleLength;
    }

    public File sampleFile() {
        return mSampleFile;
    }
    
    /**
     * Resets the recorder state. If a sample was recorded, the file is deleted.
     */
    public void delete() {
        stop();
        
        if (mSampleFile != null)
            mSampleFile.delete();

        mSampleFile = null;
        mSampleLength = 0;
        
        signalStateChanged(IDLE_STATE);
    }
    
    /**
     * Resets the recorder state. If a sample was recorded, the file is left on disk and will 
     * be reused for a new recording.
     */
    public void clear() {
        stop();
        
        mSampleLength = 0;
        
        signalStateChanged(IDLE_STATE);
    }

    public void startRecording(int outputfileformat, int recordingType, String extension, Context context) {
        stop();
        if (mSampleFile == null) {
            String myExtension = extension + TEMP_SUFFIX;
            File sampleDir = Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_DOCUMENTS);
            if (!sampleDir.canWrite()) // Workaround for broken sdcard support on the device.
                sampleDir = Environment.getExternalStorageDirectory();
            String sampleDirPath = null;
            if (sampleDir != null) {
                sampleDirPath = sampleDir.getAbsolutePath() + File.separator
                        + RECORD_FOLDER;
            }
            if (sampleDirPath != null) {
                sampleDir = new File(sampleDirPath);
            }
            if (sampleDir != null && !sampleDir.exists()) {
                if (!sampleDir.mkdirs()) {
                    Log.i(TAG, "<startRecording> make dirs fail");
                }
            }
            try {
                if (null != sampleDir) {
                    Log.i(TAG, "SR sampleDir  is:" + sampleDir.toString());
                }
                SimpleDateFormat simpleDateFormat = new SimpleDateFormat(
                        "yyyyMMddHHmmss");
                String time = simpleDateFormat.format(new Date(System
                        .currentTimeMillis()));
                StringBuilder stringBuilder = new StringBuilder();
                stringBuilder.append(SAMPLE_PREFIX).append("_" + time)
                        .append(myExtension);
                String name = stringBuilder.toString();
                mSampleFile = new File(sampleDir, name);
                boolean result = mSampleFile.createNewFile();
                if (result) {
                    Log.i(TAG, "creat file success");
                }
            } catch (IOException e) {
                setError(SDCARD_ACCESS_ERROR);
                return;
            }
        }

        mRecorder = new MediaRecorder();
        mRecorder.setAudioSource(MediaRecorder.AudioSource.MIC);
        mRecorder.setOutputFormat(outputfileformat);
        mRecorder.setOutputFile(mSampleFile.getAbsolutePath());

        mRecorder.setAudioSamplingRate(mSamplingRate);
        switch (recordingType) {
            case MediaRecorder.AudioEncoder.AAC:
                mRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
                mRecorder.setAudioEncodingBitRate(SoundRecorder.BITRATE_AAC);
                mRecorder.setAudioSamplingRate(SoundRecorder.SAMPLE_RATE_AAC);
                mRecorder.setAudioChannels(2);
                break;

            case MediaRecorder.AudioEncoder.AMR_WB:
                mRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AMR_NB);
                mRecorder.setAudioEncodingBitRate(SoundRecorder.BITRATE_AWB);
                mRecorder.setAudioSamplingRate(SoundRecorder.SAMPLE_RATE_AWB);
                break;

            case MediaRecorder.AudioEncoder.AMR_NB:
                mRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AMR_NB);
                mRecorder.setAudioEncodingBitRate(SoundRecorder.BITRATE_AMR);
                break;

            default:
                break;

        }

        // Handle IOException
        try {
            mRecorder.prepare();
        } catch (IOException exception) {
            setError(INTERNAL_ERROR);
            mRecorder.reset();
            mRecorder.release();
            mRecorder = null;
            return;
        }
        // Handle RuntimeException if the recording couldn't start
        try {
            mRecorder.start();
        } catch (RuntimeException exception) {
            AudioManager audioMngr = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            boolean isInCall = ((audioMngr.getMode() == AudioManager.MODE_IN_CALL) ||
                    (audioMngr.getMode() == AudioManager.MODE_IN_COMMUNICATION));
            if (isInCall) {
                setError(IN_CALL_RECORD_ERROR);
            } else {
                setError(INTERNAL_ERROR);
            }
            mRecorder.reset();
            mRecorder.release();
            mRecorder = null;
            return;
        }
        mSampleStart = System.currentTimeMillis();
        setState(RECORDING_STATE);
    }

    public void sampleFileDelSuffix() {
        if ((mSampleFile != null) && mSampleFile.exists()) {
            String oldPath = mSampleFile.getAbsolutePath();
            if (oldPath.endsWith(TEMP_SUFFIX)) {
                String newPath = oldPath.substring(0,
                        oldPath.lastIndexOf(TEMP_SUFFIX));
                File newFile = new File(newPath);
                boolean result = mSampleFile.renameTo(newFile);
                if (result) {
                    mSampleFile = newFile;
                    Log.i(TAG, "<sampleFileDelSuffix()> rename file <"
                            + oldPath + "> to <" + newPath + ">");
                } else {
                    Log.i(TAG,
                            "<sampleFileDelSuffix()> rename file fail");
                }
            } else {
                Log.i(TAG, "<sampleFileDelSuffix()> file <" + oldPath
                        + "> is not end with <.tmp>");
                return;
            }
        }
    }

    public void stopRecording() {
        if (mRecorder == null)
            return;

        try {
            mRecorder.stop();
        } catch (RuntimeException e) {
            Log.w("Recorder", "Catch RuntimeException on MediaRecorder.stop() due to a call " +
                "immediately after MediaRecorder.start().");
        }
        mRecorder.release();
        mRecorder = null;

        mSampleLength = (int)( (System.currentTimeMillis() - mSampleStart)/1000 );
        setState(IDLE_STATE);
    }
    
    public void startPlayback() {
        stop();
        
        mPlayer = new MediaPlayer();
        try {
            mPlayer.setDataSource(mSampleFile.getAbsolutePath());
            mPlayer.setOnCompletionListener(this);
            mPlayer.setOnErrorListener(this);
            mPlayer.prepare();
            mPlayer.start();
        } catch (IllegalArgumentException e) {
            setError(INTERNAL_ERROR);
            mPlayer = null;
            return;
        } catch (IOException e) {
            setError(SDCARD_ACCESS_ERROR);
            mPlayer = null;
            return;
        }
        
        mSampleStart = System.currentTimeMillis();
        setState(PLAYING_STATE);
    }
    
    public void stopPlayback() {
        if (mPlayer == null) // we were not in playback
            return;

        mPlayer.stop();
        mPlayer.release();
        mPlayer = null;
        setState(IDLE_STATE);
    }
    
    public void stop() {
        stopRecording();
        stopPlayback();
    }

    public boolean onError(MediaPlayer mp, int what, int extra) {
        stop();
        setError(SDCARD_ACCESS_ERROR);
        return true;
    }

    public void onCompletion(MediaPlayer mp) {
        stop();
    }
    
    private void setState(int state) {
        if (state == mState)
            return;
        
        mState = state;
        signalStateChanged(mState);
    }
    
    private void signalStateChanged(int state) {
        if (mOnStateChangedListener != null)
            mOnStateChangedListener.onStateChanged(state);
    }
    
    private void setError(int error) {
        if (mOnStateChangedListener != null)
            mOnStateChangedListener.onError(error);
    }
}
