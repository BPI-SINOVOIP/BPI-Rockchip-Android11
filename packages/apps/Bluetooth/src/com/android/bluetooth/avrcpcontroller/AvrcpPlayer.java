/*
 * Copyright (C) 2016 The Android Open Source Project
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
import android.os.SystemClock;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;

import java.util.Arrays;

/*
 * Contains information about remote player
 */
class AvrcpPlayer {
    private static final String TAG = "AvrcpPlayer";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    public static final int INVALID_ID = -1;

    public static final int FEATURE_PLAY = 40;
    public static final int FEATURE_STOP = 41;
    public static final int FEATURE_PAUSE = 42;
    public static final int FEATURE_REWIND = 44;
    public static final int FEATURE_FAST_FORWARD = 45;
    public static final int FEATURE_FORWARD = 47;
    public static final int FEATURE_PREVIOUS = 48;
    public static final int FEATURE_BROWSING = 59;
    public static final int FEATURE_NOW_PLAYING = 65;

    private BluetoothDevice mDevice;
    private int mPlayStatus = PlaybackStateCompat.STATE_NONE;
    private long mPlayTime = PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN;
    private long mPlayTimeUpdate = 0;
    private float mPlaySpeed = 1;
    private int mId;
    private String mName = "";
    private int mPlayerType;
    private byte[] mPlayerFeatures = new byte[16];
    private long mAvailableActions = PlaybackStateCompat.ACTION_PREPARE;
    private AvrcpItem mCurrentTrack;
    private PlaybackStateCompat mPlaybackStateCompat;
    private PlayerApplicationSettings mSupportedPlayerApplicationSettings =
            new PlayerApplicationSettings();
    private PlayerApplicationSettings mCurrentPlayerApplicationSettings;

    AvrcpPlayer() {
        mDevice = null;
        mId = INVALID_ID;
        //Set Default Actions in case Player data isn't available.
        mAvailableActions = PlaybackStateCompat.ACTION_PAUSE | PlaybackStateCompat.ACTION_PLAY
                | PlaybackStateCompat.ACTION_SKIP_TO_NEXT
                | PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS
                | PlaybackStateCompat.ACTION_STOP | PlaybackStateCompat.ACTION_PREPARE;
        PlaybackStateCompat.Builder playbackStateBuilder = new PlaybackStateCompat.Builder()
                .setActions(mAvailableActions);
        mPlaybackStateCompat = playbackStateBuilder.build();
    }

    AvrcpPlayer(BluetoothDevice device, int id, String name, byte[] playerFeatures, int playStatus,
            int playerType) {
        mDevice = device;
        mId = id;
        mName = name;
        mPlayStatus = playStatus;
        mPlayerType = playerType;
        mPlayerFeatures = Arrays.copyOf(playerFeatures, playerFeatures.length);
        PlaybackStateCompat.Builder playbackStateBuilder = new PlaybackStateCompat.Builder()
                .setActions(mAvailableActions);
        mPlaybackStateCompat = playbackStateBuilder.build();
        updateAvailableActions();
    }

    public BluetoothDevice getDevice() {
        return mDevice;
    }

    public int getId() {
        return mId;
    }

    public String getName() {
        return mName;
    }

    public void setPlayTime(int playTime) {
        mPlayTime = playTime;
        mPlayTimeUpdate = SystemClock.elapsedRealtime();
        mPlaybackStateCompat = new PlaybackStateCompat.Builder(mPlaybackStateCompat).setState(
                mPlayStatus, mPlayTime,
                mPlaySpeed).build();
    }

    public long getPlayTime() {
        return mPlayTime;
    }

    public void setPlayStatus(int playStatus) {
        mPlayTime += mPlaySpeed * (SystemClock.elapsedRealtime()
                - mPlaybackStateCompat.getLastPositionUpdateTime());
        mPlayStatus = playStatus;
        switch (mPlayStatus) {
            case PlaybackStateCompat.STATE_STOPPED:
                mPlaySpeed = 0;
                break;
            case PlaybackStateCompat.STATE_PLAYING:
                mPlaySpeed = 1;
                break;
            case PlaybackStateCompat.STATE_PAUSED:
                mPlaySpeed = 0;
                break;
            case PlaybackStateCompat.STATE_FAST_FORWARDING:
                mPlaySpeed = 3;
                break;
            case PlaybackStateCompat.STATE_REWINDING:
                mPlaySpeed = -3;
                break;
        }

        mPlaybackStateCompat = new PlaybackStateCompat.Builder(mPlaybackStateCompat).setState(
                mPlayStatus, mPlayTime,
                mPlaySpeed).build();
    }

