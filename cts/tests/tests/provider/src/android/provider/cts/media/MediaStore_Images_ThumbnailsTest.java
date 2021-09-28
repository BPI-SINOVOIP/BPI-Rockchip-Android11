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

import static android.provider.cts.ProviderTestUtils.assertColorMostlyEquals;
import static android.provider.cts.ProviderTestUtils.extractAverageColor;
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
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ImageDecoder;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.MediaStore.Images.Media;
import android.provider.MediaStore.Images.Thumbnails;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.provider.cts.media.MediaStoreUtils.PendingParams;
import android.provider.cts.media.MediaStoreUtils.PendingSession;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Size;

import androidx.test.InstrumentationRegistry;

import junit.framework.AssertionFailedError;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;

@RunWith(Parameterized.class)
public class MediaStore_Images_ThumbnailsTest {
    private ArrayList<Uri> mRowsAdded;

    private Context mContext;
    private ContentResolver mContentResolver;

    private Uri mExternalImages;

    @Parameter(0)
    public String mVolumeName;

    private int mLargestDimension;

    @Parameters
    public static Iterable<? extends Object> data() {
        return ProviderTestUtils.getSharedVolumeNames();
    }

    private Uri mRed;
    private Uri mBlue;

    @After
    public void tearDown() throws Exception {
        for (Uri row : mRowsAdded) {
            try {
                mContentResolver.delete(row, null, null);
            } catch (UnsupportedOperationException e) {
                // There is no way to delete rows from table "thumbnails" of internals database.
                // ignores the exception and make the loop goes on
            }
        }
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mContentResolver = mContext.getContentResolver();

        mRowsAdded = new ArrayList<Uri>();

        Log.d(TAG, "Using volume " + mVolumeName);
        mExternalImages = MediaStore.Images.Media.getContentUri(mVolumeName);

        final Resources res = mContext.getResources();
        final Configuration config = res.getConfiguration();
        mLargestDimension = (int) (Math.max(config.screenWidthDp, config.screenHeightDp)
                * res.getDisplayMetrics().density);
    }

