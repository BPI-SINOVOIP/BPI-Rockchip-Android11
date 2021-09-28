/*
 * Copyright (C) 2018 The Android Open Source Project
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
import com.android.tradefed.result.error.InfraErrorIdentifier;

import com.google.api.client.googleapis.json.GoogleJsonResponseException;
import com.google.api.services.storage.Storage;
import com.google.api.services.storage.model.Objects;
import com.google.api.services.storage.model.StorageObject;
import com.google.common.annotations.VisibleForTesting;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.SocketException;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** File downloader to download file from google cloud storage (GCS). */
public class GCSFileDownloader extends GCSCommon implements IFileDownloader {
    public static final String GCS_PREFIX = "gs://";
    public static final String GCS_APPROX_PREFIX = "gs:/";

    private static final Pattern GCS_PATH_PATTERN = Pattern.compile("gs://([^/]*)/(.*)");
    private static final String PATH_SEP = "/";
    private static final Collection<String> SCOPES =
            Collections.singleton("https://www.googleapis.com/auth/devstorage.read_only");
    private static final long LIST_BATCH_SIZE = 100;

    public GCSFileDownloader(File jsonKeyFile) {
        super(jsonKeyFile);
    }

    public GCSFileDownloader() {}

    /**
     * Download a file from a GCS bucket file.
     *
     * @param bucketName GCS bucket name
     * @param filename the filename
     * @return {@link InputStream} with the file content.
     */
    public InputStream downloadFile(String bucketName, String filename) throws IOException {
        InputStream remoteInput = null;
        ByteArrayOutputStream tmpStream = null;
        try {
            remoteInput =
                    getStorage().objects().get(bucketName, filename).executeMediaAsInputStream();
            // The input stream from api call can not be reset. Change it to ByteArrayInputStream.
            tmpStream = new ByteArrayOutputStream();
            StreamUtil.copyStreams(remoteInput, tmpStream);
            return new ByteArrayInputStream(tmpStream.toByteArray());
        } finally {
            StreamUtil.close(remoteInput);
            StreamUtil.close(tmpStream);
        }
    }

    private Storage getStorage() throws IOException {
        return getStorage(SCOPES);
    }

    @VisibleForTesting
    StorageObject getRemoteFileMetaData(String bucketName, String remoteFilename)
            throws IOException {
        try {
            return getStorage().objects().get(bucketName, remoteFilename).execute();
        } catch (GoogleJsonResponseException e) {
            if (e.getStatusCode() == 404) {
                return null;
            }
            throw e;
        }
    }

    /**
     * Download file from GCS.
     *
     * <p>Right now only support GCS path.
     *
     * @param remoteFilePath gs://bucket/file/path format GCS path.
     * @return local file
     * @throws BuildRetrievalError
     */
    @Override
    public File downloadFile(String remoteFilePath) throws BuildRetrievalError {
        File destFile = createTempFile(remoteFilePath, null);
        try {
            downloadFile(remoteFilePath, destFile);
            return destFile;
        } catch (BuildRetrievalError e) {
            FileUtil.recursiveDelete(destFile);
            throw e;
        }
    }

    @Override
    public void downloadFile(String remotePath, File destFile) throws BuildRetrievalError {
        String[] pathParts = parseGcsPath(remotePath);
        downloadFile(pathParts[0], pathParts[1], destFile);
    }

    private boolean isFileFresh(File localFile, StorageObject remoteFile) throws IOException {
        if (localFile == null && remoteFile == null) {
            return true;
        }
        if (localFile == null || remoteFile == null) {
            return false;
        }
        if (!localFile.exists()) {
            return false;
        }
        return remoteFile.getMd5Hash().equals(FileUtil.calculateBase64Md5(localFile));
    }

    @Override
    public boolean isFresh(File localFile, String remotePath) throws BuildRetrievalError {
        String[] pathParts = parseGcsPath(remotePath);
        String bucketName = pathParts[0];
        String remoteFilename = pathParts[1];
        try {
            StorageObject remoteFileMeta = getRemoteFileMetaData(bucketName, remoteFilename);
            if (localFile == null || !localFile.exists()) {
                if (!isRemoteFolder(bucketName, remoteFilename) && remoteFileMeta == null) {
                    // The local doesn't exist and the remote filename is not a folder or a file.
                    return true;
                }
                return false;
            }
            if (!localFile.isDirectory()) {
                return isFileFresh(localFile, remoteFileMeta);
            }
            remoteFilename = sanitizeDirectoryName(remoteFilename);
            return recursiveCheckFolderFreshness(bucketName, remoteFilename, localFile);
        } catch (IOException e) {
            throw new BuildRetrievalError(e.getMessage(), e);
        }
    }

