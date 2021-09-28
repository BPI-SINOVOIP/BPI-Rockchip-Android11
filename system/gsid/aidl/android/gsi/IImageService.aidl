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

package android.gsi;

import android.gsi.AvbPublicKey;
import android.gsi.MappedImage;
import android.gsi.IProgressCallback;

/** {@hide} */
interface IImageService {
    /* These flags match fiemap::ImageManager::CreateBackingImage. */
    const int CREATE_IMAGE_DEFAULT = 0x0;
    const int CREATE_IMAGE_READONLY = 0x1;
    const int CREATE_IMAGE_ZERO_FILL = 0x2;

    /* Successfully returned */
    const int IMAGE_OK = 0;
    /* Generic error code */
    const int IMAGE_ERROR = 1;

    /**
     * Create an image that can be mapped as a block device.
     *
     * This call will fail if running a GSI.
     *
     * @param name          Image name. If the image already exists, the call will fail.
     * @param size          Image size, in bytes. If too large, or not enough space is
     *                      free, the call will fail.
     * @param readonly      If readonly, MapBackingImage() will configure the device as
     *                      readonly.
     * @param on_progress   Progress callback. It is invoked when there is an interesting update.
     *                      For each invocation, |current| is the number of bytes actually written,
     *                      and |total| is set to |size|.
     * @throws ServiceSpecificException if any error occurs. Exception code is a
     *                      FiemapStatus::ErrorCode value.
     */
    void createBackingImage(@utf8InCpp String name, long size, int flags,
                            @nullable IProgressCallback on_progress);

    /**
     * Delete an image created with createBackingImage.
     *
     * @param name          Image name as passed to createBackingImage().
     * @throws ServiceSpecificException if any error occurs.
     */
    void deleteBackingImage(@utf8InCpp String name);

    /**
     * Map an image, created with createBackingImage, such that it is accessible as a
     * block device.
     *
     * @param name          Image name as passed to createBackingImage().
     * @param timeout_ms    Time to wait for a valid mapping, in milliseconds. This must be more
     *                      than zero; 10 seconds is recommended.
     * @param mapping       Information about the newly mapped block device.
     */
    void mapImageDevice(@utf8InCpp String name, int timeout_ms, out MappedImage mapping);

    /**
     * Unmap a block device previously mapped with mapBackingImage. This step is necessary before
     * calling deleteBackingImage.
     *
     * @param name          Image name as passed to createBackingImage().
     */
    void unmapImageDevice(@utf8InCpp String name);

    /**
     * Returns whether or not a backing image exists.
     *
     * @param name          Image name as passed to createBackingImage().
     */
    boolean backingImageExists(@utf8InCpp String name);

    /**
     * Returns whether or not the named image is mapped.
     *
     * @param name          Image name as passed to createBackingImage().
     */
    boolean isImageMapped(@utf8InCpp String name);

    /**
     * Retrieve AVB public key from an image.
     * If the image is already mapped then it works the same as
     * IGsiService::getAvbPublicKey(). Otherwise this will attempt to
     * map / unmap the partition image upon enter / return.
     *
     * @param name          Image name as passed to createBackingImage().
     * @param dst           Output of the AVB public key.
     * @return              0 on success, an error code on failure.
     */
    int getAvbPublicKey(@utf8InCpp String name, out AvbPublicKey dst);

    /**
     * Get all installed backing image names
     *
     * @return list of installed backing image names
     */
    @utf8InCpp List<String> getAllBackingImages();

    /**
     * Writes a given amount of zeros in image file.
     *
     * @param name          Image name. If the image does not exist, the call
     *                      will fail.
     * @param bytes         Number of zeros to be written, starting from the
     *                      beginning. If bytes is equal to 0, then the whole
     *                      image file is filled with zeros.
     * @throws ServiceSpecificException if any error occurs. Exception code is a
     *                      FiemapStatus::ErrorCode value.
     */
    void zeroFillNewImage(@utf8InCpp String name, long bytes);

    /**
     * Find and remove all images in the containing folder of this instance.
     */
    void removeAllImages();

    /**
     * Remove all images that were marked as disabled in recovery.
     */
    void removeDisabledImages();

    /**
     * Return the block device path of a mapped image, or an empty string if not mapped.
     */
    @utf8InCpp String getMappedImageDevice(@utf8InCpp String name);
}
