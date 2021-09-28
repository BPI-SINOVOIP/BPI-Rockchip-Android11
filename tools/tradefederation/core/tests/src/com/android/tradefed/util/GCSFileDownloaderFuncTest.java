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

import com.google.api.client.googleapis.batch.BatchCallback;
import com.google.api.client.googleapis.batch.BatchRequest;
import com.google.api.client.http.HttpHeaders;
import com.google.api.client.http.InputStreamContent;
import com.google.api.services.storage.Storage;
import com.google.api.services.storage.Storage.Objects.List;
import com.google.api.services.storage.model.Objects;
import com.google.api.services.storage.model.StorageObject;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Paths;
import java.util.Collections;

/** {@link GCSFileDownloader} functional test. */
@RunWith(JUnit4.class)
public class GCSFileDownloaderFuncTest {

    private static final String BUCKET_NAME = "tradefed_function_test";
    private static final String FILE_NAME1 = "a_host_config.xml";
    private static final String FILE_NAME2 = "file2.txt";
    private static final String FILE_NAME3 = "file3.txt";
    private static final String FILE_NAME4 = "file4.txt";
    private static final String FOLDER_NAME1 = "folder1";
    private static final String FOLDER_NAME2 = "folder2";
    private static final String FILE_CONTENT = "Hello World!";

    private GCSFileDownloader mDownloader;
    private String mRemoteRoot;
    private File mLocalRoot;
    private Storage mStorage;

    private static void createFile(
            Storage storage, String content, String bucketName, String... pathSegs)
            throws IOException {
        String path = String.join("/", pathSegs);
        StorageObject object = new StorageObject();
        object.setName(path);
        storage.objects()
                .insert(
                        bucketName,
                        object,
                        new InputStreamContent(null, new ByteArrayInputStream(content.getBytes())))
                .execute();
    }

    @Before
    public void setUp() throws IOException {
        File tempFile =
                FileUtil.createTempFile(GCSFileDownloaderFuncTest.class.getSimpleName(), "");
        mRemoteRoot = tempFile.getName();
        FileUtil.deleteFile(tempFile);
        mDownloader =
                new GCSFileDownloader() {

                    @Override
                    File createTempFile(String remoteFilePath, File rootDir)
                            throws BuildRetrievalError {
                        try {
                            File tmpFile =
                                    FileUtil.createTempFileForRemote(remoteFilePath, mLocalRoot);
                            tmpFile.delete();
                            return tmpFile;
                        } catch (IOException e) {
                            throw new BuildRetrievalError(e.getMessage(), e);
                        }
                    }
                };
        mStorage =
                mDownloader.getStorage(
                        Collections.singleton(
                                "https://www.googleapis.com/auth/devstorage.read_write"));
        createFile(mStorage, FILE_CONTENT, BUCKET_NAME, mRemoteRoot, FILE_NAME1);
        createFile(mStorage, FILE_NAME2, BUCKET_NAME, mRemoteRoot, FOLDER_NAME1, FILE_NAME2);
        createFile(mStorage, FILE_NAME3, BUCKET_NAME, mRemoteRoot, FOLDER_NAME1, FILE_NAME3);
        // Create a special case condition where folder name is also a file name.
        createFile(mStorage, FILE_NAME3, BUCKET_NAME, mRemoteRoot, FOLDER_NAME1, FOLDER_NAME2);
        createFile(
                mStorage,
                FILE_NAME4,
                BUCKET_NAME,
                mRemoteRoot,
                FOLDER_NAME1,
                FOLDER_NAME2,
                FILE_NAME4);
        mLocalRoot = FileUtil.createTempDir(GCSFileDownloaderFuncTest.class.getSimpleName());
    }

