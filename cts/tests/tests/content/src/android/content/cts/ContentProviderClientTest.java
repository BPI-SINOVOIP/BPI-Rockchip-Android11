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

package android.content.cts;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.IContentProvider;
import android.content.OperationApplicationException;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ICancellationSignal;
import android.os.OperationCanceledException;
import android.os.RemoteException;
import android.test.AndroidTestCase;
import android.test.mock.MockContentResolver;
import android.test.mock.MockIContentProvider;

import org.mockito.stubbing.Answer;

import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Simple delegation test for {@link ContentProviderClient}, checking the right methods are called.
 * Actual {@link ContentProvider} functionality is tested in {@link ContentProviderTest}.
 */
public class ContentProviderClientTest extends AndroidTestCase {

    private static final Answer ANSWER_SLEEP = invocation -> {
        // Sleep long enough to trigger ANR
        Thread.sleep(100);
        return null;
    };

    private static final String PACKAGE_NAME = "android.content.cts";
    private static final String FEATURE_ID = "testFeature";
    private static final String MODE = "mode";
    private static final String AUTHORITY = "authority";
    private static final String METHOD = "method";
    private static final String ARG = "arg";
    private static final Uri URI = Uri.parse("com.example.app://path");
    private static final Bundle ARGS = new Bundle();
    private static final Bundle EXTRAS = new Bundle();
    private static final ContentValues VALUES = new ContentValues();
    private static final ContentValues[] VALUES_ARRAY = {VALUES};
    private static final ArrayList<ContentProviderOperation> OPS = new ArrayList<>();

    private ContentResolver mContentResolver;
    private IContentProvider mIContentProvider;
    private ContentProviderClient mContentProviderClient;

    private CancellationSignal mCancellationSignal = new CancellationSignal();
    private ICancellationSignal mICancellationSignal;
    private boolean mCalledCancel = false;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mIContentProvider = mock(MockIContentProvider.class);
        mICancellationSignal = mock(ICancellationSignal.class);

        when(mIContentProvider.createCancellationSignal()).thenReturn(mICancellationSignal);

        mContentResolver = spy(
                new MockContentResolver(getContext().createFeatureContext(FEATURE_ID)));
        mContentProviderClient = spy(new ContentProviderClient(mContentResolver, mIContentProvider,
                false));

