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
 * limitations under the License.
 */

package com.android.cts.device.blob;

import android.app.Instrumentation;
import android.app.blob.BlobHandle;
import android.app.blob.BlobStoreManager;
import android.content.Context;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;

public class BaseBlobStoreDeviceTest {

    protected static final String KEY_SESSION_ID = "session";

    protected static final String KEY_DIGEST = "digest";
    protected static final String KEY_LABEL = "label";
    protected static final String KEY_EXPIRY = "expiry";
    protected static final String KEY_TAG = "tag";

    protected static final String KEY_ALLOW_PUBLIC = "public";

    protected static final long PARTIAL_FILE_LENGTH_BYTES = 2002;
    protected static final long TIMEOUT_WAIT_FOR_IDLE_MS = 2_000;
    protected static final long TIMEOUT_COMMIT_CALLBACK_MS = 10_000;

    protected Context mContext;
    protected Instrumentation mInstrumentation;
    protected BlobStoreManager mBlobStoreManager;

    @Before
    public void setUp() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getContext();
        mBlobStoreManager = (BlobStoreManager) mContext.getSystemService(
                Context.BLOB_STORE_SERVICE);
    }

    protected long createSession(BlobHandle blobHandle) throws Exception {
        final long sessionId = mBlobStoreManager.createSession(blobHandle);
        return sessionId;
    }
}
