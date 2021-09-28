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

package com.android.cts.permissiondeclareapp;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class SendResultActivity extends Activity {
    private static final String TAG = "SendUriActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // For simplicity, we're willing to grant whatever they asked for
        final ClipData reqClip = getIntent().getParcelableExtra(Intent.EXTRA_TEXT);
        final int reqMode = getIntent().getIntExtra(Intent.EXTRA_INDEX, 0);

        final Intent result = new Intent();
        result.setClipData(reqClip);
        result.addFlags(reqMode);

        try {
            Log.d(TAG, "Finishing OK with " + result);
            setResult(RESULT_OK, result);
            finish();
        } catch (SecurityException e) {
            Log.d(TAG, "Finishing CANCELED due to " + e);
            setResult(RESULT_CANCELED, null);
            finish();

            // Make sure we hand control back to whoever started us
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }
}