    /**
     * Check if remote folder is the same as local folder, recursively. The remoteFolderName must
     * end with "/".
     *
     * @param bucketName is the gcs bucket name.
     * @param remoteFolderName is the relative path to the bucket.
     * @param localFolder is the local folder
     * @return true if local file is the same as remote file, otherwise false.
     * @throws IOException
     */
    private boolean recursiveCheckFolderFreshness(
            String bucketName, String remoteFolderName, File localFolder) throws IOException {
        Set<String> subFilenames = new HashSet<>(Arrays.asList(localFolder.list()));
        List<String> subRemoteFolders = new ArrayList<>();
        List<StorageObject> subRemoteFiles = new ArrayList<>();
        listRemoteFilesUnderFolder(bucketName, remoteFolderName, subRemoteFiles, subRemoteFolders);
        for (StorageObject subRemoteFile : subRemoteFiles) {
            String subFilename = Paths.get(subRemoteFile.getName()).getFileName().toString();
            if (!isFileFresh(new File(localFolder, subFilename), subRemoteFile)) {
                return false;
            }
            subFilenames.remove(subFilename);
        }
        for (String subRemoteFolder : subRemoteFolders) {
            String subFolderName = Paths.get(subRemoteFolder).getFileName().toString();
            File subFolder = new File(localFolder, subFolderName);
            if (new File(localFolder, subFolderName).exists()
                    && !new File(localFolder, subFolderName).isDirectory()) {
                CLog.w("%s exists as a non-directory.", subFolder);
                subFolder = new File(localFolder, subFolderName + "_folder");
            }
            if (!recursiveCheckFolderFreshness(bucketName, subRemoteFolder, subFolder)) {
                return false;
            }
            subFilenames.remove(subFolder.getName());
        }
        return subFilenames.isEmpty();
    }

    void listRemoteFilesUnderFolder(
            String bucketName, String folder, List<StorageObject> subFiles, List<String> subFolders)
            throws IOException {
        String pageToken = null;
        while (true) {
            com.google.api.services.storage.Storage.Objects.List listOperation =
                    getStorage()
                            .objects()
                            .list(bucketName)
                            .setPrefix(folder)
                            .setDelimiter(PATH_SEP)
                            .setMaxResults(LIST_BATCH_SIZE);
            if (pageToken != null) {
                listOperation.setPageToken(pageToken);
            }
            Objects objects = listOperation.execute();
            if (objects.getItems() != null && !objects.getItems().isEmpty()) {
                subFiles.addAll(objects.getItems());
            }
            if (objects.getPrefixes() != null && !objects.getPrefixes().isEmpty()) {
                subFolders.addAll(objects.getPrefixes());
            }
            pageToken = objects.getNextPageToken();
            if (pageToken == null) {
                return;
            }
        }
    }

    String[] parseGcsPath(String remotePath) throws BuildRetrievalError {
        if (remotePath.startsWith(GCS_APPROX_PREFIX) && !remotePath.startsWith(GCS_PREFIX)) {
            // File object remove double // so we have to rebuild it in some cases
            remotePath = remotePath.replaceAll(GCS_APPROX_PREFIX, GCS_PREFIX);
        }
        Matcher m = GCS_PATH_PATTERN.matcher(remotePath);
        if (!m.find()) {
            throw new BuildRetrievalError(
                    String.format("Only GCS path is supported, %s is not supported", remotePath),
                    InfraErrorIdentifier.ARTIFACT_UNSUPPORTED_PATH);
        }
        return new String[] {m.group(1), m.group(2)};
    }

    String sanitizeDirectoryName(String name) {
        /** Folder name should end with "/" */
        if (!name.endsWith(PATH_SEP)) {
            name += PATH_SEP;
        }
        return name;
    }

    /** check given filename is a folder or not. */
    @VisibleForTesting
    boolean isRemoteFolder(String bucketName, String filename) throws IOException {
        filename = sanitizeDirectoryName(filename);
        Objects objects =
                getStorage()
                        .objects()
                        .list(bucketName)
                        .setPrefix(filename)
                        .setDelimiter(PATH_SEP)
                        .setMaxResults(1l)
                        .execute();
        if (objects.getItems() != null && !objects.getItems().isEmpty()) {
            return true;
        }
        if (objects.getPrefixes() != null && !objects.getPrefixes().isEmpty()) {
            return true;
        }
        return false;
    }

