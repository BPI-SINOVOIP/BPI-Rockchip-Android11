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

import android.bluetooth.BluetoothAvrcpController;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Message;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;
import android.util.SparseArray;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.statemachine.State;
import com.android.bluetooth.statemachine.StateMachine;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Provides Bluetooth AVRCP Controller State Machine responsible for all remote control connections
 * and interactions with a remote controlable device.
 */
class AvrcpControllerStateMachine extends StateMachine {
    static final String TAG = "AvrcpControllerStateMachine";
    static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    //0->99 Events from Outside
    public static final int CONNECT = 1;
    public static final int DISCONNECT = 2;

    //100->199 Internal Events
    protected static final int CLEANUP = 100;
    private static final int CONNECT_TIMEOUT = 101;
    static final int MESSAGE_INTERNAL_ABS_VOL_TIMEOUT = 102;

    //200->299 Events from Native
    static final int STACK_EVENT = 200;
    static final int MESSAGE_INTERNAL_CMD_TIMEOUT = 201;

    static final int MESSAGE_PROCESS_SET_ABS_VOL_CMD = 203;
    static final int MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION = 204;
    static final int MESSAGE_PROCESS_TRACK_CHANGED = 205;
    static final int MESSAGE_PROCESS_PLAY_POS_CHANGED = 206;
    static final int MESSAGE_PROCESS_PLAY_STATUS_CHANGED = 207;
    static final int MESSAGE_PROCESS_VOLUME_CHANGED_NOTIFICATION = 208;
    static final int MESSAGE_PROCESS_GET_FOLDER_ITEMS = 209;
    static final int MESSAGE_PROCESS_GET_FOLDER_ITEMS_OUT_OF_RANGE = 210;
    static final int MESSAGE_PROCESS_GET_PLAYER_ITEMS = 211;
    static final int MESSAGE_PROCESS_FOLDER_PATH = 212;
    static final int MESSAGE_PROCESS_SET_BROWSED_PLAYER = 213;
    static final int MESSAGE_PROCESS_SET_ADDRESSED_PLAYER = 214;
    static final int MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED = 215;
    static final int MESSAGE_PROCESS_NOW_PLAYING_CONTENTS_CHANGED = 216;
    static final int MESSAGE_PROCESS_SUPPORTED_APPLICATION_SETTINGS = 217;
    static final int MESSAGE_PROCESS_CURRENT_APPLICATION_SETTINGS = 218;
    static final int MESSAGE_PROCESS_AVAILABLE_PLAYER_CHANGED = 219;
    static final int MESSAGE_PROCESS_RECEIVED_COVER_ART_PSM = 220;

    //300->399 Events for Browsing
    static final int MESSAGE_GET_FOLDER_ITEMS = 300;
    static final int MESSAGE_PLAY_ITEM = 301;
    static final int MSG_AVRCP_PASSTHRU = 302;
    static final int MSG_AVRCP_SET_SHUFFLE = 303;
    static final int MSG_AVRCP_SET_REPEAT = 304;

    //400->499 Events for Cover Artwork
    static final int MESSAGE_PROCESS_IMAGE_DOWNLOADED = 400;

    /*
     * Base value for absolute volume from JNI
     */
    private static final int ABS_VOL_BASE = 127;

    /*
     * Notification types for Avrcp protocol JNI.
     */
    private static final byte NOTIFICATION_RSP_TYPE_INTERIM = 0x00;
    private static final byte NOTIFICATION_RSP_TYPE_CHANGED = 0x01;

    private static BluetoothDevice sActiveDevice;
    private final AudioManager mAudioManager;
    private final boolean mIsVolumeFixed;

    protected final BluetoothDevice mDevice;
    protected final byte[] mDeviceAddress;
    protected final AvrcpControllerService mService;
    protected int mCoverArtPsm;
    protected final AvrcpCoverArtManager mCoverArtManager;
    protected final Disconnected mDisconnected;
    protected final Connecting mConnecting;
    protected final Connected mConnected;
    protected final Disconnecting mDisconnecting;

    protected int mMostRecentState = BluetoothProfile.STATE_DISCONNECTED;

    boolean mRemoteControlConnected = false;
    boolean mBrowsingConnected = false;
    final BrowseTree mBrowseTree;
    private AvrcpPlayer mAddressedPlayer = new AvrcpPlayer();
    private int mAddressedPlayerId = -1;
    private SparseArray<AvrcpPlayer> mAvailablePlayerList = new SparseArray<AvrcpPlayer>();
    private int mVolumeChangedNotificationsToIgnore = 0;
    private int mVolumeNotificationLabel = -1;

    GetFolderList mGetFolderList = null;

    //Number of items to get in a single fetch
    static final int ITEM_PAGE_SIZE = 20;
    static final int CMD_TIMEOUT_MILLIS = 10000;
    static final int ABS_VOL_TIMEOUT_MILLIS = 1000; //1s

