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
import android.provider.MediaStore;
import android.provider.MediaStore.Audio.Media;
import android.provider.MediaStore.Audio.Playlists;
import android.provider.MediaStore.Audio.Playlists.Members;
import android.provider.cts.ProviderTestUtils;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio1;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio2;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio3;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio4;
import android.provider.cts.media.MediaStoreAudioTestHelper.Audio5;
import android.provider.cts.media.MediaStoreAudioTestHelper.MockAudioMediaInfo;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

@RunWith(Parameterized.class)
public class MediaStore_Audio_Playlists_MembersTest {
    private String[] mAudioProjection = {
            Members._ID,
            Members.ALBUM,
            Members.ALBUM_ID,
            Members.ALBUM_KEY,
            Members.ARTIST,
            Members.ARTIST_ID,
            Members.ARTIST_KEY,
            Members.COMPOSER,
            Members.DATA,
            Members.DATE_ADDED,
            Members.DATE_MODIFIED,
            Members.DISPLAY_NAME,
            Members.DURATION,
            Members.IS_ALARM,
            Members.IS_MUSIC,
            Members.IS_NOTIFICATION,
            Members.IS_RINGTONE,
            Members.MIME_TYPE,
            Members.SIZE,
            Members.TITLE,
            Members.TITLE_KEY,
            Members.TRACK,
            Members.YEAR,
    };

    private String[] mMembersProjection = {
            Members._ID,
            Members.AUDIO_ID,
            Members.PLAYLIST_ID,
            Members.PLAY_ORDER,
            Members.ALBUM,
            Members.ALBUM_ID,
            Members.ALBUM_KEY,
            Members.ARTIST,
            Members.ARTIST_ID,
            Members.ARTIST_KEY,
            Members.COMPOSER,
            Members.DATA,
            Members.DATE_ADDED,
            Members.DATE_MODIFIED,
            Members.DISPLAY_NAME,
            Members.DURATION,
            Members.IS_ALARM,
            Members.IS_MUSIC,
            Members.IS_NOTIFICATION,
            Members.IS_RINGTONE,
            Members.MIME_TYPE,
            Members.SIZE,
            Members.TITLE,
            Members.TITLE_KEY,
            Members.TRACK,
            Members.YEAR,
    };

    private Context mContext;
    private ContentResolver mContentResolver;

    private long mIdOfAudio1;
    private long mIdOfAudio2;
    private long mIdOfAudio3;
    private long mIdOfAudio4;
    private long mIdOfAudio5;

    @Parameter(0)
    public String mVolumeName;

    @Parameters
    public static Iterable<? extends Object> data() {
        return ProviderTestUtils.getSharedVolumeNames();
    }

    private long insertAudioItem(MockAudioMediaInfo which) {
        Uri uri = which.insert(mContentResolver, mVolumeName);
        Cursor c = mContentResolver.query(uri, null, null, null, null);
        c.moveToFirst();
        long id = c.getLong(c.getColumnIndex(Media._ID));
        c.close();
        return id;
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mContentResolver = mContext.getContentResolver();

        Log.d(TAG, "Using volume " + mVolumeName);

        mIdOfAudio1 = insertAudioItem(Audio1.getInstance());
        mIdOfAudio2 = insertAudioItem(Audio2.getInstance());
        mIdOfAudio3 = insertAudioItem(Audio3.getInstance());
        mIdOfAudio4 = insertAudioItem(Audio4.getInstance());
        mIdOfAudio5 = insertAudioItem(Audio5.getInstance());
    }

    @After
    public void tearDown() throws Exception {
        final Uri uri = Media.getContentUri(mVolumeName);
        mContentResolver.delete(uri, Media._ID + "=" + mIdOfAudio1, null);
        mContentResolver.delete(uri, Media._ID + "=" + mIdOfAudio2, null);
        mContentResolver.delete(uri, Media._ID + "=" + mIdOfAudio3, null);
        mContentResolver.delete(uri, Media._ID + "=" + mIdOfAudio4, null);
        mContentResolver.delete(uri, Media._ID + "=" + mIdOfAudio5, null);
    }

    @Test
    public void testGetContentUri() {
        assertEquals("content://media/external/audio/playlists/1337/members",
                Members.getContentUri("external", 1337).toString());
        assertEquals("content://media/internal/audio/playlists/3007/members",
                Members.getContentUri("internal", 3007).toString());
    }

