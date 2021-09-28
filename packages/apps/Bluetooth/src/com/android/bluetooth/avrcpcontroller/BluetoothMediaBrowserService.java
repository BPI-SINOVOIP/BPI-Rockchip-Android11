/*
 * Copyright (C) 2015 The Android Open Source Project
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

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.media.MediaBrowserCompat.MediaItem;
import android.support.v4.media.session.MediaControllerCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;

import androidx.media.MediaBrowserServiceCompat;

import com.android.bluetooth.BluetoothPrefs;
import com.android.bluetooth.R;

import java.util.ArrayList;
import java.util.List;

/**
 * Implements the MediaBrowserService interface to AVRCP and A2DP
 *
 * This service provides a means for external applications to access A2DP and AVRCP.
 * The applications are expected to use MediaBrowser (see API) and all the music
 * browsing/playback/metadata can be controlled via MediaBrowser and MediaController.
 *
 * The current behavior of MediaSessionCompat exposed by this service is as follows:
 * 1. MediaSessionCompat is active (i.e. SystemUI and other overview UIs can see updates) when
 * device is connected and first starts playing. Before it starts playing we do not activate the
 * session.
 * 1.1 The session is active throughout the duration of connection.
 * 2. The session is de-activated when the device disconnects. It will be connected again when (1)
 * happens.
 */
public class BluetoothMediaBrowserService extends MediaBrowserServiceCompat {
    private static final String TAG = "BluetoothMediaBrowserService";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    private static BluetoothMediaBrowserService sBluetoothMediaBrowserService;

    private MediaSessionCompat mSession;

    // Browsing related structures.
    private List<MediaSessionCompat.QueueItem> mMediaQueue = new ArrayList<>();

    // Media Framework Content Style constants
    private static final String CONTENT_STYLE_SUPPORTED =
            "android.media.browse.CONTENT_STYLE_SUPPORTED";
    public static final String CONTENT_STYLE_PLAYABLE_HINT =
            "android.media.browse.CONTENT_STYLE_PLAYABLE_HINT";
    public static final String CONTENT_STYLE_BROWSABLE_HINT =
            "android.media.browse.CONTENT_STYLE_BROWSABLE_HINT";
    public static final int CONTENT_STYLE_LIST_ITEM_HINT_VALUE = 1;
    public static final int CONTENT_STYLE_GRID_ITEM_HINT_VALUE = 2;

    // Error messaging extras
    public static final String ERROR_RESOLUTION_ACTION_INTENT =
            "android.media.extras.ERROR_RESOLUTION_ACTION_INTENT";
    public static final String ERROR_RESOLUTION_ACTION_LABEL =
            "android.media.extras.ERROR_RESOLUTION_ACTION_LABEL";

