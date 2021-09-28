/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.bluetooth.avrcp;

import android.annotation.NonNull;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.AudioPlaybackConfiguration;
import android.media.session.MediaSession;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;

import com.android.bluetooth.Utils;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class is directly responsible of maintaining the list of Browsable Players as well as
 * the list of Addressable Players. This variation of the list doesn't actually list all the
 * available players for a getAvailableMediaPlayers request. Instead it only reports one media
 * player with ID=0 and all the other browsable players are folders in the root of that player.
 *
 * Changing the directory to a browsable player will allow you to traverse that player as normal.
 * By only having one root player, we never have to send Addressed Player Changed notifications,
 * UIDs Changed notifications, or Available Players Changed notifications.
 *
 * TODO (apanicke): Add non-browsable players as song items to the root folder. Selecting that
 * player would effectively cause player switch by sending a play command to that player.
 */
public class MediaPlayerList {
    private static final String TAG = "AvrcpMediaPlayerList";
    private static final boolean DEBUG = true;
    static boolean sTesting = false;

    private static final String PACKAGE_SCHEME = "package";
    private static final int NO_ACTIVE_PLAYER = 0;
    private static final int BLUETOOTH_PLAYER_ID = 0;
    private static final String BLUETOOTH_PLAYER_NAME = "Bluetooth Player";
    private static final int ACTIVE_PLAYER_LOGGER_SIZE = 5;
    private static final String ACTIVE_PLAYER_LOGGER_TITLE = "Active Player Events";
    private static final int AUDIO_PLAYBACK_STATE_LOGGER_SIZE = 15;
    private static final String AUDIO_PLAYBACK_STATE_LOGGER_TITLE = "Audio Playback State Events";

    // mediaId's for the now playing list will be in the form of "NowPlayingId[XX]" where [XX]
    // is the Queue ID for the requested item.
    private static final String NOW_PLAYING_ID_PATTERN = Util.NOW_PLAYING_PREFIX + "([0-9]*)";

    // mediaId's for folder browsing will be in the form of [XX][mediaid],  where [XX] is a
    // two digit representation of the player id and [mediaid] is the original media id as a
    // string.
    private static final String BROWSE_ID_PATTERN = "\\d\\d.*";

    private Context mContext;
    private Looper mLooper; // Thread all media player callbacks and timeouts happen on
    private PackageManager mPackageManager;
    private MediaSessionManager mMediaSessionManager;
    private MediaData mCurrMediaData = null;
    private final AudioManager mAudioManager;
    private final AvrcpEventLogger mActivePlayerLogger = new AvrcpEventLogger(
            ACTIVE_PLAYER_LOGGER_SIZE, ACTIVE_PLAYER_LOGGER_TITLE);
    private final AvrcpEventLogger mAudioPlaybackStateLogger = new AvrcpEventLogger(
            AUDIO_PLAYBACK_STATE_LOGGER_SIZE, AUDIO_PLAYBACK_STATE_LOGGER_TITLE);

    private Map<Integer, MediaPlayerWrapper> mMediaPlayers =
            Collections.synchronizedMap(new HashMap<Integer, MediaPlayerWrapper>());
    private Map<String, Integer> mMediaPlayerIds =
            Collections.synchronizedMap(new HashMap<String, Integer>());
    private Map<Integer, BrowsedPlayerWrapper> mBrowsablePlayers =
            Collections.synchronizedMap(new HashMap<Integer, BrowsedPlayerWrapper>());
    private int mActivePlayerId = NO_ACTIVE_PLAYER;

    @VisibleForTesting
    private boolean mAudioPlaybackIsActive = false;

    private AvrcpTargetService.ListCallback mCallback;
    private BrowsablePlayerConnector mBrowsablePlayerConnector;

    interface MediaUpdateCallback {
        void run(MediaData data);
    }

    interface GetPlayerRootCallback {
        void run(int playerId, boolean success, String rootId, int numItems);
    }

