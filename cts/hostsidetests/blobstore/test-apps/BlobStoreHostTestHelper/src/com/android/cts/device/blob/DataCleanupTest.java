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

import static org.testng.Assert.assertThrows;
import static org.testng.Assert.fail;

import android.app.blob.BlobHandle;
import android.app.blob.BlobStoreManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import com.android.utils.blob.DummyBlobData;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Base64;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

@RunWith(AndroidJUnit4.class)
public class DataCleanupTest extends BaseBlobStoreDeviceTest {

    @Test
    public void testCreateSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setRandomSeed(24)
                .setFileName("test_blob_data")
                .build();
        blobData.prepare();

        final long sessionId = createSession(blobData.getBlobHandle());
        assertThat(sessionId).isGreaterThan(0L);
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, 0, PARTIAL_FILE_LENGTH_BYTES);
        }
        addSessionIdToResults(sessionId);
    }

    @Test
    public void testOpenSession() throws Exception {
        final long sessionId = getSessionIdFromArgs();
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            assertThat(session.getSize()).isEqualTo(PARTIAL_FILE_LENGTH_BYTES);
        }
    }

    @Test
    public void testOpenSession_shouldThrow() throws Exception {
        final long sessionId = getSessionIdFromArgs();
        try {
            mBlobStoreManager.openSession(sessionId);
            fail("Should throw");
        } catch (SecurityException e) {
            // expected
        }
        assertThrows(SecurityException.class,
                () -> mBlobStoreManager.openSession(sessionId));
    }

    @Test
    public void testCommitBlob() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setRandomSeed(24)
                .setFileName("test_blob_data")
                .setLabel("test_data_blob_label")
                .build();
        blobData.prepare();

        final long sessionId = createSession(blobData.getBlobHandle());
        assertThat(sessionId).isGreaterThan(0L);
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session);
            if (getShouldAllowPublicFromArgs()) {
                session.allowPublicAccess();
            }
            final CompletableFuture<Integer> callback = new CompletableFuture<>();
            session.commit(mContext.getMainExecutor(), callback::complete);
            assertThat(callback.get(TIMEOUT_COMMIT_CALLBACK_MS, TimeUnit.MILLISECONDS))
                    .isEqualTo(0);
        }
        addBlobHandleToResults(blobData.getBlobHandle());
    }

    @Test
    public void testOpenBlob() throws Exception {
        final BlobHandle blobHandle = getBlobHandleFromArgs();
        try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobHandle)) {
            assertThat(pfd).isNotNull();
        }
    }

    @Test
    public void testOpenBlob_shouldThrow() throws Exception {
        final BlobHandle blobHandle = getBlobHandleFromArgs();
        assertThrows(SecurityException.class,
                () -> mBlobStoreManager.openBlob(blobHandle));
    }

    private void addSessionIdToResults(long sessionId) {
        final Bundle results = new Bundle();
        results.putLong(KEY_SESSION_ID, sessionId);
        mInstrumentation.addResults(results);
    }

    private long getSessionIdFromArgs() {
        final Bundle args = InstrumentationRegistry.getArguments();
        return Long.parseLong(args.getString(KEY_SESSION_ID));
    }

    private void addBlobHandleToResults(BlobHandle blobHandle) {
        final Bundle results = new Bundle();
        results.putString(KEY_DIGEST,
                Base64.getEncoder().encodeToString(blobHandle.getSha256Digest()));
        results.putLong(KEY_EXPIRY, blobHandle.getExpiryTimeMillis());
        results.putCharSequence(KEY_LABEL, blobHandle.getLabel().toString());
        results.putString(KEY_TAG, blobHandle.getTag());
        mInstrumentation.addResults(results);
    }

    private BlobHandle getBlobHandleFromArgs() {
        final Bundle args = InstrumentationRegistry.getArguments();
        final byte[] digest = Base64.getDecoder().decode(args.getString(KEY_DIGEST));
        final CharSequence label = args.getString(KEY_LABEL);
        final long expiryTimeMillis = Long.parseLong(args.getString(KEY_EXPIRY));
        final String tag = args.getString(KEY_TAG);
        return BlobHandle.createWithSha256(digest, label, expiryTimeMillis, tag);
    }

    private boolean getShouldAllowPublicFromArgs() {
        final Bundle args = InstrumentationRegistry.getArguments();
        return "1".equals(args.getString(KEY_ALLOW_PUBLIC));
    }
}