    /**
     * Initialize this BluetoothMediaBrowserService, creating our MediaSessionCompat, MediaPlayer
     * and MediaMetaData, and setting up mechanisms to talk with the AvrcpControllerService.
     */
    @Override
    public void onCreate() {
        if (DBG) Log.d(TAG, "onCreate");
        super.onCreate();

        // Create and configure the MediaSessionCompat
        mSession = new MediaSessionCompat(this, TAG);
        setSessionToken(mSession.getSessionToken());
        mSession.setFlags(MediaSessionCompat.FLAG_HANDLES_MEDIA_BUTTONS
                | MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
        mSession.setQueueTitle(getString(R.string.bluetooth_a2dp_sink_queue_name));
        mSession.setQueue(mMediaQueue);
        setErrorPlaybackState();
        sBluetoothMediaBrowserService = this;
    }

    List<MediaItem> getContents(final String parentMediaId) {
        AvrcpControllerService avrcpControllerService =
                AvrcpControllerService.getAvrcpControllerService();
        if (avrcpControllerService == null) {
            return new ArrayList(0);
        } else {
            return avrcpControllerService.getContents(parentMediaId);
        }
    }

    private void setErrorPlaybackState() {
        Bundle extras = new Bundle();
        extras.putString(ERROR_RESOLUTION_ACTION_LABEL,
                getString(R.string.bluetooth_connect_action));
        Intent launchIntent = new Intent();
        launchIntent.setAction(BluetoothPrefs.BLUETOOTH_SETTING_ACTION);
        launchIntent.addCategory(BluetoothPrefs.BLUETOOTH_SETTING_CATEGORY);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE;
        PendingIntent pendingIntent = PendingIntent.getActivity(getApplicationContext(), 0,
                launchIntent, flags);
        extras.putParcelable(ERROR_RESOLUTION_ACTION_INTENT, pendingIntent);
        PlaybackStateCompat errorState = new PlaybackStateCompat.Builder()
                .setErrorMessage(getString(R.string.bluetooth_disconnected))
                .setExtras(extras)
                .setState(PlaybackStateCompat.STATE_ERROR, 0, 0)
                .build();
        mSession.setPlaybackState(errorState);
    }

    private Bundle getDefaultStyle() {
        Bundle style = new Bundle();
        style.putBoolean(CONTENT_STYLE_SUPPORTED, true);
        style.putInt(CONTENT_STYLE_BROWSABLE_HINT, CONTENT_STYLE_GRID_ITEM_HINT_VALUE);
        style.putInt(CONTENT_STYLE_PLAYABLE_HINT, CONTENT_STYLE_LIST_ITEM_HINT_VALUE);
        return style;
    }

    @Override
    public synchronized void onLoadChildren(final String parentMediaId,
            final Result<List<MediaItem>> result) {
        if (DBG) Log.d(TAG, "onLoadChildren parentMediaId=" + parentMediaId);
        List<MediaItem> contents = getContents(parentMediaId);
        if (contents == null) {
            result.detach();
        } else {
            result.sendResult(contents);
        }
    }

    @Override
    public BrowserRoot onGetRoot(String clientPackageName, int clientUid, Bundle rootHints) {
        if (DBG) Log.d(TAG, "onGetRoot");
        Bundle style = getDefaultStyle();
        return new BrowserRoot(BrowseTree.ROOT, style);
    }

    private void updateNowPlayingQueue(BrowseTree.BrowseNode node) {
        List<MediaItem> songList = node.getContents();
        mMediaQueue.clear();
        if (songList != null) {
            for (MediaItem song : songList) {
                mMediaQueue.add(new MediaSessionCompat.QueueItem(
                        song.getDescription(),
                        mMediaQueue.size()));
            }
        }
        mSession.setQueue(mMediaQueue);
    }

    private void clearNowPlayingQueue() {
        mMediaQueue.clear();
        mSession.setQueue(mMediaQueue);
    }

    static synchronized void notifyChanged(BrowseTree.BrowseNode node) {
        if (sBluetoothMediaBrowserService != null) {
            if (node.getScope() == AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING) {
                sBluetoothMediaBrowserService.updateNowPlayingQueue(node);
            } else {
                sBluetoothMediaBrowserService.notifyChildrenChanged(node.getID());
            }
        }
    }

    static synchronized void addressedPlayerChanged(MediaSessionCompat.Callback callback) {
        if (sBluetoothMediaBrowserService != null) {
            if (callback == null) {
                sBluetoothMediaBrowserService.setErrorPlaybackState();
                sBluetoothMediaBrowserService.clearNowPlayingQueue();
            }
            sBluetoothMediaBrowserService.mSession.setCallback(callback);
        } else {
            Log.w(TAG, "addressedPlayerChanged Unavailable");
        }
    }

    static synchronized void trackChanged(AvrcpItem track) {
        if (DBG) Log.d(TAG, "trackChanged setMetadata=" + track);
        if (sBluetoothMediaBrowserService != null) {
            if (track != null) {
                sBluetoothMediaBrowserService.mSession.setMetadata(track.toMediaMetadata());
            } else {
                sBluetoothMediaBrowserService.mSession.setMetadata(null);
            }

        } else {
            Log.w(TAG, "trackChanged Unavailable");
        }
    }

    static synchronized void notifyChanged(PlaybackStateCompat playbackState) {
        Log.d(TAG, "notifyChanged PlaybackState" + playbackState);
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.setPlaybackState(playbackState);
        } else {
            Log.w(TAG, "notifyChanged Unavailable");
        }
    }

    /**
     * Send AVRCP Play command
     */
    public static synchronized void play() {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.getController().getTransportControls().play();
        } else {
            Log.w(TAG, "play Unavailable");
        }
    }

    /**
     * Send AVRCP Pause command
     */
    public static synchronized void pause() {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.getController().getTransportControls().pause();
        } else {
            Log.w(TAG, "pause Unavailable");
        }
    }

    /**
     * Get playback state
     */
    public static synchronized int getPlaybackState() {
        if (sBluetoothMediaBrowserService != null) {
            PlaybackStateCompat currentPlaybackState =
                    sBluetoothMediaBrowserService.mSession.getController().getPlaybackState();
            if (currentPlaybackState != null) {
                return currentPlaybackState.getState();
            }
        }
        return PlaybackStateCompat.STATE_ERROR;
    }

    /**
     * Get object for controlling playback
     */
    public static synchronized MediaControllerCompat.TransportControls getTransportControls() {
        if (sBluetoothMediaBrowserService != null) {
            return sBluetoothMediaBrowserService.mSession.getController().getTransportControls();
        } else {
            Log.w(TAG, "transportControls Unavailable");
            return null;
        }
    }

    /**
     * Set Media session active whenever we have Focus of any kind
     */
    public static synchronized void setActive(boolean active) {
        if (sBluetoothMediaBrowserService != null) {
            sBluetoothMediaBrowserService.mSession.setActive(active);
        } else {
            Log.w(TAG, "setActive Unavailable");
        }
    }

    /**
     * Get Media session for updating state
     */
    public static synchronized MediaSessionCompat getSession() {
        if (sBluetoothMediaBrowserService != null) {
            return sBluetoothMediaBrowserService.mSession;
        } else {
            Log.w(TAG, "getSession Unavailable");
            return null;
        }
    }
}