    interface GetFolderItemsCallback {
        void run(String parentId, List<ListItem> items);
    }

    interface FolderUpdateCallback {
        void run(boolean availablePlayers, boolean addressedPlayers, boolean uids);
    }

    MediaPlayerList(Looper looper, Context context) {
        Log.v(TAG, "Creating MediaPlayerList");

        mLooper = looper;
        mContext = context;

        // Register for intents where available players might have changed
        IntentFilter pkgFilter = new IntentFilter();
        pkgFilter.addAction(Intent.ACTION_PACKAGE_REMOVED);
        pkgFilter.addAction(Intent.ACTION_PACKAGE_DATA_CLEARED);
        pkgFilter.addAction(Intent.ACTION_PACKAGE_ADDED);
        pkgFilter.addAction(Intent.ACTION_PACKAGE_CHANGED);
        pkgFilter.addDataScheme(PACKAGE_SCHEME);
        context.registerReceiver(mPackageChangedBroadcastReceiver, pkgFilter);

        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        mAudioManager.registerAudioPlaybackCallback(mAudioPlaybackCallback, new Handler(mLooper));

        mMediaSessionManager =
                (MediaSessionManager) context.getSystemService(Context.MEDIA_SESSION_SERVICE);
        mMediaSessionManager.addOnActiveSessionsChangedListener(
                mActiveSessionsChangedListener, null, new Handler(looper));
        mMediaSessionManager.addOnMediaKeyEventSessionChangedListener(
                mContext.getMainExecutor(), mMediaKeyEventSessionChangedListener);
    }

    void init(AvrcpTargetService.ListCallback callback) {
        Log.v(TAG, "Initializing MediaPlayerList");
        mCallback = callback;

        // Build the list of browsable players and afterwards, build the list of media players
        Intent intent = new Intent(android.service.media.MediaBrowserService.SERVICE_INTERFACE);
        List<ResolveInfo> playerList =
                mContext
                    .getApplicationContext()
                    .getPackageManager()
                    .queryIntentServices(intent, PackageManager.MATCH_ALL);

        mBrowsablePlayerConnector = BrowsablePlayerConnector.connectToPlayers(mContext, mLooper,
                playerList, (List<BrowsedPlayerWrapper> players) -> {
                Log.i(TAG, "init: Browsable Player list size is " + players.size());

                // Check to see if the list has been cleaned up before this completed
                if (mMediaSessionManager == null) {
                    return;
                }

                for (BrowsedPlayerWrapper wrapper : players) {
                    // Generate new id and add the browsable player
                    if (!havePlayerId(wrapper.getPackageName())) {
                        mMediaPlayerIds.put(wrapper.getPackageName(), getFreeMediaPlayerId());
                    }

                    d("Adding Browser Wrapper for " + wrapper.getPackageName() + " with id "
                            + mMediaPlayerIds.get(wrapper.getPackageName()));

                    mBrowsablePlayers.put(mMediaPlayerIds.get(wrapper.getPackageName()), wrapper);

                    wrapper.getFolderItems(wrapper.getRootId(),
                            (int status, String mediaId, List<ListItem> results) -> {
                                d("Got the contents for: " + mediaId + " : num results="
                                        + results.size());
                            });
                }

                // Construct the list of current players
                d("Initializing list of current media players");
                List<android.media.session.MediaController> controllers =
                        mMediaSessionManager.getActiveSessions(null);

                for (android.media.session.MediaController controller : controllers) {
                    addMediaPlayer(controller);
                }

                // If there were any active players and we don't already have one due to the Media
                // Framework Callbacks then set the highest priority one to active
                if (mActivePlayerId == 0 && mMediaPlayers.size() > 0) setActivePlayer(1);
            });
    }

