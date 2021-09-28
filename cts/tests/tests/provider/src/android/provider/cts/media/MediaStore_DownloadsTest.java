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

import static android.provider.cts.ProviderTestUtils.hash;
import static android.provider.cts.ProviderTestUtils.resolveVolumeName;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.FileUtils;
import android.provider.MediaStore;
import android.provider.MediaStore.Downloads;
import android.provider.MediaStore.Files;
import android.provider.MediaStore.Images;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.provider.cts.media.MediaStoreUtils.PendingParams;
import android.provider.cts.media.MediaStoreUtils.PendingSession;
import android.util.Log;
import android.util.Size;

import androidx.test.InstrumentationRegistry;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(Parameterized.class)
public class MediaStore_DownloadsTest {
    private static final String TAG = MediaStore_DownloadsTest.class.getSimpleName();
    private static final long NOTIFY_TIMEOUT_MILLIS = 4000;

    private Context mContext;
    private ContentResolver mContentResolver;
    private File mDownloadsDir;
    private File mPicturesDir;
    private CountDownLatch mCountDownLatch;
    private int mInitialDownloadsCount;

    private Uri mExternalFiles;
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
        mContentResolver = mContext.getContentResolver();

        Log.d(TAG, "Using volume " + mVolumeName);
        mExternalFiles = MediaStore.Files.getContentUri(mVolumeName);
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);
        mExternalDownloads = MediaStore.Downloads.getContentUri(mVolumeName);

        mDownloadsDir = new File(ProviderTestUtils.getVolumePath(resolveVolumeName(mVolumeName)),
                Environment.DIRECTORY_DOWNLOADS);
        mPicturesDir = new File(ProviderTestUtils.getVolumePath(resolveVolumeName(mVolumeName)),
                Environment.DIRECTORY_PICTURES);
        mDownloadsDir.mkdirs();
        mPicturesDir.mkdirs();
        mInitialDownloadsCount = getInitialDownloadsCount();
    }

    @Test
    public void testScannedDownload() throws Exception {
        Assume.assumeTrue(MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)
                || MediaStore.VOLUME_EXTERNAL_PRIMARY.equals(mVolumeName));

        final File downloadFile = new File(mDownloadsDir, "colors.txt");
        downloadFile.createNewFile();
        final String fileContents = "RED;GREEN;BLUE";
        try (final PrintWriter pw = new PrintWriter(downloadFile)) {
            pw.print(fileContents);
        }
        verifyScannedDownload(downloadFile);
    }

    @Test
    public void testScannedMediaDownload() throws Exception {
        Assume.assumeTrue(MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)
                || MediaStore.VOLUME_EXTERNAL_PRIMARY.equals(mVolumeName));

        final File downloadFile = new File(mDownloadsDir, "scenery.png");
        downloadFile.createNewFile();
        try (InputStream in = mContext.getResources().openRawResource(R.raw.scenery);
                OutputStream out = new FileOutputStream(downloadFile)) {
            FileUtils.copy(in, out);
        }
        verifyScannedDownload(downloadFile);
    }

    @Test
    public void testGetContentUri() throws Exception {
        Cursor c;
        assertNotNull(c = mContentResolver.query(mExternalDownloads,
                null, null, null, null));
        c.close();

        assertEquals(ContentUris.withAppendedId(Downloads.getContentUri(mVolumeName), 42),
                Downloads.getContentUri(mVolumeName, 42));
    }

    @Test
    public void testMediaInDownloadsDir() throws Exception {
        Assume.assumeTrue(MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)
                || MediaStore.VOLUME_EXTERNAL_PRIMARY.equals(mVolumeName));

        final String displayName = "cts" + System.nanoTime();
        final Uri insertUri = insertImage(displayName, "test image",
                new File(mDownloadsDir, displayName + ".jpg"), "image/jpeg", R.raw.scenery);
        final String displayName2 = "cts" + System.nanoTime();
        final Uri insertUri2 = insertImage(displayName2, "test image2",
                new File(mPicturesDir, displayName2 + ".jpg"), "image/jpeg", R.raw.volantis);

        try (Cursor cursor = mContentResolver.query(mExternalDownloads,
                null, "title LIKE ?1", new String[] { displayName }, null)) {
            assertEquals(1, cursor.getCount());
            cursor.moveToNext();
            assertEquals("image/jpeg",
                    cursor.getString(cursor.getColumnIndex(Images.Media.MIME_TYPE)));
        }

        assertEquals(1, mContentResolver.delete(insertUri, null, null));
        try (Cursor cursor = mContentResolver.query(mExternalDownloads,
                null, null, null, null)) {
            assertEquals(mInitialDownloadsCount, cursor.getCount());
        }
    }

    @Test
    public void testInsertDownload() throws Exception {
        final String content = "<html><body>Content</body></html>";
        final String displayName = "cts" + System.nanoTime();
        final String mimeType = "text/html";
        final Uri downloadUri = Uri.parse("https://developer.android.com/overview.html");
        final Uri refererUri = Uri.parse("https://www.android.com");

        final PendingParams params = new PendingParams(
                mExternalDownloads, displayName, mimeType);
        params.setDownloadUri(downloadUri);
        params.setRefererUri(refererUri);

        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        assertNotNull(pendingUri);
        final Uri publishUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (PrintWriter pw = new PrintWriter(session.openOutputStream())) {
                pw.print(content);
            }
            try (OutputStream out = session.openOutputStream()) {
                out.write(content.getBytes(StandardCharsets.UTF_8));
            }
            publishUri = session.publish();
        }

        try (Cursor cursor = mContentResolver.query(publishUri, null, null, null, null)) {
            assertEquals(1, cursor.getCount());

            cursor.moveToNext();
            assertEquals(mimeType,
                    cursor.getString(cursor.getColumnIndex(Downloads.MIME_TYPE)));
            assertEquals(displayName + ".html",
                    cursor.getString(cursor.getColumnIndex(Downloads.DISPLAY_NAME)));
            assertEquals(downloadUri.toString(),
                    cursor.getString(cursor.getColumnIndex(Downloads.DOWNLOAD_URI)));
            assertEquals(refererUri.toString(),
                    cursor.getString(cursor.getColumnIndex(Downloads.REFERER_URI)));
        }

        final ByteArrayOutputStream actual = new ByteArrayOutputStream();
        try (InputStream in = mContentResolver.openInputStream(publishUri)) {
            final byte[] buf = new byte[512];
            int bytesRead;
            while ((bytesRead = in.read(buf)) != -1) {
                actual.write(buf, 0, bytesRead);
            }
        }
        assertEquals(content, actual.toString(StandardCharsets.UTF_8.name()));
    }

    @Test
    public void testUpdateDownload() throws Exception {
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                mExternalDownloads, displayName, "video/3gpp");
        final Uri downloadUri = Uri.parse("https://www.android.com/download?file=testvideo.3gp");
        params.setDownloadUri(downloadUri);

        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        assertNotNull(pendingUri);
        final Uri publishUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(R.raw.testvideo);
                 OutputStream out = session.openOutputStream()) {
                android.os.FileUtils.copy(in, out);
            }
            publishUri = session.publish();
        }

        final ContentValues updateValues = new ContentValues();
        updateValues.put(MediaStore.Files.FileColumns.MEDIA_TYPE,
                MediaStore.Files.FileColumns.MEDIA_TYPE_AUDIO);
        updateValues.put(Downloads.MIME_TYPE, "audio/3gpp");
        assertEquals(1, mContentResolver.update(publishUri, updateValues, null, null));

        try (Cursor cursor = mContentResolver.query(publishUri,
                null, null, null, null)) {
            assertEquals(1, cursor.getCount());
            cursor.moveToNext();
            assertEquals("audio/3gpp",
                    cursor.getString(cursor.getColumnIndex(Downloads.MIME_TYPE)));
            assertEquals(downloadUri.toString(),
                    cursor.getString(cursor.getColumnIndex(Downloads.DOWNLOAD_URI)));
        }
    }

    @Test
    public void testDeleteDownload() throws Exception {
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                mExternalDownloads, displayName, "video/3gp");
        final Uri downloadUri = Uri.parse("https://www.android.com/download?file=testvideo.3gp");
        params.setDownloadUri(downloadUri);

        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        assertNotNull(pendingUri);
        final Uri publishUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(R.raw.testvideo);
                 OutputStream out = session.openOutputStream()) {
                android.os.FileUtils.copy(in, out);
            }
            publishUri = session.publish();
        }

        assertEquals(1, mContentResolver.delete(publishUri, null, null));
        try (Cursor cursor = mContentResolver.query(mExternalDownloads,
                null, null, null, null)) {
            assertEquals(mInitialDownloadsCount, cursor.getCount());
        }
    }

    @Test
    public void testNotifyChange() throws Exception {
        final ContentObserver observer = new ContentObserver(null) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                super.onChange(selfChange, uri);
                mCountDownLatch.countDown();
            }
        };
        mContentResolver.registerContentObserver(mExternalDownloads, true, observer);
        mContentResolver.registerContentObserver(MediaStore.AUTHORITY_URI, false, observer);
        final Uri volumeUri = MediaStore.AUTHORITY_URI.buildUpon()
                .appendPath(mVolumeName)
                .build();
        mContentResolver.registerContentObserver(volumeUri, false, observer);

        mCountDownLatch = new CountDownLatch(1);
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                mExternalDownloads, displayName, "video/3gp");
        final Uri downloadUri = Uri.parse("https://www.android.com/download?file=testvideo.3gp");
        params.setDownloadUri(downloadUri);
        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        assertNotNull(pendingUri);
        final Uri publishUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(R.raw.testvideo);
                 OutputStream out = session.openOutputStream()) {
                android.os.FileUtils.copy(in, out);
            }
            publishUri = session.publish();
        }
        mCountDownLatch.await(NOTIFY_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);

        mCountDownLatch = new CountDownLatch(1);
        final ContentValues updateValues = new ContentValues();
        updateValues.put(Files.FileColumns.MEDIA_TYPE, Files.FileColumns.MEDIA_TYPE_AUDIO);
        updateValues.put(Downloads.MIME_TYPE, "audio/3gp");
        assertEquals(1, mContentResolver.update(publishUri, updateValues, null, null));
        mCountDownLatch.await(NOTIFY_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);

        mCountDownLatch = new CountDownLatch(1);
        assertEquals(1, mContentResolver.delete(publishUri, null, null));
        mCountDownLatch.await(NOTIFY_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
    }

    @Test
    public void testThumbnails() throws Exception {
        final Uri uri = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalDownloads);
        final long id = ContentUris.parseId(uri);

        // Verify that we can get a thumbnail for this item regardless of which
        // collection we reference it through
        assertNotNull(mContentResolver.loadThumbnail(
                ContentUris.withAppendedId(mExternalFiles, id), new Size(320, 240), null));
        assertNotNull(mContentResolver.loadThumbnail(
                ContentUris.withAppendedId(mExternalImages, id), new Size(320, 240), null));
        assertNotNull(mContentResolver.loadThumbnail(
                ContentUris.withAppendedId(mExternalDownloads, id), new Size(320, 240), null));
    }

    private int getInitialDownloadsCount() {
        try (Cursor cursor = mContentResolver.query(mExternalDownloads,
                null, null, null, null)) {
            return cursor.getCount();
        }
    }

    private Uri insertImage(String displayName, String description,
            File file, String mimeType, int resourceId) throws Exception {
        file.createNewFile();
        try (InputStream in = mContext.getResources().openRawResource(resourceId);
             OutputStream out = new FileOutputStream(file)) {
            FileUtils.copy(in, out);
        }

        final ContentValues values = new ContentValues();
        values.put(Images.Media.DISPLAY_NAME, displayName);
        values.put(Images.Media.TITLE, displayName);
        values.put(Images.Media.DESCRIPTION, description);
        values.put(Images.Media.DATA, file.getAbsolutePath());
        values.put(Images.Media.DATE_ADDED, System.currentTimeMillis() / 1000);
        values.put(Images.Media.DATE_MODIFIED, System.currentTimeMillis() / 1000);
        values.put(Images.Media.MIME_TYPE, mimeType);

        final Uri insertUri = mContentResolver.insert(mExternalImages, values);
        assertNotNull(insertUri);
        return insertUri;
    }

    private void verifyScannedDownload(File file) throws Exception {
        final Uri mediaStoreUri = ProviderTestUtils.scanFile(file);
        Log.e(TAG, "Scanned file " + file.getAbsolutePath() + ": " + mediaStoreUri);
        assertArrayEquals("File hashes should match for " + file + " and " + mediaStoreUri,
                hash(new FileInputStream(file)),
                hash(mContentResolver.openInputStream(mediaStoreUri)));

        // Verify the file is part of downloads collection.
        final long id = ContentUris.parseId(mediaStoreUri);
        final Cursor cursor = mContentResolver.query(mExternalDownloads,
                null, MediaStore.Downloads._ID + "=" + id, null, null);
        assertEquals(1, cursor.getCount());
    }
}
