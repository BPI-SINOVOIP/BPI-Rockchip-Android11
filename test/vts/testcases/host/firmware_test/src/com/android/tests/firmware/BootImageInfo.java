/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tests.firmware;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class BootImageInfo implements AutoCloseable {
    // Common definitions to all boot header versions.
    static int KERNEL_SIZE_OFFSET = 2 * 4;
    static int HOST_IMG_HEADER_VER_OFFSET = 10 * 4;

    // Definitions specific to boot header versions < 3.
    static int RAMDISK_SIZE_OFFSET = 4 * 4;
    static int PAGE_SIZE_OFFSET = 9 * 4;
    // Offset of recovery dtbo size in boot header of version 1.
    static int BOOT_HEADER_DTBO_SIZE_OFFSET = 1632;
    static int BOOT_HEADER_SIZE_OFFSET = BOOT_HEADER_DTBO_SIZE_OFFSET + 4 + 8;
    static int DTB_SIZE_OFFSET = BOOT_HEADER_SIZE_OFFSET + 4;

    // Definitions specific to boot header version 3.
    static int V3_RAMDISK_SIZE_OFFSET = 3 * 4;
    static int V3_BOOT_HEADER_SIZE_OFFSET = 5 * 4;
    static int V3_BOOT_HEADER_SIZE = 1580;

    private int mKernelSize = 0;
    private int mRamdiskSize = 0;
    private int mPageSize = 0;
    private int mImgHeaderVer = 0;
    private int mRecoveryDtboSize = 0;
    private int mBootHeaderSize = 0;
    private int mDtbSize = 0;
    private RandomAccessFile mRaf = null;

    /**
     * Create a {@link BootImageInfo}.
     */
    public BootImageInfo(String imagePath) throws IOException {
        File bootImg = new File(imagePath);
        mRaf = new RandomAccessFile(bootImg, "r");
        byte[] bytes = new byte[4];

        mRaf.seek(KERNEL_SIZE_OFFSET);
        mRaf.read(bytes);
        mKernelSize = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();

        mRaf.seek(HOST_IMG_HEADER_VER_OFFSET);
        mRaf.read(bytes);
        mImgHeaderVer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();

        if (mImgHeaderVer < 3) {
            mRaf.seek(RAMDISK_SIZE_OFFSET);
        } else {
            mRaf.seek(V3_RAMDISK_SIZE_OFFSET);
        }
        mRaf.read(bytes);
        mRamdiskSize = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();

        if (mImgHeaderVer < 3) {
            mRaf.seek(PAGE_SIZE_OFFSET);
            mRaf.read(bytes);
            mPageSize = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();
        } else {
            mPageSize = 4096;
        }

        if (mImgHeaderVer < 3) {
            mRaf.seek(BOOT_HEADER_DTBO_SIZE_OFFSET);
            mRaf.read(bytes);
            mRecoveryDtboSize = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();
        } else {
            mRecoveryDtboSize = -1;
        }

        if (mImgHeaderVer < 3) {
            mRaf.seek(BOOT_HEADER_SIZE_OFFSET);
        } else {
            mRaf.seek(V3_BOOT_HEADER_SIZE_OFFSET);
        }
        mRaf.read(bytes);
        mBootHeaderSize = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();

        if (mImgHeaderVer == 2) {
            mRaf.seek(DTB_SIZE_OFFSET);
            mRaf.read(bytes);
            mDtbSize = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();
        } else {
            mDtbSize = -1;
        }
    }

    /**
     * Get kernel size of boot image.
     *
     * @return the value of kernel size.
     */
    public int getKernelSize() {
        return mKernelSize;
    }

    public void setKernelSize(int size) {
        mKernelSize = size;
    }

    /**
     * Get ramdisk size of boot image.
     *
     * @return the value of ramdisk size.
     */
    public int getRamdiskSize() {
        return mRamdiskSize;
    }

    public void setRamdiskSize(int size) {
        mRamdiskSize = size;
    }

    /**
     * Get page size of boot image.
     *
     * @return the value of page size.
     */
    public int getPageSize() {
        return mPageSize;
    }

    public void setPageSize(int size) {
        mPageSize = size;
    }

    /**
     * Get image header version of boot image.
     *
     * @return the value of host image header version.
     */
    public int getImgHeaderVer() {
        return mImgHeaderVer;
    }

    public void setImgHeaderVer(int version) {
        mImgHeaderVer = version;
    }

    /**
     * Get recovery dtbo size of boot image.
     *
     * @return the value of recovery dtbo size.
     */
    public int getRecoveryDtboSize() {
        return mRecoveryDtboSize;
    }

    public void setRecoveryDtboSize(int size) {
        mRecoveryDtboSize = size;
    }

    /**
     * Get boot header size of boot image.
     *
     * @return the value of boot header size.
     */
    public int getBootHeaderSize() {
        return mBootHeaderSize;
    }

    public void setBootHeaderSize(int size) {
        mBootHeaderSize = size;
    }

    /**
     * Get dtb size of boot image.
     *
     * @return the value of dtb size.
     */
    public int getDtbSize() {
        return mDtbSize;
    }

    public void setDtbSize(int size) {
        mDtbSize = size;
    }

    /**
     * Get expect header size of boot image.
     *
     * @return the value of expected header size.
     */
    public int getExpectHeaderSize() {
        switch (mImgHeaderVer) {
            case 1:
                return BOOT_HEADER_SIZE_OFFSET + 4;
            case 2:
                return BOOT_HEADER_SIZE_OFFSET + 16;
            case 3:
                return V3_BOOT_HEADER_SIZE;
            default:
                return -1;
        }
    }

    /**
     * Get kernel page numbers of boot image.
     *
     * @return the value of kernel page numbers.
     */
    int getKernelPageNum() {
        return (mKernelSize + mPageSize - 1) / mPageSize;
    }

    /**
     * Get the content of ramdisk.
     *
     * @return the content of ramdisk.
     */
    public byte[] getRamdiskStream() throws IOException {
        byte[] tmpBytes = new byte[mRamdiskSize];
        int ramDiskOffset = mPageSize * (1 + getKernelPageNum());
        mRaf.seek(ramDiskOffset);
        mRaf.read(tmpBytes);
        return tmpBytes;
    }

    @Override
    public void close() throws Exception {
        if (mRaf != null) {
            mRaf.close();
        }
    }
}
