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

package com.android.tradefed.util.zip;

import com.android.tradefed.util.ByteArrayUtil;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;

/**
 * LocalFileHeader is a class containing the information of a file/folder inside a zip file. The
 * block of data is at the beginning part of each file entry.
 *
 * <p>Overall zipfile format: [Local file header + Compressed data [+ Extended local header]?]*
 * [Central directory]* [End of central directory record]
 *
 * <p>Refer to following link for more details: https://en.wikipedia.org/wiki/Zip_(file_format)
 */
public final class LocalFileHeader {

    public static final int LOCAL_FILE_HEADER_SIZE = 30;
    private static final byte[] LOCAL_FILE_HEADER_SIGNATURE = {0x50, 0x4b, 0x03, 0x04};

    private int mCompressionMethod;
    private long mCrc;
    private long mCompressedSize;
    private long mUncompressedSize;
    private int mFileNameLength;
    private int mExtraFieldLength;

    public int getCompressionMethod() {
        return mCompressionMethod;
    }

    public long getCrc() {
        return mCrc;
    }

    public long getCompressedSize() {
        return mCompressedSize;
    }

    public long getUncompressedSize() {
        return mUncompressedSize;
    }

    public int getFileNameLength() {
        return mFileNameLength;
    }

    public int getExtraFieldLength() {
        return mExtraFieldLength;
    }

    public int getHeaderSize() {
        return LOCAL_FILE_HEADER_SIZE + mFileNameLength + mExtraFieldLength;
    }

    public LocalFileHeader(File partialZipFile) throws IOException {
        this(partialZipFile, 0);
    }

    /**
     * Constructor to collect local file header information of a file entry in a zip file.
     *
     * @param partialZipFile a {@link File} contains the local file header information.
     * @param startOffset the start offset of the block of data for a local file header.
     * @throws IOException
     */
    public LocalFileHeader(File partialZipFile, int startOffset) throws IOException {
        // Local file header:
        //    Offset   Length   Contents
        //      0      4 bytes  Local file header signature (0x04034b50)
        //      4      2 bytes  Version needed to extract
        //      6      2 bytes  General purpose bit flag
        //      8      2 bytes  Compression method
        //     10      2 bytes  Last mod file time
        //     12      2 bytes  Last mod file date
        //     14      4 bytes  CRC-32
        //     18      4 bytes  Compressed size (n)
        //     22      4 bytes  Uncompressed size
        //     26      2 bytes  Filename length (f)
        //     28      2 bytes  Extra field length (e)
        //            (f)bytes  Filename
        //            (e)bytes  Extra field
        //            (n)bytes  Compressed data
        byte[] data;
        try (FileInputStream stream = new FileInputStream(partialZipFile)) {
            stream.skip(startOffset);
            data = new byte[LOCAL_FILE_HEADER_SIZE];
            stream.read(data);
        }

        // Check signature
        if (!Arrays.equals(LOCAL_FILE_HEADER_SIGNATURE, Arrays.copyOfRange(data, 0, 4))) {
            throw new IOException("Invalid local file header for zip file is found.");
        }
        mCompressionMethod = ByteArrayUtil.getInt(data, 8, 2);
        mCrc = ByteArrayUtil.getLong(data, 14, 4);
        mCompressedSize = ByteArrayUtil.getLong(data, 18, 2);
        mUncompressedSize = ByteArrayUtil.getLong(data, 22, 2);
        mFileNameLength = ByteArrayUtil.getInt(data, 26, 2);
        mExtraFieldLength = ByteArrayUtil.getInt(data, 28, 2);
    }
}