    void cleanup() {
        mContext.unregisterReceiver(mPackageChangedBroadcastReceiver);

        mActivePlayerId = NO_ACTIVE_PLAYER;

        mMediaSessionManager.removeOnActiveSessionsChangedListener(mActiveSessionsChangedListener);
        mMediaSessionManager.removeOnMediaKeyEventSessionChangedListener(
                mMediaKeyEventSessionChangedListener);
        mMediaSessionManager = null;

        mAudioManager.unregisterAudioPlaybackCallback(mAudioPlaybackCallback);

        mMediaPlayerIds.clear();

        for (MediaPlayerWrapper player : mMediaPlayers.values()) {
            player.cleanup();
        }
        mMediaPlayers.clear();

        if (mBrowsablePlayerConnector != null) {
            mBrowsablePlayerConnector.cleanup();
        }
        for (BrowsedPlayerWrapper player : mBrowsablePlayers.values()) {
            player.disconnect();
        }
        mBrowsablePlayers.clear();
    }

    int getCurrentPlayerId() {
        return BLUETOOTH_PLAYER_ID;
    }

    int getFreeMediaPlayerId() {
        int id = 1;
        while (mMediaPlayerIds.containsValue(id)) {
            id++;
        }
        return id;
    }

    MediaPlayerWrapper getActivePlayer() {
        return mMediaPlayers.get(mActivePlayerId);
    }

    // In this case the displayed player is the Bluetooth Player, the number of items is equal
    // to the number of players. The root ID will always be empty string in this case as well.
    void getPlayerRoot(int playerId, GetPlayerRootCallback cb) {
        cb.run(playerId, playerId == BLUETOOTH_PLAYER_ID, "", mBrowsablePlayers.size());
    }

    // Return the "Bluetooth Player" as the only player always
    List<PlayerInfo> getMediaPlayerList() {
        PlayerInfo info = new PlayerInfo();
        info.id = BLUETOOTH_PLAYER_ID;
        info.name = BLUETOOTH_PLAYER_NAME;
        info.browsable = true;
        if (mBrowsablePlayers.size() == 0) {
            // Set Bluetooth Player as non-browable if there is not browsable player exist.
            info.browsable = false;
        }
        List<PlayerInfo> ret = new ArrayList<PlayerInfo>();
        ret.add(info);

        return ret;
    }

    @NonNull
    String getCurrentMediaId() {
        final MediaPlayerWrapper player = getActivePlayer();
        if (player == null) return "";

        final PlaybackState state = player.getPlaybackState();
        final List<Metadata> queue = player.getCurrentQueue();

        // Disable the now playing list if the player doesn't have a queue or provide an active
        // queue ID that can be used to determine the active song in the queue.
        if (state == null
                || state.getActiveQueueItemId() == MediaSession.QueueItem.UNKNOWN_ID
                || queue.size() == 0) {
            d("getCurrentMediaId: No active queue item Id sending empty mediaId: PlaybackState="
                     + state);
            return "";
        }

        return Util.NOW_PLAYING_PREFIX + state.getActiveQueueItemId();
    }

    @NonNull
    Metadata getCurrentSongInfo() {
        final MediaPlayerWrapper player = getActivePlayer();
        if (player == null) return Util.empty_data();

        return player.getCurrentMetadata();
    }

    PlaybackState getCurrentPlayStatus() {
        final MediaPlayerWrapper player = getActivePlayer();
        if (player == null) return null;

        PlaybackState state = player.getPlaybackState();
        if (mAudioPlaybackIsActive
                && (state == null || state.getState() != PlaybackState.STATE_PLAYING)) {
            return new PlaybackState.Builder()
                .setState(PlaybackState.STATE_PLAYING,
                          state == null ? 0 : state.getPosition(),
                          1.0f)
                .build();
        }
        return state;
    }

    @NonNull
    List<Metadata> getNowPlayingList() {
        // Only send the current song for the now playing if there is no active song. See
        // |getCurrentMediaId()| for reasons why there might be no active song.
        if (getCurrentMediaId().equals("")) {
            List<Metadata> ret = new ArrayList<Metadata>();
            Metadata data = getCurrentSongInfo();
            data.mediaId = "";
            ret.add(data);
            return ret;
        }

        return getActivePlayer().getCurrentQueue();
    }

