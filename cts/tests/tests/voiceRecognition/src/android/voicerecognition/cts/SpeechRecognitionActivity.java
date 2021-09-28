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

package android.voicerecognition.cts;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;

/**
 * An activity that uses SpeechRecognition APIs. SpeechRecognition will bind the RecognitionService
 * to provide the voice recognition functions.
 */
public class SpeechRecognitionActivity extends Activity {

    private final String TAG = "SpeechRecognitionActivity";

    private SpeechRecognizer mRecognizer;
    private Intent mRecognizerIntent;
    private Handler mHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        init();
    }

    @Override
    protected void onDestroy() {
        if (mRecognizer != null) {
            mRecognizer.destroy();
            mRecognizer = null;
        }
        super.onDestroy();
    }

    public void startListening() {
        mHandler.post(() -> {
            if (mRecognizer != null) {
                final Intent recognizerIntent =
                        new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
                recognizerIntent.putExtra(
                        RecognizerIntent.EXTRA_CALLING_PACKAGE, this.getPackageName());
                mRecognizer.startListening(recognizerIntent);
            }
        });
    }

    private void init() {
        mHandler = new Handler(getMainLooper());
        mRecognizer = SpeechRecognizer.createSpeechRecognizer(this);
    }
}
