/*
 * Copyright (C) 2020 The Android Open Source Project
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

import android.bluetooth.BluetoothDevice;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.media.MediaBrowserCompat.MediaItem;
import android.support.v4.media.MediaDescriptionCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.util.Log;

import java.util.Objects;

/**
 * An object representing a single item returned from an AVRCP folder listing in the VFS scope.
 *
 * This object knows how to turn itself into each of the Android Media Framework objects so the
 * metadata can easily be shared with the system.
 */
public class AvrcpItem {
    private static final String TAG = "AvrcpItem";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    // AVRCP Specification defined item types
    public static final int TYPE_PLAYER = 0x1;
    public static final int TYPE_FOLDER = 0x2;
    public static final int TYPE_MEDIA = 0x3;

    // AVRCP Specification defined folder item sub types. These match with the Media Framework's
    // definition of the constants as well.
    public static final int FOLDER_MIXED = 0x00;
    public static final int FOLDER_TITLES = 0x01;
    public static final int FOLDER_ALBUMS = 0x02;
    public static final int FOLDER_ARTISTS = 0x03;
    public static final int FOLDER_GENRES = 0x04;
    public static final int FOLDER_PLAYLISTS = 0x05;
    public static final int FOLDER_YEARS = 0x06;

    // AVRCP Specification defined media item sub types
    public static final int MEDIA_AUDIO = 0x00;
    public static final int MEDIA_VIDEO = 0x01;

    // Keys for packaging extra data with MediaItems
    public static final String AVRCP_ITEM_KEY_UID = "avrcp-item-key-uid";

    // Type of item, one of [TYPE_PLAYER, TYPE_FOLDER, TYPE_MEDIA]
    private int mItemType;

    // Sub type of item, dependant on whether it's a folder or media item
    // Folder -> FOLDER_* constants
    // Media -> MEDIA_* constants
    private int mType;

    // Bluetooth Device this piece of metadata came from
    private BluetoothDevice mDevice;

    // AVRCP Specification defined metadata for browsed media items
    private long mUid;
    private String mDisplayableName;

    // AVRCP Specification defined set of available attributes
    private String mTitle;
    private String mArtistName;
    private String mAlbumName;
    private long mTrackNumber;
    private long mTotalNumberOfTracks;
    private String mGenre;
    private long mPlayingTime;
    private String mCoverArtHandle;

    private boolean mPlayable = false;
    private boolean mBrowsable = false;

    // Our own book keeping value since database unaware players sometimes send repeat UIDs.
    private String mUuid;

    // A status to indicate if the image at the URI is downloaded and cached
    private String mImageUuid = null;

    // Our own internal Uri value that points to downloaded cover art image
    private Uri mImageUri;

    private AvrcpItem() {
    }

    public BluetoothDevice getDevice() {
        return mDevice;
    }

    public long getUid() {
        return mUid;
    }

    public String getUuid() {
        return mUuid;
    }

    public int getItemType() {
        return mItemType;
    }

    public int getType() {
        return mType;
    }

