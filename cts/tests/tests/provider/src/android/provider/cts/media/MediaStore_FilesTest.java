/*
 * Copyright (C) 2012 The Android Open Source Project
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

package android.provider.cts.media;

import static android.provider.cts.ProviderTestUtils.containsId;
import static android.provider.cts.ProviderTestUtils.resolveVolumeName;
import static android.provider.cts.media.MediaStoreTest.TAG;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.provider.MediaStore.Files.FileColumns;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

@RunWith(Parameterized.class)
public class MediaStore_FilesTest {
    private Context mContext;
    private ContentResolver mResolver;

    private Uri mExternalAudio;
    private Uri mExternalImages;
    private Uri mExternalFiles;

    @Parameter(0)
    public String mVolumeName;

    @Parameters
    public static Iterable<? extends Object> data() {
        return ProviderTestUtils.getSharedVolumeNames();
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mResolver = mContext.getContentResolver();

        Log.d(TAG, "Using volume " + mVolumeName);
        mExternalAudio = MediaStore.Audio.Media.getContentUri(mVolumeName);
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);
        mExternalFiles = MediaStore.Files.getContentUri(mVolumeName);
    }

    @Test
    public void testGetContentUri() throws Exception {
        Uri allFilesUri = mExternalFiles;

        ContentValues values = new ContentValues();

        // Add a path for a file and check that the returned uri appends a
        // path properly.
        String dataPath = new File(ProviderTestUtils.stageDir(mVolumeName),
                "does_not_really_exist.txt").getAbsolutePath();
        values.put(MediaColumns.DATA, dataPath);
        Uri fileUri = mResolver.insert(allFilesUri, values);
        long fileId = ContentUris.parseId(fileUri);
        assertEquals(fileUri, ContentUris.withAppendedId(allFilesUri, fileId));

        // Check that getContentUri with the file id produces the same url
        Uri rowUri = ContentUris.withAppendedId(mExternalFiles, fileId);
        assertEquals(fileUri, rowUri);

        // Check that the file count has increased.
        assertTrue(containsId(allFilesUri, fileId));

        // Check that the path we inserted was stored properly.
        assertStringColumn(fileUri, MediaColumns.DATA, dataPath);

        // Update the path and check that the database changed.
        String updatedPath = new File(ProviderTestUtils.stageDir(mVolumeName),
                "still_does_not_exist.txt").getAbsolutePath();
        values.put(MediaColumns.DATA, updatedPath);
        assertEquals(1, mResolver.update(fileUri, values, null, null));
        assertStringColumn(fileUri, MediaColumns.DATA, updatedPath);

        // check that inserting a duplicate entry updates previous entry.
        Uri foo = mResolver.insert(allFilesUri, values);
        assertEquals(foo, fileUri);

        // Delete the file and observe that the file count decreased.
        assertEquals(1, mResolver.delete(fileUri, null, null));
        assertFalse(containsId(allFilesUri, fileId));

        // Make sure the deleted file is not returned by the cursor.
        Cursor cursor = mResolver.query(fileUri, null, null, null, null);
        try {
            assertFalse(cursor.moveToNext());
        } finally {
            cursor.close();
        }

        // insert file and check its parent
        values.clear();
        try {
            File stageDir = new File(ProviderTestUtils.stageDir(mVolumeName),
                    Environment.DIRECTORY_MUSIC);
            stageDir.mkdirs();
            String b = stageDir.getAbsolutePath();
            values.put(MediaColumns.DATA, b + "/testing" + System.nanoTime());
            fileUri = mResolver.insert(allFilesUri, values);
            cursor = mResolver.query(fileUri, new String[] { MediaStore.Files.FileColumns.PARENT },
                    null, null, null);
            assertEquals(1, cursor.getCount());
            cursor.moveToFirst();
            long parentid = cursor.getLong(0);
            assertTrue("got 0 parent for non root file", parentid != 0);

            cursor.close();
            cursor = mResolver.query(ContentUris.withAppendedId(allFilesUri, parentid),
                    new String[] { MediaColumns.DATA }, null, null, null);
            assertEquals(1, cursor.getCount());
            cursor.moveToFirst();
            String parentPath = cursor.getString(0);
            assertEquals(b, parentPath);

            mResolver.delete(fileUri, null, null);
        } catch (IOException e) {
            fail(e.getMessage());
        } finally {
            cursor.close();
        }

        assertEquals(ContentUris.withAppendedId(MediaStore.Files.getContentUri(mVolumeName), 42),
                MediaStore.Files.getContentUri(mVolumeName, 42));
    }

    @Test
    public void testCaseSensitivity() throws IOException {
        final String name = "Test-" + System.nanoTime() + ".Mp3";
        final File dir = ProviderTestUtils.stageDir(mVolumeName);
        final File file = new File(dir, name);
        final File fileLower = new File(dir, name.toLowerCase());
        ProviderTestUtils.stageFile(R.raw.testmp3, file);

        Uri allFilesUri = mExternalFiles;
        ContentValues values = new ContentValues();
        values.put(MediaColumns.DATA, fileLower.getAbsolutePath());
        Uri fileUri = mResolver.insert(allFilesUri, values);
        try {
            ParcelFileDescriptor pfd = mResolver.openFileDescriptor(fileUri, "r");
            pfd.close();
        } finally {
            mResolver.delete(fileUri, null, null);
        }
    }

    @Test
    public void testAccessInternal() throws Exception {
        final Uri internalFiles = MediaStore.Files.getContentUri(MediaStore.VOLUME_INTERNAL);

        for (String valid : new String[] {
                "/system/media/" + System.nanoTime() + ".ogg",
        }) {
            final ContentValues values = new ContentValues();
            values.put(MediaColumns.DATA, valid);

            final Uri uri = mResolver.insert(internalFiles, values);
            assertNotNull(valid, uri);
            mResolver.delete(uri, null, null);
        }

        for (String invalid : new String[] {
                "/data/media/" + System.nanoTime() + ".jpg",
                "/data/system/appops.xml",
                "/data/data/com.android.providers.media/databases/internal.db",
                new File(Environment.getExternalStorageDirectory(), System.nanoTime() + ".jpg")
                        .getAbsolutePath(),
        }) {
            final ContentValues values = new ContentValues();
            values.put(MediaColumns.DATA, invalid);
            assertNull(invalid, mResolver.insert(internalFiles, values));
        }
    }

    @Test
    public void testAccess() throws Exception {
        final String path = ProviderTestUtils.getVolumePath(resolveVolumeName(mVolumeName))
                .getAbsolutePath();
        final Uri updateUri = ContentUris.withAppendedId(mExternalFiles,
                ContentUris.parseId(ProviderTestUtils.stageMedia(R.raw.volantis, mExternalImages)));

        for (String valid : new String[] {
                path + "/" + System.nanoTime() + ".jpg",
                path + "/DCIM/" + System.nanoTime() + ".jpg",
        }) {
            final ContentValues values = new ContentValues();
            values.put(MediaColumns.DATA, valid);

            final Uri uri = mResolver.insert(mExternalFiles, values);
            assertNotNull(valid, uri);
            mResolver.delete(uri, null, null);

            final int count = mResolver.update(updateUri, values, null, null);
            assertEquals(valid, 1, count);
        }

        for (String invalid : new String[] {
                "/data/media/" + System.nanoTime() + ".jpg",
                "/data/system/appops.xml",
                "/data/data/com.android.providers.media/databases/internal.db",
                path + "/../../../../../data/system/appops.xml",
        }) {
            final ContentValues values = new ContentValues();
            values.put(MediaColumns.DATA, invalid);

            try {
                assertNull(invalid, mResolver.insert(mExternalFiles, values));
            } catch (SecurityException tolerated) {
            }

            try {
                assertEquals(invalid, 0, mResolver.update(updateUri, values, null, null));
            } catch (SecurityException tolerated) {
            }
        }
    }

    @Test
    public void testUpdateMediaType() throws Exception {
        final File file = new File(ProviderTestUtils.stageDir(mVolumeName),
                "test" + System.nanoTime() + ".mp3");
        ProviderTestUtils.stageFile(R.raw.testmp3, file);

        Uri allFilesUri = mExternalFiles;
        ContentValues values = new ContentValues();
        values.put(MediaColumns.DATA, file.getAbsolutePath());
        values.put(FileColumns.MEDIA_TYPE, FileColumns.MEDIA_TYPE_AUDIO);
        Uri fileUri = mResolver.insert(allFilesUri, values);

        // There is special logic in MediaProvider#update() to update paths when a folder was moved
        // or renamed. It only checks whether newValues only has one column but assumes the provided
        // column is _data. We need to guard the case where there is only one column in newValues
        // and it's not _data.
        ContentValues newValues = new ContentValues(1);
        newValues.put(FileColumns.MEDIA_TYPE, FileColumns.MEDIA_TYPE_NONE);
        mResolver.update(fileUri, newValues, null, null);

        try (Cursor c = mResolver.query(
                fileUri, new String[] { FileColumns.MEDIA_TYPE }, null, null, null)) {
            c.moveToNext();
            assertEquals(FileColumns.MEDIA_TYPE_NONE,
                    c.getInt(c.getColumnIndex(FileColumns.MEDIA_TYPE)));
        }
    }

    @Test
    public void testInsertDocumentFile() throws Exception {
        final File file = new File(ProviderTestUtils.stageDir(mVolumeName),
                "test" + System.nanoTime() + ".txt");

        Uri allFilesUri = mExternalFiles;
        ContentValues values = new ContentValues();
        values.put(MediaColumns.DATA, file.getAbsolutePath());
        values.put(FileColumns.MEDIA_TYPE, FileColumns.MEDIA_TYPE_NONE);
        Uri fileUri = mResolver.insert(allFilesUri, values);

        try (Cursor c = mResolver.query(
                fileUri, new String[] { FileColumns.MEDIA_TYPE }, null, null, null)) {
            c.moveToNext();
            assertEquals(FileColumns.MEDIA_TYPE_DOCUMENT,
                    c.getInt(c.getColumnIndex(FileColumns.MEDIA_TYPE)));
        }
    }

    @Test
    public void testDateAddedFrozen() throws Exception {
        final long startTime = (System.currentTimeMillis() / 1000);
        final File file = new File(ProviderTestUtils.stageDir(mVolumeName),
                "test" + System.nanoTime() + ".mp3");
        ProviderTestUtils.stageFile(R.raw.testmp3, file);

        final ContentValues values = new ContentValues();
        values.put(MediaColumns.DATA, file.getAbsolutePath());
        values.put(MediaColumns.DATE_ADDED, 32);
        final Uri uri = mResolver.insert(mExternalFiles, values);

        assertTrue(queryLong(uri, MediaColumns.DATE_ADDED) >= startTime);

        values.clear();
        values.put(MediaColumns.DATE_ADDED, 64);
        mResolver.update(uri, values, null, null);

        assertTrue(queryLong(uri, MediaColumns.DATE_ADDED) >= startTime);
    }

    @Test
    public void testInPlaceUpdate_mediaFileWithInvalidRelativePath() throws Exception {
        final File file = new File(ProviderTestUtils.stageDownloadDir(mVolumeName),
                "test" + System.nanoTime() + ".mp3");
        ProviderTestUtils.stageFile(R.raw.testmp3, file);
        Log.d(TAG, "Staged audio file at " + file.getAbsolutePath());

        // Since file is created by shell, package name in MediaStore database row for this file
        // will not be test app's package name. To treat the insert as upsert, package name in
        // database must be test app's package name. Force test app to be the owner of database row.
        final Uri scannedUri = ProviderTestUtils.scanFile(file);
        ProviderTestUtils.setOwner(scannedUri,
                InstrumentationRegistry.getContext().getPackageName());

        final ContentValues insertValues = new ContentValues();
        insertValues.put(MediaColumns.DATA, file.getAbsolutePath());
        insertValues.put(MediaStore.Audio.AudioColumns.BOOKMARK, 42L);
        final Uri uri = mResolver.insert(mExternalAudio, insertValues);
        assertEquals(42L, queryLong(uri, MediaStore.Audio.AudioColumns.BOOKMARK));

        final ContentValues updateValues = new ContentValues();
        updateValues.put(MediaStore.Audio.AudioColumns.BOOKMARK, 43L);
        assertEquals(1, mResolver.update(uri, updateValues, null, null));
        assertEquals(43L, queryLong(uri, MediaStore.Audio.AudioColumns.BOOKMARK));
    }

    @Test
    public void testGeneration() throws Exception {
        final long before = MediaStore.getGeneration(mContext, mVolumeName);

        final ContentProviderOperation redAdd = ContentProviderOperation.newInsert(mExternalImages)
                .withValue(MediaColumns.DISPLAY_NAME, "red" + System.nanoTime() + ".png")
                .withValue(MediaColumns.MIME_TYPE, "image/png").build();
        final ContentProviderOperation greAdd = ContentProviderOperation.newInsert(mExternalImages)
                .withValue(MediaColumns.DISPLAY_NAME, "green" + System.nanoTime() + ".png")
                .withValue(MediaColumns.MIME_TYPE, "image/png").build();
        final ContentProviderOperation bluAdd = ContentProviderOperation.newInsert(mExternalImages)
                .withValue(MediaColumns.DISPLAY_NAME, "blue" + System.nanoTime() + ".png")
                .withValue(MediaColumns.MIME_TYPE, "image/png").build();
        final ContentProviderResult[] first = mResolver.applyBatch(mExternalImages.getAuthority(),
                new ArrayList<>(Arrays.asList(redAdd)));
        final ContentProviderResult[] second = mResolver.applyBatch(mExternalImages.getAuthority(),
                new ArrayList<>(Arrays.asList(greAdd, bluAdd)));
        final long afterAdd = MediaStore.getGeneration(mContext, mVolumeName);

        final Uri red = first[0].uri;
        final Uri gre = second[0].uri;
        final Uri blu = second[1].uri;

        final ContentProviderOperation redMod = ContentProviderOperation.newUpdate(red)
                .withValue(MediaColumns.IS_FAVORITE, 1).build();
        final ContentProviderOperation greMod = ContentProviderOperation.newUpdate(gre)
                .withValue(MediaColumns.IS_FAVORITE, 1).build();
        final ContentProviderResult[] third = mResolver.applyBatch(mExternalImages.getAuthority(),
                new ArrayList<>(Arrays.asList(redMod, greMod)));
        final long afterMod = MediaStore.getGeneration(mContext, mVolumeName);

        final long radd = queryLong(red, MediaColumns.GENERATION_ADDED);
        final long rmod = queryLong(red, MediaColumns.GENERATION_MODIFIED);
        final long gadd = queryLong(gre, MediaColumns.GENERATION_ADDED);
        final long gmod = queryLong(gre, MediaColumns.GENERATION_MODIFIED);
        final long badd = queryLong(blu, MediaColumns.GENERATION_ADDED);
        final long bmod = queryLong(blu, MediaColumns.GENERATION_MODIFIED);

        assertTrue("before < radd", before < radd);
        assertTrue("radd < gadd", radd < gadd);
        assertTrue("gadd == badd", gadd == badd);
        assertTrue("badd <= afterAdd", badd <= afterAdd);
        assertTrue("afterAdd < rmod", afterAdd < rmod);
        assertTrue("rmod == gmod", rmod == gmod);
        assertTrue("gmod <= afterMod", gmod <= afterMod);
    }

    private long queryLong(Uri uri, String columnName) {
        try (Cursor c = mResolver.query(uri, new String[] { columnName }, null, null, null)) {
            assertTrue(c.moveToFirst());
            return c.getLong(0);
        }
    }

    private String queryString(Uri uri, String columnName) {
        try (Cursor c = mResolver.query(uri, new String[] { columnName }, null, null, null)) {
            assertTrue(c.moveToFirst());
            return c.getString(0);
        }
    }

    private void assertStringColumn(Uri fileUri, String columnName, String expectedValue) {
        assertEquals(expectedValue, queryString(fileUri, columnName));
    }
}
