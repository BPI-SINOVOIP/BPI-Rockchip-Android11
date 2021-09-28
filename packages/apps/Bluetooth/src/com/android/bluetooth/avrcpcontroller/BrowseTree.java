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
import android.support.v4.media.MediaBrowserCompat.MediaItem;
import android.util.Log;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

/**
 * An object that holds the browse tree of available media from a remote device.
 *
 * Browsing hierarchy follows the AVRCP specification's description of various scopes and
 * looks like follows:
 *    Root:
 *      Player1:
 *        Now_Playing:
 *           MediaItem1
 *           MediaItem2
 *        Folder1
 *        Folder2
 *          ....
 *        Player2
 *          ....
 */
public class BrowseTree {
    private static final String TAG = "BrowseTree";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    private static final boolean VDBG = Log.isLoggable(TAG, Log.VERBOSE);

    public static final String ROOT = "__ROOT__";
    public static final String UP = "__UP__";
    public static final String NOW_PLAYING_PREFIX = "NOW_PLAYING";
    public static final String PLAYER_PREFIX = "PLAYER";

    // Static instance of Folder ID <-> Folder Instance (for navigation purposes)
    private final HashMap<String, BrowseNode> mBrowseMap = new HashMap<String, BrowseNode>();
    private BrowseNode mCurrentBrowseNode;
    private BrowseNode mCurrentBrowsedPlayer;
    private BrowseNode mCurrentAddressedPlayer;
    private int mDepth = 0;
    final BrowseNode mRootNode;
    final BrowseNode mNavigateUpNode;
    final BrowseNode mNowPlayingNode;

    // In support of Cover Artwork, Cover Art URI <-> List of UUIDs using that artwork
    private final HashMap<String, ArrayList<String>> mCoverArtMap =
            new HashMap<String, ArrayList<String>>();

    BrowseTree(BluetoothDevice device) {
        if (device == null) {
            mRootNode = new BrowseNode(new AvrcpItem.Builder()
                    .setUuid(ROOT).setTitle(ROOT).setBrowsable(true).build());
            mRootNode.setCached(true);
        } else {
            mRootNode = new BrowseNode(new AvrcpItem.Builder().setDevice(device)
                    .setUuid(ROOT + device.getAddress().toString())
                    .setTitle(device.getName()).setBrowsable(true).build());
        }
        mRootNode.mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_PLAYER_LIST;
        mRootNode.setExpectedChildren(255);

        mNavigateUpNode = new BrowseNode(new AvrcpItem.Builder()
                .setUuid(UP).setTitle(UP).setBrowsable(true).build());

        mNowPlayingNode = new BrowseNode(new AvrcpItem.Builder()
                .setUuid(NOW_PLAYING_PREFIX).setTitle(NOW_PLAYING_PREFIX)
                .setBrowsable(true).build());
        mNowPlayingNode.mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING;
        mNowPlayingNode.setExpectedChildren(255);
        mBrowseMap.put(ROOT, mRootNode);
        mBrowseMap.put(NOW_PLAYING_PREFIX, mNowPlayingNode);

        mCurrentBrowseNode = mRootNode;
    }

    public void clear() {
        // Clearing the map should garbage collect everything.
        mBrowseMap.clear();
        mCoverArtMap.clear();
    }

    void onConnected(BluetoothDevice device) {
        BrowseNode browseNode = new BrowseNode(device);
        mRootNode.addChild(browseNode);
    }

    BrowseNode getTrackFromNowPlayingList(int trackNumber) {
        return mNowPlayingNode.getChild(trackNumber);
    }

    // Each node of the tree is represented by Folder ID, Folder Name and the children.
    class BrowseNode {
        // AvrcpItem to store the media related details.
        AvrcpItem mItem;

        // Type of this browse node.
        // Since Media APIs do not define the player separately we define that
        // distinction here.
        boolean mIsPlayer = false;

        // If this folder is currently cached, can be useful to return the contents
        // without doing another fetch.
        boolean mCached = false;

        byte mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_VFS;

        // List of children.
        private BrowseNode mParent;
        private final List<BrowseNode> mChildren = new ArrayList<BrowseNode>();
        private int mExpectedChildrenCount;

        BrowseNode(AvrcpItem item) {
            mItem = item;
        }

