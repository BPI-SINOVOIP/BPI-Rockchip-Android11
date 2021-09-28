/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.tradefed.contentprovider;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * Tests for {@link android.tradefed.contentprovider.ManagedFileContentProvider}. TODO: Complete the
 * tests when automatic test setup is made.
 */
@RunWith(AndroidJUnit4.class)
public class ManagedFileContentProviderTest {

    public static final String CONTENT_PROVIDER_AUTHORITY = "android.tradefed.contentprovider";
    private static final String TEST_FILE = "ManagedFileContentProviderTest.txt";
    private static final String TEST_DIRECTORY = "ManagedFileContentProviderTestDir";
    private static final String TEST_SUBDIRECTORY = "ManagedFileContentProviderTestSubDir";

    private File mTestFile = null;
    private File mTestDir = null;
    private File mTestSubdir = null;

    private Uri mTestFileUri;
    private Uri mTestDirUri;
    private Uri mTestSubdirUri;

    private Context mAppContext;
    private ContentResolver mResolver;
    private List<Uri> mShouldBeCleaned = new ArrayList<>();
    private ContentValues mCv;

    @Before
    public void setUp() throws Exception {
        mCv = new ContentValues();

        // Context of the app under test.
        mAppContext = InstrumentationRegistry.getTargetContext();
        mResolver = mAppContext.getContentResolver();
        assertEquals("android.tradefed.contentprovider.test", mAppContext.getPackageName());
    }

    @After
    public void tearDown() {
        if (mTestFile != null) {
            mTestFile.delete();
        }
        if (mTestDir != null) {
            mTestDir.delete();
        }
        if (mTestSubdir != null) {
            mTestSubdir.delete();
        }
        for (Uri uri : mShouldBeCleaned) {
            mResolver.delete(
                    uri,
                    /** selection * */
                    null,
                    /** selectionArgs * */
                    null);
        }
    }

    private void createTestDirectories() throws Exception {
        mTestDir = new File(Environment.getExternalStorageDirectory(), TEST_DIRECTORY);
        mTestDir.mkdir();
        mTestSubdir = new File(mTestDir, TEST_SUBDIRECTORY);
        mTestSubdir.mkdir();
        createTestFile(mTestDir);
    }

    private void createTestFile(File parentDir) throws Exception {
        mTestFile = new File(parentDir, TEST_FILE);
        mTestFile.createNewFile();

        mTestFileUri = createContentUri(mTestFile.getAbsolutePath());
    }

    /** Test that we can delete a file from the content provider. */
    @Test
    public void testDelete() throws Exception {
        createTestFile(Environment.getExternalStorageDirectory());
        Uri uriResult = mResolver.insert(mTestFileUri, mCv);
        mShouldBeCleaned.add(mTestFileUri);
        // Insert is successful
        assertEquals(mTestFileUri, uriResult);

        // Trying to insert again is inop
        Uri reInsert = mResolver.insert(mTestFileUri, mCv);
        assertNull(reInsert);

        // Now delete
        int affected =
                mResolver.delete(
                        mTestFileUri,
                        /** selection * */
                        null,
                        /** selectionArgs * */
                        null);
        assertEquals(1, affected);
        // File should have been deleted.
        assertFalse(mTestFile.exists());

        // We can now insert again
        mTestFile.createNewFile();
        uriResult = mResolver.insert(mTestFileUri, mCv);
        assertEquals(mTestFileUri, uriResult);
    }

    /** Test that querying the content provider for a single File returns null. */
    @Test
    public void testQueryForFile() throws Exception {
        createTestFile(Environment.getExternalStorageDirectory());
        Cursor cursor =
                mResolver.query(
                        mTestFileUri,
                        /** projection * */
                        null,
                        /** selection * */
                        null,
                        /** selectionArgs* */
                        null,
                        /** sortOrder * */
                        null);
        try {
            assertEquals(1, cursor.getCount());
            String[] columns = cursor.getColumnNames();
            assertEquals(ManagedFileContentProvider.COLUMNS, columns);
            assertTrue(cursor.moveToNext());

            // Test values in all columns and enforce column ordering.
            // Name
            assertEquals(TEST_FILE, cursor.getString(0));
            // Absolute path
            assertEquals(
                    Environment.getExternalStorageDirectory().getAbsolutePath() + "/" + TEST_FILE,
                    cursor.getString(1));
            // Is directory
            assertEquals("false", cursor.getString(2));
            // Type
            assertEquals("text/plain", cursor.getString(3));
            // Metadata
            assertNull(cursor.getString(4));
        } finally {
            cursor.close();
        }
    }

