/*
 * Copyright 2020 The Android Open Source Project
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
package com.android.cts.blob.helper;

import static android.os.storage.StorageManager.UUID_DEFAULT;

import static com.android.utils.blob.Utils.TAG;
import static com.android.utils.blob.Utils.writeToSession;

import static com.google.common.truth.Truth.assertThat;

import android.app.Service;
import android.app.blob.BlobHandle;
import android.app.blob.BlobStoreManager;
import android.app.usage.StorageStats;
import android.app.usage.StorageStatsManager;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.IBinder;
import android.os.LimitExceededException;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

import com.android.cts.blob.ICommandReceiver;
import com.android.utils.blob.Utils;

import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

public class BlobStoreTestService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return new CommandReceiver();
    }

    private class CommandReceiver extends ICommandReceiver.Stub {
        @Override
        public int commit(BlobHandle blobHandle, ParcelFileDescriptor input, int accessTypeFlags,
                long timeoutSec, long size) {
            final BlobStoreManager blobStoreManager = getSystemService(
                    BlobStoreManager.class);
            try {
                final long sessionId = blobStoreManager.createSession(blobHandle);
                try (BlobStoreManager.Session session = blobStoreManager.openSession(sessionId)) {
                    writeToSession(session, input, size);
                    if ((accessTypeFlags & ICommandReceiver.FLAG_ACCESS_TYPE_PUBLIC) != 0) {
                        session.allowPublicAccess();
                    }

                    Log.d(TAG, "Committing session: " + sessionId + "; blob: " + blobHandle);
                    final CompletableFuture<Integer> callback = new CompletableFuture<>();
                    session.commit(getMainExecutor(), callback::complete);
                    return callback.get(timeoutSec, TimeUnit.SECONDS);
                }
            } catch (IOException | InterruptedException | TimeoutException | ExecutionException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public ParcelFileDescriptor openBlob(BlobHandle blobHandle) {
            final BlobStoreManager blobStoreManager = getSystemService(
                    BlobStoreManager.class);
            try {
                return blobStoreManager.openBlob(blobHandle);
            } catch (IOException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public void openSession(long sessionId) {
            final BlobStoreManager blobStoreManager = getSystemService(
                    BlobStoreManager.class);
            try {
                try (BlobStoreManager.Session session = blobStoreManager.openSession(sessionId)) {
                    assertThat(session).isNotNull();
                }
            } catch (IOException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public void acquireLease(BlobHandle blobHandle) {
            final BlobStoreManager blobStoreManager = getSystemService(
                    BlobStoreManager.class);
            try {
                Utils.acquireLease(BlobStoreTestService.this, blobHandle, "Test description");
                assertThat(blobStoreManager.getLeasedBlobs()).contains(blobHandle);
            } catch (IOException | LimitExceededException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public void releaseLease(BlobHandle blobHandle) {
            final BlobStoreManager blobStoreManager = getSystemService(
                    BlobStoreManager.class);
            try {
                Utils.releaseLease(BlobStoreTestService.this, blobHandle);
                assertThat(blobStoreManager.getLeasedBlobs()).doesNotContain(blobHandle);
            } catch (IOException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public StorageStats queryStatsForPackage() {
            final StorageStatsManager storageStatsManager = getSystemService(
                    StorageStatsManager.class);
            try {
                return storageStatsManager
                        .queryStatsForPackage(UUID_DEFAULT, getPackageName(), getUser());
            } catch (IOException | NameNotFoundException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public StorageStats queryStatsForUid() {
            final StorageStatsManager storageStatsManager = getSystemService(
                    StorageStatsManager.class);
            try {
                return storageStatsManager
                        .queryStatsForUid(UUID_DEFAULT, Process.myUid());
            } catch (IOException e) {
                throw new IllegalStateException(e);
            }
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            try {
                return super.onTransact(code, data, reply, flags);
            } catch (AssertionError e) {
                throw new IllegalStateException(e);
            }
        }
    }
}
