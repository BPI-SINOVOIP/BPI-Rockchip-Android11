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

package android.provider.cts.media;

import static android.provider.cts.ProviderTestUtils.containsId;
import static android.provider.cts.ProviderTestUtils.getRawFile;
import static android.provider.cts.ProviderTestUtils.getRawFileHash;
import static android.provider.cts.ProviderTestUtils.hash;
import static android.provider.cts.media.MediaStoreTest.TAG;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.FileUtils;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.provider.cts.media.MediaStoreUtils.PendingParams;
import android.provider.cts.media.MediaStoreUtils.PendingSession;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.common.base.Objects;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.NoSuchFileException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

@RunWith(Parameterized.class)
public class MediaStorePendingTest {
    private Context mContext;
    private ContentResolver mResolver;

    private Uri mExternalAudio;
    private Uri mExternalVideo;
    private Uri mExternalImages;
    private Uri mExternalDownloads;

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
        mExternalVideo = MediaStore.Video.Media.getContentUri(mVolumeName);
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);
        mExternalDownloads = MediaStore.Downloads.getContentUri(mVolumeName);
    }

    @Test
    public void testSimple_Success() throws Exception {
        verifySuccessfulImageInsertion(mExternalImages, Environment.DIRECTORY_PICTURES);
    }

    @Test
    public void testSimpleDownload_Success() throws Exception {
        verifySuccessfulImageInsertion(mExternalDownloads, Environment.DIRECTORY_DOWNLOADS);
    }

    private void verifySuccessfulImageInsertion(Uri insertUri, String expectedDestDir)
            throws Exception {
        final String displayName = "cts" + System.nanoTime();

        final PendingParams params = new PendingParams(
                insertUri, displayName, "image/png");

        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        final long id = ContentUris.parseId(pendingUri);

        // Verify pending status across various queries
        try (Cursor c = mResolver.query(pendingUri,
                new String[] { MediaColumns.IS_PENDING }, null, null)) {
            assertTrue(c.moveToFirst());
            assertEquals(1, c.getInt(0));
        }
        assertFalse(containsId(insertUri, id));
        assertTrue(containsId(MediaStore.setIncludePending(insertUri), id));

        // Write an image into place
        final Uri publishUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(R.raw.scenery);
                 OutputStream out = session.openOutputStream()) {
                FileUtils.copy(in, out);
            }
            publishUri = session.publish();
        }

        // Verify pending status across various queries
        try (Cursor c = mResolver.query(publishUri,
                new String[] { MediaColumns.IS_PENDING }, null, null)) {
            assertTrue(c.moveToFirst());
            assertEquals(0, c.getInt(0));
        }
        assertTrue(containsId(insertUri, id));
        assertTrue(containsId(MediaStore.setIncludePending(insertUri), id));

        // Make sure our raw filename looks sane
        final File rawFile = getRawFile(publishUri);
        assertEquals(displayName + ".png", rawFile.getName());
        assertEquals(expectedDestDir, rawFile.getParentFile().getName());

        // Make sure file actually exists
        getRawFileHash(rawFile);
        try (InputStream in = mResolver.openInputStream(publishUri)) {
        }
    }

    @Test
    public void testSimple_Abandoned() throws Exception {
        final String displayName = "cts" + System.nanoTime();

        final Uri insertUri = mExternalImages;
        final PendingParams params = new PendingParams(
                insertUri, displayName, "image/png");

        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        final File pendingFile;

        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(R.raw.scenery);
                    OutputStream out = session.openOutputStream()) {
                FileUtils.copy(in, out);
            }

            // Pending file should exist
            pendingFile = getRawFile(pendingUri);
            getRawFileHash(pendingFile);

            session.abandon();
        }

        // Should have no record of abandoned item
        try (Cursor c = mResolver.query(pendingUri,
                new String[] { MediaColumns.IS_PENDING }, null, null)) {
            assertFalse(c.moveToNext());
        }

        // Pending file should be gone
        try {
            getRawFileHash(pendingFile);
            fail();
        } catch (NoSuchFileException expected) {
        }
    }

    @Test
    public void testDuplicates() throws Exception {
        final String displayName = "cts" + System.nanoTime();

        final Uri insertUri = mExternalAudio;
        final PendingParams params1 = new PendingParams(
                insertUri, displayName, "audio/mpeg");
        final PendingParams params2 = new PendingParams(
                insertUri, displayName, "audio/mpeg");

        final Uri publishUri1 = execPending(params1, R.raw.testmp3);
        final Uri publishUri2 = execPending(params2, R.raw.testmp3_2);

        // Make sure both files landed with unique filenames, and that we didn't
        // cross the streams
        final File rawFile1 = getRawFile(publishUri1);
        final File rawFile2 = getRawFile(publishUri2);
        assertFalse(Objects.equal(rawFile1, rawFile2));

        assertArrayEquals(hash(mContext.getResources().openRawResource(R.raw.testmp3)),
                hash(mResolver.openInputStream(publishUri1)));
        assertArrayEquals(hash(mContext.getResources().openRawResource(R.raw.testmp3_2)),
                hash(mResolver.openInputStream(publishUri2)));
    }

    @Test
    public void testMimeTypes() throws Exception {
        final String displayName = "cts" + System.nanoTime();

        assertCreatePending(new PendingParams(mExternalAudio, displayName, "audio/ogg"));
        assertNotCreatePending(new PendingParams(mExternalAudio, displayName, "video/ogg"));
        assertNotCreatePending(new PendingParams(mExternalAudio, displayName, "image/png"));

        assertNotCreatePending(new PendingParams(mExternalVideo, displayName, "audio/ogg"));
        assertCreatePending(new PendingParams(mExternalVideo, displayName, "video/ogg"));
        assertNotCreatePending(new PendingParams(mExternalVideo, displayName, "image/png"));

        assertNotCreatePending(new PendingParams(mExternalImages, displayName, "audio/ogg"));
        assertNotCreatePending(new PendingParams(mExternalImages, displayName, "video/ogg"));
        assertCreatePending(new PendingParams(mExternalImages, displayName, "image/png"));

        assertCreatePending(new PendingParams(mExternalDownloads, displayName, "audio/ogg"));
        assertCreatePending(new PendingParams(mExternalDownloads, displayName, "video/ogg"));
        assertCreatePending(new PendingParams(mExternalDownloads, displayName, "image/png"));
        assertCreatePending(new PendingParams(mExternalDownloads, displayName,
                "application/pdf"));
    }

    @Test
    public void testMimeTypes_Forced() throws Exception {
        {
            final String displayName = "cts" + System.nanoTime();
            final Uri uri = execPending(new PendingParams(mExternalImages,
                    displayName, "image/png"), R.raw.scenery);
            assertEquals(displayName + ".png", getRawFile(uri).getName());
        }
        {
            final String displayName = "cts" + System.nanoTime() + ".png";
            final Uri uri = execPending(new PendingParams(mExternalImages,
                    displayName, "image/png"), R.raw.scenery);
            assertEquals(displayName, getRawFile(uri).getName());
        }
        {
            final String displayName = "cts" + System.nanoTime() + ".jpg";
            final Uri uri = execPending(new PendingParams(mExternalImages,
                    displayName, "image/png"), R.raw.scenery);
            assertEquals(displayName + ".png", getRawFile(uri).getName());
        }
    }

    @Test
    public void testDirectories() throws Exception {
        final String displayName = "cts" + System.nanoTime();

        final Set<String> allowedAudio = new HashSet<>(
                Arrays.asList(Environment.DIRECTORY_MUSIC, Environment.DIRECTORY_RINGTONES,
                        Environment.DIRECTORY_NOTIFICATIONS, Environment.DIRECTORY_PODCASTS,
                        Environment.DIRECTORY_ALARMS));
        final Set<String> allowedVideo = new HashSet<>(
                Arrays.asList(Environment.DIRECTORY_MOVIES, Environment.DIRECTORY_DCIM,
                Environment.DIRECTORY_PICTURES));
        final Set<String> allowedImages = new HashSet<>(
                Arrays.asList(Environment.DIRECTORY_PICTURES, Environment.DIRECTORY_DCIM));
        final Set<String> allowedDownloads = new HashSet<>(
                Arrays.asList(Environment.DIRECTORY_DOWNLOADS));

        final Set<String> everything = new HashSet<>();
        everything.addAll(allowedAudio);
        everything.addAll(allowedVideo);
        everything.addAll(allowedImages);
        everything.addAll(allowedDownloads);
        everything.add(Environment.DIRECTORY_DOCUMENTS);

        {
            final PendingParams params = new PendingParams(mExternalAudio,
                    displayName, "audio/ogg");
            for (String dir : everything) {
                params.setPath(dir);
                if (allowedAudio.contains(dir)) {
                    assertCreatePending(params);
                } else {
                    assertNotCreatePending(dir, params);
                }
            }
        }
        {
            final PendingParams params = new PendingParams(mExternalVideo,
                    displayName, "video/ogg");
            for (String dir : everything) {
                params.setPath(dir);
                if (allowedVideo.contains(dir)) {
                    assertCreatePending(params);
                } else {
                    assertNotCreatePending(dir, params);
                }
            }
        }
        {
            final PendingParams params = new PendingParams(mExternalImages,
                    displayName, "image/png");
            for (String dir : everything) {
                params.setPath(dir);
                if (allowedImages.contains(dir)) {
                    assertCreatePending(params);
                } else {
                    assertNotCreatePending(dir, params);
                }
            }
        }
        {
            final PendingParams params = new PendingParams(mExternalDownloads,
                        displayName, "video/ogg");
            for (String dir : everything) {
                params.setPath(dir);
                if (allowedDownloads.contains(dir)) {
                    assertCreatePending(params);
                } else {
                    assertNotCreatePending(dir, params);
                }
            }
        }
    }

    @Test
    public void testDirectories_Defaults() throws Exception {
        {
            final String displayName = "cts" + System.nanoTime();
            final Uri uri = execPending(new PendingParams(mExternalImages,
                    displayName, "image/png"), R.raw.scenery);
            assertEquals(Environment.DIRECTORY_PICTURES, getRawFile(uri).getParentFile().getName());
        }
        {
            final String displayName = "cts" + System.nanoTime();
            final Uri uri = execPending(new PendingParams(mExternalAudio,
                    displayName, "audio/ogg"), R.raw.scenery);
            assertEquals(Environment.DIRECTORY_MUSIC, getRawFile(uri).getParentFile().getName());
        }
        {
            final String displayName = "cts" + System.nanoTime();
            final Uri uri = execPending(new PendingParams(mExternalVideo,
                    displayName, "video/ogg"), R.raw.scenery);
            assertEquals(Environment.DIRECTORY_MOVIES, getRawFile(uri).getParentFile().getName());
        }
        {
            final String displayName = "cts" + System.nanoTime();
            final Uri uri = execPending(new PendingParams(mExternalDownloads,
                    displayName, "image/png"), R.raw.scenery);
            assertEquals(Environment.DIRECTORY_DOWNLOADS,
                    getRawFile(uri).getParentFile().getName());
        }
    }

    @Test
    public void testDirectories_Primary() throws Exception {
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(mExternalImages, displayName, "image/png");
        params.setPath(Environment.DIRECTORY_DCIM);

        final Uri uri = execPending(params, R.raw.scenery);
        assertEquals(Environment.DIRECTORY_DCIM, getRawFile(uri).getParentFile().getName());

        // Verify that shady paths don't work
        params.setPath("foo/../bar");
        assertNotCreatePending(params);
    }

    @Test
    public void testDirectories_PrimarySecondary() throws Exception {
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(mExternalImages, displayName, "image/png");
        params.setPath("DCIM/Kittens");

        final Uri uri = execPending(params, R.raw.scenery);
        final File rawFile = getRawFile(uri);
        assertEquals("Kittens", rawFile.getParentFile().getName());
        assertEquals(Environment.DIRECTORY_DCIM, rawFile.getParentFile().getParentFile().getName());
    }

    @Test
    public void testMutableColumns() throws Exception {
        // Stage pending content
        final ContentValues values = new ContentValues();
        values.put(MediaColumns.MIME_TYPE, "image/png");
        values.put(MediaColumns.IS_PENDING, 1);
        values.put(MediaColumns.HEIGHT, 32);
        final Uri uri = mResolver.insert(mExternalImages, values);
        try (InputStream in = mContext.getResources().openRawResource(R.raw.scenery);
                OutputStream out = mResolver.openOutputStream(uri)) {
            FileUtils.copy(in, out);
        }

        // Verify that initial values are present
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            c.moveToFirst();
            assertEquals(32, c.getLong(c.getColumnIndexOrThrow(MediaColumns.HEIGHT)));
        }

        // Verify that we can update values while pending
        values.clear();
        values.put(MediaColumns.HEIGHT, 64);
        mResolver.update(uri, values, null, null);
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            c.moveToFirst();
            assertEquals(64, c.getLong(c.getColumnIndexOrThrow(MediaColumns.HEIGHT)));
        }

        // Publishing triggers scan of underlying file
        values.clear();
        values.put(MediaColumns.IS_PENDING, 0);
        mResolver.update(uri, values, null, null);
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            c.moveToFirst();
            assertEquals(107, c.getLong(c.getColumnIndexOrThrow(MediaColumns.HEIGHT)));
        }

        // Ignored now that we're published
        values.clear();
        values.put(MediaColumns.HEIGHT, 48);
        mResolver.update(uri, values, null, null);
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            c.moveToFirst();
            assertEquals(107, c.getLong(c.getColumnIndexOrThrow(MediaColumns.HEIGHT)));
        }
    }

    /**
     * Verify that if clever apps try writing the exact modified time and size
     * as part of publishing that we still perform a full scan.
     */
    @Test
    public void testMatchingColumns() throws Exception {
        // Stage pending content
        final ContentValues values = new ContentValues();
        values.put(MediaColumns.MIME_TYPE, "image/png");
        values.put(MediaColumns.IS_PENDING, 1);
        values.put(MediaColumns.HEIGHT, 32);
        final Uri uri = mResolver.insert(mExternalImages, values);
        try (InputStream in = mContext.getResources().openRawResource(R.raw.scenery);
                OutputStream out = mResolver.openOutputStream(uri)) {
            FileUtils.copy(in, out);
        }

        // Fill in modified time and size based on values on disk
        values.clear();
        values.put(MediaColumns.IS_PENDING, 0);
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            assertTrue(c.moveToFirst());
            final File file = new File(c.getString(c.getColumnIndexOrThrow(MediaColumns.DATA)));
            values.put(MediaColumns.DATE_MODIFIED, file.lastModified() / 1000);
            values.put(MediaColumns.SIZE, file.length());
        }
        mResolver.applyBatch(MediaStore.AUTHORITY, new ArrayList<>(
                Arrays.asList(ContentProviderOperation.newUpdate(uri).withValues(values).build())));
        try (Cursor c = mResolver.query(uri, null, null, null)) {
            assertTrue(c.moveToFirst());
            assertEquals(0, c.getInt(c.getColumnIndexOrThrow(MediaColumns.IS_PENDING)));
            assertEquals(107, c.getLong(c.getColumnIndexOrThrow(MediaColumns.HEIGHT)));
        }
    }

    private void assertCreatePending(PendingParams params) {
        MediaStoreUtils.createPending(mContext, params);
    }

    private void assertNotCreatePending(PendingParams params) {
        assertNotCreatePending(null, params);
    }

    private void assertNotCreatePending(String message, PendingParams params) {
        try {
            MediaStoreUtils.createPending(mContext, params);
            fail(message);
        } catch (Exception expected) {
        }
    }

    private Uri execPending(PendingParams params, int resId) throws Exception {
        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(resId);
                    OutputStream out = session.openOutputStream()) {
                FileUtils.copy(in, out);
            }
            return session.publish();
        }
    }
}