        BrowseNode(AvrcpPlayer player) {
            mIsPlayer = true;

            // Transform the player into a item.
            AvrcpItem.Builder aid = new AvrcpItem.Builder();
            aid.setDevice(player.getDevice());
            aid.setUid(player.getId());
            aid.setUuid(UUID.randomUUID().toString());
            aid.setDisplayableName(player.getName());
            aid.setTitle(player.getName());
            aid.setBrowsable(player.supportsFeature(AvrcpPlayer.FEATURE_BROWSING));
            mItem = aid.build();
        }

        BrowseNode(BluetoothDevice device) {
            mIsPlayer = true;
            String playerKey = PLAYER_PREFIX + device.getAddress().toString();

            AvrcpItem.Builder aid = new AvrcpItem.Builder();
            aid.setDevice(device);
            aid.setUuid(playerKey);
            aid.setDisplayableName(device.getName());
            aid.setTitle(device.getName());
            aid.setBrowsable(true);
            mItem = aid.build();
        }

        private BrowseNode(String name) {
            AvrcpItem.Builder aid = new AvrcpItem.Builder();
            aid.setUuid(name);
            aid.setDisplayableName(name);
            aid.setTitle(name);
            mItem = aid.build();
        }

        synchronized void setExpectedChildren(int count) {
            mExpectedChildrenCount = count;
        }

        synchronized int getExpectedChildren() {
            return mExpectedChildrenCount;
        }

        synchronized <E> int addChildren(List<E> newChildren) {
            for (E child : newChildren) {
                BrowseNode currentNode = null;
                if (child instanceof AvrcpItem) {
                    currentNode = new BrowseNode((AvrcpItem) child);
                } else if (child instanceof AvrcpPlayer) {
                    currentNode = new BrowseNode((AvrcpPlayer) child);
                }
                addChild(currentNode);
            }
            return newChildren.size();
        }

        synchronized boolean addChild(BrowseNode node) {
            if (node != null) {
                node.mParent = this;
                if (this.mBrowseScope == AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING) {
                    node.mBrowseScope = this.mBrowseScope;
                }
                mChildren.add(node);
                mBrowseMap.put(node.getID(), node);

                // Each time we add a node to the tree, check for an image handle so we can add
                // the artwork URI once it has been downloaded
                String imageUuid = node.getCoverArtUuid();
                if (imageUuid != null) {
                    indicateCoverArtUsed(node.getID(), imageUuid);
                }
                return true;
            }
            return false;
        }

        synchronized void removeChild(BrowseNode node) {
            mChildren.remove(node);
            mBrowseMap.remove(node.getID());
            indicateCoverArtUnused(node.getID(), node.getCoverArtUuid());
        }

        synchronized int getChildrenCount() {
            return mChildren.size();
        }

        synchronized List<BrowseNode> getChildren() {
            return mChildren;
        }

        synchronized BrowseNode getChild(int index) {
            if (index < 0 || index >= mChildren.size()) {
                return null;
            }
            return mChildren.get(index);
        }

        synchronized BrowseNode getParent() {
            return mParent;
        }

        synchronized BluetoothDevice getDevice() {
            return mItem.getDevice();
        }

        synchronized String getCoverArtUuid() {
            return mItem.getCoverArtUuid();
        }

        synchronized void setCoverArtUri(Uri uri) {
            mItem.setCoverArtLocation(uri);
        }

        synchronized List<MediaItem> getContents() {
            if (mChildren.size() > 0 || mCached) {
                List<MediaItem> contents = new ArrayList<MediaItem>(mChildren.size());
                for (BrowseNode child : mChildren) {
                    contents.add(child.getMediaItem());
                }
                return contents;
            }
            return null;
        }

        synchronized boolean isChild(BrowseNode node) {
            return mChildren.contains(node);
        }

        synchronized boolean isCached() {
            return mCached;
        }

        synchronized boolean isBrowsable() {
            return mItem.isBrowsable();
        }

        synchronized void setCached(boolean cached) {
            if (DBG) Log.d(TAG, "Set Cache" + cached + "Node" + toString());
            mCached = cached;
            if (!cached) {
                for (BrowseNode child : mChildren) {
                    mBrowseMap.remove(child.getID());
                    indicateCoverArtUnused(child.getID(), child.getCoverArtUuid());
                }
                mChildren.clear();
            }
        }

