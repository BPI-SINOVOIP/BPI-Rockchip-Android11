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

import android.content.ContentResolver;
import android.content.ContentValues;
import android.net.Uri;
import android.provider.MediaStore;
import android.provider.MediaStore.Audio.Media;
import android.provider.cts.ProviderTestUtils;

import androidx.test.runner.AndroidJUnit4;

import junit.framework.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;

/**
 * This class contains fake data and convenient methods for testing:
 * {@link MediaStore.Audio.Media}
 * {@link MediaStore.Audio.Genres}
 * {@link MediaStore.Audio.Genres.Members}
 * {@link MediaStore.Audio.Playlists}
 * {@link MediaStore.Audio.Playlists.Members}
 * {@link MediaStore.Audio.Albums}
 * {@link MediaStore.Audio.Artists}
 * {@link MediaStore.Audio.Artists.Albums}
 *
 * @see MediaStore_Audio_MediaTest
 * @see MediaStore_Audio_GenresTest
 * @see MediaStore_Audio_Genres_MembersTest
 * @see MediaStore_Audio_PlaylistsTest
 * @see MediaStore_Audio_Playlists_MembersTest
 * @see MediaStore_Audio_ArtistsTest
 * @see MediaStore_Audio_Artists_AlbumsTest
 * @see MediaStore_Audio_AlbumsTest
 */
@RunWith(AndroidJUnit4.class)
public class MediaStoreAudioTestHelper {
    public static abstract class MockAudioMediaInfo {
        public abstract ContentValues getContentValues(String volumeName);

        public Uri insert(ContentResolver contentResolver, String volumeName) {
            final Uri dirUri = MediaStore.Audio.Media.getContentUri(volumeName);
            final ContentValues values = getContentValues(volumeName);
            contentResolver.delete(dirUri, MediaStore.Audio.Media.DATA + "=?", new String[] {
                    values.getAsString(MediaStore.Audio.Media.DATA)
            });

            final Uri itemUri = contentResolver.insert(dirUri, values);
            Assert.assertNotNull(itemUri);
            return itemUri;
        }

        public int delete(ContentResolver contentResolver, Uri uri) {
            return contentResolver.delete(uri, null, null);
        }
    }

    public static class Audio1 extends MockAudioMediaInfo {
        private Audio1() {
        }

        private static Audio1 sInstance = new Audio1();

        public static Audio1 getInstance() {
            return sInstance;
        }

        public static final int IS_RINGTONE = 0;
        public static final int IS_NOTIFICATION = 0;
        public static final int IS_ALARM = 0;
        public static final int IS_MUSIC = 1;
        public static final int YEAR = 1992;
        public static final int TRACK = 1;
        public static final int DURATION = 340000;
        public static final String COMPOSER = "Bruce Swedien";
        public static final String ARTIST = "Michael Jackson";
        public static final String ALBUM = "Dangerous";
        public static final String TITLE = "Jam";
        public static final int SIZE = 2737870;
        public static final String MIME_TYPE = "audio/x-mpeg";
        public static final String FILE_NAME = "Jam.mp3";
        public static final String DISPLAY_NAME = FILE_NAME;
        public static final long DATE_MODIFIED = System.currentTimeMillis() / 1000;
        public static final String GENRE = "POP";

        @Override
        public ContentValues getContentValues(String volumeName) {
            ContentValues values = new ContentValues();
            try {
                final File data;
                if (MediaStore.VOLUME_INTERNAL.equals(volumeName)) {
                    data = new File("/data/data/android.provider.cts/files/", FILE_NAME);
                } else {
                    data = new File(ProviderTestUtils.stageDir(volumeName), FILE_NAME);
                }
                values.put(Media.DATA, data.getAbsolutePath());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            values.put(Media.DATE_MODIFIED, DATE_MODIFIED);
            values.put(Media.DISPLAY_NAME, DISPLAY_NAME);
            values.put(Media.MIME_TYPE, MIME_TYPE);
            values.put(Media.SIZE, SIZE);
            values.put(Media.TITLE, TITLE);
            values.put(Media.ALBUM, ALBUM);
            values.put(Media.ARTIST, ARTIST);
            values.put(Media.COMPOSER, COMPOSER);
            values.put(Media.DURATION, DURATION);
            values.put(Media.TRACK, TRACK);
            values.put(Media.YEAR, YEAR);
            values.put(Media.IS_MUSIC, IS_MUSIC);
            values.put(Media.IS_ALARM, IS_ALARM);
            values.put(Media.IS_NOTIFICATION, IS_NOTIFICATION);
            values.put(Media.IS_RINGTONE, IS_RINGTONE);
            return values;
        }
    }

