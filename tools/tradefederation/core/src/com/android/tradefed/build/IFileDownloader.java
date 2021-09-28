/*
 * Copyright (C) 2011 The Android Open Source Project
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
package com.android.tradefed.build;

import java.io.File;
import java.io.IOException;
import java.util.List;

/**
 * Interface for downloading a remote file.
 */
public interface IFileDownloader {

    /**
     * Downloads a remote file to a temporary file on local disk.
     *
     * @param remoteFilePath the remote path to the file to download, relative to a implementation
     * specific root.
     * @return the temporary local downloaded {@link File}.
     * @throws BuildRetrievalError if file could not be downloaded
     */
    public File downloadFile(String remoteFilePath) throws BuildRetrievalError;

    /**
     * Alternate form of {@link #downloadFile(String)}, that allows caller to specify the
     * destination file the remote contents should be placed in.
     *
     * @param relativeRemotePath the remote path to the file to download, relative to an
     *        implementation-specific root.
     * @param destFile the file to place the downloaded contents into. Should not exist.
     * @throws BuildRetrievalError if file could not be downloaded
     */
    public void downloadFile(String relativeRemotePath, File destFile) throws BuildRetrievalError;

    /**
     * Alternate form of {@link #downloadFile(String, File)}, that allows caller to download a
     * section of the file and save to a specific destination file.
     *
     * @param remoteFilePath the remote path to the file to download, relative to an
     *     implementation-specific root.
     * @param destFile the file to place the downloaded contents into. Should not exist.
     * @param startOffset the start offset in the remote file.
     * @param size the number of bytes to download from the remote file. Set it to a negative value
     *     to download the whole file.
     * @throws BuildRetrievalError if file could not be downloaded
     */
    public default void downloadFile(
            String remoteFilePath, File destFile, long startOffset, long size)
            throws BuildRetrievalError {
        throw new UnsupportedOperationException("Partial downloading is not implemented.");
    }

    /**
     * Check local file's freshness. If local file is the same as remote file, then it's fresh. If
     * not, local file is stale. This is mainly used for cache. The default implementation will
     * always return true, so if the file is immutable it will never need to check freshness.
     *
     * @param localFile local file.
     * @param remoteFilePath remote file path.
     * @return True if local file is fresh, otherwise false.
     * @throws BuildRetrievalError
     */
    public default boolean isFresh(File localFile, String remoteFilePath)
            throws BuildRetrievalError {
        return true;
    }

    /**
     * Download the files matching given filters in a remote zip file.
     *
     * <p>A file inside the remote zip file is only downloaded to its path matches any of the
     * include filters but not the exclude filters.
     *
     * @param destDir the file to place the downloaded contents into.
     * @param remoteFilePath the remote path to the file to download, relative to an implementation
     *     specific root.
     * @param includeFilters a list of filters to download matching files.
     * @param excludeFilters a list of filters to skip downloading matching files.
     * @throws BuildRetrievalError if files could not be downloaded.
     * @throws IOException
     */
    public default void downloadZippedFiles(
            File destDir,
            String remoteFilePath,
            List<String> includeFilters,
            List<String> excludeFilters)
            throws BuildRetrievalError, IOException {
        throw new UnsupportedOperationException();
    }
}