        // Fetch the Unique UID for this item, this is unique across all elements in the tree.
        synchronized String getID() {
            return mItem.getUuid();
        }

        // Get the BT Player ID associated with this node.
        synchronized int getPlayerID() {
            return Integer.parseInt(getID().replace(PLAYER_PREFIX, ""));
        }

        synchronized byte getScope() {
            return mBrowseScope;
        }

        // Fetch the Folder UID that can be used to fetch folder listing via bluetooth.
        // This may not be unique hence this combined with direction will define the
        // browsing here.
        synchronized String getFolderUID() {
            return getID();
        }

        synchronized long getBluetoothID() {
            return mItem.getUid();
        }

        synchronized MediaItem getMediaItem() {
            return mItem.toMediaItem();
        }

        synchronized boolean isPlayer() {
            return mIsPlayer;
        }

        synchronized boolean isNowPlaying() {
            return getID().startsWith(NOW_PLAYING_PREFIX);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof BrowseNode)) {
                return false;
            }
            BrowseNode otherNode = (BrowseNode) other;
            return getID().equals(otherNode.getID());
        }

        @Override
        public synchronized String toString() {
            if (VDBG) {
                String serialized = "[ Name: " + mItem.getTitle()
                        + " Scope:" + mBrowseScope + " expected Children: "
                        + mExpectedChildrenCount + "] ";
                for (BrowseNode node : mChildren) {
                    serialized += node.toString();
                }
                return serialized;
            } else {
                return "ID: " + getID();
            }
        }

        // Returns true if target is a descendant of this.
        synchronized boolean isDescendant(BrowseNode target) {
            return getEldestChild(this, target) == null ? false : true;
        }
    }

    synchronized BrowseNode findBrowseNodeByID(String parentID) {
        BrowseNode bn = mBrowseMap.get(parentID);
        if (bn == null) {
            Log.e(TAG, "folder " + parentID + " not found!");
            return null;
        }
        if (VDBG) {
            Log.d(TAG, "Size" + mBrowseMap.size());
        }
        return bn;
    }

    synchronized boolean setCurrentBrowsedFolder(String uid) {
        BrowseNode bn = mBrowseMap.get(uid);
        if (bn == null) {
            Log.e(TAG, "Setting an unknown browsed folder, ignoring bn " + uid);
            return false;
        }

        // Set the previous folder as not cached so that we fetch the contents again.
        if (!bn.equals(mCurrentBrowseNode)) {
            Log.d(TAG, "Set cache  " + bn + " curr " + mCurrentBrowseNode);
        }
        mCurrentBrowseNode = bn;
        return true;
    }

    synchronized BrowseNode getCurrentBrowsedFolder() {
        return mCurrentBrowseNode;
    }

    synchronized boolean setCurrentBrowsedPlayer(String uid, int items, int depth) {
        BrowseNode bn = mBrowseMap.get(uid);
        if (bn == null) {
            Log.e(TAG, "Setting an unknown browsed player, ignoring bn " + uid);
            return false;
        }
        mCurrentBrowsedPlayer = bn;
        mCurrentBrowseNode = mCurrentBrowsedPlayer;
        for (Integer level = 0; level < depth; level++) {
            BrowseNode dummyNode = new BrowseNode(level.toString());
            dummyNode.mParent = mCurrentBrowseNode;
            dummyNode.mBrowseScope = AvrcpControllerService.BROWSE_SCOPE_VFS;
            mCurrentBrowseNode = dummyNode;
        }
        mCurrentBrowseNode.setExpectedChildren(items);
        mDepth = depth;
        return true;
    }

    synchronized BrowseNode getCurrentBrowsedPlayer() {
        return mCurrentBrowsedPlayer;
    }

    synchronized boolean setCurrentAddressedPlayer(String uid) {
        BrowseNode bn = mBrowseMap.get(uid);
        if (bn == null) {
            if (DBG) Log.d(TAG, "Setting an unknown addressed player, ignoring bn " + uid);
            mRootNode.setCached(false);
            mRootNode.mChildren.add(mNowPlayingNode);
            mBrowseMap.put(NOW_PLAYING_PREFIX, mNowPlayingNode);
            return false;
        }
        mCurrentAddressedPlayer = bn;
        return true;
    }

    synchronized BrowseNode getCurrentAddressedPlayer() {
        return mCurrentAddressedPlayer;
    }

    /**
     * Indicate that a node in the tree is using a specific piece of cover art, identified by the
     * given image handle.
     */
    synchronized void indicateCoverArtUsed(String nodeId, String handle) {
        mCoverArtMap.putIfAbsent(handle, new ArrayList<String>());
        mCoverArtMap.get(handle).add(nodeId);
    }

    /**
     * Indicate that a node in the tree no longer needs a specific piece of cover art.
     */
    synchronized void indicateCoverArtUnused(String nodeId, String handle) {
        if (mCoverArtMap.containsKey(handle) && mCoverArtMap.get(handle).contains(nodeId)) {
            mCoverArtMap.get(handle).remove(nodeId);
        }
    }

    /**
     * Get a list of items using the piece of cover art identified by the given handle.
     */
    synchronized ArrayList<String> getNodesUsingCoverArt(String handle) {
        if (!mCoverArtMap.containsKey(handle)) return new ArrayList<String>();
        return (ArrayList<String>) mCoverArtMap.get(handle).clone();
    }

    /**
     * Get a list of Cover Art UUIDs that are no longer being used by the tree. Clear that list.
     */
    synchronized ArrayList<String> getAndClearUnusedCoverArt() {
        ArrayList<String> unused = new ArrayList<String>();
        for (String uuid : mCoverArtMap.keySet()) {
            if (mCoverArtMap.get(uuid).isEmpty()) {
                unused.add(uuid);
            }
        }
        for (String uuid : unused) {
            mCoverArtMap.remove(uuid);
        }
        return unused;
    }

    /**
     * Adds the Uri of a newly downloaded image to all tree nodes using that specific handle.
     * Returns the set of parent nodes that have children impacted by the new art so clients can
     * be notified of the change.
     */
    synchronized Set<BrowseNode> notifyImageDownload(String uuid, Uri uri) {
        if (DBG) Log.d(TAG, "Received downloaded image handle to cascade to BrowseNodes using it");
        ArrayList<String> nodes = getNodesUsingCoverArt(uuid);
        HashSet<BrowseNode> parents = new HashSet<BrowseNode>();
        for (String nodeId : nodes) {
            BrowseNode node = findBrowseNodeByID(nodeId);
            if (node == null) {
                Log.e(TAG, "Node was removed without clearing its cover art status");
                indicateCoverArtUnused(nodeId, uuid);
                continue;
            }
            node.setCoverArtUri(uri);
            if (node.mParent != null) {
                parents.add(node.mParent);
            }
        }
        return parents;
    }


    @Override
    public String toString() {
        String serialized = "Size: " + mBrowseMap.size();
        if (VDBG) {
            serialized += mRootNode.toString();
            serialized += "\n  Image handles in use (" + mCoverArtMap.size() + "):";
            for (String handle : mCoverArtMap.keySet()) {
                serialized += "\n    " + handle + "\n";
            }
        }
        return serialized;
    }

    // Calculates the path to target node.
    // Returns: UP node to go up
    // Returns: target node if there
    // Returns: named node to go down
    // Returns: null node if unknown
    BrowseNode getNextStepToFolder(BrowseNode target) {
        if (target == null) {
            return null;
        } else if (target.equals(mCurrentBrowseNode)
                || target.equals(mNowPlayingNode)
                || target.equals(mRootNode)) {
            return target;
        } else if (target.isPlayer()) {
            if (mDepth > 0) {
                mDepth--;
                return mNavigateUpNode;
            } else {
                return target;
            }
        } else if (mBrowseMap.get(target.getID()) == null) {
            return null;
        } else {
            BrowseNode nextChild = getEldestChild(mCurrentBrowseNode, target);
            if (nextChild == null) {
                return mNavigateUpNode;
            } else {
                return nextChild;
            }
        }
    }

    static BrowseNode getEldestChild(BrowseNode ancestor, BrowseNode target) {
        // ancestor is an ancestor of target
        BrowseNode descendant = target;
        if (DBG) {
            Log.d(TAG, "NAVIGATING ancestor" + ancestor.toString() + "Target"
                    + target.toString());
        }
        while (!ancestor.equals(descendant.mParent)) {
            descendant = descendant.mParent;
            if (descendant == null) {
                return null;
            }
        }
        if (DBG) Log.d(TAG, "NAVIGATING Descendant" + descendant.toString());
        return descendant;
    }
}