    private Uri insertPlaylistItem(Uri playlistMembersUri, long itemid, int order) {
        ContentValues values = new ContentValues();
        values.put(Members.AUDIO_ID, itemid);
        values.put(Members.PLAY_ORDER, order);
        return mContentResolver.insert(playlistMembersUri, values);
    }

    /**
     * check that the specified playlist contains the given members in the given order
     */
    private void verifyPlaylist(Uri playlistMembersUri, long [] members, int [] ordering) {
        Cursor c = mContentResolver.query(playlistMembersUri,
                new String[] { Members.AUDIO_ID, Members.PLAY_ORDER },
                null, null, // selection, selection args
                Members.PLAY_ORDER);
        assertFalse("neither members nor ordering specified",
                members == null && ordering == null);
        if (members != null) {
            assertEquals("members length doesn't match cursor length",
                    members.length, c.getCount());
            if (ordering != null) {
                assertEquals("members and ordering must have same length",
                        members.length, ordering.length);
            }
        }
        if (ordering != null) {
            assertEquals("ordering length doesn't match cursor length",
                    ordering.length, c.getCount());
        }
        while (c.moveToNext()) {
            int pos = c.getPosition();
            if (members != null) {
                assertEquals("mismatched member at position " + pos,
                        members[pos], c.getInt(c.getColumnIndex(Members.AUDIO_ID)));
            }
            if (ordering != null) {
                assertEquals("mismatched ordering at position " + pos,
                        ordering[pos], c.getInt(c.getColumnIndex(Members.PLAY_ORDER)));
            }
        }
        c.close();
    }

