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
package android.textclassifier.cts2;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextLanguage;

/**
 * An activity that queries the device's TextClassifierService when started and immediately
 * terminates itself.
 */
public final class QueryTextClassifierServiceActivity extends Activity {

    private static final String TAG = QueryTextClassifierServiceActivity.class.getSimpleName();
    private PendingIntent mPendingIntent;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Intent intent = getIntent();
        mPendingIntent = intent.getParcelableExtra("finishBroadcast");
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Do a TextClassifier call
        detectLanguageAndFinish();
    }

    private void detectLanguageAndFinish() {
        final String text = "An email address is test@example.com.";
        new Thread(() -> {
            final TextLanguage.Request textLanguageRequest =
                    new TextLanguage.Request.Builder(text)
                            .build();
            TextClassifier textClassifier = getClassifier();
            textClassifier.detectLanguage(textLanguageRequest);
            // Send finish broadcast
            if (mPendingIntent != null) {
                try {
                    mPendingIntent.send();
                } catch (CanceledException e) {
                    Log.w(TAG, "Pending intent " + mPendingIntent + " canceled");
                }
            }
            finish();
        }).start();
    }

    private TextClassifier getClassifier() {
        final TextClassificationManager tcm = getSystemService(TextClassificationManager.class);
        return tcm != null ? tcm.getTextClassifier() : TextClassifier.NO_OP;
    }
}