    void playItem(int playerId, boolean nowPlaying, String mediaId) {
        if (nowPlaying) {
            playNowPlayingItem(mediaId);
        } else {
            playFolderItem(mediaId);
        }
    }

    private void playNowPlayingItem(String mediaId) {
        d("playNowPlayingItem: mediaId=" + mediaId);

        Pattern regex = Pattern.compile(NOW_PLAYING_ID_PATTERN);
        Matcher m = regex.matcher(mediaId);
        if (!m.find()) {
            // This should never happen since we control the media ID's reported
            Log.wtf(TAG, "playNowPlayingItem: Couldn't match mediaId to pattern: mediaId="
                    + mediaId);
        }

        long queueItemId = Long.parseLong(m.group(1));
        if (getActivePlayer() != null) {
            getActivePlayer().playItemFromQueue(queueItemId);
        }
    }

    private void playFolderItem(String mediaId) {
        d("playFolderItem: mediaId=" + mediaId);

        if (!mediaId.matches(BROWSE_ID_PATTERN)) {
            // This should never happen since we control the media ID's reported
            Log.wtf(TAG, "playFolderItem: mediaId didn't match pattern: mediaId=" + mediaId);
        }

        int playerIndex = Integer.parseInt(mediaId.substring(0, 2));
        String itemId = mediaId.substring(2);

        if (!haveMediaBrowser(playerIndex)) {
            e("playFolderItem: Do not have the a browsable player with ID " + playerIndex);
            return;
        }

        mBrowsablePlayers.get(playerIndex).playItem(itemId);
    }

    void getFolderItemsMediaPlayerList(GetFolderItemsCallback cb) {
        d("getFolderItemsMediaPlayerList: Sending Media Player list for root directory");

        ArrayList<ListItem> playerList = new ArrayList<ListItem>();
        for (BrowsedPlayerWrapper player : mBrowsablePlayers.values()) {

            String displayName = Util.getDisplayName(mContext, player.getPackageName());
            int id = mMediaPlayerIds.get(player.getPackageName());

            d("getFolderItemsMediaPlayerList: Adding player " + displayName);
            Folder playerFolder = new Folder(String.format("%02d", id), false, displayName);
            playerList.add(new ListItem(playerFolder));
        }
        cb.run("", playerList);
        return;
    }

    void getFolderItems(int playerId, String mediaId, GetFolderItemsCallback cb) {
        // The playerId is unused since we always assume the remote device is using the
        // Bluetooth Player.
        d("getFolderItems(): playerId=" + playerId + ", mediaId=" + mediaId);

        // The device is requesting the content of the root folder. This folder contains a list of
        // Browsable Media Players displayed as folders with their contents contained within.
        if (mediaId.equals("")) {
            getFolderItemsMediaPlayerList(cb);
            return;
        }

        if (!mediaId.matches(BROWSE_ID_PATTERN)) {
            // This should never happen since we control the media ID's reported
            Log.wtf(TAG, "getFolderItems: mediaId didn't match pattern: mediaId=" + mediaId);
        }

        int playerIndex = Integer.parseInt(mediaId.substring(0, 2));
        String itemId = mediaId.substring(2);

        // TODO (apanicke): Add timeouts for looking up folder items since media browsers don't
        // have to respond.
        if (haveMediaBrowser(playerIndex)) {
            BrowsedPlayerWrapper wrapper = mBrowsablePlayers.get(playerIndex);
            if (itemId.equals("")) {
                Log.i(TAG, "Empty media id, getting the root for "
                        + wrapper.getPackageName());
                itemId = wrapper.getRootId();
            }

            wrapper.getFolderItems(itemId, (status, id, results) -> {
                if (status != BrowsedPlayerWrapper.STATUS_SUCCESS) {
                    cb.run(mediaId, new ArrayList<ListItem>());
                    return;
                }

                String playerPrefix = String.format("%02d", playerIndex);
                for (ListItem item : results) {
                    if (item.isFolder) {
                        item.folder.mediaId = playerPrefix.concat(item.folder.mediaId);
                    } else {
                        item.song.mediaId = playerPrefix.concat(item.song.mediaId);
                    }
                }
                cb.run(mediaId, results);
            });
            return;
        } else {
            cb.run(mediaId, new ArrayList<ListItem>());
        }
    }

