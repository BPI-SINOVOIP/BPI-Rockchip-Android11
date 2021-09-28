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
 * limitations under the License
 */

package com.android.tradefed.util;

import com.google.api.services.storage.Storage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collections;

/** {@link GCSFileUploader} functional test. */
@RunWith(JUnit4.class)
public class GCSFileUploaderFuncTest {
    private static final String BUCKET_NAME = "tradefed_function_test";
    private static final String FILE_DATA = "Simple test string to write to file.";
    private static final String FILE_MIME_TYPE = "text/plain";
    private static final boolean ENABLE_OVERWRITE = true;
    private static final boolean DISABLE_OVERWRITE = false;

    private File mTestFile;

    private GCSFileUploader mUploader;
    private GCSFileDownloader mDownloader;

    @Before
    public void setUp() throws Exception {
        mUploader = new GCSFileUploader();
        mDownloader = new GCSFileDownloader();
        mTestFile = FileUtil.createTempFile("test-upload-file", ".txt");
    }

    @After
    public void tearDown() throws IOException {
        try {
            Storage storage =
                    mUploader.getStorage(
                            Collections.singleton(
                                    "https://www.googleapis.com/auth/devstorage.read_write"));
            storage.objects().delete(BUCKET_NAME, mTestFile.getName()).execute();
        } finally {
            FileUtil.recursiveDelete(mTestFile);
        }
    }

    @Test
    public void testUploadFile_roundTrip() throws Exception {
        InputStream uploadFileStream = new ByteArrayInputStream(FILE_DATA.getBytes());
        mUploader.uploadFile(
                BUCKET_NAME,
                mTestFile.getName(),
                uploadFileStream,
                FILE_MIME_TYPE,
                ENABLE_OVERWRITE);
        String readBack = new String(toByteArray(pullFileFromGcs(mTestFile.getName())));
        Assert.assertEquals(FILE_DATA, readBack);
    }

    @Test
    public void testUploadFile_overwrite() throws Exception {
        InputStream uploadFileStream = new ByteArrayInputStream(FILE_DATA.getBytes());
        mUploader.uploadFile(
                BUCKET_NAME,
                mTestFile.getName(),
                uploadFileStream,
                FILE_MIME_TYPE,
                DISABLE_OVERWRITE);

        try {
            mUploader.uploadFile(
                    BUCKET_NAME,
                    mTestFile.getName(),
                    uploadFileStream,
                    FILE_MIME_TYPE,
                    DISABLE_OVERWRITE);
            Assert.fail("Should throw IOException.");
        } catch (IOException e) {
            // Expect IOException
        }

        mUploader.uploadFile(
                BUCKET_NAME,
                mTestFile.getName(),
                uploadFileStream,
                FILE_MIME_TYPE,
                ENABLE_OVERWRITE);
    }

    private byte[] toByteArray(InputStream in) throws IOException {

        ByteArrayOutputStream os = new ByteArrayOutputStream();

        byte[] buffer = new byte[1024];
        int len;

        // read bytes from the input stream and store them in buffer
        while ((len = in.read(buffer)) != -1) {
            // write bytes from the buffer into output stream
            os.write(buffer, 0, len);
        }

        return os.toByteArray();
    }

    private InputStream pullFileFromGcs(String gcsFilePath) throws IOException {
        return mDownloader.downloadFile(BUCKET_NAME, gcsFilePath);
    }
}
