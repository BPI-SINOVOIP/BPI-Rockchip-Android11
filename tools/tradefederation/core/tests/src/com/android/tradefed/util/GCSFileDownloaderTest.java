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

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit test for {@link GCSFileDownloader}. */
@RunWith(JUnit4.class)
public class GCSFileDownloaderTest {

    private File mLocalRoot;
    private GCSFileDownloader mGCSFileDownloader;

    @Before
    public void setUp() {
        mGCSFileDownloader =
                new GCSFileDownloader() {
                    @Override
                    void downloadFile(String bucketName, String filename, File localFile)
                            throws BuildRetrievalError {
                        try {
                            FileUtil.writeToFile(bucketName + "\n" + filename, localFile);
                        } catch (Exception e) {
                            throw new BuildRetrievalError(e.getMessage(), e);
                        }
                    }
                };
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mLocalRoot);
    }

    @Test
    public void testDownloadFile() throws Exception {
        File localFile = null;
        try {
            localFile = mGCSFileDownloader.downloadFile("gs://bucket/this/is/a/file.txt");
            String content = FileUtil.readStringFromFile(localFile);
            Assert.assertEquals("bucket\nthis/is/a/file.txt", content);
        } finally {
            FileUtil.deleteFile(localFile);
        }
    }

    @Test
    public void testDownloadFile_nonGCSFile() {
        try {
            mGCSFileDownloader.downloadFile("/this/is/a/file.txt");
            Assert.assertTrue("Expect to throw BuildRetrievalError", false);
        } catch (BuildRetrievalError e) {
            Assert.assertTrue(e.getMessage().startsWith("Only GCS path is supported"));
        }
    }

    @Test
    public void testSanitizeDirectoryName() {
        Assert.assertEquals(
                "/this/is/a/folder/",
                mGCSFileDownloader.sanitizeDirectoryName("/this/is/a/folder"));
        Assert.assertEquals(
                "/this/is/a/folder/",
                mGCSFileDownloader.sanitizeDirectoryName("/this/is/a/folder/"));
    }

    @Test
    public void testParseGcsPath() throws Exception {
        String[] parts = mGCSFileDownloader.parseGcsPath("gs://bucketname/path/to/file");
        Assert.assertEquals("bucketname", parts[0]);
        Assert.assertEquals("path/to/file", parts[1]);
    }

    @Test
    public void testparseGcsPath_singleSlash() throws Exception {

        String gcsPath = "gs:/bucketName/path/to/file";
        String[] parts = mGCSFileDownloader.parseGcsPath(gcsPath);
        Assert.assertEquals("bucketName", parts[0]);
        Assert.assertEquals("path/to/file", parts[1]);
    }

    @Test
    public void testParseGcsPath_noGCSPath() {
        try {
            mGCSFileDownloader.parseGcsPath("/bucketname/path/to/file");
            Assert.assertTrue("Should throw exception.", false);
        } catch (BuildRetrievalError e) {
            // Expected
        }
    }
}