    @VisibleForTesting
    void downloadFile(String bucketName, String remoteFilename, File localFile)
            throws BuildRetrievalError {
        int i = 0;
        try {
            do {
                i++;
                try {
                    if (!isRemoteFolder(bucketName, remoteFilename)) {
                        fetchRemoteFile(bucketName, remoteFilename, localFile);
                        return;
                    }
                    remoteFilename = sanitizeDirectoryName(remoteFilename);
                    recursiveDownloadFolder(bucketName, remoteFilename, localFile);
                    return;
                } catch (SocketException se) {
                    // Allow one retry in case of flaky connection.
                    if (i >= 2) {
                        throw se;
                    }
                    CLog.e(
                            "Error '%s' while downloading gs://%s/%s. retrying.",
                            se.getMessage(), bucketName, remoteFilename);
                }
            } while (true);
        } catch (IOException e) {
            CLog.e("Failed to download gs://%s/%s, clean up.", bucketName, remoteFilename);
            throw new BuildRetrievalError(e.getMessage(), e);
        }
    }

    private void fetchRemoteFile(String bucketName, String remoteFilename, File localFile)
            throws IOException {
        try (OutputStream writeStream = new FileOutputStream(localFile)) {
            getStorage()
                    .objects()
                    .get(bucketName, remoteFilename)
                    .executeMediaAndDownloadTo(writeStream);
        }
    }

    /**
     * Recursively download remote folder to local folder.
     *
     * @param bucketName the gcs bucket name
     * @param remoteFolderName remote folder name, must end with "/"
     * @param localFolder local folder
     * @throws IOException
     */
    private void recursiveDownloadFolder(
            String bucketName, String remoteFolderName, File localFolder) throws IOException {
        CLog.d("Downloading folder gs://%s/%s.", bucketName, remoteFolderName);
        if (!localFolder.exists()) {
            FileUtil.mkdirsRWX(localFolder);
        }
        if (!localFolder.isDirectory()) {
            String error =
                    String.format(
                            "%s is not a folder. (gs://%s/%s)",
                            localFolder, bucketName, remoteFolderName);
            CLog.e(error);
            throw new IOException(error);
        }
        Set<String> subFilenames = new HashSet<>(Arrays.asList(localFolder.list()));
        List<String> subRemoteFolders = new ArrayList<>();
        List<StorageObject> subRemoteFiles = new ArrayList<>();
        listRemoteFilesUnderFolder(bucketName, remoteFolderName, subRemoteFiles, subRemoteFolders);
        for (StorageObject subRemoteFile : subRemoteFiles) {
            String subFilename = Paths.get(subRemoteFile.getName()).getFileName().toString();
            fetchRemoteFile(
                    bucketName, subRemoteFile.getName(), new File(localFolder, subFilename));
            subFilenames.remove(subFilename);
        }
        for (String subRemoteFolder : subRemoteFolders) {
            String subFolderName = Paths.get(subRemoteFolder).getFileName().toString();
            File subFolder = new File(localFolder, subFolderName);
            if (new File(localFolder, subFolderName).exists()
                    && !new File(localFolder, subFolderName).isDirectory()) {
                CLog.w("%s exists as a non-directory.", subFolder);
                subFolder = new File(localFolder, subFolderName + "_folder");
            }
            recursiveDownloadFolder(bucketName, subRemoteFolder, subFolder);
            subFilenames.remove(subFolder.getName());
        }
        for (String subFilename : subFilenames) {
            FileUtil.recursiveDelete(new File(localFolder, subFilename));
        }
    }

    /**
     * Creates a unique file on temporary disk to house downloaded file with given path.
     *
     * <p>Constructs the file name based on base file name from path
     *
     * @param remoteFilePath the remote path to construct the name from
     */
    @VisibleForTesting
    File createTempFile(String remoteFilePath, File rootDir) throws BuildRetrievalError {
        try {
            // create a unique file.
            File tmpFile = FileUtil.createTempFileForRemote(remoteFilePath, rootDir);
            // now delete it so name is available
            tmpFile.delete();
            return tmpFile;
        } catch (IOException e) {
            String msg = String.format("Failed to create tmp file for %s", remoteFilePath);
            throw new BuildRetrievalError(msg, e);
        }
    }
}
