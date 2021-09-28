/*
 * Copyright (C) 2019 The Android Open Source Project
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
import android.bluetooth.BluetoothProfile;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.SystemProperties;
import android.util.Log;

import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

import javax.obex.ResponseCodes;

/**
 * Manager of all AVRCP Controller connections to remote devices' BIP servers for retrieving cover
 * art.
 *
 * When given an image handle and device, this manager will negotiate the downloaded image
 * properties, download the image, and place it into a Content Provider for others to retrieve from
 */
public class AvrcpCoverArtManager {
    private static final String TAG = "AvrcpCoverArtManager";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    // Image Download Schemes for cover art
    public static final String AVRCP_CONTROLLER_COVER_ART_SCHEME =
            "persist.bluetooth.avrcpcontroller.BIP_DOWNLOAD_SCHEME";
    public static final String SCHEME_NATIVE = "native";
    public static final String SCHEME_THUMBNAIL = "thumbnail";

    private final AvrcpControllerService mService;
    protected final Map<BluetoothDevice, AvrcpBipClient> mClients = new ConcurrentHashMap<>(1);
    private Map<BluetoothDevice, AvrcpBipSession> mBipSessions = new ConcurrentHashMap<>(1);
    private final AvrcpCoverArtStorage mCoverArtStorage;
    private final Callback mCallback;
    private final String mDownloadScheme;

    /**
     * An object representing an image download event. Contains the information necessary to
     * retrieve the image from storage.
     */
    public class DownloadEvent {
        final String mImageUuid;
        final Uri mUri;
        public DownloadEvent(String uuid, Uri uri) {
            mImageUuid = uuid;
            mUri = uri;
        }
        public String getUuid() {
            return mImageUuid;
        }
        public Uri getUri() {
            return mUri;
        }
    }

    interface Callback {
        /**
         * Notify of a get image download completing
         *
         * @param device The device the image handle belongs to
         * @param event The download event, containing the downloaded image's information
         */
        void onImageDownloadComplete(BluetoothDevice device, DownloadEvent event);
    }

    /**
     * A thread-safe collection of BIP connection specific imformation meant to be cleared each
     * time a client disconnects from the Target's BIP OBEX server.
     *
     * Currently contains the mapping of image handles seen to assigned UUIDs.
     */
    private class AvrcpBipSession {
        private final BluetoothDevice mDevice;
        private Map<String, String> mUuids = new ConcurrentHashMap<>(1); /* handle -> UUID */
        private Map<String, String> mHandles = new ConcurrentHashMap<>(1); /* UUID -> handle */

        AvrcpBipSession(BluetoothDevice device) {
            mDevice = device;
        }

        public String getHandleUuid(String handle) {
            if (handle == null) return null;
            String newUuid = UUID.randomUUID().toString();
            String existingUuid = mUuids.putIfAbsent(handle, newUuid);
            if (existingUuid != null) return existingUuid;
            mHandles.put(newUuid, handle);
            return newUuid;
        }

        public String getUuidHandle(String uuid) {
            return mHandles.get(uuid);
        }

        public void clearHandleUuids() {
            mUuids.clear();
            mHandles.clear();
        }

        public Set<String> getSessionHandles() {
            return mUuids.keySet();
        }
    }

    public AvrcpCoverArtManager(AvrcpControllerService service, Callback callback) {
        mService = service;
        mCoverArtStorage = new AvrcpCoverArtStorage(mService);
        mCallback = callback;
        mDownloadScheme =
                SystemProperties.get(AVRCP_CONTROLLER_COVER_ART_SCHEME, SCHEME_THUMBNAIL);
        mCoverArtStorage.clear();
    }

