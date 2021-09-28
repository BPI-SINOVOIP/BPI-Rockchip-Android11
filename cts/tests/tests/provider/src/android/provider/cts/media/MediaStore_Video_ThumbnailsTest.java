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

import static android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT;
import static android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH;
import static android.provider.cts.ProviderTestUtils.assertColorMostlyNotEquals;
import static android.provider.cts.ProviderTestUtils.extractAverageColor;

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
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.FileUtils;
import android.provider.MediaStore;
import android.provider.MediaStore.Files;
import android.provider.MediaStore.Video.Media;
import android.provider.MediaStore.Video.Thumbnails;
import android.provider.MediaStore.Video.VideoColumns;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.util.Log;
import android.util.Size;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.MediaUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

@RunWith(Parameterized.class)
public class MediaStore_Video_ThumbnailsTest {
    private static final String TAG = "MediaStore_Video_ThumbnailsTest";

    private Context mContext;
    private ContentResolver mResolver;

    private boolean hasCodec() {
        return MediaUtils.hasCodecForResourceAndDomain(
                mContext, R.raw.testthumbvideo, "video/");
    }

    private Uri mExternalVideo;

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
        mExternalVideo = MediaStore.Video.Media.getContentUri(mVolumeName);
    }

    @Test
    public void testGetContentUri() {
        Uri internalUri = Thumbnails.getContentUri(MediaStoreAudioTestHelper.INTERNAL_VOLUME_NAME);
        Uri externalUri = Thumbnails.getContentUri(MediaStoreAudioTestHelper.EXTERNAL_VOLUME_NAME);
        assertEquals(Thumbnails.INTERNAL_CONTENT_URI, internalUri);
        assertEquals(Thumbnails.EXTERNAL_CONTENT_URI, externalUri);
    }

    @Test
    public void testGetThumbnail() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;

        // Insert a video into the provider.
        Uri videoUri = insertVideo();
        long videoId = ContentUris.parseId(videoUri);
        assertTrue(videoId != -1);
        assertEquals(ContentUris.withAppendedId(Media.EXTERNAL_CONTENT_URI, videoId),
                videoUri);

        // Don't run the test if the codec isn't supported.
        if (!hasCodec()) {
            // Calling getThumbnail should not generate a new thumbnail.
            ProviderTestUtils.waitForIdle();
            assertNull(Thumbnails.getThumbnail(mResolver, videoId, Thumbnails.MINI_KIND, null));
            Log.i(TAG, "SKIPPING testGetThumbnail(): codec not supported");
            return;
        }

        // Calling getThumbnail should generate a new thumbnail.
        ProviderTestUtils.waitForIdle();
        assertNotNull(Thumbnails.getThumbnail(mResolver, videoId, Thumbnails.MINI_KIND, null));
        assertNotNull(Thumbnails.getThumbnail(mResolver, videoId, Thumbnails.MICRO_KIND, null));

        assertEquals(1, mResolver.delete(videoUri, null, null));
    }

    @Test
    public void testThumbnailGenerationAndCleanup() throws Exception {
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;

        if (!hasCodec()) {
            // we don't support video, so no need to run the test
            Log.i(TAG, "SKIPPING testThumbnailGenerationAndCleanup(): codec not supported");
            return;
        }

        // insert a video
        Uri uri = insertVideo();

        // request thumbnail creation
        ProviderTestUtils.waitForIdle();
        assertNotNull(Thumbnails.getThumbnail(mResolver, Long.valueOf(uri.getLastPathSegment()),
                Thumbnails.MINI_KIND, null /* options */));

        // delete the source video and check that the thumbnail is gone too
        mResolver.delete(uri, null /* where clause */, null /* where args */);
        ProviderTestUtils.waitForIdle();
        assertNull(Thumbnails.getThumbnail(mResolver, Long.valueOf(uri.getLastPathSegment()),
                Thumbnails.MINI_KIND, null /* options */));

        // insert again
        uri = insertVideo();

        // request thumbnail creation
        ProviderTestUtils.waitForIdle();
        assertNotNull(Thumbnails.getThumbnail(mResolver, Long.valueOf(uri.getLastPathSegment()),
                Thumbnails.MINI_KIND, null));

        // update the media type
        ContentValues values = new ContentValues();
        values.put("media_type", 0);
        assertEquals("unexpected number of updated rows",
                1, mResolver.update(uri, values, null /* where */, null /* where args */));

        // video was marked as regular file in the database, which should have deleted its thumbnail
        ProviderTestUtils.waitForIdle();
        assertNull(Thumbnails.getThumbnail(mResolver, Long.valueOf(uri.getLastPathSegment()),
                Thumbnails.MINI_KIND, null /* options */));

        // check source no longer exists as video
        Cursor c = mResolver.query(uri,
                null /* projection */, null /* where */, null /* where args */, null /* sort */);
        assertFalse("source entry should be gone", c.moveToNext());
        c.close();

        // check source still exists as file
        Uri fileUri = ContentUris.withAppendedId(
                Files.getContentUri("external"),
                Long.valueOf(uri.getLastPathSegment()));
        c = mResolver.query(fileUri,
                null /* projection */, null /* where */, null /* where args */, null /* sort */);
        assertTrue("source entry should be gone", c.moveToNext());
        String sourcePath = c.getString(c.getColumnIndex("_data"));
        c.close();

        // clean up
        mResolver.delete(fileUri, null /* where */, null /* where args */);
        new File(sourcePath).delete();
    }

    private Uri insertVideo() throws IOException {
        File file = new File(ProviderTestUtils.stageDir(MediaStore.VOLUME_EXTERNAL),
                "testVideo" + System.nanoTime() + ".3gp");
        // clean up any potential left over entries from a previous aborted run
        mResolver.delete(Media.EXTERNAL_CONTENT_URI,
                "_data=?", new String[] { file.getAbsolutePath() });
        file.delete();

        ProviderTestUtils.stageFile(R.raw.testthumbvideo, file);

        ContentValues values = new ContentValues();
        values.put(VideoColumns.DATA, file.getAbsolutePath());
        return mResolver.insert(Media.EXTERNAL_CONTENT_URI, values);
    }

    @Test
    public void testInsertUpdateDelete() throws Exception {
        final Uri finalUri = ProviderTestUtils.stageMedia(R.raw.testvideo,
                mExternalVideo, "video/mp4");

        // Directly reading should be larger
        final Size full;
        try (MediaMetadataRetriever mmr = new MediaMetadataRetriever()) {
            mmr.setDataSource(mContext, finalUri);
            full = new Size(
                    Integer.parseInt(mmr.extractMetadata(METADATA_KEY_VIDEO_WIDTH)),
                    Integer.parseInt(mmr.extractMetadata(METADATA_KEY_VIDEO_HEIGHT)));
        }

        // Thumbnail should be smaller
        ProviderTestUtils.waitForIdle();
        final Bitmap beforeThumb = mResolver.loadThumbnail(finalUri, new Size(64, 64), null);
        assertTrue(beforeThumb.getWidth() < full.getWidth());
        assertTrue(beforeThumb.getHeight() < full.getHeight());
        final int beforeColor = extractAverageColor(beforeThumb);

        // Verify legacy APIs still work
        if (MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) {
            for (int kind : new int[] {
                    MediaStore.Video.Thumbnails.MINI_KIND,
                    MediaStore.Video.Thumbnails.FULL_SCREEN_KIND,
                    MediaStore.Video.Thumbnails.MICRO_KIND
            }) {
                ProviderTestUtils.waitForIdle();
                assertNotNull(MediaStore.Video.Thumbnails.getThumbnail(mResolver,
                        ContentUris.parseId(finalUri), kind, null));
            }
        }

        // Edit video contents
        try (InputStream from = mContext.getResources().openRawResource(R.raw.testvideo2);
                OutputStream to = mResolver.openOutputStream(finalUri)) {
            FileUtils.copy(from, to);
        }

        // Thumbnail should match updated contents
        ProviderTestUtils.waitForIdle();
        final Bitmap afterThumb = mResolver.loadThumbnail(finalUri, new Size(64, 64), null);
        final int afterColor = extractAverageColor(afterThumb);
        assertColorMostlyNotEquals(beforeColor, afterColor);

        // Delete video contents
        mResolver.delete(finalUri, null, null);

        // Thumbnail should no longer exist
        try {
            ProviderTestUtils.waitForIdle();
            mResolver.loadThumbnail(finalUri, new Size(64, 64), null);
            fail("Funky; we somehow made a thumbnail out of nothing?");
        } catch (FileNotFoundException expected) {
        }
    }
}
