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

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAvrcpPlayerSettings;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothAvrcpController;
import android.content.Intent;
import android.support.v4.media.MediaBrowserCompat.MediaItem;
import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;

import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.ProfileService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Provides Bluetooth AVRCP Controller profile, as a service in the Bluetooth application.
 */
public class AvrcpControllerService extends ProfileService {
    static final String TAG = "AvrcpControllerService";
    static final int MAXIMUM_CONNECTED_DEVICES = 5;
    static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    static final boolean VDBG = Log.isLoggable(TAG, Log.VERBOSE);

    /*
     *  Play State Values from JNI
     */
    private static final byte JNI_PLAY_STATUS_STOPPED = 0x00;
    private static final byte JNI_PLAY_STATUS_PLAYING = 0x01;
    private static final byte JNI_PLAY_STATUS_PAUSED = 0x02;
    private static final byte JNI_PLAY_STATUS_FWD_SEEK = 0x03;
    private static final byte JNI_PLAY_STATUS_REV_SEEK = 0x04;
    private static final byte JNI_PLAY_STATUS_ERROR = -1;

    /* Folder/Media Item scopes.
     * Keep in sync with AVRCP 1.6 sec. 6.10.1
     */
    public static final byte BROWSE_SCOPE_PLAYER_LIST = 0x00;
    public static final byte BROWSE_SCOPE_VFS = 0x01;
    public static final byte BROWSE_SCOPE_SEARCH = 0x02;
    public static final byte BROWSE_SCOPE_NOW_PLAYING = 0x03;

    /* Folder navigation directions
     * This is borrowed from AVRCP 1.6 spec and must be kept with same values
     */
    public static final byte FOLDER_NAVIGATION_DIRECTION_UP = 0x00;
    public static final byte FOLDER_NAVIGATION_DIRECTION_DOWN = 0x01;

    /*
     * KeyCoded for Pass Through Commands
     */
    public static final int PASS_THRU_CMD_ID_PLAY = 0x44;
    public static final int PASS_THRU_CMD_ID_PAUSE = 0x46;
    public static final int PASS_THRU_CMD_ID_VOL_UP = 0x41;
    public static final int PASS_THRU_CMD_ID_VOL_DOWN = 0x42;
    public static final int PASS_THRU_CMD_ID_STOP = 0x45;
    public static final int PASS_THRU_CMD_ID_FF = 0x49;
    public static final int PASS_THRU_CMD_ID_REWIND = 0x48;
    public static final int PASS_THRU_CMD_ID_FORWARD = 0x4B;
    public static final int PASS_THRU_CMD_ID_BACKWARD = 0x4C;

    /* Key State Variables */
    public static final int KEY_STATE_PRESSED = 0;
    public static final int KEY_STATE_RELEASED = 1;

    static BrowseTree sBrowseTree;
    private static AvrcpControllerService sService;
    private final BluetoothAdapter mAdapter;

    protected Map<BluetoothDevice, AvrcpControllerStateMachine> mDeviceStateMap =
            new ConcurrentHashMap<>(1);

    private boolean mCoverArtEnabled = false;
    protected AvrcpCoverArtManager mCoverArtManager;

