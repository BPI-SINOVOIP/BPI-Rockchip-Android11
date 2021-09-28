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
package com.android.cts.blob;

import android.app.blob.BlobHandle;
import android.app.usage.StorageStats;
import android.os.ParcelFileDescriptor;

interface ICommandReceiver {
    int commit(in BlobHandle blobHandle, in ParcelFileDescriptor input, int accessTypeFlags,
            long size, long timeoutSec);
    ParcelFileDescriptor openBlob(in BlobHandle blobHandle);
    void openSession(long sessionId);

    void acquireLease(in BlobHandle blobHandle);
    void releaseLease(in BlobHandle blobHandle);

    StorageStats queryStatsForPackage();
    StorageStats queryStatsForUid();

    const int FLAG_ACCESS_TYPE_PRIVATE = 0;
    const int FLAG_ACCESS_TYPE_PUBLIC = 1;
}