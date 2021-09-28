/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.net.Uri;
import android.support.v4.media.MediaBrowserCompat.MediaItem;
import android.support.v4.media.MediaDescriptionCompat;
import android.support.v4.media.MediaMetadataCompat;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test suite for the AvrcpItem class.
 */
@RunWith(AndroidJUnit4.class)
public final class AvrcpItemTest {

    private BluetoothDevice mDevice;
    private static final String UUID = "AVRCP-ITEM-TEST-UUID";

     // Attribute ID Values from AVRCP Specification
    private static final int MEDIA_ATTRIBUTE_TITLE = 0x01;
    private static final int MEDIA_ATTRIBUTE_ARTIST_NAME = 0x02;
    private static final int MEDIA_ATTRIBUTE_ALBUM_NAME = 0x03;
    private static final int MEDIA_ATTRIBUTE_TRACK_NUMBER = 0x04;
    private static final int MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER = 0x05;
    private static final int MEDIA_ATTRIBUTE_GENRE = 0x06;
    private static final int MEDIA_ATTRIBUTE_PLAYING_TIME = 0x07;
    private static final int MEDIA_ATTRIBUTE_COVER_ART_HANDLE = 0x08;

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getTargetContext();
        mDevice = BluetoothAdapter.getDefaultAdapter().getRemoteDevice("AA:BB:CC:DD:EE:FF");
    }

    @After
    public void tearDown() {
        mDevice = null;
    }

    @Test
    public void buildAvrcpItem() {
        String title = "Aaaaargh";
        String artist = "Bluetooth";
        String album = "The Best Protocol";
        long trackNumber = 1;
        long totalTracks = 12;
        String genre = "Viking Metal";
        long playingTime = 301;
        String artHandle = "abc123";
        Uri uri = Uri.parse("content://somewhere");
        Uri uri2 = Uri.parse("content://somewhereelse");

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setItemType(AvrcpItem.TYPE_MEDIA);
        builder.setType(AvrcpItem.MEDIA_AUDIO);
        builder.setDevice(mDevice);
        builder.setPlayable(true);
        builder.setUid(0);
        builder.setUuid(UUID);
        builder.setTitle(title);
        builder.setArtistName(artist);
        builder.setAlbumName(album);
        builder.setTrackNumber(trackNumber);
        builder.setTotalNumberOfTracks(totalTracks);
        builder.setGenre(genre);
        builder.setPlayingTime(playingTime);
        builder.setCoverArtHandle(artHandle);
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();

        Assert.assertEquals(mDevice, item.getDevice());
        Assert.assertEquals(true, item.isPlayable());
        Assert.assertEquals(false, item.isBrowsable());
        Assert.assertEquals(0, item.getUid());
        Assert.assertEquals(UUID, item.getUuid());
        Assert.assertEquals(null, item.getDisplayableName());
        Assert.assertEquals(title, item.getTitle());
        Assert.assertEquals(artist, item.getArtistName());
        Assert.assertEquals(album, item.getAlbumName());
        Assert.assertEquals(trackNumber, item.getTrackNumber());
        Assert.assertEquals(totalTracks, item.getTotalNumberOfTracks());
        Assert.assertEquals(artHandle, item.getCoverArtHandle());
        Assert.assertEquals(uri, item.getCoverArtLocation());
    }

    @Test
    public void buildAvrcpItemFromAvrcpAttributes() {
        String title = "Aaaaargh";
        String artist = "Bluetooth";
        String album = "The Best Protocol";
        String trackNumber = "1";
        String totalTracks = "12";
        String genre = "Viking Metal";
        String playingTime = "301";
        String artHandle = "abc123";

        int[] attrIds = new int[]{
            MEDIA_ATTRIBUTE_TITLE,
            MEDIA_ATTRIBUTE_ARTIST_NAME,
            MEDIA_ATTRIBUTE_ALBUM_NAME,
            MEDIA_ATTRIBUTE_TRACK_NUMBER,
            MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER,
            MEDIA_ATTRIBUTE_GENRE,
            MEDIA_ATTRIBUTE_PLAYING_TIME,
            MEDIA_ATTRIBUTE_COVER_ART_HANDLE
        };

        String[] attrMap = new String[]{
            title,
            artist,
            album,
            trackNumber,
            totalTracks,
            genre,
            playingTime,
            artHandle
        };

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.fromAvrcpAttributeArray(attrIds, attrMap);
        AvrcpItem item = builder.build();

        Assert.assertEquals(null, item.getDevice());
        Assert.assertEquals(false, item.isPlayable());
        Assert.assertEquals(false, item.isBrowsable());
        Assert.assertEquals(0, item.getUid());
        Assert.assertEquals(null, item.getUuid());
        Assert.assertEquals(null, item.getDisplayableName());
        Assert.assertEquals(title, item.getTitle());
        Assert.assertEquals(artist, item.getArtistName());
        Assert.assertEquals(album, item.getAlbumName());
        Assert.assertEquals(1, item.getTrackNumber());
        Assert.assertEquals(12, item.getTotalNumberOfTracks());
        Assert.assertEquals(artHandle, item.getCoverArtHandle());
        Assert.assertEquals(null, item.getCoverArtLocation());
    }

    @Test
    public void buildAvrcpItemFromAvrcpAttributesWithBadIds_badIdsIgnored() {
        String title = "Aaaaargh";
        String artist = "Bluetooth";
        String album = "The Best Protocol";
        String trackNumber = "1";
        String totalTracks = "12";
        String genre = "Viking Metal";
        String playingTime = "301";
        String artHandle = "abc123";

        int[] attrIds = new int[]{
            MEDIA_ATTRIBUTE_TITLE,
            MEDIA_ATTRIBUTE_ARTIST_NAME,
            MEDIA_ATTRIBUTE_ALBUM_NAME,
            MEDIA_ATTRIBUTE_TRACK_NUMBER,
            MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER,
            MEDIA_ATTRIBUTE_GENRE,
            MEDIA_ATTRIBUTE_PLAYING_TIME,
            MEDIA_ATTRIBUTE_COVER_ART_HANDLE,
            75,
            76,
            77,
            78
        };

        String[] attrMap = new String[]{
            title,
            artist,
            album,
            trackNumber,
            totalTracks,
            genre,
            playingTime,
            artHandle,
            "ignore me",
            "ignore me",
            "ignore me",
            "ignore me"
        };

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.fromAvrcpAttributeArray(attrIds, attrMap);
        AvrcpItem item = builder.build();

        Assert.assertEquals(null, item.getDevice());
        Assert.assertEquals(false, item.isPlayable());
        Assert.assertEquals(false, item.isBrowsable());
        Assert.assertEquals(0, item.getUid());
        Assert.assertEquals(null, item.getUuid());
        Assert.assertEquals(null, item.getDisplayableName());
        Assert.assertEquals(title, item.getTitle());
        Assert.assertEquals(artist, item.getArtistName());
        Assert.assertEquals(album, item.getAlbumName());
        Assert.assertEquals(1, item.getTrackNumber());
        Assert.assertEquals(12, item.getTotalNumberOfTracks());
        Assert.assertEquals(artHandle, item.getCoverArtHandle());
        Assert.assertEquals(null, item.getCoverArtLocation());
    }

    @Test
    public void updateCoverArtLocation() {
        Uri uri = Uri.parse("content://somewhere");
        Uri uri2 = Uri.parse("content://somewhereelse");

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();
        Assert.assertEquals(uri, item.getCoverArtLocation());

        item.setCoverArtLocation(uri2);
        Assert.assertEquals(uri2, item.getCoverArtLocation());
    }

    @Test
    public void avrcpMediaItem_toMediaMetadata() {
        String title = "Aaaaargh";
        String artist = "Bluetooth";
        String album = "The Best Protocol";
        long trackNumber = 1;
        long totalTracks = 12;
        String genre = "Viking Metal";
        long playingTime = 301;
        String artHandle = "abc123";
        Uri uri = Uri.parse("content://somewhere");

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setItemType(AvrcpItem.TYPE_MEDIA);
        builder.setType(AvrcpItem.MEDIA_AUDIO);
        builder.setDevice(mDevice);
        builder.setPlayable(true);
        builder.setUid(0);
        builder.setUuid(UUID);
        builder.setDisplayableName(title);
        builder.setTitle(title);
        builder.setArtistName(artist);
        builder.setAlbumName(album);
        builder.setTrackNumber(trackNumber);
        builder.setTotalNumberOfTracks(totalTracks);
        builder.setGenre(genre);
        builder.setPlayingTime(playingTime);
        builder.setCoverArtHandle(artHandle);
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();
        MediaMetadataCompat metadata = item.toMediaMetadata();

        Assert.assertEquals(UUID, metadata.getString(MediaMetadataCompat.METADATA_KEY_MEDIA_ID));
        Assert.assertEquals(title,
                metadata.getString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE));
        Assert.assertEquals(title, metadata.getString(MediaMetadataCompat.METADATA_KEY_TITLE));
        Assert.assertEquals(artist, metadata.getString(MediaMetadataCompat.METADATA_KEY_ARTIST));
        Assert.assertEquals(album, metadata.getString(MediaMetadataCompat.METADATA_KEY_ALBUM));
        Assert.assertEquals(trackNumber,
                metadata.getLong(MediaMetadataCompat.METADATA_KEY_TRACK_NUMBER));
        Assert.assertEquals(totalTracks,
                metadata.getLong(MediaMetadataCompat.METADATA_KEY_NUM_TRACKS));
        Assert.assertEquals(genre, metadata.getString(MediaMetadataCompat.METADATA_KEY_GENRE));
        Assert.assertEquals(playingTime,
                metadata.getLong(MediaMetadataCompat.METADATA_KEY_DURATION));
        Assert.assertEquals(uri,
                Uri.parse(metadata.getString(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON_URI)));
        Assert.assertEquals(uri,
                Uri.parse(metadata.getString(MediaMetadataCompat.METADATA_KEY_ART_URI)));
        Assert.assertEquals(uri,
                Uri.parse(metadata.getString(MediaMetadataCompat.METADATA_KEY_ALBUM_ART_URI)));
        Assert.assertEquals(null,
                metadata.getBitmap(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON));
        Assert.assertEquals(null, metadata.getBitmap(MediaMetadataCompat.METADATA_KEY_ART));
        Assert.assertEquals(null, metadata.getBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART));
        Assert.assertFalse(metadata.containsKey(MediaMetadataCompat.METADATA_KEY_BT_FOLDER_TYPE));
    }

    @Test
    public void avrcpFolderItem_toMediaMetadata() {
        String title = "Bluetooth Playlist";
        String artist = "Many";
        long totalTracks = 12;
        String genre = "Viking Metal";
        long playingTime = 301;
        String artHandle = "abc123";
        Uri uri = Uri.parse("content://somewhere");
        int type = AvrcpItem.FOLDER_TITLES;

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setItemType(AvrcpItem.TYPE_FOLDER);
        builder.setType(type);
        builder.setDevice(mDevice);
        builder.setUuid(UUID);
        builder.setDisplayableName(title);
        builder.setTitle(title);
        builder.setArtistName(artist);
        builder.setTotalNumberOfTracks(totalTracks);
        builder.setGenre(genre);
        builder.setPlayingTime(playingTime);
        builder.setCoverArtHandle(artHandle);
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();
        MediaMetadataCompat metadata = item.toMediaMetadata();

        Assert.assertEquals(UUID, metadata.getString(MediaMetadataCompat.METADATA_KEY_MEDIA_ID));
        Assert.assertEquals(title,
                metadata.getString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE));
        Assert.assertEquals(title, metadata.getString(MediaMetadataCompat.METADATA_KEY_TITLE));
        Assert.assertEquals(artist, metadata.getString(MediaMetadataCompat.METADATA_KEY_ARTIST));
        Assert.assertEquals(null, metadata.getString(MediaMetadataCompat.METADATA_KEY_ALBUM));
        Assert.assertEquals(totalTracks,
                metadata.getLong(MediaMetadataCompat.METADATA_KEY_NUM_TRACKS));
        Assert.assertEquals(genre, metadata.getString(MediaMetadataCompat.METADATA_KEY_GENRE));
        Assert.assertEquals(playingTime,
                metadata.getLong(MediaMetadataCompat.METADATA_KEY_DURATION));
        Assert.assertEquals(uri,
                Uri.parse(metadata.getString(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON_URI)));
        Assert.assertEquals(uri,
                Uri.parse(metadata.getString(MediaMetadataCompat.METADATA_KEY_ART_URI)));
        Assert.assertEquals(uri,
                Uri.parse(metadata.getString(MediaMetadataCompat.METADATA_KEY_ALBUM_ART_URI)));
        Assert.assertEquals(null,
                metadata.getBitmap(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON));
        Assert.assertEquals(null, metadata.getBitmap(MediaMetadataCompat.METADATA_KEY_ART));
        Assert.assertEquals(null, metadata.getBitmap(MediaMetadataCompat.METADATA_KEY_ALBUM_ART));
        Assert.assertEquals(type,
                metadata.getLong(MediaMetadataCompat.METADATA_KEY_BT_FOLDER_TYPE));
    }

    @Test
    public void avrcpItemNoDisplayName_toMediaItem() {
        String title = "Aaaaargh";
        Uri uri = Uri.parse("content://somewhere");

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setPlayable(true);
        builder.setUuid(UUID);
        builder.setTitle(title);
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();
        MediaItem mediaItem = item.toMediaItem();
        MediaDescriptionCompat desc = mediaItem.getDescription();

        Assert.assertTrue(mediaItem.isPlayable());
        Assert.assertFalse(mediaItem.isBrowsable());
        Assert.assertEquals(UUID, mediaItem.getMediaId());

        Assert.assertEquals(UUID, desc.getMediaId());
        Assert.assertEquals(null, desc.getMediaUri());
        Assert.assertEquals(title, desc.getTitle().toString());
        Assert.assertEquals(null, desc.getSubtitle());
        Assert.assertEquals(uri, desc.getIconUri());
        Assert.assertEquals(null, desc.getIconBitmap());
    }

    @Test
    public void avrcpItemWithDisplayName_toMediaItem() {
        String title = "Aaaaargh";
        String displayName = "A Different Type of Aaaaargh";
        Uri uri = Uri.parse("content://somewhere");

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setPlayable(true);
        builder.setUuid(UUID);
        builder.setDisplayableName(displayName);
        builder.setTitle(title);
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();
        MediaItem mediaItem = item.toMediaItem();
        MediaDescriptionCompat desc = mediaItem.getDescription();

        Assert.assertTrue(mediaItem.isPlayable());
        Assert.assertFalse(mediaItem.isBrowsable());
        Assert.assertEquals(UUID, mediaItem.getMediaId());

        Assert.assertEquals(UUID, desc.getMediaId());
        Assert.assertEquals(null, desc.getMediaUri());
        Assert.assertEquals(displayName, desc.getTitle().toString());
        Assert.assertEquals(null, desc.getSubtitle());
        Assert.assertEquals(uri, desc.getIconUri());
        Assert.assertEquals(null, desc.getIconBitmap());
    }

    @Test
    public void avrcpItemBrowsable_toMediaItem() {
        String title = "Aaaaargh";
        Uri uri = Uri.parse("content://somewhere");

        AvrcpItem.Builder builder = new AvrcpItem.Builder();
        builder.setBrowsable(true);
        builder.setUuid(UUID);
        builder.setTitle(title);
        builder.setCoverArtLocation(uri);

        AvrcpItem item = builder.build();
        MediaItem mediaItem = item.toMediaItem();
        MediaDescriptionCompat desc = mediaItem.getDescription();

        Assert.assertFalse(mediaItem.isPlayable());
        Assert.assertTrue(mediaItem.isBrowsable());
        Assert.assertEquals(UUID, mediaItem.getMediaId());

        Assert.assertEquals(UUID, desc.getMediaId());
        Assert.assertEquals(null, desc.getMediaUri());
        Assert.assertEquals(title, desc.getTitle().toString());
        Assert.assertEquals(null, desc.getSubtitle());
        Assert.assertEquals(uri, desc.getIconUri());
        Assert.assertEquals(null, desc.getIconBitmap());
    }
}