    @VisibleForTesting
    int addMediaPlayer(MediaController controller) {
        // Each new player has an ID of 1 plus the highest ID. The ID 0 is reserved to signify that
        // there is no active player. If we already have a browsable player for the package, reuse
        // that key.
        String packageName = controller.getPackageName();
        if (!havePlayerId(packageName)) {
            mMediaPlayerIds.put(packageName, getFreeMediaPlayerId());
        }

        int playerId = mMediaPlayerIds.get(packageName);

        // If we already have a controller for the package, then update it with this new controller
        // as the old controller has probably gone stale.
        if (haveMediaPlayer(playerId)) {
            d("Already have a controller for the player: " + packageName + ", updating instead");
            MediaPlayerWrapper player = mMediaPlayers.get(playerId);
            player.updateMediaController(controller);

            // If the media controller we updated was the active player check if the media updated
            if (playerId == mActivePlayerId) {
                sendMediaUpdate(getActivePlayer().getCurrentMediaData());
            }

            return playerId;
        }

        MediaPlayerWrapper newPlayer = MediaPlayerWrapperFactory.wrap(
                controller,
                mLooper);

        Log.i(TAG, "Adding wrapped media player: " + packageName + " at key: "
                + mMediaPlayerIds.get(controller.getPackageName()));

        mMediaPlayers.put(playerId, newPlayer);
        return playerId;
    }

    // Adds the controller to the MediaPlayerList or updates the controller if we already had
    // a controller for a package. Returns the new ID of the controller where its added or its
    // previous value if it already existed. Returns -1 if the controller passed in is invalid
    int addMediaPlayer(android.media.session.MediaController controller) {
        if (controller == null) {
            e("Trying to add a null MediaController");
            return -1;
        }

        return addMediaPlayer(MediaControllerFactory.wrap(controller));
    }

    boolean havePlayerId(String packageName) {
        if (packageName == null) return false;
        return mMediaPlayerIds.containsKey(packageName);
    }

    boolean haveMediaPlayer(String packageName) {
        if (!havePlayerId(packageName)) return false;
        int playerId = mMediaPlayerIds.get(packageName);
        return mMediaPlayers.containsKey(playerId);
    }

    boolean haveMediaPlayer(int playerId) {
        return mMediaPlayers.containsKey(playerId);
    }

    boolean haveMediaBrowser(int playerId) {
        return mBrowsablePlayers.containsKey(playerId);
    }

    void removeMediaPlayer(int playerId) {
        if (!haveMediaPlayer(playerId)) {
            e("Trying to remove nonexistent media player: " + playerId);
            return;
        }

        // If we removed the active player, set no player as active until the Media Framework
        // tells us otherwise
        if (playerId == mActivePlayerId && playerId != NO_ACTIVE_PLAYER) {
            getActivePlayer().unregisterCallback();
            mActivePlayerId = NO_ACTIVE_PLAYER;
            List<Metadata> queue = new ArrayList<Metadata>();
            queue.add(Util.empty_data());
            MediaData newData = new MediaData(
                    Util.empty_data(),
                    null,
                    queue
                );

            sendMediaUpdate(newData);
        }

        final MediaPlayerWrapper wrapper = mMediaPlayers.get(playerId);
        d("Removing media player " + wrapper.getPackageName());
        mMediaPlayers.remove(playerId);
        if (!haveMediaBrowser(playerId)) {
            d(wrapper.getPackageName() + " doesn't have a browse service. Recycle player ID.");
            mMediaPlayerIds.remove(wrapper.getPackageName());
        }
        wrapper.cleanup();
    }

