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

package android.scopedstorage.cts.legacy;

import static android.scopedstorage.cts.lib.TestUtils.BYTES_DATA1;
import static android.scopedstorage.cts.lib.TestUtils.BYTES_DATA2;
import static android.scopedstorage.cts.lib.TestUtils.STR_DATA1;
import static android.scopedstorage.cts.lib.TestUtils.STR_DATA2;
import static android.scopedstorage.cts.lib.TestUtils.assertCanRenameDirectory;
import static android.scopedstorage.cts.lib.TestUtils.assertCanRenameFile;
import static android.scopedstorage.cts.lib.TestUtils.assertCantRenameFile;
import static android.scopedstorage.cts.lib.TestUtils.assertDirectoryContains;
import static android.scopedstorage.cts.lib.TestUtils.assertFileContent;
import static android.scopedstorage.cts.lib.TestUtils.createFileAs;
import static android.scopedstorage.cts.lib.TestUtils.createImageEntryAs;
import static android.scopedstorage.cts.lib.TestUtils.deleteFileAsNoThrow;
import static android.scopedstorage.cts.lib.TestUtils.deleteWithMediaProviderNoThrow;
import static android.scopedstorage.cts.lib.TestUtils.executeShellCommand;
import static android.scopedstorage.cts.lib.TestUtils.getContentResolver;
import static android.scopedstorage.cts.lib.TestUtils.getFileOwnerPackageFromDatabase;
import static android.scopedstorage.cts.lib.TestUtils.getFileRowIdFromDatabase;
import static android.scopedstorage.cts.lib.TestUtils.getImageContentUri;
import static android.scopedstorage.cts.lib.TestUtils.installApp;
import static android.scopedstorage.cts.lib.TestUtils.listAs;
import static android.scopedstorage.cts.lib.TestUtils.openFileAs;
import static android.scopedstorage.cts.lib.TestUtils.openWithMediaProvider;
import static android.scopedstorage.cts.lib.TestUtils.pollForExternalStorageState;
import static android.scopedstorage.cts.lib.TestUtils.pollForPermission;
import static android.scopedstorage.cts.lib.TestUtils.setupDefaultDirectories;
import static android.scopedstorage.cts.lib.TestUtils.uninstallApp;
import static android.scopedstorage.cts.lib.TestUtils.uninstallAppNoThrow;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.Manifest;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.scopedstorage.cts.lib.TestUtils;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.text.TextUtils;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.install.lib.TestApp;

import com.google.common.io.Files;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Test app targeting Q and requesting legacy storage - tests legacy file path access.
 * Designed to be run by LegacyAccessHostTest.
 *
 * <p> Test cases that assume we have WRITE_EXTERNAL_STORAGE only are appended with hasW,
 * those that assume we have READ_EXTERNAL_STORAGE only are appended with hasR, those who assume we
 * have both are appended with hasRW.
 *
 * <p> The tests here are also run by {@link PublicVolumeLegacyTest} on a public volume.
 */
@RunWith(AndroidJUnit4.class)
public class LegacyStorageTest {
    private static final String TAG = "LegacyFileAccessTest";
    static final String THIS_PACKAGE_NAME = InstrumentationRegistry.getContext().getPackageName();

    /**
     * To help avoid flaky tests, give ourselves a unique nonce to be used for
     * all filesystem paths, so that we don't risk conflicting with previous
     * test runs.
     */
    static final String NONCE = String.valueOf(System.nanoTime());

    static final String IMAGE_FILE_NAME = "LegacyStorageTest_file_" + NONCE + ".jpg";
    static final String VIDEO_FILE_NAME = "LegacyStorageTest_file_" + NONCE + ".mp4";
    static final String NONMEDIA_FILE_NAME = "LegacyStorageTest_file_" + NONCE + ".pdf";

    private static final TestApp TEST_APP_A = new TestApp("TestAppA",
            "android.scopedstorage.cts.testapp.A", 1, false, "CtsScopedStorageTestAppA.apk");

    /**
     * This method needs to be called once before running the whole test.
     */
    @Test
    public void setupExternalStorage() {
        setupDefaultDirectories();
    }

    @Before
    public void setup() throws Exception {
        pollForExternalStorageState();
    }

    @After
    public void teardown() throws Exception {
        executeShellCommand("rm " + getShellFile());
        MediaStore.scanFile(getContentResolver(), getShellFile());
    }