    @After
    public void tearDown() throws IOException {
        FileUtil.recursiveDelete(mLocalRoot);
        String pageToken = null;
        BatchRequest batchRequest = mStorage.batch();

        while (true) {
            List listOperation = mStorage.objects().list(BUCKET_NAME).setPrefix(mRemoteRoot);
            if (pageToken == null) {
                listOperation.setPageToken(pageToken);
            }
            Objects objects = listOperation.execute();
            for (StorageObject object : objects.getItems()) {
                batchRequest.queue(
                        mStorage.objects().delete(BUCKET_NAME, object.getName()).buildHttpRequest(),
                        Void.class,
                        IOException.class,
                        new BatchCallback<Void, IOException>() {
                            @Override
                            public void onSuccess(Void arg0, HttpHeaders arg1) throws IOException {}

                            @Override
                            public void onFailure(IOException e, HttpHeaders arg1)
                                    throws IOException {
                                throw e;
                            }
                        });
            }
            pageToken = objects.getNextPageToken();
            if (pageToken == null) {
                batchRequest.execute();
                return;
            }
        }
    }

    @Test
    public void testDownloadFile_streamOutput() throws Exception {
        InputStream inputStream =
                mDownloader.downloadFile(BUCKET_NAME, mRemoteRoot + "/" + FILE_NAME1);
        String content = StreamUtil.getStringFromStream(inputStream);
        Assert.assertEquals(FILE_CONTENT, content);
        inputStream.reset();
    }

    @Test
    public void testDownloadFile_streamOutput_notExist() throws Exception {
        try {
            mDownloader.downloadFile(BUCKET_NAME, mRemoteRoot + "/" + "non_exist_file");
            Assert.fail("Should throw IOException.");
        } catch (IOException e) {
            // Expect IOException
        }
    }

    @Test
    public void testGetRemoteFileMetaData() throws Exception {
        String filename = mRemoteRoot + "/" + FILE_NAME1;
        StorageObject object = mDownloader.getRemoteFileMetaData(BUCKET_NAME, filename);
        Assert.assertEquals(filename, object.getName());
    }

    @Test
    public void testGetRemoteFileMetaData_notExist() throws Exception {
        String filename = mRemoteRoot + "/" + "not_exist";
        StorageObject object = mDownloader.getRemoteFileMetaData(BUCKET_NAME, filename);
        Assert.assertNull(object);
    }

    @Test
    public void testIsRemoteFolder() throws Exception {
        Assert.assertFalse(
                mDownloader.isRemoteFolder(
                        BUCKET_NAME, Paths.get(mRemoteRoot, FILE_NAME1).toString()));
        Assert.assertTrue(
                mDownloader.isRemoteFolder(
                        BUCKET_NAME, Paths.get(mRemoteRoot, FOLDER_NAME1).toString()));
    }