        mCalledCancel = false;
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (!mCalledCancel) {
            // Client should never cancel unless the test called cancel
            assertFalse(mCancellationSignal.isCanceled());
            verify(mICancellationSignal, never()).cancel();
        }
    }

    public void testQuery() throws RemoteException {
        mContentProviderClient.query(URI, null, ARGS, mCancellationSignal);
        verify(mIContentProvider).query(PACKAGE_NAME, FEATURE_ID, URI, null, ARGS,
                mICancellationSignal);
    }

    public void testQueryTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.query(PACKAGE_NAME, FEATURE_ID, URI, null, ARGS,
                mICancellationSignal)).thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.query(URI, null, ARGS, mCancellationSignal));

        verify(mIContentProvider).query(PACKAGE_NAME, FEATURE_ID, URI, null, ARGS,
                mICancellationSignal);
    }

    public void testQueryAlreadyCancelled() throws Exception {
        testAlreadyCancelled(
                () -> mContentProviderClient.query(URI, null, ARGS, mCancellationSignal));
        verify(mIContentProvider, never()).query(PACKAGE_NAME, FEATURE_ID, URI, null, ARGS,
                mICancellationSignal);
    }

    public void testGetType() throws RemoteException {
        mContentProviderClient.getType(URI);
        verify(mIContentProvider).getType(URI);
    }

    public void testGetTypeTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.getType(URI))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.getType(URI));

        verify(mIContentProvider).getType(URI);
    }

    public void testGetStreamTypes() throws RemoteException {
        mContentProviderClient.getStreamTypes(URI, "");
        verify(mIContentProvider).getStreamTypes(URI, "");
    }

    public void testGetStreamTypesTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.getStreamTypes(URI, ""))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.getStreamTypes(URI, ""));

        verify(mIContentProvider).getStreamTypes(URI, "");
    }

    public void testCanonicalize() throws RemoteException {
        mContentProviderClient.canonicalize(URI);
        verify(mIContentProvider).canonicalize(PACKAGE_NAME, FEATURE_ID, URI);
    }

    public void testCanonicalizeTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.canonicalize(PACKAGE_NAME, FEATURE_ID, URI))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.canonicalize(URI));

        verify(mIContentProvider).canonicalize(PACKAGE_NAME, FEATURE_ID, URI);
    }

    public void testUncanonicalize() throws RemoteException {
        mContentProviderClient.uncanonicalize(URI);
        verify(mIContentProvider).uncanonicalize(PACKAGE_NAME, FEATURE_ID, URI);
    }

    public void testUncanonicalizeTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.uncanonicalize(PACKAGE_NAME, FEATURE_ID, URI))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.uncanonicalize(URI));

        verify(mIContentProvider).uncanonicalize(PACKAGE_NAME, FEATURE_ID, URI);
    }

    public void testRefresh() throws RemoteException {
        mContentProviderClient.refresh(URI, ARGS, mCancellationSignal);
        verify(mIContentProvider).refresh(PACKAGE_NAME, FEATURE_ID, URI, ARGS,
                mICancellationSignal);
    }

    public void testRefreshTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.refresh(PACKAGE_NAME, FEATURE_ID, URI, ARGS, mICancellationSignal))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.refresh(URI, ARGS, mCancellationSignal));

        verify(mIContentProvider).refresh(PACKAGE_NAME, FEATURE_ID, URI, ARGS,
                mICancellationSignal);
    }

    public void testRefreshAlreadyCancelled() throws Exception {
        testAlreadyCancelled(() -> mContentProviderClient.refresh(URI, ARGS, mCancellationSignal));
        verify(mIContentProvider, never()).refresh(PACKAGE_NAME, FEATURE_ID, URI, ARGS,
                mICancellationSignal);
    }

    public void testInsert() throws RemoteException {
        mContentProviderClient.insert(URI, VALUES, EXTRAS);
        verify(mIContentProvider).insert(PACKAGE_NAME, FEATURE_ID, URI, VALUES, EXTRAS);
    }

    public void testInsertTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.insert(PACKAGE_NAME, FEATURE_ID, URI, VALUES, EXTRAS))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.insert(URI, VALUES, EXTRAS));

        verify(mIContentProvider).insert(PACKAGE_NAME, FEATURE_ID, URI, VALUES, EXTRAS);
    }

    public void testBulkInsert() throws RemoteException {
        mContentProviderClient.bulkInsert(URI, VALUES_ARRAY);
        verify(mIContentProvider).bulkInsert(PACKAGE_NAME, FEATURE_ID, URI, VALUES_ARRAY);
    }

    public void testBulkInsertTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.bulkInsert(PACKAGE_NAME, FEATURE_ID, URI, VALUES_ARRAY))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.bulkInsert(URI, VALUES_ARRAY));

        verify(mIContentProvider).bulkInsert(PACKAGE_NAME, FEATURE_ID, URI, VALUES_ARRAY);
    }

    public void testDelete() throws RemoteException {
        mContentProviderClient.delete(URI, EXTRAS);
        verify(mIContentProvider).delete(PACKAGE_NAME, FEATURE_ID, URI, EXTRAS);
    }

    public void testDeleteTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.delete(PACKAGE_NAME, FEATURE_ID, URI, EXTRAS))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.delete(URI, EXTRAS));

        verify(mIContentProvider).delete(PACKAGE_NAME, FEATURE_ID, URI, EXTRAS);
    }

    public void testUpdate() throws RemoteException {
        mContentProviderClient.update(URI, VALUES, EXTRAS);
        verify(mIContentProvider).update(PACKAGE_NAME, FEATURE_ID, URI, VALUES, EXTRAS);
    }

    public void testUpdateTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.update(PACKAGE_NAME, FEATURE_ID, URI, VALUES, EXTRAS))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.update(URI, VALUES, EXTRAS));

        verify(mIContentProvider).update(PACKAGE_NAME, FEATURE_ID, URI, VALUES, EXTRAS);
    }

    public void testOpenFile() throws RemoteException, FileNotFoundException {
        mContentProviderClient.openFile(URI, MODE, mCancellationSignal);

        verify(mIContentProvider).openFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal, null);
    }

    public void testOpenFileTimeout()
            throws RemoteException, InterruptedException, FileNotFoundException {
        when(mIContentProvider.openFile(PACKAGE_NAME, FEATURE_ID, URI, MODE, mICancellationSignal,
                null)).thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.openFile(URI, MODE, mCancellationSignal));

        verify(mIContentProvider).openFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal, null);
    }

    public void testOpenFileAlreadyCancelled() throws Exception {
        testAlreadyCancelled(() -> mContentProviderClient.openFile(URI, MODE, mCancellationSignal));

        verify(mIContentProvider, never()).openFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal, null);
    }

    public void testOpenAssetFile() throws RemoteException, FileNotFoundException {
        mContentProviderClient.openAssetFile(URI, MODE, mCancellationSignal);

        verify(mIContentProvider).openAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal);
    }

    public void testOpenAssetFileTimeout()
            throws RemoteException, InterruptedException, FileNotFoundException {
        when(mIContentProvider.openAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal)).thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.openAssetFile(URI, MODE, mCancellationSignal));

        verify(mIContentProvider).openAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal);
    }

    public void testOpenAssetFileAlreadyCancelled() throws Exception {
        testAlreadyCancelled(
                () -> mContentProviderClient.openAssetFile(URI, MODE, mCancellationSignal));

        verify(mIContentProvider, never()).openAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                mICancellationSignal);
    }

    public void testOpenTypedAssetFileDescriptor() throws RemoteException, FileNotFoundException {
        mContentProviderClient.openTypedAssetFileDescriptor(URI, MODE, ARGS, mCancellationSignal);

        verify(mIContentProvider).openTypedAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE, ARGS,
                mICancellationSignal);
    }

    public void testOpenTypedAssetFile() throws RemoteException, FileNotFoundException {
        mContentProviderClient.openTypedAssetFile(URI, MODE, ARGS, mCancellationSignal);

        verify(mIContentProvider).openTypedAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE, ARGS,
                mICancellationSignal);
    }

    public void testOpenTypedAssetFileTimeout()
            throws RemoteException, InterruptedException, FileNotFoundException {
        when(mIContentProvider.openTypedAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE, ARGS,
                mICancellationSignal))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.openTypedAssetFile(URI, MODE, ARGS,
                mCancellationSignal));

        verify(mIContentProvider).openTypedAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE, ARGS,
                mICancellationSignal);
    }

    public void testOpenTypedAssetFileAlreadyCancelled() throws Exception {
        testAlreadyCancelled(
                () -> mContentProviderClient.openTypedAssetFile(URI, MODE, ARGS,
                        mCancellationSignal));

        verify(mIContentProvider, never()).openTypedAssetFile(PACKAGE_NAME, FEATURE_ID, URI, MODE,
                ARGS, mICancellationSignal);
    }

    public void testApplyBatch() throws RemoteException, OperationApplicationException {
        mContentProviderClient.applyBatch(AUTHORITY, OPS);

        verify(mIContentProvider).applyBatch(PACKAGE_NAME, FEATURE_ID, AUTHORITY, OPS);
    }

    public void testApplyBatchTimeout()
            throws RemoteException, InterruptedException, OperationApplicationException {
        when(mIContentProvider.applyBatch(PACKAGE_NAME, FEATURE_ID, AUTHORITY, OPS))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.applyBatch(AUTHORITY, OPS));

        verify(mIContentProvider).applyBatch(PACKAGE_NAME, FEATURE_ID, AUTHORITY, OPS);
    }

    public void testCall() throws RemoteException {
        mContentProviderClient.call(AUTHORITY, METHOD, ARG, ARGS);

        verify(mIContentProvider).call(PACKAGE_NAME, FEATURE_ID, AUTHORITY, METHOD, ARG, ARGS);
    }

    public void testCallTimeout() throws RemoteException, InterruptedException {
        when(mIContentProvider.call(PACKAGE_NAME, FEATURE_ID, AUTHORITY, METHOD, ARG, ARGS))
                .thenAnswer(ANSWER_SLEEP);

        testTimeout(() -> mContentProviderClient.call(AUTHORITY, METHOD, ARG, ARGS));

        verify(mIContentProvider).call(PACKAGE_NAME, FEATURE_ID, AUTHORITY, METHOD, ARG, ARGS);
    }

    private void testTimeout(Function function) throws InterruptedException {
        mContentProviderClient.setDetectNotResponding(1);
        CountDownLatch latch = new CountDownLatch(1);
        doAnswer(invocation -> {
            latch.countDown();
            return null;
        })
                .when(mContentResolver)
                .appNotRespondingViaProvider(mIContentProvider);

        new Thread(() -> {
            try {
                function.run();
            } catch (Exception ignored) {
            } finally {
                latch.countDown();
            }
        }).start();

        latch.await(100, TimeUnit.MILLISECONDS);
        assertEquals(0, latch.getCount());

        verify(mContentResolver).appNotRespondingViaProvider(mIContentProvider);
    }

    private void testAlreadyCancelled(Function function) throws Exception {
        mCancellationSignal.cancel();
        mCalledCancel = true;

        try {
            function.run();
            fail("Expected OperationCanceledException");
        } catch (OperationCanceledException expected) {
        }
    }

    private interface Function {
        void run() throws Exception;
    }
}