    /**
     * Tests that legacy apps bypass the type-path conformity restrictions imposed by
     * MediaProvider. <p> Assumes we have WRITE_EXTERNAL_STORAGE.
     */
    @Test
    public void testCreateFilesInRandomPlaces_hasW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ false);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);
        // Can create file under root dir
        assertCanCreateFile(new File(TestUtils.getExternalStorageDir(), "LegacyFileAccessTest.txt"));

        // Can create music file under DCIM
        assertCanCreateFile(new File(TestUtils.getDcimDir(), "LegacyFileAccessTest.mp3"));

        // Can create random file under external files dir
        assertCanCreateFile(new File(TestUtils.getExternalFilesDir(),
                "LegacyFileAccessTest"));

        // However, even legacy apps can't create files under other app's directories
        final File otherAppDir = new File(TestUtils.getAndroidDataDir(), "com.android.shell");
        final File file = new File(otherAppDir, "LegacyFileAccessTest.txt");

        // otherAppDir was already created by the host test
        try {
            file.createNewFile();
            fail("File creation expected to fail: " + file);
        } catch (IOException expected) {
        }
    }

    /**
     * Tests that legacy apps bypass dir creation/deletion restrictions imposed by MediaProvider.
     * <p> Assumes we have WRITE_EXTERNAL_STORAGE.
     */
    @Test
    public void testMkdirInRandomPlaces_hasW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ false);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);
        // Can create a top-level direcotry
        final File topLevelDir = new File(TestUtils.getExternalStorageDir(), "LegacyFileAccessTest");
        assertCanCreateDir(topLevelDir);

        final File otherAppDir = new File(TestUtils.getAndroidDataDir(), "com.android.shell");

        // However, even legacy apps can't create dirs under other app's directories
        final File subDir = new File(otherAppDir, "LegacyFileAccessTest");
        // otherAppDir was already created by the host test
        assertThat(subDir.mkdir()).isFalse();

        // Try to list a directory and fail because it requires READ permission
        assertThat(TestUtils.getMusicDir().list()).isNull();
    }

    /**
     * Tests that an app can't access external storage without permissions.
     */
    @Test
    public void testCantAccessExternalStorage() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ false);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ false);
        // Can't create file under root dir
        final File newTxtFile = new File(TestUtils.getExternalStorageDir(),
                "LegacyFileAccessTest.txt");
        try {
            newTxtFile.createNewFile();
            fail("File creation expected to fail: " + newTxtFile);
        } catch (IOException expected) {
        }

        // Can't create music file under /MUSIC
        final File newMusicFile = new File(TestUtils.getMusicDir(), "LegacyFileAccessTest.mp3");
        try {
            newMusicFile.createNewFile();
            fail("File creation expected to fail: " + newMusicFile);
        } catch (IOException expected) {
        }

        // Can't create a top-level direcotry
        final File topLevelDir = new File(TestUtils.getExternalStorageDir(), "LegacyFileAccessTest");
        assertThat(topLevelDir.mkdir()).isFalse();

        // Can't read existing file
        final File existingFile = getShellFile();

        try {
            executeShellCommand("touch " + existingFile);
            MediaStore.scanFile(getContentResolver(), existingFile);
            Os.open(existingFile.getPath(), OsConstants.O_RDONLY, /*mode*/ 0);
            fail("Opening file for read expected to fail: " + existingFile);
        } catch (ErrnoException expected) {
        }

        // Can't delete file
        assertThat(existingFile.delete()).isFalse();

        // try to list a directory and fail
        assertThat(TestUtils.getMusicDir().list()).isNull();
        assertThat(TestUtils.getExternalStorageDir().list()).isNull();

        // However, even without permissions, we can access our own external dir
        final File fileInDataDir =
                new File(TestUtils.getExternalFilesDir(),
                        "LegacyFileAccessTest");
        try {
            assertThat(fileInDataDir.createNewFile()).isTrue();
            assertThat(Arrays.asList(fileInDataDir.getParentFile().list()))
                    .contains("LegacyFileAccessTest");
        } finally {
            fileInDataDir.delete();
        }

        // we can access our own external media directory without permissions.
        final File fileInMediaDir =
                new File(TestUtils.getExternalMediaDir(),
                        "LegacyFileAccessTest");
        try {
            assertThat(fileInMediaDir.createNewFile()).isTrue();
            assertThat(Arrays.asList(fileInMediaDir.getParentFile().list()))
                    .contains("LegacyFileAccessTest");
        } finally {
            fileInMediaDir.delete();
        }
    }

    // test read storage permission
    @Test
    public void testReadOnlyExternalStorage_hasR() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ false);
        // can list directory content
        assertThat(TestUtils.getMusicDir().list()).isNotNull();

        // try to write a file and fail
        final File existingFile = getShellFile();

        // can open file for read
        FileDescriptor fd = null;
        try {
            executeShellCommand("touch " + existingFile);
            MediaStore.scanFile(getContentResolver(), existingFile);
            fd = Os.open(existingFile.getPath(), OsConstants.O_RDONLY, /*mode*/ 0);
        } finally {
            if (fd != null) {
                Os.close(fd);
            }
        }

        try {
            fd = Os.open(existingFile.getPath(), OsConstants.O_WRONLY, /*mode*/ 0);
            Os.close(fd);
            fail("Opening file for write expected to fail: " + existingFile);
        } catch (ErrnoException expected) {
        }

        // try to create file and fail, because it requires WRITE
        final File newFile = new File(TestUtils.getMusicDir(), "LegacyFileAccessTest.mp3");
        try {
            newFile.createNewFile();
            fail("Creating file expected to fail: " + newFile);
        } catch (IOException expected) {
        }

        // try to mkdir and fail, because it requires WRITE
        final File newDir = new File(TestUtils.getExternalStorageDir(), "LegacyFileAccessTest");
        try {
            assertThat(newDir.mkdir()).isFalse();
        } finally {
            newDir.delete();
        }
    }

    /**
     * Test that legacy app with storage permission can list all files
     */
    @Test
    public void testListFiles_hasR() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ false);
        final File shellFile = getShellFile();

        executeShellCommand("touch " + getShellFile());
        MediaStore.scanFile(getContentResolver(), getShellFile());
        // can list a non-media file created by other package.
        assertThat(Arrays.asList(shellFile.getParentFile().list()))
                .contains(shellFile.getName());
    }

    /**
     * Test that rename for legacy app with WRITE_EXTERNAL_STORAGE permission bypasses rename
     * restrictions imposed by MediaProvider
     */
    @Test
    public void testCanRename_hasRW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File musicFile1 = new File(TestUtils.getDcimDir(), "LegacyFileAccessTest.mp3");
        final File musicFile2 = new File(TestUtils.getExternalStorageDir(),
                "LegacyFileAccessTest.mp3");
        final File musicFile3 = new File(TestUtils.getMoviesDir(), "LegacyFileAccessTest.mp3");
        final File nonMediaDir1 = new File(TestUtils.getDcimDir(), "LegacyFileAccessTest");
        final File nonMediaDir2 = new File(TestUtils.getExternalStorageDir(),
                "LegacyFileAccessTest");
        final File pdfFile1 = new File(nonMediaDir1, "LegacyFileAccessTest.pdf");
        final File pdfFile2 = new File(nonMediaDir2, "LegacyFileAccessTest.pdf");
        try {
            // can rename a file to root directory.
            assertThat(musicFile1.createNewFile()).isTrue();
            assertCanRenameFile(musicFile1, musicFile2);

            // can rename a music file to Movies directory.
            assertCanRenameFile(musicFile2, musicFile3);

            assertThat(nonMediaDir1.mkdir()).isTrue();
            assertThat(pdfFile1.createNewFile()).isTrue();
            // can rename directory to root directory.
            assertCanRenameDirectory(
                    nonMediaDir1, nonMediaDir2, new File[] {pdfFile1}, new File[] {pdfFile2});
        } finally {
            musicFile1.delete();
            musicFile2.delete();
            musicFile3.delete();

            pdfFile1.delete();
            pdfFile2.delete();
            nonMediaDir1.delete();
            nonMediaDir2.delete();
        }
    }

    /**
     * Test that legacy app with only READ_EXTERNAL_STORAGE can only rename files in app external
     * directories.
     */
    @Test
    public void testCantRename_hasR() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ false);

        final File shellFile1 = getShellFile();
        final File shellFile2 = new File(TestUtils.getDownloadDir(), "LegacyFileAccessTest_shell");
        final File mediaFile1 =
                new File(TestUtils.getExternalMediaDir(),
                        "LegacyFileAccessTest1");
        final File mediaFile2 =
                new File(TestUtils.getExternalMediaDir(),
                        "LegacyFileAccessTest2");
        try {
            executeShellCommand("touch " + shellFile1);
            MediaStore.scanFile(getContentResolver(), shellFile1);
            // app can't rename shell file.
            assertCantRenameFile(shellFile1, shellFile2);
            // app can't move shell file to its media directory.
            assertCantRenameFile(shellFile1, mediaFile1);
            // However, even without permissions, app can rename files in its own external media
            // directory.
            assertThat(mediaFile1.createNewFile()).isTrue();
            assertThat(mediaFile1.renameTo(mediaFile2)).isTrue();
            assertThat(mediaFile2.exists()).isTrue();
        } finally {
            mediaFile1.delete();
            mediaFile2.delete();
        }
    }

    /**
     * Test that legacy app with no storage permission can only rename files in app external
     * directories.
     */
    @Test
    public void testCantRename_noStoragePermission() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ false);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ false);

        final File shellFile1 = getShellFile();
        final File shellFile2 = new File(TestUtils.getDownloadDir(), "LegacyFileAccessTest_shell");
        final File mediaFile1 =
                new File(TestUtils.getExternalMediaDir(),
                        "LegacyFileAccessTest1");
        final File mediaFile2 =
                new File(TestUtils.getExternalMediaDir(),
                        "LegacyFileAccessTest2");
        try {
            executeShellCommand("touch " + shellFile1);
            MediaStore.scanFile(getContentResolver(), shellFile1);
            // app can't rename shell file.
            assertCantRenameFile(shellFile1, shellFile2);
            // app can't move shell file to its media directory.
            assertCantRenameFile(shellFile1, mediaFile1);
            // However, even without permissions, app can rename files in its own external media
            // directory.
            assertThat(mediaFile1.createNewFile()).isTrue();
            assertThat(mediaFile1.renameTo(mediaFile2)).isTrue();
            assertThat(mediaFile2.exists()).isTrue();
        } finally {
            mediaFile1.delete();
            mediaFile2.delete();
        }
    }

    /**
     * b/156046098, Test that MediaProvider doesn't throw UNIQUE constraint error while updating db
     * rows corresponding to renamed directory.
     */
    @Test
    public void testRenameDirectoryAndUpdateDB_hasW() throws Exception {
        final String testDirectoryName = "LegacyFileAccessTestDirectory";
        File directoryOldPath = new File(TestUtils.getDcimDir(), testDirectoryName);
        File directoryNewPath = new File(TestUtils.getMoviesDir(), testDirectoryName);
        try {
            if (directoryOldPath.exists()) {
                executeShellCommand("rm -r " + directoryOldPath.getPath());
            }
            assertThat(directoryOldPath.mkdirs()).isTrue();
            assertCanRenameDirectory(directoryOldPath, directoryNewPath, null, null);

            // Verify that updating directoryOldPath to directoryNewPath doesn't throw
            // UNIQUE constraint error.
            TestUtils.renameWithMediaProvider(directoryOldPath, directoryNewPath);
        } finally {
            directoryOldPath.delete();
            directoryNewPath.delete();
        }
    }

    /**
     * Test that legacy app with WRITE_EXTERNAL_STORAGE can delete all files, and corresponding
     * database entry is deleted on deleting the file.
     */
    @Test
    public void testCanDeleteAllFiles_hasRW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File videoFile = new File(TestUtils.getExternalStorageDir(), VIDEO_FILE_NAME);
        final File otherAppPdfFile = new File(TestUtils.getDownloadDir(), NONMEDIA_FILE_NAME);

        try {
            assertThat(videoFile.createNewFile()).isTrue();
            assertDirectoryContains(videoFile.getParentFile(), videoFile);

            assertThat(getFileRowIdFromDatabase(videoFile)).isNotEqualTo(-1);
            // Legacy app can delete its own file.
            assertThat(videoFile.delete()).isTrue();
            // Deleting the file will remove videoFile entry from database.
            assertThat(getFileRowIdFromDatabase(videoFile)).isEqualTo(-1);

            installApp(TEST_APP_A, false);
            assertThat(createFileAs(TEST_APP_A, otherAppPdfFile.getAbsolutePath())).isTrue();
            assertThat(getFileRowIdFromDatabase(otherAppPdfFile)).isNotEqualTo(-1);
            // Legacy app with write permission can delete the pdfFile owned by TestApp.
            assertThat(otherAppPdfFile.delete()).isTrue();
            // Deleting the pdfFile also removes pdfFile from database.
            //TODO(b/148841336): W_E_S doesn't grant legacy apps write access to other apps' files
            // on a public volume, which is different from the behaviour on a primary external.
//            assertThat(getFileRowIdFromDatabase(otherAppPdfFile)).isEqualTo(-1);
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, otherAppPdfFile.getAbsolutePath());
            uninstallApp(TEST_APP_A);
            videoFile.delete();
        }
    }

    /**
     * Test that file created by legacy app is inserted to MediaProvider database. And,
     * MediaColumns.OWNER_PACKAGE_NAME is updated with calling package's name.
     */
    @Test
    public void testLegacyAppCanOwnAFile_hasW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File videoFile = new File(TestUtils.getExternalStorageDir(), VIDEO_FILE_NAME);
        try {
            assertThat(videoFile.createNewFile()).isTrue();

            installApp(TEST_APP_A, true);
            // videoFile is inserted to database, non-legacy app can see this videoFile on 'ls'.
            assertThat(listAs(TEST_APP_A, TestUtils.getExternalStorageDir().getAbsolutePath()))
                    .contains(VIDEO_FILE_NAME);

            // videoFile is in database, row ID for videoFile can not be -1.
            assertNotEquals(-1, getFileRowIdFromDatabase(videoFile));
            assertEquals(THIS_PACKAGE_NAME, getFileOwnerPackageFromDatabase(videoFile));

            assertTrue(videoFile.delete());
            // videoFile is removed from database on delete, hence row ID is -1.
            assertEquals(-1, getFileRowIdFromDatabase(videoFile));
        } finally {
            videoFile.delete();
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testCreateAndRenameDoesntLeaveStaleDBRow_hasRW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File videoFile = new File(TestUtils.getDcimDir(), VIDEO_FILE_NAME);
        final File renamedVideoFile = new File(TestUtils.getDcimDir(), "Renamed_" + VIDEO_FILE_NAME);
        final ContentResolver cr = getContentResolver();

        try {
            assertThat(videoFile.createNewFile()).isTrue();
            assertThat(videoFile.renameTo(renamedVideoFile)).isTrue();

            // Insert new renamedVideoFile to database
            final Uri uri = TestUtils.insertFileUsingDataColumn(renamedVideoFile);
            assertNotNull(uri);

            // Query for all images/videos in the device.
            // This shouldn't list videoFile which was renamed to renamedVideoFile.
            final ArrayList<String> imageAndVideoFiles = getImageAndVideoFilesFromDatabase();
            assertThat(imageAndVideoFiles).contains(renamedVideoFile.getName());
            assertThat(imageAndVideoFiles).doesNotContain(videoFile.getName());
        } finally {
            videoFile.delete();
            renamedVideoFile.delete();
        }
    }

    /**
     * b/150147690,b/150193381: Test that file rename doesn't delete any existing Uri.
     */
    @Test
    public void testRenameDoesntInvalidateUri_hasRW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File imageFile = new File(TestUtils.getDcimDir(), IMAGE_FILE_NAME);
        final File temporaryImageFile = new File(TestUtils.getDcimDir(), IMAGE_FILE_NAME + "_.tmp");
        final ContentResolver cr = getContentResolver();

        try {
            assertThat(imageFile.createNewFile()).isTrue();
            try (final FileOutputStream fos = new FileOutputStream(imageFile)) {
                fos.write(BYTES_DATA1);
            }
            // Insert this file to database.
            final Uri uri = TestUtils.insertFileUsingDataColumn(imageFile);
            assertNotNull(uri);

            Files.copy(imageFile, temporaryImageFile);
            // Write more bytes to temporaryImageFile
            try (final FileOutputStream fos = new FileOutputStream(temporaryImageFile, true)) {
                fos.write(BYTES_DATA2);
            }
            assertThat(imageFile.delete()).isTrue();
            temporaryImageFile.renameTo(imageFile);

            // Previous uri of imageFile is unaltered after delete & rename.
            final Uri scannedUri = MediaStore.scanFile(cr, imageFile);
            assertThat(scannedUri.getLastPathSegment()).isEqualTo(uri.getLastPathSegment());

            final byte[] expected = (STR_DATA1 + STR_DATA2).getBytes();
            assertFileContent(imageFile, expected);
        } finally {
            imageFile.delete();
            temporaryImageFile.delete();
        }
    }

    /**
     * b/150498564,b/150274099: Test that apps can rename files that are not in database.
     */
    @Test
    public void testCanRenameAFileWithNoDBRow_hasRW() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File directoryNoMedia = new File(TestUtils.getDcimDir(), ".directoryNoMedia");
        final File imageInNoMediaDir = new File(directoryNoMedia, IMAGE_FILE_NAME);
        final File renamedImageInDCIM = new File(TestUtils.getDcimDir(), IMAGE_FILE_NAME);
        final File noMediaFile = new File(directoryNoMedia, ".nomedia");
        final ContentResolver cr = getContentResolver();

        try {
            if (!directoryNoMedia.exists()) {
                assertThat(directoryNoMedia.mkdirs()).isTrue();
            }
            assertThat(noMediaFile.createNewFile()).isTrue();
            assertThat(imageInNoMediaDir.createNewFile()).isTrue();
            // Remove imageInNoMediaDir from database.
            MediaStore.scanFile(cr, directoryNoMedia);

            // Query for all images/videos in the device. This shouldn't list imageInNoMediaDir
            assertThat(getImageAndVideoFilesFromDatabase())
                    .doesNotContain(imageInNoMediaDir.getName());

            // Rename shouldn't throw error even if imageInNoMediaDir is not in database.
            assertThat(imageInNoMediaDir.renameTo(renamedImageInDCIM)).isTrue();
            // We can insert renamedImageInDCIM to database
            ContentValues values = new ContentValues();
            values.put(MediaStore.MediaColumns.DATA, renamedImageInDCIM.getAbsolutePath());
            final Uri uri = TestUtils.insertFileUsingDataColumn(renamedImageInDCIM);
            assertNotNull(uri);
        } finally {
            imageInNoMediaDir.delete();
            renamedImageInDCIM.delete();
            noMediaFile.delete();
            directoryNoMedia.delete();
        }
    }

    /**
     * Test that legacy apps creating files for existing db row doesn't upsert and set IS_PENDING
     */
    @Test
    public void testCreateDoesntUpsert() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File file = new File(TestUtils.getDcimDir(), IMAGE_FILE_NAME);
        Uri uri = null;
        try {
            uri = TestUtils.insertFileUsingDataColumn(file);
            assertNotNull(uri);

            assertTrue(file.createNewFile());

            try (Cursor c = TestUtils.queryFile(file,
                    new String[] {MediaStore.MediaColumns.IS_PENDING})) {
                // This file will not have IS_PENDING=1 because create didn't set IS_PENDING.
                assertTrue(c.moveToFirst());
                assertEquals(c.getInt(0), 0);
            }
        } finally {
            file.delete();
            // If create file fails, we should delete the inserted db row.
            deleteWithMediaProviderNoThrow(uri);
        }
    }

    @Test
    public void testAndroidDataObbCannotBeDeleted() throws Exception {
        File canDeleteDir = new File("/sdcard/canDelete");
        canDeleteDir.mkdirs();

        File dataDir = new File("/sdcard/Android/data");
        File obbDir = new File("/sdcard/Android/obb");
        File androidDir = new File("/sdcard/Android");

        assertThat(dataDir.exists()).isTrue();
        assertThat(obbDir.exists()).isTrue();
        assertThat(androidDir.exists()).isTrue();

        assertThat(dataDir.delete()).isFalse();
        assertThat(obbDir.delete()).isFalse();
        assertThat(androidDir.delete()).isFalse();
        assertThat(canDeleteDir.delete()).isTrue();
    }

    @Test
    public void testLegacyAppUpdatingOwnershipOfExistingEntry() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final File fullPath = new File(TestUtils.getDcimDir(),
                "OwnershipChange_" + IMAGE_FILE_NAME);
        final String relativePath = "DCIM/OwnershipChange_" + IMAGE_FILE_NAME;
        try {
            installApp(TEST_APP_A, false);
            createImageEntryAs(TEST_APP_A, relativePath);
            assertThat(fullPath.createNewFile()).isTrue();

            // We have transferred ownership away from TEST_APP_A so reads / writes
            // should no longer work.
            assertThat(openFileAs(TEST_APP_A, fullPath, false /* for write */)).isFalse();
            assertThat(openFileAs(TEST_APP_A, fullPath, false /* for read */)).isFalse();
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, fullPath.getAbsolutePath());
            uninstallAppNoThrow(TEST_APP_A);
            fullPath.delete();
        }
    }

    /**
     * b/156717256,b/156336269: Test that MediaProvider doesn't throw error on usage of unsupported
     * or empty/null MIME type.
     */
    @Test
    public void testInsertWithUnsupportedMimeType() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);

        final String IMAGE_FILE_DISPLAY_NAME = "LegacyStorageTest_file_" + NONCE;
        final File imageFile = new File(TestUtils.getDcimDir(), IMAGE_FILE_DISPLAY_NAME + ".jpg");

        for (String mimeType : new String[] {
            "image/*", "", null, "foo/bar"
        }) {
            Uri uri = null;
            try {
                ContentValues values = new ContentValues();
                values.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DCIM);
                if (TextUtils.isEmpty(mimeType)) {
                    values.put(MediaStore.MediaColumns.DISPLAY_NAME, imageFile.getName());
                } else {
                    values.put(MediaStore.MediaColumns.DISPLAY_NAME, IMAGE_FILE_DISPLAY_NAME);
                }
                values.put(MediaStore.MediaColumns.MIME_TYPE, mimeType);

                uri = getContentResolver().insert(getImageContentUri(), values, Bundle.EMPTY);
                assertNotNull(uri);

                try (final OutputStream fos = getContentResolver().openOutputStream(uri, "rw")) {
                    fos.write(BYTES_DATA1);
                }

                // Closing the file should trigger a scan, we still scan again to ensure MIME type
                // is extracted from file extension
                assertNotNull(MediaStore.scanFile(getContentResolver(), imageFile));

                final String[] projection = {MediaStore.MediaColumns.DISPLAY_NAME,
                        MediaStore.MediaColumns.MIME_TYPE};
                try (Cursor c = getContentResolver().query(uri, projection, null, null, null)) {
                    assertTrue(c.moveToFirst());
                    assertEquals(c.getCount(), 1);
                    assertEquals(c.getString(0), imageFile.getName());
                    assertTrue("image/jpeg".equalsIgnoreCase(c.getString(1)));
                }
            } finally {
                deleteWithMediaProviderNoThrow(uri);
            }
        }
    }

    private static void assertCanCreateFile(File file) throws IOException {
        if (file.exists()) {
            file.delete();
        }
        try {
            if (!file.createNewFile()) {
                fail("Could not create file: " + file);
            }
        } finally {
            file.delete();
        }
    }

    private static void assertCanCreateDir(File dir) throws IOException {
        if (dir.exists()) {
            if (!dir.delete()) {
                Log.w(TAG,
                        "Can't create dir " + dir + " because it already exists and we can't "
                                + "delete it!");
                return;
            }
        }
        try {
            if (!dir.mkdir()) {
                fail("Could not mkdir: " + dir);
            }
        } finally {
            dir.delete();
        }
    }

    /**
     * Queries {@link ContentResolver} for all image and video files, returns display name of
     * corresponding files.
     */
    private static ArrayList<String> getImageAndVideoFilesFromDatabase() {
        ArrayList<String> mediaFiles = new ArrayList<>();
        final String selection = "is_pending = 0 AND is_trashed = 0 AND "
                + "(media_type = ? OR media_type = ?)";
        final String[] selectionArgs =
                new String[] {String.valueOf(MediaStore.Files.FileColumns.MEDIA_TYPE_IMAGE),
                        String.valueOf(MediaStore.Files.FileColumns.MEDIA_TYPE_VIDEO)};

        try (Cursor c = getContentResolver().query(TestUtils.getTestVolumeFileUri(),
                /* projection */ new String[] {MediaStore.MediaColumns.DISPLAY_NAME},
                selection, selectionArgs, null)) {
            while (c.moveToNext()) {
                mediaFiles.add(c.getString(0));
            }
        }
        return mediaFiles;
    }

    private File getShellFile() throws Exception {
        return new File(TestUtils.getExternalStorageDir(),
                "LegacyAccessHostTest_shell");
    }
}