    void setActivePlayer(int playerId) {
        if (!haveMediaPlayer(playerId)) {
            e("Player doesn't exist in list(): " + playerId);
            return;
        }

        if (playerId == mActivePlayerId) {
            Log.w(TAG, getActivePlayer().getPackageName() + " is already the active player");
            return;
        }

        if (mActivePlayerId != NO_ACTIVE_PLAYER) getActivePlayer().unregisterCallback();

        mActivePlayerId = playerId;
        getActivePlayer().registerCallback(mMediaPlayerCallback);
        mActivePlayerLogger.logd(TAG, "setActivePlayer(): setting player to "
                + getActivePlayer().getPackageName());

        // Ensure that metadata is synced on the new player
        if (!getActivePlayer().isMetadataSynced()) {
            Log.w(TAG, "setActivePlayer(): Metadata not synced on new player");
            return;
        }

        if (Utils.isPtsTestMode()) {
            sendFolderUpdate(true, true, false);
        }

        MediaData data = getActivePlayer().getCurrentMediaData();
        if (mAudioPlaybackIsActive) {
            data.state = mCurrMediaData.state;
            Log.d(TAG, "setActivePlayer mAudioPlaybackIsActive=true, state=" + data.state);
        }
        sendMediaUpdate(data);
    }

    // TODO (apanicke): Add logging for media key events in dumpsys
    void sendMediaKeyEvent(int key, boolean pushed) {
        d("sendMediaKeyEvent: key=" + key + " pushed=" + pushed);
        int action = pushed ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP;
        KeyEvent event = new KeyEvent(action, AvrcpPassthrough.toKeyCode(key));
        mMediaSessionManager.dispatchMediaKeyEvent(event);
    }

    private void sendFolderUpdate(boolean availablePlayers, boolean addressedPlayers,
            boolean uids) {
        d("sendFolderUpdate");
        if (mCallback == null) {
            return;
        }

        mCallback.run(availablePlayers, addressedPlayers, uids);
    }

    private void sendMediaUpdate(MediaData data) {
        d("sendMediaUpdate");
        if (mCallback == null) {
            return;
        }

        // Always have items in the queue
        if (data.queue.size() == 0) {
            Log.i(TAG, "sendMediaUpdate: Creating a one item queue for a player with no queue");
            data.queue.add(data.metadata);
        }

        Log.d(TAG, "sendMediaUpdate state=" + data.state);
        mCurrMediaData = data;
        mCallback.run(data);
    }

    private final MediaSessionManager.OnActiveSessionsChangedListener
            mActiveSessionsChangedListener =
            new MediaSessionManager.OnActiveSessionsChangedListener() {
        @Override
        public void onActiveSessionsChanged(
                List<android.media.session.MediaController> newControllers) {
            synchronized (MediaPlayerList.this) {
                Log.v(TAG, "onActiveSessionsChanged: number of controllers: "
                        + newControllers.size());
                if (newControllers.size() == 0) return;

                // Apps are allowed to have multiple MediaControllers. If an app does have
                // multiple controllers then newControllers contains them in highest
                // priority order. Since we only want to keep the highest priority one,
                // we keep track of which controllers we updated and skip over ones
                // we've already looked at.
                HashSet<String> addedPackages = new HashSet<String>();

                for (int i = 0; i < newControllers.size(); i++) {
                    Log.d(TAG, "onActiveSessionsChanged: controller: "
                            + newControllers.get(i).getPackageName());
                    if (addedPackages.contains(newControllers.get(i).getPackageName())) {
                        continue;
                    }

                    addedPackages.add(newControllers.get(i).getPackageName());
                    addMediaPlayer(newControllers.get(i));
                }
            }
        }
    };

    // TODO (apanicke): Write a test that tests uninstalling the active session
    private final BroadcastReceiver mPackageChangedBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.v(TAG, "mPackageChangedBroadcastReceiver: action: " + action);

