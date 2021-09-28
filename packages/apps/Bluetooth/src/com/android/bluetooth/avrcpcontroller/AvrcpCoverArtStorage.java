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
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.util.Log;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * An abstraction of the cover art image storage mechanism.
 */
public class AvrcpCoverArtStorage {
    private static final String TAG = "AvrcpCoverArtStorage";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    private final Context mContext;

    /* Each device gets its own place to land images. This makes it easier to clean things up on a
     * per device basis. This also allows us to be confident that acting on one device will not
     * impact the images of another.
     *
     * The "landing place" is simply a map that will direct a given UUID to the proper bitmap image
     */
    private final Map<BluetoothDevice, Map<String, Bitmap>> mDeviceImages =
            new ConcurrentHashMap<>(1);

    /**
     * Create and initialize this Cover Art storage interface
     */
    public AvrcpCoverArtStorage(Context context) {
        mContext = context;
    }

    /**
     * Determine if an image already exists in storage
     *
     * @param device - The device the images was downloaded from
     * @param imageUuid - The UUID that identifies the image
     */
    public boolean doesImageExist(BluetoothDevice device, String imageUuid) {
        if (device == null || imageUuid == null || "".equals(imageUuid)) return false;
        Map<String, Bitmap> images = mDeviceImages.get(device);
        if (images == null) return false;
        return images.containsKey(imageUuid);
    }

    /**
     * Retrieve an image file from storage
     *
     * @param device - The device the images was downloaded from
     * @param imageUuid - The UUID that identifies the image
     * @return A Bitmap object of the image
     */
    public Bitmap getImage(BluetoothDevice device, String imageUuid) {
        if (device == null || imageUuid == null || "".equals(imageUuid)) return null;
        Map<String, Bitmap> images = mDeviceImages.get(device);
        if (images == null) return null;
        return images.get(imageUuid);
    }

    /**
     * Add an image to storage
     *
     * @param device - The device the images was downloaded from
     * @param imageUuid - The UUID that identifies the image
     * @param image - The image
     */
    public Uri addImage(BluetoothDevice device, String imageUuid, Bitmap image) {
        debug("Storing image '" + imageUuid + "' from device " + device);
        if (device == null || imageUuid == null || "".equals(imageUuid) || image == null) {
            debug("Cannot store image. Improper aruguments");
            return null;
        }

        // A Thread safe way of creating a new UUID->Image set for a device. The putIfAbsent()
        // function will return the value of the key if it wasn't absent. If it returns null, then
        // there was no value there and we are to assume the reference we passed in was added.
        Map<String, Bitmap> newImageSet = new ConcurrentHashMap<String, Bitmap>(1);
        Map<String, Bitmap> images = mDeviceImages.putIfAbsent(device, newImageSet);
        if (images == null) {
            newImageSet.put(imageUuid, image);
        } else {
            images.put(imageUuid, image);
        }

        Uri uri = AvrcpCoverArtProvider.getImageUri(device, imageUuid);
        mContext.getContentResolver().notifyChange(uri, null);
        debug("Image '" + imageUuid + "' stored for device '" + device.getAddress() + "'");
        return uri;
    }

    /**
     * Remove a specific image
     *
     * @param device The device the image belongs to
     * @param imageUuid - The UUID that identifies the image
     */
    public void removeImage(BluetoothDevice device, String imageUuid) {
        debug("Removing image '" + imageUuid + "' from device " + device);
        if (device == null || imageUuid == null || "".equals(imageUuid)) return;

        Map<String, Bitmap> images = mDeviceImages.get(device);
        if (images == null) {
            return;
        }

        images.remove(imageUuid);
        if (images.size() == 0) {
            mDeviceImages.remove(device);
        }

        debug("Image '" + imageUuid + "' removed for device '" + device.getAddress() + "'");
    }

    /**
     * Remove all stored images associated with a device
     *
     * @param device The device you wish to have images removed for
     */
    public void removeImagesForDevice(BluetoothDevice device) {
        if (device == null) return;
        debug("Remove cover art for device " + device.getAddress());
        mDeviceImages.remove(device);
    }

    /**
     * Clear the entirety of storage
     */
    public void clear() {
        debug("Clearing all images");
        mDeviceImages.clear();
    }

    @Override
    public String toString() {
        String s = "CoverArtStorage:\n";
        for (BluetoothDevice device : mDeviceImages.keySet()) {
            Map<String, Bitmap> images = mDeviceImages.get(device);
            s += "  " + device.getAddress() + " (" + images.size() + "):";
            for (String uuid : images.keySet()) {
                s += "\n    " + uuid;
            }
            s += "\n";
        }
        return s;
    }

    private void debug(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    private void error(String msg) {
        Log.e(TAG, msg);
    }
}