    public String getDisplayableName() {
        return mDisplayableName;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getArtistName() {
        return mArtistName;
    }

    public String getAlbumName() {
        return mAlbumName;
    }

    public long getTrackNumber() {
        return mTrackNumber;
    }

    public long getTotalNumberOfTracks() {
        return mTotalNumberOfTracks;
    }

    public String getGenre() {
        return mGenre;
    }

    public long getPlayingTime() {
        return mPlayingTime;
    }

    public boolean isPlayable() {
        return mPlayable;
    }

    public boolean isBrowsable() {
        return mBrowsable;
    }

    public String getCoverArtHandle() {
        return mCoverArtHandle;
    }

    public String getCoverArtUuid() {
        return mImageUuid;
    }

    public void setCoverArtUuid(String uuid) {
        mImageUuid = uuid;
    }

    public synchronized Uri getCoverArtLocation() {
        return mImageUri;
    }

    public synchronized void setCoverArtLocation(Uri uri) {
        mImageUri = uri;
    }

    /**
     * Convert this item an Android Media Framework MediaMetadata
     */
    public MediaMetadataCompat toMediaMetadata() {
        MediaMetadataCompat.Builder metaDataBuilder = new MediaMetadataCompat.Builder();
        Uri coverArtUri = getCoverArtLocation();
        String uriString = coverArtUri != null ? coverArtUri.toString() : null;
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_MEDIA_ID, mUuid);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE, mDisplayableName);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_TITLE, mTitle);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST, mArtistName);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ALBUM, mAlbumName);
        metaDataBuilder.putLong(MediaMetadataCompat.METADATA_KEY_TRACK_NUMBER, mTrackNumber);
        metaDataBuilder.putLong(MediaMetadataCompat.METADATA_KEY_NUM_TRACKS, mTotalNumberOfTracks);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_GENRE, mGenre);
        metaDataBuilder.putLong(MediaMetadataCompat.METADATA_KEY_DURATION, mPlayingTime);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON_URI, uriString);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ART_URI, uriString);
        metaDataBuilder.putString(MediaMetadataCompat.METADATA_KEY_ALBUM_ART_URI, uriString);
        if (mItemType == TYPE_FOLDER) {
            metaDataBuilder.putLong(MediaMetadataCompat.METADATA_KEY_BT_FOLDER_TYPE, mType);
        }
        return metaDataBuilder.build();
    }

    /**
     * Convert this item an Android Media Framework MediaItem
     */
    public MediaItem toMediaItem() {
        MediaDescriptionCompat.Builder descriptionBuilder = new MediaDescriptionCompat.Builder();

        descriptionBuilder.setMediaId(mUuid);

        String name = null;
        if (mDisplayableName != null) {
            name = mDisplayableName;
        } else if (mTitle != null) {
            name = mTitle;
        }
        descriptionBuilder.setTitle(name);

        descriptionBuilder.setIconUri(getCoverArtLocation());

        Bundle extras = new Bundle();
        extras.putLong(AVRCP_ITEM_KEY_UID, mUid);
        descriptionBuilder.setExtras(extras);

        int flags = 0x0;
        if (mPlayable) flags |= MediaItem.FLAG_PLAYABLE;
        if (mBrowsable) flags |= MediaItem.FLAG_BROWSABLE;

        return new MediaItem(descriptionBuilder.build(), flags);
    }

    @Override
    public String toString() {
        return "AvrcpItem{mUuid=" + mUuid + ", mUid=" + mUid + ", mItemType=" + mItemType
                + ", mType=" + mType + ", mDisplayableName=" + mDisplayableName
                + ", mTitle=" + mTitle + ", mPlayable=" + mPlayable + ", mBrowsable="
                + mBrowsable + ", mCoverArtHandle=" + getCoverArtHandle()
                + ", mImageUuid=" + mImageUuid + ", mImageUri" + mImageUri + "}";
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }

        if (!(o instanceof AvrcpItem)) {
            return false;
        }

        AvrcpItem other = ((AvrcpItem) o);
        return Objects.equals(mUuid, other.getUuid())
                && Objects.equals(mDevice, other.getDevice())
                && Objects.equals(mUid, other.getUid())
                && Objects.equals(mItemType, other.getItemType())
                && Objects.equals(mType, other.getType())
                && Objects.equals(mTitle, other.getTitle())
                && Objects.equals(mDisplayableName, other.getDisplayableName())
                && Objects.equals(mArtistName, other.getArtistName())
                && Objects.equals(mAlbumName, other.getAlbumName())
                && Objects.equals(mTrackNumber, other.getTrackNumber())
                && Objects.equals(mTotalNumberOfTracks, other.getTotalNumberOfTracks())
                && Objects.equals(mGenre, other.getGenre())
                && Objects.equals(mPlayingTime, other.getPlayingTime())
                && Objects.equals(mCoverArtHandle, other.getCoverArtHandle())
                && Objects.equals(mPlayable, other.isPlayable())
                && Objects.equals(mBrowsable, other.isBrowsable())
                && Objects.equals(mImageUri, other.getCoverArtLocation());
    }

    /**
     * Builder for an AvrcpItem
     */
    public static class Builder {
        private static final String TAG = "AvrcpItem.Builder";
        private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

        // Attribute ID Values from AVRCP Specification
        private static final int MEDIA_ATTRIBUTE_TITLE = 0x01;
        private static final int MEDIA_ATTRIBUTE_ARTIST_NAME = 0x02;
        private static final int MEDIA_ATTRIBUTE_ALBUM_NAME = 0x03;
        private static final int MEDIA_ATTRIBUTE_TRACK_NUMBER = 0x04;
        private static final int MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER = 0x05;
        private static final int MEDIA_ATTRIBUTE_GENRE = 0x06;
        private static final int MEDIA_ATTRIBUTE_PLAYING_TIME = 0x07;
        private static final int MEDIA_ATTRIBUTE_COVER_ART_HANDLE = 0x08;

        private AvrcpItem mAvrcpItem = new AvrcpItem();

        /**
         * Initialize all relevant AvrcpItem internals from the AVRCP specification defined set of
         * item attributes
         *
         * @param attrIds The array of AVRCP specification defined IDs in the order they match to
         *                the value string attrMap
         * @param attrMap The mapped values for each ID
         * @return This object so you can continue building
         */
        public Builder fromAvrcpAttributeArray(int[] attrIds, String[] attrMap) {
            int attributeCount = Math.max(attrIds.length, attrMap.length);
            for (int i = 0; i < attributeCount; i++) {
                if (DBG) Log.d(TAG, attrIds[i] + " = " + attrMap[i]);
                switch (attrIds[i]) {
                    case MEDIA_ATTRIBUTE_TITLE:
                        mAvrcpItem.mTitle = attrMap[i];
                        break;
                    case MEDIA_ATTRIBUTE_ARTIST_NAME:
                        mAvrcpItem.mArtistName = attrMap[i];
                        break;
                    case MEDIA_ATTRIBUTE_ALBUM_NAME:
                        mAvrcpItem.mAlbumName = attrMap[i];
                        break;
                    case MEDIA_ATTRIBUTE_TRACK_NUMBER:
                        try {
                            mAvrcpItem.mTrackNumber = Long.valueOf(attrMap[i]);
                        } catch (java.lang.NumberFormatException e) {
                            // If Track Number doesn't parse, leave it unset
                        }
                        break;
                    case MEDIA_ATTRIBUTE_TOTAL_TRACK_NUMBER:
                        try {
                            mAvrcpItem.mTotalNumberOfTracks = Long.valueOf(attrMap[i]);
                        } catch (java.lang.NumberFormatException e) {
                            // If Total Track Number doesn't parse, leave it unset
                        }
                        break;
                    case MEDIA_ATTRIBUTE_GENRE:
                        mAvrcpItem.mGenre = attrMap[i];
                        break;
                    case MEDIA_ATTRIBUTE_PLAYING_TIME:
                        try {
                            mAvrcpItem.mPlayingTime = Long.valueOf(attrMap[i]);
                        } catch (java.lang.NumberFormatException e) {
                            // If Playing Time doesn't parse, leave it unset
                        }
                        break;
                    case MEDIA_ATTRIBUTE_COVER_ART_HANDLE:
                        mAvrcpItem.mCoverArtHandle = attrMap[i];
                        break;
                }
            }
            return this;
        }

        /**
         * Set the item type for the AvrcpItem you are building
         *
         * Type can be one of PLAYER, FOLDER, or MEDIA
         *
         * @param itemType The item type as an AvrcpItem.* type value
         * @return This object, so you can continue building
         */
        public Builder setItemType(int itemType) {
            mAvrcpItem.mItemType = itemType;
            return this;
        }

        /**
         * Set the type for the AvrcpItem you are building
         *
         * This is the type of the PLAYER, FOLDER, or MEDIA item.
         *
         * @param type The type as one of the AvrcpItem.MEDIA_* or FOLDER_* types
         * @return This object, so you can continue building
         */
        public Builder setType(int type) {
            mAvrcpItem.mType = type;
            return this;
        }

        /**
         * Set the device for the AvrcpItem you are building
         *
         * @param device The BluetoothDevice object that this item came from
         * @return This object, so you can continue building
         */
        public Builder setDevice(BluetoothDevice device) {
            mAvrcpItem.mDevice = device;
            return this;
        }

        /**
         * Note that the AvrcpItem you are building is playable
         *
         * @param playable True if playable, false otherwise
         * @return This object, so you can continue building
         */
        public Builder setPlayable(boolean playable) {
            mAvrcpItem.mPlayable = playable;
            return this;
        }

        /**
         * Note that the AvrcpItem you are building is browsable
         *
         * @param browsable True if browsable, false otherwise
         * @return This object, so you can continue building
         */
        public Builder setBrowsable(boolean browsable) {
            mAvrcpItem.mBrowsable = browsable;
            return this;
        }

        /**
         * Set the AVRCP defined UID assigned to the AvrcpItem you are building
         *
         * @param uid The UID given to this item by the remote device
         * @return This object, so you can continue building
         */
        public Builder setUid(long uid) {
            mAvrcpItem.mUid = uid;
            return this;
        }

        /**
         * Set the UUID you wish to associate with the AvrcpItem you are building
         *
         * @param uuid A string UUID value
         * @return This object, so you can continue building
         */
        public Builder setUuid(String uuid) {
            mAvrcpItem.mUuid = uuid;
            return this;
        }

        /**
         * Set the displayable name for the AvrcpItem you are building
         *
         * @param displayableName A string representing a friendly, displayable name
         * @return This object, so you can continue building
         */
        public Builder setDisplayableName(String displayableName) {
            mAvrcpItem.mDisplayableName = displayableName;
            return this;
        }

        /**
         * Set the title for the AvrcpItem you are building
         *
         * @param title The title as a string
         * @return This object, so you can continue building
         */
        public Builder setTitle(String title) {
            mAvrcpItem.mTitle = title;
            return this;
        }

        /**
         * Set the artist name for the AvrcpItem you are building
         *
         * @param artistName The artist name as a string
         * @return This object, so you can continue building
         */
        public Builder setArtistName(String artistName) {
            mAvrcpItem.mArtistName = artistName;
            return this;
        }

        /**
         * Set the album name for the AvrcpItem you are building
         *
         * @param albumName The album name as a string
         * @return This object, so you can continue building
         */
        public Builder setAlbumName(String albumName) {
            mAvrcpItem.mAlbumName = albumName;
            return this;
        }

        /**
         * Set the track number for the AvrcpItem you are building
         *
         * @param trackNumber The track number
         * @return This object, so you can continue building
         */
        public Builder setTrackNumber(long trackNumber) {
            mAvrcpItem.mTrackNumber = trackNumber;
            return this;
        }

        /**
         * Set the total number of tracks on the playlist or album that this AvrcpItem is on
         *
         * @param totalNumberOfTracks The total number of tracks along side this item
         * @return This object, so you can continue building
         */
        public Builder setTotalNumberOfTracks(long totalNumberOfTracks) {
            mAvrcpItem.mTotalNumberOfTracks = totalNumberOfTracks;
            return this;
        }

        /**
         * Set the genre name for the AvrcpItem you are building
         *
         * @param genre The genre as a string
         * @return This object, so you can continue building
         */
        public Builder setGenre(String genre) {
            mAvrcpItem.mGenre = genre;
            return this;
        }

        /**
         * Set the total playing time for the AvrcpItem you are building
         *
         * @param playingTime The playing time in seconds
         * @return This object, so you can continue building
         */
        public Builder setPlayingTime(long playingTime) {
            mAvrcpItem.mPlayingTime = playingTime;
            return this;
        }

        /**
         * Set the cover art handle for the AvrcpItem you are building
         *
         * @param coverArtHandle The cover art image handle provided by a remote device
         * @return This object, so you can continue building
         */
        public Builder setCoverArtHandle(String coverArtHandle) {
            mAvrcpItem.mCoverArtHandle = coverArtHandle;
            return this;
        }

        /**
         * Set the location of the downloaded cover art for the AvrcpItem you are building
         *
         * @param uri The URI where our storage has placed the image associated with this item
         * @return This object, so you can continue building
         */
        public Builder setCoverArtLocation(Uri uri) {
            mAvrcpItem.setCoverArtLocation(uri);
            return this;
        }

        /**
         * Build the AvrcpItem
         *
         * @return An AvrcpItem object
         */
        public AvrcpItem build() {
            return mAvrcpItem;
        }
    }
}
