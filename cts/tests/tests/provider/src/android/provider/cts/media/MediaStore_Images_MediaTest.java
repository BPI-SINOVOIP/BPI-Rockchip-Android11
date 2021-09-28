/*
 * Copyright (C) 2009 The Android Open Source Project
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

import static android.provider.cts.media.MediaStoreTest.TAG;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.ExifInterface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;
import android.os.storage.StorageManager;
import android.provider.BaseColumns;
import android.provider.MediaStore;
import android.provider.MediaStore.Images.ImageColumns;
import android.provider.MediaStore.Images.Media;
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.HashSet;

@RunWith(Parameterized.class)
public class MediaStore_Images_MediaTest {
    private static final String MIME_TYPE_JPEG = "image/jpeg";

    private Context mContext;
    private ContentResolver mContentResolver;

    private Uri mExternalImages;

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
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);
    }

    @Test
    public void testInsertImageWithImagePath() throws Exception {
        // TODO: expand test to verify paths from secondary storage devices
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;

        final long unique1 = System.nanoTime();
        final String TEST_TITLE1 = "Title " + unique1;

        final long unique2 = System.nanoTime();
        final String TEST_TITLE2 = "Title " + unique2;

        File file = new File(ProviderTestUtils.stageDir(MediaStore.VOLUME_EXTERNAL),
                "mediaStoreTest1.jpg");
        String path = file.getAbsolutePath();
        ProviderTestUtils.stageFile(R.raw.scenery, file);

        file = new File(ProviderTestUtils.stageDir(MediaStore.VOLUME_EXTERNAL),
                "mediaStoreTest2.jpg");
        path = file.getAbsolutePath();
        ProviderTestUtils.stageFile(R.raw.scenery, file);

        Cursor c = Media.query(mContentResolver, mExternalImages, null, null,
                "_id ASC");
        int previousCount = c.getCount();
        c.close();

        // insert an image by path
        String stringUrl = null;
        try {
            stringUrl = Media.insertImage(mContentResolver, path, TEST_TITLE1, null);
        } catch (FileNotFoundException e) {
            fail(e.getMessage());
        } catch (UnsupportedOperationException e) {
            // the tests will be aborted because the image will be put in sdcard
            fail("There is no sdcard attached! " + e.getMessage());
        }
        assertInsertionSuccess(stringUrl);

        // insert another image by path
        stringUrl = null;
        try {
            stringUrl = Media.insertImage(mContentResolver, path, TEST_TITLE2, null);
        } catch (FileNotFoundException e) {
            fail(e.getMessage());
        } catch (UnsupportedOperationException e) {
            // the tests will be aborted because the image will be put in sdcard
            fail("There is no sdcard attached! " + e.getMessage());
        }
        assertInsertionSuccess(stringUrl);

        // query the newly added image
        c = Media.query(mContentResolver, Uri.parse(stringUrl),
                new String[] { Media.TITLE, Media.DESCRIPTION, Media.MIME_TYPE });
        assertEquals(1, c.getCount());
        c.moveToFirst();
        assertEquals(TEST_TITLE2, c.getString(c.getColumnIndex(Media.TITLE)));
        assertEquals(MIME_TYPE_JPEG, c.getString(c.getColumnIndex(Media.MIME_TYPE)));
        c.close();

        // query all the images in external db and order them by descending id
        // (make the images added in test case in the first positions)
        c = Media.query(mContentResolver, mExternalImages,
                new String[] { Media.TITLE, Media.DESCRIPTION, Media.MIME_TYPE }, null,
                "_id DESC");
        assertEquals(previousCount + 2, c.getCount());
        c.moveToFirst();
        assertEquals(TEST_TITLE2, c.getString(c.getColumnIndex(Media.TITLE)));
        assertEquals(MIME_TYPE_JPEG, c.getString(c.getColumnIndex(Media.MIME_TYPE)));
        c.moveToNext();
        assertEquals(TEST_TITLE1, c.getString(c.getColumnIndex(Media.TITLE)));
        assertEquals(MIME_TYPE_JPEG, c.getString(c.getColumnIndex(Media.MIME_TYPE)));
        c.close();

        // query the second image added in the test
        c = Media.query(mContentResolver, Uri.parse(stringUrl),
                new String[] { Media.DESCRIPTION, Media.MIME_TYPE }, Media.TITLE + "=?",
                new String[] { TEST_TITLE2 }, "_id ASC");
        assertEquals(1, c.getCount());
        c.moveToFirst();
        assertEquals(MIME_TYPE_JPEG, c.getString(c.getColumnIndex(Media.MIME_TYPE)));
        c.close();
    }

    @Test
    public void testInsertImageWithBitmap() throws Exception {
        final long unique3 = System.nanoTime();
        final String TEST_TITLE3 = "Title " + unique3;
        final String TEST_DESCRIPTION3 = "Description " + unique3;

        // insert the image by bitmap
        Bitmap src = BitmapFactory.decodeResource(mContext.getResources(), R.raw.scenery);
        String stringUrl = null;
        try{
            stringUrl = Media.insertImage(mContentResolver, src, TEST_TITLE3, TEST_DESCRIPTION3);
        } catch (UnsupportedOperationException e) {
            // the tests will be aborted because the image will be put in sdcard
            fail("There is no sdcard attached! " + e.getMessage());
        }
        assertInsertionSuccess(stringUrl);

        Cursor c = Media.query(mContentResolver, Uri.parse(stringUrl), new String[] { Media.DATA },
                null, "_id ASC");
        c.moveToFirst();
        // get the bimap by the path
        Bitmap result = Media.getBitmap(mContentResolver,
                    Uri.fromFile(new File(c.getString(c.getColumnIndex(Media.DATA)))));

        // can not check the identity between the result and source bitmap because
        // source bitmap is compressed before it is saved as result bitmap
        assertEquals(src.getWidth(), result.getWidth());
        assertEquals(src.getHeight(), result.getHeight());
    }

    @Test
    public void testGetContentUri() {
        Cursor c = null;
        assertNotNull(c = mContentResolver.query(Media.getContentUri("internal"), null, null, null,
                null));
        c.close();
        assertNotNull(c = mContentResolver.query(Media.getContentUri(mVolumeName), null, null, null,
                null));
        c.close();

        assertEquals(ContentUris.withAppendedId(Media.getContentUri(mVolumeName), 42),
                Media.getContentUri(mVolumeName, 42));
    }

    private void cleanExternalMediaFile(String path) {
        mContentResolver.delete(mExternalImages, "_data=?", new String[] { path });
        new File(path).delete();
    }

    @Test
    public void testStoreImagesMediaExternal() throws Exception {
        final File dir = ProviderTestUtils.stageDir(mVolumeName);
        final File file = ProviderTestUtils.stageFile(R.raw.scenery,
                new File(dir, "cts" + System.nanoTime() + ".jpg"));

        final String externalPath = file.getAbsolutePath();
        final long numBytes = file.length();

        ProviderTestUtils.waitUntilExists(file);

        ContentValues values = new ContentValues();
        values.put(Media.ORIENTATION, 0);
        values.put(Media.PICASA_ID, 0);
        long dateTaken = System.currentTimeMillis();
        values.put(Media.DATE_TAKEN, dateTaken);
        values.put(Media.DESCRIPTION, "This is a image");
        values.put(Media.IS_PRIVATE, 1);
        values.put(Media.MINI_THUMB_MAGIC, 0);
        values.put(Media.DATA, externalPath);
        values.put(Media.DISPLAY_NAME, file.getName());
        values.put(Media.MIME_TYPE, "image/jpeg");
        values.put(Media.SIZE, numBytes);
        values.put(Media.TITLE, "testimage");
        long dateAdded = System.currentTimeMillis() / 1000;
        values.put(Media.DATE_ADDED, dateAdded);
        long dateModified = System.currentTimeMillis() / 1000;
        values.put(Media.DATE_MODIFIED, dateModified);

        // insert
        Uri uri = mContentResolver.insert(mExternalImages, values);
        assertNotNull(uri);

        try {
            // query
            Cursor c = mContentResolver.query(uri, null, null, null, null);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            long id = c.getLong(c.getColumnIndex(Media._ID));
            assertTrue(id > 0);
            assertEquals(0, c.getInt(c.getColumnIndex(Media.ORIENTATION)));
            assertEquals(0, c.getLong(c.getColumnIndex(Media.PICASA_ID)));
            assertEquals(dateTaken, c.getLong(c.getColumnIndex(Media.DATE_TAKEN)));
            assertEquals("This is a image",
                    c.getString(c.getColumnIndex(Media.DESCRIPTION)));
            assertEquals(1, c.getInt(c.getColumnIndex(Media.IS_PRIVATE)));
            assertEquals(0, c.getLong(c.getColumnIndex(Media.MINI_THUMB_MAGIC)));
            assertEquals(externalPath, c.getString(c.getColumnIndex(Media.DATA)));
            assertEquals(file.getName(), c.getString(c.getColumnIndex(Media.DISPLAY_NAME)));
            assertEquals("image/jpeg", c.getString(c.getColumnIndex(Media.MIME_TYPE)));
            assertEquals("testimage", c.getString(c.getColumnIndex(Media.TITLE)));
            assertEquals(numBytes, c.getInt(c.getColumnIndex(Media.SIZE)));
            long realDateAdded = c.getLong(c.getColumnIndex(Media.DATE_ADDED));
            assertTrue(realDateAdded >= dateAdded);
            // there can be delay as time is read after creation
            assertTrue(Math.abs(dateModified - c.getLong(c.getColumnIndex(Media.DATE_MODIFIED)))
                       < 5);
            c.close();
        } finally {
            // delete
            assertEquals(1, mContentResolver.delete(uri, null, null));
            file.delete();
        }
    }

    /**
     * b/155320967 Test that update with conflict is resolved as replace.
     */
    @Test
    public void testUpdateAndReplace() throws Exception {
        File dir = mContext.getSystemService(StorageManager.class)
                .getStorageVolume(mExternalImages).getDirectory();
        File dcimDir = new File(dir, Environment.DIRECTORY_DCIM);
        File file = null;
        try {
            // Create file
            file = copyResourceToFile(R.raw.scenery, dcimDir, "cts" + System.nanoTime() + ".jpg");
            final String externalPath = file.getAbsolutePath();
            assertNotNull(MediaStore.scanFile(mContentResolver, file));

            // Insert another file, insertedFile doesn't exist in lower file system.
            ContentValues values = new ContentValues();
            final File insertedFile = new File(dcimDir, "cts" + System.nanoTime() + ".jpg");
            values.put(Media.DATA, insertedFile.getAbsolutePath());

            final Uri uri = mContentResolver.insert(mExternalImages, values);
            assertNotNull(uri);

            // Now update the second file to the same file path as the first file.
            values.put(Media.DATA, externalPath);
            // This update is implemented as update and replace on conflict and shouldn't throw
            // UNIQUE constraint error.
            assertEquals(1, mContentResolver.update(uri, values, Bundle.EMPTY));
            Uri scannedUri = MediaStore.scanFile(mContentResolver, file);
            assertNotNull(scannedUri);

            // _id in inserted uri and scannedUri should be same.
            assertEquals(uri.getLastPathSegment(), scannedUri.getLastPathSegment());
        } finally {
            if (file != null) {
                file.delete();
            }
        }
    }

    @Test
    public void testUpsert() throws Exception {
        File dir = mContext.getSystemService(StorageManager.class)
                .getStorageVolume(mExternalImages).getDirectory();
        File dcimDir = new File(dir, Environment.DIRECTORY_DCIM);
        File file = null;
        try {
            // Create file
            file = copyResourceToFile(R.raw.scenery, dcimDir, "cts" + System.nanoTime() + ".jpg");
            final String externalPath = file.getAbsolutePath();

            // Verify a record exists in MediaProvider.
            final Uri scannedUri = MediaStore.scanFile(mContentResolver, file);
            assertNotNull(scannedUri);

            // Now insert via ContentResolver and verify it works.
            ContentValues values = new ContentValues();
            values.put(Media.DATA, externalPath);

            // This insert is really an upsert. It should work.
            final Uri uri = mContentResolver.insert(mExternalImages, values);
            assertNotNull(uri);
            // insert was an upsert, _id in uri and scannedUri should be same.
            assertEquals(uri.getLastPathSegment(), scannedUri.getLastPathSegment());
        } finally {
            if (file != null) {
                file.delete();
            }
        }
    }

    private File copyResourceToFile(int sourceResId, File destinationDir,
            String destinationFileName) throws Exception {
        final File file = new File(destinationDir, destinationFileName);
        file.createNewFile();

        try (InputStream source = InstrumentationRegistry.getTargetContext().getResources()
                .openRawResource(sourceResId);
             OutputStream target = new FileOutputStream(file)) {
            FileUtils.copy(source, target);
        }
        return file;
    }

    private void assertInsertionSuccess(String stringUrl) throws IOException {
        final Uri uri = Uri.parse(stringUrl);

        // check whether the thumbnails are generated
        try (Cursor c = mContentResolver.query(uri, null, null, null)) {
            assertEquals(1, c.getCount());
        }

        assertNotNull(mContentResolver.loadThumbnail(uri, new Size(512, 384), null));
        assertNotNull(mContentResolver.loadThumbnail(uri, new Size(96, 96), null));
    }

    @Test
    public void testLocationRedaction() throws Exception {
        // STOPSHIP: remove this once isolated storage is always enabled
        Assume.assumeTrue(StorageManager.hasIsolatedStorage());
        final Uri publishUri = ProviderTestUtils.stageMedia(R.raw.lg_g4_iso_800_jpg, mExternalImages,
                "image/jpeg");
        final Uri originalUri = MediaStore.setRequireOriginal(publishUri);

        // Since we own the image, we should be able to see the Exif data that
        // we ourselves contributed
        try (InputStream is = mContentResolver.openInputStream(publishUri)) {
            final ExifInterface exif = new ExifInterface(is);
            final float[] latLong = new float[2];
            exif.getLatLong(latLong);
            assertEquals(53.83451, latLong[0], 0.001);
            assertEquals(10.69585, latLong[1], 0.001);

            String xmp = exif.getAttribute(ExifInterface.TAG_XMP);
            assertTrue("Failed to read XMP longitude", xmp.contains("53,50.070500N"));
            assertTrue("Failed to read XMP latitude", xmp.contains("10,41.751000E"));
            assertTrue("Failed to read non-location XMP", xmp.contains("LensDefaults"));
        }
        // As owner, we should be able to request the original bytes
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(originalUri, "r")) {
        }

        // Revoke location access and remove ownership, which means that location should be redacted
        ProviderTestUtils.revokeMediaLocationPermission(mContext);
        ProviderTestUtils.clearOwner(publishUri);
        try (InputStream is = mContentResolver.openInputStream(publishUri)) {
            final ExifInterface exif = new ExifInterface(is);
            final float[] latLong = new float[2];
            exif.getLatLong(latLong);
            assertEquals(0, latLong[0], 0.001);
            assertEquals(0, latLong[1], 0.001);

            String xmp = exif.getAttribute(ExifInterface.TAG_XMP);
            assertFalse("Failed to redact XMP longitude", xmp.contains("53,50.070500N"));
            assertFalse("Failed to redact XMP latitude", xmp.contains("10,41.751000E"));
            assertTrue("Redacted non-location XMP", xmp.contains("LensDefaults"));
        }
        // We can't request original bytes unless we have permission
        try (ParcelFileDescriptor pfd = mContentResolver.openFileDescriptor(originalUri, "r")) {
            fail("Able to read original content without ACCESS_MEDIA_LOCATION");
        } catch (UnsupportedOperationException expected) {
        }
    }

    @Test
    public void testLocationDeprecated() throws Exception {
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                mExternalImages, displayName, "image/jpeg");

        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        final Uri publishUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (InputStream in = mContext.getResources().openRawResource(R.raw.volantis);
                    OutputStream out = session.openOutputStream()) {
                android.os.FileUtils.copy(in, out);
            }
            publishUri = session.publish();
        }

        // Verify that location wasn't indexed
        try (Cursor c = mContentResolver.query(publishUri,
                new String[] { ImageColumns.LATITUDE, ImageColumns.LONGITUDE }, null, null)) {
            assertTrue(c.moveToFirst());
            assertTrue(c.isNull(0));
            assertTrue(c.isNull(1));
        }

        // Verify that location values aren't recorded
        final ContentValues values = new ContentValues();
        values.put(ImageColumns.LATITUDE, 32f);
        values.put(ImageColumns.LONGITUDE, 64f);
        mContentResolver.update(publishUri, values, null, null);

        try (Cursor c = mContentResolver.query(publishUri,
                new String[] { ImageColumns.LATITUDE, ImageColumns.LONGITUDE }, null, null)) {
            assertTrue(c.moveToFirst());
            assertTrue(c.isNull(0));
            assertTrue(c.isNull(1));
        }
    }

    @Test
    public void testCanonicalize() throws Exception {
        // Remove all audio left over from other tests
        ProviderTestUtils.executeShellCommand("content delete"
                + " --user " + InstrumentationRegistry.getTargetContext().getUserId()
                + " --uri " + mExternalImages,
                InstrumentationRegistry.getInstrumentation().getUiAutomation());

        // Publish some content
        final File dir = ProviderTestUtils.stageDir(mVolumeName);
        final Uri a = ProviderTestUtils.scanFileFromShell(
                ProviderTestUtils.stageFile(R.raw.scenery, new File(dir, "a.jpg")));
        final Uri b = ProviderTestUtils.scanFileFromShell(
                ProviderTestUtils.stageFile(R.raw.lg_g4_iso_800_jpg, new File(dir, "b.jpg")));
        final Uri c = ProviderTestUtils.scanFileFromShell(
                ProviderTestUtils.stageFile(R.raw.scenery, new File(dir, "c.jpg")));

        // Confirm we can canonicalize and recover it
        final Uri canonicalized = mContentResolver.canonicalize(b);
        assertNotNull(canonicalized);
        assertEquals(b, mContentResolver.uncanonicalize(canonicalized));

        // Delete all items above
        mContentResolver.delete(a, null, null);
        mContentResolver.delete(b, null, null);
        mContentResolver.delete(c, null, null);

        // Confirm canonical item isn't found
        assertNull(mContentResolver.uncanonicalize(canonicalized));

        // Publish data again and confirm we can recover it
        final Uri d = ProviderTestUtils.scanFileFromShell(
                ProviderTestUtils.stageFile(R.raw.lg_g4_iso_800_jpg, new File(dir, "d.jpg")));
        assertEquals(d, mContentResolver.uncanonicalize(canonicalized));
    }

    @Test
    public void testMetadata() throws Exception {
        final Uri uri = ProviderTestUtils.stageMedia(R.raw.lg_g4_iso_800_jpg, mExternalImages,
                "image/jpeg");

        try (Cursor c = mContentResolver.query(uri, null, null, null)) {
            assertTrue(c.moveToFirst());

            // Confirm that we parsed Exif metadata
            assertEquals(0, c.getLong(c.getColumnIndex(ImageColumns.ORIENTATION)));
            assertEquals(600, c.getLong(c.getColumnIndex(ImageColumns.WIDTH)));
            assertEquals(337, c.getLong(c.getColumnIndex(ImageColumns.HEIGHT)));

            // Confirm that we parsed XMP metadata
            assertEquals("xmp.did:041dfd42-0b46-4302-918a-836fba5016ed",
                    c.getString(c.getColumnIndex(ImageColumns.DOCUMENT_ID)));
            assertEquals("xmp.iid:041dfd42-0b46-4302-918a-836fba5016ed",
                    c.getString(c.getColumnIndex(ImageColumns.INSTANCE_ID)));
            assertEquals("3F9DD7A46B26513A7C35272F0D623A06",
                    c.getString(c.getColumnIndex(ImageColumns.ORIGINAL_DOCUMENT_ID)));
            assertTrue(new String(c.getBlob(c.getColumnIndex(ImageColumns.XMP)))
                    .contains("exif:ShutterSpeedValue"));

            // Confirm that we redacted location metadata
            assertFalse(new String(c.getBlob(c.getColumnIndex(ImageColumns.XMP)))
                    .contains("exif:GPSLatitude"));
            assertFalse(new String(c.getBlob(c.getColumnIndex(ImageColumns.XMP)))
                    .contains("exif:GPSLongitude"));

            // Confirm that timestamp was parsed with offset information
            assertEquals(1447346778000L + 25200000L,
                    c.getLong(c.getColumnIndex(ImageColumns.DATE_TAKEN)));

            // We just added and modified the file, so should be recent
            final long added = c.getLong(c.getColumnIndex(ImageColumns.DATE_ADDED));
            final long modified = c.getLong(c.getColumnIndex(ImageColumns.DATE_MODIFIED));
            final long now = System.currentTimeMillis() / 1000;
            assertTrue("Invalid added time " + added, Math.abs(added - now) < 5);
            assertTrue("Invalid modified time " + modified, Math.abs(modified - now) < 5);

            // Confirm that we trusted value from XMP metadata
            assertEquals("image/dng", c.getString(c.getColumnIndex(ImageColumns.MIME_TYPE)));

            assertEquals(107704, c.getLong(c.getColumnIndex(ImageColumns.SIZE)));

            final String displayName = c.getString(c.getColumnIndex(ImageColumns.DISPLAY_NAME));
            assertTrue("Invalid display name " + displayName, displayName.startsWith("cts"));
            assertTrue("Invalid display name " + displayName, displayName.endsWith(".jpg"));
        }
    }

    @Test
    public void testGroup() throws Exception {
        // Confirm that we have at least two images staged
        ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);
        ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);

        final Bundle queryArgs = new Bundle();
        queryArgs.putStringArray(ContentResolver.QUERY_ARG_GROUP_COLUMNS,
                new String[] { ImageColumns.BUCKET_ID });

        final HashSet<Integer> seen = new HashSet<>();
        int maxCount = 0;
        try (Cursor c = mContentResolver.query(mExternalImages,
                new String[] { ImageColumns.BUCKET_ID, "COUNT(_id)" }, queryArgs, null)) {
            final HashSet<String> honored = new HashSet<>(Arrays
                    .asList(c.getExtras().getStringArray(ContentResolver.EXTRA_HONORED_ARGS)));
            assertTrue(honored.contains(ContentResolver.QUERY_ARG_GROUP_COLUMNS));

            while (c.moveToNext()) {
                final int id = c.getInt(0);
                final int count = c.getInt(1);

                // We should never see the same BUCKET_ID twice
                assertFalse(seen.contains(id));
                seen.add(id);

                maxCount = Math.max(maxCount, count);
            }
        }

        // At least one bucket should have more than one item
        assertTrue(maxCount > 1);
    }

    @Test
    public void testLimit() throws Exception {
        // Confirm that we have at least two images staged
        final Uri red = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);
        final Uri blue = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);

        final long redId = ContentUris.parseId(red);
        final long blueId = ContentUris.parseId(blue);

        final Bundle queryArgs = new Bundle();
        queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION,
                BaseColumns._ID + " IN (" + redId + "," + blueId + ")");
        queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SORT_ORDER,
                BaseColumns._ID + " ASC");
        queryArgs.putInt(ContentResolver.QUERY_ARG_LIMIT, 1);

        try (Cursor c = mContentResolver.query(mExternalImages,
                new String[] { BaseColumns._ID }, queryArgs, null)) {
            final HashSet<String> honored = new HashSet<>(Arrays
                    .asList(c.getExtras().getStringArray(ContentResolver.EXTRA_HONORED_ARGS)));
            assertTrue(honored.contains(ContentResolver.QUERY_ARG_LIMIT));

            // We should only have single lowest image
            assertEquals(1, c.getCount());
            assertTrue(c.moveToFirst());
            assertEquals(Math.min(redId, blueId), c.getLong(0));
        }

        queryArgs.putInt(ContentResolver.QUERY_ARG_OFFSET, 1);

        try (Cursor c = mContentResolver.query(mExternalImages,
                new String[] { BaseColumns._ID }, queryArgs, null)) {
            final HashSet<String> honored = new HashSet<>(Arrays
                    .asList(c.getExtras().getStringArray(ContentResolver.EXTRA_HONORED_ARGS)));
            assertTrue(honored.contains(ContentResolver.QUERY_ARG_LIMIT));
            assertTrue(honored.contains(ContentResolver.QUERY_ARG_OFFSET));

            // We should only have single highest image
            assertEquals(1, c.getCount());
            assertTrue(c.moveToFirst());
            assertEquals(Math.max(redId, blueId), c.getLong(0));
        }
    }
}
