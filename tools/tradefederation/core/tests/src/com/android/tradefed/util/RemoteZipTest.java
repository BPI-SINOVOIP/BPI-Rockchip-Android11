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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.IFileDownloader;
import com.android.tradefed.util.zip.CentralDirectoryInfo;
import com.android.tradefed.util.zip.EndCentralDirectoryInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;
import org.mockito.stubbing.Answer;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link RemoteZip} */
@RunWith(JUnit4.class)
public class RemoteZipTest {

    private static final String REMOTE_FILE =
            "aosp_main-linux-yakju-userdebug/P123/device-tests.zip";

    private IFileDownloader mDownloader;
    private List<CentralDirectoryInfo> mExpectedEntries;
    private long mZipFileSize;

    private void saveTestDataFile(File destFile, long startOffset, long size) {
        try {
            final InputStream inputStream =
                    ZipUtilTest.class.getResourceAsStream("/util/partial_zip.zip");
            FileUtil.writeToFile(inputStream, destFile, false, startOffset, size);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Before
    public void setUp() throws Exception {
        mDownloader = Mockito.mock(IFileDownloader.class);

        Mockito.doAnswer(
                        (Answer<?>)
                                invocation -> {
                                    File destFile = (File) invocation.getArgument(1);
                                    long startOffset = (long) invocation.getArgument(2);
                                    long size = (long) invocation.getArgument(3);
                                    saveTestDataFile(destFile, startOffset, size);
                                    return null;
                                })
                .when(mDownloader)
                .downloadFile(
                        Mockito.eq(REMOTE_FILE),
                        Mockito.any(),
                        Mockito.anyLong(),
                        Mockito.anyLong());

        File zipFile = FileUtil.createTempFileForRemote("zipfile", null);
        // Delete it so name is available
        zipFile.delete();
        try {
            saveTestDataFile(zipFile, -1, 0);
            EndCentralDirectoryInfo endCentralDirInfo = new EndCentralDirectoryInfo(zipFile);
            mExpectedEntries =
                    ZipUtil.getZipCentralDirectoryInfos(
                            zipFile, endCentralDirInfo, endCentralDirInfo.getCentralDirOffset());
            mZipFileSize = zipFile.length();
        } finally {
            FileUtil.deleteFile(zipFile);
        }
    }

    @Test
    public void testGetZipEntries() throws Exception {
        File destDir = null;
        try {
            destDir = FileUtil.createTempDir("test");

            RemoteZip remoteZip = new RemoteZip(REMOTE_FILE, mZipFileSize, mDownloader);

            List<CentralDirectoryInfo> entries = remoteZip.getZipEntries();

            assertEquals(7, entries.size());
            assertTrue(mExpectedEntries.containsAll(entries));
        } finally {
            FileUtil.recursiveDelete(destDir);
        }
    }

    /**
     * Test get zip entries with use-zip64-in-partial-download is set.
     *
     * @throws Exception
     */
    @Test
    public void testGetZipEntriesWithUseZip64() throws Exception {
        File destDir = null;
        try {
            destDir = FileUtil.createTempDir("test");
            RemoteZip remoteZip = new RemoteZip(REMOTE_FILE, mZipFileSize, mDownloader, true);
            List<CentralDirectoryInfo> entries = remoteZip.getZipEntries();
            assertEquals(7, entries.size());
            assertTrue(mExpectedEntries.containsAll(entries));
        } finally {
            FileUtil.recursiveDelete(destDir);
        }
    }

    @Test
    public void testDownloadFilesFromZip() throws Exception {
        File destDir = null;
        try {
            destDir = FileUtil.createTempDir("test");

            List<CentralDirectoryInfo> files = new ArrayList<>();
            for (CentralDirectoryInfo info : mExpectedEntries) {
                if (info.getFileName().equals("large_text/file.txt")
                        || info.getFileName().equals("executable/executable_file")) {
                    files.add(info);
                }
            }

            RemoteZip remoteZip = new RemoteZip(REMOTE_FILE, mZipFileSize, mDownloader);
            remoteZip.downloadFiles(destDir, files);

            File targetFile = Paths.get(destDir.getPath(), "large_text", "file.txt").toFile();
            assertEquals(4146093769L, FileUtil.calculateCrc32(targetFile));
            targetFile = Paths.get(destDir.getPath(), "executable", "executable_file").toFile();
            assertTrue(targetFile.exists());
            // File not in the list is not unzipped.
            targetFile = Paths.get(destDir.getPath(), "empty_file").toFile();
            assertFalse(targetFile.exists());
        } finally {
            FileUtil.recursiveDelete(destDir);
        }
    }
}