    /** Test that querying the content provider for a file is working when abstracting the sdcard */
    @Test
    public void testQueryForFile_sdcard() throws Exception {
        createTestFile(Environment.getExternalStorageDirectory());
        Uri sdcardUri = createContentUri(String.format("sdcard/%s", mTestFile.getName()));

        Cursor cursor =
                mResolver.query(
                        sdcardUri,
                        /** projection * */
                        null,
                        /** selection * */
                        null,
                        /** selectionArgs* */
                        null,
                        /** sortOrder * */
                        null);
        try {
            assertEquals(1, cursor.getCount());
            String[] columns = cursor.getColumnNames();
            assertEquals(ManagedFileContentProvider.COLUMNS, columns);
            assertTrue(cursor.moveToNext());

            // Test values in all columns and enforce column ordering.
            // Name
            assertEquals(TEST_FILE, cursor.getString(0));
            // Absolute path
            assertEquals(
                    Environment.getExternalStorageDirectory().getAbsolutePath() + "/" + TEST_FILE,
                    cursor.getString(1));
            // Is directory
            assertEquals("false", cursor.getString(2));
            // Type
            assertEquals("text/plain", cursor.getString(3));
            // Metadata
            assertNull(cursor.getString(4));
        } finally {
            cursor.close();
        }
    }

    /**
     * Test that querying the content provider for a directory returns content of the directory -
     * one row per each subdirectory/file.
     */
    @Test
    public void testQueryForDirectoryContent() throws Exception {
        createTestDirectories();

        mTestDirUri = createContentUri(mTestDir.getAbsolutePath());
        Cursor cursor =
                mResolver.query(
                        mTestDirUri,
                        /** projection * */
                        null,
                        /** selection * */
                        null,
                        /** selectionArgs* */
                        null,
                        /** sortOrder * */
                        null);
        try {
            // One row for subdir, one row for a file.
            assertEquals(2, cursor.getCount());
            String[] columns = cursor.getColumnNames();
            assertEquals(ManagedFileContentProvider.COLUMNS, columns);

            // Test the file.
            assertTrue(cursor.moveToNext());
            // Test values in all columns and enforce column ordering.
            // Name
            assertEquals(TEST_FILE, cursor.getString(0));
            // Absolute path
            assertEquals(
                    Environment.getExternalStorageDirectory().getAbsolutePath()
                            + "/"
                            + TEST_DIRECTORY
                            + "/"
                            + TEST_FILE,
                    cursor.getString(1));
            // Is directory
            assertEquals("false", cursor.getString(2));
            // Type
            assertEquals("text/plain", cursor.getString(3));
            // Metadata
            assertNull(cursor.getString(4));

            // Test the subdirectory.
            assertTrue(cursor.moveToNext());
            // Test values in all columns and enforce column ordering.
            // Name
            assertEquals(TEST_SUBDIRECTORY, cursor.getString(0));
            // Absolute path
            assertEquals(
                    Environment.getExternalStorageDirectory().getAbsolutePath()
                            + "/"
                            + TEST_DIRECTORY
                            + "/"
                            + TEST_SUBDIRECTORY,
                    cursor.getString(1));
            // Is directory
            assertEquals("true", cursor.getString(2));
            // Type
            assertNull(cursor.getString(3));
            // Metadata
            assertNull(cursor.getString(4));
        } finally {
            cursor.close();
        }
    }

    private Uri createContentUri(String path) {
        Uri.Builder builder = new Uri.Builder();
        return builder.scheme(ContentResolver.SCHEME_CONTENT)
                .authority(CONTENT_PROVIDER_AUTHORITY)
                .appendPath(path)
                .build();
    }
}
