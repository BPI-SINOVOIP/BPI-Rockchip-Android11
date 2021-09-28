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
 * limitations under the License
 */

package android.systemui.cts.audiorecorder.mediarecorder;

import android.media.MediaRecorder;
import android.media.MediaRecorder.AudioSource;
import android.systemui.cts.audiorecorder.base.BaseAudioRecorderService;

import java.io.File;
import java.io.IOException;

public class AudioRecorderService extends BaseAudioRecorderService {
    private MediaRecorder mMediaRecorder = null;

    protected void startRecording() {
        mMediaRecorder = new MediaRecorder();
        mMediaRecorder.setAudioSource(AudioSource.MIC);
        mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.THREE_GPP);
        mMediaRecorder.setOutputFile(new File(getExternalCacheDir(), "record.3gp"));
        mMediaRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AMR_NB);

        try {
            mMediaRecorder.prepare();
        } catch (IOException e) {
            mMediaRecorder.release();
            mMediaRecorder = null;
            return;
        }

        mMediaRecorder.start();
    }

    protected void stopRecording() {
        mMediaRecorder.stop();
        mMediaRecorder.release();
        mMediaRecorder = null;
    }

    protected boolean isRecording() {
        return mMediaRecorder != null;
    }
}
