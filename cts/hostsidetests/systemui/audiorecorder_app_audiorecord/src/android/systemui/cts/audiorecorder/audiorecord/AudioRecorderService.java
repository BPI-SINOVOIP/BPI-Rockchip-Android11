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

package android.systemui.cts.audiorecorder.audiorecord;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.systemui.cts.audiorecorder.base.BaseAudioRecorderService;

public class AudioRecorderService extends BaseAudioRecorderService {
    /**
     * The flag that indicates whether service is recording.
     * This is only written in UI thread. AudioRecordingThread only reads the value in order to
     * decide whether it should carry on recording. For such usage model we do not need
     * synchronization, marking the flag as `volatile` is enough.
     */
    private volatile boolean mRecording;

    protected void startRecording() {
        mRecording = true;
        new AudioRecordingThread().start();
    }

    protected void stopRecording() {
        mRecording = false;
    }

    protected boolean isRecording() {
        return mRecording;
    }

    private class AudioRecordingThread extends Thread {
        @Override
        public void run() {
            final int channelConfig = AudioFormat.CHANNEL_IN_MONO;
            final int sampleRate = 32000;
            final int format = AudioFormat.ENCODING_PCM_16BIT;
            final int bufferSizeInBytes = 2 * AudioRecord.getMinBufferSize(sampleRate, channelConfig,
                    format);
            AudioRecord audioRecord =
                    new AudioRecord.Builder()
                            .setAudioFormat(
                                    new AudioFormat.Builder()
                                            .setEncoding(format)
                                            .setSampleRate(sampleRate)
                                            .setChannelMask(channelConfig)
                                            .build())
                            .setBufferSizeInBytes(bufferSizeInBytes)
                            .build();

            audioRecord.startRecording();

            while (mRecording) {
                final short[] data = new short[bufferSizeInBytes / 2];
                audioRecord.read(data, 0, data.length);
            }

            audioRecord.stop();
            audioRecord.release();
        }
    }
}
