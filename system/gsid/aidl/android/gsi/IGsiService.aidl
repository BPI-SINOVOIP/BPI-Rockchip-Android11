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
import android.gsi.GsiProgress;
import android.gsi.IGsiServiceCallback;
import android.gsi.IImageService;
import android.os.ParcelFileDescriptor;

/** {@hide} */
interface IGsiService {
    /* Status codes for GsiProgress.status */
    const int STATUS_NO_OPERATION = 0;
    const int STATUS_WORKING = 1;
    const int STATUS_COMPLETE = 2;

    /* Install succeeded. */
    const int INSTALL_OK = 0;
    /* Install failed with a generic system error. */
    const int INSTALL_ERROR_GENERIC = 1;
    /* Install failed because there was no free space. */
    const int INSTALL_ERROR_NO_SPACE = 2;
    /**
     * Install failed because the file system was too fragmented or did not
     * have enough additional free space.
     */
    const int INSTALL_ERROR_FILE_SYSTEM_CLUTTERED = 3;

    /**
     * Write bytes from a stream to the on-disk GSI.
     *
     * @param stream        Stream descriptor.
     * @param bytes         Number of bytes that can be read from stream.
     * @return              true on success, false otherwise.
     */
    boolean commitGsiChunkFromStream(in ParcelFileDescriptor stream, long bytes);

    /**
     * Query the progress of the current asynchronous install operation. This
     * can be called while another operation is in progress.
     */
    GsiProgress getInstallProgress();

    /**
     * Set the file descriptor that points to a ashmem which will be used
     * to fetch data during the commitGsiChunkFromAshmem.
     *
     * @param stream        fd that points to a ashmem
     * @param size          size of the ashmem file
     */
    boolean setGsiAshmem(in ParcelFileDescriptor stream, long size);

    /**
     * Write bytes from ashmem previously set with setGsiAshmem to GSI partition
     *
     * @param bytes         Number of bytes to submit
     * @return              true on success, false otherwise.
     */
    boolean commitGsiChunkFromAshmem(long bytes);

    /**
     * Complete a GSI installation and mark it as bootable. The caller is
     * responsible for rebooting the device as soon as possible.
     *
     * @param oneShot       If true, the GSI will boot once and then disable itself.
     *                      It can still be re-enabled again later with setGsiBootable.
     * @param dsuSlot       The DSU slot to be enabled. Possible values are available
     *                      with the getInstalledDsuSlots()
     *
     * @return              INSTALL_* error code.
     */
    int enableGsi(boolean oneShot, @utf8InCpp String dsuSlot);

    /**
     * Asynchronous enableGsi
     * @param result        callback for result
     */
    oneway void enableGsiAsync(boolean oneShot, @utf8InCpp String dsuSlot, IGsiServiceCallback result);

    /**
     * @return              True if Gsi is enabled
     */
    boolean isGsiEnabled();

    /**
     * Cancel an in-progress GSI install.
     */
    boolean cancelGsiInstall();

    /**
     * Return if a GSI installation is currently in-progress.
     */
    boolean isGsiInstallInProgress();

    /**
     * Remove a GSI install. This will completely remove and reclaim space used
     * by the GSI and its userdata. If currently running a GSI, space will be
     * reclaimed on the reboot.
     *
     * @return              true on success, false otherwise.
     */
    boolean removeGsi();

    /**
     * Asynchronous removeGsi
     * @param result        callback for result
     */
    oneway void removeGsiAsync(IGsiServiceCallback result);

    /**
     * Disables a GSI install. The image and userdata will be retained, but can
     * be re-enabled at any time with setGsiBootable.
     */
    boolean disableGsi();

    /**
     * Returns true if a gsi is installed.
     */
    boolean isGsiInstalled();
    /**
     * Returns true if the gsi is currently running, false otherwise.
     */
    boolean isGsiRunning();

    /**
     * Returns the active DSU slot if there is any DSU installed, empty string otherwise.
     */
    @utf8InCpp String getActiveDsuSlot();

    /**
     * If a GSI is installed, returns the directory where the installed images
     * are located. Otherwise, returns an empty string.
     */
    @utf8InCpp String getInstalledGsiImageDir();

    /**
     * Returns all installed DSU slots.
     */
    @utf8InCpp List<String> getInstalledDsuSlots();

    /**
     * Open a DSU installation
     *
     * @param installDir The directory to install DSU images under. This must be
     *     either an empty string (which will use the default /data/gsi),
     *     "/data/gsi", or a mount under /mnt/media_rw. It may end in a trailing slash.
     *
     * @return              0 on success, an error code on failure.
     */
    int openInstall(in @utf8InCpp String installDir);

    /**
     * Close a DSU installation. An installation is complete after the close been invoked.
     */
    int closeInstall();

    /**
     * Create a DSU partition within the current installation
     *
     * @param name The DSU partition name
     * @param size Bytes in the partition
     * @param readOnly True if the partition is readOnly when DSU is running
     */
    int createPartition(in @utf8InCpp String name, long size, boolean readOnly);

    /**
     * Wipe a partition. This will not work if the GSI is currently running.
     * The partition will not be removed, but the first block will be zeroed.
     *
     * @param name The DSU partition name
     *
     * @return              0 on success, an error code on failure.
     */
    int zeroPartition(in @utf8InCpp String name);

    /**
     * Open a handle to an IImageService for the given metadata and data storage paths.
     *
     * @param prefix        A prefix used to organize images. The data path will become
     *                      /data/gsi/{prefix} and the metadata path will become
     *                      /metadata/gsi/{prefix}.
     */
    IImageService openImageService(@utf8InCpp String prefix);

    /**
     * Dump diagnostic information about device-mapper devices. This is intended
     * for dumpstate.
     */
    @utf8InCpp String dumpDeviceMapperDevices();

    /**
     * Retrieve AVB public key from the current mapped partition.
     * This works only while partition device is mapped and the end-of-partition
     * AVB footer has been written.
     * A call to createPartition() does the following things:
     * 1. Close the previous partition installer, thus unmap the partition.
     * 2. Open a new partition installer.
     * 3. Create and map the new partition.
     *
     * In other words, getAvbPublicKey() works between two createPartition() calls.
     *
     * @param dst           Output the AVB public key.
     * @return              0 on success, an error code on failure.
     */
    int getAvbPublicKey(out AvbPublicKey dst);
}