    /**
     * Create a client and connect to a remote device's BIP Image Pull Server
     *
     * @param device The remote Bluetooth device you wish to connect to
     * @param psm The Protocol Service Multiplexer that the remote device is hosting the server on
     * @return True if the connection is successfully queued, False otherwise.
     */
    public synchronized boolean connect(BluetoothDevice device, int psm) {
        debug("Connect " + device.getAddress() + ", psm: " + psm);
        if (mClients.containsKey(device)) return false;
        AvrcpBipClient client = new AvrcpBipClient(device, psm, new BipClientCallback(device));
        mClients.put(device, client);
        mBipSessions.put(device, new AvrcpBipSession(device));
        return true;
    }

    /**
     * Refresh the OBEX session of a connected client
     *
     * @param device The remote Bluetooth device you wish to refresh
     * @return True if the refresh is successfully queued, False otherwise.
     */
    public synchronized boolean refreshSession(BluetoothDevice device) {
        debug("Refresh OBEX session for " + device.getAddress());
        AvrcpBipClient client = getClient(device);
        if (client == null) {
            warn("No client for " + device.getAddress());
            return false;
        }
        client.refreshSession();
        return true;
    }

    /**
     * Disconnect from a remote device's BIP Image Pull Server
     *
     * @param device The remote Bluetooth device you wish to disconnect from
     * @return True if the disconnection is successfully queued, False otherwise.
     */
    public synchronized boolean disconnect(BluetoothDevice device) {
        debug("Disconnect " + device.getAddress());
        AvrcpBipClient client = getClient(device);
        if (client == null) {
            warn("No client for " + device.getAddress());
            return false;
        }
        client.shutdown();
        mClients.remove(device);
        mBipSessions.remove(device);
        mCoverArtStorage.removeImagesForDevice(device);
        return true;
    }

    /**
     * Cleanup all cover art related resources
     *
     * Please call when you've committed to shutting down the service.
     */
    public synchronized void cleanup() {
        debug("Clean up and shutdown");
        for (BluetoothDevice device : mClients.keySet()) {
            disconnect(device);
        }
    }

    /**
     * Get the client connection state for a particular device's BIP Client
     *
     * @param device The Bluetooth device you want connection status for
     * @return Connection status, based on BluetoothProfile.STATE_* constants
     */
    public int getState(BluetoothDevice device) {
        AvrcpBipClient client = getClient(device);
        if (client == null) return BluetoothProfile.STATE_DISCONNECTED;
        return client.getState();
    }

     /**
     * Get the UUID for an image handle coming from a particular device.
     *
     * This UUID is used to request and track downloads.
     *
     * Image handles are only good for the life of the BIP client. Since this connection is torn
     * down frequently by specification, we have a layer of indirection to the images in the form
     * of an UUID. This UUID will allow images to be identified outside the connection lifecycle.
     * It also allows handles to be reused by the target in ways that won't impact image consumer's
     * cache schemes.
     *
     * @param device The Bluetooth device you want a handle from
     * @param handle The image handle you want a UUID for
     * @return A string UUID by which the handle can be identified during the life of the BIP
     *         connection.
     */
    public String getUuidForHandle(BluetoothDevice device, String handle) {
        AvrcpBipSession session = getSession(device);
        if (session == null || handle == null) return null;
        return session.getHandleUuid(handle);
    }

    /**
     * Get the handle thats associated with a particular UUID.
     *
     * The handle must have been seen during this connection.
     *
     * @param device The Bluetooth device you want a handle from
     * @param uuid The UUID you want the associated handle for
     * @return The image handle associated with this UUID if it exists, null otherwise.
     */
    public String getHandleForUuid(BluetoothDevice device, String uuid) {
        AvrcpBipSession session = getSession(device);
        if (session == null || uuid == null) return null;
        return session.getUuidHandle(uuid);
    }

    private void clearHandleUuids(BluetoothDevice device) {
        AvrcpBipSession session = getSession(device);
        if (session == null) return;
        session.clearHandleUuids();
    }

