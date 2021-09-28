/*
 * Copyright (C) 2020 The Android Open Source Project
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

import com.android.tradefed.util.zip.CentralDirectoryInfo;
import com.android.tradefed.util.zip.EndCentralDirectoryInfo;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.RandomAccessFile;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.zip.ZipFile;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import static org.junit.Assert.fail;

/** {@link ZipUtilFuncTest} functional test.*/
@RunWith(JUnit4.class)
public class ZipUtilFuncTest {

    static private File sLargeFile = null;
    static private File sTempDir = null;
    static final private String OVERFLOW = "ffffffff";
    static final private String TEMPDIR = "Zip64Test";
    static final private int SENTRIES = 700;

    @BeforeClass
    public static void setUpBeforeClass() throws Exception {
        // Programmingly make one large zip file.
        sLargeFile = FileUtil.createTempFile(TEMPDIR, ".zip");
        sTempDir = FileUtil.createTempDir(TEMPDIR);
        createLargeZipFile();
    }

    @AfterClass
    public static void tearDownAfterClass() throws Exception {
        FileUtil.deleteFile(sLargeFile);
        FileUtil.recursiveDelete(sTempDir);
    }

    /** Functional test for {@link EndCentralDirectoryInfo} to ensure the number of entries are the
     *  same when use-zip64-in-partial-download is set and file is larger than 4GB. */
    @Test
    public void testCentralDirectoryInfoSize() throws IOException {
        EndCentralDirectoryInfo endCentralDirInfo = new EndCentralDirectoryInfo(sLargeFile, true);
        Assert.assertEquals(SENTRIES, endCentralDirInfo.getEntryNumber());
    }

    /**
     * Functional test for {@link EndCentralDirectoryInfo} to ensure the central directory offset
     * is overflow when use-zip64-in-partial-download is not set and file is larger than 4GB.
     */
    @Test
    public void testCentralDirectoryOffsetWithoutUseZip64() throws IOException {
        EndCentralDirectoryInfo endCentralDirInfo = new EndCentralDirectoryInfo(sLargeFile, false);
        Assert.assertEquals(OVERFLOW, Long.toHexString(endCentralDirInfo.getCentralDirOffset()));
    }

    /**
     * Functional test for {@link EndCentralDirectoryInfo} to ensure the central directory data
     * is good when use-zip64-in-partial-download is set and file is larger than 4GB.
     */
    @Test
    public void testCentralDirectoryInfos() throws IOException {
        EndCentralDirectoryInfo endCentralDirInfo = new EndCentralDirectoryInfo(sLargeFile, true);

        // Use ZipFile to make ensure the contents are equal.
        ZipFile zipFile = new ZipFile(sLargeFile.getAbsolutePath());
        List<CentralDirectoryInfo> zipEntries =
                ZipUtil.getZipCentralDirectoryInfos(
                        sLargeFile,
                        endCentralDirInfo,
                        endCentralDirInfo.getCentralDirOffset(),
                        true);
        Assert.assertEquals(zipFile.size(), endCentralDirInfo.getEntryNumber());
        for (CentralDirectoryInfo entry : zipEntries) {
            Assert.assertFalse(validateCentralDirectoryInfo(entry));
        }
        validateCentralDirectoryInfos(zipFile, zipEntries);
    }

    /**
     * Functional test for {@link EndCentralDirectoryInfo}  to ensure getting expected exception
     * when use-zip64-in-partial-download is set and file is larger than 4GB.
     */
    @Test
    public void testCentralDirectoryInfosWithoutUseZip64() throws IOException {
        EndCentralDirectoryInfo endCentralDirInfo = new EndCentralDirectoryInfo(sLargeFile, false);
        try {
            ZipUtil.getZipCentralDirectoryInfos(
                        sLargeFile,
                        endCentralDirInfo,
                        endCentralDirInfo.getCentralDirOffset(),
                        false);
            fail("Should have thrown an exception.");
        } catch (IOException expected) {
            Assert.assertEquals(
                "Invalid central directory info for zip file is found.", expected.getMessage());
        }
    }

    /** Helper to create a large zip file with 700 entries, each entry is with 7MB size. */
    private static void createLargeZipFile() throws IOException {
        FileOutputStream fos = new FileOutputStream(sLargeFile);
        ZipOutputStream zos = new ZipOutputStream(fos);
        // This sets the compression level to STORED, ie, uncompressed
        zos.setLevel(ZipOutputStream.STORED);

        long largeSize = 7000000;
        for (int i = 1 ; i <= SENTRIES ; i++) {
            File srcDir = FileUtil.createTempDir("src", sTempDir);
            String testLargeFile = String.format("testLargeFile_%s", i);
            File localLargeFile = new File(srcDir, testLargeFile);
            localLargeFile.createNewFile();
            RandomAccessFile raf = new RandomAccessFile(localLargeFile, "rw");
            raf.setLength(largeSize);
            raf.close();
            addToZipFile(localLargeFile.getAbsolutePath(), zos);
        }
        zos.close();
        fos.close();
    }

    /** Helper to create a file. */
    private static void addToZipFile(String fileName, ZipOutputStream zos) throws IOException {
        File file = new File(fileName);
        FileInputStream fis = new FileInputStream(file);
        ZipEntry zipEntry = new ZipEntry(fileName);
        zos.putNextEntry(zipEntry);
        byte[] bytes = new byte[1024];
        int length;
        while ((length = fis.read(bytes)) >= 0) {
            zos.write(bytes, 0, length);
        }
        zos.closeEntry();
        fis.close();
    }

    /** Helper to validate the {@link CentralDirectoryInfo} that the data is overflow or not. */
    private boolean validateCentralDirectoryInfo(CentralDirectoryInfo entry) {
        return (Long.toHexString(entry.getCompressedSize()).equals(OVERFLOW) ||
                Long.toHexString(entry.getUncompressedSize()).equals(OVERFLOW) ||
                Long.toHexString(entry.getLocalHeaderOffset()).equals(OVERFLOW));
    }

    /** Helper to validate the {@link CentralDirectoryInfo} objects are equal. */
    private void validateCentralDirectoryInfos(
            ZipFile zipFile, List<CentralDirectoryInfo> zipEntries) {
        for (CentralDirectoryInfo entry : zipEntries) {
            boolean found = false;
            Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry zipEntry = entries.nextElement();
                if (zipEntry.getName().equals(entry.getFileName())) {
                    found = true;
                    break;
                }
            }
            Assert.assertTrue(found);
        }
    }
}