    @Test
    public void testDownloadFile() throws Exception {
        File localFile =
                mDownloader.downloadFile(
                        String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FILE_NAME1));
        String content = FileUtil.readStringFromFile(localFile);
        Assert.assertEquals(FILE_CONTENT, content);
    }

    @Test
    public void testDownloadFile_nonExist() throws Exception {
        try {
            mDownloader.downloadFile(
                    String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, "non_exist_file"));
            Assert.fail("Should throw BuildRetrievalError.");
        } catch (BuildRetrievalError e) {
            // Expect BuildRetrievalError
        }
    }

    @Test
    public void testDownloadFile_folder() throws Exception {
        File localFile =
                mDownloader.downloadFile(
                        String.format("gs://%s/%s/%s/", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1));
        checkDownloadedFolder(localFile);
    }

    @Test
    public void testDownloadFile_folderNotsanitize() throws Exception {
        File localFile =
                mDownloader.downloadFile(
                        String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1));
        checkDownloadedFolder(localFile);
    }

    private void checkDownloadedFolder(File localFile) throws Exception {
        Assert.assertTrue(localFile.isDirectory());
        Assert.assertEquals(4, localFile.list().length);
        for (String filename : localFile.list()) {
            if (filename.equals(FILE_NAME2)) {
                Assert.assertEquals(
                        FILE_NAME2,
                        FileUtil.readStringFromFile(
                                new File(localFile.getAbsolutePath(), filename)));
            } else if (filename.equals(FILE_NAME3)) {
                Assert.assertEquals(
                        FILE_NAME3,
                        FileUtil.readStringFromFile(
                                new File(localFile.getAbsolutePath(), filename)));
            } else if (filename.equals(FOLDER_NAME2 + "_folder")) {
                File subFolder = new File(localFile.getAbsolutePath(), filename);
                Assert.assertTrue(subFolder.isDirectory());
                Assert.assertEquals(1, subFolder.list().length);
                Assert.assertEquals(
                        FILE_NAME4,
                        FileUtil.readStringFromFile(
                                new File(subFolder.getAbsolutePath(), subFolder.list()[0])));
            } else if (filename.equals(FOLDER_NAME2)) {
                File fileWithFolderName = new File(localFile.getAbsolutePath(), filename);
                Assert.assertTrue(fileWithFolderName.isFile());
            } else {
                Assert.assertTrue(String.format("Unknonwn file %s", filename), false);
            }
        }
    }

    @Test
    public void testDownloadFile_folder_nonExist() throws Exception {
        try {
            mDownloader.downloadFile(
                    String.format("gs://%s/%s/%s/", BUCKET_NAME, "mRemoteRoot", "nonExistFolder"));
            Assert.fail("Should throw BuildRetrievalError.");
        } catch (BuildRetrievalError e) {
            // Expect BuildRetrievalError
        }
    }

    @Test
    public void testCheckFreshness() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FILE_NAME1);
        File localFile = mDownloader.downloadFile(remotePath);
        Assert.assertTrue(mDownloader.isFresh(localFile, remotePath));
    }

    @Test
    public void testCheckFreshness_notExist() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FILE_NAME1);
        Assert.assertFalse(mDownloader.isFresh(new File("/not/exist"), remotePath));
    }

    @Test
    public void testCheckFreshness_folderNotExist() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1);
        Assert.assertFalse(mDownloader.isFresh(new File("/not/exist"), remotePath));
    }

    @Test
    public void testCheckFreshness_remoteNotExist() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FILE_NAME1);
        String remoteNotExistPath = String.format("gs://%s/%s/no_exist", BUCKET_NAME, mRemoteRoot);
        File localFile = mDownloader.downloadFile(remotePath);
        Assert.assertFalse(mDownloader.isFresh(localFile, remoteNotExistPath));
    }

    @Test
    public void testCheckFreshness_remoteFolderNotExist() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1);
        String remoteNotExistPath = String.format("gs://%s/%s/no_exist/", BUCKET_NAME, mRemoteRoot);
        File localFolder = mDownloader.downloadFile(remotePath);
        Assert.assertFalse(mDownloader.isFresh(localFolder, remoteNotExistPath));
    }

    @Test
    public void testCheckFreshness_notFresh() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FILE_NAME1);
        File localFile = mDownloader.downloadFile(remotePath);
        // Change the remote file.
        createFile(mStorage, "New content.", BUCKET_NAME, mRemoteRoot, FILE_NAME1);
        Assert.assertFalse(mDownloader.isFresh(localFile, remotePath));
    }

    @Test
    public void testCheckFreshness_folder() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1);
        File localFolder = mDownloader.downloadFile(remotePath);
        Assert.assertTrue(mDownloader.isFresh(localFolder, remotePath));
    }

    @Test
    public void testCheckFreshness_folder_addFile() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1);
        File localFolder = mDownloader.downloadFile(remotePath);
        createFile(
                mStorage,
                "A new file",
                BUCKET_NAME,
                mRemoteRoot,
                FOLDER_NAME1,
                FOLDER_NAME2,
                "new_file.txt");
        Assert.assertFalse(mDownloader.isFresh(localFolder, remotePath));
    }

    @Test
    public void testCheckFreshness_folder_removeFile() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1);
        File localFolder = mDownloader.downloadFile(remotePath);
        mStorage.objects()
                .delete(BUCKET_NAME, Paths.get(mRemoteRoot, FOLDER_NAME1, FILE_NAME3).toString())
                .execute();
        Assert.assertFalse(mDownloader.isFresh(localFolder, remotePath));
    }

    @Test
    public void testCheckFreshness_folder_changeFile() throws Exception {
        String remotePath = String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1);
        File localFolder = mDownloader.downloadFile(remotePath);
        createFile(mStorage, "New content", BUCKET_NAME, mRemoteRoot, FOLDER_NAME1, FILE_NAME3);
        Assert.assertFalse(mDownloader.isFresh(localFolder, remotePath));
    }
}