    public static class Audio2 extends MockAudioMediaInfo {
        private Audio2() {
        }

        private static Audio2 sInstance = new Audio2();

        public static Audio2 getInstance() {
            return sInstance;
        }

        public static final int IS_RINGTONE = 1;
        public static final int IS_NOTIFICATION = 0;
        public static final int IS_ALARM = 0;
        public static final int IS_MUSIC = 0;
        public static final int YEAR = 1992;
        public static final int TRACK = 1001;
        public static final int DURATION = 338000;
        public static final String COMPOSER = "Bruce Swedien";
        public static final String ARTIST =
            "Michael Jackson - Live And Dangerous - National Stadium Bucharest";
        public static final String ALBUM =
            "Michael Jackson - Live And Dangerous - National Stadium Bucharest";
        public static final String TITLE = "Jam";
        public static final int SIZE = 2737321;
        public static final String MIME_TYPE = "audio/x-mpeg";
        public static final String FILE_NAME = "Jam_live.mp3";
        public static final String DISPLAY_NAME = FILE_NAME;
        public static final long DATE_MODIFIED = System.currentTimeMillis() / 1000;

        @Override
        public ContentValues getContentValues(String volumeName) {
            ContentValues values = new ContentValues();
            try {
                final File data;
                if (MediaStore.VOLUME_INTERNAL.equals(volumeName)) {
                    data = new File("/data/data/android.provider.cts/files/", FILE_NAME);
                } else {
                    data = new File(ProviderTestUtils.stageDir(volumeName), FILE_NAME);
                }
                values.put(Media.DATA, data.getAbsolutePath());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            values.put(Media.DATE_MODIFIED, DATE_MODIFIED);
            values.put(Media.DISPLAY_NAME, DISPLAY_NAME);
            values.put(Media.MIME_TYPE, MIME_TYPE);
            values.put(Media.SIZE, SIZE);
            values.put(Media.TITLE, TITLE);
            values.put(Media.ALBUM, ALBUM);
            values.put(Media.ARTIST, ARTIST);
            values.put(Media.COMPOSER, COMPOSER);
            values.put(Media.DURATION, DURATION);
            values.put(Media.TRACK, TRACK);
            values.put(Media.YEAR, YEAR);
            values.put(Media.IS_MUSIC, IS_MUSIC);
            values.put(Media.IS_ALARM, IS_ALARM);
            values.put(Media.IS_NOTIFICATION, IS_NOTIFICATION);
            values.put(Media.IS_RINGTONE, IS_RINGTONE);
            return values;
        }
    }

    public static class Audio3 extends Audio1 {
        private Audio3() {
        }

        private static Audio3 sInstance = new Audio3();

        public static Audio3 getInstance() {
            return sInstance;
        }

        @Override
        public ContentValues getContentValues(String volumeName) {
            ContentValues values = super.getContentValues(volumeName);
            values.put(Media.DATA, values.getAsString(Media.DATA) + ".3.mp3");
            return values;
        }
    }

    public static class Audio4 extends Audio1 {
        private Audio4() {
        }

        private static Audio4 sInstance = new Audio4();

        public static Audio4 getInstance() {
            return sInstance;
        }

        @Override
        public ContentValues getContentValues(String volumeName) {
            ContentValues values = super.getContentValues(volumeName);
            values.put(Media.DATA, values.getAsString(Media.DATA) + ".4.mp3");
            return values;
        }
    }

    public static class Audio5 extends Audio1 {
        private Audio5() {
        }

        private static Audio5 sInstance = new Audio5();

        public static Audio5 getInstance() {
            return sInstance;
        }

        @Override
        public ContentValues getContentValues(String volumeName) {
            ContentValues values = super.getContentValues(volumeName);
            values.put(Media.DATA, values.getAsString(Media.DATA) + ".5.mp3");
            return values;
        }
    }

    @Test
    public void testStub() {
        // No-op test here to keep atest happy
    }

    // These constants are not part of the public API
    public static final String EXTERNAL_VOLUME_NAME = "external";
    public static final String INTERNAL_VOLUME_NAME = "internal";
}
