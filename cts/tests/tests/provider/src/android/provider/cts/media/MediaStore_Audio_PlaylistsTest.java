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
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.provider.MediaStore.Audio.Playlists;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.ProviderTestUtils;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

@RunWith(Parameterized.class)
public class MediaStore_Audio_PlaylistsTest {
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
                Playlists.getContentUri(mVolumeName), null, null,
                null, null));
        c.close();
    }

    @Test
    public void testStoreAudioPlaylistsExternal() throws Exception {
        ContentValues values = new ContentValues();
        values.put(Playlists.NAME, "My favourites " + System.nanoTime());
        long dateAdded = System.currentTimeMillis() / 1000;
        long dateModified = System.currentTimeMillis() / 1000;
        values.put(Playlists.DATE_MODIFIED, dateModified);
        // insert
        Uri uri = mContentResolver.insert(Playlists.getContentUri(mVolumeName), values);
        assertNotNull(uri);

        try {
            // query
            Cursor c = mContentResolver.query(uri, null, null, null, null);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            assertEquals(values.getAsString(Playlists.NAME),
                    c.getString(c.getColumnIndex(Playlists.NAME)));

            long realDateAdded = c.getLong(c.getColumnIndex(Playlists.DATE_ADDED));
            assertTrue(realDateAdded >= dateAdded);
            assertEquals(dateModified, c.getLong(c.getColumnIndex(Playlists.DATE_MODIFIED)));
            assertTrue(c.getLong(c.getColumnIndex(Playlists._ID)) > 0);
            c.close();
        } finally {
            assertEquals(1, mContentResolver.delete(uri, null, null));
        }
    }

    /**
     * Verify that creating playlists using only {@link Playlists#NAME} defined
     * will flow into the {@link MediaColumns#DISPLAY_NAME}, both during initial
     * insert and subsequent updates.
     */
    @Test
    public void testName() throws Exception {
        final String name1 = "Playlist " + System.nanoTime();
        final String name2 = "Playlist " + System.nanoTime();
        assertNotEquals(name1, name2);

        final ContentValues values = new ContentValues();
        values.clear();
        values.put(Playlists.NAME, name1);
        final Uri playlist = mContentResolver
                .insert(MediaStore.Audio.Playlists.getContentUri(mVolumeName), values);
        MediaStore.waitForIdle(mContentResolver);
        try (Cursor c = mContentResolver.query(playlist,
                new String[] { Playlists.NAME, MediaColumns.DISPLAY_NAME }, null, null)) {
            assertTrue(c.moveToFirst());
            assertTrue(c.getString(0).startsWith(name1));
            assertTrue(c.getString(1).startsWith(name1));
        }

        values.clear();
        values.put(Playlists.NAME, name2);
        mContentResolver.update(playlist, values, null);
        MediaStore.waitForIdle(mContentResolver);
        try (Cursor c = mContentResolver.query(playlist,
                new String[] { Playlists.NAME, MediaColumns.DISPLAY_NAME }, null, null)) {
            assertTrue(c.moveToFirst());
            assertTrue(c.getString(0).startsWith(name2));
            assertTrue(c.getString(1).startsWith(name2));
        }
    }
}