    @Test
    public void testStoreAudioPlaylistsMembersExternal() throws Exception {
        // TODO: expand test to verify paths from secondary storage devices
        if (!MediaStore.VOLUME_EXTERNAL.equals(mVolumeName)) return;

        ContentValues values = new ContentValues();
        values.put(Playlists.NAME, "My favourites " + System.nanoTime());
        long dateAdded = System.currentTimeMillis();
        values.put(Playlists.DATE_ADDED, dateAdded);
        long dateModified = System.currentTimeMillis();
        values.put(Playlists.DATE_MODIFIED, dateModified);
        // insert
        Uri uri = mContentResolver.insert(Playlists.getContentUri(mVolumeName), values);
        assertNotNull(uri);
        Cursor c = mContentResolver.query(uri, null, null, null, null);
        c.moveToFirst();
        long playlistId = c.getLong(c.getColumnIndex(Playlists._ID));
        long playlist2Id = -1; // used later

        // verify that the Uri has the correct format and playlist value
        assertEquals(ContentUris.withAppendedId(Playlists.getContentUri(mVolumeName), playlistId),
                uri);

        // insert audio as the member of the playlist
        Uri membersUri = Members.getContentUri(mVolumeName, playlistId);
        Uri audioUri = insertPlaylistItem(membersUri, mIdOfAudio1, 1);

        assertNotNull(audioUri);
        assertTrue(audioUri.toString().startsWith(membersUri.toString()));

        try {
            // query the audio info
            c = mContentResolver.query(audioUri, mAudioProjection, null, null, null);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            long memberId = c.getLong(c.getColumnIndex(Members._ID));
            assertEquals(memberId, Long.parseLong(audioUri.getPathSegments().get(5)));
            final String expected1 = Audio1.getInstance().getContentValues(mVolumeName)
                    .getAsString(Members.DATA);
            assertEquals(expected1, c.getString(c.getColumnIndex(Members.DATA)));
            assertTrue(c.getLong(c.getColumnIndex(Members.DATE_ADDED)) > 0);
            assertEquals(Audio1.DATE_MODIFIED, c.getLong(c.getColumnIndex(Members.DATE_MODIFIED)));
            assertEquals(Audio1.DISPLAY_NAME, c.getString(c.getColumnIndex(Members.DISPLAY_NAME)));
            assertEquals(Audio1.MIME_TYPE, c.getString(c.getColumnIndex(Members.MIME_TYPE)));
            assertEquals(Audio1.SIZE, c.getInt(c.getColumnIndex(Members.SIZE)));
            assertEquals(Audio1.TITLE, c.getString(c.getColumnIndex(Members.TITLE)));
            assertEquals(Audio1.ALBUM, c.getString(c.getColumnIndex(Members.ALBUM)));
            assertNotNull(c.getString(c.getColumnIndex(Members.ALBUM_KEY)));
            assertTrue(c.getLong(c.getColumnIndex(Members.ALBUM_ID)) > 0);
            assertEquals(Audio1.ARTIST, c.getString(c.getColumnIndex(Members.ARTIST)));
            assertNotNull(c.getString(c.getColumnIndex(Members.ARTIST_KEY)));
            assertTrue(c.getLong(c.getColumnIndex(Members.ARTIST_ID)) > 0);
            assertEquals(Audio1.COMPOSER, c.getString(c.getColumnIndex(Members.COMPOSER)));
            assertEquals(Audio1.DURATION, c.getLong(c.getColumnIndex(Members.DURATION)));
            assertEquals(Audio1.IS_ALARM, c.getInt(c.getColumnIndex(Members.IS_ALARM)));
            assertEquals(Audio1.IS_MUSIC, c.getInt(c.getColumnIndex(Members.IS_MUSIC)));
            assertEquals(Audio1.IS_NOTIFICATION,
                    c.getInt(c.getColumnIndex(Members.IS_NOTIFICATION)));
            assertEquals(Audio1.IS_RINGTONE, c.getInt(c.getColumnIndex(Members.IS_RINGTONE)));
            assertEquals(Audio1.TRACK, c.getInt(c.getColumnIndex(Members.TRACK)));
            assertEquals(Audio1.YEAR, c.getInt(c.getColumnIndex(Members.YEAR)));
            assertNotNull(c.getString(c.getColumnIndex(Members.TITLE_KEY)));
            c.close();

            // query the play order of the audio
            verifyPlaylist(membersUri, new long [] {mIdOfAudio1}, new int [] {1});

            // update the member
            values.clear();
            values.put(Members.PLAY_ORDER, 2);
            values.put(Members.AUDIO_ID, mIdOfAudio2);
            int result = mContentResolver.update(membersUri, values, Members.AUDIO_ID + "="
                    + mIdOfAudio1, null);
            assertEquals(1, result);

            // query all info
            c = mContentResolver.query(membersUri, mMembersProjection, null, null,
                    Members.DEFAULT_SORT_ORDER);
            assertEquals(1, c.getCount());
            c.moveToFirst();
            assertEquals(1, c.getInt(c.getColumnIndex(Members.PLAY_ORDER)));
            assertEquals(memberId, c.getLong(c.getColumnIndex(Members._ID)));
            final String expected2 = Audio2.getInstance().getContentValues(mVolumeName)
                    .getAsString(Members.DATA);
            assertEquals(expected2, c.getString(c.getColumnIndex(Members.DATA)));
            assertTrue(c.getLong(c.getColumnIndex(Members.DATE_ADDED)) > 0);
            assertEquals(Audio2.DATE_MODIFIED, c.getLong(c.getColumnIndex(Members.DATE_MODIFIED)));
            assertEquals(Audio2.DISPLAY_NAME, c.getString(c.getColumnIndex(Members.DISPLAY_NAME)));
            assertEquals(Audio2.MIME_TYPE, c.getString(c.getColumnIndex(Members.MIME_TYPE)));
            assertEquals(Audio2.SIZE, c.getInt(c.getColumnIndex(Members.SIZE)));
            assertEquals(Audio2.TITLE, c.getString(c.getColumnIndex(Members.TITLE)));
            assertEquals(Audio2.ALBUM, c.getString(c.getColumnIndex(Members.ALBUM)));
            assertNotNull(c.getString(c.getColumnIndex(Members.ALBUM_KEY)));
            assertTrue(c.getLong(c.getColumnIndex(Members.ALBUM_ID)) > 0);
            assertEquals(Audio2.ARTIST, c.getString(c.getColumnIndex(Members.ARTIST)));
            assertNotNull(c.getString(c.getColumnIndex(Members.ARTIST_KEY)));
            assertTrue(c.getLong(c.getColumnIndex(Members.ARTIST_ID)) > 0);
            assertEquals(Audio2.COMPOSER, c.getString(c.getColumnIndex(Members.COMPOSER)));
            assertEquals(Audio2.DURATION, c.getLong(c.getColumnIndex(Members.DURATION)));
            assertEquals(Audio2.IS_ALARM, c.getInt(c.getColumnIndex(Members.IS_ALARM)));
            assertEquals(Audio2.IS_MUSIC, c.getInt(c.getColumnIndex(Members.IS_MUSIC)));
            assertEquals(Audio2.IS_NOTIFICATION,
                    c.getInt(c.getColumnIndex(Members.IS_NOTIFICATION)));
            assertEquals(Audio2.IS_RINGTONE, c.getInt(c.getColumnIndex(Members.IS_RINGTONE)));
            assertEquals(Audio2.TRACK, c.getInt(c.getColumnIndex(Members.TRACK)));
            assertEquals(Audio2.YEAR, c.getInt(c.getColumnIndex(Members.YEAR)));
            assertNotNull(c.getString(c.getColumnIndex(Members.TITLE_KEY)));
            c.close();

            // update the member back to its original state
            values.clear();
            values.put(Members.PLAY_ORDER, 1);
            values.put(Members.AUDIO_ID, mIdOfAudio1);
            result = mContentResolver.update(membersUri, values, Members.AUDIO_ID + "="
                    + mIdOfAudio2, null);
            assertEquals(1, result);

            // insert another member into the playlist
            Uri audioUri2 = insertPlaylistItem(membersUri, mIdOfAudio2, 2);
            // the playlist should now have id1 at position 1 and id2 at position2, check that
            verifyPlaylist(membersUri, new long [] {mIdOfAudio1, mIdOfAudio2}, new int [] {1,2});

            // swap the items around
            assertTrue(MediaStore.Audio.Playlists.Members.moveItem(mContentResolver, playlistId, 1, 0));

            // check the new positions
            verifyPlaylist(membersUri, new long [] {mIdOfAudio2, mIdOfAudio1}, new int [] {1,2});

            // swap the items around in the other direction
            assertTrue(MediaStore.Audio.Playlists.Members.moveItem(mContentResolver, playlistId, 0, 1));

            // check the positions again
            verifyPlaylist(membersUri, new long [] {mIdOfAudio1, mIdOfAudio2}, new int [] {1,2});

            // insert a third item into the playlist
            Uri audioUri3 = insertPlaylistItem(membersUri, mIdOfAudio3, 3);
            // the playlist should now have id1 at position 1, id2 at position2, and
            // id3 at position3, check that
            verifyPlaylist(membersUri,
                    new long [] {mIdOfAudio1, mIdOfAudio2, mIdOfAudio3}, new int [] {1,2,3});

            // delete the middle item
            mContentResolver.delete(membersUri,
                    Playlists.Members.AUDIO_ID + "=" + mIdOfAudio2, null);

            // check the remaining items are still in the right order, and the play_order of the
            // last item has been adjusted
            verifyPlaylist(membersUri, new long [] {mIdOfAudio1, mIdOfAudio3}, new int [] {1,2});

            // try to swap the remaining two items
            assertTrue(MediaStore.Audio.Playlists.Members.moveItem(mContentResolver, playlistId, 1, 0));

            // check that they're swapped
            verifyPlaylist(membersUri, new long [] {mIdOfAudio3, mIdOfAudio1}, new int [] {1,2});

            // add 3 items, do some more moving and checking
            mIdOfAudio2 = insertAudioItem(Audio2.getInstance());
            insertPlaylistItem(membersUri, mIdOfAudio2, 3);
            insertPlaylistItem(membersUri, mIdOfAudio4, 4);
            insertPlaylistItem(membersUri, mIdOfAudio5, 5);
            verifyPlaylist(membersUri,
                    new long [] {mIdOfAudio3, mIdOfAudio1, mIdOfAudio2, mIdOfAudio4, mIdOfAudio5},
                    new int[] {1, 2, 3, 4, 5});
            assertTrue(MediaStore.Audio.Playlists.Members.moveItem(mContentResolver, playlistId, 1, 3));
            verifyPlaylist(membersUri,
                    new long [] {mIdOfAudio3, mIdOfAudio2, mIdOfAudio4, mIdOfAudio1, mIdOfAudio5},
                    new int[] {1, 2, 3, 4, 5});
            c = mContentResolver.query(membersUri, null, null, null, null);
            c.close();

            // create another playlist
            values.clear();
            values.put(Playlists.NAME, "My favourites " + System.nanoTime());
            values.put(Playlists.DATE_ADDED, dateAdded);
            values.put(Playlists.DATE_MODIFIED, dateModified);
            // insert
            uri = mContentResolver.insert(Playlists.getContentUri(mVolumeName), values);
            assertNotNull(uri);
            c = mContentResolver.query(uri, null, null, null, null);
            c.moveToFirst();
            playlist2Id = c.getLong(c.getColumnIndex(Playlists._ID));
            c.close();

            // insert audio into 2nd playlist
            Uri members2Uri = Members.getContentUri(mVolumeName, playlist2Id);
            Uri audio2Uri = insertPlaylistItem(members2Uri, mIdOfAudio1, 1);

            c = mContentResolver.query(membersUri, null, null, null, null);
            int allcolscount = c.getCount();
            c.close();
            c = mContentResolver.query(membersUri,
                    new String[] { Members.AUDIO_ID, Members.PLAY_ORDER }, null, null, null);
            int somecolscount = c.getCount();
            c.close();
            assertEquals("Different count depending on columns", allcolscount, somecolscount);

            // check that the audio exists in both playlist
            c = mContentResolver.query(membersUri, null, null, null, null);
            assertEquals(5, c.getCount());
            int cnt = 0;
            int colidx = c.getColumnIndex(Members.AUDIO_ID);
            assertTrue(colidx >= 0);
            while(c.moveToNext()) {
                if (c.getLong(colidx) == mIdOfAudio1) {
                    cnt++;
                }
            }
            assertEquals(1, cnt);
            c.close();
            c = mContentResolver.query(members2Uri, null, null, null, null);
            assertEquals(1, c.getCount());
            cnt = 0;
            while(c.moveToNext()) {
                if (c.getLong(colidx) == mIdOfAudio1) {
                    cnt++;
                }
            }
            assertEquals(1, cnt);
            c.close();

            // delete the members
            result = mContentResolver.delete(membersUri, null, null);
            assertEquals(5, result);
            result = mContentResolver.delete(members2Uri, null, null);
            assertEquals(1, result);

            // insert again, then verify that deleting the audio entry cleans up its playlist member
            // entry as well
            membersUri = Members.getContentUri(mVolumeName, playlistId);
            audioUri = insertPlaylistItem(membersUri, mIdOfAudio1, 1);
            assertNotNull(audioUri);
            // Query members of the playlist
            c = mContentResolver.query(membersUri,
                    new String[] { Members.AUDIO_ID, Members.PLAYLIST_ID}, null, null, null);
            colidx = c.getColumnIndex(Members.AUDIO_ID);
            cnt = 0;
            // The song should appear only once, for the playlist we used when inserting it
            while(c.moveToNext()) {
                if (c.getLong(colidx) == mIdOfAudio1) {
                    cnt++;
                    assertEquals(playlistId, c.getLong(c.getColumnIndex(Members.PLAYLIST_ID)));
                }
            }
            assertEquals(1, cnt);
            c.close();
            mContentResolver.delete(Media.getContentUri(mVolumeName),
                    Media._ID + "=" + mIdOfAudio1, null);
            // Query members of the playlist
            c = mContentResolver.query(membersUri,
                    new String[] { Members.AUDIO_ID, Members.PLAYLIST_ID}, null, null, null);
            colidx = c.getColumnIndex(Members.AUDIO_ID);
            cnt = 0;
            // The song should no longer appear in the playlist
            while(c.moveToNext()) {
                if (c.getLong(colidx) == mIdOfAudio1) {
                    cnt++;
                }
            }
            assertEquals(0, cnt);
            c.close();

        } finally {
            // delete the playlists
            mContentResolver.delete(Playlists.getContentUri(mVolumeName),
                    Playlists._ID + "=" + playlistId, null);
            if (playlist2Id >= 0) {
                mContentResolver.delete(Playlists.getContentUri(mVolumeName),
                        Playlists._ID + "=" + playlist2Id, null);
            }
        }
    }
}
