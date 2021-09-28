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

package com.android.test.storagedelegator;

import android.app.Activity;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;

/**
 * Writes file content for use in tests. This is needed to create files when the test does not have
 * sufficient permission.
 */
public class StorageDelegator extends Activity {

    private static final String TAG = "StorageDelegator";
    private static final String EXTRA_PATH = "path";
    private static final String EXTRA_CONTENTS = "contents";
    private static final String EXTRA_CALLBACK = "callback";
    private static final String KEY_ERROR = "error";
    private static final String ACTION_CREATE_FILE_WITH_CONTENT =
            "com.android.cts.action.CREATE_FILE_WITH_CONTENT";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (!getIntent().getAction().equals(ACTION_CREATE_FILE_WITH_CONTENT)) {
            Log.w(TAG, "Unsupported action: " + getIntent().getAction());
            finish();
            return;
        }

        final String path = getIntent().getStringExtra(EXTRA_PATH);
        final String contents = getIntent().getStringExtra(EXTRA_CONTENTS);
        Log.i(TAG, "onHandleIntent: path=" + path + ", content=" + contents);

        final File file = new File(path);
        if (file.getParentFile() != null) {
            file.getParentFile().mkdirs();
        }

        final Bundle result = new Bundle();
        try {
            Files.write(file.toPath(), contents.getBytes());
            Log.i(TAG, "onHandleIntent: path=" + path + ", length=" + file.length());
        } catch (IOException e) {
            Log.e(TAG, "writing file: " + path, e);
            result.putString(KEY_ERROR, e.getMessage());
        }
        sendResult(result);
        finish();
    }

    void sendResult(Bundle result) {
        getIntent().<RemoteCallback>getParcelableExtra(EXTRA_CALLBACK).sendResult(result);
    }
}
