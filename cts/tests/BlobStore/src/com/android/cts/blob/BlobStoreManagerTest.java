/*
 * Copyright 2019 The Android Open Source Project
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

import static android.os.storage.StorageManager.UUID_DEFAULT;

import static com.android.utils.blob.Utils.acquireLease;
import static com.android.utils.blob.Utils.assertLeasedBlobs;
import static com.android.utils.blob.Utils.assertNoLeasedBlobs;
import static com.android.utils.blob.Utils.releaseLease;
import static com.android.utils.blob.Utils.TAG;
import static com.android.utils.blob.Utils.runShellCmd;
import static com.android.utils.blob.Utils.triggerIdleMaintenance;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.app.blob.BlobHandle;
import android.app.blob.BlobStoreManager;
import android.app.usage.StorageStats;
import android.app.usage.StorageStatsManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Environment;
import android.os.IBinder;
import android.os.LimitExceededException;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.SystemClock;
import android.provider.DeviceConfig;
import android.util.ArrayMap;
import android.util.Log;
import android.util.LongSparseArray;
import android.util.Pair;

import com.android.compatibility.common.util.SystemUtil;
import com.android.compatibility.common.util.ThrowingRunnable;
import com.android.cts.blob.R;
import com.android.cts.blob.ICommandReceiver;
import com.android.utils.blob.DummyBlobData;
import com.android.utils.blob.Utils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;
import java.util.Objects;
import java.util.Random;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

import androidx.test.platform.app.InstrumentationRegistry;

import com.google.common.io.BaseEncoding;

@RunWith(BlobStoreTestRunner.class)
public class BlobStoreManagerTest {

    private static final long TIMEOUT_COMMIT_CALLBACK_SEC = 5;

    private static final long TIMEOUT_BIND_SERVICE_SEC = 2;

    private static final long TIMEOUT_WAIT_FOR_IDLE_MS = 2_000;

    // TODO: Make it a @TestApi or move the test using this to a different location.
    // Copy of DeviceConfig.NAMESPACE_BLOBSTORE constant
    private static final String NAMESPACE_BLOBSTORE = "blobstore";
    private static final String KEY_SESSION_EXPIRY_TIMEOUT_MS = "session_expiry_timeout_ms";
    private static final String KEY_LEASE_ACQUISITION_WAIT_DURATION_MS =
            "lease_acquisition_wait_time_ms";
    private static final String KEY_DELETE_ON_LAST_LEASE_DELAY_MS =
            "delete_on_last_lease_delay_ms";
    private static final String KEY_TOTAL_BYTES_PER_APP_LIMIT_FLOOR =
            "total_bytes_per_app_limit_floor";
    private static final String KEY_TOTAL_BYTES_PER_APP_LIMIT_FRACTION =
            "total_bytes_per_app_limit_fraction";
    private static final String KEY_MAX_ACTIVE_SESSIONS = "max_active_sessions";
    private static final String KEY_MAX_COMMITTED_BLOBS = "max_committed_blobs";
    private static final String KEY_MAX_LEASED_BLOBS = "max_leased_blobs";
    private static final String KEY_MAX_BLOB_ACCESS_PERMITTED_PACKAGES = "max_permitted_pks";

    private static final String HELPER_PKG = "com.android.cts.blob.helper";
    private static final String HELPER_PKG2 = "com.android.cts.blob.helper2";
    private static final String HELPER_PKG3 = "com.android.cts.blob.helper3";

    private static final String HELPER_SERVICE = HELPER_PKG + ".BlobStoreTestService";

    private static final byte[] HELPER_PKG2_CERT_SHA256 = BaseEncoding.base16().decode(
            "187E3D3172F2177D6FEC2EA53785BF1E25DFF7B2E5F6E59807E365A7A837E6C3");
    private static final byte[] HELPER_PKG3_CERT_SHA256 = BaseEncoding.base16().decode(
            "D760873D812FE1CFC02C15ED416AB774B2D4C2E936DF6D8B6707277479D4812F");

    private Context mContext;
    private BlobStoreManager mBlobStoreManager;

    private static final byte[] EMPTY_BYTE_ARRAY = new byte[0];

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mBlobStoreManager = (BlobStoreManager) mContext.getSystemService(
                Context.BLOB_STORE_SERVICE);
    }

    @After
    public void tearDown() throws Exception {
        runShellCmd("cmd blob_store clear-all-sessions");
        runShellCmd("cmd blob_store clear-all-blobs");
        mContext.getFilesDir().delete();
    }

    @Test
    public void testGetCreateSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);
            assertThat(mBlobStoreManager.openSession(sessionId)).isNotNull();
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testCreateBlobHandle_invalidArguments() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final BlobHandle handle = blobData.getBlobHandle();
        try {
            assertThrows(IllegalArgumentException.class, () -> BlobHandle.createWithSha256(
                    handle.getSha256Digest(), null, handle.getExpiryTimeMillis(), handle.getTag()));
            assertThrows(IllegalArgumentException.class, () -> BlobHandle.createWithSha256(
                    handle.getSha256Digest(), handle.getLabel(), handle.getExpiryTimeMillis(),
                    null));
            assertThrows(IllegalArgumentException.class, () -> BlobHandle.createWithSha256(
                    handle.getSha256Digest(), handle.getLabel(), -1, handle.getTag()));
            assertThrows(IllegalArgumentException.class, () -> BlobHandle.createWithSha256(
                    EMPTY_BYTE_ARRAY, handle.getLabel(), handle.getExpiryTimeMillis(),
                    handle.getTag()));
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testGetCreateSession_invalidArguments() throws Exception {
        assertThrows(NullPointerException.class, () -> mBlobStoreManager.createSession(null));
    }

    @Test
    public void testOpenSession_invalidArguments() throws Exception {
        assertThrows(IllegalArgumentException.class, () -> mBlobStoreManager.openSession(-1));
    }

    @Test
    public void testAbandonSession_invalidArguments() throws Exception {
        assertThrows(IllegalArgumentException.class, () -> mBlobStoreManager.abandonSession(-1));
    }

    @Test
    public void testAbandonSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);
            // Verify that session can be opened.
            assertThat(mBlobStoreManager.openSession(sessionId)).isNotNull();

            mBlobStoreManager.abandonSession(sessionId);
            // Verify that trying to open session after it is deleted will throw.
            assertThrows(SecurityException.class, () -> mBlobStoreManager.openSession(sessionId));
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testOpenReadWriteSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);
            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session, 0, blobData.getFileSize());
                blobData.readFromSessionAndVerifyDigest(session);
                blobData.readFromSessionAndVerifyBytes(session,
                        101 /* offset */, 1001 /* length */);

                blobData.writeToSession(session, 202 /* offset */, 2002 /* length */,
                        blobData.getFileSize());
                blobData.readFromSessionAndVerifyBytes(session,
                        202 /* offset */, 2002 /* length */);

                commitSession(sessionId, session, blobData.getBlobHandle());
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testOpenSession_fromAnotherPkg() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);
            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                assertThat(session).isNotNull();
                session.allowPublicAccess();
            }
            assertThrows(SecurityException.class, () -> openSessionFromPkg(sessionId, HELPER_PKG));
            assertThrows(SecurityException.class, () -> openSessionFromPkg(sessionId, HELPER_PKG2));

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session, 0, blobData.getFileSize());
                blobData.readFromSessionAndVerifyDigest(session);
                session.allowPublicAccess();
            }
            assertThrows(SecurityException.class, () -> openSessionFromPkg(sessionId, HELPER_PKG));
            assertThrows(SecurityException.class, () -> openSessionFromPkg(sessionId, HELPER_PKG2));
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testOpenSessionAndAbandon() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                // Verify session can be opened for read/write.
                assertThat(session).isNotNull();
                assertThat(session.openWrite(0, 0)).isNotNull();
                assertThat(session.openRead()).isNotNull();

                // Verify that trying to read/write to the session after it is abandoned will throw.
                session.abandon();
                assertThrows(IllegalStateException.class, () -> session.openWrite(0, 0));
                assertThrows(IllegalStateException.class, () -> session.openRead());
            }

            // Verify that trying to open the session after it is abandoned will throw.
            assertThrows(SecurityException.class, () -> mBlobStoreManager.openSession(sessionId));
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testCloseSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            // Verify session can be opened for read/write.
            BlobStoreManager.Session session = null;
            try {
                session = mBlobStoreManager.openSession(sessionId);
                assertThat(session).isNotNull();
                assertThat(session.openWrite(0, 0)).isNotNull();
                assertThat(session.openRead()).isNotNull();
            } finally {
                session.close();
            }

            // Verify trying to read/write to session after it is closed will throw.
            // an exception.
            final BlobStoreManager.Session closedSession = session;
            assertThrows(IllegalStateException.class, () -> closedSession.openWrite(0, 0));
            assertThrows(IllegalStateException.class, () -> closedSession.openRead());

            // Verify that the session can be opened again for read/write.
            try {
                session = mBlobStoreManager.openSession(sessionId);
                assertThat(session).isNotNull();
                assertThat(session.openWrite(0, 0)).isNotNull();
                assertThat(session.openRead()).isNotNull();
            } finally {
                session.close();
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowPublicAccess() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData, session -> {
                session.allowPublicAccess();
                assertThat(session.isPublicAccessAllowed()).isTrue();
            });

            assertPkgCanAccess(blobData, HELPER_PKG);
            assertPkgCanAccess(blobData, HELPER_PKG2);
            assertPkgCanAccess(blobData, HELPER_PKG3);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowPublicAccess_abandonedSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                session.allowPublicAccess();
                assertThat(session.isPublicAccessAllowed()).isTrue();

                session.abandon();
                assertThrows(IllegalStateException.class,
                        () -> session.allowPublicAccess());
                assertThrows(IllegalStateException.class,
                        () -> session.isPublicAccessAllowed());
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowSameSignatureAccess() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData, session -> {
                session.allowSameSignatureAccess();
                assertThat(session.isSameSignatureAccessAllowed()).isTrue();
            });

            assertPkgCanAccess(blobData, HELPER_PKG);
            assertPkgCannotAccess(blobData, HELPER_PKG2);
            assertPkgCannotAccess(blobData, HELPER_PKG3);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowSameSignatureAccess_abandonedSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                session.allowSameSignatureAccess();
                assertThat(session.isSameSignatureAccessAllowed()).isTrue();

                session.abandon();
                assertThrows(IllegalStateException.class,
                        () -> session.allowSameSignatureAccess());
                assertThrows(IllegalStateException.class,
                        () -> session.isSameSignatureAccessAllowed());
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowPackageAccess() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData, session -> {
                session.allowPackageAccess(HELPER_PKG2, HELPER_PKG2_CERT_SHA256);
                assertThat(session.isPackageAccessAllowed(HELPER_PKG2, HELPER_PKG2_CERT_SHA256))
                        .isTrue();
            });

            assertPkgCannotAccess(blobData, HELPER_PKG);
            assertPkgCanAccess(blobData, HELPER_PKG2);
            assertPkgCannotAccess(blobData, HELPER_PKG3);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowPackageAccess_allowMultiple() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData, session -> {
                session.allowPackageAccess(HELPER_PKG2, HELPER_PKG2_CERT_SHA256);
                session.allowPackageAccess(HELPER_PKG3, HELPER_PKG3_CERT_SHA256);
                assertThat(session.isPackageAccessAllowed(HELPER_PKG2, HELPER_PKG2_CERT_SHA256))
                        .isTrue();
                assertThat(session.isPackageAccessAllowed(HELPER_PKG3, HELPER_PKG3_CERT_SHA256))
                        .isTrue();
            });

            assertPkgCannotAccess(blobData, HELPER_PKG);
            assertPkgCanAccess(blobData, HELPER_PKG2);
            assertPkgCanAccess(blobData, HELPER_PKG3);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAllowPackageAccess_abandonedSession() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                session.allowPackageAccess("com.example", "test_bytes".getBytes());
                assertThat(session.isPackageAccessAllowed("com.example", "test_bytes".getBytes()))
                        .isTrue();

                session.abandon();
                assertThrows(IllegalStateException.class,
                        () -> session.allowPackageAccess(
                                "com.example2", "test_bytes2".getBytes()));
                assertThrows(IllegalStateException.class,
                        () -> session.isPackageAccessAllowed(
                                "com.example", "test_bytes".getBytes()));
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testPrivateAccess() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final TestServiceConnection connection1 = bindToHelperService(HELPER_PKG);
        final TestServiceConnection connection2 = bindToHelperService(HELPER_PKG2);
        final TestServiceConnection connection3 = bindToHelperService(HELPER_PKG3);
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData);

            assertPkgCannotAccess(blobData, connection1);
            assertPkgCannotAccess(blobData, connection2);
            assertPkgCannotAccess(blobData, connection3);

            commitBlobFromPkg(blobData, connection1);
            assertPkgCanAccess(blobData, connection1);
            assertPkgCannotAccess(blobData, connection2);
            assertPkgCannotAccess(blobData, connection3);

            commitBlobFromPkg(blobData, connection2);
            assertPkgCanAccess(blobData, connection1);
            assertPkgCanAccess(blobData, connection2);
            assertPkgCannotAccess(blobData, connection3);
        } finally {
            blobData.delete();
            connection1.unbind();
            connection2.unbind();
            connection3.unbind();
        }
    }

    @Test
    public void testMixedAccessType() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData, session -> {
                session.allowSameSignatureAccess();
                session.allowPackageAccess(HELPER_PKG3, HELPER_PKG3_CERT_SHA256);
                assertThat(session.isSameSignatureAccessAllowed()).isTrue();
                assertThat(session.isPackageAccessAllowed(HELPER_PKG3, HELPER_PKG3_CERT_SHA256))
                        .isTrue();
                assertThat(session.isPublicAccessAllowed()).isFalse();
            });

            assertPkgCanAccess(blobData, HELPER_PKG);
            assertPkgCannotAccess(blobData, HELPER_PKG2);
            assertPkgCanAccess(blobData, HELPER_PKG3);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testMixedAccessType_fromMultiplePackages() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final TestServiceConnection connection1 = bindToHelperService(HELPER_PKG);
        final TestServiceConnection connection2 = bindToHelperService(HELPER_PKG2);
        final TestServiceConnection connection3 = bindToHelperService(HELPER_PKG3);
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData, session -> {
                session.allowSameSignatureAccess();
                session.allowPackageAccess(HELPER_PKG2, HELPER_PKG2_CERT_SHA256);
                assertThat(session.isSameSignatureAccessAllowed()).isTrue();
                assertThat(session.isPackageAccessAllowed(HELPER_PKG2, HELPER_PKG2_CERT_SHA256))
                        .isTrue();
                assertThat(session.isPublicAccessAllowed()).isFalse();
            });

            assertPkgCanAccess(blobData, connection1);
            assertPkgCanAccess(blobData, connection2);
            assertPkgCannotAccess(blobData, connection3);

            commitBlobFromPkg(blobData, ICommandReceiver.FLAG_ACCESS_TYPE_PUBLIC, connection2);

            assertPkgCanAccess(blobData, connection1);
            assertPkgCanAccess(blobData, connection2);
            assertPkgCanAccess(blobData, connection3);
        } finally {
            blobData.delete();
            connection1.unbind();
            connection2.unbind();
            connection3.unbind();
        }
    }

    @Test
    public void testSessionCommit() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session);

                final ParcelFileDescriptor pfd = session.openWrite(
                        0L /* offset */, 0L /* length */);
                assertThat(pfd).isNotNull();
                blobData.writeToFd(pfd.getFileDescriptor(), 0 /* offset */, 100 /* length */);

                commitSession(sessionId, session, blobData.getBlobHandle());

                // Verify that writing to the session after commit will throw.
                assertThrows(IOException.class, () -> blobData.writeToFd(
                        pfd.getFileDescriptor() /* length */, 0 /* offset */, 100 /* length */));
                assertThrows(IllegalStateException.class, () -> session.openWrite(0L, 0L));
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testSessionCommit_incompleteData() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session, 0, blobData.getFileSize() - 2);

                commitSession(sessionId, session, blobData.getBlobHandle(),
                        false /* expectSuccess */);
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testSessionCommit_incorrectData() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session, 0, blobData.getFileSize());
                try (OutputStream out = new ParcelFileDescriptor.AutoCloseOutputStream(
                        session.openWrite(0, blobData.getFileSize()))) {
                    out.write("wrong_data".getBytes(StandardCharsets.UTF_8));
                }

                commitSession(sessionId, session, blobData.getBlobHandle(),
                        false /* expectSuccess */);
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testRecommitBlob() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();

        try {
            commitBlob(blobData);
            // Verify that blob can be accessed after committing.
            try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
                assertThat(pfd).isNotNull();
                blobData.verifyBlob(pfd);
            }

            commitBlob(blobData);
            // Verify that blob can be accessed after re-committing.
            try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
                assertThat(pfd).isNotNull();
                blobData.verifyBlob(pfd);
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testRecommitBlob_fromMultiplePackages() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final TestServiceConnection connection = bindToHelperService(HELPER_PKG);
        try {
            commitBlob(blobData);
            // Verify that blob can be accessed after committing.
            try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
                assertThat(pfd).isNotNull();
                blobData.verifyBlob(pfd);
            }

            commitBlobFromPkg(blobData, connection);
            // Verify that blob can be accessed after re-committing.
            try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
                assertThat(pfd).isNotNull();
                blobData.verifyBlob(pfd);
            }
            assertPkgCanAccess(blobData, connection);
        } finally {
            blobData.delete();
            connection.unbind();
        }
    }

    @Test
    public void testSessionCommit_largeBlob() throws Exception {
        final long fileSizeBytes = Math.min(mBlobStoreManager.getRemainingLeaseQuotaBytes(),
                150 * 1024L * 1024L);
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setFileSize(fileSizeBytes)
                .build();
        blobData.prepare();
        final long commitTimeoutSec = TIMEOUT_COMMIT_CALLBACK_SEC * 2;
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);
            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session);

                Log.d(TAG, "Committing session: " + sessionId
                        + "; blob: " + blobData.getBlobHandle());
                final CompletableFuture<Integer> callback = new CompletableFuture<>();
                session.commit(mContext.getMainExecutor(), callback::complete);
                assertThat(callback.get(commitTimeoutSec, TimeUnit.SECONDS))
                        .isEqualTo(0);
            }

            // Verify that blob can be accessed after committing.
            try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
                assertThat(pfd).isNotNull();
                blobData.verifyBlob(pfd);
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testCommitSession_multipleWrites() throws Exception {
        final int numThreads = 2;
        final Random random = new Random(0);
        final ExecutorService executorService = Executors.newFixedThreadPool(numThreads);
        final CompletableFuture<Throwable>[] completableFutures = new CompletableFuture[numThreads];
        for (int i = 0; i < numThreads; ++i) {
            completableFutures[i] = CompletableFuture.supplyAsync(() -> {
                final int minSizeMb = 30;
                final long fileSizeBytes = (minSizeMb + random.nextInt(minSizeMb)) * 1024L * 1024L;
                final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                        .setFileSize(fileSizeBytes)
                        .build();
                try {
                    blobData.prepare();
                    commitAndVerifyBlob(blobData);
                } catch (Throwable t) {
                    return t;
                } finally {
                    blobData.delete();
                }
                return null;
            }, executorService);
        }
        final ArrayList<Throwable> invalidResults = new ArrayList<>();
        for (int i = 0; i < numThreads; ++i) {
             final Throwable result = completableFutures[i].get();
             if (result != null) {
                 invalidResults.add(result);
             }
        }
        assertThat(invalidResults).isEmpty();
    }

    @Test
    public void testCommitSession_multipleReadWrites() throws Exception {
        final int numThreads = 2;
        final Random random = new Random(0);
        final ExecutorService executorService = Executors.newFixedThreadPool(numThreads);
        final CompletableFuture<Throwable>[] completableFutures = new CompletableFuture[numThreads];
        for (int i = 0; i < numThreads; ++i) {
            completableFutures[i] = CompletableFuture.supplyAsync(() -> {
                final int minSizeMb = 30;
                final long fileSizeBytes = (minSizeMb + random.nextInt(minSizeMb)) * 1024L * 1024L;
                final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                        .setFileSize(fileSizeBytes)
                        .build();
                try {
                    blobData.prepare();
                    final long sessionId = mBlobStoreManager.createSession(
                            blobData.getBlobHandle());
                    assertThat(sessionId).isGreaterThan(0L);
                    try (BlobStoreManager.Session session = mBlobStoreManager.openSession(
                            sessionId)) {
                        final long partialFileSizeBytes = minSizeMb * 1024L * 1024L;
                        blobData.writeToSession(session, 0L, partialFileSizeBytes,
                                blobData.getFileSize());
                        blobData.readFromSessionAndVerifyBytes(session, 0L,
                                (int) partialFileSizeBytes);
                        blobData.writeToSession(session, partialFileSizeBytes,
                                blobData.getFileSize() - partialFileSizeBytes,
                                blobData.getFileSize());
                        blobData.readFromSessionAndVerifyBytes(session, partialFileSizeBytes,
                                (int) (blobData.getFileSize() - partialFileSizeBytes));

                        commitSession(sessionId, session, blobData.getBlobHandle());
                    }

                    // Verify that blob can be accessed after committing.
                    try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(
                            blobData.getBlobHandle())) {
                        assertThat(pfd).isNotNull();
                        blobData.verifyBlob(pfd);
                    }
                } catch (Throwable t) {
                    return t;
                } finally {
                    blobData.delete();
                }
                return null;
            }, executorService);
        }
        final ArrayList<Throwable> invalidResults = new ArrayList<>();
        for (int i = 0; i < numThreads; ++i) {
            final Throwable result = completableFutures[i].get();
            if (result != null) {
                invalidResults.add(result);
            }
        }
        assertThat(invalidResults).isEmpty();
    }

    @Test
    public void testOpenBlob() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session);

                // Verify that trying to accessed the blob before commit throws
                assertThrows(SecurityException.class,
                        () -> mBlobStoreManager.openBlob(blobData.getBlobHandle()));

                commitSession(sessionId, session, blobData.getBlobHandle());
            }

            // Verify that blob can be accessed after committing.
            try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
                assertThat(pfd).isNotNull();

                blobData.verifyBlob(pfd);
            }
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testOpenBlob_invalidArguments() throws Exception {
        assertThrows(NullPointerException.class, () -> mBlobStoreManager.openBlob(null));
    }

    @Test
    public void testAcquireReleaseLease() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            commitBlob(blobData);

            assertThrows(IllegalArgumentException.class, () ->
                    acquireLease(mContext, blobData.getBlobHandle(),
                            R.string.test_desc, blobData.getExpiryTimeMillis() + 1000));
            assertNoLeasedBlobs(mBlobStoreManager);

            acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                    blobData.getExpiryTimeMillis() - 1000);
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());
            acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc);
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());
            releaseLease(mContext, blobData.getBlobHandle());
            assertNoLeasedBlobs(mBlobStoreManager);

            acquireLease(mContext, blobData.getBlobHandle(), "Test description",
                    blobData.getExpiryTimeMillis() - 20000);
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());
            acquireLease(mContext, blobData.getBlobHandle(), "Test description two");
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());
            releaseLease(mContext, blobData.getBlobHandle());
            assertNoLeasedBlobs(mBlobStoreManager);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAcquireLease_multipleLeases() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        final DummyBlobData blobData2 = new DummyBlobData.Builder(mContext)
                .setRandomSeed(42)
                .build();
        blobData.prepare();
        blobData2.prepare();
        try {
            commitBlob(blobData);

            acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                    blobData.getExpiryTimeMillis() - 1000);
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

            commitBlob(blobData2);

            acquireLease(mContext, blobData2.getBlobHandle(), "Test desc2",
                    blobData.getExpiryTimeMillis() - 2000);
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle(),
                    blobData2.getBlobHandle());

            releaseLease(mContext, blobData.getBlobHandle());
            assertLeasedBlobs(mBlobStoreManager, blobData2.getBlobHandle());

            releaseLease(mContext, blobData2.getBlobHandle());
            assertNoLeasedBlobs(mBlobStoreManager);
        } finally {
            blobData.delete();
            blobData2.delete();
        }
    }

    @Test
    public void testAcquireLease_leaseExpired() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);
        try {
            commitBlob(blobData);

            acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                    System.currentTimeMillis() + waitDurationMs);
            assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

            waitForLeaseExpiration(waitDurationMs, blobData.getBlobHandle());
            assertNoLeasedBlobs(mBlobStoreManager);
        } finally {
            blobData.delete();
        }
    }

    @Test
    public void testAcquireRelease_deleteAfterDelay() throws Exception {
        final long waitDurationMs = TimeUnit.SECONDS.toMillis(1);
        runWithKeyValues(() -> {
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
            blobData.prepare();
            try {
                commitBlob(blobData);

                acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                        blobData.getExpiryTimeMillis());
                assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

                SystemClock.sleep(waitDurationMs);

                releaseLease(mContext, blobData.getBlobHandle());
                assertNoLeasedBlobs(mBlobStoreManager);

                SystemClock.sleep(waitDurationMs);
                SystemUtil.runWithShellPermissionIdentity(() ->
                        mBlobStoreManager.waitForIdle(TIMEOUT_WAIT_FOR_IDLE_MS));

                assertThrows(SecurityException.class, () -> mBlobStoreManager.acquireLease(
                        blobData.getBlobHandle(), R.string.test_desc,
                        blobData.getExpiryTimeMillis()));
                assertNoLeasedBlobs(mBlobStoreManager);
            } finally {
                blobData.delete();
            }
        }, Pair.create(KEY_LEASE_ACQUISITION_WAIT_DURATION_MS, String.valueOf(waitDurationMs)),
                Pair.create(KEY_DELETE_ON_LAST_LEASE_DELAY_MS, String.valueOf(waitDurationMs)));
    }

    @Test
    public void testAcquireReleaseLease_invalidArguments() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        try {
            assertThrows(NullPointerException.class, () -> mBlobStoreManager.acquireLease(
                    null, R.string.test_desc, blobData.getExpiryTimeMillis()));
            assertThrows(IllegalArgumentException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), R.string.test_desc, -1));
            assertThrows(IllegalArgumentException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), -1));
            assertThrows(IllegalArgumentException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), null));
            assertThrows(IllegalArgumentException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), null, blobData.getExpiryTimeMillis()));
        } finally {
            blobData.delete();
        }

        assertThrows(NullPointerException.class, () -> mBlobStoreManager.releaseLease(null));
    }

    @Test
    public void testStorageAttributedToSelf() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final long partialFileSize = 3373L;

        final StorageStatsManager storageStatsManager = mContext.getSystemService(
                StorageStatsManager.class);
        StorageStats beforeStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        StorageStats beforeStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // Create a session and write some data.
        final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
        assertThat(sessionId).isGreaterThan(0L);
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, 0, partialFileSize, partialFileSize);
        }

        StorageStats afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        StorageStats afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // 'partialFileSize' bytes were written, verify the size increase.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(partialFileSize);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(partialFileSize);

        // Complete writing data.
        final long totalFileSize = blobData.getFileSize();
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, partialFileSize, totalFileSize - partialFileSize,
                    totalFileSize);
        }

        afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // 'totalFileSize' bytes were written so far, verify the size increase.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(totalFileSize);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(totalFileSize);

        // Commit the session.
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, partialFileSize,
                    session.getSize() - partialFileSize, blobData.getFileSize());
            commitSession(sessionId, session, blobData.getBlobHandle());
        }

        acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc);
        assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

        afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // Session was committed but no one else is using it, verify the size increase stays
        // the same as earlier.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(totalFileSize);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(totalFileSize);

        releaseLease(mContext, blobData.getBlobHandle());
        assertNoLeasedBlobs(mBlobStoreManager);

        afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // No leases on the blob, so it should not be attributed.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(0L);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(0L);
    }

    @Test
    public void testStorageAttribution_acquireLease() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();

        final StorageStatsManager storageStatsManager = mContext.getSystemService(
                StorageStatsManager.class);
        StorageStats beforeStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        StorageStats beforeStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session);
            session.allowPublicAccess();

            commitSession(sessionId, session, blobData.getBlobHandle());
        }

        StorageStats afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        StorageStats afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // No leases on the blob, so it should not be attributed.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(0L);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(0L);

        final TestServiceConnection serviceConnection = bindToHelperService(HELPER_PKG);
        final ICommandReceiver commandReceiver = serviceConnection.getCommandReceiver();
        try {
            StorageStats beforeStatsForHelperPkg = commandReceiver.queryStatsForPackage();
            StorageStats beforeStatsForHelperUid = commandReceiver.queryStatsForUid();

            commandReceiver.acquireLease(blobData.getBlobHandle());

            StorageStats afterStatsForHelperPkg = commandReceiver.queryStatsForPackage();
            StorageStats afterStatsForHelperUid = commandReceiver.queryStatsForUid();

            assertThat(
                    afterStatsForHelperPkg.getDataBytes() - beforeStatsForHelperPkg.getDataBytes())
                    .isEqualTo(blobData.getFileSize());
            assertThat(
                    afterStatsForHelperUid.getDataBytes() - beforeStatsForHelperUid.getDataBytes())
                    .isEqualTo(blobData.getFileSize());

            afterStatsForPkg = storageStatsManager
                    .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(),
                            mContext.getUser());
            afterStatsForUid = storageStatsManager
                    .queryStatsForUid(UUID_DEFAULT, Process.myUid());

            // There shouldn't be no change in stats for this package
            assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                    .isEqualTo(0L);
            assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                    .isEqualTo(0L);

            commandReceiver.releaseLease(blobData.getBlobHandle());

            afterStatsForHelperPkg = commandReceiver.queryStatsForPackage();
            afterStatsForHelperUid = commandReceiver.queryStatsForUid();

            // Lease is released, so it should not be attributed anymore.
            assertThat(
                    afterStatsForHelperPkg.getDataBytes() - beforeStatsForHelperPkg.getDataBytes())
                    .isEqualTo(0L);
            assertThat(
                    afterStatsForHelperUid.getDataBytes() - beforeStatsForHelperUid.getDataBytes())
                    .isEqualTo(0L);
        } finally {
            serviceConnection.unbind();
        }
    }

    @Test
    public void testStorageAttribution_withExpiredLease() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();

        final StorageStatsManager storageStatsManager = mContext.getSystemService(
                StorageStatsManager.class);
        StorageStats beforeStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        StorageStats beforeStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        commitBlob(blobData);

        StorageStats afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        StorageStats afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // No leases on the blob, so it should not be attributed.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(0L);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(0L);

        final long leaseExpiryDurationMs = TimeUnit.SECONDS.toMillis(5);
        acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                System.currentTimeMillis() + leaseExpiryDurationMs);

        final long startTimeMs = System.currentTimeMillis();
        afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(blobData.getFileSize());
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(blobData.getFileSize());

        waitForLeaseExpiration(
                Math.abs(leaseExpiryDurationMs - (System.currentTimeMillis() - startTimeMs)),
                blobData.getBlobHandle());

        afterStatsForPkg = storageStatsManager
                .queryStatsForPackage(UUID_DEFAULT, mContext.getPackageName(), mContext.getUser());
        afterStatsForUid = storageStatsManager
                .queryStatsForUid(UUID_DEFAULT, Process.myUid());

        // Lease is expired, so it should not be attributed anymore.
        assertThat(afterStatsForPkg.getDataBytes() - beforeStatsForPkg.getDataBytes())
                .isEqualTo(0L);
        assertThat(afterStatsForUid.getDataBytes() - beforeStatsForUid.getDataBytes())
                .isEqualTo(0L);

        blobData.delete();
    }

    @Test
    public void testLeaseQuotaExceeded() throws Exception {
        final long totalBytes = Environment.getDataDirectory().getTotalSpace();
        final long limitBytes = 100 * Utils.MB_IN_BYTES;
        runWithKeyValues(() -> {
            final LongSparseArray<BlobHandle> blobs = new LongSparseArray<>();
            for (long blobSize : new long[] {20L, 30L, 40L}) {
                final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                        .setFileSize(blobSize * Utils.MB_IN_BYTES)
                        .build();
                blobData.prepare();

                commitBlob(blobData);
                acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                        blobData.getExpiryTimeMillis());
                blobs.put(blobSize, blobData.getBlobHandle());
            }
            final long blobSize = 40L;
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                    .setFileSize(blobSize * Utils.MB_IN_BYTES)
                    .build();
            blobData.prepare();
            commitBlob(blobData);
            assertThrows(LimitExceededException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), R.string.test_desc, blobData.getExpiryTimeMillis()));

            releaseLease(mContext, blobs.get(20L));
            assertThrows(LimitExceededException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), R.string.test_desc, blobData.getExpiryTimeMillis()));

            releaseLease(mContext, blobs.get(30L));
            acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                    blobData.getExpiryTimeMillis());
        }, Pair.create(KEY_TOTAL_BYTES_PER_APP_LIMIT_FLOOR, String.valueOf(limitBytes)),
                Pair.create(KEY_TOTAL_BYTES_PER_APP_LIMIT_FRACTION,
                        String.valueOf((double) limitBytes / totalBytes)));
    }

    @Test
    public void testLeaseQuotaExceeded_singleFileExceedsLimit() throws Exception {
        final long totalBytes = Environment.getDataDirectory().getTotalSpace();
        final long limitBytes = 50 * Utils.MB_IN_BYTES;
        runWithKeyValues(() -> {
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                    .setFileSize(limitBytes + (5 * Utils.MB_IN_BYTES))
                    .build();
            blobData.prepare();
            commitBlob(blobData);
            assertThrows(LimitExceededException.class, () -> mBlobStoreManager.acquireLease(
                    blobData.getBlobHandle(), R.string.test_desc, blobData.getExpiryTimeMillis()));
        }, Pair.create(KEY_TOTAL_BYTES_PER_APP_LIMIT_FLOOR, String.valueOf(limitBytes)),
                Pair.create(KEY_TOTAL_BYTES_PER_APP_LIMIT_FRACTION,
                        String.valueOf((double) limitBytes / totalBytes)));
    }

    @Test
    public void testLeaseQuotaExceeded_withExpiredLease() throws Exception {
        final long totalBytes = Environment.getDataDirectory().getTotalSpace();
        final long limitBytes = 50 * Utils.MB_IN_BYTES;
        final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);
        runWithKeyValues(() -> {
            final DummyBlobData blobData1 = new DummyBlobData.Builder(mContext)
                    .setFileSize(40 * Utils.MB_IN_BYTES)
                    .build();
            blobData1.prepare();
            final DummyBlobData blobData2 = new DummyBlobData.Builder(mContext)
                    .setFileSize(30 * Utils.MB_IN_BYTES)
                    .build();
            blobData2.prepare();

            commitBlob(blobData1);
            commitBlob(blobData2);
            acquireLease(mContext, blobData1.getBlobHandle(), R.string.test_desc,
                    System.currentTimeMillis() + waitDurationMs);

            assertThrows(LimitExceededException.class, () -> mBlobStoreManager.acquireLease(
                    blobData2.getBlobHandle(), R.string.test_desc));

            waitForLeaseExpiration(waitDurationMs, blobData1.getBlobHandle());
            acquireLease(mContext, blobData2.getBlobHandle(), R.string.test_desc);
        }, Pair.create(KEY_TOTAL_BYTES_PER_APP_LIMIT_FLOOR, String.valueOf(limitBytes)),
                Pair.create(KEY_TOTAL_BYTES_PER_APP_LIMIT_FRACTION,
                        String.valueOf((double) limitBytes / totalBytes)));
    }

    @Test
    public void testRemainingLeaseQuota() throws Exception {
        final long initialRemainingQuota = mBlobStoreManager.getRemainingLeaseQuotaBytes();
        final long blobSize = 100 * Utils.MB_IN_BYTES;
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setFileSize(blobSize)
                .build();
        blobData.prepare();

        commitBlob(blobData);
        acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                blobData.getExpiryTimeMillis());
        assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

        assertThat(mBlobStoreManager.getRemainingLeaseQuotaBytes())
                .isEqualTo(initialRemainingQuota - blobSize);

        releaseLease(mContext, blobData.getBlobHandle());
        assertNoLeasedBlobs(mBlobStoreManager);

        assertThat(mBlobStoreManager.getRemainingLeaseQuotaBytes())
                .isEqualTo(initialRemainingQuota);
    }

    @Test
    public void testRemainingLeaseQuota_withExpiredLease() throws Exception {
        final long initialRemainingQuota = mBlobStoreManager.getRemainingLeaseQuotaBytes();
        final long blobSize = 100 * Utils.MB_IN_BYTES;
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setFileSize(blobSize)
                .build();
        blobData.prepare();

        final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);
        commitBlob(blobData);
        acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                System.currentTimeMillis() + waitDurationMs);
        assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

        assertThat(mBlobStoreManager.getRemainingLeaseQuotaBytes())
                .isEqualTo(initialRemainingQuota - blobSize);

        waitForLeaseExpiration(waitDurationMs, blobData.getBlobHandle());
        assertNoLeasedBlobs(mBlobStoreManager);

        assertThat(mBlobStoreManager.getRemainingLeaseQuotaBytes())
                .isEqualTo(initialRemainingQuota);
    }

    @Test
    public void testAccessExpiredBlob() throws Exception {
        final long expiryDurationMs = TimeUnit.SECONDS.toMillis(6);
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setExpiryDurationMs(expiryDurationMs)
                .build();
        blobData.prepare();

        final long startTimeMs = System.currentTimeMillis();
        final long blobId = commitBlob(blobData);
        assertThat(runShellCmd("cmd blob_store query-blob-existence -b " + blobId)).isEqualTo("1");
        final long commitDurationMs = System.currentTimeMillis() - startTimeMs;

        SystemClock.sleep(Math.abs(expiryDurationMs - commitDurationMs));

        assertThrows(SecurityException.class,
                () -> mBlobStoreManager.openBlob(blobData.getBlobHandle()));
        assertThrows(SecurityException.class,
                () -> mBlobStoreManager.acquireLease(blobData.getBlobHandle(),
                        R.string.test_desc));

        triggerIdleMaintenance();
        assertThat(runShellCmd("cmd blob_store query-blob-existence -b " + blobId)).isEqualTo("0");
    }

    @Test
    public void testAccessExpiredBlob_withLeaseAcquired() throws Exception {
        final long expiryDurationMs = TimeUnit.SECONDS.toMillis(6);
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext)
                .setExpiryDurationMs(expiryDurationMs)
                .build();
        blobData.prepare();

        final long startTimeMs = System.currentTimeMillis();
        final long blobId = commitBlob(blobData);
        assertThat(runShellCmd("cmd blob_store query-blob-existence -b " + blobId)).isEqualTo("1");
        final long commitDurationMs = System.currentTimeMillis() - startTimeMs;
        acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc);

        SystemClock.sleep(Math.abs(expiryDurationMs - commitDurationMs));

        assertThrows(SecurityException.class,
                () -> mBlobStoreManager.openBlob(blobData.getBlobHandle()));
        assertThrows(SecurityException.class,
                () -> mBlobStoreManager.acquireLease(blobData.getBlobHandle(),
                        R.string.test_desc));

        triggerIdleMaintenance();
        assertThat(runShellCmd("cmd blob_store query-blob-existence -b " + blobId)).isEqualTo("0");
    }

    @Test
    public void testAccessBlob_withExpiredLease() throws Exception {
        final long leaseExpiryDurationMs = TimeUnit.SECONDS.toMillis(2);
        final long leaseAcquisitionWaitDurationMs = TimeUnit.SECONDS.toMillis(1);
        runWithKeyValues(() -> {
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
            blobData.prepare();
            try {
                final long blobId = commitBlob(blobData);
                assertThat(runShellCmd("cmd blob_store query-blob-existence -b " + blobId))
                        .isEqualTo("1");

                acquireLease(mContext, blobData.getBlobHandle(), R.string.test_desc,
                        System.currentTimeMillis() + leaseExpiryDurationMs);
                assertLeasedBlobs(mBlobStoreManager, blobData.getBlobHandle());

                waitForLeaseExpiration(leaseExpiryDurationMs, blobData.getBlobHandle());
                assertNoLeasedBlobs(mBlobStoreManager);

                triggerIdleMaintenance();
                assertThat(runShellCmd("cmd blob_store query-blob-existence -b " + blobId))
                        .isEqualTo("0");

                assertThrows(SecurityException.class, () -> mBlobStoreManager.acquireLease(
                        blobData.getBlobHandle(), R.string.test_desc,
                        blobData.getExpiryTimeMillis()));
                assertNoLeasedBlobs(mBlobStoreManager);
            } finally {
                blobData.delete();
            }
        }, Pair.create(KEY_LEASE_ACQUISITION_WAIT_DURATION_MS,
                String.valueOf(leaseAcquisitionWaitDurationMs)));
    }

    @Test
    public void testCommitBlobAfterIdleMaintenance() throws Exception {
        final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
        blobData.prepare();
        final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);
        final long partialFileSize = 100L;
        final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
        assertThat(sessionId).isGreaterThan(0L);

        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, 0, partialFileSize, blobData.getFileSize());
        }

        SystemClock.sleep(waitDurationMs);

        // Trigger idle maintenance which takes of deleting expired sessions.
        triggerIdleMaintenance();

        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session, partialFileSize,
                    blobData.getFileSize() - partialFileSize, blobData.getFileSize());
            commitSession(sessionId, session, blobData.getBlobHandle());
        }
    }

    @Test
    public void testExpiredSessionsDeleted() throws Exception {
        final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);
        runWithKeyValues(() -> {
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
            blobData.prepare();

            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            SystemClock.sleep(waitDurationMs);

            // Trigger idle maintenance which takes of deleting expired sessions.
            triggerIdleMaintenance();

            assertThrows(SecurityException.class, () -> mBlobStoreManager.openSession(sessionId));
        }, Pair.create(KEY_SESSION_EXPIRY_TIMEOUT_MS, String.valueOf(waitDurationMs)));
    }

    @Test
    public void testExpiredSessionsDeleted_withPartialData() throws Exception {
        final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);
        runWithKeyValues(() -> {
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
            blobData.prepare();

            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);

            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session, 0, 100, blobData.getFileSize());
            }

            SystemClock.sleep(waitDurationMs);

            // Trigger idle maintenance which takes of deleting expired sessions.
            triggerIdleMaintenance();

            assertThrows(SecurityException.class, () -> mBlobStoreManager.openSession(sessionId));
        }, Pair.create(KEY_SESSION_EXPIRY_TIMEOUT_MS, String.valueOf(waitDurationMs)));
    }

    @Test
    public void testCreateSession_countLimitExceeded() throws Exception {
        runWithKeyValues(() -> {
            final DummyBlobData blobData1 = new DummyBlobData.Builder(mContext).build();
            blobData1.prepare();
            final DummyBlobData blobData2 = new DummyBlobData.Builder(mContext).build();
            blobData2.prepare();

            mBlobStoreManager.createSession(blobData1.getBlobHandle());
            assertThrows(LimitExceededException.class,
                    () -> mBlobStoreManager.createSession(blobData2.getBlobHandle()));
        }, Pair.create(KEY_MAX_ACTIVE_SESSIONS, String.valueOf(1)));
    }

    @Test
    public void testCommitSession_countLimitExceeded() throws Exception {
        runWithKeyValues(() -> {
            final DummyBlobData blobData1 = new DummyBlobData.Builder(mContext).build();
            blobData1.prepare();
            final DummyBlobData blobData2 = new DummyBlobData.Builder(mContext).build();
            blobData2.prepare();

            commitBlob(blobData1, null /* accessModifier */, true /* expectSuccess */);
            commitBlob(blobData2, null /* accessModifier */, false /* expectSuccess */);
        }, Pair.create(KEY_MAX_COMMITTED_BLOBS, String.valueOf(1)));
    }

    @Test
    public void testLeaseBlob_countLimitExceeded() throws Exception {
        runWithKeyValues(() -> {
            final DummyBlobData blobData1 = new DummyBlobData.Builder(mContext).build();
            blobData1.prepare();
            final DummyBlobData blobData2 = new DummyBlobData.Builder(mContext).build();
            blobData2.prepare();

            commitBlob(blobData1);
            commitBlob(blobData2);

            acquireLease(mContext, blobData1.getBlobHandle(), "test desc");
            assertThrows(LimitExceededException.class,
                    () -> acquireLease(mContext, blobData2.getBlobHandle(), "test desc"));
        }, Pair.create(KEY_MAX_LEASED_BLOBS, String.valueOf(1)));
    }

    @Test
    public void testExpiredLease_countLimitExceeded() throws Exception {
        runWithKeyValues(() -> {
            final DummyBlobData blobData1 = new DummyBlobData.Builder(mContext).build();
            blobData1.prepare();
            final DummyBlobData blobData2 = new DummyBlobData.Builder(mContext).build();
            blobData2.prepare();
            final DummyBlobData blobData3 = new DummyBlobData.Builder(mContext).build();
            blobData3.prepare();
            final long waitDurationMs = TimeUnit.SECONDS.toMillis(2);

            commitBlob(blobData1);
            commitBlob(blobData2);
            commitBlob(blobData3);

            acquireLease(mContext, blobData1.getBlobHandle(), "test desc1",
                    System.currentTimeMillis() + waitDurationMs);
            assertThrows(LimitExceededException.class,
                    () -> acquireLease(mContext, blobData2.getBlobHandle(), "test desc2",
                            System.currentTimeMillis() + TimeUnit.HOURS.toMillis(4)));

            waitForLeaseExpiration(waitDurationMs, blobData1.getBlobHandle());

            acquireLease(mContext, blobData2.getBlobHandle(), "test desc2",
                    System.currentTimeMillis() + TimeUnit.HOURS.toMillis(4));
            assertThrows(LimitExceededException.class,
                    () -> acquireLease(mContext, blobData3.getBlobHandle(), "test desc3"));
        }, Pair.create(KEY_MAX_LEASED_BLOBS, String.valueOf(1)));
    }

    @Test
    public void testAllowPackageAccess_countLimitExceeded() throws Exception {
        runWithKeyValues(() -> {
            final DummyBlobData blobData = new DummyBlobData.Builder(mContext).build();
            blobData.prepare();

            final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
            assertThat(sessionId).isGreaterThan(0L);
            try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
                blobData.writeToSession(session);

                session.allowPackageAccess(HELPER_PKG2, HELPER_PKG2_CERT_SHA256);
                assertThrows(LimitExceededException.class,
                        () -> session.allowPackageAccess(HELPER_PKG3, HELPER_PKG3_CERT_SHA256));
            }
        }, Pair.create(KEY_MAX_BLOB_ACCESS_PERMITTED_PACKAGES, String.valueOf(1)));
    }

    @Test
    public void testBlobHandleEquality() throws Exception {
        // Check that BlobHandle objects are considered equal when digest, label, expiry time
        // and tag are equal.
        {
            final BlobHandle blobHandle1 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob", 1111L, "tag");
            final BlobHandle blobHandle2 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob", 1111L, "tag");
            assertThat(blobHandle1).isEqualTo(blobHandle2);
        }

        // Check that BlobHandle objects are not equal if digests are not equal.
        {
            final BlobHandle blobHandle1 = BlobHandle.createWithSha256("digest1".getBytes(),
                    "Dummy blob", 1111L, "tag");
            final BlobHandle blobHandle2 = BlobHandle.createWithSha256("digest2".getBytes(),
                    "Dummy blob", 1111L, "tag");
            assertThat(blobHandle1).isNotEqualTo(blobHandle2);
        }

        // Check that BlobHandle objects are not equal if expiry times are not equal.
        {
            final BlobHandle blobHandle1 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob", 1111L, "tag");
            final BlobHandle blobHandle2 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob", 1112L, "tag");
            assertThat(blobHandle1).isNotEqualTo(blobHandle2);
        }

        // Check that BlobHandle objects are not equal if labels are not equal.
        {
            final BlobHandle blobHandle1 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob1", 1111L, "tag");
            final BlobHandle blobHandle2 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob2", 1111L, "tag");
            assertThat(blobHandle1).isNotEqualTo(blobHandle2);
        }

        // Check that BlobHandle objects are not equal if tags are not equal.
        {
            final BlobHandle blobHandle1 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob", 1111L, "tag1");
            final BlobHandle blobHandle2 = BlobHandle.createWithSha256("digest".getBytes(),
                    "Dummy blob", 1111L, "tag2");
            assertThat(blobHandle1).isNotEqualTo(blobHandle2);
        }
    }

    @Test
    public void testBlobHandleCreation() throws Exception {
        // Creating a BlobHandle with label > 100 chars will fail
        {
            final CharSequence label = String.join("", Collections.nCopies(101, "a"));
            assertThrows(IllegalArgumentException.class,
                    () -> BlobHandle.createWithSha256("digest".getBytes(), label, 1111L, "tag"));
        }

        // Creating a BlobHandle with tag > 128 chars will fail
        {
            final String tag = String.join("", Collections.nCopies(129, "a"));
            assertThrows(IllegalArgumentException.class,
                    () -> BlobHandle.createWithSha256("digest".getBytes(), "label", 1111L, tag));
        }
    }

    private static void runWithKeyValues(ThrowingRunnable runnable,
            Pair<String, String>... keyValues) throws Exception {
        final Map<String, String> previousValues = new ArrayMap();
        SystemUtil.runWithShellPermissionIdentity(() -> {
            for (Pair<String, String> pair : keyValues) {
                final String key = pair.first;
                final String value = pair.second;
                final String previousValue = DeviceConfig.getProperty(NAMESPACE_BLOBSTORE, key);
                if (!Objects.equals(previousValue, value)) {
                    previousValues.put(key, previousValue);
                    Log.i(TAG, key + " previous value: " + previousValue);
                    assertThat(DeviceConfig.setProperty(NAMESPACE_BLOBSTORE, key, value,
                            false /* makeDefault */)).isTrue();
                }
                Log.i(TAG, key + " value set: " + value);
            }
        });
        try {
            runnable.run();
        } finally {
            SystemUtil.runWithShellPermissionIdentity(() -> {
                previousValues.forEach((key, previousValue) -> {
                    final String currentValue = DeviceConfig.getProperty(
                            NAMESPACE_BLOBSTORE, key);
                    if (!Objects.equals(previousValue, currentValue)) {
                        assertThat(DeviceConfig.setProperty(NAMESPACE_BLOBSTORE,
                                key, previousValue, false /* makeDefault */)).isTrue();
                        Log.i(TAG, key + " value restored: " + previousValue);
                    }
                });
            });
        }
    }

    private void commitAndVerifyBlob(DummyBlobData blobData) throws Exception {
        commitBlob(blobData);

        // Verify that blob can be accessed after committing.
        try (ParcelFileDescriptor pfd = mBlobStoreManager.openBlob(blobData.getBlobHandle())) {
            assertThat(pfd).isNotNull();
            blobData.verifyBlob(pfd);
        }
    }

    private long commitBlob(DummyBlobData blobData) throws Exception {
        return commitBlob(blobData, null);
    }

    private long commitBlob(DummyBlobData blobData,
            AccessModifier accessModifier) throws Exception {
        return commitBlob(blobData, accessModifier, true /* expectSuccess */);
    }

    private long commitBlob(DummyBlobData blobData,
            AccessModifier accessModifier, boolean expectSuccess) throws Exception {
        final long sessionId = mBlobStoreManager.createSession(blobData.getBlobHandle());
        assertThat(sessionId).isGreaterThan(0L);
        try (BlobStoreManager.Session session = mBlobStoreManager.openSession(sessionId)) {
            blobData.writeToSession(session);

            if (accessModifier != null) {
                accessModifier.modify(session);
            }
            commitSession(sessionId, session, blobData.getBlobHandle(), expectSuccess);
        }
        return sessionId;
    }

    private void commitSession(long sessionId, BlobStoreManager.Session session,
            BlobHandle blobHandle) throws Exception {
        commitSession(sessionId, session, blobHandle, true /* expectSuccess */);
    }

    private void commitSession(long sessionId, BlobStoreManager.Session session,
            BlobHandle blobHandle, boolean expectSuccess) throws Exception {
        Log.d(TAG, "Committing session: " + sessionId + "; blob: " + blobHandle);
        final CompletableFuture<Integer> callback = new CompletableFuture<>();
        session.commit(mContext.getMainExecutor(), callback::complete);
        final int result = callback.get(TIMEOUT_COMMIT_CALLBACK_SEC, TimeUnit.SECONDS);
        if (expectSuccess) {
            assertThat(result).isEqualTo(0);
        } else {
            assertThat(result).isNotEqualTo(0);
        }
    }

    private interface AccessModifier {
        void modify(BlobStoreManager.Session session) throws Exception;
    }

    private void commitBlobFromPkg(DummyBlobData blobData, TestServiceConnection serviceConnection)
            throws Exception {
        commitBlobFromPkg(blobData, ICommandReceiver.FLAG_ACCESS_TYPE_PRIVATE, serviceConnection);
    }

    private void commitBlobFromPkg(DummyBlobData blobData, int accessTypeFlags,
            TestServiceConnection serviceConnection) throws Exception {
        final ICommandReceiver commandReceiver = serviceConnection.getCommandReceiver();
        try (ParcelFileDescriptor pfd = blobData.openForRead()) {
            assertThat(commandReceiver.commit(blobData.getBlobHandle(),
                    pfd, accessTypeFlags, TIMEOUT_COMMIT_CALLBACK_SEC, blobData.getFileSize()))
                            .isEqualTo(0);
        }
    }

    private void openSessionFromPkg(long sessionId, String pkg) throws Exception {
        final TestServiceConnection serviceConnection = bindToHelperService(pkg);
        try {
            final ICommandReceiver commandReceiver = serviceConnection.getCommandReceiver();
            commandReceiver.openSession(sessionId);
        } finally {
            serviceConnection.unbind();
        }
    }

    private void assertPkgCanAccess(DummyBlobData blobData, String pkg) throws Exception {
        final TestServiceConnection serviceConnection = bindToHelperService(pkg);
        try {
            assertPkgCanAccess(blobData, serviceConnection);
        } finally {
            serviceConnection.unbind();
        }
    }

    private void assertPkgCanAccess(DummyBlobData blobData,
            TestServiceConnection serviceConnection) throws Exception {
        final ICommandReceiver commandReceiver = serviceConnection.getCommandReceiver();
        commandReceiver.acquireLease(blobData.getBlobHandle());
        try (ParcelFileDescriptor pfd = commandReceiver.openBlob(blobData.getBlobHandle())) {
            assertThat(pfd).isNotNull();
            blobData.verifyBlob(pfd);
        }
    }

    private void assertPkgCannotAccess(DummyBlobData blobData, String pkg) throws Exception {
        final TestServiceConnection serviceConnection = bindToHelperService(pkg);
        try {
            assertPkgCannotAccess(blobData, serviceConnection);
        } finally {
            serviceConnection.unbind();
        }
    }

    private void assertPkgCannotAccess(DummyBlobData blobData,
        TestServiceConnection serviceConnection) throws Exception {
        final ICommandReceiver commandReceiver = serviceConnection.getCommandReceiver();
        assertThrows(SecurityException.class,
                () -> commandReceiver.acquireLease(blobData.getBlobHandle()));
        assertThrows(SecurityException.class,
                () -> commandReceiver.openBlob(blobData.getBlobHandle()));
    }

    private void waitForLeaseExpiration(long waitDurationMs, BlobHandle leasedBlob)
            throws Exception {
        SystemClock.sleep(waitDurationMs);
        assertThat(mBlobStoreManager.getLeaseInfo(leasedBlob)).isNull();
    }

    private TestServiceConnection bindToHelperService(String pkg) throws Exception {
        final TestServiceConnection serviceConnection = new TestServiceConnection(mContext);
        final Intent intent = new Intent()
                .setComponent(new ComponentName(pkg, HELPER_SERVICE));
        mContext.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
        return serviceConnection;
    }

    private class TestServiceConnection implements ServiceConnection {
        private final Context mContext;
        private final BlockingQueue<IBinder> mBlockingQueue = new LinkedBlockingQueue<>();
        private ICommandReceiver mCommandReceiver;

        TestServiceConnection(Context context) {
            mContext = context;
        }

        public void onServiceConnected(ComponentName componentName, IBinder service) {
            Log.i(TAG, "Service got connected: " + componentName);
            mBlockingQueue.offer(service);
        }

        public void onServiceDisconnected(ComponentName componentName) {
            Log.e(TAG, "Service got disconnected: " + componentName);
        }

        private IBinder getService() throws Exception {
            final IBinder service = mBlockingQueue.poll(TIMEOUT_BIND_SERVICE_SEC,
                    TimeUnit.SECONDS);
            return service;
        }

        public ICommandReceiver getCommandReceiver() throws Exception {
            if (mCommandReceiver == null) {
                mCommandReceiver = ICommandReceiver.Stub.asInterface(getService());
            }
            return mCommandReceiver;
        }

        public void unbind() {
            mCommandReceiver = null;
            mContext.unbindService(this);
        }
    }
}
