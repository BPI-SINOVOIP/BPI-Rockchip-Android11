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

import static com.google.common.truth.Truth.assertThat;

import android.app.blob.BlobHandle;
import android.app.blob.BlobStoreManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;

import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.utils.blob.DummyBlobData;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Base64;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

@RunWith(AndroidJUnit4.class)
public class DataPersistenceTest extends BaseBlobStoreDeviceTest {

    @Test
    public void testCreateSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setRandomSeed(22)
                .setFileName("test_blob_data")
                .build();
        blobData.prepare();

        final long sessionId = createSession(blobData.getBlobHandle());
        assertThat(sessionId).isGreaterThan(0L);
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, 0, PARTIAL_FILE_LENGTH_BYTES);
        }
        writeSessionIdToDisk(sessionId);
        writeBlobHandleToDisk(blobData.getBlobHandle());

        ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mBlobStoreManager,
                (blobStoreManager) -> blobStoreManager.waitForIdle(TIMEOUT_WAIT_FOR_IDLE_MS),
                Exception.class, android.Manifest.permission.DUMP);
    }

    @Test
    public void testOpenSessionAndWrite() throws Exception {
        final long sessionId = readSessionIdFromDisk();
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setRandomSeed(22)
                .setFileName("test_blob_data")
                .build();
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            assertThat(session.getSize()).isEqualTo(PARTIAL_FILE_LENGTH_BYTES);
            blobData.writeToSession(session, PARTIAL_FILE_LENGTH_BYTES,
                    blobData.getFileSize() - PARTIAL_FILE_LENGTH_BYTES);
        }
    }

    @Test
    public void testCommitSession() throws Exception {
        final long sessionId = readSessionIdFromDisk();
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            final CompletableFuture<Integer> callback = new CompletableFuture<>();
            session.commit(mContext.getMainExecutor(), callback::complete);
            assertThat(callback.get(TIMEOUT_COMMIT_CALLBACK_MS, TimeUnit.MILLISECONDS))
                    .isEqualTo(0);
        }
    }

    @Test
    public void testOpenBlob() throws Exception {
        final BlobHandle blobHandle = readBlobHandleFromDisk();
        try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobHandle)) {
            assertThat(pfd).isNotNull();
        }
    }

    private void writeSessionIdToDisk(long sessionId) {
        final SharedPreferences sharedPref = getSharedPreferences();
        assertThat(sharedPref.edit().putLong(KEY_SESSION_ID, sessionId).commit())
                .isTrue();
    }

    private long readSessionIdFromDisk() {
        final SharedPreferences sharedPref = getSharedPreferences();
        final long sessionId = sharedPref.getLong(KEY_SESSION_ID, -1);
        assertThat(sessionId).isNotEqualTo(-1);
        return sessionId;
    }

    private void writeBlobHandleToDisk(BlobHandle handle) {
        final SharedPreferences.Editor sharedPrefEditor = getSharedPreferences().edit();
        sharedPrefEditor.putString(KEY_DIGEST, Base64.getEncoder().encodeToString(
                handle.getSha256Digest()));
        sharedPrefEditor.putString(KEY_LABEL, handle.getLabel().toString());
        sharedPrefEditor.putLong(KEY_EXPIRY, handle.getExpiryTimeMillis());
        sharedPrefEditor.putString(KEY_TAG, handle.getTag());
        assertThat(sharedPrefEditor.commit()).isTrue();
    }

    private BlobHandle readBlobHandleFromDisk() {
        final SharedPreferences sharedPref = getSharedPreferences();
        final byte[] digest = Base64.getDecoder().decode(sharedPref.getString(KEY_DIGEST, null));
        final CharSequence label = sharedPref.getString(KEY_LABEL, null);
        final long expiryMillis = sharedPref.getLong(KEY_EXPIRY, -1);
        final String tag = sharedPref.getString(KEY_TAG, null);
        return BlobHandle.createWithSha256(digest, label, expiryMillis, tag);
    }

    private SharedPreferences getSharedPreferences() {
        return mContext.createDeviceProtectedStorageContext().getSharedPreferences(
                mContext.getPackageName(), Context.MODE_PRIVATE);
    }
}