    private class ImageDownloadCallback implements AvrcpCoverArtManager.Callback {
        @Override
        public void onImageDownloadComplete(BluetoothDevice device,
                AvrcpCoverArtManager.DownloadEvent event) {
            if (DBG) {
                Log.d(TAG, "Image downloaded [device: " + device + ", uuid: " + event.getUuid()
                        + ", uri: " + event.getUri());
            }
            AvrcpControllerStateMachine stateMachine = getStateMachine(device);
            if (stateMachine == null) {
                Log.e(TAG, "No state machine found for device " + device);
                mCoverArtManager.removeImage(device, event.getUuid());
                return;
            }
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_IMAGE_DOWNLOADED,
                    event);
        }
    }

    static {
        classInitNative();
    }

    public AvrcpControllerService() {
        super();
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    @Override
    protected synchronized boolean start() {
        initNative();
        mCoverArtEnabled = getResources().getBoolean(R.bool.avrcp_controller_enable_cover_art);
        if (mCoverArtEnabled) {
            mCoverArtManager = new AvrcpCoverArtManager(this, new ImageDownloadCallback());
        }
        sBrowseTree = new BrowseTree(null);
        sService = this;

        // Start the media browser service.
        Intent startIntent = new Intent(this, BluetoothMediaBrowserService.class);
        startService(startIntent);
        return true;
    }

    @Override
    protected synchronized boolean stop() {
        Intent stopIntent = new Intent(this, BluetoothMediaBrowserService.class);
        stopService(stopIntent);
        for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
            stateMachine.quitNow();
        }

        sService = null;
        sBrowseTree = null;
        if (mCoverArtManager != null) {
            mCoverArtManager.cleanup();
            mCoverArtManager = null;
        }
        return true;
    }

    public static AvrcpControllerService getAvrcpControllerService() {
        return sService;
    }

    protected AvrcpControllerStateMachine newStateMachine(BluetoothDevice device) {
        return new AvrcpControllerStateMachine(device, this);
    }

    protected void getCurrentMetadataIfNoCoverArt(BluetoothDevice device) {
        if (device == null) return;
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine == null) return;
        AvrcpItem track = stateMachine.getCurrentTrack();
        if (track != null && track.getCoverArtLocation() == null) {
            getCurrentMetadataNative(Utils.getByteAddress(device));
        }
    }

    private void refreshContents(BrowseTree.BrowseNode node) {
        BluetoothDevice device = node.getDevice();
        if (device == null) {
            return;
        }
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.requestContents(node);
        }
    }

    void playItem(String parentMediaId) {
        if (DBG) Log.d(TAG, "playItem(" + parentMediaId + ")");
        // Check if the requestedNode is a player rather than a song
        BrowseTree.BrowseNode requestedNode = sBrowseTree.findBrowseNodeByID(parentMediaId);
        if (requestedNode == null) {
            for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
                // Check each state machine for the song and then play it
                requestedNode = stateMachine.findNode(parentMediaId);
                if (requestedNode != null) {
                    if (DBG) Log.d(TAG, "Found a node");
                    stateMachine.playItem(requestedNode);
                    break;
                }
            }
        }
    }

    /*Java API*/

    /**
     * Get a List of MediaItems that are children of the specified media Id
     *
     * @param parentMediaId The player or folder to get the contents of
     * @return List of Children if available, an empty list if there are none,
     * or null if a search must be performed.
     */
    public synchronized List<MediaItem> getContents(String parentMediaId) {
        if (DBG) Log.d(TAG, "getContents(" + parentMediaId + ")");

        BrowseTree.BrowseNode requestedNode = sBrowseTree.findBrowseNodeByID(parentMediaId);
        if (requestedNode == null) {
            for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
                requestedNode = stateMachine.findNode(parentMediaId);
                if (requestedNode != null) {
                    Log.d(TAG, "Found a node");
                    break;
                }
            }
        }

        // If we don't find a node in the tree then do not have any way to browse for the contents.
        // Return an empty list instead.
        if (requestedNode == null) {
            if (DBG) Log.d(TAG, "Didn't find a node");
            return new ArrayList(0);
        } else {
            if (!requestedNode.isCached()) {
                if (DBG) Log.d(TAG, "node is not cached");
                refreshContents(requestedNode);
            }
            if (DBG) Log.d(TAG, "Returning contents");
            return requestedNode.getContents();
        }
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new AvrcpControllerServiceBinder(this);
    }

    //Binder object: Must be static class or memory leak may occur
    private static class AvrcpControllerServiceBinder extends IBluetoothAvrcpController.Stub
            implements IProfileServiceBinder {
        private AvrcpControllerService mService;

        private AvrcpControllerService getService() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "AVRCP call not allowed for non-active user");
                return null;
            }

            if (mService != null) {
                return mService;
            }
            return null;
        }

        AvrcpControllerServiceBinder(AvrcpControllerService service) {
            mService = service;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices() {
            AvrcpControllerService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getConnectedDevices();
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
            AvrcpControllerService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getDevicesMatchingConnectionStates(states);
        }

        @Override
        public int getConnectionState(BluetoothDevice device) {
            AvrcpControllerService service = getService();
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public void sendGroupNavigationCmd(BluetoothDevice device, int keyCode, int keyState) {
            Log.w(TAG, "sendGroupNavigationCmd not implemented");
            return;
        }

        @Override
        public boolean setPlayerApplicationSetting(BluetoothAvrcpPlayerSettings settings) {
            Log.w(TAG, "setPlayerApplicationSetting not implemented");
            return false;
        }

        @Override
        public BluetoothAvrcpPlayerSettings getPlayerSettings(BluetoothDevice device) {
            Log.w(TAG, "getPlayerSettings not implemented");
            return null;
        }
    }


    /* JNI API*/
    // Called by JNI when a passthrough key was received.
    private void handlePassthroughRsp(int id, int keyState, byte[] address) {
        if (DBG) {
            Log.d(TAG, "passthrough response received as: key: " + id
                    + " state: " + keyState + "address:" + address);
        }
    }

    private void handleGroupNavigationRsp(int id, int keyState) {
        if (DBG) {
            Log.d(TAG, "group navigation response received as: key: " + id + " state: "
                    + keyState);
        }
    }

    // Called by JNI when a device has connected or disconnected.
    private synchronized void onConnectionStateChanged(boolean remoteControlConnected,
            boolean browsingConnected, byte[] address) {
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        if (DBG) {
            Log.d(TAG, "onConnectionStateChanged " + remoteControlConnected + " "
                    + browsingConnected + device);
        }
        if (device == null) {
            Log.e(TAG, "onConnectionStateChanged Device is null");
            return;
        }

        StackEvent event =
                StackEvent.connectionStateChanged(remoteControlConnected, browsingConnected);
        AvrcpControllerStateMachine stateMachine = getOrCreateStateMachine(device);
        if (remoteControlConnected || browsingConnected) {
            stateMachine.connect(event);
        } else {
            stateMachine.disconnect();
        }
    }

    // Called by JNI to notify Avrcp of features supported by the Remote device.
    private void getRcFeatures(byte[] address, int features) {
        /* Do Nothing. */
    }

    // Called by JNI to notify Avrcp of a remote device's Cover Art PSM
    private void getRcPsm(byte[] address, int psm) {
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        if (DBG) Log.d(TAG, "getRcPsm(device=" + device + ", psm=" + psm + ")");
        AvrcpControllerStateMachine stateMachine = getOrCreateStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_RECEIVED_COVER_ART_PSM, psm);
        }
    }

    // Called by JNI
    private void setPlayerAppSettingRsp(byte[] address, byte accepted) {
        /* Do Nothing. */
    }

    // Called by JNI when remote wants to receive absolute volume notifications.
    private synchronized void handleRegisterNotificationAbsVol(byte[] address, byte label) {
        if (DBG) {
            Log.d(TAG, "handleRegisterNotificationAbsVol");
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION);
        }
    }

    // Called by JNI when remote wants to set absolute volume.
    private synchronized void handleSetAbsVolume(byte[] address, byte absVol, byte label) {
        if (DBG) {
            Log.d(TAG, "handleSetAbsVolume ");
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_SET_ABS_VOL_CMD,
                    absVol);
        }
    }

    // Called by JNI when a track changes and local AvrcpController is registered for updates.
    private synchronized void onTrackChanged(byte[] address, byte numAttributes, int[] attributes,
            String[] attribVals) {
        if (DBG) {
            Log.d(TAG, "onTrackChanged");
        }

        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            AvrcpItem.Builder aib = new AvrcpItem.Builder();
            aib.fromAvrcpAttributeArray(attributes, attribVals);
            aib.setDevice(device);
            aib.setItemType(AvrcpItem.TYPE_MEDIA);
            aib.setUuid(UUID.randomUUID().toString());
            AvrcpItem item = aib.build();
            if (mCoverArtManager != null) {
                String handle = item.getCoverArtHandle();
                if (handle != null) {
                    item.setCoverArtUuid(mCoverArtManager.getUuidForHandle(device, handle));
                }
            }
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_TRACK_CHANGED,
                    item);
        }
    }

    // Called by JNI periodically based upon timer to update play position
    private synchronized void onPlayPositionChanged(byte[] address, int songLen,
            int currSongPosition) {
        if (DBG) {
            Log.d(TAG, "onPlayPositionChanged pos " + currSongPosition);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_POS_CHANGED,
                    songLen, currSongPosition);
        }
    }

    // Called by JNI on changes of play status
    private synchronized void onPlayStatusChanged(byte[] address, byte playStatus) {
        if (DBG) {
            Log.d(TAG, "onPlayStatusChanged " + playStatus);
        }
        int playbackState = PlaybackStateCompat.STATE_NONE;
        switch (playStatus) {
            case JNI_PLAY_STATUS_STOPPED:
                playbackState = PlaybackStateCompat.STATE_STOPPED;
                break;
            case JNI_PLAY_STATUS_PLAYING:
                playbackState = PlaybackStateCompat.STATE_PLAYING;
                break;
            case JNI_PLAY_STATUS_PAUSED:
                playbackState = PlaybackStateCompat.STATE_PAUSED;
                break;
            case JNI_PLAY_STATUS_FWD_SEEK:
                playbackState = PlaybackStateCompat.STATE_FAST_FORWARDING;
                break;
            case JNI_PLAY_STATUS_REV_SEEK:
                playbackState = PlaybackStateCompat.STATE_REWINDING;
                break;
            default:
                playbackState = PlaybackStateCompat.STATE_NONE;
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED, playbackState);
        }
    }

    // Called by JNI to report remote Player's capabilities
    private synchronized void handlePlayerAppSetting(byte[] address, byte[] playerAttribRsp,
            int rspLen) {
        if (DBG) {
            Log.d(TAG, "handlePlayerAppSetting rspLen = " + rspLen);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            PlayerApplicationSettings supportedSettings =
                    PlayerApplicationSettings.makeSupportedSettings(playerAttribRsp);
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_SUPPORTED_APPLICATION_SETTINGS,
                    supportedSettings);
        }
    }

    private synchronized void onPlayerAppSettingChanged(byte[] address, byte[] playerAttribRsp,
            int rspLen) {
        if (DBG) {
            Log.d(TAG, "onPlayerAppSettingChanged ");
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {

            PlayerApplicationSettings currentSettings =
                    PlayerApplicationSettings.makeSettings(playerAttribRsp);
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_CURRENT_APPLICATION_SETTINGS,
                    currentSettings);
        }
    }

    private void onAvailablePlayerChanged(byte[] address) {
        if (DBG) {
            Log.d(TAG," onAvailablePlayerChanged");
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_AVAILABLE_PLAYER_CHANGED);
        }
    }

    // Browsing related JNI callbacks.
    void handleGetFolderItemsRsp(byte[] address, int status, AvrcpItem[] items) {
        if (DBG) {
            Log.d(TAG, "handleGetFolderItemsRsp called with status " + status + " items "
                    + items.length + " items.");
        }

        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        List<AvrcpItem> itemsList = new ArrayList<>();
        for (AvrcpItem item : items) {
            if (VDBG) Log.d(TAG, item.toString());
            if (mCoverArtManager != null) {
                String handle = item.getCoverArtHandle();
                if (handle != null) {
                    item.setCoverArtUuid(mCoverArtManager.getUuidForHandle(device, handle));
                }
            }
            itemsList.add(item);
        }

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_FOLDER_ITEMS,
                    itemsList);
        }
    }

    void handleGetPlayerItemsRsp(byte[] address, AvrcpPlayer[] items) {
        if (DBG) {
            Log.d(TAG, "handleGetFolderItemsRsp called with " + items.length + " items.");
        }

        List<AvrcpPlayer> itemsList = new ArrayList<>();
        for (AvrcpPlayer item : items) {
            if (VDBG) Log.d(TAG, "bt player item: " + item);
            itemsList.add(item);
        }

        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_PLAYER_ITEMS,
                    itemsList);
        }
    }

    // JNI Helper functions to convert native objects to java.
    AvrcpItem createFromNativeMediaItem(byte[] address, long uid, int type, String name,
            int[] attrIds, String[] attrVals) {
        if (VDBG) {
            Log.d(TAG, "createFromNativeMediaItem uid: " + uid + " type: " + type + " name: " + name
                    + " attrids: " + attrIds + " attrVals: " + attrVals);
        }

        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpItem.Builder aib = new AvrcpItem.Builder().fromAvrcpAttributeArray(attrIds, attrVals);
        aib.setDevice(device);
        aib.setItemType(AvrcpItem.TYPE_MEDIA);
        aib.setType(type);
        aib.setUid(uid);
        aib.setUuid(UUID.randomUUID().toString());
        aib.setPlayable(true);
        AvrcpItem item = aib.build();
        return item;
    }

    AvrcpItem createFromNativeFolderItem(byte[] address, long uid, int type, String name,
            int playable) {
        if (VDBG) {
            Log.d(TAG, "createFromNativeFolderItem uid: " + uid + " type " + type + " name "
                    + name + " playable " + playable);
        }

        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpItem.Builder aib = new AvrcpItem.Builder();
        aib.setDevice(device);
        aib.setItemType(AvrcpItem.TYPE_FOLDER);
        aib.setType(type);
        aib.setUid(uid);
        aib.setUuid(UUID.randomUUID().toString());
        aib.setDisplayableName(name);
        aib.setPlayable(playable == 0x01);
        aib.setBrowsable(true);
        return aib.build();
    }

    AvrcpPlayer createFromNativePlayerItem(byte[] address, int id, String name,
            byte[] transportFlags, int playStatus, int playerType) {
        if (VDBG) {
            Log.d(TAG,
                    "createFromNativePlayerItem name: " + name + " transportFlags "
                            + transportFlags + " play status " + playStatus + " player type "
                            + playerType);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpPlayer player = new AvrcpPlayer(device, id, name, transportFlags, playStatus,
                playerType);
        return player;
    }

    private void handleChangeFolderRsp(byte[] address, int count) {
        if (DBG) {
            Log.d(TAG, "handleChangeFolderRsp count: " + count);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_FOLDER_PATH,
                    count);
        }
    }

    private void handleSetBrowsedPlayerRsp(byte[] address, int items, int depth) {
        if (DBG) {
            Log.d(TAG, "handleSetBrowsedPlayerRsp depth: " + depth);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_SET_BROWSED_PLAYER,
                    items, depth);
        }
    }

    private void handleSetAddressedPlayerRsp(byte[] address, int status) {
        if (DBG) {
            Log.d(TAG, "handleSetAddressedPlayerRsp status: " + status);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_SET_ADDRESSED_PLAYER);
        }
    }

    private void handleAddressedPlayerChanged(byte[] address, int id) {
        if (DBG) {
            Log.d(TAG, "handleAddressedPlayerChanged id: " + id);
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED, id);
        }
    }

    private void handleNowPlayingContentChanged(byte[] address) {
        if (DBG) {
            Log.d(TAG, "handleNowPlayingContentChanged");
        }
        BluetoothDevice device = mAdapter.getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.nowPlayingContentChanged();
        }
    }

    /* Generic Profile Code */

    /**
     * Disconnect the given Bluetooth device.
     *
     * @return true if disconnect is successful, false otherwise.
     */
    public synchronized boolean disconnect(BluetoothDevice device) {
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        // a map state machine instance doesn't exist. maybe it is already gone?
        if (stateMachine == null) {
            return false;
        }
        int connectionState = stateMachine.getState();
        if (connectionState != BluetoothProfile.STATE_CONNECTED
                && connectionState != BluetoothProfile.STATE_CONNECTING) {
            return false;
        }
        stateMachine.disconnect();
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        return true;
    }

    /**
     * Remove state machine from device map once it is no longer needed.
     */
    public void removeStateMachine(AvrcpControllerStateMachine stateMachine) {
        mDeviceStateMap.remove(stateMachine.getDevice());
    }

    public List<BluetoothDevice> getConnectedDevices() {
        return getDevicesMatchingConnectionStates(new int[]{BluetoothAdapter.STATE_CONNECTED});
    }

    protected AvrcpControllerStateMachine getStateMachine(BluetoothDevice device) {
        return mDeviceStateMap.get(device);
    }

    protected AvrcpControllerStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        if (stateMachine == null) {
            stateMachine = newStateMachine(device);
            mDeviceStateMap.put(device, stateMachine);
            stateMachine.start();
        }
        return stateMachine;
    }

    protected AvrcpCoverArtManager getCoverArtManager() {
        return mCoverArtManager;
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (DBG) Log.d(TAG, "getDevicesMatchingConnectionStates" + Arrays.toString(states));
        List<BluetoothDevice> deviceList = new ArrayList<>();
        Set<BluetoothDevice> bondedDevices = mAdapter.getBondedDevices();
        int connectionState;
        for (BluetoothDevice device : bondedDevices) {
            connectionState = getConnectionState(device);
            if (DBG) Log.d(TAG, "Device: " + device + "State: " + connectionState);
            for (int i = 0; i < states.length; i++) {
                if (connectionState == states[i]) {
                    deviceList.add(device);
                }
            }
        }
        if (DBG) Log.d(TAG, deviceList.toString());
        Log.d(TAG, "GetDevicesDone");
        return deviceList;
    }

    synchronized int getConnectionState(BluetoothDevice device) {
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        return (stateMachine == null) ? BluetoothProfile.STATE_DISCONNECTED
                : stateMachine.getState();
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        ProfileService.println(sb, "Devices Tracked = " + mDeviceStateMap.size());

        for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
            ProfileService.println(sb,
                    "==== StateMachine for " + stateMachine.getDevice() + " ====");
            stateMachine.dump(sb);
        }
        sb.append("\n  sBrowseTree: " + sBrowseTree.toString());

        sb.append("\n  Cover Artwork Enabled: " + (mCoverArtEnabled ? "True" : "False"));
        if (mCoverArtManager != null) {
            sb.append("\n  " + mCoverArtManager.toString());
        }
    }

    /*JNI*/
    private static native void classInitNative();

    private native void initNative();

    private native void cleanupNative();

    /**
     * Send button press commands to addressed device
     *
     * @param keyCode  key code as defined in AVRCP specification
     * @param keyState 0 = key pressed, 1 = key released
     * @return command was sent
     */
    public native boolean sendPassThroughCommandNative(byte[] address, int keyCode, int keyState);

    /**
     * Send group navigation commands
     *
     * @param keyCode  next/previous
     * @param keyState state
     * @return command was sent
     */
    public native boolean sendGroupNavigationCommandNative(byte[] address, int keyCode,
            int keyState);

    /**
     * Change player specific settings such as shuffle
     *
     * @param numAttrib number of settings being sent
     * @param attribIds list of settings to be changed
     * @param attribVal list of settings values
     */
    public native void setPlayerApplicationSettingValuesNative(byte[] address, byte numAttrib,
            byte[] attribIds, byte[] attribVal);

    /**
     * Send response to set absolute volume
     *
     * @param absVol new volume
     * @param label  label
     */
    public native void sendAbsVolRspNative(byte[] address, int absVol, int label);

    /**
     * Register for any volume level changes
     *
     * @param rspType type of response
     * @param absVol  current volume
     * @param label   label
     */
    public native void sendRegisterAbsVolRspNative(byte[] address, byte rspType, int absVol,
            int label);

    /**
     * Fetch the current track's metadata
     *
     * This method is specifically meant to allow us to fetch image handles that may not have been
     * sent to us yet, prior to having a BIP client connection. See the AVRCP 1.6+ specification,
     * section 4.1.7, for more details.
     */
    public native void getCurrentMetadataNative(byte[] address);

    /**
     * Fetch the playback state
     */
    public native void getPlaybackStateNative(byte[] address);

    /**
     * Fetch the current now playing list
     *
     * @param start first index to retrieve
     * @param end   last index to retrieve
     */
    public native void getNowPlayingListNative(byte[] address, int start, int end);

    /**
     * Fetch the current folder's listing
     *
     * @param start first index to retrieve
     * @param end   last index to retrieve
     */
    public native void getFolderListNative(byte[] address, int start, int end);

    /**
     * Fetch the listing of players
     *
     * @param start first index to retrieve
     * @param end   last index to retrieve
     */
    public native void getPlayerListNative(byte[] address, int start, int end);

    /**
     * Change the current browsed folder
     *
     * @param direction up/down
     * @param uid       folder unique id
     */
    public native void changeFolderPathNative(byte[] address, byte direction, long uid);

    /**
     * Play item with provided uid
     *
     * @param scope      scope of item to played
     * @param uid        song unique id
     * @param uidCounter counter
     */
    public native void playItemNative(byte[] address, byte scope, long uid, int uidCounter);

    /**
     * Set a specific player for browsing
     *
     * @param playerId player number
     */
    public native void setBrowsedPlayerNative(byte[] address, int playerId);

    /**
     * Set a specific player for handling playback commands
     *
     * @param playerId player number
     */
    public native void setAddressedPlayerNative(byte[] address, int playerId);
}