    AvrcpControllerStateMachine(BluetoothDevice device, AvrcpControllerService service) {
        super(TAG);
        mDevice = device;
        mDeviceAddress = Utils.getByteAddress(mDevice);
        mService = service;
        mCoverArtPsm = 0;
        mCoverArtManager = service.getCoverArtManager();
        logD(device.toString());

        mBrowseTree = new BrowseTree(mDevice);
        mDisconnected = new Disconnected();
        mConnecting = new Connecting();
        mConnected = new Connected();
        mDisconnecting = new Disconnecting();

        addState(mDisconnected);
        addState(mConnecting);
        addState(mConnected);
        addState(mDisconnecting);

        mGetFolderList = new GetFolderList();
        addState(mGetFolderList, mConnected);
        mAudioManager = (AudioManager) service.getSystemService(Context.AUDIO_SERVICE);
        mIsVolumeFixed = mAudioManager.isVolumeFixed();

        setInitialState(mDisconnected);
    }

    BrowseTree.BrowseNode findNode(String parentMediaId) {
        logD("FindNode");
        return mBrowseTree.findBrowseNodeByID(parentMediaId);
    }

    /**
     * Get the current connection state
     *
     * @return current State
     */
    public int getState() {
        return mMostRecentState;
    }

    /**
     * Get the underlying device tracked by this state machine
     *
     * @return device in focus
     */
    public synchronized BluetoothDevice getDevice() {
        return mDevice;
    }

    /**
     * send the connection event asynchronously
     */
    public boolean connect(StackEvent event) {
        if (event.mBrowsingConnected) {
            onBrowsingConnected();
        }
        mRemoteControlConnected = event.mRemoteControlConnected;
        sendMessage(CONNECT);
        return true;
    }

    /**
     * send the Disconnect command asynchronously
     */
    public void disconnect() {
        sendMessage(DISCONNECT);
    }

    /**
     * Get the current playing track
     */
    public AvrcpItem getCurrentTrack() {
        return mAddressedPlayer.getCurrentTrack();
    }

    /**
     * Dump the current State Machine to the string builder.
     *
     * @param sb output string
     */
    public void dump(StringBuilder sb) {
        ProfileService.println(sb, "mDevice: " + mDevice.getAddress() + "("
                + mDevice.getName() + ") " + this.toString());
        ProfileService.println(sb, "isActive: " + isActive());
    }

    @VisibleForTesting
    boolean isActive() {
        return mDevice == sActiveDevice;
    }

    /*
     * requestActive
     *
     * Set the current device active if nothing an already connected device isn't playing
     */
    private boolean requestActive() {
        if (sActiveDevice == null
                || BluetoothMediaBrowserService.getPlaybackState()
                != PlaybackStateCompat.STATE_PLAYING) {
            return setActive(true);
        }
        return false;
    }

