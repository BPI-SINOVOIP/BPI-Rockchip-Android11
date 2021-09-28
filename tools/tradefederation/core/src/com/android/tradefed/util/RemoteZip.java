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
package com.android.tradefed.util;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IFileDownloader;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.zip.CentralDirectoryInfo;
import com.android.tradefed.util.zip.EndCentralDirectoryInfo;
import com.android.tradefed.util.zip.LocalFileHeader;
import com.android.tradefed.util.zip.MergedZipEntryCollection;

import java.io.File;
import java.io.IOException;
import java.nio.file.Paths;
import java.util.List;

/** Utilities to unzip individual files inside a remote zip file. */
public class RemoteZip {

    private String mRemoteFilePath;
    private List<CentralDirectoryInfo> mZipEntries;
    private long mFileSize;
    private IFileDownloader mDownloader;
    // Last time this object is accessed. The timestamp is used to maintain the cache of RemoteZip
    // objects.
    private long mLastAccess;
    private boolean mUseZip64;

    /**
     * Constructor
     *
     * @param remoteFilePath the remote path to the file to download.
     * @param fileSize size of the remote file.
     * @param downloader a @{link IFileDownloader} used to download a remote file.
     * @param useZip64 whether to use zip64 format for partial download or not.
     */
    public RemoteZip(
            String remoteFilePath,
            long fileSize,
            IFileDownloader downloader,
            boolean useZip64) {
        mRemoteFilePath = remoteFilePath;
        mFileSize = fileSize;
        mDownloader = downloader;
        mZipEntries = null;
        mUseZip64 = useZip64;
        mLastAccess = System.currentTimeMillis();
    }

    /**
     * Constructor
     *
     * @param remoteFilePath the remote path to the file to download.
     * @param fileSize size of the remote file.
     * @param downloader a @{link IFileDownloader} used to download a remote file.
     */
    public RemoteZip(String remoteFilePath, long fileSize, IFileDownloader downloader) {
        this(remoteFilePath, fileSize, downloader, false);
    }

    /** Get the remote file path of the remote zip artifact. */
    public String getRemoteFilePath() {
        return mRemoteFilePath;
    }

    /** Get the last time this object is accessed. */
    public long getLastAccess() {
        return mLastAccess;
    }

    /** Update the last access timestamp of the object. */
    public void setLastAccess(long timestamp) {
        mLastAccess = timestamp;
    }

    /**
     * Gets the zip file entries of a remote zip file.
     *
     * @throws BuildRetrievalError if file could not be downloaded.
     */
    public List<CentralDirectoryInfo> getZipEntries() throws BuildRetrievalError, IOException {
        if (mZipEntries != null) {
            return mZipEntries;
        }

        File partialZipFile = FileUtil.createTempFileForRemote(mRemoteFilePath, null);
        // Delete it so name is available
        partialZipFile.delete();
        try {
            // Get the end central directory of the zip file requested.
            // Download last 64kb (or entire file if the size is less than 64kb)
            long size = EndCentralDirectoryInfo.MAX_LOOKBACK;
            long startOffset = mFileSize - size;
            if (startOffset < 0) {
                // The default lookback size is greater than the size of the file, so read the whole
                // file by setting size to -1.
                startOffset = 0;
                size = -1;
            }

            mDownloader.downloadFile(mRemoteFilePath, partialZipFile, startOffset, size);
            EndCentralDirectoryInfo endCentralDirInfo =
                    new EndCentralDirectoryInfo(partialZipFile, mUseZip64);
            partialZipFile.delete();

            // Read central directory infos
            mDownloader.downloadFile(
                    mRemoteFilePath,
                    partialZipFile,
                    endCentralDirInfo.getCentralDirOffset(),
                    endCentralDirInfo.getCentralDirSize());

            mZipEntries =
                    ZipUtil.getZipCentralDirectoryInfos(
                            partialZipFile,
                            endCentralDirInfo,
                            mUseZip64);
            return mZipEntries;
        } catch (IOException e) {
            throw new BuildRetrievalError(
                    String.format("Failed to get zip entries of remote file %s", mRemoteFilePath),
                    e);
        } finally {
            FileUtil.deleteFile(partialZipFile);
        }
    }

    /**
     * Download the specified files in the remote zip file.
     *
     * @param destDir the directory to place the downloaded files to.
     * @param files a list of entries to download from the remote zip file.
     * @throws BuildRetrievalError
     * @throws IOException
     */
    public void downloadFiles(File destDir, List<CentralDirectoryInfo> files)
            throws BuildRetrievalError, IOException {
        long totalDownloadedSize = 0;
        long startTime = System.currentTimeMillis();

        // Merge the entries into sections to minimize the download attempts.
        List<MergedZipEntryCollection> collections =
                MergedZipEntryCollection.createCollections(files);
        CLog.d(
                "Downloading %d files from remote zip file %s in %d sections.",
                files.size(), mRemoteFilePath, collections.size());
        for (MergedZipEntryCollection collection : collections) {
            File partialZipFile = null;
            try {
                partialZipFile = FileUtil.createTempFileForRemote(mRemoteFilePath, null);
                // Delete it so name is available
                partialZipFile.delete();
                // End offset is based on the maximum guess of local file header size (2KB). So it
                // can exceed the file size.
                long downloadedSize = collection.getEndOffset() - collection.getStartOffset();
                if (collection.getStartOffset() + downloadedSize > mFileSize) {
                    downloadedSize = mFileSize - collection.getStartOffset();
                }
                mDownloader.downloadFile(
                        mRemoteFilePath,
                        partialZipFile,
                        collection.getStartOffset(),
                        downloadedSize);
                totalDownloadedSize += downloadedSize;

                // Extract each file from the partial download.
                for (CentralDirectoryInfo entry : collection.getZipEntries()) {
                    File targetFile =
                            new File(Paths.get(destDir.toString(), entry.getFileName()).toString());
                    LocalFileHeader localFileHeader =
                            new LocalFileHeader(
                                    partialZipFile,
                                    (int)
                                            (entry.getLocalHeaderOffset()
                                                    - collection.getStartOffset()));
                    ZipUtil.unzipPartialZipFile(
                            partialZipFile,
                            targetFile,
                            entry,
                            localFileHeader,
                            entry.getLocalHeaderOffset() - collection.getStartOffset());
                }
            } finally {
                FileUtil.deleteFile(partialZipFile);
            }
        }
        CLog.d(
                "%d files downloaded from remote zip file in %s. Total download size: %,d bytes.",
                files.size(),
                TimeUtil.formatElapsedTime(System.currentTimeMillis() - startTime),
                totalDownloadedSize);
    }
}
