/*
 * Copyright (C) 2020 The Android Open Source Project
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
 * limitations under the License
 */

package android.recognitionservice.service;

import android.app.AppOpsManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.media.MediaRecorder;
import android.os.Binder;
import android.speech.RecognitionService;
import android.speech.RecognizerIntent;
import android.util.Log;

import java.io.File;
import java.io.IOException;

/**
 * Implementation of {@link RecognitionService} used in the tests.
 */
public class CtsVoiceRecognitionService extends RecognitionService {

    private final String TAG = "CtsVoiceRecognitionService";

    private MediaRecorder mMediaRecorder;
    private File mOutputFile;

    @Override
    protected void onCancel(Callback listener) {
        // No-op.
    }

    @Override
    protected void onStopListening(Callback listener) {
        // No-op.
    }

    @Override
    protected void onStartListening(Intent recognizerIntent, Callback listener) {
        Log.d(TAG, "onStartListening");
        mediaRecorderReady();
        blameCameraPermission(recognizerIntent, listener.getCallingUid());
        try {
            mMediaRecorder.prepare();
            mMediaRecorder.start();
        } catch (IOException e) {
            // We focus on the open mic behavior, wedon't need to real record and save to the file.
            // Because we don't set the output the output file. The IOException occurred when start.
            // We catch this and reset the media record.
            e.printStackTrace();
            mMediaRecorder.release();
            mMediaRecorder = null;
        }
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        stopRecord();
        super.onDestroy();
    }

    // RecognitionService try to blame non-mic permission
    private void blameCameraPermission(Intent recognizerIntent, int callingPackageUid) {
        final String callingPackage =
                recognizerIntent.getStringExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE);
        final AppOpsManager appOpsManager = getSystemService(AppOpsManager.class);
        appOpsManager.noteProxyOpNoThrow(AppOpsManager.OPSTR_CAMERA, callingPackage,
                callingPackageUid, /*attributionTag*/ null, /*message*/ null);
    }

    private void mediaRecorderReady() {
        mMediaRecorder = new MediaRecorder();
        mOutputFile = new File(getExternalCacheDir(), "test.3gp");
        mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.MIC);
        mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.THREE_GPP);
        mMediaRecorder.setAudioEncoder(MediaRecorder.OutputFormat.AMR_NB);
        mMediaRecorder.setOutputFile(mOutputFile);
    }

    private void stopRecord() {
        if (mMediaRecorder != null) {
            mMediaRecorder.stop();
            mMediaRecorder.release();
            mMediaRecorder = null;
        }
        if (mOutputFile != null && mOutputFile.exists()) {
            mOutputFile.delete();
        }
    }
}
