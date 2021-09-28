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
import static org.junit.Assert.assertTrue;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore.Audio.Genres;
import android.provider.MediaStore.Audio.Genres.Members;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio1;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

@RunWith(Parameterized.class)
public class MediaStore_Audio_GenresTest {
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
                Genres.getContentUri(mVolumeName), null, null,
                    null, null));
        c.close();
    }

    @Test
    @Ignore("Genres cannot be directly modified")
    public void testStoreAudioGenresExternal() {
        // insert
        ContentValues values = new ContentValues();
        values.put(Genres.NAME, "POP");
        Uri uri = mContentResolver.insert(Genres.getContentUri(mVolumeName), values);
        assertNotNull(uri);

        try {
            // query
            Cursor c = mContentResolver.query(uri, null, null, null, null);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            assertEquals("POP", c.getString(c.getColumnIndex(Genres.NAME)));
            assertTrue(c.getLong(c.getColumnIndex(Genres._ID)) > 0);
            c.close();
        } finally {
            assertEquals(1, mContentResolver.delete(uri, null, null));
        }
    }

    @Test
    @Ignore("Genres cannot be directly modified")
    public void testGetContentUriForAudioId() {
        // Insert an audio file into the content provider.
        Uri audioUri = Audio1.getInstance().insert(mContentResolver, mVolumeName);
        assertNotNull(audioUri);
        long audioId = ContentUris.parseId(audioUri);
        assertTrue(audioId != -1);

        // Insert a genre into the content provider.
        ContentValues values = new ContentValues();
        values.put(Genres.NAME, "Soda Pop");
        Uri genreUri = mContentResolver.insert(Genres.getContentUri(mVolumeName), values);
        assertNotNull(genreUri);
        long genreId = ContentUris.parseId(genreUri);
        assertTrue(genreId != -1);

        Cursor cursor = null;
        try {
            // Check that the audio file has no genres yet.
            Uri audioGenresUri = Genres.getContentUriForAudioId(mVolumeName, (int) audioId);
            cursor = mContentResolver.query(audioGenresUri, null, null, null, null);
            assertFalse(cursor.moveToNext());

            // Link the audio file to the genre.
            values.clear();
            values.put(Members.AUDIO_ID, audioId);
            Uri membersUri = Members.getContentUri(mVolumeName, genreId);
            assertNotNull(mContentResolver.insert(membersUri, values));

            // Check that the audio file has the genre it was linked to.
            cursor = mContentResolver.query(audioGenresUri, null, null, null, null);
            assertTrue(cursor.moveToNext());
            assertEquals(genreId, cursor.getLong(cursor.getColumnIndex(Genres._ID)));
        } finally {
            if (cursor != null) {
                cursor.close();
            }
            assertEquals(1, mContentResolver.delete(audioUri, null, null));
            assertEquals(1, mContentResolver.delete(genreUri, null, null));
        }
    }
}