            if (action.equals(Intent.ACTION_PACKAGE_REMOVED)
                    || action.equals(Intent.ACTION_PACKAGE_DATA_CLEARED)) {
                if (intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) return;

                String packageName = intent.getData().getSchemeSpecificPart();
                if (haveMediaPlayer(packageName)) {
                    removeMediaPlayer(mMediaPlayerIds.get(packageName));
                }
            } else if (action.equals(Intent.ACTION_PACKAGE_ADDED)
                    || action.equals(Intent.ACTION_PACKAGE_CHANGED)) {
                String packageName = intent.getData().getSchemeSpecificPart();
                if (packageName != null) {
                    if (DEBUG) Log.d(TAG, "Name of package changed: " + packageName);
                    // TODO (apanicke): Handle either updating or adding the new package.
                    // Check if its browsable and send the UIDS changed to update the
                    // root folder
                }
            }
        }
    };

    void updateMediaForAudioPlayback() {
        MediaData currMediaData = null;
        PlaybackState currState = null;
        if (getActivePlayer() == null) {
            Log.d(TAG, "updateMediaForAudioPlayback: no active player");
            PlaybackState.Builder builder = new PlaybackState.Builder()
                    .setState(PlaybackState.STATE_STOPPED, 0L, 0L);
            List<Metadata> queue = new ArrayList<Metadata>();
            queue.add(Util.empty_data());
            currMediaData = new MediaData(
                    Util.empty_data(),
                    builder.build(),
                    queue
                );
        } else {
            currMediaData = getActivePlayer().getCurrentMediaData();
            currState = currMediaData.state;
        }

        if (currState != null
                && currState.getState() == PlaybackState.STATE_PLAYING) {
            Log.i(TAG, "updateMediaForAudioPlayback: Active player is playing, drop it");
            return;
        }

        if (mAudioPlaybackIsActive) {
            PlaybackState.Builder builder = new PlaybackState.Builder()
                    .setState(PlaybackState.STATE_PLAYING,
                        currState == null ? 0 : currState.getPosition(),
                        1.0f);
            currMediaData.state = builder.build();
        }
        mAudioPlaybackStateLogger.logd(TAG, "updateMediaForAudioPlayback: update state="
                + currMediaData.state);
        sendMediaUpdate(currMediaData);
    }

    @VisibleForTesting
    void injectAudioPlaybacActive(boolean isActive) {
        mAudioPlaybackIsActive = isActive;
        updateMediaForAudioPlayback();
    }

    private final AudioManager.AudioPlaybackCallback mAudioPlaybackCallback =
            new AudioManager.AudioPlaybackCallback() {
        @Override
        public void onPlaybackConfigChanged(List<AudioPlaybackConfiguration> configs) {
            if (configs == null) {
                return;
            }
            boolean isActive = false;
            AudioPlaybackConfiguration activeConfig = null;
            for (AudioPlaybackConfiguration config : configs) {
                if (config.isActive() && (config.getAudioAttributes().getUsage()
                            == AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
                        && (config.getAudioAttributes().getContentType()
                            == AudioAttributes.CONTENT_TYPE_SPEECH)) {
                    activeConfig = config;
                    isActive = true;
                }
            }
            if (isActive != mAudioPlaybackIsActive) {
                mAudioPlaybackStateLogger.logd(DEBUG, TAG, "onPlaybackConfigChanged: "
                        + (mAudioPlaybackIsActive ? "Active" : "Non-active") + " -> "
                        + (isActive ? "Active" : "Non-active"));
                if (isActive) {
                    mAudioPlaybackStateLogger.logd(DEBUG, TAG, "onPlaybackConfigChanged: "
                            + "active config: " + activeConfig);
                }
                mAudioPlaybackIsActive = isActive;
                updateMediaForAudioPlayback();
            }
        }
    };

    private final MediaPlayerWrapper.Callback mMediaPlayerCallback =
            new MediaPlayerWrapper.Callback() {
        @Override
        public void mediaUpdatedCallback(MediaData data) {
            if (data.metadata == null) {
                Log.d(TAG, "mediaUpdatedCallback(): metadata is null");
                return;
            }

            if (data.state == null) {
                Log.w(TAG, "mediaUpdatedCallback(): Tried to update with null state");
                return;
            }

            if (mAudioPlaybackIsActive && (data.state.getState() != PlaybackState.STATE_PLAYING)) {
                Log.d(TAG, "Some audio playbacks are still active, drop it");
                return;
            }
            sendMediaUpdate(data);
        }

        @Override
        public void sessionUpdatedCallback(String packageName) {
            if (haveMediaPlayer(packageName)) {
                Log.d(TAG, "sessionUpdatedCallback(): packageName: " + packageName);
                removeMediaPlayer(mMediaPlayerIds.get(packageName));
            }
        }
    };

    private final MediaSessionManager.OnMediaKeyEventSessionChangedListener
            mMediaKeyEventSessionChangedListener =
            new MediaSessionManager.OnMediaKeyEventSessionChangedListener() {
                @Override
                public void onMediaKeyEventSessionChanged(String packageName,
                        MediaSession.Token token) {
                    if (mMediaSessionManager == null) {
                        Log.w(TAG, "onMediaKeyEventSessionChanged(): Unexpected callback "
                                + "from the MediaSessionManager, pkg" + packageName + ", token="
                                + token);
                        return;
                    }
                    if (TextUtils.isEmpty(packageName)) {
                        return;
                    }
                    if (token != null) {
                        android.media.session.MediaController controller =
                                new android.media.session.MediaController(mContext, token);
                        if (!haveMediaPlayer(controller.getPackageName())) {
                            // Since we have a controller, we can try to to recover by adding the
                            // player and then setting it as active.
                            Log.w(TAG, "onMediaKeyEventSessionChanged(Token): Addressed Player "
                                    + "changed to a player we didn't have a session for");
                            addMediaPlayer(controller);
                        }

                        Log.i(TAG, "onMediaKeyEventSessionChanged: token="
                                + controller.getPackageName());
                        setActivePlayer(mMediaPlayerIds.get(controller.getPackageName()));
                    } else {
                        if (!haveMediaPlayer(packageName)) {
                            e("onMediaKeyEventSessionChanged(PackageName): Media key event session "
                                    + "changed to a player we don't have a session for");
                            return;
                        }

                        Log.i(TAG, "onMediaKeyEventSessionChanged: packageName=" + packageName);
                        setActivePlayer(mMediaPlayerIds.get(packageName));
                    }
                }
            };


    void dump(StringBuilder sb) {
        sb.append("List of MediaControllers: size=" + mMediaPlayers.size() + "\n");
        for (int id : mMediaPlayers.keySet()) {
            if (id == mActivePlayerId) {
                sb.append("<Active> ");
            }
            MediaPlayerWrapper player = mMediaPlayers.get(id);
            sb.append("  Media Player " + id + ": " + player.getPackageName() + "\n");
            sb.append(player.toString().replaceAll("(?m)^", "  "));
            sb.append("\n");
        }

        sb.append("List of Browsers: size=" + mBrowsablePlayers.size() + "\n");
        for (BrowsedPlayerWrapper player : mBrowsablePlayers.values()) {
            sb.append(player.toString().replaceAll("(?m)^", "  "));
            sb.append("\n");
        }

        mActivePlayerLogger.dump(sb);
        sb.append("\n");
        mAudioPlaybackStateLogger.dump(sb);
        sb.append("\n");
        // TODO (apanicke): Add last sent data
    }

    private static void e(String message) {
        if (sTesting) {
            Log.wtf(TAG, message);
        } else {
            Log.e(TAG, message);
        }
    }

    private static void d(String message) {
        if (DEBUG) {
            Log.d(TAG, message);
        }
    }
}
