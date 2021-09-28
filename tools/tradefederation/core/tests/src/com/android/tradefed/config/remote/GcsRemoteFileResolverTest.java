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
package com.android.tradefed.config.remote;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.gcs.GCSDownloaderHelper;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link GcsRemoteFileResolver}. */
@RunWith(JUnit4.class)
public class GcsRemoteFileResolverTest {

    private GcsRemoteFileResolver mResolver;
    private GCSDownloaderHelper mMockHelper;

    @Before
    public void setUp() {
        mMockHelper = Mockito.mock(GCSDownloaderHelper.class);
        mResolver =
                new GcsRemoteFileResolver() {
                    @Override
                    protected GCSDownloaderHelper getDownloader() {
                        return mMockHelper;
                    }
                };
    }

    @Test
    public void testResolve() throws Exception {
        mResolver.resolveRemoteFiles(new File("gs:/fake/file"), new HashMap<>());

        Mockito.verify(mMockHelper).fetchTestResource("gs:/fake/file");
    }

    @Test
    public void testResolve_error() throws Exception {
        Mockito.doThrow(
                        new BuildRetrievalError(
                                "download failure", InfraErrorIdentifier.ARTIFACT_DOWNLOAD_ERROR))
                .when(mMockHelper)
                .fetchTestResource("gs:/fake/file");

        try {
            mResolver.resolveRemoteFiles(new File("gs:/fake/file"), new HashMap<>());
            fail("Should have thrown an exception.");
        } catch (BuildRetrievalError expected) {
            assertEquals(
                    "Failed to download gs:/fake/file due to: download failure",
                    expected.getMessage());
        }

        Mockito.verify(mMockHelper).fetchTestResource("gs:/fake/file");
    }

    /** Test that we can request a zip to be unzipped automatically. */
    @Test
    public void testResolve_unzip() throws Exception {
        File testDir = FileUtil.createTempDir("test-resolve-dir");
        File zipFile = ZipUtil.createZip(testDir);
        File resolvedFile = null;
        try {
            Mockito.doReturn(zipFile).when(mMockHelper).fetchTestResource("gs:/fake/file");
            Map<String, String> query = new HashMap<>();
            query.put(DynamicRemoteFileResolver.UNZIP_KEY, /* Case doesn't matter */ "TrUe");
            resolvedFile = mResolver.resolveRemoteFiles(new File("gs:/fake/file"), query);
            // File was unzipped
            assertTrue(resolvedFile.isDirectory());
            // Zip file was cleaned
            assertFalse(zipFile.exists());

            Mockito.verify(mMockHelper).fetchTestResource("gs:/fake/file");
        } finally {
            FileUtil.recursiveDelete(testDir);
            FileUtil.deleteFile(zipFile);
            FileUtil.recursiveDelete(resolvedFile);
        }
    }

    /** Test that if we request to unzip a non-zip file, nothing is done. */
    @Test
    public void testResolve_notZip() throws Exception {
        File testFile = FileUtil.createTempFile("test-resolve-file", ".txt");
        try {
            Mockito.doReturn(testFile).when(mMockHelper).fetchTestResource("gs:/fake/file");
            Map<String, String> query = new HashMap<>();
            query.put(DynamicRemoteFileResolver.UNZIP_KEY, /* Case doesn't matter */ "TrUe");
            File resolvedFile = mResolver.resolveRemoteFiles(new File("gs:/fake/file"), query);
            // File was not unzipped since it's not one
            assertEquals(testFile, resolvedFile);

            Mockito.verify(mMockHelper).fetchTestResource("gs:/fake/file");
        } finally {
            FileUtil.deleteFile(testFile);
        }
    }
}