    /**
     * Attempt to set the active status for this device
     */
    boolean setActive(boolean becomeActive) {
        logD("setActive(" + becomeActive + ")");
        if (becomeActive) {
            if (isActive()) {
                return true;
            }

            A2dpSinkService a2dpSinkService = A2dpSinkService.getA2dpSinkService();
            if (a2dpSinkService == null) {
                return false;
            }

            if (a2dpSinkService.setActiveDeviceNative(mDeviceAddress)) {
                sActiveDevice = mDevice;
                BluetoothMediaBrowserService.addressedPlayerChanged(mSessionCallbacks);
                BluetoothMediaBrowserService.notifyChanged(mAddressedPlayer.getPlaybackState());
                BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mNowPlayingNode);
            }
            return mDevice == sActiveDevice;
        } else if (isActive()) {
            sActiveDevice = null;
            BluetoothMediaBrowserService.trackChanged(null);
            BluetoothMediaBrowserService.addressedPlayerChanged(null);
        }
        return true;
    }

    @Override
    protected void unhandledMessage(Message msg) {
        Log.w(TAG, "Unhandled message in state " + getCurrentState() + "msg.what=" + msg.what);
    }

    private static void logD(String message) {
        if (DBG) {
            Log.d(TAG, message);
        }
    }

    synchronized void onBrowsingConnected() {
        if (mBrowsingConnected) return;
        mService.sBrowseTree.mRootNode.addChild(mBrowseTree.mRootNode);
        BluetoothMediaBrowserService.notifyChanged(mService
                .sBrowseTree.mRootNode);
        mBrowsingConnected = true;
    }

    synchronized void onBrowsingDisconnected() {
        if (!mBrowsingConnected) return;
        mAddressedPlayer.setPlayStatus(PlaybackStateCompat.STATE_ERROR);
        AvrcpItem previousTrack = mAddressedPlayer.getCurrentTrack();
        String previousTrackUuid = previousTrack != null ? previousTrack.getCoverArtUuid() : null;
        mAddressedPlayer.updateCurrentTrack(null);
        mBrowseTree.mNowPlayingNode.setCached(false);
        if (isActive()) {
            BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mNowPlayingNode);
        }
        mService.sBrowseTree.mRootNode.removeChild(
                mBrowseTree.mRootNode);
        BluetoothMediaBrowserService.notifyChanged(mService
                .sBrowseTree.mRootNode);
        removeUnusedArtwork(previousTrackUuid);
        removeUnusedArtworkFromBrowseTree();
        mBrowsingConnected = false;
    }

    synchronized void connectCoverArt() {
        // Called from "connected" state, which assumes either control or browse is connected
        if (mCoverArtManager != null && mCoverArtPsm != 0
                && mCoverArtManager.getState(mDevice) != BluetoothProfile.STATE_CONNECTED) {
            logD("Attempting to connect to AVRCP BIP, psm: " + mCoverArtPsm);
            mCoverArtManager.connect(mDevice, /* psm */ mCoverArtPsm);
        }
    }

    synchronized void refreshCoverArt() {
        if (mCoverArtManager != null && mCoverArtPsm != 0
                && mCoverArtManager.getState(mDevice) == BluetoothProfile.STATE_CONNECTED) {
            logD("Attempting to refresh AVRCP BIP OBEX session, psm: " + mCoverArtPsm);
            mCoverArtManager.refreshSession(mDevice);
        }
    }

    synchronized void disconnectCoverArt() {
        // Safe to call even if we're not connected
        if (mCoverArtManager != null) {
            logD("Disconnect BIP cover artwork");
            mCoverArtManager.disconnect(mDevice);
        }
    }

    /**
     * Remove an unused cover art image from storage if it's unused by the browse tree and the
     * current track.
     */
    synchronized void removeUnusedArtwork(String previousTrackUuid) {
        logD("removeUnusedArtwork(" + previousTrackUuid + ")");
        if (mCoverArtManager == null) return;
        AvrcpItem currentTrack = getCurrentTrack();
        String currentTrackUuid = currentTrack != null ? currentTrack.getCoverArtUuid() : null;
        if (previousTrackUuid != null) {
            if (!previousTrackUuid.equals(currentTrackUuid)
                    && mBrowseTree.getNodesUsingCoverArt(previousTrackUuid).isEmpty()) {
                mCoverArtManager.removeImage(mDevice, previousTrackUuid);
            }
        }
    }

    /**
     * Queries the browse tree for unused uuids and removes the associated images from storage
     * if the uuid is not used by the current track.
     */
    synchronized void removeUnusedArtworkFromBrowseTree() {
        logD("removeUnusedArtworkFromBrowseTree()");
        if (mCoverArtManager == null) return;
        AvrcpItem currentTrack = getCurrentTrack();
        String currentTrackUuid = currentTrack != null ? currentTrack.getCoverArtUuid() : null;
        ArrayList<String> unusedArtwork = mBrowseTree.getAndClearUnusedCoverArt();
        for (String uuid : unusedArtwork) {
            if (!uuid.equals(currentTrackUuid)) {
                mCoverArtManager.removeImage(mDevice, uuid);
            }
        }
    }

    private void notifyChanged(BrowseTree.BrowseNode node) {
        // We should only notify now playing content updates if we're the active device. VFS
        // updates are fine at any time
        int scope = node.getScope();
        if (scope != AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING
                || (scope == AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING
                && isActive())) {
            BluetoothMediaBrowserService.notifyChanged(node);
        }
    }

    private void notifyChanged(PlaybackStateCompat state) {
        if (isActive()) {
            BluetoothMediaBrowserService.notifyChanged(state);
        }
    }

    void requestContents(BrowseTree.BrowseNode node) {
        sendMessage(MESSAGE_GET_FOLDER_ITEMS, node);
        logD("Fetching " + node);
    }

    public void playItem(BrowseTree.BrowseNode node) {
        sendMessage(MESSAGE_PLAY_ITEM, node);
    }

    void nowPlayingContentChanged() {
        mBrowseTree.mNowPlayingNode.setCached(false);
        removeUnusedArtworkFromBrowseTree();
        sendMessage(MESSAGE_GET_FOLDER_ITEMS, mBrowseTree.mNowPlayingNode);
    }

    protected class Disconnected extends State {
        @Override
        public void enter() {
            logD("Enter Disconnected");
            if (mMostRecentState != BluetoothProfile.STATE_DISCONNECTED) {
                sendMessage(CLEANUP);
            }
            broadcastConnectionStateChanged(BluetoothProfile.STATE_DISCONNECTED);
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case MESSAGE_PROCESS_RECEIVED_COVER_ART_PSM:
                    mCoverArtPsm = message.arg1;
                    break;
                case CONNECT:
                    logD("Connect");
                    transitionTo(mConnecting);
                    break;
                case CLEANUP:
                    mService.removeStateMachine(AvrcpControllerStateMachine.this);
                    break;
            }
            return true;
        }
    }

    protected class Connecting extends State {
        @Override
        public void enter() {
            logD("Enter Connecting");
            broadcastConnectionStateChanged(BluetoothProfile.STATE_CONNECTING);
            transitionTo(mConnected);
        }
    }


    class Connected extends State {
        private static final String STATE_TAG = "Avrcp.ConnectedAvrcpController";
        private int mCurrentlyHeldKey = 0;

        @Override
        public void enter() {
            if (mMostRecentState == BluetoothProfile.STATE_CONNECTING) {
                requestActive();
                broadcastConnectionStateChanged(BluetoothProfile.STATE_CONNECTED);
                connectCoverArt(); // only works if we have a valid PSM
            } else {
                logD("ReEnteringConnected");
            }
            super.enter();
        }

        @Override
        public boolean processMessage(Message msg) {
            logD(STATE_TAG + " processMessage " + msg.what);
            switch (msg.what) {
                case MESSAGE_PROCESS_SET_ABS_VOL_CMD:
                    mVolumeChangedNotificationsToIgnore++;
                    removeMessages(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT);
                    sendMessageDelayed(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT,
                            ABS_VOL_TIMEOUT_MILLIS);
                    handleAbsVolumeRequest(msg.arg1, msg.arg2);
                    return true;

                case MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION:
                    mVolumeNotificationLabel = msg.arg1;
                    mService.sendRegisterAbsVolRspNative(mDeviceAddress,
                            NOTIFICATION_RSP_TYPE_INTERIM,
                            getAbsVolume(), mVolumeNotificationLabel);
                    return true;

                case MESSAGE_GET_FOLDER_ITEMS:
                    transitionTo(mGetFolderList);
                    return true;

                case MESSAGE_PLAY_ITEM:
                    //Set Addressed Player
                    processPlayItem((BrowseTree.BrowseNode) msg.obj);
                    return true;

                case MSG_AVRCP_PASSTHRU:
                    passThru(msg.arg1);
                    return true;

                case MSG_AVRCP_SET_REPEAT:
                    setRepeat(msg.arg1);
                    return true;

                case MSG_AVRCP_SET_SHUFFLE:
                    setShuffle(msg.arg1);
                    return true;

                case MESSAGE_PROCESS_TRACK_CHANGED:
                    AvrcpItem track = (AvrcpItem) msg.obj;
                    AvrcpItem previousTrack = mAddressedPlayer.getCurrentTrack();
                    downloadImageIfNeeded(track);
                    mAddressedPlayer.updateCurrentTrack(track);
                    if (isActive()) {
                        BluetoothMediaBrowserService.trackChanged(track);
                    }
                    if (previousTrack != null) {
                        removeUnusedArtwork(previousTrack.getCoverArtUuid());
                        removeUnusedArtworkFromBrowseTree();
                    }
                    return true;

                case MESSAGE_PROCESS_PLAY_STATUS_CHANGED:
                    mAddressedPlayer.setPlayStatus(msg.arg1);
                    if (!isActive()) {
                        sendMessage(MSG_AVRCP_PASSTHRU,
                                AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE);
                        return true;
                    }

                    PlaybackStateCompat playbackState = mAddressedPlayer.getPlaybackState();
                    BluetoothMediaBrowserService.notifyChanged(playbackState);

                    int focusState = AudioManager.ERROR;
                    A2dpSinkService a2dpSinkService = A2dpSinkService.getA2dpSinkService();
                    if (a2dpSinkService != null) {
                        focusState = a2dpSinkService.getFocusState();
                    }

                    if (focusState == AudioManager.ERROR) {
                        sendMessage(MSG_AVRCP_PASSTHRU,
                                AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE);
                        return true;
                    }

                    if (playbackState.getState() == PlaybackStateCompat.STATE_PLAYING
                            && focusState == AudioManager.AUDIOFOCUS_NONE) {
                        if (shouldRequestFocus()) {
                            mSessionCallbacks.onPrepare();
                        } else {
                            sendMessage(MSG_AVRCP_PASSTHRU,
                                    AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE);
                        }
                    }
                    return true;

                case MESSAGE_PROCESS_PLAY_POS_CHANGED:
                    if (msg.arg2 != -1) {
                        mAddressedPlayer.setPlayTime(msg.arg2);
                        notifyChanged(mAddressedPlayer.getPlaybackState());
                    }
                    return true;

                case MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED:
                    mAddressedPlayerId = msg.arg1;
                    logD("AddressedPlayer = " + mAddressedPlayerId);

                    // The now playing list is tied to the addressed player by specification in
                    // AVRCP 5.9.1. A new addressed player means our now playing content is now
                    // invalid
                    mBrowseTree.mNowPlayingNode.setCached(false);
                    if (isActive()) {
                        BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mNowPlayingNode);
                    }

                    AvrcpPlayer updatedPlayer = mAvailablePlayerList.get(mAddressedPlayerId);
                    if (updatedPlayer != null) {
                        mAddressedPlayer = updatedPlayer;
                        // If the new player supports the now playing feature then fetch it
                        if (mAddressedPlayer.supportsFeature(AvrcpPlayer.FEATURE_NOW_PLAYING)) {
                            sendMessage(MESSAGE_GET_FOLDER_ITEMS, mBrowseTree.mNowPlayingNode);
                        }
                        logD("AddressedPlayer = " + mAddressedPlayer.getName());
                    } else {
                        logD("Addressed player changed to unknown ID=" + mAddressedPlayerId);
                        mBrowseTree.mRootNode.setCached(false);
                        mBrowseTree.mRootNode.setExpectedChildren(255);
                        BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mRootNode);
                    }
                    removeUnusedArtworkFromBrowseTree();
                    return true;

                case MESSAGE_PROCESS_SUPPORTED_APPLICATION_SETTINGS:
                    mAddressedPlayer.setSupportedPlayerApplicationSettings(
                            (PlayerApplicationSettings) msg.obj);
                    notifyChanged(mAddressedPlayer.getPlaybackState());
                    return true;

                case MESSAGE_PROCESS_CURRENT_APPLICATION_SETTINGS:
                    mAddressedPlayer.setCurrentPlayerApplicationSettings(
                            (PlayerApplicationSettings) msg.obj);
                    notifyChanged(mAddressedPlayer.getPlaybackState());
                    return true;

                case MESSAGE_PROCESS_AVAILABLE_PLAYER_CHANGED:
                    processAvailablePlayerChanged();
                    return true;

                case MESSAGE_PROCESS_RECEIVED_COVER_ART_PSM:
                    mCoverArtPsm = msg.arg1;
                    connectCoverArt();
                    return true;

                case MESSAGE_PROCESS_IMAGE_DOWNLOADED:
                    AvrcpCoverArtManager.DownloadEvent event =
                            (AvrcpCoverArtManager.DownloadEvent) msg.obj;
                    String uuid = event.getUuid();
                    Uri uri = event.getUri();
                    logD("Received image for " + uuid + " at " + uri.toString());

                    // Let the addressed player know we got an image so it can see if the current
                    // track now has cover artwork
                    boolean addedArtwork = mAddressedPlayer.notifyImageDownload(uuid, uri);
                    if (addedArtwork && isActive()) {
                        BluetoothMediaBrowserService.trackChanged(
                                mAddressedPlayer.getCurrentTrack());
                    }

                    // Let the browse tree know of the newly downloaded image so it can attach it to
                    // all the items that need it. Notify of changed nodes accordingly
                    Set<BrowseTree.BrowseNode> nodes = mBrowseTree.notifyImageDownload(uuid, uri);
                    for (BrowseTree.BrowseNode node : nodes) {
                        notifyChanged(node);
                    }

                    // Delete images that were downloaded and entirely unused
                    if (!addedArtwork && nodes.isEmpty()) {
                        removeUnusedArtwork(uuid);
                        removeUnusedArtworkFromBrowseTree();
                    }

                    return true;

                case DISCONNECT:
                    transitionTo(mDisconnecting);
                    return true;

                default:
                    return super.processMessage(msg);
            }

        }

        private void processPlayItem(BrowseTree.BrowseNode node) {
            setActive(true);
            if (node == null) {
                Log.w(TAG, "Invalid item to play");
            } else {
                mService.playItemNative(
                        mDeviceAddress, node.getScope(),
                        node.getBluetoothID(), 0);
            }
        }

        private synchronized void passThru(int cmd) {
            logD("msgPassThru " + cmd);
            // Some keys should be held until the next event.
            if (mCurrentlyHeldKey != 0) {
                mService.sendPassThroughCommandNative(
                        mDeviceAddress, mCurrentlyHeldKey,
                        AvrcpControllerService.KEY_STATE_RELEASED);

                if (mCurrentlyHeldKey == cmd) {
                    // Return to prevent starting FF/FR operation again
                    mCurrentlyHeldKey = 0;
                    return;
                } else {
                    // FF/FR is in progress and other operation is desired
                    // so after stopping FF/FR, not returning so that command
                    // can be sent for the desired operation.
                    mCurrentlyHeldKey = 0;
                }
            }

            // Send the pass through.
            mService.sendPassThroughCommandNative(mDeviceAddress, cmd,
                    AvrcpControllerService.KEY_STATE_PRESSED);

            if (isHoldableKey(cmd)) {
                // Release cmd next time a command is sent.
                mCurrentlyHeldKey = cmd;
            } else {
                mService.sendPassThroughCommandNative(mDeviceAddress,
                        cmd, AvrcpControllerService.KEY_STATE_RELEASED);
            }
        }

        private boolean isHoldableKey(int cmd) {
            return (cmd == AvrcpControllerService.PASS_THRU_CMD_ID_REWIND)
                    || (cmd == AvrcpControllerService.PASS_THRU_CMD_ID_FF);
        }

        private void setRepeat(int repeatMode) {
            mService.setPlayerApplicationSettingValuesNative(mDeviceAddress, (byte) 1,
                    new byte[]{PlayerApplicationSettings.REPEAT_STATUS}, new byte[]{
                            PlayerApplicationSettings.mapAvrcpPlayerSettingstoBTattribVal(
                                    PlayerApplicationSettings.REPEAT_STATUS, repeatMode)});
        }

        private void setShuffle(int shuffleMode) {
            mService.setPlayerApplicationSettingValuesNative(mDeviceAddress, (byte) 1,
                    new byte[]{PlayerApplicationSettings.SHUFFLE_STATUS}, new byte[]{
                            PlayerApplicationSettings.mapAvrcpPlayerSettingstoBTattribVal(
                                    PlayerApplicationSettings.SHUFFLE_STATUS, shuffleMode)});
        }

        private void processAvailablePlayerChanged() {
            logD("processAvailablePlayerChanged");
            mBrowseTree.mRootNode.setCached(false);
            mBrowseTree.mRootNode.setExpectedChildren(255);
            BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mRootNode);
            removeUnusedArtworkFromBrowseTree();
        }
    }

    // Handle the get folder listing action
    // a) Fetch the listing of folders
    // b) Once completed return the object listing
    class GetFolderList extends State {
        private static final String STATE_TAG = "Avrcp.GetFolderList";

        boolean mAbort;
        BrowseTree.BrowseNode mBrowseNode;
        BrowseTree.BrowseNode mNextStep;

        @Override
        public void enter() {
            logD(STATE_TAG + " Entering GetFolderList");
            // Setup the timeouts.
            sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
            super.enter();
            mAbort = false;
            Message msg = getCurrentMessage();
            if (msg.what == MESSAGE_GET_FOLDER_ITEMS) {
                {
                    logD(STATE_TAG + " new Get Request");
                    mBrowseNode = (BrowseTree.BrowseNode) msg.obj;
                }
            }

            if (mBrowseNode == null) {
                transitionTo(mConnected);
            } else {
                navigateToFolderOrRetrieve(mBrowseNode);
            }
        }

        @Override
        public boolean processMessage(Message msg) {
            logD(STATE_TAG + " processMessage " + msg.what);
            switch (msg.what) {
                case MESSAGE_PROCESS_GET_FOLDER_ITEMS:
                    ArrayList<AvrcpItem> folderList = (ArrayList<AvrcpItem>) msg.obj;
                    int endIndicator = mBrowseNode.getExpectedChildren() - 1;
                    logD("GetFolderItems: End " + endIndicator
                            + " received " + folderList.size());

                    // Queue up image download if the item has an image and we don't have it yet
                    // Only do this if the feature is enabled.
                    for (AvrcpItem track : folderList) {
                        if (shouldDownloadBrowsedImages()) {
                            downloadImageIfNeeded(track);
                        } else {
                            track.setCoverArtUuid(null);
                        }
                    }

                    // Always update the node so that the user does not wait forever
                    // for the list to populate.
                    int newSize = mBrowseNode.addChildren(folderList);
                    logD("Added " + newSize + " items to the browse tree");
                    notifyChanged(mBrowseNode);

                    if (mBrowseNode.getChildrenCount() >= endIndicator || folderList.size() == 0
                            || mAbort) {
                        // If we have fetched all the elements or if the remotes sends us 0 elements
                        // (which can lead us into a loop since mCurrInd does not proceed) we simply
                        // abort.
                        mBrowseNode.setCached(true);
                        transitionTo(mConnected);
                    } else {
                        // Fetch the next set of items.
                        fetchContents(mBrowseNode);
                        // Reset the timeout message since we are doing a new fetch now.
                        removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
                        sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
                    }
                    break;
                case MESSAGE_PROCESS_SET_BROWSED_PLAYER:
                    mBrowseTree.setCurrentBrowsedPlayer(mNextStep.getID(), msg.arg1, msg.arg2);
                    removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
                    sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
                    navigateToFolderOrRetrieve(mBrowseNode);
                    break;

                case MESSAGE_PROCESS_FOLDER_PATH:
                    mBrowseTree.setCurrentBrowsedFolder(mNextStep.getID());
                    mBrowseTree.getCurrentBrowsedFolder().setExpectedChildren(msg.arg1);

                    // AVRCP Specification says, if we're not database aware, we must disconnect and
                    // reconnect our BIP client each time we successfully change path
                    refreshCoverArt();

                    if (mAbort) {
                        transitionTo(mConnected);
                    } else {
                        removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
                        sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
                        navigateToFolderOrRetrieve(mBrowseNode);
                    }
                    break;

                case MESSAGE_PROCESS_GET_PLAYER_ITEMS:
                    BrowseTree.BrowseNode rootNode = mBrowseTree.mRootNode;
                    if (!rootNode.isCached()) {
                        List<AvrcpPlayer> playerList = (List<AvrcpPlayer>) msg.obj;
                        mAvailablePlayerList.clear();
                        for (AvrcpPlayer player : playerList) {
                            mAvailablePlayerList.put(player.getId(), player);
                        }
                        rootNode.addChildren(playerList);
                        mBrowseTree.setCurrentBrowsedFolder(BrowseTree.ROOT);
                        rootNode.setExpectedChildren(playerList.size());
                        rootNode.setCached(true);
                        notifyChanged(rootNode);
                    }
                    transitionTo(mConnected);
                    break;

                case MESSAGE_INTERNAL_CMD_TIMEOUT:
                    // We have timed out to execute the request, we should simply send
                    // whatever listing we have gotten until now.
                    Log.w(TAG, "TIMEOUT");
                    transitionTo(mConnected);
                    break;

                case MESSAGE_PROCESS_GET_FOLDER_ITEMS_OUT_OF_RANGE:
                    // If we have gotten an error for OUT OF RANGE we have
                    // already sent all the items to the client hence simply
                    // transition to Connected state here.
                    mBrowseNode.setCached(true);
                    transitionTo(mConnected);
                    break;

                case MESSAGE_GET_FOLDER_ITEMS:
                    if (!mBrowseNode.equals(msg.obj)) {
                        if (shouldAbort(mBrowseNode.getScope(),
                                ((BrowseTree.BrowseNode) msg.obj).getScope())) {
                            mAbort = true;
                        }
                        deferMessage(msg);
                        logD("GetFolderItems: Go Get Another Directory");
                    } else {
                        logD("GetFolderItems: Get The Same Directory, ignore");
                    }
                    break;

                default:
                    // All of these messages should be handled by parent state immediately.
                    return false;
            }
            return true;
        }

        /**
         * shouldAbort calculates the cases where fetching the current directory is no longer
         * necessary.
         *
         * @return true:  a new folder in the same scope
         * a new player while fetching contents of a folder
         * false: other cases, specifically Now Playing while fetching a folder
         */
        private boolean shouldAbort(int currentScope, int fetchScope) {
            if ((currentScope == fetchScope)
                    || (currentScope == AvrcpControllerService.BROWSE_SCOPE_VFS
                    && fetchScope == AvrcpControllerService.BROWSE_SCOPE_PLAYER_LIST)) {
                return true;
            }
            return false;
        }

        private void fetchContents(BrowseTree.BrowseNode target) {
            int start = target.getChildrenCount();
            int end = Math.min(target.getExpectedChildren(), target.getChildrenCount()
                    + ITEM_PAGE_SIZE) - 1;
            logD("fetchContents(title=" + target.getID() + ", scope=" + target.getScope()
                    + ", start=" + start + ", end=" + end + ", expected="
                    + target.getExpectedChildren() + ")");
            switch (target.getScope()) {
                case AvrcpControllerService.BROWSE_SCOPE_PLAYER_LIST:
                    mService.getPlayerListNative(mDeviceAddress,
                            start, end);
                    break;
                case AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING:
                    mService.getNowPlayingListNative(
                            mDeviceAddress, start, end);
                    break;
                case AvrcpControllerService.BROWSE_SCOPE_VFS:
                    mService.getFolderListNative(mDeviceAddress,
                            start, end);
                    break;
                default:
                    Log.e(TAG, STATE_TAG + " Scope " + target.getScope()
                            + " cannot be handled here.");
            }
        }

        /* One of several things can happen when trying to get a folder list
         *
         *
         * 0: The folder handle is no longer valid
         * 1: The folder contents can be retrieved directly (NowPlaying, Root, Current)
         * 2: The folder is a browsable player
         * 3: The folder is a non browsable player
         * 4: The folder is not a child of the current folder
         * 5: The folder is a child of the current folder
         *
         */
        private void navigateToFolderOrRetrieve(BrowseTree.BrowseNode target) {
            mNextStep = mBrowseTree.getNextStepToFolder(target);
            logD("NAVIGATING From "
                    + mBrowseTree.getCurrentBrowsedFolder().toString());
            logD("NAVIGATING Toward " + target.toString());
            if (mNextStep == null) {
                return;
            } else if (target.equals(mBrowseTree.mNowPlayingNode)
                    || target.equals(mBrowseTree.mRootNode)
                    || mNextStep.equals(mBrowseTree.getCurrentBrowsedFolder())) {
                fetchContents(mNextStep);
            } else if (mNextStep.isPlayer()) {
                logD("NAVIGATING Player " + mNextStep.toString());
                if (mNextStep.isBrowsable()) {
                    mService.setBrowsedPlayerNative(
                            mDeviceAddress, (int) mNextStep.getBluetoothID());
                } else {
                    logD("Player doesn't support browsing");
                    mNextStep.setCached(true);
                    transitionTo(mConnected);
                }
            } else if (mNextStep.equals(mBrowseTree.mNavigateUpNode)) {
                logD("NAVIGATING UP " + mNextStep.toString());
                mNextStep = mBrowseTree.getCurrentBrowsedFolder().getParent();
                mBrowseTree.getCurrentBrowsedFolder().setCached(false);
                removeUnusedArtworkFromBrowseTree();
                mService.changeFolderPathNative(
                        mDeviceAddress,
                        AvrcpControllerService.FOLDER_NAVIGATION_DIRECTION_UP,
                        0);

            } else {
                logD("NAVIGATING DOWN " + mNextStep.toString());
                mService.changeFolderPathNative(
                        mDeviceAddress,
                        AvrcpControllerService.FOLDER_NAVIGATION_DIRECTION_DOWN,
                        mNextStep.getBluetoothID());
            }
        }

        @Override
        public void exit() {
            removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
            mBrowseNode = null;
            super.exit();
        }
    }

    protected class Disconnecting extends State {
        @Override
        public void enter() {
            disconnectCoverArt();
            onBrowsingDisconnected();
            setActive(false);
            broadcastConnectionStateChanged(BluetoothProfile.STATE_DISCONNECTING);
            transitionTo(mDisconnected);
        }
    }

    /**
     * Handle a request to align our local volume with the volume of a remote device. If
     * we're assuming the source volume is fixed then a response of ABS_VOL_MAX will always be
     * sent and no volume adjustment action will be taken on the sink side.
     *
     * @param absVol A volume level based on a domain of [0, ABS_VOL_MAX]
     * @param label Volume notification label
     */
    private void handleAbsVolumeRequest(int absVol, int label) {
        logD("handleAbsVolumeRequest: absVol = " + absVol + ", label = " + label);
        if (mIsVolumeFixed) {
            logD("Source volume is assumed to be fixed, responding with max volume");
            absVol = ABS_VOL_BASE;
        } else {
            mVolumeChangedNotificationsToIgnore++;
            removeMessages(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT);
            sendMessageDelayed(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT,
                    ABS_VOL_TIMEOUT_MILLIS);
            setAbsVolume(absVol);
        }
        mService.sendAbsVolRspNative(mDeviceAddress, absVol, label);
    }

    /**
     * Align our volume with a requested absolute volume level
     *
     * @param absVol A volume level based on a domain of [0, ABS_VOL_MAX]
     */
    private void setAbsVolume(int absVol) {
        int maxLocalVolume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int curLocalVolume = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        int reqLocalVolume = (maxLocalVolume * absVol) / ABS_VOL_BASE;
        logD("setAbsVolme: absVol = " + absVol + ", reqLocal = " + reqLocalVolume
                + ", curLocal = " + curLocalVolume + ", maxLocal = " + maxLocalVolume);

        /*
         * In some cases change in percentage is not sufficient enough to warrant
         * change in index values which are in range of 0-15. For such cases
         * no action is required
         */
        if (reqLocalVolume != curLocalVolume) {
            mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, reqLocalVolume,
                    AudioManager.FLAG_SHOW_UI);
        }
    }

    private int getAbsVolume() {
        if (mIsVolumeFixed) {
            return ABS_VOL_BASE;
        }
        int maxVolume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int currIndex = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        int newIndex = (currIndex * ABS_VOL_BASE) / maxVolume;
        return newIndex;
    }

    private boolean shouldDownloadBrowsedImages() {
        return mService.getResources()
                .getBoolean(R.bool.avrcp_controller_cover_art_browsed_images);
    }

    private void downloadImageIfNeeded(AvrcpItem track) {
        if (mCoverArtManager == null) return;
        String uuid = track.getCoverArtUuid();
        Uri imageUri = null;
        if (uuid != null) {
            imageUri = mCoverArtManager.getImageUri(mDevice, uuid);
            if (imageUri != null) {
                track.setCoverArtLocation(imageUri);
            } else {
                mCoverArtManager.downloadImage(mDevice, uuid);
            }
        }
    }

    MediaSessionCompat.Callback mSessionCallbacks = new MediaSessionCompat.Callback() {
        @Override
        public void onPlay() {
            logD("onPlay");
            onPrepare();
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_PLAY);
        }

        @Override
        public void onPause() {
            logD("onPause");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE);
        }

        @Override
        public void onSkipToNext() {
            logD("onSkipToNext");
            onPrepare();
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_FORWARD);
        }

        @Override
        public void onSkipToPrevious() {
            logD("onSkipToPrevious");
            onPrepare();
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_BACKWARD);
        }

        @Override
        public void onSkipToQueueItem(long id) {
            logD("onSkipToQueueItem id=" + id);
            onPrepare();
            BrowseTree.BrowseNode node = mBrowseTree.getTrackFromNowPlayingList((int) id);
            if (node != null) {
                sendMessage(MESSAGE_PLAY_ITEM, node);
            }
        }

        @Override
        public void onStop() {
            logD("onStop");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_STOP);
        }

        @Override
        public void onPrepare() {
            logD("onPrepare");
            A2dpSinkService a2dpSinkService = A2dpSinkService.getA2dpSinkService();
            if (a2dpSinkService != null) {
                a2dpSinkService.requestAudioFocus(mDevice, true);
            }
        }

        @Override
        public void onRewind() {
            logD("onRewind");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_REWIND);
        }

        @Override
        public void onFastForward() {
            logD("onFastForward");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_FF);
        }

        @Override
        public void onPlayFromMediaId(String mediaId, Bundle extras) {
            logD("onPlayFromMediaId");
            // Play the item if possible.
            onPrepare();
            BrowseTree.BrowseNode node = mBrowseTree.findBrowseNodeByID(mediaId);
            if (node != null) {
                // node was found on this bluetooth device
                sendMessage(MESSAGE_PLAY_ITEM, node);
            } else {
                // node was not found on this device, pause here, and play on another device
                sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE);
                mService.playItem(mediaId);
            }
        }

        @Override
        public void onSetRepeatMode(int repeatMode) {
            logD("onSetRepeatMode");
            sendMessage(MSG_AVRCP_SET_REPEAT, repeatMode);
        }

        @Override
        public void onSetShuffleMode(int shuffleMode) {
            logD("onSetShuffleMode");
            sendMessage(MSG_AVRCP_SET_SHUFFLE, shuffleMode);

        }
    };

    protected void broadcastConnectionStateChanged(int currentState) {
        if (mMostRecentState == currentState) {
            return;
        }
        if (currentState == BluetoothProfile.STATE_CONNECTED) {
            MetricsLogger.logProfileConnectionEvent(
                    BluetoothMetricsProto.ProfileId.AVRCP_CONTROLLER);
        }
        logD("Connection state " + mDevice + ": " + mMostRecentState + "->" + currentState);
        Intent intent = new Intent(BluetoothAvrcpController.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, mMostRecentState);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, currentState);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mDevice);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        mMostRecentState = currentState;
        mService.sendBroadcast(intent, ProfileService.BLUETOOTH_PERM);
    }

    private boolean shouldRequestFocus() {
        return mService.getResources()
                .getBoolean(R.bool.a2dp_sink_automatically_request_audio_focus);
    }
}