    /**
     * Get the Uri of an image if it has already been downloaded.
     *
     * @param device The remote Bluetooth device you wish to get an image for
     * @param imageUuid The UUID associated with the image you want
     * @return A Uri the image can be found at, null if it does not exist
     */
    public Uri getImageUri(BluetoothDevice device, String imageUuid) {
        if (mCoverArtStorage.doesImageExist(device, imageUuid)) {
            return AvrcpCoverArtProvider.getImageUri(device, imageUuid);
        }
        return null;
    }

    /**
     * Download an image from a remote device and make it findable via the given uri
     *
     * Downloading happens in three steps:
     *   1) Get the available image formats by requesting the Image Properties
     *   2) Determine the specific format we want the image in and turn it into an image descriptor
     *   3) Get the image using the chosen descriptor
     *
     * Getting image properties and the image are both asynchronous in nature.
     *
     * @param device The remote Bluetooth device you wish to download from
     * @param imageUuid The UUID associated with the image you wish to download. This will be
     *                  translated into an image handle.
     * @return A Uri that will be assign to the image once the download is complete
     */
    public Uri downloadImage(BluetoothDevice device, String imageUuid) {
        debug("Download Image - device: " + device.getAddress() + ", Handle: " + imageUuid);
        AvrcpBipClient client = getClient(device);
        if (client == null) {
            error("Cannot download an image. No client is available.");
            return null;
        }

        // Check to see if we have the image already. No need to download it if we do have it.
        if (mCoverArtStorage.doesImageExist(device, imageUuid)) {
            debug("Image is already downloaded");
            return AvrcpCoverArtProvider.getImageUri(device, imageUuid);
        }

        // Getting image properties will return via the callback created when connecting, which
        // invokes the download image function after we're returned the properties. If we already
        // have the image, GetImageProperties returns true but does not start a download.
        String imageHandle = getHandleForUuid(device, imageUuid);
        if (imageHandle == null) {
            warn("No handle for UUID");
            return null;
        }
        boolean status = client.getImageProperties(imageHandle);
        if (!status) return null;

        // Return the Uri that the caller should use to retrieve the image
        return AvrcpCoverArtProvider.getImageUri(device, imageUuid);
    }

    /**
     * Get a specific downloaded image if it exists
     *
     * @param device The remote Bluetooth device associated with the image
     * @param imageUuid The UUID associated with the image you wish to retrieve
     */
    public Bitmap getImage(BluetoothDevice device, String imageUuid) {
        return mCoverArtStorage.getImage(device, imageUuid);
    }

    /**
     * Remove a specific downloaded image if it exists
     *
     * @param device The remote Bluetooth device associated with the image
     * @param imageUuid The UUID associated with the image you wish to remove
     */
    public void removeImage(BluetoothDevice device, String imageUuid) {
        mCoverArtStorage.removeImage(device, imageUuid);
    }

    /**
     * Get a device's BIP client if it exists
     *
     * @param device The device you want the client for
     * @return The AvrcpBipClient object associated with the device, or null if it doesn't exist
     */
    private AvrcpBipClient getClient(BluetoothDevice device) {
        return mClients.get(device);
    }

    /**
     * Get a device's BIP session information, if it exists
     *
     * @param device The device you want the client for
     * @return The AvrcpBipSession object associated with the device, or null if it doesn't exist
     */
    private AvrcpBipSession getSession(BluetoothDevice device) {
        return mBipSessions.get(device);
    }

    /**
     * Determines our preferred download descriptor from the list of available image download
     * formats presented in the image properties object.
     *
     * Our goal is ensure the image arrives in a format Android can consume and to minimize transfer
     * size if possible.
     *
     * @param properties The set of available formats and image is downloadable in
     * @return A descriptor containing the desirable download format
     */
    private BipImageDescriptor determineImageDescriptor(BipImageProperties properties) {
        BipImageDescriptor.Builder builder = new BipImageDescriptor.Builder();
        switch (mDownloadScheme) {
            // BIP Specification says a blank/null descriptor signals to pull the native format
            case SCHEME_NATIVE:
                return null;
            // AVRCP 1.6.2 defined "thumbnail" size is guaranteed so we'll do that for now
            case SCHEME_THUMBNAIL:
            default:
                builder.setEncoding(BipEncoding.JPEG);
                builder.setFixedDimensions(200, 200);
                break;
        }
        return builder.build();
    }

