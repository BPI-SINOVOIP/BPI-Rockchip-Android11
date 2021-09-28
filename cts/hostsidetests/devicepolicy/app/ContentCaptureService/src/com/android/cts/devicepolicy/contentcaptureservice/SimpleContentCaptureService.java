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

package com.android.cts.devicepolicy.contentcaptureservice;

import android.content.ComponentName;
import android.service.contentcapture.ContentCaptureService;
import android.util.ArraySet;
import android.util.Log;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureSessionId;

public class SimpleContentCaptureService extends ContentCaptureService {

    private static final String TAG = SimpleContentCaptureService.class.getSimpleName();

    @Override
    public void onConnected() {
        Log.d(TAG, "onConnected()");
        final ArraySet<String> packages = new ArraySet<>();
        packages.add("com.android.cts.devicepolicy.contentcaptureapp");
        packages.add("com.android.cts.devicepolicy.contentcaptureservice");
        packages.add("com.android.cts.deviceandprofileowner");

        setContentCaptureWhitelist(packages, null);
    }

    @Override
    public void onDisconnected() {
        Log.d(TAG, "onDisconnected()");
    }

    @Override
    public void onCreateContentCaptureSession(ContentCaptureContext context,
            ContentCaptureSessionId sessionId) {
        Log.d(TAG, "onCreateContentCaptureSession(): " + sessionId);
    }
}