    public void setSupportedPlayerApplicationSettings(
            PlayerApplicationSettings playerApplicationSettings) {
        mSupportedPlayerApplicationSettings = playerApplicationSettings;
        updateAvailableActions();
    }

    public void setCurrentPlayerApplicationSettings(
            PlayerApplicationSettings playerApplicationSettings) {
        Log.d(TAG, "Settings changed");
        mCurrentPlayerApplicationSettings = playerApplicationSettings;
        MediaSessionCompat session = BluetoothMediaBrowserService.getSession();
        session.setRepeatMode(mCurrentPlayerApplicationSettings.getSetting(
                PlayerApplicationSettings.REPEAT_STATUS));
        session.setShuffleMode(mCurrentPlayerApplicationSettings.getSetting(
                PlayerApplicationSettings.SHUFFLE_STATUS));
    }

    public int getPlayStatus() {
        return mPlayStatus;
    }

    public boolean supportsFeature(int featureId) {
        int byteNumber = featureId / 8;
        byte bitMask = (byte) (1 << (featureId % 8));
        return (mPlayerFeatures[byteNumber] & bitMask) == bitMask;
    }

    public boolean supportsSetting(int settingType, int settingValue) {
        return mSupportedPlayerApplicationSettings.supportsSetting(settingType, settingValue);
    }

    public PlaybackStateCompat getPlaybackState() {
        if (DBG) {
            Log.d(TAG, "getPlayBackState state " + mPlayStatus + " time " + mPlayTime);
        }
        return mPlaybackStateCompat;
    }

    public synchronized void updateCurrentTrack(AvrcpItem update) {
        if (update != null) {
            long trackNumber = update.getTrackNumber();
            mPlaybackStateCompat = new PlaybackStateCompat.Builder(
                    mPlaybackStateCompat).setActiveQueueItemId(
                    trackNumber - 1).build();
        }
        mCurrentTrack = update;
    }

    public synchronized boolean notifyImageDownload(String uuid, Uri imageUri) {
        if (DBG) Log.d(TAG, "Got an image download -- uuid=" + uuid + ", uri=" + imageUri);
        if (uuid == null || imageUri == null || mCurrentTrack == null) return false;
        if (uuid.equals(mCurrentTrack.getCoverArtUuid())) {
            mCurrentTrack.setCoverArtLocation(imageUri);
            if (DBG) Log.d(TAG, "Image UUID '" + uuid + "' was added to current track.");
            return true;
        }
        return false;
    }

    public synchronized AvrcpItem getCurrentTrack() {
        return mCurrentTrack;
    }

    private void updateAvailableActions() {
        if (supportsFeature(FEATURE_PLAY)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_PLAY;
        }
        if (supportsFeature(FEATURE_STOP)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_STOP;
        }
        if (supportsFeature(FEATURE_PAUSE)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_PAUSE;
        }
        if (supportsFeature(FEATURE_REWIND)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_REWIND;
        }
        if (supportsFeature(FEATURE_FAST_FORWARD)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_FAST_FORWARD;
        }
        if (supportsFeature(FEATURE_FORWARD)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_SKIP_TO_NEXT;
        }
        if (supportsFeature(FEATURE_PREVIOUS)) {
            mAvailableActions = mAvailableActions | PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS;
        }
        if (mSupportedPlayerApplicationSettings.supportsSetting(
                PlayerApplicationSettings.REPEAT_STATUS)) {
            mAvailableActions |= PlaybackStateCompat.ACTION_SET_REPEAT_MODE;
        }
        if (mSupportedPlayerApplicationSettings.supportsSetting(
                PlayerApplicationSettings.SHUFFLE_STATUS)) {
            mAvailableActions |= PlaybackStateCompat.ACTION_SET_SHUFFLE_MODE;
        }
        mPlaybackStateCompat = new PlaybackStateCompat.Builder(mPlaybackStateCompat)
                .setActions(mAvailableActions).build();

        if (DBG) Log.d(TAG, "Supported Actions = " + mAvailableActions);
    }
}