    /**
     * Callback for facilitating image download
     */
    class BipClientCallback implements AvrcpBipClient.Callback {
        final BluetoothDevice mDevice;

        BipClientCallback(BluetoothDevice device) {
            mDevice = device;
        }

        @Override
        public void onConnectionStateChanged(int oldState, int newState) {
            debug(mDevice.getAddress() + ": " + oldState + " -> " + newState);
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                // Ensure the handle map is cleared since old ones are invalid on a new connection
                clearHandleUuids(mDevice);

                // Once we're connected fetch the current metadata again in case the target has an
                // image handle they can now give us. Only do this if we don't already have one.
                mService.getCurrentMetadataIfNoCoverArt(mDevice);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                AvrcpBipClient client = getClient(mDevice);
                boolean shouldReconnect = (client != null);
                disconnect(mDevice);
                if (shouldReconnect) {
                    debug("Disconnect was not expected by us. Attempt to reconnect.");
                    connect(mDevice, client.getL2capPsm());
                }
            }
        }

        @Override
        public void onGetImagePropertiesComplete(int status, String imageHandle,
                BipImageProperties properties) {
            if (status != ResponseCodes.OBEX_HTTP_OK || properties == null) {
                warn(mDevice.getAddress() + ": GetImageProperties() failed - Handle: " + imageHandle
                        + ", Code: " + status);
                return;
            }
            BipImageDescriptor descriptor = determineImageDescriptor(properties);
            debug(mDevice.getAddress() + ": Download image - handle='" + imageHandle + "'");

            AvrcpBipClient client = getClient(mDevice);
            if (client == null) {
                warn(mDevice.getAddress() + ": Could not getImage() for " + imageHandle
                        + " because client has disconnected.");
                return;
            }
            client.getImage(imageHandle, descriptor);
        }

        @Override
        public void onGetImageComplete(int status, String imageHandle, BipImage image) {
            if (status != ResponseCodes.OBEX_HTTP_OK) {
                warn(mDevice.getAddress() + ": GetImage() failed - Handle: " + imageHandle
                        + ", Code: " + status);
                return;
            }
            String imageUuid = getUuidForHandle(mDevice, imageHandle);
            debug(mDevice.getAddress() + ": Received image data for handle: " + imageHandle
                    + ", uuid: " + imageUuid + ", image: " + image);
            Uri uri = mCoverArtStorage.addImage(mDevice, imageUuid, image.getImage());
            if (uri == null) {
                error("Could not store downloaded image");
                return;
            }
            DownloadEvent event = new DownloadEvent(imageUuid, uri);
            if (mCallback != null) mCallback.onImageDownloadComplete(mDevice, event);
        }
    }

    @Override
    public String toString() {
        String s = "CoverArtManager:\n";
        s += "    Download Scheme: " + mDownloadScheme + "\n";
        for (BluetoothDevice device : mClients.keySet()) {
            AvrcpBipClient client = getClient(device);
            AvrcpBipSession session = getSession(device);
            s += "    " + device.getAddress() + ":" + "\n";
            s += "      Client: " + client.toString() + "\n";
            s += "      Handles: " + "\n";
            for (String handle : session.getSessionHandles()) {
                s += "        " + handle + " -> " + session.getHandleUuid(handle) + "\n";
            }
        }
        s += "  " + mCoverArtStorage.toString();
        return s;
    }

    /**
     * Print to debug if debug is enabled for this class
     */
    private void debug(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    /**
     * Print to warn
     */
    private void warn(String msg) {
        Log.w(TAG, msg);
    }

    /**
     * Print to error
     */
    private void error(String msg) {
        Log.e(TAG, msg);
    }
}
