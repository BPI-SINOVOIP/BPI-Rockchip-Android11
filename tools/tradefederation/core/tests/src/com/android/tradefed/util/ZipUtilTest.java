/*
 * Copyright (C) 2013 The Android Open Source Project
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
import com.android.tradefed.util.zip.LocalFileHeader;

import junit.framework.TestCase;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.attribute.PosixFilePermission;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipFile;

/**
 * Unit tests for {@link ZipUtil}
 */
public class ZipUtilTest extends TestCase {
    private Set<File> mTempFiles = new HashSet<File>();

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        for (File file : mTempFiles) {
            if (file != null && file.exists()) {
                if (file.isDirectory()) {
                    FileUtil.recursiveDelete(file);
                } else {
                    file.delete();
                }
            }
        }
    }

    private File getTestDataFile(String name) throws IOException {
        final InputStream inputStream =
                getClass().getResourceAsStream(String.format("/util/%s.zip", name));
        final File zipFile = createTempFile(name, ".zip");
        FileUtil.writeToFile(inputStream, zipFile);
        return zipFile;
    }

    /**
     * Test that our _simple_ corrupt zip detection heuristics work properly.  It is expected
     * that this check will _fail_ to detect a corrupt but well-formed Zip archive.
     */
    public void testSimpleCorruptZipCheck() throws Exception {
        assertTrue("Falsely detected 'normal.zip' test file as corrupt!",
                ZipUtil.isZipFileValid(getTestDataFile("normal"), false));
        assertFalse("Failed to detect 'truncated.zip' test file as corrupt!",
                ZipUtil.isZipFileValid(getTestDataFile("truncated"), false));
        assertTrue("Unexpectedly detected 'normal.zip' test file as corrupt!  Hope?",
                ZipUtil.isZipFileValid(getTestDataFile("corrupt"), false));
    }

    /**
     * Test that our _thorough_ corrupt zip detection heuristics work properly.
     */
    public void testThoroughCorruptZipCheck() throws Exception {
        assertTrue("Falsely detected 'normal.zip' test file as corrupt with thorough check!",
                ZipUtil.isZipFileValid(getTestDataFile("normal"), true));
        assertFalse("Failed to detect 'truncated.zip' test file as corrupt with thorough check!",
                ZipUtil.isZipFileValid(getTestDataFile("truncated"), true));
        assertFalse("Failed to detect 'normal.zip' test file as corrupt with thorough check!",
                ZipUtil.isZipFileValid(getTestDataFile("corrupt"), true));
    }

    /**
     * Test creating then extracting a zip file from a directory
     *
     * @throws IOException
     */
    public void testCreateAndExtractZip() throws IOException {
        File tmpParentDir = createTempDir("foo");
        File zipFile = null;
        File extractedDir = createTempDir("extract-foo");
        try {
            File childDir = new File(tmpParentDir, "foochild");
            assertTrue(childDir.mkdir());
            File subFile = new File(childDir, "foo.txt");
            FileUtil.writeToFile("contents", subFile);
            zipFile = ZipUtil.createZip(tmpParentDir);
            ZipUtil.extractZip(new ZipFile(zipFile), extractedDir);

            // assert all contents of original zipped dir are extracted
            File extractedParentDir = new File(extractedDir, tmpParentDir.getName());
            File extractedChildDir = new File(extractedParentDir, childDir.getName());
            File extractedSubFile = new File(extractedChildDir, subFile.getName());
            assertTrue(extractedParentDir.exists());
            assertTrue(extractedChildDir.exists());
            assertTrue(extractedSubFile.exists());
            assertTrue(FileUtil.compareFileContents(subFile, extractedSubFile));
        } finally {
            FileUtil.deleteFile(zipFile);
        }
    }

    /**
     * Test creating then extracting a zip file from a list of files
     *
     * @throws IOException
     */
    public void testCreateAndExtractZip_fromFiles() throws IOException {
        File tmpParentDir = createTempDir("foo");
        File zipFile = null;
        File extractedDir = createTempDir("extract-foo");
        try {
            File file1 = new File(tmpParentDir, "foo.txt");
            File file2 = new File(tmpParentDir, "bar.txt");
            FileUtil.writeToFile("contents1", file1);
            FileUtil.writeToFile("contents2", file2);
            zipFile = ZipUtil.createZip(Arrays.asList(file1, file2));
            ZipUtil.extractZip(new ZipFile(zipFile), extractedDir);

            // assert all contents of original zipped dir are extracted
            File extractedFile1 = new File(extractedDir, file1.getName());
            File extractedFile2 = new File(extractedDir, file2.getName());
            assertTrue(extractedFile1.exists());
            assertTrue(extractedFile2.exists());
            assertTrue(FileUtil.compareFileContents(file1, extractedFile1));
            assertTrue(FileUtil.compareFileContents(file2, extractedFile2));
        } finally {
            FileUtil.deleteFile(zipFile);
        }
    }


    /**
     * Test that isZipFileValid returns false if calling with a file that does not exist.
     *
     * @throws IOException
     */
    public void testZipFileDoesNotExist() throws IOException {
        File file = new File("/tmp/this-file-does-not-exist.zip");
        assertFalse(ZipUtil.isZipFileValid(file, true));
        assertFalse(ZipUtil.isZipFileValid(file, false));
    }

    /**
     * Test creating then extracting a a single file from zip file
     *
     * @throws IOException
     */
    public void testCreateAndExtractFileFromZip() throws IOException {
        File tmpParentDir = createTempDir("foo");
        File zipFile = null;
        File extractedSubFile = null;
        try {
            File childDir = new File(tmpParentDir, "foochild");
            assertTrue(childDir.mkdir());
            File subFile = new File(childDir, "foo.txt");
            FileUtil.writeToFile("contents", subFile);
            zipFile = ZipUtil.createZip(tmpParentDir);

            extractedSubFile = ZipUtil.extractFileFromZip(new ZipFile(zipFile),
                    tmpParentDir.getName() + "/foochild/foo.txt");
            assertNotNull(extractedSubFile);
            assertTrue(FileUtil.compareFileContents(subFile, extractedSubFile));
        } finally {
            FileUtil.deleteFile(zipFile);
            FileUtil.deleteFile(extractedSubFile);
        }
    }

    /**
     * Test that {@link ZipUtil#extractZipToTemp(File, String)} properly throws when an incorrect
     * zip is presented.
     */
    public void testExtractZipToTemp() throws Exception {
        File tmpFile = FileUtil.createTempFile("ziputiltest", ".zip");
        try {
            ZipUtil.extractZipToTemp(tmpFile, "testExtractZipToTemp");
            fail("Should have thrown an exception");
        } catch (IOException expected) {
            // expected
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    public void testPartipUnzip() throws Exception {
        File partialZipFile = null;
        File tmpDir = null;
        Set<PosixFilePermission> permissions;
        try {
            // The zip file is small, read the whole file and assume it's partial.
            // This does not affect testing the behavior of partial unzipping.
            partialZipFile = getTestDataFile("partial_zip");
            EndCentralDirectoryInfo endCentralDirInfo = new EndCentralDirectoryInfo(partialZipFile);
            List<CentralDirectoryInfo> zipEntries =
                    ZipUtil.getZipCentralDirectoryInfos(
                            partialZipFile,
                            endCentralDirInfo,
                            endCentralDirInfo.getCentralDirOffset());
            // The zip file has 3 folders, 4 files.
            assertEquals(7, zipEntries.size());

            CentralDirectoryInfo zipEntry;
            LocalFileHeader localFileHeader;
            File targetFile;
            tmpDir = FileUtil.createTempDir("partial_unzip");

            // Unzip empty file
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("empty_file"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            // Verify file permissions - readonly - 644 rw-r--r--
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rw-r--r--"), permissions);

            // Unzip text file
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("large_text/file.txt"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            // Verify CRC
            long crc = FileUtil.calculateCrc32(targetFile);
            assertEquals(4146093769L, crc);
            try (BufferedReader br = new BufferedReader(new FileReader(targetFile))) {
                String line = br.readLine();
                assertTrue(line.endsWith("this is a text file."));
            } catch (IOException e) {
                // fail if the file is corrupt in any way
                throw new RuntimeException(
                        String.format("failed reading text file, msg: %s", e.getMessage()));
            }
            // Verify file permissions - read/write - 666 rw-rw-rw-
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rw-rw-rw-"), permissions);

            // Verify file permissions - executable - 755 rwxr-xr-x
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("executable/executable_file"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rwxr-xr-x"), permissions);

            // Verify file permissions - readonly - 444 r--r--r--
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("read_only/readonly_file"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("r--r--r--"), permissions);

            // Verify folder permissions - readonly - 744 rwxr--r--
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("read_only/"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rwxr--r--"), permissions);

            // Verify folder permissions - read/write - 755 rwxr-xr-x
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("large_text/"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rwxr-xr-x"), permissions);
        } finally {
            FileUtil.deleteFile(partialZipFile);
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test partial unzip a small zip file successfully with use-zip64-in-partial-download is set.
     *
     * @throws IOException
     */
    public void testPartialUnzipWithUseZip64() throws Exception {
        File partialZipFile = null;
        File tmpDir = null;
        Set<PosixFilePermission> permissions;
        try {
            // The zip file is small, read the whole file and assume it's partial.
            // This does not affect testing the behavior of partial unzipping.
            partialZipFile = getTestDataFile("partial_zip");
            EndCentralDirectoryInfo endCentralDirInfo =
                    new EndCentralDirectoryInfo(partialZipFile, true);
            List<CentralDirectoryInfo> zipEntries =
                    ZipUtil.getZipCentralDirectoryInfos(
                            partialZipFile,
                            endCentralDirInfo,
                            endCentralDirInfo.getCentralDirOffset(),
                            true);
            // The zip file has 3 folders, 4 files.
            assertEquals(7, zipEntries.size());

            CentralDirectoryInfo zipEntry;
            LocalFileHeader localFileHeader;
            File targetFile;
            tmpDir = FileUtil.createTempDir("partial_unzip");

            // Unzip empty file
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("empty_file"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            // Verify file permissions - readonly - 644 rw-r--r--
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rw-r--r--"), permissions);

            // Unzip text file
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("large_text/file.txt"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            // Verify CRC
            long crc = FileUtil.calculateCrc32(targetFile);
            assertEquals(4146093769L, crc);
            try (BufferedReader br = new BufferedReader(new FileReader(targetFile))) {
                String line = br.readLine();
                assertTrue(line.endsWith("this is a text file."));
            } catch (IOException e) {
                // fail if the file is corrupt in any way
                throw new RuntimeException(
                        String.format("failed reading text file, msg: %s", e.getMessage()));
            }
            // Verify file permissions - read/write - 666 rw-rw-rw-
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rw-rw-rw-"), permissions);

            // Verify file permissions - executable - 755 rwxr-xr-x
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("executable/executable_file"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rwxr-xr-x"), permissions);

            // Verify file permissions - readonly - 444 r--r--r--
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("read_only/readonly_file"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("r--r--r--"), permissions);

            // Verify folder permissions - readonly - 744 rwxr--r--
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("read_only/"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rwxr--r--"), permissions);

            // Verify folder permissions - read/write - 755 rwxr-xr-x
            zipEntry =
                    zipEntries
                            .stream()
                            .filter(e -> e.getFileName().equals("large_text/"))
                            .findFirst()
                            .get();
            targetFile = new File(Paths.get(tmpDir.toString(), zipEntry.getFileName()).toString());
            localFileHeader =
                    new LocalFileHeader(partialZipFile, (int) zipEntry.getLocalHeaderOffset());
            ZipUtil.unzipPartialZipFile(
                    partialZipFile,
                    targetFile,
                    zipEntry,
                    localFileHeader,
                    zipEntry.getLocalHeaderOffset());
            permissions = Files.getPosixFilePermissions(targetFile.toPath());
            assertEquals(PosixFilePermissions.fromString("rwxr-xr-x"), permissions);
        } finally {
            FileUtil.deleteFile(partialZipFile);
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    // Helpers
    private File createTempDir(String prefix) throws IOException {
        return createTempDir(prefix, null);
    }

    private File createTempDir(String prefix, File parentDir) throws IOException {
        File tempDir = FileUtil.createTempDir(prefix, parentDir);
        mTempFiles.add(tempDir);
        return tempDir;
    }

    private File createTempFile(String prefix, String suffix) throws IOException {
        File tempFile = FileUtil.createTempFile(prefix, suffix);
        mTempFiles.add(tempFile);
        return tempFile;
    }
}