    private void prepareImages() throws Exception {
        mRed = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);
        mBlue = ProviderTestUtils.stageMedia(R.raw.scenery, mExternalImages);
        mRowsAdded.add(mRed);
        mRowsAdded.add(mBlue);
        ProviderTestUtils.waitForIdle();
    }

    public static void assertMostlyEquals(long expected, long actual, long delta) {
        if (Math.abs(expected - actual) > delta) {
            throw new AssertionFailedError("Expected roughly " + expected + " but was " + actual);
        }
    }

    @Test
    public void testQueryExternalThumbnails() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;
        prepareImages();

        Cursor c = Thumbnails.queryMiniThumbnails(mContentResolver,
                Thumbnails.EXTERNAL_CONTENT_URI, Thumbnails.MICRO_KIND, null);
        int previousMicroKindCount = c.getCount();
        c.close();

        // add a thumbnail
        final File file = new File(ProviderTestUtils.stageDir(MediaStore.VOLUME_EXTERNAL),
                "testThumbnails.jpg");
        final String path = file.getAbsolutePath();
        ProviderTestUtils.stageFile(R.raw.scenery, file);
        ContentValues values = new ContentValues();
        values.put(Thumbnails.KIND, Thumbnails.MINI_KIND);
        values.put(Thumbnails.DATA, path);
        values.put(Thumbnails.IMAGE_ID, ContentUris.parseId(mRed));
        Uri uri = mContentResolver.insert(Thumbnails.EXTERNAL_CONTENT_URI, values);
        if (uri != null) {
            mRowsAdded.add(uri);
        }

        // query with the uri of the thumbnail and the kind
        c = Thumbnails.queryMiniThumbnails(mContentResolver, uri, Thumbnails.MINI_KIND, null);
        c.moveToFirst();
        assertEquals(1, c.getCount());
        assertEquals(Thumbnails.MINI_KIND, c.getInt(c.getColumnIndex(Thumbnails.KIND)));
        assertEquals(path, c.getString(c.getColumnIndex(Thumbnails.DATA)));

        // query all thumbnails with other kind
        c = Thumbnails.queryMiniThumbnails(mContentResolver, Thumbnails.EXTERNAL_CONTENT_URI,
                Thumbnails.MICRO_KIND, null);
        assertEquals(previousMicroKindCount, c.getCount());
        c.close();

        // query without kind
        c = Thumbnails.query(mContentResolver, uri, null);
        assertEquals(1, c.getCount());
        c.moveToFirst();
        assertEquals(Thumbnails.MINI_KIND, c.getInt(c.getColumnIndex(Thumbnails.KIND)));
        assertEquals(path, c.getString(c.getColumnIndex(Thumbnails.DATA)));
        c.close();
    }

    @Test
    public void testQueryExternalMiniThumbnails() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;
        final ContentResolver resolver = mContentResolver;

        // insert the image by bitmap
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inTargetDensity = DisplayMetrics.DENSITY_XHIGH;
        Bitmap src = BitmapFactory.decodeResource(mContext.getResources(), R.raw.scenery,opts);
        String stringUrl = null;
        try{
            stringUrl = Media.insertImage(mContentResolver, src, "cts" + System.nanoTime(), null);
        } catch (UnsupportedOperationException e) {
            // the tests will be aborted because the image will be put in sdcard
            fail("There is no sdcard attached! " + e.getMessage());
        }
        assertNotNull(stringUrl);
        Uri stringUri = Uri.parse(stringUrl);
        mRowsAdded.add(stringUri);

        // get the original image id and path
        Cursor c = mContentResolver.query(stringUri,
                new String[]{ Media._ID, Media.DATA }, null, null, null);
        c.moveToFirst();
        long imageId = c.getLong(c.getColumnIndex(Media._ID));
        String imagePath = c.getString(c.getColumnIndex(Media.DATA));
        c.close();

        ProviderTestUtils.waitForIdle();
        assertFileExists(imagePath);
        assertNotNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MINI_KIND, null));
        assertNotNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MICRO_KIND, null));

        // deleting the image from the database also deletes the image file, and the
        // corresponding entry in the thumbnail table, which in turn triggers deletion
        // of the thumbnail file on disk
        mContentResolver.delete(stringUri, null, null);
        mRowsAdded.remove(stringUri);

        ProviderTestUtils.waitForIdle();
        assertFileNotExists(imagePath);
        assertNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MINI_KIND, null));
        assertNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MICRO_KIND, null));

        // insert image, then delete it via the files table
        stringUrl = Media.insertImage(mContentResolver, src, "cts" + System.nanoTime(), null);
        c = mContentResolver.query(Uri.parse(stringUrl),
                new String[]{ Media._ID, Media.DATA}, null, null, null);
        c.moveToFirst();
        imageId = c.getLong(c.getColumnIndex(Media._ID));
        imagePath = c.getString(c.getColumnIndex(Media.DATA));
        c.close();
        assertFileExists(imagePath);
        Uri fileuri = MediaStore.Files.getContentUri("external", imageId);
        mContentResolver.delete(fileuri, null, null);
        assertFileNotExists(imagePath);
    }

    @Test
    public void testGetContentUri() {
        Cursor c = null;
        assertNotNull(c = mContentResolver.query(Thumbnails.getContentUri("internal"), null, null,
                null, null));
        c.close();
        assertNotNull(c = mContentResolver.query(Thumbnails.getContentUri(mVolumeName), null, null,
                null, null));
        c.close();
    }

    @Test
    public void testStoreImagesMediaExternal() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;
        prepareImages();

        final String externalImgPath = Environment.getExternalStorageDirectory() +
                "/testimage.jpg";
        final String externalImgPath2 = Environment.getExternalStorageDirectory() +
                "/testimage1.jpg";
        ContentValues values = new ContentValues();
        values.put(Thumbnails.KIND, Thumbnails.FULL_SCREEN_KIND);
        values.put(Thumbnails.IMAGE_ID, ContentUris.parseId(mRed));
        values.put(Thumbnails.HEIGHT, 480);
        values.put(Thumbnails.WIDTH, 320);
        values.put(Thumbnails.DATA, externalImgPath);

        // insert
        Uri uri = mContentResolver.insert(Thumbnails.EXTERNAL_CONTENT_URI, values);
        assertNotNull(uri);

        // query
        Cursor c = mContentResolver.query(uri, null, null, null, null);
        assertEquals(1, c.getCount());
        c.moveToFirst();
        long id = c.getLong(c.getColumnIndex(Thumbnails._ID));
        assertTrue(id > 0);
        assertEquals(Thumbnails.FULL_SCREEN_KIND, c.getInt(c.getColumnIndex(Thumbnails.KIND)));
        assertEquals(ContentUris.parseId(mRed), c.getLong(c.getColumnIndex(Thumbnails.IMAGE_ID)));
        assertEquals(480, c.getInt(c.getColumnIndex(Thumbnails.HEIGHT)));
        assertEquals(320, c.getInt(c.getColumnIndex(Thumbnails.WIDTH)));
        assertEquals(externalImgPath, c.getString(c.getColumnIndex(Thumbnails.DATA)));
        c.close();

        // update
        values.clear();
        values.put(Thumbnails.KIND, Thumbnails.MICRO_KIND);
        values.put(Thumbnails.IMAGE_ID, ContentUris.parseId(mBlue));
        values.put(Thumbnails.HEIGHT, 50);
        values.put(Thumbnails.WIDTH, 50);
        values.put(Thumbnails.DATA, externalImgPath2);
        assertEquals(1, mContentResolver.update(uri, values, null, null));

        // delete
        assertEquals(1, mContentResolver.delete(uri, null, null));
    }

    @Test
    public void testThumbnailGenerationAndCleanup() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;
        final ContentResolver resolver = mContentResolver;

        // insert an image
        Bitmap src = BitmapFactory.decodeResource(mContext.getResources(), R.raw.scenery);
        Uri uri = Uri.parse(Media.insertImage(mContentResolver, src, "cts" + System.nanoTime(),
                "test description"));
        long imageId = ContentUris.parseId(uri);

        ProviderTestUtils.waitForIdle();
        assertNotNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MINI_KIND, null));
        assertNotNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MICRO_KIND, null));

        // delete the source image and check that the thumbnail is gone too
        mContentResolver.delete(uri, null /* where clause */, null /* where args */);

        ProviderTestUtils.waitForIdle();
        assertNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MINI_KIND, null));
        assertNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MICRO_KIND, null));

        // insert again
        uri = Uri.parse(Media.insertImage(mContentResolver, src, "cts" + System.nanoTime(),
                "test description"));
        imageId = ContentUris.parseId(uri);

        // query its thumbnail again
        ProviderTestUtils.waitForIdle();
        assertNotNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MINI_KIND, null));
        assertNotNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MICRO_KIND, null));

        // update the media type
        ContentValues values = new ContentValues();
        values.put("media_type", 0);
        assertEquals("unexpected number of updated rows",
                1, mContentResolver.update(uri, values, null /* where */, null /* where args */));

        // image was marked as regular file in the database, which should have deleted its thumbnail
        ProviderTestUtils.waitForIdle();
        assertNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MINI_KIND, null));
        assertNull(Thumbnails.getThumbnail(resolver, imageId, Thumbnails.MICRO_KIND, null));

        // check source no longer exists as image
        Cursor c = mContentResolver.query(uri,
                null /* projection */, null /* where */, null /* where args */, null /* sort */);
        assertFalse("source entry should be gone", c.moveToNext());
        c.close();

        // check source still exists as file
        Uri fileUri = ContentUris.withAppendedId(
                MediaStore.Files.getContentUri("external"),
                Long.valueOf(uri.getLastPathSegment()));
        c = mContentResolver.query(fileUri,
                null /* projection */, null /* where */, null /* where args */, null /* sort */);
        assertTrue("source entry is gone", c.moveToNext());
        String sourcePath = c.getString(c.getColumnIndex("_data"));
        c.close();

        // clean up
        mContentResolver.delete(fileUri, null /* where */, null /* where args */);
        new File(sourcePath).delete();
    }

    @Test
    public void testThumbnailOrderedQuery() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;

        Bitmap src = BitmapFactory.decodeResource(mContext.getResources(), R.raw.scenery);
        Uri url[] = new Uri[3];
        try{
            for (int i = 0; i < url.length; i++) {
                url[i] = Uri.parse(
                        Media.insertImage(mContentResolver, src, "cts" + System.nanoTime(), null));
                mRowsAdded.add(url[i]);
                long origId = Long.parseLong(url[i].getLastPathSegment());
                ProviderTestUtils.waitForIdle();
                Bitmap foo = MediaStore.Images.Thumbnails.getThumbnail(mContentResolver,
                        origId, Thumbnails.MICRO_KIND, null);
                assertNotNull(foo);
            }

            // Remove one of the images, which will also delete any thumbnails
            // If the image was deleted, we don't want to delete it again
            if (mContentResolver.delete(url[1], null, null) > 0) {
                mRowsAdded.remove(url[1]);
            }

            long removedId = Long.parseLong(url[1].getLastPathSegment());
            long remainingId1 = Long.parseLong(url[0].getLastPathSegment());
            long remainingId2 = Long.parseLong(url[2].getLastPathSegment());

            // check if a thumbnail is still being returned for the image that was removed
            ProviderTestUtils.waitForIdle();
            Bitmap foo = MediaStore.Images.Thumbnails.getThumbnail(mContentResolver,
                    removedId, Thumbnails.MICRO_KIND, null);
            assertNull(foo);

            for (String order: new String[] { " ASC", " DESC" }) {
                Cursor c = mContentResolver.query(
                        MediaStore.Images.Media.EXTERNAL_CONTENT_URI, null, null, null,
                        MediaColumns._ID + order);
                while (c.moveToNext()) {
                    long id = c.getLong(c.getColumnIndex(MediaColumns._ID));
                    ProviderTestUtils.waitForIdle();
                    foo = MediaStore.Images.Thumbnails.getThumbnail(
                            mContentResolver, id,
                            MediaStore.Images.Thumbnails.MICRO_KIND, null);
                    if (id == removedId) {
                        assertNull("unexpected bitmap with" + order + " ordering", foo);
                    } else if (id == remainingId1 || id == remainingId2) {
                        assertNotNull("missing bitmap with" + order + " ordering", foo);
                    }
                }
                c.close();
            }
        } catch (UnsupportedOperationException e) {
            // the tests will be aborted because the image will be put in sdcard
            fail("There is no sdcard attached! " + e.getMessage());
        }
    }

    @Test
    public void testInsertUpdateDelete() throws Exception {
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(
                mExternalImages, displayName, "image/png");
        final Uri pendingUri = MediaStoreUtils.createPending(mContext, params);
        final Uri finalUri;
        try (PendingSession session = MediaStoreUtils.openPending(mContext, pendingUri)) {
            try (OutputStream out = session.openOutputStream()) {
                writeImage(mLargestDimension, mLargestDimension, Color.RED, out);
            }
            finalUri = session.publish();
        }

        // Directly reading should be larger
        final Bitmap full = ImageDecoder
                .decodeBitmap(ImageDecoder.createSource(mContentResolver, finalUri));
        assertEquals(mLargestDimension, full.getWidth());
        assertEquals(mLargestDimension, full.getHeight());

        {
            // Thumbnail should be smaller
            ProviderTestUtils.waitForIdle();
            final Bitmap thumb = mContentResolver.loadThumbnail(finalUri, new Size(32, 32), null);
            assertTrue(thumb.getWidth() < full.getWidth());
            assertTrue(thumb.getHeight() < full.getHeight());

            // Thumbnail should match contents
            assertColorMostlyEquals(Color.RED, extractAverageColor(thumb));
        }

        // Verify legacy APIs still work
        if (MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) {
            for (int kind : new int[] {
                    MediaStore.Images.Thumbnails.MINI_KIND,
                    MediaStore.Images.Thumbnails.FULL_SCREEN_KIND,
                    MediaStore.Images.Thumbnails.MICRO_KIND
            }) {
                // Thumbnail should be smaller
                ProviderTestUtils.waitForIdle();
                final Bitmap thumb = MediaStore.Images.Thumbnails.getThumbnail(mContentResolver,
                        ContentUris.parseId(finalUri), kind, null);
                assertTrue(thumb.getWidth() < full.getWidth());
                assertTrue(thumb.getHeight() < full.getHeight());

                // Thumbnail should match contents
                assertColorMostlyEquals(Color.RED, extractAverageColor(thumb));
            }
        }

        // Edit image contents
        try (OutputStream out = mContentResolver.openOutputStream(finalUri)) {
            writeImage(mLargestDimension, mLargestDimension, Color.BLUE, out);
        }

        // Wait a few moments for events to settle
        ProviderTestUtils.waitForIdle();

        {
            // Thumbnail should match updated contents
            ProviderTestUtils.waitForIdle();
            final Bitmap thumb = mContentResolver.loadThumbnail(finalUri, new Size(32, 32), null);
            assertColorMostlyEquals(Color.BLUE, extractAverageColor(thumb));
        }

        // Delete image contents
        mContentResolver.delete(finalUri, null, null);

        // Thumbnail should no longer exist
        try {
            ProviderTestUtils.waitForIdle();
            mContentResolver.loadThumbnail(finalUri, new Size(32, 32), null);
            fail("Funky; we somehow made a thumbnail out of nothing?");
        } catch (FileNotFoundException expected) {
        }
    }

    private static void writeImage(int width, int height, int color, OutputStream out) {
        final Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(color);
        bitmap.compress(Bitmap.CompressFormat.PNG, 90, out);
    }

    private static void assertFileExists(String path) throws Exception {
        try {
            Os.access(path, OsConstants.F_OK);
        } catch (ErrnoException e) {
            if (e.errno == OsConstants.ENOENT) {
                fail("File " + path + " doesn't exist.");
            } else {
                throw e;
            }
        }
    }

    private static void assertFileNotExists(String path) throws Exception {
        try {
            Os.access(path, OsConstants.F_OK);
            fail("File " + path + " exists.");
        } catch (ErrnoException e) {
            if (e.errno != OsConstants.ENOENT) {
                throw e;
            }
        }
    }
}
