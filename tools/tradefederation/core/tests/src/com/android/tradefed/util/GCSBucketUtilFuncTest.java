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

import com.android.tradefed.util.GCSBucketUtil.GCSFileMetadata;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.List;

/** {@link GCSBucketUtil} functional test. */
@RunWith(JUnit4.class)
public class GCSBucketUtilFuncTest {

    private static final String BUCKET_NAME = "tradefed_function_test";
    private static final String FILE_NAME = "a_host_config.xml";
    private static final String FOLDER_NAME = "folder";
    private static final String FILE_CONTENT = "Hello World!";
    private static final long TIMEOUT = 10000;
    private String mRemoteRoot;
    private File mLocalRoot;
    private GCSBucketUtil mBucket;

    @Before
    public void setUp() throws IOException {
        mBucket = new GCSBucketUtil(BUCKET_NAME);
        File tempFile = FileUtil.createTempFile(GCSBucketUtilFuncTest.class.getSimpleName(), "");
        mRemoteRoot = tempFile.getName();
        FileUtil.deleteFile(tempFile);

        mBucket.setBotoConfig(null);
        mBucket.setBotoPath(null);
        mBucket.setRecursive(false);
        mBucket.setTimeoutMs(TIMEOUT);

        mLocalRoot = FileUtil.createTempDir(GCSBucketUtilFuncTest.class.getSimpleName());
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mLocalRoot);
        mBucket.setRecursive(true);
        mBucket.remove(mRemoteRoot, true);
    }

    @Test
    public void testStringUploadThenDownLoad() throws Exception {
        Path path = Paths.get(mRemoteRoot, FILE_NAME);
        mBucket.pushString(FILE_CONTENT, path);
        Assert.assertEquals(FILE_CONTENT, mBucket.pullContents(path));
    }

    @Test
    public void testDownloadMultiple() throws Exception {
        List<String> expectedFiles = Arrays.asList("A", "B", "C");
        for(String file : expectedFiles) {
            mBucket.pushString(FILE_CONTENT, Paths.get(mRemoteRoot, file));
        }

        mBucket.setRecursive(true);
        mBucket.pull(Paths.get(mRemoteRoot), mLocalRoot);

        // This need to be notice. "gsutil cp -r gs://bucket/remote local" will create local/remote
        // instead of copying files inside gs://bucket/remote to local.
        File local = new File(mLocalRoot, mRemoteRoot);
        List<String> actualFiles = Arrays.asList(local.list());
        for(String expected : expectedFiles) {
            if(actualFiles.indexOf(expected) == -1) {
                Assert.fail(
                        String.format(
                                "Could not find file %s in gs://%s/%s [have: %s]",
                                expected, BUCKET_NAME, mRemoteRoot, actualFiles));
            }
        }
    }

    @Test
    public void testUploadDownload() throws IOException {
        File tempSrc = FileUtil.createTempFile("src", "", mLocalRoot);
        File tempDst = FileUtil.createTempFile("dst", "", mLocalRoot);
        FileUtil.writeToFile(FILE_CONTENT, tempDst);
        mBucket.push(tempSrc, Paths.get(mRemoteRoot, FILE_NAME));
        mBucket.pull(Paths.get(mRemoteRoot, FILE_NAME), tempDst);
        Assert.assertTrue(
                "File contents should match", FileUtil.compareFileContents(tempSrc, tempDst));
    }

    @Test
    public void testDownloadFile_notExist() throws Exception {
        try {
            mBucket.pullContents(Paths.get(mRemoteRoot, "non_exist_file"));
            Assert.fail("Should throw IOExcepiton.");
        } catch (IOException e) {
            // Expect IOException
        }
    }

    @Test
    public void testRemoveFile_notExist() throws Exception {
        try {
            mBucket.remove(Paths.get(mRemoteRoot, "non_exist_file"));
            Assert.fail("Should throw IOExcepiton.");
        } catch (IOException e) {
            // Expect IOException
        }
    }

    @Test
    public void testRemoveFile_notExist_force() throws Exception {
        mBucket.remove(Paths.get(mRemoteRoot, "non_exist_file"), true);
    }

    @Test
    public void testBotoPathCanBeSet() throws Exception {
        mBucket.setBotoPath("/dev/null");
        mBucket.setBotoConfig("/dev/null");
        try {
            testStringUploadThenDownLoad();
            Assert.fail("Should throw IOExcepiton.");
        } catch (IOException e) {
            // expected
        }
    }

    @Test
    public void testLs_folder() throws Exception {
        List<String> expectedFiles = Arrays.asList("A", "B", "C");
        for (String file : expectedFiles) {
            mBucket.pushString(FILE_CONTENT, Paths.get(mRemoteRoot, file));
        }
        List<String> files = mBucket.ls(Paths.get(mRemoteRoot));
        Assert.assertEquals(expectedFiles.size(), files.size());
        for (int i = 0; i < expectedFiles.size(); ++i) {
            Assert.assertEquals(
                    String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, expectedFiles.get(i)),
                    files.get(i));
        }
    }

    @Test
    public void testLs_file() throws Exception {
        Path path = Paths.get(mRemoteRoot, FILE_NAME);
        mBucket.pushString(FILE_CONTENT, path);
        List<String> files = mBucket.ls(Paths.get(mRemoteRoot, FILE_NAME));
        Assert.assertEquals(1, files.size());
        Assert.assertEquals(
                String.format("gs://%s/%s/%s", BUCKET_NAME, mRemoteRoot, FILE_NAME), files.get(0));
    }

    @Test
    public void testIsFile() throws Exception {
        Path path = Paths.get(mRemoteRoot, FOLDER_NAME, FILE_NAME);
        mBucket.pushString(FILE_CONTENT, path);
        Assert.assertTrue(mBucket.isFile(mRemoteRoot + "/" + FOLDER_NAME + "/" + FILE_NAME));
    }

    @Test
    public void testIsFile_folder() throws Exception {
        Path path = Paths.get(mRemoteRoot, FOLDER_NAME, FILE_NAME);
        mBucket.pushString(FILE_CONTENT, path);
        Assert.assertFalse(mBucket.isFile(mRemoteRoot + "/" + FOLDER_NAME));
    }

    @Test
    public void testIsFile_endWithPathSep() throws Exception {
        Assert.assertFalse(mBucket.isFile(mRemoteRoot + "/" + FOLDER_NAME + "/"));
    }

    @Test
    public void testStat() throws Exception {
        Path path = Paths.get(mRemoteRoot, FILE_NAME);
        mBucket.pushString(FILE_CONTENT, path);
        GCSFileMetadata info = mBucket.stat(path);
        Assert.assertNotNull(info.mMd5Hash);
        Assert.assertEquals(mBucket.getUriForGcsPath(path), info.mName);
    }

    @Test
    public void testmd5Hash() throws Exception {
        Path path = Paths.get(mRemoteRoot, FILE_NAME);
        mBucket.pushString(FILE_CONTENT, path);
        GCSFileMetadata info = mBucket.stat(path);

        File localFile = FileUtil.createTempFile(FILE_NAME, "", mLocalRoot);
        FileUtil.writeToFile(FILE_CONTENT, localFile);
        Assert.assertEquals(info.mMd5Hash, mBucket.md5Hash(localFile));
    }
}
