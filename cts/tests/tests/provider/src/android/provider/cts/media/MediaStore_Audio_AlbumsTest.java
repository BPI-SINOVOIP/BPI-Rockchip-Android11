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
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.provider.MediaStore;
import android.provider.MediaStore.Audio.Albums;
import android.provider.MediaStore.Audio.Media;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.R;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio1;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio2;
import android.util.Log;
import android.util.Size;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.io.File;
import java.io.IOException;

@RunWith(Parameterized.class)
public class MediaStore_Audio_AlbumsTest {
    private Context mContext;
    private ContentResolver mContentResolver;

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
    }

    @Test
    public void testGetContentUri() {
        Cursor c = null;
        assertNotNull(c = mContentResolver.query(
                Albums.getContentUri(mVolumeName), null, null,
                null, null));
        c.close();
    }

    @Test
    public void testStoreAudioAlbums() {
        // do not support direct insert operation of the albums
        Uri audioAlbumsUri = Albums.getContentUri(mVolumeName);
        try {
            mContentResolver.insert(audioAlbumsUri, new ContentValues());
            fail("Should throw UnsupportedOperationException!");
        } catch (UnsupportedOperationException e) {
            // expected
        }

        // the album item is inserted when inserting audio media
        Audio1 audio1 = Audio1.getInstance();
        Uri audioMediaUri = audio1.insert(mContentResolver, mVolumeName);

        String selection = Albums.ALBUM +"=?";
        String[] selectionArgs = new String[] { Audio1.ALBUM };
        try {
            // query
            Cursor c = mContentResolver.query(audioAlbumsUri, null, selection, selectionArgs,
                    null);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            long id = c.getLong(c.getColumnIndex(Albums._ID));
            assertTrue(id > 0);
            assertFalse(c.isNull(c.getColumnIndex(Albums.ALBUM_ID)));
            assertEquals(Audio1.ALBUM, c.getString(c.getColumnIndex(Albums.ALBUM)));
            assertNull(c.getString(c.getColumnIndex(Albums.ALBUM_ART)));
            assertNotNull(c.getString(c.getColumnIndex(Albums.ALBUM_KEY)));
            assertFalse(c.isNull(c.getColumnIndex(Albums.ARTIST_ID)));
            assertEquals(Audio1.ARTIST, c.getString(c.getColumnIndex(Albums.ARTIST)));
            assertEquals(Audio1.YEAR, c.getInt(c.getColumnIndex(Albums.FIRST_YEAR)));
            assertEquals(Audio1.YEAR, c.getInt(c.getColumnIndex(Albums.LAST_YEAR)));
            assertEquals(1, c.getInt(c.getColumnIndex(Albums.NUMBER_OF_SONGS)));
            c.close();

            // do not support update operation of the albums
            ContentValues albumValues = new ContentValues();
            albumValues.put(Albums.ALBUM, Audio2.ALBUM);
            try {
                mContentResolver.update(audioAlbumsUri, albumValues, selection, selectionArgs);
                fail("Should throw UnsupportedOperationException!");
            } catch (UnsupportedOperationException e) {
                // expected
            }

            // do not support delete operation of the albums
            try {
                mContentResolver.delete(audioAlbumsUri, selection, selectionArgs);
                fail("Should throw UnsupportedOperationException!");
            } catch (UnsupportedOperationException e) {
                // expected
            }

            // test filtering
            Uri filterUri = audioAlbumsUri.buildUpon()
                .appendQueryParameter("filter", Audio1.ARTIST).build();
            c = mContentResolver.query(filterUri, null, null, null, null);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            long fid = c.getLong(c.getColumnIndex(Albums._ID));
            assertTrue(id == fid);
            c.close();

            filterUri = audioAlbumsUri.buildUpon().appendQueryParameter("filter", "xyzfoo").build();
            c = mContentResolver.query(filterUri, null, null, null, null);
            assertEquals(0, c.getCount());
            c.close();
        } finally {
            mContentResolver.delete(audioMediaUri, null, null);
        }
        // the album items are deleted when deleting the audio media which belongs to the album
        Cursor c = mContentResolver.query(audioAlbumsUri, null, selection, selectionArgs, null);
        assertEquals(0, c.getCount());
        c.close();
    }

    @Test
    public void testAlbumArt() throws Exception {
        final File dir = ProviderTestUtils.stageDir(mVolumeName);
        final File path = new File(dir, "test" + System.currentTimeMillis() + ".mp3");
        try {
            ProviderTestUtils.stageFile(R.raw.testmp3, path);

            ContentValues v = new ContentValues();
            v.put(Media.DATA, path.getAbsolutePath());
            v.put(Media.TITLE, "testing");
            v.put(Albums.ALBUM, "test" + System.currentTimeMillis());

            final Uri mediaUri = mContentResolver
                    .insert(MediaStore.Audio.Media.getContentUri(mVolumeName), v);
            final long mediaId = ContentUris.parseId(mediaUri);

            final long albumId;
            try (Cursor c = mContentResolver.query(mediaUri, null, null, null, null)) {
                assertTrue(c.moveToFirst());
                albumId = c.getLong(c.getColumnIndex(Albums.ALBUM_ID));
            }

            final Uri albumUri = ContentUris
                    .withAppendedId(MediaStore.Audio.Albums.getContentUri(mVolumeName), albumId);

            // Verify that normal thumbnails work
            assertNotNull(mContentResolver.loadThumbnail(mediaUri, new Size(32, 32), null));
            assertNotNull(mContentResolver.loadThumbnail(albumUri, new Size(32, 32), null));

            // Verify that hidden APIs still work to obtain album art
            final Uri byMedia = MediaStore.AUTHORITY_URI.buildUpon().appendPath(mVolumeName)
                    .appendPath("audio").appendPath("media")
                    .appendPath(Long.toString(mediaId)).appendPath("albumart").build();
            final Uri byAlbum = MediaStore.AUTHORITY_URI.buildUpon().appendPath(mVolumeName)
                    .appendPath("audio").appendPath("albumart")
                    .appendPath(Long.toString(albumId)).build();
            assertNotNull(BitmapFactory.decodeStream(mContentResolver.openInputStream(byMedia)));
            assertNotNull(BitmapFactory.decodeStream(mContentResolver.openInputStream(byAlbum)));

            // Delete item and confirm art is cleaned up
            mContentResolver.delete(mediaUri, null, null);
            MediaStore.waitForIdle(mContentResolver);

            try {
                mContentResolver.loadThumbnail(mediaUri, new Size(32, 32), null);
                fail();
            } catch (IOException expected) {
            }
            try {
                mContentResolver.loadThumbnail(albumUri, new Size(32, 32), null);
                fail();
            } catch (IOException expected) {
            }
            try {
                BitmapFactory.decodeStream(mContentResolver.openInputStream(byMedia));
                fail();
            } catch (IOException expected) {
            }
            try {
                BitmapFactory.decodeStream(mContentResolver.openInputStream(byAlbum));
                fail();
            } catch (IOException expected) {
            }

        } finally {
            path.delete();
        }
    }
}
