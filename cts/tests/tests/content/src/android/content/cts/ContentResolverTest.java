/*
 * Copyright (C) 2008 The Android Open Source Project
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

import static android.content.ContentResolver.NOTIFY_INSERT;
import static android.content.ContentResolver.NOTIFY_UPDATE;

import android.accounts.Account;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.ContentResolver.MimeTypeInfo;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.database.ContentObserver;
import android.database.Cursor;
import android.icu.text.Collator;
import android.icu.util.ULocale;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.OperationCanceledException;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.PollingCheck;
import com.android.internal.util.ArrayUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class ContentResolverTest extends AndroidTestCase {
    private static final String TAG = "ContentResolverTest";

    private final static String COLUMN_ID_NAME = "_id";
    private final static String COLUMN_KEY_NAME = "key";
    private final static String COLUMN_VALUE_NAME = "value";

    private static final String AUTHORITY = "ctstest";
    private static final Uri TABLE1_URI = Uri.parse("content://" + AUTHORITY + "/testtable1/");
    private static final Uri TABLE1_CROSS_URI =
            Uri.parse("content://" + AUTHORITY + "/testtable1/cross");
    private static final Uri TABLE2_URI = Uri.parse("content://" + AUTHORITY + "/testtable2/");

    private static final Uri LEVEL1_URI = Uri.parse("content://" + AUTHORITY + "/level/");
    private static final Uri LEVEL2_URI = Uri.parse("content://" + AUTHORITY + "/level/child");
    private static final Uri LEVEL3_URI = Uri.parse("content://" + AUTHORITY
            + "/level/child/grandchild/");

    private static final String REMOTE_AUTHORITY = "remotectstest";
    private static final Uri REMOTE_TABLE1_URI = Uri.parse("content://"
                + REMOTE_AUTHORITY + "/testtable1/");
    private static final Uri REMOTE_CRASH_URI = Uri.parse("content://"
            + REMOTE_AUTHORITY + "/crash/");
    private static final Uri REMOTE_HANG_URI = Uri.parse("content://"
            + REMOTE_AUTHORITY + "/hang/");

    private static final Account ACCOUNT = new Account("cts", "cts");

    private static final String KEY1 = "key1";
    private static final String KEY2 = "key2";
    private static final String KEY3 = "key3";
    private static final int VALUE1 = 1;
    private static final int VALUE2 = 2;
    private static final int VALUE3 = 3;

    private static final String TEST_PACKAGE_NAME = "android.content.cts";

    private Context mContext;
    private ContentResolver mContentResolver;
    private Cursor mCursor;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mContext = getContext();
        mContentResolver = mContext.getContentResolver();

        MockContentProvider.setCrashOnLaunch(mContext, false);

        // add three rows to database when every test case start.
        ContentValues values = new ContentValues();

        values.put(COLUMN_KEY_NAME, KEY1);
        values.put(COLUMN_VALUE_NAME, VALUE1);
        mContentResolver.insert(TABLE1_URI, values);
        mContentResolver.insert(REMOTE_TABLE1_URI, values);

        values.put(COLUMN_KEY_NAME, KEY2);
        values.put(COLUMN_VALUE_NAME, VALUE2);
        mContentResolver.insert(TABLE1_URI, values);
        mContentResolver.insert(REMOTE_TABLE1_URI, values);

        values.put(COLUMN_KEY_NAME, KEY3);
        values.put(COLUMN_VALUE_NAME, VALUE3);
        mContentResolver.insert(TABLE1_URI, values);
        mContentResolver.insert(REMOTE_TABLE1_URI, values);
    }

    @Override
    protected void tearDown() throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();

        mContentResolver.delete(TABLE1_URI, null, null);
        if ( null != mCursor && !mCursor.isClosed() ) {
            mCursor.close();
        }
        mContentResolver.delete(REMOTE_TABLE1_URI, null, null);
        if ( null != mCursor && !mCursor.isClosed() ) {
            mCursor.close();
        }
        super.tearDown();
    }

    public void testConstructor() {
        assertNotNull(mContentResolver);
    }

    public void testCrashOnLaunch() {
        // This test is going to make sure that the platform deals correctly
        // with a content provider process going away while a client is waiting
        // for it to come up.
        // First, we need to make sure our provider process is gone.  Goodbye!
        ContentProviderClient client = mContentResolver.acquireContentProviderClient(
                REMOTE_AUTHORITY);
        // We are going to do something wrong here...  release the client first,
        // so the act of killing it doesn't kill our own process.
        client.release();
        try {
            client.delete(REMOTE_CRASH_URI, null, null);
        } catch (RemoteException e) {
        }
        // Now make sure the thing is actually gone.
        boolean gone = true;
        try {
            client.getType(REMOTE_TABLE1_URI);
            gone = false;
        } catch (RemoteException e) {
        }
        if (!gone) {
            fail("Content provider process is not gone!");
        }
        try {
            MockContentProvider.setCrashOnLaunch(mContext, true);
            String type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
            assertFalse(MockContentProvider.getCrashOnLaunch(mContext));
            assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));
        } finally {
            MockContentProvider.setCrashOnLaunch(mContext, false);
        }
    }

    public void testUnstableToStableRefs() {
        // Get an unstable refrence on the remote content provider.
        ContentProviderClient uClient = mContentResolver.acquireUnstableContentProviderClient(
                REMOTE_AUTHORITY);
        // Verify we can access it.
        String type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        // Get a stable reference on the remote content provider.
        ContentProviderClient sClient = mContentResolver.acquireContentProviderClient(
                REMOTE_AUTHORITY);
        // Verify we can still access it.
        type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        // Release unstable reference.
        uClient.release();
        // Verify we can still access it.
        type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        // Release stable reference, removing last ref.
        sClient.release();
        // Kill it.  Note that a bug at this point where it causes our own
        // process to be killed will result in the entire test failing.
        try {
            Log.i("ContentResolverTest",
                    "Killing remote client -- if test process goes away, that is why!");
            uClient.delete(REMOTE_CRASH_URI, null, null);
        } catch (RemoteException e) {
        }
        // Make sure the remote client is actually gone.
        boolean gone = true;
        try {
            sClient.getType(REMOTE_TABLE1_URI);
            gone = false;
        } catch (RemoteException e) {
        }
        if (!gone) {
            fail("Content provider process is not gone!");
        }
    }

    public void testStableToUnstableRefs() {
        // Get a stable reference on the remote content provider.
        ContentProviderClient sClient = mContentResolver.acquireContentProviderClient(
                REMOTE_AUTHORITY);
        // Verify we can still access it.
        String type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        // Get an unstable refrence on the remote content provider.
        ContentProviderClient uClient = mContentResolver.acquireUnstableContentProviderClient(
                REMOTE_AUTHORITY);
        // Verify we can access it.
        type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        // Release stable reference, leaving only an unstable ref.
        sClient.release();

        // Kill it.  Note that a bug at this point where it causes our own
        // process to be killed will result in the entire test failing.
        try {
            Log.i("ContentResolverTest",
                    "Killing remote client -- if test process goes away, that is why!");
            uClient.delete(REMOTE_CRASH_URI, null, null);
        } catch (RemoteException e) {
        }
        // Make sure the remote client is actually gone.
        boolean gone = true;
        try {
            uClient.getType(REMOTE_TABLE1_URI);
            gone = false;
        } catch (RemoteException e) {
        }
        if (!gone) {
            fail("Content provider process is not gone!");
        }

        // Release unstable reference.
        uClient.release();
    }

    public void testGetType() {
        String type1 = mContentResolver.getType(TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        String type2 = mContentResolver.getType(TABLE2_URI);
        assertTrue(type2.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        Uri invalidUri = Uri.parse("abc");
        assertNull(mContentResolver.getType(invalidUri));

        try {
            mContentResolver.getType(null);
            fail("did not throw NullPointerException when Uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testUnstableGetType() {
        // Get an unstable refrence on the remote content provider.
        ContentProviderClient client = mContentResolver.acquireUnstableContentProviderClient(
                REMOTE_AUTHORITY);
        // Verify we can access it.
        String type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));

        // Kill it.  Note that a bug at this point where it causes our own
        // process to be killed will result in the entire test failing.
        try {
            Log.i("ContentResolverTest",
                    "Killing remote client -- if test process goes away, that is why!");
            client.delete(REMOTE_CRASH_URI, null, null);
        } catch (RemoteException e) {
        }
        // Make sure the remote client is actually gone.
        boolean gone = true;
        try {
            client.getType(REMOTE_TABLE1_URI);
            gone = false;
        } catch (RemoteException e) {
        }
        if (!gone) {
            fail("Content provider process is not gone!");
        }

        // Now the remote client is gone, can we recover?
        // Release our old reference.
        client.release();
        // Get a new reference.
        client = mContentResolver.acquireUnstableContentProviderClient(REMOTE_AUTHORITY);
        // Verify we can access it.
        type1 = mContentResolver.getType(REMOTE_TABLE1_URI);
        assertTrue(type1.startsWith(ContentResolver.CURSOR_DIR_BASE_TYPE, 0));
    }

    public void testQuery() {
        mCursor = mContentResolver.query(TABLE1_URI, null, null, null);

        assertNotNull(mCursor);
        assertEquals(3, mCursor.getCount());
        assertEquals(3, mCursor.getColumnCount());

        mCursor.moveToLast();
        assertEquals(3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY3, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToPrevious();
        assertEquals(2, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY2, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE2, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();
    }

    public void testQuery_WithSqlSelectionArgs() {
        Bundle queryArgs = new Bundle();
        queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION, COLUMN_ID_NAME + "=?");
        queryArgs.putStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS, new String[] {"1"});

        mCursor = mContentResolver.query(TABLE1_URI, null, queryArgs, null);
        assertNotNull(mCursor);
        assertEquals(1, mCursor.getCount());
        assertEquals(3, mCursor.getColumnCount());

        mCursor.moveToFirst();
        assertEquals(1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY1, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION, COLUMN_KEY_NAME + "=?");
        queryArgs.putStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS, new String[] {KEY3});
        mCursor = mContentResolver.query(TABLE1_URI, null, queryArgs, null);
        assertNotNull(mCursor);
        assertEquals(1, mCursor.getCount());
        assertEquals(3, mCursor.getColumnCount());

        mCursor.moveToFirst();
        assertEquals(3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY3, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();
    }

    /*
     * NOTE: this test is implicitly coupled to the implementation
     * of MockContentProvider#query, specifically the facts:
     *
     * - it does *not* override the query w/ Bundle methods
     * - it receives the auto-generated sql format arguments (supplied by the framework)
     * - it is backed by sqlite and forwards the sql formatted args.
     */
    public void testQuery_SqlSortingFromBundleArgs() {

        mContentResolver.delete(TABLE1_URI, null, null);
        ContentValues values = new ContentValues();

        values.put(COLUMN_KEY_NAME, "0");
        values.put(COLUMN_VALUE_NAME, "abc");
        mContentResolver.insert(TABLE1_URI, values);

        values.put(COLUMN_KEY_NAME, "1");
        values.put(COLUMN_VALUE_NAME, "DEF");
        mContentResolver.insert(TABLE1_URI, values);

        values.put(COLUMN_KEY_NAME, "2");
        values.put(COLUMN_VALUE_NAME, "ghi");
        mContentResolver.insert(TABLE1_URI, values);

        String[] sortCols = new String[] { COLUMN_VALUE_NAME };
        Bundle queryArgs = new Bundle();
        queryArgs.putStringArray(
                ContentResolver.QUERY_ARG_SORT_COLUMNS,
                sortCols);

        // Sort ascending...
        queryArgs.putInt(
                ContentResolver.QUERY_ARG_SORT_DIRECTION,
                ContentResolver.QUERY_SORT_DIRECTION_ASCENDING);

        mCursor = mContentResolver.query(TABLE1_URI, sortCols, queryArgs, null);
        int col = mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME);

        mCursor.moveToNext();
        assertEquals("DEF", mCursor.getString(col));
        mCursor.moveToNext();
        assertEquals("abc", mCursor.getString(col));
        mCursor.moveToNext();
        assertEquals("ghi", mCursor.getString(col));

        mCursor.close();

        // Nocase collation, descending...
        queryArgs.putInt(
                ContentResolver.QUERY_ARG_SORT_DIRECTION,
                ContentResolver.QUERY_SORT_DIRECTION_DESCENDING);
        queryArgs.putInt(
                ContentResolver.QUERY_ARG_SORT_COLLATION,
                java.text.Collator.SECONDARY);

        mCursor = mContentResolver.query(TABLE1_URI, null, queryArgs, null);
        col = mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME);

        mCursor.moveToNext();
        assertEquals("ghi", mCursor.getString(col));
        mCursor.moveToNext();
        assertEquals("DEF", mCursor.getString(col));
        mCursor.moveToNext();
        assertEquals("abc", mCursor.getString(col));

        mCursor.close();
    }

    public void testQuery_SqlSortingFromBundleArgs_Locale() {
        mContentResolver.delete(TABLE1_URI, null, null);

        final List<String> data = Arrays.asList(
                "ABC", "abc", "pinyin", "가나다", "바사", "테스트", "马",
                "嘛", "妈", "骂", "吗", "码", "玛", "麻", "中", "梵", "苹果", "久了", "伺候");

        for (String s : data) {
            final ContentValues values = new ContentValues();
            values.put(COLUMN_KEY_NAME, s.hashCode());
            values.put(COLUMN_VALUE_NAME, s);
            mContentResolver.insert(TABLE1_URI, values);
        }

        String[] sortCols = new String[] { COLUMN_VALUE_NAME };
        Bundle queryArgs = new Bundle();
        queryArgs.putStringArray(ContentResolver.QUERY_ARG_SORT_COLUMNS, sortCols);

        for (String locale : new String[] {
                "zh",
                "zh@collation=pinyin",
                "zh@collation=stroke",
                "zh@collation=zhuyin",
        }) {
            // Assert that sorting is identical between SQLite and ICU4J
            queryArgs.putString(ContentResolver.QUERY_ARG_SORT_LOCALE, locale);
            try (Cursor c = mContentResolver.query(TABLE1_URI, sortCols, queryArgs, null)) {
                data.sort(Collator.getInstance(new ULocale(locale)));
                assertEquals(data, collect(c));
            }
        }
    }

    private static List<String> collect(Cursor c) {
        List<String> res = new ArrayList<>();
        while (c.moveToNext()) {
            res.add(c.getString(0));
        }
        return res;
    }

    /**
     * Verifies that paging information is correctly relayed, and that
     * honored arguments from a supporting client are returned correctly.
     */
    public void testQuery_PagedResults() {

        Bundle queryArgs = new Bundle();
        queryArgs.putInt(ContentResolver.QUERY_ARG_OFFSET, 10);
        queryArgs.putInt(ContentResolver.QUERY_ARG_LIMIT, 3);
        queryArgs.putInt(TestPagingContentProvider.RECORD_COUNT, 100);

        mCursor = mContentResolver.query(
                TestPagingContentProvider.PAGED_DATA_URI, null, queryArgs, null);

        Bundle extras = mCursor.getExtras();
        extras = extras != null ? extras : Bundle.EMPTY;

        assertEquals(3, mCursor.getCount());
        assertTrue(extras.containsKey(ContentResolver.EXTRA_TOTAL_COUNT));
        assertEquals(100, extras.getInt(ContentResolver.EXTRA_TOTAL_COUNT));

        String[] honoredArgs = extras.getStringArray(ContentResolver.EXTRA_HONORED_ARGS);
        assertNotNull(honoredArgs);
        assertTrue(ArrayUtils.contains(honoredArgs, ContentResolver.QUERY_ARG_OFFSET));
        assertTrue(ArrayUtils.contains(honoredArgs, ContentResolver.QUERY_ARG_LIMIT));

        int col = mCursor.getColumnIndexOrThrow(TestPagingContentProvider.COLUMN_POS);

        mCursor.moveToNext();
        assertEquals(10, mCursor.getInt(col));
        mCursor.moveToNext();
        assertEquals(11, mCursor.getInt(col));
        mCursor.moveToNext();
        assertEquals(12, mCursor.getInt(col));

        assertFalse(mCursor.moveToNext());

        mCursor.close();
    }

    public void testQuery_NullUriThrows() {
        try {
            mContentResolver.query(null, null, null, null, null);
            fail("did not throw NullPointerException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testCrashingQuery() {
        try {
            MockContentProvider.setCrashOnLaunch(mContext, true);
            mCursor = mContentResolver.query(REMOTE_CRASH_URI, null, null, null, null);
            assertFalse(MockContentProvider.getCrashOnLaunch(mContext));
        } finally {
            MockContentProvider.setCrashOnLaunch(mContext, false);
        }

        assertNotNull(mCursor);
        assertEquals(3, mCursor.getCount());
        assertEquals(3, mCursor.getColumnCount());

        mCursor.moveToLast();
        assertEquals(3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY3, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToPrevious();
        assertEquals(2, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY2, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE2, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();
    }

    public void testCancelableQuery_WhenNotCanceled_ReturnsResultSet() {
        CancellationSignal cancellationSignal = new CancellationSignal();

        Cursor cursor = mContentResolver.query(TABLE1_URI, null, null, null, null,
                cancellationSignal);
        assertEquals(3, cursor.getCount());
        cursor.close();
    }

    public void testCancelableQuery_WhenCanceledBeforeQuery_ThrowsImmediately() {
        CancellationSignal cancellationSignal = new CancellationSignal();
        cancellationSignal.cancel();

        try {
            mContentResolver.query(TABLE1_URI, null, null, null, null, cancellationSignal);
            fail("Expected OperationCanceledException");
        } catch (OperationCanceledException ex) {
            // expected
        }
    }

    public void testCancelableQuery_WhenCanceledDuringLongRunningQuery_CancelsQueryAndThrows() {
        // Populate a table with a bunch of integers.
        mContentResolver.delete(TABLE1_URI, null, null);
        ContentValues values = new ContentValues();
        for (int i = 0; i < 100; i++) {
            values.put(COLUMN_KEY_NAME, i);
            values.put(COLUMN_VALUE_NAME, i);
            mContentResolver.insert(TABLE1_URI, values);
        }

        for (int i = 0; i < 5; i++) {
            final CancellationSignal cancellationSignal = new CancellationSignal();
            Thread cancellationThread = new Thread() {
                @Override
                public void run() {
                    try {
                        Thread.sleep(300);
                    } catch (InterruptedException ex) {
                    }
                    cancellationSignal.cancel();
                }
            };
            try {
                // Build an unsatisfiable 5-way cross-product query over 100 values but
                // produces no output.  This should force SQLite to loop for a long time
                // as it tests 10^10 combinations.
                cancellationThread.start();

                final long startTime = System.nanoTime();
                try {
                    mContentResolver.query(TABLE1_CROSS_URI, null,
                            "a.value + b.value + c.value + d.value + e.value > 1000000",
                            null, null, cancellationSignal);
                    fail("Expected OperationCanceledException");
                } catch (OperationCanceledException ex) {
                    // expected
                }

                // We want to confirm that the query really was running and then got
                // canceled midway.
                final long waitTime = System.nanoTime() - startTime;
                if (waitTime > 150 * 1000000L && waitTime < 600 * 1000000L) {
                    return; // success!
                }
            } finally {
                try {
                    cancellationThread.join();
                } catch (InterruptedException e) {
                }
            }
        }

        // Occasionally we might miss the timing deadline due to factors in the
        // environment, but if after several trials we still couldn't demonstrate
        // that the query was canceled, then the test must be broken.
        fail("Could not prove that the query actually canceled midway during execution.");
    }

    public void testOpenInputStream() throws IOException {
        final Uri uri = Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE +
                "://" + TEST_PACKAGE_NAME + "/" + R.drawable.pass);

        InputStream is = mContentResolver.openInputStream(uri);
        assertNotNull(is);
        is.close();

        final Uri invalidUri = Uri.parse("abc");
        try {
            mContentResolver.openInputStream(invalidUri);
            fail("did not throw FileNotFoundException when uri is invalid.");
        } catch (FileNotFoundException e) {
            //expected.
        }
    }

    public void testOpenOutputStream() throws IOException {
        Uri uri = Uri.parse(ContentResolver.SCHEME_FILE + "://" +
                getContext().getCacheDir().getAbsolutePath() +
                "/temp.jpg");
        OutputStream os = mContentResolver.openOutputStream(uri);
        assertNotNull(os);
        os.close();

        os = mContentResolver.openOutputStream(uri, "wa");
        assertNotNull(os);
        os.close();

        uri = Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE +
                "://" + TEST_PACKAGE_NAME + "/" + R.raw.testimage);
        try {
            mContentResolver.openOutputStream(uri);
            fail("did not throw FileNotFoundException when scheme is not accepted.");
        } catch (FileNotFoundException e) {
            //expected.
        }

        try {
            mContentResolver.openOutputStream(uri, "w");
            fail("did not throw FileNotFoundException when scheme is not accepted.");
        } catch (FileNotFoundException e) {
            //expected.
        }

        Uri invalidUri = Uri.parse("abc");
        try {
            mContentResolver.openOutputStream(invalidUri);
            fail("did not throw FileNotFoundException when uri is invalid.");
        } catch (FileNotFoundException e) {
            //expected.
        }

        try {
            mContentResolver.openOutputStream(invalidUri, "w");
            fail("did not throw FileNotFoundException when uri is invalid.");
        } catch (FileNotFoundException e) {
            //expected.
        }
    }

    public void testOpenAssetFileDescriptor() throws IOException {
        Uri uri = Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE +
                "://" + TEST_PACKAGE_NAME + "/" + R.raw.testimage);

        AssetFileDescriptor afd = mContentResolver.openAssetFileDescriptor(uri, "r");
        assertNotNull(afd);
        afd.close();

        try {
            mContentResolver.openAssetFileDescriptor(uri, "d");
            fail("did not throw FileNotFoundException when mode is unknown.");
        } catch (FileNotFoundException e) {
            //expected.
        }

        Uri invalidUri = Uri.parse("abc");
        try {
            mContentResolver.openAssetFileDescriptor(invalidUri, "r");
            fail("did not throw FileNotFoundException when uri is invalid.");
        } catch (FileNotFoundException e) {
            //expected.
        }
    }

    private String consumeAssetFileDescriptor(AssetFileDescriptor afd)
            throws IOException {
        FileInputStream stream = null;
        try {
            stream = afd.createInputStream();
            InputStreamReader reader = new InputStreamReader(stream, "UTF-8");

            // Got it...  copy the stream into a local string and return it.
            StringBuilder builder = new StringBuilder(128);
            char[] buffer = new char[8192];
            int len;
            while ((len=reader.read(buffer)) > 0) {
                builder.append(buffer, 0, len);
            }
            return builder.toString();

        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (IOException e) {
                }
            }
        }

    }

    public void testCrashingOpenAssetFileDescriptor() throws IOException {
        AssetFileDescriptor afd = null;
        try {
            MockContentProvider.setCrashOnLaunch(mContext, true);
            afd = mContentResolver.openAssetFileDescriptor(REMOTE_CRASH_URI, "rw");
            assertFalse(MockContentProvider.getCrashOnLaunch(mContext));
            assertNotNull(afd);
            String str = consumeAssetFileDescriptor(afd);
            afd = null;
            assertEquals(str, "This is the openAssetFile test data!");
        } finally {
            MockContentProvider.setCrashOnLaunch(mContext, false);
            if (afd != null) {
                afd.close();
            }
        }

        // Make sure a content provider crash at this point won't hurt us.
        ContentProviderClient uClient = mContentResolver.acquireUnstableContentProviderClient(
                REMOTE_AUTHORITY);
        // Kill it.  Note that a bug at this point where it causes our own
        // process to be killed will result in the entire test failing.
        try {
            Log.i("ContentResolverTest",
                    "Killing remote client -- if test process goes away, that is why!");
            uClient.delete(REMOTE_CRASH_URI, null, null);
        } catch (RemoteException e) {
        }
        uClient.release();
    }

    public void testCrashingOpenTypedAssetFileDescriptor() throws IOException {
        AssetFileDescriptor afd = null;
        try {
            MockContentProvider.setCrashOnLaunch(mContext, true);
            afd = mContentResolver.openTypedAssetFileDescriptor(
                    REMOTE_CRASH_URI, "text/plain", null);
            assertFalse(MockContentProvider.getCrashOnLaunch(mContext));
            assertNotNull(afd);
            String str = consumeAssetFileDescriptor(afd);
            afd = null;
            assertEquals(str, "This is the openTypedAssetFile test data!");
        } finally {
            MockContentProvider.setCrashOnLaunch(mContext, false);
            if (afd != null) {
                afd.close();
            }
        }

        // Make sure a content provider crash at this point won't hurt us.
        ContentProviderClient uClient = mContentResolver.acquireUnstableContentProviderClient(
                REMOTE_AUTHORITY);
        // Kill it.  Note that a bug at this point where it causes our own
        // process to be killed will result in the entire test failing.
        try {
            Log.i("ContentResolverTest",
                    "Killing remote client -- if test process goes away, that is why!");
            uClient.delete(REMOTE_CRASH_URI, null, null);
        } catch (RemoteException e) {
        }
        uClient.release();
    }

    public void testOpenFileDescriptor() throws IOException {
        Uri uri = Uri.parse(ContentResolver.SCHEME_FILE + "://" +
                getContext().getCacheDir().getAbsolutePath() +
                "/temp.jpg");
        ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(uri, "w");
        assertNotNull(pfd);
        pfd.close();

        try {
            mContentResolver.openFileDescriptor(uri, "d");
            fail("did not throw IllegalArgumentException when mode is unknown.");
        } catch (IllegalArgumentException e) {
            //expected.
        }

        Uri invalidUri = Uri.parse("abc");
        try {
            mContentResolver.openFileDescriptor(invalidUri, "w");
            fail("did not throw FileNotFoundException when uri is invalid.");
        } catch (FileNotFoundException e) {
            //expected.
        }

        uri = Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE +
                "://" + TEST_PACKAGE_NAME + "/" + R.raw.testimage);
        try {
            mContentResolver.openFileDescriptor(uri, "w");
            fail("did not throw FileNotFoundException when scheme is not accepted.");
        } catch (FileNotFoundException e) {
            //expected.
        }
    }

    public void testInsert() {
        String key4 = "key4";
        String key5 = "key5";
        int value4 = 4;
        int value5 = 5;
        String key4Selection = COLUMN_KEY_NAME + "=\"" + key4 + "\"";

        mCursor = mContentResolver.query(TABLE1_URI, null, key4Selection, null, null);
        assertEquals(0, mCursor.getCount());
        mCursor.close();

        ContentValues values = new ContentValues();
        values.put(COLUMN_KEY_NAME, key4);
        values.put(COLUMN_VALUE_NAME, value4);
        Uri uri = mContentResolver.insert(TABLE1_URI, values);
        assertNotNull(uri);

        mCursor = mContentResolver.query(TABLE1_URI, null, key4Selection, null, null);
        assertNotNull(mCursor);
        assertEquals(1, mCursor.getCount());

        mCursor.moveToFirst();
        assertEquals(4, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key4, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value4, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        values.put(COLUMN_KEY_NAME, key5);
        values.put(COLUMN_VALUE_NAME, value5);
        uri = mContentResolver.insert(TABLE1_URI, values);
        assertNotNull(uri);

        // check returned uri
        mCursor = mContentResolver.query(uri, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(1, mCursor.getCount());

        mCursor.moveToLast();
        assertEquals(5, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key5, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value5, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        try {
            mContentResolver.insert(null, values);
            fail("did not throw NullPointerException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testBulkInsert() {
        String key4 = "key4";
        String key5 = "key5";
        int value4 = 4;
        int value5 = 5;

        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(3, mCursor.getCount());
        mCursor.close();

        ContentValues[] cvs = new ContentValues[2];
        cvs[0] = new ContentValues();
        cvs[0].put(COLUMN_KEY_NAME, key4);
        cvs[0].put(COLUMN_VALUE_NAME, value4);

        cvs[1] = new ContentValues();
        cvs[1].put(COLUMN_KEY_NAME, key5);
        cvs[1].put(COLUMN_VALUE_NAME, value5);

        assertEquals(2, mContentResolver.bulkInsert(TABLE1_URI, cvs));
        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(5, mCursor.getCount());

        mCursor.moveToLast();
        assertEquals(5, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key5, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value5, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToPrevious();
        assertEquals(4, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key4, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value4, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        try {
            mContentResolver.bulkInsert(null, cvs);
            fail("did not throw NullPointerException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testDelete() {
        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(3, mCursor.getCount());
        mCursor.close();

        assertEquals(3, mContentResolver.delete(TABLE1_URI, null, null));
        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(0, mCursor.getCount());
        mCursor.close();

        // add three rows to database.
        ContentValues values = new ContentValues();
        values.put(COLUMN_KEY_NAME, KEY1);
        values.put(COLUMN_VALUE_NAME, VALUE1);
        mContentResolver.insert(TABLE1_URI, values);

        values.put(COLUMN_KEY_NAME, KEY2);
        values.put(COLUMN_VALUE_NAME, VALUE2);
        mContentResolver.insert(TABLE1_URI, values);

        values.put(COLUMN_KEY_NAME, KEY3);
        values.put(COLUMN_VALUE_NAME, VALUE3);
        mContentResolver.insert(TABLE1_URI, values);

        // test delete row using selection
        String selection = COLUMN_ID_NAME + "=2";
        assertEquals(1, mContentResolver.delete(TABLE1_URI, selection, null));

        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(2, mCursor.getCount());

        mCursor.moveToFirst();
        assertEquals(1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY1, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToNext();
        assertEquals(3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY3, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        selection = COLUMN_VALUE_NAME + "=3";
        assertEquals(1, mContentResolver.delete(TABLE1_URI, selection, null));

        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(1, mCursor.getCount());

        mCursor.moveToFirst();
        assertEquals(1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(KEY1, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(VALUE1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        selection = COLUMN_KEY_NAME + "=\"" + KEY1 + "\"";
        assertEquals(1, mContentResolver.delete(TABLE1_URI, selection, null));

        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(0, mCursor.getCount());
        mCursor.close();

        try {
            mContentResolver.delete(null, null, null);
            fail("did not throw NullPointerException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testUpdate() {
        ContentValues values = new ContentValues();
        String key10 = "key10";
        String key20 = "key20";
        int value10 = 10;
        int value20 = 20;

        values.put(COLUMN_KEY_NAME, key10);
        values.put(COLUMN_VALUE_NAME, value10);

        // test update all the rows.
        assertEquals(3, mContentResolver.update(TABLE1_URI, values, null, null));
        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(3, mCursor.getCount());

        mCursor.moveToFirst();
        assertEquals(1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key10, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value10, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToNext();
        assertEquals(2, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key10, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value10, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToLast();
        assertEquals(3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key10, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value10, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        // test update one row using selection.
        String selection = COLUMN_ID_NAME + "=1";
        values.put(COLUMN_KEY_NAME, key20);
        values.put(COLUMN_VALUE_NAME, value20);

        assertEquals(1, mContentResolver.update(TABLE1_URI, values, selection, null));
        mCursor = mContentResolver.query(TABLE1_URI, null, null, null, null);
        assertNotNull(mCursor);
        assertEquals(3, mCursor.getCount());

        mCursor.moveToFirst();
        assertEquals(1, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key20, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value20, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToNext();
        assertEquals(2, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key10, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value10, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));

        mCursor.moveToLast();
        assertEquals(3, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_ID_NAME)));
        assertEquals(key10, mCursor.getString(mCursor.getColumnIndexOrThrow(COLUMN_KEY_NAME)));
        assertEquals(value10, mCursor.getInt(mCursor.getColumnIndexOrThrow(COLUMN_VALUE_NAME)));
        mCursor.close();

        try {
            mContentResolver.update(null, values, null, null);
            fail("did not throw NullPointerException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }

        // javadoc says it will throw NullPointerException when values are null,
        // but actually, it throws IllegalArgumentException here.
        try {
            mContentResolver.update(TABLE1_URI, null, null, null);
            fail("did not throw IllegalArgumentException when values are null.");
        } catch (IllegalArgumentException e) {
            //expected.
        }
    }

    public void testRefresh_DefaultImplReturnsFalse() {
        boolean refreshed = mContentResolver.refresh(TABLE1_URI, null, null);
        assertFalse(refreshed);
        MockContentProvider.assertRefreshed(TABLE1_URI);
    }

    public void testRefresh_ReturnsProviderValue() {
        try {
            MockContentProvider.setRefreshReturnValue(true);
            boolean refreshed = mContentResolver.refresh(TABLE1_URI, null, null);
            assertTrue(refreshed);
            MockContentProvider.assertRefreshed(TABLE1_URI);
        } finally {
            MockContentProvider.setRefreshReturnValue(false);
        }
    }

    public void testRefresh_NullUriThrowsImmediately() {
        try {
            mContentResolver.refresh(null, null, null);
            fail("did not throw NullPointerException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testRefresh_CancellableThrowsImmediately() {
        CancellationSignal cancellationSignal = new CancellationSignal();
        cancellationSignal.cancel();

        try {
            mContentResolver.refresh(TABLE1_URI, null, cancellationSignal);
            fail("Expected OperationCanceledException");
        } catch (OperationCanceledException ex) {
            // expected
        }
    }

    public void testCheckUriPermission() {
        assertEquals(PackageManager.PERMISSION_GRANTED, mContentResolver.checkUriPermission(
                TABLE1_URI, android.os.Process.myUid(), Intent.FLAG_GRANT_READ_URI_PERMISSION));
        assertEquals(PackageManager.PERMISSION_DENIED, mContentResolver.checkUriPermission(
                TABLE1_URI, android.os.Process.myUid(), Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
    }

    public void testRegisterContentObserver() {
        final MockContentObserver mco = new MockContentObserver();

        mContentResolver.registerContentObserver(TABLE1_URI, true, mco);
        assertFalse(mco.hadOnChanged());

        ContentValues values = new ContentValues();
        values.put(COLUMN_KEY_NAME, "key10");
        values.put(COLUMN_VALUE_NAME, 10);
        mContentResolver.update(TABLE1_URI, values, null, null);
        new PollingCheck() {
            @Override
            protected boolean check() {
                return mco.hadOnChanged();
            }
        }.run();

        mco.reset();
        mContentResolver.unregisterContentObserver(mco);
        assertFalse(mco.hadOnChanged());
        mContentResolver.update(TABLE1_URI, values, null, null);

        assertFalse(mco.hadOnChanged());

        try {
            mContentResolver.registerContentObserver(null, false, mco);
            fail("did not throw NullPointerException or IllegalArgumentException when uri is null.");
        } catch (NullPointerException e) {
            //expected.
        } catch (IllegalArgumentException e) {
            // also expected
        }

        try {
            mContentResolver.registerContentObserver(TABLE1_URI, false, null);
            fail("did not throw NullPointerException when register null content observer.");
        } catch (NullPointerException e) {
            //expected.
        }

        try {
            mContentResolver.unregisterContentObserver(null);
            fail("did not throw NullPointerException when unregister null content observer.");
        } catch (NullPointerException e) {
            //expected.
        }
    }

    public void testRegisterContentObserverDescendantBehavior() throws Exception {
        final MockContentObserver mco1 = new MockContentObserver();
        final MockContentObserver mco2 = new MockContentObserver();

        // Register one content observer with notifyDescendants set to false, and
        // another with true.
        mContentResolver.registerContentObserver(LEVEL2_URI, false, mco1);
        mContentResolver.registerContentObserver(LEVEL2_URI, true, mco2);

        // Initially nothing has happened.
        assertFalse(mco1.hadOnChanged());
        assertFalse(mco2.hadOnChanged());

        // Fire a change with the exact URI.
        // Should signal both observers due to exact match, notifyDescendants doesn't matter.
        mContentResolver.notifyChange(LEVEL2_URI, null);
        Thread.sleep(200);
        assertTrue(mco1.hadOnChanged());
        assertTrue(mco2.hadOnChanged());
        mco1.reset();
        mco2.reset();

        // Fire a change with a descendant URI.
        // Should only signal observer with notifyDescendants set to true.
        mContentResolver.notifyChange(LEVEL3_URI, null);
        Thread.sleep(200);
        assertFalse(mco1.hadOnChanged());
        assertTrue(mco2.hadOnChanged());
        mco2.reset();

        // Fire a change with an ancestor URI.
        // Should signal both observers due to ancestry, notifyDescendants doesn't matter.
        mContentResolver.notifyChange(LEVEL1_URI, null);
        Thread.sleep(200);
        assertTrue(mco1.hadOnChanged());
        assertTrue(mco2.hadOnChanged());
        mco1.reset();
        mco2.reset();

        // Fire a change with an unrelated URI.
        // Should signal neither observer.
        mContentResolver.notifyChange(TABLE1_URI, null);
        Thread.sleep(200);
        assertFalse(mco1.hadOnChanged());
        assertFalse(mco2.hadOnChanged());
    }

    public void testNotifyChange1() {
        final MockContentObserver mco = new MockContentObserver();

        mContentResolver.registerContentObserver(TABLE1_URI, true, mco);
        assertFalse(mco.hadOnChanged());

        mContentResolver.notifyChange(TABLE1_URI, mco);
        new PollingCheck() {
            @Override
            protected boolean check() {
                return mco.hadOnChanged();
            }
        }.run();

        mContentResolver.unregisterContentObserver(mco);
    }

    public void testNotifyChange2() {
        final MockContentObserver mco = new MockContentObserver();

        mContentResolver.registerContentObserver(TABLE1_URI, true, mco);
        assertFalse(mco.hadOnChanged());

        mContentResolver.notifyChange(TABLE1_URI, mco, false);
        new PollingCheck() {
            @Override
            protected boolean check() {
                return mco.hadOnChanged();
            }
        }.run();

        mContentResolver.unregisterContentObserver(mco);
    }

    /**
     * Verify that callers using the {@link Iterable} version of
     * {@link ContentResolver#notifyChange} are correctly split and delivered to
     * disjoint listeners.
     */
    public void testNotifyChange_MultipleSplit() {
        final MockContentObserver observer1 = new MockContentObserver();
        final MockContentObserver observer2 = new MockContentObserver();

        mContentResolver.registerContentObserver(TABLE1_URI, true, observer1);
        mContentResolver.registerContentObserver(TABLE2_URI, true, observer2);

        assertFalse(observer1.hadOnChanged());
        assertFalse(observer2.hadOnChanged());

        final ArrayList<Uri> list = new ArrayList<>();
        list.add(TABLE1_URI);
        list.add(TABLE2_URI);
        mContentResolver.notifyChange(list, null, 0);

        new PollingCheck() {
            @Override
            protected boolean check() {
                return observer1.hadOnChanged() && observer2.hadOnChanged();
            }
        }.run();

        mContentResolver.unregisterContentObserver(observer1);
        mContentResolver.unregisterContentObserver(observer2);
    }

    /**
     * Verify that callers using the {@link Iterable} version of
     * {@link ContentResolver#notifyChange} are correctly grouped and delivered
     * to overlapping listeners, including untouched flags.
     */
    public void testNotifyChange_MultipleFlags() {
        final MockContentObserver observer1 = new MockContentObserver();
        final MockContentObserver observer2 = new MockContentObserver();

        mContentResolver.registerContentObserver(LEVEL1_URI, false, observer1);
        mContentResolver.registerContentObserver(LEVEL2_URI, false, observer2);

        mContentResolver.notifyChange(
                Arrays.asList(LEVEL1_URI), null, 0);
        mContentResolver.notifyChange(
                Arrays.asList(LEVEL1_URI, LEVEL2_URI), null, NOTIFY_INSERT);
        mContentResolver.notifyChange(
                Arrays.asList(LEVEL2_URI), null, NOTIFY_UPDATE);

        final List<Change> expected1 = Arrays.asList(
                new Change(false, Arrays.asList(LEVEL1_URI), 0),
                new Change(false, Arrays.asList(LEVEL1_URI), NOTIFY_INSERT));

        final List<Change> expected2 = Arrays.asList(
                new Change(false, Arrays.asList(LEVEL1_URI), 0),
                new Change(false, Arrays.asList(LEVEL1_URI, LEVEL2_URI), NOTIFY_INSERT),
                new Change(false, Arrays.asList(LEVEL2_URI), NOTIFY_UPDATE));

        new PollingCheck() {
            @Override
            protected boolean check() {
                return observer1.hadChanges(expected1)
                        && observer2.hadChanges(expected2);
            }
        }.run();

        mContentResolver.unregisterContentObserver(observer1);
        mContentResolver.unregisterContentObserver(observer2);
    }

    public void testStartCancelSync() {
        Bundle extras = new Bundle();

        extras.putBoolean(ContentResolver.SYNC_EXTRAS_MANUAL, true);

        ContentResolver.requestSync(ACCOUNT, AUTHORITY, extras);
        //FIXME: how to get the result to assert.

        ContentResolver.cancelSync(ACCOUNT, AUTHORITY);
        //FIXME: how to assert.
    }

    public void testStartSyncFailure() {
        try {
            ContentResolver.requestSync(null, null, null);
            fail("did not throw IllegalArgumentException when extras is null.");
        } catch (IllegalArgumentException e) {
            //expected.
        }
    }

    public void testValidateSyncExtrasBundle() {
        Bundle extras = new Bundle();
        extras.putInt("Integer", 20);
        extras.putLong("Long", 10l);
        extras.putBoolean("Boolean", true);
        extras.putFloat("Float", 5.5f);
        extras.putDouble("Double", 2.5);
        extras.putString("String", "cts");
        extras.putCharSequence("CharSequence", null);

        ContentResolver.validateSyncExtrasBundle(extras);

        extras.putChar("Char", 'a'); // type Char is invalid
        try {
            ContentResolver.validateSyncExtrasBundle(extras);
            fail("did not throw IllegalArgumentException when extras is invalide.");
        } catch (IllegalArgumentException e) {
            //expected.
        }
    }

    @AppModeFull
    public void testHangRecover() throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(android.Manifest.permission.REMOVE_TASKS);

        final CountDownLatch latch = new CountDownLatch(1);
        new Thread(() -> {
            final ContentProviderClient client = mContentResolver
                    .acquireUnstableContentProviderClient(REMOTE_AUTHORITY);
            client.setDetectNotResponding(2_000);
            try {
                client.query(REMOTE_HANG_URI, null, null, null);
                fail("Funky, we somehow returned?");
            } catch (RemoteException e) {
                latch.countDown();
            }
        }).start();

        // The remote process should have been killed after the ANR was detected
        // above, causing our pending call to return and release our latch above
        // within 10 seconds; if our Binder thread hasn't been freed, then we
        // fail with a timeout.
        latch.await(10, TimeUnit.SECONDS);
    }

    public void testGetTypeInfo() throws Exception {
        for (String mimeType : new String[] {
                "image/png",
                "IMage/PnG",
                "image/x-custom",
                "application/x-flac",
                "application/rdf+xml",
                "x-custom/x-custom",
        }) {
            final MimeTypeInfo ti = mContentResolver.getTypeInfo(mimeType);
            assertNotNull(ti);
            assertNotNull(ti.getLabel());
            assertNotNull(ti.getContentDescription());
            assertNotNull(ti.getIcon());
        }
    }

    public void testGetTypeInfo_Invalid() throws Exception {
        try {
            mContentResolver.getTypeInfo(null);
            fail("Expected exception for null");
        } catch (NullPointerException expected) {
        }
    }

    public void testWrapContentProvider() throws Exception {
        try (ContentProviderClient local = getContext().getContentResolver()
                .acquireContentProviderClient(AUTHORITY)) {
            final ContentResolver resolver = ContentResolver.wrap(local.getLocalContentProvider());
            assertNotNull(resolver.getType(TABLE1_URI));
            try {
                resolver.getType(REMOTE_TABLE1_URI);
                fail();
            } catch (SecurityException | IllegalArgumentException expected) {
            }
        }
    }

    public void testWrapContentProviderClient() throws Exception {
        try (ContentProviderClient remote = getContext().getContentResolver()
                .acquireContentProviderClient(REMOTE_AUTHORITY)) {
            final ContentResolver resolver = ContentResolver.wrap(remote);
            assertNotNull(resolver.getType(REMOTE_TABLE1_URI));
            try {
                resolver.getType(TABLE1_URI);
                fail();
            } catch (SecurityException | IllegalArgumentException expected) {
            }
        }
    }

    @AppModeFull
    public void testContentResolverCaching() throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                android.Manifest.permission.CACHE_CONTENT,
                android.Manifest.permission.INTERACT_ACROSS_USERS_FULL);

        Bundle cached = new Bundle();
        cached.putString("key", "value");
        mContentResolver.putCache(TABLE1_URI, cached);

        Bundle response = mContentResolver.getCache(TABLE1_URI);
        assertEquals("value", response.getString("key"));

        ContentValues values = new ContentValues();
        values.put(COLUMN_KEY_NAME, "key10");
        values.put(COLUMN_VALUE_NAME, 10);
        mContentResolver.update(TABLE1_URI, values, null, null);

        response = mContentResolver.getCache(TABLE1_URI);
        assertNull(response);
    }

    public void testEncodeDecode() {
        final Uri expected = Uri.parse("content://com.example/item/23");
        final File file = ContentResolver.encodeToFile(expected);
        assertNotNull(file);

        final Uri actual = ContentResolver.decodeFromFile(file);
        assertNotNull(actual);
        assertEquals(expected, actual);
    }

    public static class Change {
        public final boolean selfChange;
        public final Iterable<Uri> uris;
        public final int flags;

        public Change(boolean selfChange, Iterable<Uri> uris, int flags) {
            this.selfChange = selfChange;
            this.uris = uris;
            this.flags = flags;
        }

        @Override
        public String toString() {
            return String.format("onChange(%b, %s, %d)",
                    selfChange, asSet(uris).toString(), flags);
        }

        @Override
        public boolean equals(Object other) {
            if (other instanceof Change) {
                final Change change = (Change) other;
                return change.selfChange == selfChange &&
                        Objects.equals(asSet(change.uris), asSet(uris)) &&
                        change.flags == flags;
            } else {
                return false;
            }
        }

        private static Set<Uri> asSet(Iterable<Uri> uris) {
            final Set<Uri> asSet = new HashSet<>();
            uris.forEach(asSet::add);
            return asSet;
        }
    }

    private static class MockContentObserver extends ContentObserver {
        private boolean mHadOnChanged = false;
        private List<Change> mChanges = new ArrayList<>();

        public MockContentObserver() {
            super(null);
        }

        @Override
        public boolean deliverSelfNotifications() {
            return true;
        }

        @Override
        public synchronized void onChange(boolean selfChange, Collection<Uri> uris, int flags) {
            final Change change = new Change(selfChange, uris, flags);
            Log.v(TAG, change.toString());

            mHadOnChanged = true;
            mChanges.add(change);
        }

        public synchronized boolean hadOnChanged() {
            return mHadOnChanged;
        }

        public synchronized void reset() {
            mHadOnChanged = false;
        }

        public synchronized boolean hadChanges(Collection<Change> changes) {
            return mChanges.containsAll(changes);
        }
    }
}
