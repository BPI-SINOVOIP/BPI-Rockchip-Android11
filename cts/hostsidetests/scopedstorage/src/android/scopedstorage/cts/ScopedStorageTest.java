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

package android.scopedstorage.cts;

import static android.app.AppOpsManager.permissionToOp;
import static android.os.SystemProperties.getBoolean;
import static android.provider.MediaStore.MediaColumns;
import static android.scopedstorage.cts.lib.RedactionTestHelper.assertExifMetadataMatch;
import static android.scopedstorage.cts.lib.RedactionTestHelper.assertExifMetadataMismatch;
import static android.scopedstorage.cts.lib.RedactionTestHelper.getExifMetadata;
import static android.scopedstorage.cts.lib.RedactionTestHelper.getExifMetadataFromRawResource;
import static android.scopedstorage.cts.lib.TestUtils.BYTES_DATA1;
import static android.scopedstorage.cts.lib.TestUtils.BYTES_DATA2;
import static android.scopedstorage.cts.lib.TestUtils.STR_DATA1;
import static android.scopedstorage.cts.lib.TestUtils.STR_DATA2;
import static android.scopedstorage.cts.lib.TestUtils.adoptShellPermissionIdentity;
import static android.scopedstorage.cts.lib.TestUtils.allowAppOpsToUid;
import static android.scopedstorage.cts.lib.TestUtils.assertCanRenameDirectory;
import static android.scopedstorage.cts.lib.TestUtils.assertCanRenameFile;
import static android.scopedstorage.cts.lib.TestUtils.assertCantRenameDirectory;
import static android.scopedstorage.cts.lib.TestUtils.assertCantRenameFile;
import static android.scopedstorage.cts.lib.TestUtils.assertDirectoryContains;
import static android.scopedstorage.cts.lib.TestUtils.assertFileContent;
import static android.scopedstorage.cts.lib.TestUtils.assertThrows;
import static android.scopedstorage.cts.lib.TestUtils.canOpen;
import static android.scopedstorage.cts.lib.TestUtils.canReadAndWriteAs;
import static android.scopedstorage.cts.lib.TestUtils.createFileAs;
import static android.scopedstorage.cts.lib.TestUtils.deleteFileAs;
import static android.scopedstorage.cts.lib.TestUtils.deleteFileAsNoThrow;
import static android.scopedstorage.cts.lib.TestUtils.deleteRecursively;
import static android.scopedstorage.cts.lib.TestUtils.deleteWithMediaProvider;
import static android.scopedstorage.cts.lib.TestUtils.deleteWithMediaProviderNoThrow;
import static android.scopedstorage.cts.lib.TestUtils.denyAppOpsToUid;
import static android.scopedstorage.cts.lib.TestUtils.dropShellPermissionIdentity;
import static android.scopedstorage.cts.lib.TestUtils.executeShellCommand;
import static android.scopedstorage.cts.lib.TestUtils.getAlarmsDir;
import static android.scopedstorage.cts.lib.TestUtils.getAndroidDataDir;
import static android.scopedstorage.cts.lib.TestUtils.getAndroidDir;
import static android.scopedstorage.cts.lib.TestUtils.getAndroidMediaDir;
import static android.scopedstorage.cts.lib.TestUtils.getAudiobooksDir;
import static android.scopedstorage.cts.lib.TestUtils.getContentResolver;
import static android.scopedstorage.cts.lib.TestUtils.getDcimDir;
import static android.scopedstorage.cts.lib.TestUtils.getDefaultTopLevelDirs;
import static android.scopedstorage.cts.lib.TestUtils.getDocumentsDir;
import static android.scopedstorage.cts.lib.TestUtils.getDownloadDir;
import static android.scopedstorage.cts.lib.TestUtils.getExternalFilesDir;
import static android.scopedstorage.cts.lib.TestUtils.getExternalMediaDir;
import static android.scopedstorage.cts.lib.TestUtils.getExternalStorageDir;
import static android.scopedstorage.cts.lib.TestUtils.getFileMimeTypeFromDatabase;
import static android.scopedstorage.cts.lib.TestUtils.getFileOwnerPackageFromDatabase;
import static android.scopedstorage.cts.lib.TestUtils.getFileRowIdFromDatabase;
import static android.scopedstorage.cts.lib.TestUtils.getFileSizeFromDatabase;
import static android.scopedstorage.cts.lib.TestUtils.getFileUri;
import static android.scopedstorage.cts.lib.TestUtils.getMoviesDir;
import static android.scopedstorage.cts.lib.TestUtils.getMusicDir;
import static android.scopedstorage.cts.lib.TestUtils.getNotificationsDir;
import static android.scopedstorage.cts.lib.TestUtils.getPicturesDir;
import static android.scopedstorage.cts.lib.TestUtils.getPodcastsDir;
import static android.scopedstorage.cts.lib.TestUtils.getRingtonesDir;
import static android.scopedstorage.cts.lib.TestUtils.grantPermission;
import static android.scopedstorage.cts.lib.TestUtils.installApp;
import static android.scopedstorage.cts.lib.TestUtils.installAppWithStoragePermissions;
import static android.scopedstorage.cts.lib.TestUtils.listAs;
import static android.scopedstorage.cts.lib.TestUtils.openFileAs;
import static android.scopedstorage.cts.lib.TestUtils.openWithMediaProvider;
import static android.scopedstorage.cts.lib.TestUtils.pollForExternalStorageState;
import static android.scopedstorage.cts.lib.TestUtils.pollForManageExternalStorageAllowed;
import static android.scopedstorage.cts.lib.TestUtils.pollForPermission;
import static android.scopedstorage.cts.lib.TestUtils.queryFile;
import static android.scopedstorage.cts.lib.TestUtils.queryFileExcludingPending;
import static android.scopedstorage.cts.lib.TestUtils.queryImageFile;
import static android.scopedstorage.cts.lib.TestUtils.queryVideoFile;
import static android.scopedstorage.cts.lib.TestUtils.readExifMetadataFromTestApp;
import static android.scopedstorage.cts.lib.TestUtils.revokePermission;
import static android.scopedstorage.cts.lib.TestUtils.setAttrAs;
import static android.scopedstorage.cts.lib.TestUtils.setupDefaultDirectories;
import static android.scopedstorage.cts.lib.TestUtils.uninstallApp;
import static android.scopedstorage.cts.lib.TestUtils.uninstallAppNoThrow;
import static android.scopedstorage.cts.lib.TestUtils.updateDisplayNameWithMediaProvider;
import static android.system.OsConstants.F_OK;
import static android.system.OsConstants.O_APPEND;
import static android.system.OsConstants.O_CREAT;
import static android.system.OsConstants.O_EXCL;
import static android.system.OsConstants.O_RDWR;
import static android.system.OsConstants.O_TRUNC;
import static android.system.OsConstants.R_OK;
import static android.system.OsConstants.S_IRWXU;
import static android.system.OsConstants.W_OK;

import static androidx.test.InstrumentationRegistry.getContext;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.Manifest;
import android.app.AppOpsManager;
import android.app.WallpaperManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.platform.test.annotations.AppModeInstant;
import android.provider.MediaStore;
import android.system.ErrnoException;
import android.system.Os;
import android.system.StructStat;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.install.lib.TestApp;

import com.google.common.io.Files;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/**
 * Runs the scoped storage tests on primary external storage.
 *
 * <p>These tests are also run on a public volume by {@link PublicVolumeTest}.
 */
@RunWith(AndroidJUnit4.class)
public class ScopedStorageTest {
    static final String TAG = "ScopedStorageTest";
    static final String THIS_PACKAGE_NAME = getContext().getPackageName();

    /**
     * To help avoid flaky tests, give ourselves a unique nonce to be used for
     * all filesystem paths, so that we don't risk conflicting with previous
     * test runs.
     */
    static final String NONCE = String.valueOf(System.nanoTime());

    static final String TEST_DIRECTORY_NAME = "ScopedStorageTestDirectory" + NONCE;

    static final String AUDIO_FILE_NAME = "ScopedStorageTest_file_" + NONCE + ".mp3";
    static final String PLAYLIST_FILE_NAME = "ScopedStorageTest_file_" + NONCE + ".m3u";
    static final String SUBTITLE_FILE_NAME = "ScopedStorageTest_file_" + NONCE + ".srt";
    static final String VIDEO_FILE_NAME = "ScopedStorageTest_file_" + NONCE + ".mp4";
    static final String IMAGE_FILE_NAME = "ScopedStorageTest_file_" + NONCE + ".jpg";
    static final String NONMEDIA_FILE_NAME = "ScopedStorageTest_file_" + NONCE + ".pdf";

    static final String FILE_CREATION_ERROR_MESSAGE = "No such file or directory";

    private static final TestApp TEST_APP_A = new TestApp("TestAppA",
            "android.scopedstorage.cts.testapp.A", 1, false, "CtsScopedStorageTestAppA.apk");
    private static final TestApp TEST_APP_B = new TestApp("TestAppB",
            "android.scopedstorage.cts.testapp.B", 1, false, "CtsScopedStorageTestAppB.apk");
    private static final TestApp TEST_APP_C = new TestApp("TestAppC",
            "android.scopedstorage.cts.testapp.C", 1, false, "CtsScopedStorageTestAppC.apk");
    private static final TestApp TEST_APP_C_LEGACY = new TestApp("TestAppCLegacy",
            "android.scopedstorage.cts.testapp.C", 1, false, "CtsScopedStorageTestAppCLegacy.apk");
    private static final String[] SYSTEM_GALERY_APPOPS = {
            AppOpsManager.OPSTR_WRITE_MEDIA_IMAGES, AppOpsManager.OPSTR_WRITE_MEDIA_VIDEO};
    private static final String OPSTR_MANAGE_EXTERNAL_STORAGE =
            permissionToOp(Manifest.permission.MANAGE_EXTERNAL_STORAGE);

    @Before
    public void setup() throws Exception {
        // skips all test cases if FUSE is not active.
        assumeTrue(getBoolean("persist.sys.fuse", false));

        if (!getContext().getPackageManager().isInstantApp()) {
            pollForExternalStorageState();
            getExternalFilesDir().mkdirs();
        }
    }

    /**
     * This method needs to be called once before running the whole test.
     */
    @Test
    public void setupExternalStorage() {
        setupDefaultDirectories();
    }

    /**
     * Test that we enforce certain media types can only be created in certain directories.
     */
    @Test
    public void testTypePathConformity() throws Exception {
        final File dcimDir = getDcimDir();
        final File documentsDir = getDocumentsDir();
        final File downloadDir = getDownloadDir();
        final File moviesDir = getMoviesDir();
        final File musicDir = getMusicDir();
        final File picturesDir = getPicturesDir();
        // Only audio files can be created in Music
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(musicDir, NONMEDIA_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(musicDir, VIDEO_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(musicDir, IMAGE_FILE_NAME).createNewFile(); });
        // Only video files can be created in Movies
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(moviesDir, NONMEDIA_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(moviesDir, AUDIO_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(moviesDir, IMAGE_FILE_NAME).createNewFile(); });
        // Only image and video files can be created in DCIM
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(dcimDir, NONMEDIA_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(dcimDir, AUDIO_FILE_NAME).createNewFile(); });
        // Only image and video files can be created in Pictures
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(picturesDir, NONMEDIA_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(picturesDir, AUDIO_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(picturesDir, PLAYLIST_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(dcimDir, SUBTITLE_FILE_NAME).createNewFile(); });

        assertCanCreateFile(new File(getAlarmsDir(), AUDIO_FILE_NAME));
        assertCanCreateFile(new File(getAudiobooksDir(), AUDIO_FILE_NAME));
        assertCanCreateFile(new File(dcimDir, IMAGE_FILE_NAME));
        assertCanCreateFile(new File(dcimDir, VIDEO_FILE_NAME));
        assertCanCreateFile(new File(documentsDir, AUDIO_FILE_NAME));
        assertCanCreateFile(new File(documentsDir, IMAGE_FILE_NAME));
        assertCanCreateFile(new File(documentsDir, NONMEDIA_FILE_NAME));
        assertCanCreateFile(new File(documentsDir, VIDEO_FILE_NAME));
        assertCanCreateFile(new File(downloadDir, AUDIO_FILE_NAME));
        assertCanCreateFile(new File(downloadDir, IMAGE_FILE_NAME));
        assertCanCreateFile(new File(downloadDir, NONMEDIA_FILE_NAME));
        assertCanCreateFile(new File(downloadDir, VIDEO_FILE_NAME));
        assertCanCreateFile(new File(moviesDir, VIDEO_FILE_NAME));
        assertCanCreateFile(new File(moviesDir, SUBTITLE_FILE_NAME));
        assertCanCreateFile(new File(musicDir, AUDIO_FILE_NAME));
        assertCanCreateFile(new File(musicDir, PLAYLIST_FILE_NAME));
        assertCanCreateFile(new File(getNotificationsDir(), AUDIO_FILE_NAME));
        assertCanCreateFile(new File(picturesDir, IMAGE_FILE_NAME));
        assertCanCreateFile(new File(picturesDir, VIDEO_FILE_NAME));
        assertCanCreateFile(new File(getPodcastsDir(), AUDIO_FILE_NAME));
        assertCanCreateFile(new File(getRingtonesDir(), AUDIO_FILE_NAME));

        // No file whatsoever can be created in the top level directory
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(getExternalStorageDir(), NONMEDIA_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(getExternalStorageDir(), AUDIO_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(getExternalStorageDir(), IMAGE_FILE_NAME).createNewFile(); });
        assertThrows(IOException.class, "Operation not permitted",
                () -> { new File(getExternalStorageDir(), VIDEO_FILE_NAME).createNewFile(); });
    }

    /**
     * Test that we can create a file in app's external files directory,
     * and that we can write and read to/from the file.
     */
    @Test
    public void testCreateFileInAppExternalDir() throws Exception {
        final File file = new File(getExternalFilesDir(), "text.txt");
        try {
            assertThat(file.createNewFile()).isTrue();
            assertThat(file.delete()).isTrue();
            // Ensure the file is properly deleted and can be created again
            assertThat(file.createNewFile()).isTrue();

            // Write to file
            try (final FileOutputStream fos = new FileOutputStream(file)) {
                fos.write(BYTES_DATA1);
            }

            // Read the same data from file
            assertFileContent(file, BYTES_DATA1);
        } finally {
            file.delete();
        }
    }

    /**
     * Test that we can't create a file in another app's external files directory,
     * and that we'll get the same error regardless of whether the app exists or not.
     */
    @Test
    public void testCreateFileInOtherAppExternalDir() throws Exception {
        // Creating a file in a non existent package dir should return ENOENT, as expected
        final File nonexistentPackageFileDir = new File(
                getExternalFilesDir().getPath().replace(THIS_PACKAGE_NAME, "no.such.package"));
        final File file1 = new File(nonexistentPackageFileDir, NONMEDIA_FILE_NAME);
        assertThrows(
                IOException.class, FILE_CREATION_ERROR_MESSAGE, () -> { file1.createNewFile(); });

        // Creating a file in an existent package dir should give the same error string to avoid
        // leaking installed app names, and we know the following directory exists because shell
        // mkdirs it in test setup
        final File shellPackageFileDir = new File(
                getExternalFilesDir().getPath().replace(THIS_PACKAGE_NAME, "com.android.shell"));
        final File file2 = new File(shellPackageFileDir, NONMEDIA_FILE_NAME);
        assertThrows(
                IOException.class, FILE_CREATION_ERROR_MESSAGE, () -> { file1.createNewFile(); });
    }

    /**
     * Test that apps can't read/write files in another app's external files directory,
     * and can do so in their own app's external file directory.
     */
    @Test
    public void testReadWriteFilesInOtherAppExternalDir() throws Exception {
        final File videoFile = new File(getExternalFilesDir(), VIDEO_FILE_NAME);

        try {
            // Create a file in app's external files directory
            if (!videoFile.exists()) {
                assertThat(videoFile.createNewFile()).isTrue();
            }

            // Install TEST_APP_A with READ_EXTERNAL_STORAGE permission.
            installAppWithStoragePermissions(TEST_APP_A);

            // TEST_APP_A should not be able to read/write to other app's external files directory.
            assertThat(openFileAs(TEST_APP_A, videoFile.getPath(), false /* forWrite */)).isFalse();
            assertThat(openFileAs(TEST_APP_A, videoFile.getPath(), true /* forWrite */)).isFalse();
            // TEST_APP_A should not be able to delete files in other app's external files
            // directory.
            assertThat(deleteFileAs(TEST_APP_A, videoFile.getPath())).isFalse();

            // Apps should have read/write access in their own app's external files directory.
            assertThat(canOpen(videoFile, false /* forWrite */)).isTrue();
            assertThat(canOpen(videoFile, true /* forWrite */)).isTrue();
            // Apps should be able to delete files in their own app's external files directory.
            assertThat(videoFile.delete()).isTrue();
        } finally {
            videoFile.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that we can contribute media without any permissions.
     */
    @Test
    public void testContributeMediaFile() throws Exception {
        final File imageFile = new File(getDcimDir(), IMAGE_FILE_NAME);

        try {
            assertThat(imageFile.createNewFile()).isTrue();

            // Ensure that the file was successfully added to the MediaProvider database
            assertThat(getFileOwnerPackageFromDatabase(imageFile)).isEqualTo(THIS_PACKAGE_NAME);

            // Try to write random data to the file
            try (final FileOutputStream fos = new FileOutputStream(imageFile)) {
                fos.write(BYTES_DATA1);
                fos.write(BYTES_DATA2);
            }

            final byte[] expected = (STR_DATA1 + STR_DATA2).getBytes();
            assertFileContent(imageFile, expected);

            // Closing the file after writing will not trigger a MediaScan. Call scanFile to update
            // file's entry in MediaProvider's database.
            assertThat(MediaStore.scanFile(getContentResolver(), imageFile)).isNotNull();

            // Ensure that the scan was completed and the file's size was updated.
            assertThat(getFileSizeFromDatabase(imageFile)).isEqualTo(
                    BYTES_DATA1.length + BYTES_DATA2.length);
        } finally {
            imageFile.delete();
        }
        // Ensure that delete makes a call to MediaProvider to remove the file from its database.
        assertThat(getFileRowIdFromDatabase(imageFile)).isEqualTo(-1);
    }

    @Test
    public void testCreateAndDeleteEmptyDir() throws Exception {
        final File externalFilesDir = getExternalFilesDir();
        // Remove directory in order to create it again
        externalFilesDir.delete();

        // Can create own external files dir
        assertThat(externalFilesDir.mkdir()).isTrue();

        final File dir1 = new File(externalFilesDir, "random_dir");
        // Can create dirs inside it
        assertThat(dir1.mkdir()).isTrue();

        final File dir2 = new File(dir1, "random_dir_inside_random_dir");
        // And create a dir inside the new dir
        assertThat(dir2.mkdir()).isTrue();

        // And can delete them all
        assertThat(dir2.delete()).isTrue();
        assertThat(dir1.delete()).isTrue();
        assertThat(externalFilesDir.delete()).isTrue();

        // Can't create external dir for other apps
        final File nonexistentPackageFileDir = new File(
                externalFilesDir.getPath().replace(THIS_PACKAGE_NAME, "no.such.package"));
        final File shellPackageFileDir = new File(
                externalFilesDir.getPath().replace(THIS_PACKAGE_NAME, "com.android.shell"));

        assertThat(nonexistentPackageFileDir.mkdir()).isFalse();
        assertThat(shellPackageFileDir.mkdir()).isFalse();
    }

    @Test
    public void testCantAccessOtherAppsContents() throws Exception {
        final File mediaFile = new File(getPicturesDir(), IMAGE_FILE_NAME);
        final File nonMediaFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        try {
            installApp(TEST_APP_A);

            assertThat(createFileAs(TEST_APP_A, mediaFile.getPath())).isTrue();
            assertThat(createFileAs(TEST_APP_A, nonMediaFile.getPath())).isTrue();

            // We can still see that the files exist
            assertThat(mediaFile.exists()).isTrue();
            assertThat(nonMediaFile.exists()).isTrue();

            // But we can't access their content
            assertThat(canOpen(mediaFile, /* forWrite */ false)).isFalse();
            assertThat(canOpen(nonMediaFile, /* forWrite */ true)).isFalse();
            assertThat(canOpen(mediaFile, /* forWrite */ false)).isFalse();
            assertThat(canOpen(nonMediaFile, /* forWrite */ true)).isFalse();
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, nonMediaFile.getPath());
            deleteFileAsNoThrow(TEST_APP_A, mediaFile.getPath());
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testCantDeleteOtherAppsContents() throws Exception {
        final File dirInDownload = new File(getDownloadDir(), TEST_DIRECTORY_NAME);
        final File mediaFile = new File(dirInDownload, IMAGE_FILE_NAME);
        final File nonMediaFile = new File(dirInDownload, NONMEDIA_FILE_NAME);
        try {
            installApp(TEST_APP_A);
            assertThat(dirInDownload.mkdir()).isTrue();
            // Have another app create a media file in the directory
            assertThat(createFileAs(TEST_APP_A, mediaFile.getPath())).isTrue();

            // Can't delete the directory since it contains another app's content
            assertThat(dirInDownload.delete()).isFalse();
            // Can't delete another app's content
            assertThat(deleteRecursively(dirInDownload)).isFalse();

            // Have another app create a non-media file in the directory
            assertThat(createFileAs(TEST_APP_A, nonMediaFile.getPath())).isTrue();

            // Can't delete the directory since it contains another app's content
            assertThat(dirInDownload.delete()).isFalse();
            // Can't delete another app's content
            assertThat(deleteRecursively(dirInDownload)).isFalse();

            // Delete only the media file and keep the non-media file
            assertThat(deleteFileAs(TEST_APP_A, mediaFile.getPath())).isTrue();
            // Directory now has only the non-media file contributed by another app, so we still
            // can't delete it nor its content
            assertThat(dirInDownload.delete()).isFalse();
            assertThat(deleteRecursively(dirInDownload)).isFalse();

            // Delete the last file belonging to another app
            assertThat(deleteFileAs(TEST_APP_A, nonMediaFile.getPath())).isTrue();
            // Create our own file
            assertThat(nonMediaFile.createNewFile()).isTrue();

            // Now that the directory only has content that was contributed by us, we can delete it
            assertThat(deleteRecursively(dirInDownload)).isTrue();
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, nonMediaFile.getPath());
            deleteFileAsNoThrow(TEST_APP_A, mediaFile.getPath());
            // At this point, we're not sure who created this file, so we'll have both apps
            // deleting it
            mediaFile.delete();
            uninstallAppNoThrow(TEST_APP_A);
            dirInDownload.delete();
        }
    }

    /**
     * Test that deleting uri corresponding to a file which was already deleted via filePath
     * doesn't result in a security exception.
     */
    @Test
    public void testDeleteAlreadyUnlinkedFile() throws Exception {
        final File nonMediaFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        try {
            assertTrue(nonMediaFile.createNewFile());
            final Uri uri = MediaStore.scanFile(getContentResolver(), nonMediaFile);
            assertNotNull(uri);

            // Delete the file via filePath
            assertTrue(nonMediaFile.delete());

            // If we delete nonMediaFile with ContentResolver#delete, it shouldn't result in a
            // security exception.
            assertThat(getContentResolver().delete(uri, Bundle.EMPTY)).isEqualTo(0);
        } finally {
            nonMediaFile.delete();
        }
    }

    /**
     * This test relies on the fact that {@link File#list} uses opendir internally, and that it
     * returns {@code null} if opendir fails.
     */
    @Test
    public void testOpendirRestrictions() throws Exception {
        // Opening a non existent package directory should fail, as expected
        final File nonexistentPackageFileDir = new File(
                getExternalFilesDir().getPath().replace(THIS_PACKAGE_NAME, "no.such.package"));
        assertThat(nonexistentPackageFileDir.list()).isNull();

        // Opening another package's external directory should fail as well, even if it exists
        final File shellPackageFileDir = new File(
                getExternalFilesDir().getPath().replace(THIS_PACKAGE_NAME, "com.android.shell"));
        assertThat(shellPackageFileDir.list()).isNull();

        // We can open our own external files directory
        final String[] filesList = getExternalFilesDir().list();
        assertThat(filesList).isNotNull();

        // We can open any public directory in external storage
        assertThat(getDcimDir().list()).isNotNull();
        assertThat(getDownloadDir().list()).isNotNull();
        assertThat(getMoviesDir().list()).isNotNull();
        assertThat(getMusicDir().list()).isNotNull();

        // We can open the root directory of external storage
        final String[] topLevelDirs = getExternalStorageDir().list();
        assertThat(topLevelDirs).isNotNull();
        // TODO(b/145287327): This check fails on a device with no visible files.
        // This can be fixed if we display default directories.
        // assertThat(topLevelDirs).isNotEmpty();
    }

    @Test
    public void testLowLevelFileIO() throws Exception {
        String filePath = new File(getDownloadDir(), NONMEDIA_FILE_NAME).toString();
        try {
            int createFlags = O_CREAT | O_RDWR;
            int createExclFlags = createFlags | O_EXCL;

            FileDescriptor fd = Os.open(filePath, createExclFlags, S_IRWXU);
            Os.close(fd);
            assertThrows(
                    ErrnoException.class, () -> { Os.open(filePath, createExclFlags, S_IRWXU); });

            fd = Os.open(filePath, createFlags, S_IRWXU);
            try {
                assertThat(Os.write(fd, ByteBuffer.wrap(BYTES_DATA1))).isEqualTo(BYTES_DATA1.length);
                assertFileContent(fd, BYTES_DATA1);
            } finally {
                Os.close(fd);
            }
            // should just append the data
            fd = Os.open(filePath, createFlags | O_APPEND, S_IRWXU);
            try {
                assertThat(Os.write(fd, ByteBuffer.wrap(BYTES_DATA2))).isEqualTo(BYTES_DATA2.length);
                final byte[] expected = (STR_DATA1 + STR_DATA2).getBytes();
                assertFileContent(fd, expected);
            } finally {
                Os.close(fd);
            }
            // should overwrite everything
            fd = Os.open(filePath, createFlags | O_TRUNC, S_IRWXU);
            try {
                final byte[] otherData = "this is different data".getBytes();
                assertThat(Os.write(fd, ByteBuffer.wrap(otherData))).isEqualTo(otherData.length);
                assertFileContent(fd, otherData);
            } finally {
                Os.close(fd);
            }
        } finally {
            new File(filePath).delete();
        }
    }

    /**
     * Test that media files from other packages are only visible to apps with storage permission.
     */
    @Test
    public void testListDirectoriesWithMediaFiles() throws Exception {
        final File dcimDir = getDcimDir();
        final File dir = new File(dcimDir, TEST_DIRECTORY_NAME);
        final File videoFile = new File(dir, VIDEO_FILE_NAME);
        final String videoFileName = videoFile.getName();
        try {
            if (!dir.exists()) {
                assertThat(dir.mkdir()).isTrue();
            }

            // Install TEST_APP_A and create media file in the new directory.
            installApp(TEST_APP_A);
            assertThat(createFileAs(TEST_APP_A, videoFile.getPath())).isTrue();
            // TEST_APP_A should see TEST_DIRECTORY in DCIM and new file in TEST_DIRECTORY.
            assertThat(listAs(TEST_APP_A, dcimDir.getPath())).contains(TEST_DIRECTORY_NAME);
            assertThat(listAs(TEST_APP_A, dir.getPath())).containsExactly(videoFileName);

            // Install TEST_APP_B with storage permission.
            installAppWithStoragePermissions(TEST_APP_B);
            // TEST_APP_B with storage permission should see TEST_DIRECTORY in DCIM and new file
            // in TEST_DIRECTORY.
            assertThat(listAs(TEST_APP_B, dcimDir.getPath())).contains(TEST_DIRECTORY_NAME);
            assertThat(listAs(TEST_APP_B, dir.getPath())).containsExactly(videoFileName);

            // Revoke storage permission for TEST_APP_B
            revokePermission(
                    TEST_APP_B.getPackageName(), Manifest.permission.READ_EXTERNAL_STORAGE);
            // TEST_APP_B without storage permission should see TEST_DIRECTORY in DCIM and should
            // not see new file in new TEST_DIRECTORY.
            assertThat(listAs(TEST_APP_B, dcimDir.getPath())).contains(TEST_DIRECTORY_NAME);
            assertThat(listAs(TEST_APP_B, dir.getPath())).doesNotContain(videoFileName);
        } finally {
            uninstallAppNoThrow(TEST_APP_B);
            deleteFileAsNoThrow(TEST_APP_A, videoFile.getPath());
            dir.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that app can't see non-media files created by other packages
     */
    @Test
    public void testListDirectoriesWithNonMediaFiles() throws Exception {
        final File downloadDir = getDownloadDir();
        final File dir = new File(downloadDir, TEST_DIRECTORY_NAME);
        final File pdfFile = new File(dir, NONMEDIA_FILE_NAME);
        final String pdfFileName = pdfFile.getName();
        try {
            if (!dir.exists()) {
                assertThat(dir.mkdir()).isTrue();
            }

            // Install TEST_APP_A and create non media file in the new directory.
            installApp(TEST_APP_A);
            assertThat(createFileAs(TEST_APP_A, pdfFile.getPath())).isTrue();

            // TEST_APP_A should see TEST_DIRECTORY in downloadDir and new non media file in
            // TEST_DIRECTORY.
            assertThat(listAs(TEST_APP_A, downloadDir.getPath())).contains(TEST_DIRECTORY_NAME);
            assertThat(listAs(TEST_APP_A, dir.getPath())).containsExactly(pdfFileName);

            // Install TEST_APP_B with storage permission.
            installAppWithStoragePermissions(TEST_APP_B);
            // TEST_APP_B with storage permission should see TEST_DIRECTORY in downloadDir
            // and should not see new non media file in TEST_DIRECTORY.
            assertThat(listAs(TEST_APP_B, downloadDir.getPath())).contains(TEST_DIRECTORY_NAME);
            assertThat(listAs(TEST_APP_B, dir.getPath())).doesNotContain(pdfFileName);
        } finally {
            uninstallAppNoThrow(TEST_APP_B);
            deleteFileAsNoThrow(TEST_APP_A, pdfFile.getPath());
            dir.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that app can only see its directory in Android/data.
     */
    @Test
    public void testListFilesFromExternalFilesDirectory() throws Exception {
        final String packageName = THIS_PACKAGE_NAME;
        final File nonmediaFile = new File(getExternalFilesDir(), NONMEDIA_FILE_NAME);

        try {
            // Create a file in app's external files directory
            if (!nonmediaFile.exists()) {
                assertThat(nonmediaFile.createNewFile()).isTrue();
            }
            // App should see its directory and directories of shared packages. App should see all
            // files and directories in its external directory.
            assertDirectoryContains(nonmediaFile.getParentFile(), nonmediaFile);

            // Install TEST_APP_A with READ_EXTERNAL_STORAGE permission.
            // TEST_APP_A should not see other app's external files directory.
            installAppWithStoragePermissions(TEST_APP_A);

            assertThrows(IOException.class,
                    () -> listAs(TEST_APP_A, getAndroidDataDir().getPath()));
            assertThrows(IOException.class,
                    () -> listAs(TEST_APP_A, getExternalFilesDir().getPath()));
        } finally {
            nonmediaFile.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that app can see files and directories in Android/media.
     */
    @Test
    public void testListFilesFromExternalMediaDirectory() throws Exception {
        final File videoFile = new File(getExternalMediaDir(), VIDEO_FILE_NAME);

        try {
            // Create a file in app's external media directory
            if (!videoFile.exists()) {
                assertThat(videoFile.createNewFile()).isTrue();
            }

            // App should see its directory and other app's external media directories with media
            // files.
            assertDirectoryContains(videoFile.getParentFile(), videoFile);

            // Install TEST_APP_A with READ_EXTERNAL_STORAGE permission.
            // TEST_APP_A with storage permission should see other app's external media directory.
            installAppWithStoragePermissions(TEST_APP_A);
            // Apps with READ_EXTERNAL_STORAGE can list files in other app's external media
            // directory.
            assertThat(listAs(TEST_APP_A, getAndroidMediaDir().getPath()))
                    .contains(THIS_PACKAGE_NAME);
            assertThat(listAs(TEST_APP_A, getExternalMediaDir().getPath()))
                    .containsExactly(videoFile.getName());
        } finally {
            videoFile.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that readdir lists unsupported file types in default directories.
     */
    @Test
    public void testListUnsupportedFileType() throws Exception {
        final File pdfFile = new File(getDcimDir(), NONMEDIA_FILE_NAME);
        final File videoFile = new File(getMusicDir(), VIDEO_FILE_NAME);
        try {
            // TEST_APP_A with storage permission should not see pdf file in DCIM
            executeShellCommand("touch " + pdfFile.getAbsolutePath());
            assertThat(pdfFile.exists()).isTrue();
            assertThat(MediaStore.scanFile(getContentResolver(), pdfFile)).isNotNull();

            installAppWithStoragePermissions(TEST_APP_A);
            assertThat(listAs(TEST_APP_A, getDcimDir().getPath()))
                    .doesNotContain(NONMEDIA_FILE_NAME);

            executeShellCommand("touch " + videoFile.getAbsolutePath());
            // We don't insert files to db for files created by shell.
            assertThat(MediaStore.scanFile(getContentResolver(), videoFile)).isNotNull();
            // TEST_APP_A with storage permission should see video file in Music directory.
            assertThat(listAs(TEST_APP_A, getMusicDir().getPath())).contains(VIDEO_FILE_NAME);
        } finally {
            executeShellCommand("rm " + pdfFile.getAbsolutePath());
            executeShellCommand("rm " + videoFile.getAbsolutePath());
            MediaStore.scanFile(getContentResolver(), pdfFile);
            MediaStore.scanFile(getContentResolver(), videoFile);
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testMetaDataRedaction() throws Exception {
        File jpgFile = new File(getPicturesDir(), "img_metadata.jpg");
        try {
            if (jpgFile.exists()) {
                assertThat(jpgFile.delete()).isTrue();
            }

            HashMap<String, String> originalExif =
                    getExifMetadataFromRawResource(R.raw.img_with_metadata);

            try (InputStream in =
                            getContext().getResources().openRawResource(R.raw.img_with_metadata);
                    OutputStream out = new FileOutputStream(jpgFile)) {
                // Dump the image we have to external storage
                FileUtils.copy(in, out);
            }

            HashMap<String, String> exif = getExifMetadata(jpgFile);
            assertExifMetadataMatch(exif, originalExif);

            installAppWithStoragePermissions(TEST_APP_A);
            HashMap<String, String> exifFromTestApp =
                    readExifMetadataFromTestApp(TEST_APP_A, jpgFile.getPath());
            // Other apps shouldn't have access to the same metadata without explicit permission
            assertExifMetadataMismatch(exifFromTestApp, originalExif);

            // TODO(b/146346138): Test that if we give TEST_APP_A write URI permission,
            //  it would be able to access the metadata.
        } finally {
            jpgFile.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testOpenFilePathFirstWriteContentResolver() throws Exception {
        String displayName = "open_file_path_write_content_resolver.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            assertThat(file.createNewFile()).isTrue();

            ParcelFileDescriptor readPfd =
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_WRITE);
            ParcelFileDescriptor writePfd = openWithMediaProvider(file, "rw");

            assertRWR(readPfd, writePfd);
            assertUpperFsFd(writePfd); // With cache
        } finally {
            file.delete();
        }
    }

    @Test
    public void testOpenContentResolverFirstWriteContentResolver() throws Exception {
        String displayName = "open_content_resolver_write_content_resolver.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            assertThat(file.createNewFile()).isTrue();

            ParcelFileDescriptor writePfd = openWithMediaProvider(file, "rw");
            ParcelFileDescriptor readPfd =
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_WRITE);

            assertRWR(readPfd, writePfd);
            assertLowerFsFd(writePfd);
        } finally {
            file.delete();
        }
    }

    @Test
    public void testOpenFilePathFirstWriteFilePath() throws Exception {
        String displayName = "open_file_path_write_file_path.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            assertThat(file.createNewFile()).isTrue();

            ParcelFileDescriptor writePfd =
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_WRITE);
            ParcelFileDescriptor readPfd = openWithMediaProvider(file, "rw");

            assertRWR(readPfd, writePfd);
            assertUpperFsFd(readPfd); // With cache
        } finally {
            file.delete();
        }
    }

    @Test
    public void testOpenContentResolverFirstWriteFilePath() throws Exception {
        String displayName = "open_content_resolver_write_file_path.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            assertThat(file.createNewFile()).isTrue();

            ParcelFileDescriptor readPfd = openWithMediaProvider(file, "rw");
            ParcelFileDescriptor writePfd =
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_WRITE);

            assertRWR(readPfd, writePfd);
            assertLowerFsFd(readPfd);
        } finally {
            file.delete();
        }
    }

    @Test
    public void testOpenContentResolverWriteOnly() throws Exception {
        String displayName = "open_content_resolver_write_only.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            assertThat(file.createNewFile()).isTrue();

            // We upgrade 'w' only to 'rw'
            ParcelFileDescriptor writePfd = openWithMediaProvider(file, "w");
            ParcelFileDescriptor readPfd = openWithMediaProvider(file, "rw");

            assertRWR(readPfd, writePfd);
            assertRWR(writePfd, readPfd); // Can read on 'w' only pfd
            assertLowerFsFd(writePfd);
            assertLowerFsFd(readPfd);
        } finally {
            file.delete();
        }
    }

    @Test
    public void testOpenContentResolverDup() throws Exception {
        String displayName = "open_content_resolver_dup.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            file.delete();
            assertThat(file.createNewFile()).isTrue();

            // Even if we close the original fd, since we have a dup open
            // the FUSE IO should still bypass the cache
            try (ParcelFileDescriptor writePfd = openWithMediaProvider(file, "rw")) {
                try (ParcelFileDescriptor writePfdDup = writePfd.dup();
                        ParcelFileDescriptor readPfd = ParcelFileDescriptor.open(
                                file, ParcelFileDescriptor.MODE_READ_WRITE)) {
                    writePfd.close();

                    assertRWR(readPfd, writePfdDup);
                    assertLowerFsFd(writePfdDup);
                }
            }
        } finally {
            file.delete();
        }
    }

    @Test
    public void testOpenContentResolverClose() throws Exception {
        String displayName = "open_content_resolver_close.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            byte[] readBuffer = new byte[10];
            byte[] writeBuffer = new byte[10];
            Arrays.fill(writeBuffer, (byte) 1);

            assertThat(file.createNewFile()).isTrue();

            // Lower fs open and write
            ParcelFileDescriptor writePfd = openWithMediaProvider(file, "rw");
            Os.pwrite(writePfd.getFileDescriptor(), writeBuffer, 0, 10, 0);

            // Close so upper fs open will not use direct_io
            writePfd.close();

            // Upper fs open and read without direct_io
            ParcelFileDescriptor readPfd =
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_WRITE);
            Os.pread(readPfd.getFileDescriptor(), readBuffer, 0, 10, 0);

            // Last write on lower fs is visible via upper fs
            assertThat(readBuffer).isEqualTo(writeBuffer);
            assertThat(readPfd.getStatSize()).isEqualTo(writeBuffer.length);
        } finally {
            file.delete();
        }
    }

    @Test
    public void testContentResolverDelete() throws Exception {
        String displayName = "content_resolver_delete.jpg";
        File file = new File(getDcimDir(), displayName);

        try {
            assertThat(file.createNewFile()).isTrue();

            deleteWithMediaProvider(file);

            assertThat(file.exists()).isFalse();
            assertThat(file.createNewFile()).isTrue();
        } finally {
            file.delete();
        }
    }

    @Test
    public void testContentResolverUpdate() throws Exception {
        String oldDisplayName = "content_resolver_update_old.jpg";
        String newDisplayName = "content_resolver_update_new.jpg";
        File oldFile = new File(getDcimDir(), oldDisplayName);
        File newFile = new File(getDcimDir(), newDisplayName);

        try {
            assertThat(oldFile.createNewFile()).isTrue();
            // Publish the pending oldFile before updating with MediaProvider. Not publishing the
            // file will make MP consider pending from FUSE as explicit IS_PENDING
            final Uri uri = MediaStore.scanFile(getContentResolver(), oldFile);
            assertNotNull(uri);

            updateDisplayNameWithMediaProvider(uri,
                    Environment.DIRECTORY_DCIM, oldDisplayName, newDisplayName);

            assertThat(oldFile.exists()).isFalse();
            assertThat(oldFile.createNewFile()).isTrue();
            assertThat(newFile.exists()).isTrue();
            assertThat(newFile.createNewFile()).isFalse();
        } finally {
            oldFile.delete();
            newFile.delete();
        }
    }

    @Test
    public void testCreateLowerCaseDeleteUpperCase() throws Exception {
        File upperCase = new File(getDownloadDir(), "CREATE_LOWER_DELETE_UPPER");
        File lowerCase = new File(getDownloadDir(), "create_lower_delete_upper");

        createDeleteCreate(lowerCase, upperCase);
    }

    @Test
    public void testCreateUpperCaseDeleteLowerCase() throws Exception {
        File upperCase = new File(getDownloadDir(), "CREATE_UPPER_DELETE_LOWER");
        File lowerCase = new File(getDownloadDir(), "create_upper_delete_lower");

        createDeleteCreate(upperCase, lowerCase);
    }

    @Test
    public void testCreateMixedCaseDeleteDifferentMixedCase() throws Exception {
        File mixedCase1 = new File(getDownloadDir(), "CrEaTe_MiXeD_dElEtE_mIxEd");
        File mixedCase2 = new File(getDownloadDir(), "cReAtE_mIxEd_DeLeTe_MiXeD");

        createDeleteCreate(mixedCase1, mixedCase2);
    }

    @Test
    public void testAndroidDataObbDoesNotForgetMount() throws Exception {
        File dataDir = getContext().getExternalFilesDir(null);
        File upperCaseDataDir = new File(dataDir.getPath().replace("Android/data", "ANDROID/DATA"));

        File obbDir = getContext().getObbDir();
        File upperCaseObbDir = new File(obbDir.getPath().replace("Android/obb", "ANDROID/OBB"));


        StructStat beforeDataStruct = Os.stat(dataDir.getPath());
        StructStat beforeObbStruct = Os.stat(obbDir.getPath());

        assertThat(dataDir.exists()).isTrue();
        assertThat(upperCaseDataDir.exists()).isTrue();
        assertThat(obbDir.exists()).isTrue();
        assertThat(upperCaseObbDir.exists()).isTrue();

        StructStat afterDataStruct = Os.stat(upperCaseDataDir.getPath());
        StructStat afterObbStruct = Os.stat(upperCaseObbDir.getPath());

        assertThat(beforeDataStruct.st_dev).isEqualTo(afterDataStruct.st_dev);
        assertThat(beforeObbStruct.st_dev).isEqualTo(afterObbStruct.st_dev);
    }

    @Test
    public void testCacheConsistencyForCaseInsensitivity() throws Exception {
        File upperCaseFile = new File(getDownloadDir(), "CACHE_CONSISTENCY_FOR_CASE_INSENSITIVITY");
        File lowerCaseFile = new File(getDownloadDir(), "cache_consistency_for_case_insensitivity");

        try {
            ParcelFileDescriptor upperCasePfd =
                    ParcelFileDescriptor.open(upperCaseFile,
                            ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE);
            ParcelFileDescriptor lowerCasePfd =
                    ParcelFileDescriptor.open(lowerCaseFile,
                            ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE);

            assertRWR(upperCasePfd, lowerCasePfd);
            assertRWR(lowerCasePfd, upperCasePfd);
        } finally {
            upperCaseFile.delete();
            lowerCaseFile.delete();
        }
    }

    private void createDeleteCreate(File create, File delete) throws Exception {
        try {
            assertThat(create.createNewFile()).isTrue();
            Thread.sleep(5);

            assertThat(delete.delete()).isTrue();
            Thread.sleep(5);

            assertThat(create.createNewFile()).isTrue();
            Thread.sleep(5);
        } finally {
            create.delete();
            delete.delete();
        }
    }

    @Test
    public void testReadStorageInvalidation() throws Exception {
        testAppOpInvalidation(TEST_APP_C, new File(getDcimDir(), "read_storage.jpg"),
                Manifest.permission.READ_EXTERNAL_STORAGE,
                AppOpsManager.OPSTR_READ_EXTERNAL_STORAGE, /* forWrite */ false);
    }

    @Test
    public void testWriteStorageInvalidation() throws Exception {
        testAppOpInvalidation(TEST_APP_C_LEGACY, new File(getDcimDir(), "write_storage.jpg"),
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                AppOpsManager.OPSTR_WRITE_EXTERNAL_STORAGE, /* forWrite */ true);
    }

    @Test
    public void testManageStorageInvalidation() throws Exception {
        testAppOpInvalidation(TEST_APP_C, new File(getDownloadDir(), "manage_storage.pdf"),
                /* permission */ null, OPSTR_MANAGE_EXTERNAL_STORAGE, /* forWrite */ true);
    }

    @Test
    public void testWriteImagesInvalidation() throws Exception {
        testAppOpInvalidation(TEST_APP_C, new File(getDcimDir(), "write_images.jpg"),
                /* permission */ null, AppOpsManager.OPSTR_WRITE_MEDIA_IMAGES, /* forWrite */ true);
    }

    @Test
    public void testWriteVideoInvalidation() throws Exception {
        testAppOpInvalidation(TEST_APP_C, new File(getDcimDir(), "write_video.mp4"),
                /* permission */ null, AppOpsManager.OPSTR_WRITE_MEDIA_VIDEO, /* forWrite */ true);
    }

    @Test
    public void testAccessMediaLocationInvalidation() throws Exception {
        File imgFile = new File(getDcimDir(), "access_media_location.jpg");

        try {
            // Setup image with sensitive data on external storage
            HashMap<String, String> originalExif =
                    getExifMetadataFromRawResource(R.raw.img_with_metadata);
            try (InputStream in =
                            getContext().getResources().openRawResource(R.raw.img_with_metadata);
                    OutputStream out = new FileOutputStream(imgFile)) {
                // Dump the image we have to external storage
                FileUtils.copy(in, out);
            }
            HashMap<String, String> exif = getExifMetadata(imgFile);
            assertExifMetadataMatch(exif, originalExif);

            // Install test app
            installAppWithStoragePermissions(TEST_APP_C);

            // Grant A_M_L and verify access to sensitive data
            grantPermission(TEST_APP_C.getPackageName(), Manifest.permission.ACCESS_MEDIA_LOCATION);
            HashMap<String, String> exifFromTestApp =
                    readExifMetadataFromTestApp(TEST_APP_C, imgFile.getPath());
            assertExifMetadataMatch(exifFromTestApp, originalExif);

            // Revoke A_M_L and verify sensitive data redaction
            revokePermission(
                    TEST_APP_C.getPackageName(), Manifest.permission.ACCESS_MEDIA_LOCATION);
            exifFromTestApp = readExifMetadataFromTestApp(TEST_APP_C, imgFile.getPath());
            assertExifMetadataMismatch(exifFromTestApp, originalExif);

            // Re-grant A_M_L and verify access to sensitive data
            grantPermission(TEST_APP_C.getPackageName(), Manifest.permission.ACCESS_MEDIA_LOCATION);
            exifFromTestApp = readExifMetadataFromTestApp(TEST_APP_C, imgFile.getPath());
            assertExifMetadataMatch(exifFromTestApp, originalExif);
        } finally {
            imgFile.delete();
            uninstallAppNoThrow(TEST_APP_C);
        }
    }

    @Test
    public void testAppUpdateInvalidation() throws Exception {
        File file = new File(getDcimDir(), "app_update.jpg");
        try {
            assertThat(file.createNewFile()).isTrue();

            // Install legacy
            installAppWithStoragePermissions(TEST_APP_C_LEGACY);
            grantPermission(TEST_APP_C_LEGACY.getPackageName(),
                    Manifest.permission.WRITE_EXTERNAL_STORAGE); // Grants write access for legacy
            // Legacy app can read and write media files contributed by others
            assertThat(openFileAs(TEST_APP_C_LEGACY, file.getPath(), /* forWrite */ false)).isTrue();
            assertThat(openFileAs(TEST_APP_C_LEGACY, file.getPath(), /* forWrite */ true)).isTrue();

            // Update to non-legacy
            installAppWithStoragePermissions(TEST_APP_C);
            grantPermission(TEST_APP_C_LEGACY.getPackageName(),
                    Manifest.permission.WRITE_EXTERNAL_STORAGE); // No effect for non-legacy
            // Non-legacy app can read media files contributed by others
            assertThat(openFileAs(TEST_APP_C, file.getPath(), /* forWrite */ false)).isTrue();
            // But cannot write
            assertThat(openFileAs(TEST_APP_C, file.getPath(), /* forWrite */ true)).isFalse();
        } finally {
            file.delete();
            uninstallAppNoThrow(TEST_APP_C);
        }
    }

    @Test
    public void testAppReinstallInvalidation() throws Exception {
        File file = new File(getDcimDir(), "app_reinstall.jpg");

        try {
            assertThat(file.createNewFile()).isTrue();

            // Install
            installAppWithStoragePermissions(TEST_APP_C);
            assertThat(openFileAs(TEST_APP_C, file.getPath(), /* forWrite */ false)).isTrue();

            // Re-install
            uninstallAppNoThrow(TEST_APP_C);
            installApp(TEST_APP_C);
            assertThat(openFileAs(TEST_APP_C, file.getPath(), /* forWrite */ false)).isFalse();
        } finally {
            file.delete();
            uninstallAppNoThrow(TEST_APP_C);
        }
    }

    private void testAppOpInvalidation(TestApp app, File file, @Nullable String permission,
            String opstr, boolean forWrite) throws Exception {
        try {
            installApp(app);
            assertThat(file.createNewFile()).isTrue();
            assertAppOpInvalidation(app, file, permission, opstr, forWrite);
        } finally {
            file.delete();
            uninstallApp(app);
        }
    }

    /** If {@code permission} is null, appops are flipped, otherwise permissions are flipped */
    private void assertAppOpInvalidation(TestApp app, File file, @Nullable String permission,
            String opstr, boolean forWrite) throws Exception {
        String packageName = app.getPackageName();
        int uid = getContext().getPackageManager().getPackageUid(packageName, 0);

        // Deny
        if (permission != null) {
            revokePermission(packageName, permission);
        } else {
            denyAppOpsToUid(uid, opstr);
        }
        assertThat(openFileAs(app, file.getPath(), forWrite)).isFalse();

        // Grant
        if (permission != null) {
            grantPermission(packageName, permission);
        } else {
            allowAppOpsToUid(uid, opstr);
        }
        assertThat(openFileAs(app, file.getPath(), forWrite)).isTrue();

        // Deny
        if (permission != null) {
            revokePermission(packageName, permission);
        } else {
            denyAppOpsToUid(uid, opstr);
        }
        assertThat(openFileAs(app, file.getPath(), forWrite)).isFalse();
    }

    @Test
    public void testSystemGalleryAppHasFullAccessToImages() throws Exception {
        final File otherAppImageFile = new File(getDcimDir(), "other_" + IMAGE_FILE_NAME);
        final File topLevelImageFile = new File(getExternalStorageDir(), IMAGE_FILE_NAME);
        final File imageInAnObviouslyWrongPlace = new File(getMusicDir(), IMAGE_FILE_NAME);

        try {
            installApp(TEST_APP_A);
            allowAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);

            // Have another app create an image file
            assertThat(createFileAs(TEST_APP_A, otherAppImageFile.getPath())).isTrue();
            assertThat(otherAppImageFile.exists()).isTrue();

            // Assert we can write to the file
            try (final FileOutputStream fos = new FileOutputStream(otherAppImageFile)) {
                fos.write(BYTES_DATA1);
            }

            // Assert we can read from the file
            assertFileContent(otherAppImageFile, BYTES_DATA1);

            // Assert we can delete the file
            assertThat(otherAppImageFile.delete()).isTrue();
            assertThat(otherAppImageFile.exists()).isFalse();

            // Can create an image anywhere
            assertCanCreateFile(topLevelImageFile);
            assertCanCreateFile(imageInAnObviouslyWrongPlace);

            // Put the file back in its place and let TEST_APP_A delete it
            assertThat(otherAppImageFile.createNewFile()).isTrue();
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, otherAppImageFile.getAbsolutePath());
            otherAppImageFile.delete();
            uninstallApp(TEST_APP_A);
            denyAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);
        }
    }

    @Test
    public void testSystemGalleryAppHasNoFullAccessToAudio() throws Exception {
        final File otherAppAudioFile = new File(getMusicDir(), "other_" + AUDIO_FILE_NAME);
        final File topLevelAudioFile = new File(getExternalStorageDir(), AUDIO_FILE_NAME);
        final File audioInAnObviouslyWrongPlace = new File(getPicturesDir(), AUDIO_FILE_NAME);

        try {
            installApp(TEST_APP_A);
            allowAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);

            // Have another app create an audio file
            assertThat(createFileAs(TEST_APP_A, otherAppAudioFile.getPath())).isTrue();
            assertThat(otherAppAudioFile.exists()).isTrue();

            // Assert we can't access the file
            assertThat(canOpen(otherAppAudioFile, /* forWrite */ false)).isFalse();
            assertThat(canOpen(otherAppAudioFile, /* forWrite */ true)).isFalse();

            // Assert we can't delete the file
            assertThat(otherAppAudioFile.delete()).isFalse();

            // Can't create an audio file where it doesn't belong
            assertThrows(IOException.class, "Operation not permitted",
                    () -> { topLevelAudioFile.createNewFile(); });
            assertThrows(IOException.class, "Operation not permitted",
                    () -> { audioInAnObviouslyWrongPlace.createNewFile(); });
        } finally {
            deleteFileAs(TEST_APP_A, otherAppAudioFile.getPath());
            uninstallApp(TEST_APP_A);
            topLevelAudioFile.delete();
            audioInAnObviouslyWrongPlace.delete();
            denyAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);
        }
    }

    @Test
    public void testSystemGalleryCanRenameImagesAndVideos() throws Exception {
        final File otherAppVideoFile = new File(getDcimDir(), "other_" + VIDEO_FILE_NAME);
        final File imageFile = new File(getPicturesDir(), IMAGE_FILE_NAME);
        final File videoFile = new File(getPicturesDir(), VIDEO_FILE_NAME);
        final File topLevelVideoFile = new File(getExternalStorageDir(), VIDEO_FILE_NAME);
        final File musicFile = new File(getMusicDir(), AUDIO_FILE_NAME);
        try {
            installApp(TEST_APP_A);
            allowAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);

            // Have another app create a video file
            assertThat(createFileAs(TEST_APP_A, otherAppVideoFile.getPath())).isTrue();
            assertThat(otherAppVideoFile.exists()).isTrue();

            // Write some data to the file
            try (final FileOutputStream fos = new FileOutputStream(otherAppVideoFile)) {
                fos.write(BYTES_DATA1);
            }
            assertFileContent(otherAppVideoFile, BYTES_DATA1);

            // Assert we can rename the file and ensure the file has the same content
            assertCanRenameFile(otherAppVideoFile, videoFile);
            assertFileContent(videoFile, BYTES_DATA1);
            // We can even move it to the top level directory
            assertCanRenameFile(videoFile, topLevelVideoFile);
            assertFileContent(topLevelVideoFile, BYTES_DATA1);
            // And we can even convert it into an image file, because why not?
            assertCanRenameFile(topLevelVideoFile, imageFile);
            assertFileContent(imageFile, BYTES_DATA1);

            // We can convert it to a music file, but we won't have access to music file after
            // renaming.
            assertThat(imageFile.renameTo(musicFile)).isTrue();
            assertThat(getFileRowIdFromDatabase(musicFile)).isEqualTo(-1);
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, otherAppVideoFile.getAbsolutePath());
            uninstallApp(TEST_APP_A);
            imageFile.delete();
            videoFile.delete();
            topLevelVideoFile.delete();
            executeShellCommand("rm  " + musicFile.getAbsolutePath());
            MediaStore.scanFile(getContentResolver(), musicFile);
            denyAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);
        }
    }

    /**
     * Test that basic file path restrictions are enforced on file rename.
     */
    @Test
    public void testRenameFile() throws Exception {
        final File downloadDir = getDownloadDir();
        final File nonMediaDir = new File(downloadDir, TEST_DIRECTORY_NAME);
        final File pdfFile1 = new File(downloadDir, NONMEDIA_FILE_NAME);
        final File pdfFile2 = new File(nonMediaDir, NONMEDIA_FILE_NAME);
        final File videoFile1 = new File(getDcimDir(), VIDEO_FILE_NAME);
        final File videoFile2 = new File(getMoviesDir(), VIDEO_FILE_NAME);
        final File videoFile3 = new File(downloadDir, VIDEO_FILE_NAME);

        try {
            // Renaming non media file to media directory is not allowed.
            assertThat(pdfFile1.createNewFile()).isTrue();
            assertCantRenameFile(pdfFile1, new File(getDcimDir(), NONMEDIA_FILE_NAME));
            assertCantRenameFile(pdfFile1, new File(getMusicDir(), NONMEDIA_FILE_NAME));
            assertCantRenameFile(pdfFile1, new File(getMoviesDir(), NONMEDIA_FILE_NAME));

            // Renaming non media files to non media directories is allowed.
            if (!nonMediaDir.exists()) {
                assertThat(nonMediaDir.mkdirs()).isTrue();
            }
            // App can rename pdfFile to non media directory.
            assertCanRenameFile(pdfFile1, pdfFile2);

            assertThat(videoFile1.createNewFile()).isTrue();
            // App can rename video file to Movies directory
            assertCanRenameFile(videoFile1, videoFile2);
            // App can rename video file to Download directory
            assertCanRenameFile(videoFile2, videoFile3);
        } finally {
            pdfFile1.delete();
            pdfFile2.delete();
            videoFile1.delete();
            videoFile2.delete();
            videoFile3.delete();
            nonMediaDir.delete();
        }
    }

    /**
     * Test that renaming file to different mime type is allowed.
     */
    @Test
    public void testRenameFileType() throws Exception {
        final File pdfFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        final File videoFile = new File(getDcimDir(), VIDEO_FILE_NAME);
        try {
            assertThat(pdfFile.createNewFile()).isTrue();
            assertThat(videoFile.exists()).isFalse();
            // Moving pdfFile to DCIM directory is not allowed.
            assertCantRenameFile(pdfFile, new File(getDcimDir(), NONMEDIA_FILE_NAME));
            // However, moving pdfFile to DCIM directory with changing the mime type to video is
            // allowed.
            assertCanRenameFile(pdfFile, videoFile);

            // On rename, MediaProvider database entry for pdfFile should be updated with new
            // videoFile path and mime type should be updated to video/mp4.
            assertThat(getFileMimeTypeFromDatabase(videoFile)).isEqualTo("video/mp4");
        } finally {
            pdfFile.delete();
            videoFile.delete();
        }
    }

    /**
     * Test that renaming files overwrites files in newPath.
     */
    @Test
    public void testRenameAndReplaceFile() throws Exception {
        final File videoFile1 = new File(getDcimDir(), VIDEO_FILE_NAME);
        final File videoFile2 = new File(getMoviesDir(), VIDEO_FILE_NAME);
        final ContentResolver cr = getContentResolver();
        try {
            assertThat(videoFile1.createNewFile()).isTrue();
            assertThat(videoFile2.createNewFile()).isTrue();
            final Uri uriVideoFile1 = MediaStore.scanFile(cr, videoFile1);
            final Uri uriVideoFile2 = MediaStore.scanFile(cr, videoFile2);

            // Renaming a file which replaces file in newPath videoFile2 is allowed.
            assertCanRenameFile(videoFile1, videoFile2);

            // Uri of videoFile2 should be accessible after rename.
            assertThat(cr.openFileDescriptor(uriVideoFile2, "rw")).isNotNull();
            // Uri of videoFile1 should not be accessible after rename.
            assertThrows(FileNotFoundException.class,
                    () -> { cr.openFileDescriptor(uriVideoFile1, "rw"); });
        } finally {
            videoFile1.delete();
            videoFile2.delete();
        }
    }

    /**
     * Test that app without write permission for file can't update the file.
     */
    @Test
    public void testRenameFileNotOwned() throws Exception {
        final File videoFile1 = new File(getDcimDir(), VIDEO_FILE_NAME);
        final File videoFile2 = new File(getMoviesDir(), VIDEO_FILE_NAME);
        try {
            installApp(TEST_APP_A);
            assertThat(createFileAs(TEST_APP_A, videoFile1.getAbsolutePath())).isTrue();
            // App can't rename a file owned by TEST_APP_A.
            assertCantRenameFile(videoFile1, videoFile2);

            assertThat(videoFile2.createNewFile()).isTrue();
            // App can't rename a file to videoFile1 which is owned by TEST_APP_A
            assertCantRenameFile(videoFile2, videoFile1);
            // TODO(b/146346138): Test that app with right URI permission should be able to rename
            // the corresponding file
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, videoFile1.getAbsolutePath());
            videoFile2.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that renaming directories is allowed and aligns to default directory restrictions.
     */
    @Test
    public void testRenameDirectory() throws Exception {
        final File dcimDir = getDcimDir();
        final File downloadDir = getDownloadDir();
        final String nonMediaDirectoryName = TEST_DIRECTORY_NAME + "NonMedia";
        final File nonMediaDirectory = new File(downloadDir, nonMediaDirectoryName);
        final File pdfFile = new File(nonMediaDirectory, NONMEDIA_FILE_NAME);

        final String mediaDirectoryName = TEST_DIRECTORY_NAME + "Media";
        final File mediaDirectory1 = new File(dcimDir, mediaDirectoryName);
        final File videoFile1 = new File(mediaDirectory1, VIDEO_FILE_NAME);
        final File mediaDirectory2 = new File(downloadDir, mediaDirectoryName);
        final File videoFile2 = new File(mediaDirectory2, VIDEO_FILE_NAME);
        final File mediaDirectory3 = new File(getMoviesDir(), TEST_DIRECTORY_NAME);
        final File videoFile3 = new File(mediaDirectory3, VIDEO_FILE_NAME);
        final File mediaDirectory4 = new File(mediaDirectory3, mediaDirectoryName);

        try {
            if (!nonMediaDirectory.exists()) {
                assertThat(nonMediaDirectory.mkdirs()).isTrue();
            }
            assertThat(pdfFile.createNewFile()).isTrue();
            // Move directory with pdf file to DCIM directory is not allowed.
            assertThat(nonMediaDirectory.renameTo(new File(dcimDir, nonMediaDirectoryName)))
                    .isFalse();

            if (!mediaDirectory1.exists()) {
                assertThat(mediaDirectory1.mkdirs()).isTrue();
            }
            assertThat(videoFile1.createNewFile()).isTrue();
            // Renaming to and from default directories is not allowed.
            assertThat(mediaDirectory1.renameTo(dcimDir)).isFalse();
            // Moving top level default directories is not allowed.
            assertCantRenameDirectory(downloadDir, new File(dcimDir, TEST_DIRECTORY_NAME), null);

            // Moving media directory to Download directory is allowed.
            assertCanRenameDirectory(mediaDirectory1, mediaDirectory2, new File[] {videoFile1},
                    new File[] {videoFile2});

            // Moving media directory to Movies directory and renaming directory in new path is
            // allowed.
            assertCanRenameDirectory(mediaDirectory2, mediaDirectory3, new File[] {videoFile2},
                    new File[] {videoFile3});

            // Can't rename a mediaDirectory to non empty non Media directory.
            assertCantRenameDirectory(mediaDirectory3, nonMediaDirectory, new File[] {videoFile3});
            // Can't rename a file to a directory.
            assertCantRenameFile(videoFile3, mediaDirectory3);
            // Can't rename a directory to file.
            assertCantRenameDirectory(mediaDirectory3, pdfFile, null);
            if (!mediaDirectory4.exists()) {
                assertThat(mediaDirectory4.mkdir()).isTrue();
            }
            // Can't rename a directory to subdirectory of itself.
            assertCantRenameDirectory(mediaDirectory3, mediaDirectory4, new File[] {videoFile3});

        } finally {
            pdfFile.delete();
            nonMediaDirectory.delete();

            videoFile1.delete();
            videoFile2.delete();
            videoFile3.delete();
            mediaDirectory1.delete();
            mediaDirectory2.delete();
            mediaDirectory3.delete();
            mediaDirectory4.delete();
        }
    }

    /**
     * Test that renaming directory checks file ownership permissions.
     */
    @Test
    public void testRenameDirectoryNotOwned() throws Exception {
        final String mediaDirectoryName = TEST_DIRECTORY_NAME + "Media";
        File mediaDirectory1 = new File(getDcimDir(), mediaDirectoryName);
        File mediaDirectory2 = new File(getMoviesDir(), mediaDirectoryName);
        File videoFile = new File(mediaDirectory1, VIDEO_FILE_NAME);

        try {
            installApp(TEST_APP_A);

            if (!mediaDirectory1.exists()) {
                assertThat(mediaDirectory1.mkdirs()).isTrue();
            }
            assertThat(createFileAs(TEST_APP_A, videoFile.getAbsolutePath())).isTrue();
            // App doesn't have access to videoFile1, can't rename mediaDirectory1.
            assertThat(mediaDirectory1.renameTo(mediaDirectory2)).isFalse();
            assertThat(videoFile.exists()).isTrue();
            // Test app can delete the file since the file is not moved to new directory.
            assertThat(deleteFileAs(TEST_APP_A, videoFile.getAbsolutePath())).isTrue();
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, videoFile.getAbsolutePath());
            uninstallAppNoThrow(TEST_APP_A);
            mediaDirectory1.delete();
        }
    }

    /**
     * Test renaming empty directory is allowed
     */
    @Test
    public void testRenameEmptyDirectory() throws Exception {
        final String emptyDirectoryName = TEST_DIRECTORY_NAME + "Media";
        File emptyDirectoryOldPath = new File(getDcimDir(), emptyDirectoryName);
        File emptyDirectoryNewPath = new File(getMoviesDir(), TEST_DIRECTORY_NAME);
        try {
            if (emptyDirectoryOldPath.exists()) {
                executeShellCommand("rm -r " + emptyDirectoryOldPath.getPath());
            }
            assertThat(emptyDirectoryOldPath.mkdirs()).isTrue();
            assertCanRenameDirectory(emptyDirectoryOldPath, emptyDirectoryNewPath, null, null);
        } finally {
            emptyDirectoryOldPath.delete();
            emptyDirectoryNewPath.delete();
        }
    }

    @Test
    public void testManageExternalStorageCanCreateFilesAnywhere() throws Exception {
        pollForManageExternalStorageAllowed();

        final File topLevelPdf = new File(getExternalStorageDir(), NONMEDIA_FILE_NAME);
        final File musicFileInMovies = new File(getMoviesDir(), AUDIO_FILE_NAME);
        final File imageFileInDcim = new File(getDcimDir(), IMAGE_FILE_NAME);

        // Nothing special about this, anyone can create an image file in DCIM
        assertCanCreateFile(imageFileInDcim);
        // This is where we see the special powers of MANAGE_EXTERNAL_STORAGE, because it can
        // create a top level file
        assertCanCreateFile(topLevelPdf);
        // It can even create a music file in Pictures
        assertCanCreateFile(musicFileInMovies);
    }

    @Test
    public void testManageExternalStorageCantReadWriteOtherAppExternalDir() throws Exception {
        pollForManageExternalStorageAllowed();

        try {
            // Install TEST_APP_A with READ_EXTERNAL_STORAGE permission.
            installAppWithStoragePermissions(TEST_APP_A);

            // Let app A create a file in its data dir
            final File otherAppExternalDataDir = new File(getExternalFilesDir().getPath().replace(
                    THIS_PACKAGE_NAME, TEST_APP_A.getPackageName()));
            final File otherAppExternalDataFile = new File(otherAppExternalDataDir,
                    NONMEDIA_FILE_NAME);
            assertCreateFilesAs(TEST_APP_A, otherAppExternalDataFile);

            // File Manager app gets global access with MANAGE_EXTERNAL_STORAGE permission, however,
            // file manager app doesn't have access to other app's external files directory
            assertThat(canOpen(otherAppExternalDataFile, /* forWrite */ false)).isFalse();
            assertThat(canOpen(otherAppExternalDataFile, /* forWrite */ true)).isFalse();
            assertThat(otherAppExternalDataFile.delete()).isFalse();

            assertThat(deleteFileAs(TEST_APP_A, otherAppExternalDataFile.getPath())).isTrue();

            assertThrows(IOException.class,
                    () -> { otherAppExternalDataFile.createNewFile(); });

        } finally {
            uninstallApp(TEST_APP_A); // Uninstalling deletes external app dirs
        }
    }

    /**
     * Tests that an instant app can't access external storage.
     */
    @Test
    @AppModeInstant
    public void testInstantAppsCantAccessExternalStorage() throws Exception {
        assumeTrue("This test requires that the test runs as an Instant app",
                getContext().getPackageManager().isInstantApp());
        assertThat(getContext().getPackageManager().isInstantApp()).isTrue();

        // Can't read ExternalStorageDir
        assertThat(getExternalStorageDir().list()).isNull();

        // Can't create a top-level direcotry
        final File topLevelDir = new File(getExternalStorageDir(), TEST_DIRECTORY_NAME);
        assertThat(topLevelDir.mkdir()).isFalse();

        // Can't create file under root dir
        final File newTxtFile = new File(getExternalStorageDir(), NONMEDIA_FILE_NAME);
        assertThrows(IOException.class,
                () -> { newTxtFile.createNewFile(); });

        // Can't create music file under /MUSIC
        final File newMusicFile = new File(getMusicDir(), AUDIO_FILE_NAME);
        assertThrows(IOException.class,
                () -> { newMusicFile.createNewFile(); });

        // getExternalFilesDir() is not null
        assertThat(getExternalFilesDir()).isNotNull();

        // Can't read/write app specific dir
        assertThat(getExternalFilesDir().list()).isNull();
        assertThat(getExternalFilesDir().exists()).isFalse();
    }

    /**
     * Test that apps can create and delete hidden file.
     */
    @Test
    public void testCanCreateHiddenFile() throws Exception {
        final File hiddenImageFile = new File(getDownloadDir(), ".hiddenFile" + IMAGE_FILE_NAME);
        try {
            assertThat(hiddenImageFile.createNewFile()).isTrue();
            // Write to hidden file is allowed.
            try (final FileOutputStream fos = new FileOutputStream(hiddenImageFile)) {
                fos.write(BYTES_DATA1);
            }
            assertFileContent(hiddenImageFile, BYTES_DATA1);

            assertNotMediaTypeImage(hiddenImageFile);

            assertDirectoryContains(getDownloadDir(), hiddenImageFile);
            assertThat(getFileRowIdFromDatabase(hiddenImageFile)).isNotEqualTo(-1);

            // We can delete hidden file
            assertThat(hiddenImageFile.delete()).isTrue();
            assertThat(hiddenImageFile.exists()).isFalse();
        } finally {
            hiddenImageFile.delete();
        }
    }

    /**
     * Test that apps can rename a hidden file.
     */
    @Test
    public void testCanRenameHiddenFile() throws Exception {
        final String hiddenFileName = ".hidden" + IMAGE_FILE_NAME;
        final File hiddenImageFile1 = new File(getDcimDir(), hiddenFileName);
        final File hiddenImageFile2 = new File(getDownloadDir(), hiddenFileName);
        final File imageFile = new File(getDownloadDir(), IMAGE_FILE_NAME);
        try {
            assertThat(hiddenImageFile1.createNewFile()).isTrue();
            assertCanRenameFile(hiddenImageFile1, hiddenImageFile2);
            assertNotMediaTypeImage(hiddenImageFile2);

            // We can also rename hidden file to non-hidden
            assertCanRenameFile(hiddenImageFile2, imageFile);
            assertIsMediaTypeImage(imageFile);

            // We can rename non-hidden file to hidden
            assertCanRenameFile(imageFile, hiddenImageFile1);
            assertNotMediaTypeImage(hiddenImageFile1);
        } finally {
            hiddenImageFile1.delete();
            hiddenImageFile2.delete();
            imageFile.delete();
        }
    }

    /**
     * Test that files in hidden directory have MEDIA_TYPE=MEDIA_TYPE_NONE
     */
    @Test
    public void testHiddenDirectory() throws Exception {
        final File hiddenDir = new File(getDownloadDir(), ".hidden" + TEST_DIRECTORY_NAME);
        final File hiddenImageFile = new File(hiddenDir, IMAGE_FILE_NAME);
        final File nonHiddenDir = new File(getDownloadDir(), TEST_DIRECTORY_NAME);
        final File imageFile = new File(nonHiddenDir, IMAGE_FILE_NAME);
        try {
            if (!hiddenDir.exists()) {
                assertThat(hiddenDir.mkdir()).isTrue();
            }
            assertThat(hiddenImageFile.createNewFile()).isTrue();

            assertNotMediaTypeImage(hiddenImageFile);

            // Renaming hiddenDir to nonHiddenDir makes the imageFile non-hidden and vice versa
            assertCanRenameDirectory(
                    hiddenDir, nonHiddenDir, new File[] {hiddenImageFile}, new File[] {imageFile});
            assertIsMediaTypeImage(imageFile);

            assertCanRenameDirectory(
                    nonHiddenDir, hiddenDir, new File[] {imageFile}, new File[] {hiddenImageFile});
            assertNotMediaTypeImage(hiddenImageFile);
        } finally {
            hiddenImageFile.delete();
            imageFile.delete();
            hiddenDir.delete();
            nonHiddenDir.delete();
        }
    }

    /**
     * Test that files in directory with nomedia have MEDIA_TYPE=MEDIA_TYPE_NONE
     */
    @Test
    public void testHiddenDirectory_nomedia() throws Exception {
        final File directoryNoMedia = new File(getDownloadDir(), "nomedia" + TEST_DIRECTORY_NAME);
        final File noMediaFile = new File(directoryNoMedia, ".nomedia");
        final File imageFile = new File(directoryNoMedia, IMAGE_FILE_NAME);
        final File videoFile = new File(directoryNoMedia, VIDEO_FILE_NAME);
        try {
            if (!directoryNoMedia.exists()) {
                assertThat(directoryNoMedia.mkdir()).isTrue();
            }
            assertThat(noMediaFile.createNewFile()).isTrue();
            assertThat(imageFile.createNewFile()).isTrue();

            assertNotMediaTypeImage(imageFile);

            // Deleting the .nomedia file makes the parent directory non hidden.
            noMediaFile.delete();
            MediaStore.scanFile(getContentResolver(), directoryNoMedia);
            assertIsMediaTypeImage(imageFile);

            // Creating the .nomedia file makes the parent directory hidden again
            assertThat(noMediaFile.createNewFile()).isTrue();
            MediaStore.scanFile(getContentResolver(), directoryNoMedia);
            assertNotMediaTypeImage(imageFile);

            // Renaming the .nomedia file to non hidden file makes the parent directory non hidden.
            assertCanRenameFile(noMediaFile, videoFile);
            assertIsMediaTypeImage(imageFile);
        } finally {
            noMediaFile.delete();
            imageFile.delete();
            videoFile.delete();
            directoryNoMedia.delete();
        }
    }

    /**
     * Test that only file manager and app that created the hidden file can list it.
     */
    @Test
    public void testListHiddenFile() throws Exception {
        final File dcimDir = getDcimDir();
        final String hiddenImageFileName = ".hidden" + IMAGE_FILE_NAME;
        final File hiddenImageFile = new File(dcimDir, hiddenImageFileName);
        try {
            assertThat(hiddenImageFile.createNewFile()).isTrue();
            assertNotMediaTypeImage(hiddenImageFile);

            assertDirectoryContains(dcimDir, hiddenImageFile);

            installApp(TEST_APP_A, true);
            // TestApp with read permissions can't see the hidden image file created by other app
            assertThat(listAs(TEST_APP_A, dcimDir.getAbsolutePath()))
                    .doesNotContain(hiddenImageFileName);

            final int testAppUid =
                    getContext().getPackageManager().getPackageUid(TEST_APP_A.getPackageName(), 0);
            // FileManager can see the hidden image file created by other app
            try {
                allowAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
                assertThat(listAs(TEST_APP_A, dcimDir.getAbsolutePath()))
                        .contains(hiddenImageFileName);
            } finally {
                denyAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
            }

            // Gallery can not see the hidden image file created by other app
            try {
                allowAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
                assertThat(listAs(TEST_APP_A, dcimDir.getAbsolutePath()))
                        .doesNotContain(hiddenImageFileName);
            } finally {
                denyAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
            }
        } finally {
            hiddenImageFile.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testOpenPendingAndTrashed() throws Exception {
        final File pendingImageFile = new File(getDcimDir(), IMAGE_FILE_NAME);
        final File trashedVideoFile = new File(getPicturesDir(), VIDEO_FILE_NAME);
        final File pendingPdfFile = new File(getDocumentsDir(), NONMEDIA_FILE_NAME);
        final File trashedPdfFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        Uri pendingImgaeFileUri = null;
        Uri trashedVideoFileUri = null;
        Uri pendingPdfFileUri = null;
        Uri trashedPdfFileUri = null;
        try {
            installAppWithStoragePermissions(TEST_APP_A);

            pendingImgaeFileUri = createPendingFile(pendingImageFile);
            assertOpenPendingOrTrashed(pendingImgaeFileUri, TEST_APP_A, /*isImageOrVideo*/ true);

            pendingPdfFileUri = createPendingFile(pendingPdfFile);
            assertOpenPendingOrTrashed(pendingPdfFileUri, TEST_APP_A,
                    /*isImageOrVideo*/ false);

            trashedVideoFileUri = createTrashedFile(trashedVideoFile);
            assertOpenPendingOrTrashed(trashedVideoFileUri, TEST_APP_A, /*isImageOrVideo*/ true);

            trashedPdfFileUri = createTrashedFile(trashedPdfFile);
            assertOpenPendingOrTrashed(trashedPdfFileUri, TEST_APP_A,
                    /*isImageOrVideo*/ false);

        } finally {
            deleteFiles(pendingImageFile, pendingImageFile, trashedVideoFile,
                    trashedPdfFile);
            deleteWithMediaProviderNoThrow(pendingImgaeFileUri, trashedVideoFileUri,
                    pendingPdfFileUri, trashedPdfFileUri);
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testListPendingAndTrashed() throws Exception {
        final File imageFile = new File(getDcimDir(), IMAGE_FILE_NAME);
        final File pdfFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        Uri imageFileUri = null;
        Uri pdfFileUri = null;
        try {
            installAppWithStoragePermissions(TEST_APP_A);

            imageFileUri = createPendingFile(imageFile);
            // Check that only owner package, file manager and system gallery can list pending image
            // file.
            assertListPendingOrTrashed(imageFileUri, imageFile, TEST_APP_A,
                    /*isImageOrVideo*/ true);

            trashFile(imageFileUri);
            // Check that only owner package, file manager and system gallery can list trashed image
            // file.
            assertListPendingOrTrashed(imageFileUri, imageFile, TEST_APP_A,
                    /*isImageOrVideo*/ true);

            pdfFileUri = createPendingFile(pdfFile);
            // Check that only owner package, file manager can list pending non media file.
            assertListPendingOrTrashed(pdfFileUri, pdfFile, TEST_APP_A,
                    /*isImageOrVideo*/ false);

            trashFile(pdfFileUri);
            // Check that only owner package, file manager can list trashed non media file.
            assertListPendingOrTrashed(pdfFileUri, pdfFile, TEST_APP_A,
                    /*isImageOrVideo*/ false);
        } finally {
            deleteWithMediaProviderNoThrow(imageFileUri, pdfFileUri);
            deleteFiles(imageFile, pdfFile);
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testDeletePendingAndTrashed() throws Exception {
        final File pendingVideoFile = new File(getDcimDir(), VIDEO_FILE_NAME);
        final File trashedImageFile = new File(getPicturesDir(), IMAGE_FILE_NAME);
        final File pendingPdfFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        final File trashedPdfFile = new File(getDocumentsDir(), NONMEDIA_FILE_NAME);
        // Actual path of the file gets rewritten for pending and trashed files.
        String pendingVideoFilePath = null;
        String trashedImageFilePath = null;
        String pendingPdfFilePath = null;
        String trashedPdfFilePath = null;
        try {
            pendingVideoFilePath = getFilePathFromUri(createPendingFile(pendingVideoFile));
            trashedImageFilePath = getFilePathFromUri(createTrashedFile(trashedImageFile));
            pendingPdfFilePath = getFilePathFromUri(createPendingFile(pendingPdfFile));
            trashedPdfFilePath = getFilePathFromUri(createTrashedFile(trashedPdfFile));

            // App can delete its own pending and trashed file.
            assertCanDeletePaths(pendingVideoFilePath, trashedImageFilePath, pendingPdfFilePath,
                    trashedPdfFilePath);

            pendingVideoFilePath = getFilePathFromUri(createPendingFile(pendingVideoFile));
            trashedImageFilePath = getFilePathFromUri(createTrashedFile(trashedImageFile));
            pendingPdfFilePath = getFilePathFromUri(createPendingFile(pendingPdfFile));
            trashedPdfFilePath = getFilePathFromUri(createTrashedFile(trashedPdfFile));

            installAppWithStoragePermissions(TEST_APP_A);

            // App can't delete other app's pending and trashed file.
            assertCantDeletePathsAs(TEST_APP_A, pendingVideoFilePath, trashedImageFilePath,
                    pendingPdfFilePath, trashedPdfFilePath);

            final int testAppUid =
                    getContext().getPackageManager().getPackageUid(TEST_APP_A.getPackageName(), 0);
            try {
                allowAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
                // File Manager can delete any pending and trashed file
                assertCanDeletePathsAs(TEST_APP_A, pendingVideoFilePath, trashedImageFilePath,
                        pendingPdfFilePath, trashedPdfFilePath);
            } finally {
                denyAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
            }

            pendingVideoFilePath = getFilePathFromUri(createPendingFile(pendingVideoFile));
            trashedImageFilePath = getFilePathFromUri(createTrashedFile(trashedImageFile));
            pendingPdfFilePath = getFilePathFromUri(createPendingFile(pendingPdfFile));
            trashedPdfFilePath = getFilePathFromUri(createTrashedFile(trashedPdfFile));

            try {
                allowAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
                // System Gallery can delete any pending and trashed image or video file.
                assertTrue(isMediaTypeImageOrVideo(new File(pendingVideoFilePath)));
                assertTrue(isMediaTypeImageOrVideo(new File(trashedImageFilePath)));
                assertCanDeletePathsAs(TEST_APP_A, pendingVideoFilePath, trashedImageFilePath);

                // System Gallery can't delete other app's pending and trashed pdf file.
                assertFalse(isMediaTypeImageOrVideo(new File(pendingPdfFilePath)));
                assertFalse(isMediaTypeImageOrVideo(new File(trashedPdfFilePath)));
                assertCantDeletePathsAs(TEST_APP_A, pendingPdfFilePath, trashedPdfFilePath);
            } finally {
                denyAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
            }
        } finally {
            deletePaths(pendingVideoFilePath, trashedImageFilePath, pendingPdfFilePath,
                    trashedPdfFilePath);
            deleteFiles(pendingVideoFile, trashedImageFile, pendingPdfFile, trashedPdfFile);
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testManageExternalStorageCanDeleteOtherAppsContents() throws Exception {
        pollForManageExternalStorageAllowed();

        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImage = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        try {
            installApp(TEST_APP_A);

            // Create all of the files as another app
            assertThat(createFileAs(TEST_APP_A, otherAppPdf.getPath())).isTrue();
            assertThat(createFileAs(TEST_APP_A, otherAppImage.getPath())).isTrue();
            assertThat(createFileAs(TEST_APP_A, otherAppMusic.getPath())).isTrue();

            assertThat(otherAppPdf.delete()).isTrue();
            assertThat(otherAppPdf.exists()).isFalse();

            assertThat(otherAppImage.delete()).isTrue();
            assertThat(otherAppImage.exists()).isFalse();

            assertThat(otherAppMusic.delete()).isTrue();
            assertThat(otherAppMusic.exists()).isFalse();
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, otherAppPdf.getAbsolutePath());
            deleteFileAsNoThrow(TEST_APP_A, otherAppImage.getAbsolutePath());
            deleteFileAsNoThrow(TEST_APP_A, otherAppMusic.getAbsolutePath());
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testAccess_file() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);

        final File downloadDir = getDownloadDir();
        final File otherAppPdf = new File(downloadDir, "other-" + NONMEDIA_FILE_NAME);
        final File shellPdfAtRoot = new File(getExternalStorageDir(),
                "shell-" + NONMEDIA_FILE_NAME);
        final File otherAppImage = new File(getDcimDir(), "other-" + IMAGE_FILE_NAME);
        final File myAppPdf = new File(downloadDir, "my-" + NONMEDIA_FILE_NAME);
        final File doesntExistPdf = new File(downloadDir, "nada-" + NONMEDIA_FILE_NAME);

        try {
            installApp(TEST_APP_A);

            assertThat(createFileAs(TEST_APP_A, otherAppPdf.getPath())).isTrue();
            assertThat(createFileAs(TEST_APP_A, otherAppImage.getPath())).isTrue();

            // We can read our image and pdf files.
            assertThat(myAppPdf.createNewFile()).isTrue();
            assertFileAccess_readWrite(myAppPdf);

            // We can read the other app's image file because we hold R_E_S, but we can
            // check only exists for the pdf files.
            assertFileAccess_readOnly(otherAppImage);
            assertFileAccess_existsOnly(otherAppPdf);
            assertAccess(doesntExistPdf, false, false, false);

            // We can check only exists for another app's files on root.
            // Use shell to create root file because TEST_APP_A is in
            // scoped storage.
            executeShellCommand("touch " + shellPdfAtRoot.getAbsolutePath());
            MediaStore.scanFile(getContentResolver(), shellPdfAtRoot);
            assertFileAccess_existsOnly(shellPdfAtRoot);
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, otherAppPdf.getAbsolutePath());
            deleteFileAsNoThrow(TEST_APP_A, otherAppImage.getAbsolutePath());
            executeShellCommand("rm " + shellPdfAtRoot.getAbsolutePath());
            MediaStore.scanFile(getContentResolver(), shellPdfAtRoot);
            myAppPdf.delete();
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testAccess_directory() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        pollForPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE, /*granted*/ true);
        File topLevelDir = new File(getExternalStorageDir(), "Test");
        try {
            installApp(TEST_APP_A);

            // Let app A create a file in its data dir
            final File otherAppExternalDataDir = new File(getExternalFilesDir().getPath().replace(
                    THIS_PACKAGE_NAME, TEST_APP_A.getPackageName()));
            final File otherAppExternalDataSubDir = new File(otherAppExternalDataDir, "subdir");
            final File otherAppExternalDataFile = new File(otherAppExternalDataSubDir, "abc.jpg");
            assertThat(createFileAs(TEST_APP_A, otherAppExternalDataFile.getAbsolutePath()))
                    .isTrue();

            // We cannot read or write the file, but app A can.
            assertThat(canReadAndWriteAs(TEST_APP_A,
                    otherAppExternalDataFile.getAbsolutePath())).isTrue();
            assertCannotReadOrWrite(otherAppExternalDataFile);

            // We cannot read or write the dir, but app A can.
            assertThat(canReadAndWriteAs(TEST_APP_A,
                    otherAppExternalDataDir.getAbsolutePath())).isTrue();
            assertCannotReadOrWrite(otherAppExternalDataDir);

            // We cannot read or write the sub dir, but app A can.
            assertThat(canReadAndWriteAs(TEST_APP_A,
                    otherAppExternalDataSubDir.getAbsolutePath())).isTrue();
            assertCannotReadOrWrite(otherAppExternalDataSubDir);

            // We can read and write our own app dir, but app A cannot.
            assertThat(canReadAndWriteAs(TEST_APP_A,
                    getExternalFilesDir().getAbsolutePath())).isFalse();
            assertCanAccessMyAppFile(getExternalFilesDir());

            assertDirectoryAccess(getDcimDir(), /* exists */ true, /* canWrite */ true);
            assertDirectoryAccess(getExternalStorageDir(),true, false);
            assertDirectoryAccess(new File(getExternalStorageDir(), "Android"), true, false);
            assertDirectoryAccess(new File(getExternalStorageDir(), "doesnt/exist"), false, false);

            executeShellCommand("mkdir " + topLevelDir.getAbsolutePath());
            assertDirectoryAccess(topLevelDir, true, false);

            assertCannotReadOrWrite(new File("/storage/emulated"));
        } finally {
            uninstallApp(TEST_APP_A); // Uninstalling deletes external app dirs
            executeShellCommand("rmdir " + topLevelDir.getAbsolutePath());
        }
    }

    @Test
    public void testManageExternalStorageCanRenameOtherAppsContents() throws Exception {
        pollForManageExternalStorageAllowed();

        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File pdf = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        final File pdfInObviouslyWrongPlace = new File(getPicturesDir(), NONMEDIA_FILE_NAME);
        final File topLevelPdf = new File(getExternalStorageDir(), NONMEDIA_FILE_NAME);
        final File musicFile = new File(getMusicDir(), AUDIO_FILE_NAME);
        try {
            installApp(TEST_APP_A);

            // Have another app create a PDF
            assertThat(createFileAs(TEST_APP_A, otherAppPdf.getPath())).isTrue();
            assertThat(otherAppPdf.exists()).isTrue();


            // Write some data to the file
            try (final FileOutputStream fos = new FileOutputStream(otherAppPdf)) {
                fos.write(BYTES_DATA1);
            }
            assertFileContent(otherAppPdf, BYTES_DATA1);

            // Assert we can rename the file and ensure the file has the same content
            assertCanRenameFile(otherAppPdf, pdf, /* checkDatabase */ false);
            assertFileContent(pdf, BYTES_DATA1);
            // We can even move it to the top level directory
            assertCanRenameFile(pdf, topLevelPdf, /* checkDatabase */ false);
            assertFileContent(topLevelPdf, BYTES_DATA1);
            // And even rename to a place where PDFs don't belong, because we're an omnipotent
            // external storage manager
            assertCanRenameFile(topLevelPdf, pdfInObviouslyWrongPlace, /* checkDatabase */ false);
            assertFileContent(pdfInObviouslyWrongPlace, BYTES_DATA1);

            // And we can even convert it into a music file, because why not?
            assertCanRenameFile(pdfInObviouslyWrongPlace, musicFile, /* checkDatabase */ false);
            assertFileContent(musicFile, BYTES_DATA1);
        } finally {
            pdf.delete();
            pdfInObviouslyWrongPlace.delete();
            topLevelPdf.delete();
            musicFile.delete();
            deleteFileAsNoThrow(TEST_APP_A, otherAppPdf.getAbsolutePath());
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testCanCreateDefaultDirectory() throws Exception {
        final File podcastsDir = getPodcastsDir();
        try {
            if (podcastsDir.exists()) {
                // Apps can't delete top level directories, not even default directories, so we let
                // shell do the deed for us.
                executeShellCommand("rm -r " + podcastsDir);
            }
            assertThat(podcastsDir.mkdir()).isTrue();
        } finally {
            executeShellCommand("mkdir " + podcastsDir);
        }
    }

    @Test
    public void testManageExternalStorageReaddir() throws Exception {
        pollForManageExternalStorageAllowed();

        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImg = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        final File otherTopLevelFile = new File(getExternalStorageDir(),
                "other" + NONMEDIA_FILE_NAME);
        try {
            installApp(TEST_APP_A);
            assertCreateFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf);
            executeShellCommand("touch " + otherTopLevelFile);
            MediaStore.scanFile(getContentResolver(), otherTopLevelFile);

            // We can list other apps' files
            assertDirectoryContains(otherAppPdf.getParentFile(), otherAppPdf);
            assertDirectoryContains(otherAppImg.getParentFile(), otherAppImg);
            assertDirectoryContains(otherAppMusic.getParentFile(), otherAppMusic);
            // We can list top level files
            assertDirectoryContains(getExternalStorageDir(), otherTopLevelFile);

            // We can also list all top level directories
            assertDirectoryContains(getExternalStorageDir(), getDefaultTopLevelDirs());
        } finally {
            executeShellCommand("rm " + otherTopLevelFile);
            MediaStore.scanFile(getContentResolver(), otherTopLevelFile);
            deleteFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf);
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testManageExternalStorageQueryOtherAppsFile() throws Exception {
        pollForManageExternalStorageAllowed();

        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImg = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        final File otherHiddenFile = new File(getPicturesDir(), ".otherHiddenFile.jpg");
        try {
            installApp(TEST_APP_A);
            // Apps can't query other app's pending file, hence create file and publish it.
            assertCreatePublishedFilesAs(
                    TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);

            assertCanQueryAndOpenFile(otherAppPdf, "rw");
            assertCanQueryAndOpenFile(otherAppImg, "rw");
            assertCanQueryAndOpenFile(otherAppMusic, "rw");
            assertCanQueryAndOpenFile(otherHiddenFile, "rw");
        } finally {
            deleteFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testQueryOtherAppsFiles() throws Exception {
        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImg = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        final File otherHiddenFile = new File(getPicturesDir(), ".otherHiddenFile.jpg");
        try {
            installApp(TEST_APP_A);
            // Apps can't query other app's pending file, hence create file and publish it.
            assertCreatePublishedFilesAs(
                    TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);

            // Since the test doesn't have READ_EXTERNAL_STORAGE nor any other special permissions,
            // it can't query for another app's contents.
            assertCantQueryFile(otherAppImg);
            assertCantQueryFile(otherAppMusic);
            assertCantQueryFile(otherAppPdf);
            assertCantQueryFile(otherHiddenFile);
        } finally {
            deleteFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testSystemGalleryQueryOtherAppsFiles() throws Exception {
        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImg = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        final File otherHiddenFile = new File(getPicturesDir(), ".otherHiddenFile.jpg");
        try {
            installApp(TEST_APP_A);
            // Apps can't query other app's pending file, hence create file and publish it.
            assertCreatePublishedFilesAs(
                    TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);

            // System gallery apps have access to video and image files
            allowAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);

            assertCanQueryAndOpenFile(otherAppImg, "rw");
            // System gallery doesn't have access to hidden image files of other app
            assertCantQueryFile(otherHiddenFile);
            // But no access to PDFs or music files
            assertCantQueryFile(otherAppMusic);
            assertCantQueryFile(otherAppPdf);
        } finally {
            denyAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);
            deleteFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);
            uninstallApp(TEST_APP_A);
        }
    }

    /**
     * Test that System Gallery app can rename any directory under the default directories
     * designated for images and videos, even if they contain other apps' contents that
     * System Gallery doesn't have read access to.
     */
    @Test
    public void testSystemGalleryCanRenameImageAndVideoDirs() throws Exception {
        final File dirInDcim = new File(getDcimDir(), TEST_DIRECTORY_NAME);
        final File dirInPictures = new File(getPicturesDir(), TEST_DIRECTORY_NAME);
        final File dirInPodcasts = new File(getPodcastsDir(), TEST_DIRECTORY_NAME);
        final File otherAppImageFile1 = new File(dirInDcim, "other_" + IMAGE_FILE_NAME);
        final File otherAppVideoFile1 = new File(dirInDcim, "other_" + VIDEO_FILE_NAME);
        final File otherAppPdfFile1 = new File(dirInDcim, "other_" + NONMEDIA_FILE_NAME);
        final File otherAppImageFile2 = new File(dirInPictures, "other_" + IMAGE_FILE_NAME);
        final File otherAppVideoFile2 = new File(dirInPictures, "other_" + VIDEO_FILE_NAME);
        final File otherAppPdfFile2 = new File(dirInPictures, "other_" + NONMEDIA_FILE_NAME);
        try {
            assertThat(dirInDcim.exists() || dirInDcim.mkdir()).isTrue();

            executeShellCommand("touch " + otherAppPdfFile1);
            MediaStore.scanFile(getContentResolver(), otherAppPdfFile1);

            installAppWithStoragePermissions(TEST_APP_A);
            allowAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);

            assertCreateFilesAs(TEST_APP_A, otherAppImageFile1, otherAppVideoFile1);

            // System gallery privileges don't go beyond DCIM, Movies and Pictures boundaries.
            assertCantRenameDirectory(dirInDcim, dirInPodcasts, /*oldFilesList*/ null);

            // Rename should succeed, but System Gallery still can't access that PDF file!
            assertCanRenameDirectory(dirInDcim, dirInPictures,
                    new File[] {otherAppImageFile1, otherAppVideoFile1},
                    new File[] {otherAppImageFile2, otherAppVideoFile2});
            assertThat(getFileRowIdFromDatabase(otherAppPdfFile1)).isEqualTo(-1);
            assertThat(getFileRowIdFromDatabase(otherAppPdfFile2)).isEqualTo(-1);
        } finally {
            executeShellCommand("rm " + otherAppPdfFile1);
            executeShellCommand("rm " + otherAppPdfFile2);
            MediaStore.scanFile(getContentResolver(), otherAppPdfFile1);
            MediaStore.scanFile(getContentResolver(), otherAppPdfFile2);
            otherAppImageFile1.delete();
            otherAppImageFile2.delete();
            otherAppVideoFile1.delete();
            otherAppVideoFile2.delete();
            otherAppPdfFile1.delete();
            otherAppPdfFile2.delete();
            dirInDcim.delete();
            dirInPictures.delete();
            uninstallAppNoThrow(TEST_APP_A);
            denyAppOpsToUid(Process.myUid(), SYSTEM_GALERY_APPOPS);
        }
    }

    /**
     * Test that row ID corresponding to deleted path is restored on subsequent create.
     */
    @Test
    public void testCreateCanRestoreDeletedRowId() throws Exception {
        final File imageFile = new File(getDcimDir(), IMAGE_FILE_NAME);
        final ContentResolver cr = getContentResolver();

        try {
            assertThat(imageFile.createNewFile()).isTrue();
            final long oldRowId = getFileRowIdFromDatabase(imageFile);
            assertThat(oldRowId).isNotEqualTo(-1);
            final Uri uriOfOldFile = MediaStore.scanFile(cr, imageFile);
            assertThat(uriOfOldFile).isNotNull();

            assertThat(imageFile.delete()).isTrue();
            // We should restore old row Id corresponding to deleted imageFile.
            assertThat(imageFile.createNewFile()).isTrue();
            assertThat(getFileRowIdFromDatabase(imageFile)).isEqualTo(oldRowId);
            assertThat(cr.openFileDescriptor(uriOfOldFile, "rw")).isNotNull();

            assertThat(imageFile.delete()).isTrue();
            installApp(TEST_APP_A);
            assertThat(createFileAs(TEST_APP_A, imageFile.getAbsolutePath())).isTrue();

            final Uri uriOfNewFile = MediaStore.scanFile(getContentResolver(), imageFile);
            assertThat(uriOfNewFile).isNotNull();
            // We shouldn't restore deleted row Id if delete & create are called from different apps
            assertThat(Integer.getInteger(uriOfNewFile.getLastPathSegment())).isNotEqualTo(oldRowId);
        } finally {
            imageFile.delete();
            deleteFileAsNoThrow(TEST_APP_A, imageFile.getAbsolutePath());
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that row ID corresponding to deleted path is restored on subsequent rename.
     */
    @Test
    public void testRenameCanRestoreDeletedRowId() throws Exception {
        final File imageFile = new File(getDcimDir(), IMAGE_FILE_NAME);
        final File temporaryFile = new File(getDownloadDir(), IMAGE_FILE_NAME + "_.tmp");
        final ContentResolver cr = getContentResolver();

        try {
            assertThat(imageFile.createNewFile()).isTrue();
            final Uri oldUri = MediaStore.scanFile(cr, imageFile);
            assertThat(oldUri).isNotNull();

            Files.copy(imageFile, temporaryFile);
            assertThat(imageFile.delete()).isTrue();
            assertCanRenameFile(temporaryFile, imageFile);

            final Uri newUri = MediaStore.scanFile(cr, imageFile);
            assertThat(newUri).isNotNull();
            assertThat(newUri.getLastPathSegment()).isEqualTo(oldUri.getLastPathSegment());
            // oldUri of imageFile is still accessible after delete and rename.
            assertThat(cr.openFileDescriptor(oldUri, "rw")).isNotNull();
        } finally {
            imageFile.delete();
            temporaryFile.delete();
        }
    }

    @Test
    public void testCantCreateOrRenameFileWithInvalidName() throws Exception {
        File invalidFile = new File(getDownloadDir(), "<>");
        File validFile = new File(getDownloadDir(), NONMEDIA_FILE_NAME);
        try {
            assertThrows(IOException.class, "Operation not permitted",
                    () -> { invalidFile.createNewFile(); });

            assertThat(validFile.createNewFile()).isTrue();
            // We can't rename a file to a file name with invalid FAT characters.
            assertCantRenameFile(validFile, invalidFile);
        } finally {
            invalidFile.delete();
            validFile.delete();
        }
    }

    @Test
    public void testAndroidMedia() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);

        try {
            installApp(TEST_APP_A);

            final File myMediaDir = getExternalMediaDir();
            final File otherAppMediaDir = new File(myMediaDir.getAbsolutePath().
                    replace(THIS_PACKAGE_NAME, TEST_APP_A.getPackageName()));

            // Verify that accessing other app's /sdcard/Android/media behaves exactly like DCIM for
            // image files and exactly like Downloads for documents.
            assertSharedStorageAccess(otherAppMediaDir, otherAppMediaDir, TEST_APP_A);
            assertSharedStorageAccess(getDcimDir(), getDownloadDir(), TEST_APP_A);

        } finally {
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testWallpaperApisNoPermission() throws Exception {
        WallpaperManager wallpaperManager = WallpaperManager.getInstance(getContext());
        assertThrows(SecurityException.class, () -> wallpaperManager.getFastDrawable());
        assertThrows(SecurityException.class, () -> wallpaperManager.peekFastDrawable());
        assertThrows(SecurityException.class,
                () -> wallpaperManager.getWallpaperFile(WallpaperManager.FLAG_SYSTEM));
    }

    @Test
    public void testWallpaperApisReadExternalStorage() throws Exception {
        pollForPermission(Manifest.permission.READ_EXTERNAL_STORAGE, /*granted*/ true);
        WallpaperManager wallpaperManager = WallpaperManager.getInstance(getContext());
        wallpaperManager.getFastDrawable();
        wallpaperManager.peekFastDrawable();
        wallpaperManager.getWallpaperFile(WallpaperManager.FLAG_SYSTEM);
    }

    @Test
    public void testWallpaperApisManageExternalStorageAppOp() throws Exception {
        pollForManageExternalStorageAllowed();

        WallpaperManager wallpaperManager = WallpaperManager.getInstance(getContext());
        wallpaperManager.getFastDrawable();
        wallpaperManager.peekFastDrawable();
        wallpaperManager.getWallpaperFile(WallpaperManager.FLAG_SYSTEM);
    }

    @Test
    public void testWallpaperApisManageExternalStoragePrivileged() throws Exception {
        adoptShellPermissionIdentity(Manifest.permission.MANAGE_EXTERNAL_STORAGE);
        try {
            WallpaperManager wallpaperManager = WallpaperManager.getInstance(getContext());
            wallpaperManager.getFastDrawable();
            wallpaperManager.peekFastDrawable();
            wallpaperManager.getWallpaperFile(WallpaperManager.FLAG_SYSTEM);
        } finally {
            dropShellPermissionIdentity();
        }
    }

    /**
     * Verifies that files created by {@code otherApp} in shared locations {@code imageDir}
     * and {@code documentDir} follow the scoped storage rules. Requires the running app to hold
     * {@code READ_EXTERNAL_STORAGE}.
     */
    private void assertSharedStorageAccess(File imageDir, File documentDir, TestApp otherApp)
            throws Exception {
        final File otherAppImage = new File(imageDir, "abc.jpg");
        final File otherAppBinary = new File(documentDir, "abc.bin");
        try {
            assertCreateFilesAs(otherApp, otherAppImage, otherAppBinary);

            // We can read the other app's image
            assertFileAccess_readOnly(otherAppImage);
            assertFileContent(otherAppImage, new String().getBytes());

            // .. but not the binary file
            assertFileAccess_existsOnly(otherAppBinary);
            assertThrows(FileNotFoundException.class, () -> {
                assertFileContent(otherAppBinary, new String().getBytes()); });
        } finally {
            deleteFileAsNoThrow(otherApp, otherAppImage.getAbsolutePath());
            deleteFileAsNoThrow(otherApp, otherAppBinary.getAbsolutePath());
        }
    }

    /**
     * Test that IS_PENDING is set for files created via filepath
     */
    @Test
    public void testPendingFromFuse() throws Exception {
        final File pendingFile = new File(getDcimDir(), IMAGE_FILE_NAME);
        final File otherPendingFile = new File(getDcimDir(), VIDEO_FILE_NAME);
        try {
            assertTrue(pendingFile.createNewFile());
            // Newly created file should have IS_PENDING set
            try (Cursor c = queryFile(pendingFile, MediaStore.MediaColumns.IS_PENDING)) {
                assertTrue(c.moveToFirst());
                assertThat(c.getInt(0)).isEqualTo(1);
            }

            // If we query with MATCH_EXCLUDE, we should still see this pendingFile
            try (Cursor c = queryFileExcludingPending(pendingFile, MediaColumns.IS_PENDING)) {
                assertThat(c.getCount()).isEqualTo(1);
                assertTrue(c.moveToFirst());
                assertThat(c.getInt(0)).isEqualTo(1);
            }

            assertNotNull(MediaStore.scanFile(getContentResolver(), pendingFile));

            // IS_PENDING should be unset after the scan
            try (Cursor c = queryFile(pendingFile, MediaStore.MediaColumns.IS_PENDING)) {
                assertTrue(c.moveToFirst());
                assertThat(c.getInt(0)).isEqualTo(0);
            }

            installAppWithStoragePermissions(TEST_APP_A);
            assertCreateFilesAs(TEST_APP_A, otherPendingFile);
            // We can't query other apps pending file from FUSE with MATCH_EXCLUDE
            try (Cursor c = queryFileExcludingPending(otherPendingFile, MediaColumns.IS_PENDING)) {
                assertThat(c.getCount()).isEqualTo(0);
            }
        } finally {
            pendingFile.delete();
            deleteFileAsNoThrow(TEST_APP_A, otherPendingFile.getAbsolutePath());
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testOpenOtherPendingFilesFromFuse() throws Exception {
        final File otherPendingFile = new File(getDcimDir(), IMAGE_FILE_NAME);
        try {
            installApp(TEST_APP_A);
            assertCreateFilesAs(TEST_APP_A, otherPendingFile);

            // We can read other app's pending file from FUSE via filePath
            assertCanQueryAndOpenFile(otherPendingFile, "r");

            // We can also read other app's pending file via MediaStore API
            assertNotNull(openWithMediaProvider(otherPendingFile, "r"));
        } finally {
            deleteFileAsNoThrow(TEST_APP_A, otherPendingFile.getAbsolutePath());
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    /**
     * Test that apps can't set attributes on another app's files.
     */
    @Test
    public void testCantSetAttrOtherAppsFile() throws Exception {
        // This path's permission is checked in MediaProvider (directory/external media dir)
        final File externalMediaPath = new File(getExternalMediaDir(), VIDEO_FILE_NAME);

        try {
            // Create the files
            if (!externalMediaPath.exists()) {
                assertThat(externalMediaPath.createNewFile()).isTrue();
            }

            // Install TEST_APP_A with READ_EXTERNAL_STORAGE permission.
            installAppWithStoragePermissions(TEST_APP_A);

            // TEST_APP_A should not be able to setattr to other app's files.
            assertWithMessage(
                "setattr on directory/external media path [%s]", externalMediaPath.getPath())
                .that(setAttrAs(TEST_APP_A, externalMediaPath.getPath()))
                .isFalse();
        } finally {
            externalMediaPath.delete();
            uninstallAppNoThrow(TEST_APP_A);
        }
    }

    @Test
    public void testNoIsolatedStorageCanCreateFilesAnywhere() throws Exception {
        final File topLevelPdf = new File(getExternalStorageDir(), NONMEDIA_FILE_NAME);
        final File musicFileInMovies = new File(getMoviesDir(), AUDIO_FILE_NAME);
        final File imageFileInDcim = new File(getDcimDir(), IMAGE_FILE_NAME);
        // Nothing special about this, anyone can create an image file in DCIM
        assertCanCreateFile(imageFileInDcim);
        // This is where we see the special powers of MANAGE_EXTERNAL_STORAGE, because it can
        // create a top level file
        assertCanCreateFile(topLevelPdf);
        // It can even create a music file in Pictures
        assertCanCreateFile(musicFileInMovies);
    }

    @Test
    public void testNoIsolatedStorageCantReadWriteOtherAppExternalDir() throws Exception {
        try {
            // Install TEST_APP_A with READ_EXTERNAL_STORAGE permission.
            installAppWithStoragePermissions(TEST_APP_A);

            // Let app A create a file in its data dir
            final File otherAppExternalDataDir = new File(getExternalFilesDir().getPath().replace(
                    THIS_PACKAGE_NAME, TEST_APP_A.getPackageName()));
            final File otherAppExternalDataFile = new File(otherAppExternalDataDir,
                    NONMEDIA_FILE_NAME);
            assertCreateFilesAs(TEST_APP_A, otherAppExternalDataFile);

            // File Manager app gets global access with MANAGE_EXTERNAL_STORAGE permission, however,
            // file manager app doesn't have access to other app's external files directory
            assertThat(canOpen(otherAppExternalDataFile, /* forWrite */ false)).isFalse();
            assertThat(canOpen(otherAppExternalDataFile, /* forWrite */ true)).isFalse();
            assertThat(otherAppExternalDataFile.delete()).isFalse();

            assertThat(deleteFileAs(TEST_APP_A, otherAppExternalDataFile.getPath())).isTrue();

            assertThrows(IOException.class,
                    () -> { otherAppExternalDataFile.createNewFile(); });

        } finally {
            uninstallApp(TEST_APP_A); // Uninstalling deletes external app dirs
        }
    }

    @Test
    public void testNoIsolatedStorageStorageReaddir() throws Exception {
        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImg = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        final File otherTopLevelFile = new File(getExternalStorageDir(),
                "other" + NONMEDIA_FILE_NAME);
        try {
            installApp(TEST_APP_A);
            assertCreateFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf);
            executeShellCommand("touch " + otherTopLevelFile);

            // We can list other apps' files
            assertDirectoryContains(otherAppPdf.getParentFile(), otherAppPdf);
            assertDirectoryContains(otherAppImg.getParentFile(), otherAppImg);
            assertDirectoryContains(otherAppMusic.getParentFile(), otherAppMusic);
            // We can list top level files
            assertDirectoryContains(getExternalStorageDir(), otherTopLevelFile);

            // We can also list all top level directories
            assertDirectoryContains(getExternalStorageDir(), getDefaultTopLevelDirs());
        } finally {
            executeShellCommand("rm " + otherTopLevelFile);
            deleteFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf);
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testNoIsolatedStorageQueryOtherAppsFile() throws Exception {
        final File otherAppPdf = new File(getDownloadDir(), "other" + NONMEDIA_FILE_NAME);
        final File otherAppImg = new File(getDcimDir(), "other" + IMAGE_FILE_NAME);
        final File otherAppMusic = new File(getMusicDir(), "other" + AUDIO_FILE_NAME);
        final File otherHiddenFile = new File(getPicturesDir(), ".otherHiddenFile.jpg");
        try {
            installApp(TEST_APP_A);
            // Apps can't query other app's pending file, hence create file and publish it.
            assertCreatePublishedFilesAs(
                    TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);

            assertCanQueryAndOpenFile(otherAppPdf, "rw");
            assertCanQueryAndOpenFile(otherAppImg, "rw");
            assertCanQueryAndOpenFile(otherAppMusic, "rw");
            assertCanQueryAndOpenFile(otherHiddenFile, "rw");
        } finally {
            deleteFilesAs(TEST_APP_A, otherAppImg, otherAppMusic, otherAppPdf, otherHiddenFile);
            uninstallApp(TEST_APP_A);
        }
    }

    @Test
    public void testRenameFromShell() throws Exception {
        final File imageFile = new File(getPicturesDir(), IMAGE_FILE_NAME);
        final File dir = new File(getMoviesDir(), TEST_DIRECTORY_NAME);
        final File renamedDir = new File(getMusicDir(), TEST_DIRECTORY_NAME);
        final File renamedImageFile = new File(dir, IMAGE_FILE_NAME);
        final File imageFileInRenamedDir = new File(renamedDir, IMAGE_FILE_NAME);
        try {
            assertTrue(imageFile.createNewFile());
            assertThat(getFileRowIdFromDatabase(imageFile)).isNotEqualTo(-1);
            if (!dir.exists()) {
                assertThat(dir.mkdir()).isTrue();
            }

            final String renameFileCommand = String.format("mv %s %s",
                    imageFile.getAbsolutePath(), renamedImageFile.getAbsolutePath());
            executeShellCommand(renameFileCommand);
            assertFalse(imageFile.exists());
            assertThat(getFileRowIdFromDatabase(imageFile)).isEqualTo(-1);
            assertTrue(renamedImageFile.exists());
            assertThat(getFileRowIdFromDatabase(renamedImageFile)).isNotEqualTo(-1);

            final String renameDirectoryCommand = String.format("mv %s %s",
                    dir.getAbsolutePath(), renamedDir.getAbsolutePath());
            executeShellCommand(renameDirectoryCommand);
            assertFalse(dir.exists());
            assertFalse(renamedImageFile.exists());
            assertThat(getFileRowIdFromDatabase(renamedImageFile)).isEqualTo(-1);
            assertTrue(renamedDir.exists());
            assertTrue(imageFileInRenamedDir.exists());
            assertThat(getFileRowIdFromDatabase(imageFileInRenamedDir)).isNotEqualTo(-1);
        } finally {
            imageFile.delete();
            renamedImageFile.delete();
            imageFileInRenamedDir.delete();
            dir.delete();
            renamedDir.delete();
        }
    }

    /**
     * Checks restrictions for opening pending and trashed files by different apps. Assumes that
     * given {@code testApp} is already installed and has READ_EXTERNAL_STORAGE permission. This
     * method doesn't uninstall given {@code testApp} at the end.
     */
    private void assertOpenPendingOrTrashed(Uri uri, TestApp testApp, boolean isImageOrVideo)
            throws Exception {
        final File pendingOrTrashedFile = new File(getFilePathFromUri(uri));

        // App can open its pending or trashed file for read or write
        assertTrue(canOpen(pendingOrTrashedFile, /*forWrite*/ false));
        assertTrue(canOpen(pendingOrTrashedFile, /*forWrite*/ true));

        // App with READ_EXTERNAL_STORAGE can't open other app's pending or trashed file for read or
        // write
        assertFalse(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ false));
        assertFalse(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ true));

        final int testAppUid =
                getContext().getPackageManager().getPackageUid(testApp.getPackageName(), 0);
        try {
            allowAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
            // File Manager can open any pending or trashed file for read or write
            assertTrue(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ false));
            assertTrue(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ true));
        } finally {
            denyAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
        }

        try {
            allowAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
            if (isImageOrVideo) {
                // System Gallery can open any pending or trashed image/video file for read or write
                assertTrue(isMediaTypeImageOrVideo(pendingOrTrashedFile));
                assertTrue(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ false));
                assertTrue(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ true));
            } else {
                // System Gallery can't open other app's pending or trashed non-media file for read
                // or write
                assertFalse(isMediaTypeImageOrVideo(pendingOrTrashedFile));
                assertFalse(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ false));
                assertFalse(openFileAs(testApp, pendingOrTrashedFile, /*forWrite*/ true));
            }
        } finally {
            denyAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
        }
    }

    /**
     * Checks restrictions for listing pending and trashed files by different apps. Assumes that
     * given {@code testApp} is already installed and has READ_EXTERNAL_STORAGE permission. This
     * method doesn't uninstall given {@code testApp} at the end.
     */
    private void assertListPendingOrTrashed(Uri uri, File file, TestApp testApp,
            boolean isImageOrVideo) throws Exception {
        final String parentDirPath = file.getParent();
        assertTrue(new File(parentDirPath).isDirectory());

        final List<String> listedFileNames = Arrays.asList(new File(parentDirPath).list());
        assertThat(listedFileNames).doesNotContain(file);

        final File pendingOrTrashedFile = new File(getFilePathFromUri(uri));

        assertThat(listedFileNames).contains(pendingOrTrashedFile.getName());

        // App with READ_EXTERNAL_STORAGE can't see other app's pending or trashed file.
        assertThat(listAs(testApp, parentDirPath)).doesNotContain(pendingOrTrashedFile.getName());

        final int testAppUid =
                getContext().getPackageManager().getPackageUid(testApp.getPackageName(), 0);
        try {
            allowAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
            // File Manager can see any pending or trashed file.
            assertThat(listAs(testApp, parentDirPath)).contains(pendingOrTrashedFile.getName());
        } finally {
            denyAppOpsToUid(testAppUid, OPSTR_MANAGE_EXTERNAL_STORAGE);
        }

        try {
            allowAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
            if (isImageOrVideo) {
                // System Gallery can see any pending or trashed image/video file.
                assertTrue(isMediaTypeImageOrVideo(pendingOrTrashedFile));
                assertThat(listAs(testApp, parentDirPath)).contains(pendingOrTrashedFile.getName());
            } else {
                // System Gallery can't see other app's pending or trashed non media file.
                assertFalse(isMediaTypeImageOrVideo(pendingOrTrashedFile));
                assertThat(listAs(testApp, parentDirPath))
                        .doesNotContain(pendingOrTrashedFile.getName());
            }
        } finally {
            denyAppOpsToUid(testAppUid, SYSTEM_GALERY_APPOPS);
        }
    }

    private Uri createPendingFile(File pendingFile) throws Exception {
        assertTrue(pendingFile.createNewFile());

        final ContentResolver cr = getContentResolver();
        final Uri trashedFileUri = MediaStore.scanFile(cr, pendingFile);
        assertNotNull(trashedFileUri);

        final ContentValues values = new ContentValues();
        values.put(MediaColumns.IS_PENDING, 1);
        assertEquals(1, cr.update(trashedFileUri, values, Bundle.EMPTY));

        return trashedFileUri;
    }

    private Uri createTrashedFile(File trashedFile) throws Exception {
        assertTrue(trashedFile.createNewFile());

        final ContentResolver cr = getContentResolver();
        final Uri trashedFileUri = MediaStore.scanFile(cr, trashedFile);
        assertNotNull(trashedFileUri);

        trashFile(trashedFileUri);
        return trashedFileUri;
    }

    private void trashFile(Uri uri) throws Exception {
        final ContentValues values = new ContentValues();
        values.put(MediaColumns.IS_TRASHED, 1);
        assertEquals(1, getContentResolver().update(uri, values, Bundle.EMPTY));
    }

    /**
     * Gets file path corresponding to the db row pointed by {@code uri}. If {@code uri} points to
     * multiple db rows, file path is extracted from the first db row of the database query result.
     */
    private String getFilePathFromUri(Uri uri) {
        final String[] projection = new String[] {MediaColumns.DATA};
        try (Cursor c = getContentResolver().query(uri, projection, null, null)) {
            assertTrue(c.moveToFirst());
            return c.getString(0);
        }
    }

    private boolean isMediaTypeImageOrVideo(File file) {
        return queryImageFile(file).getCount() == 1 || queryVideoFile(file).getCount() == 1;
    }

    private static void assertIsMediaTypeImage(File file) {
        final Cursor c = queryImageFile(file);
        assertEquals(1, c.getCount());
    }

    private static void assertNotMediaTypeImage(File file) {
        final Cursor c = queryImageFile(file);
        assertEquals(0, c.getCount());
    }

    private static void assertCantQueryFile(File file) {
        assertThat(getFileUri(file)).isNull();
        // Confirm that file exists in the database.
        assertNotNull(MediaStore.scanFile(getContentResolver(), file));
    }

    private static void assertCreateFilesAs(TestApp testApp, File... files) throws Exception {
        for (File file : files) {
            assertThat(createFileAs(testApp, file.getPath())).isTrue();
        }
    }

    /**
     * Makes {@code testApp} create {@code files}. Publishes {@code files} by scanning the file.
     * Pending files from FUSE are not visible to other apps via MediaStore APIs. We have to publish
     * the file or make the file non-pending to make the file visible to other apps.
     * <p>
     * Note that this method can only be used for scannable files.
     */
    private static void assertCreatePublishedFilesAs(TestApp testApp, File... files)
            throws Exception {
        for (File file : files) {
            assertThat(createFileAs(testApp, file.getPath())).isTrue();
            assertNotNull(MediaStore.scanFile(getContentResolver(), file));
        }
    }


    private static void deleteFilesAs(TestApp testApp, File... files) throws Exception {
        for (File file : files) {
            deleteFileAs(testApp, file.getPath());
        }
    }
    private static void assertCanDeletePathsAs(TestApp testApp, String... filePaths)
            throws Exception {
        for (String path: filePaths) {
            assertTrue(deleteFileAs(testApp, path));
        }
    }

    private static void assertCantDeletePathsAs(TestApp testApp, String... filePaths)
            throws Exception {
        for (String path: filePaths) {
            assertFalse(deleteFileAs(testApp, path));
        }
    }

    private void deleteFiles(File... files) {
        for (File file: files) {
            if (file == null) continue;
            file.delete();
        }
    }

    private void deletePaths(String... paths) {
        for (String path: paths) {
            if (path == null) continue;
            new File(path).delete();
        }
    }

    private static void assertCanDeletePaths(String... filePaths) {
        for (String filePath : filePaths) {
            assertTrue(new File(filePath).delete());
        }
    }

    /**
     * For possible values of {@code mode}, look at {@link android.content.ContentProvider#openFile}
     */
    private static void assertCanQueryAndOpenFile(File file, String mode) throws IOException {
        // This call performs the query
        final Uri fileUri = getFileUri(file);
        // The query succeeds iff it didn't return null
        assertThat(fileUri).isNotNull();
        // Now we assert that we can open the file through ContentResolver
        try (final ParcelFileDescriptor pfd =
                        getContentResolver().openFileDescriptor(fileUri, mode)) {
            assertThat(pfd).isNotNull();
        }
    }

    /**
     * Assert that the last read in: read - write - read using {@code readFd} and {@code writeFd}
     * see the last write. {@code readFd} and {@code writeFd} are fds pointing to the same
     * underlying file on disk but may be derived from different mount points and in that case
     * have separate VFS caches.
     */
    private void assertRWR(ParcelFileDescriptor readPfd, ParcelFileDescriptor writePfd)
            throws Exception {
        FileDescriptor readFd = readPfd.getFileDescriptor();
        FileDescriptor writeFd = writePfd.getFileDescriptor();

        byte[] readBuffer = new byte[10];
        byte[] writeBuffer = new byte[10];
        Arrays.fill(writeBuffer, (byte) 1);

        // Write so readFd has content to read from next
        Os.pwrite(readFd, readBuffer, 0, 10, 0);
        // Read so readBuffer is in readFd's mount VFS cache
        Os.pread(readFd, readBuffer, 0, 10, 0);

        // Assert that readBuffer is zeroes
        assertThat(readBuffer).isEqualTo(new byte[10]);

        // Write so writeFd and readFd should now see writeBuffer
        Os.pwrite(writeFd, writeBuffer, 0, 10, 0);

        // Read so the last write can be verified on readFd
        Os.pread(readFd, readBuffer, 0, 10, 0);

        // Assert that the last write is indeed visible via readFd
        assertThat(readBuffer).isEqualTo(writeBuffer);
        assertThat(readPfd.getStatSize()).isEqualTo(writePfd.getStatSize());
    }

    private void assertLowerFsFd(ParcelFileDescriptor pfd) throws Exception {
        assertThat(Os.readlink("/proc/self/fd/" + pfd.getFd()).startsWith("/storage")).isTrue();
    }

    private void assertUpperFsFd(ParcelFileDescriptor pfd) throws Exception {
        assertThat(Os.readlink("/proc/self/fd/" + pfd.getFd()).startsWith("/mnt/user")).isTrue();
    }

    private static void assertCanCreateFile(File file) throws IOException {
        // If the file somehow managed to survive a previous run, then the test app was uninstalled
        // and MediaProvider will remove our its ownership of the file, so it's not guaranteed that
        // we can create nor delete it.
        if (!file.exists()) {
            assertThat(file.createNewFile()).isTrue();
            assertThat(file.delete()).isTrue();
        } else {
            Log.w(TAG,
                    "Couldn't assertCanCreateFile(" + file + ") because file existed prior to "
                            + "running the test!");
        }
    }

    private static void assertFileAccess_existsOnly(File file) throws Exception {
        assertThat(file.isFile()).isTrue();
        assertAccess(file, true, false, false);
    }

    private static void assertFileAccess_readOnly(File file) throws Exception {
        assertThat(file.isFile()).isTrue();
        assertAccess(file, true, true, false);
    }

    private static void assertFileAccess_readWrite(File file) throws Exception {
        assertThat(file.isFile()).isTrue();
        assertAccess(file, true, true, true);
    }

    private static void assertDirectoryAccess(File dir, boolean exists, boolean canWrite)
            throws Exception {
        // This util does not handle app data directories.
        assumeFalse(dir.getAbsolutePath().startsWith(getAndroidDir().getAbsolutePath())
                && !dir.equals(getAndroidDir()));
        assertThat(dir.isDirectory()).isEqualTo(exists);
        // For non-app data directories, exists => canRead().
        assertAccess(dir, exists, exists, exists && canWrite);
    }

    private static void assertAccess(File file, boolean exists, boolean canRead, boolean canWrite)
            throws Exception {
        assertAccess(file, exists, canRead, canWrite, true /* checkExists */);
    }

    private static void assertCannotReadOrWrite(File file)
            throws Exception {
        // App data directories have different 'x' bits on upgrading vs new devices. Let's not
        // check 'exists', by passing checkExists=false. But assert this app cannot read or write
        // the other app's file.
        assertAccess(file, false /* value is moot */, false /* canRead */,
                false /* canWrite */, false /* checkExists */);
    }

    private static void assertCanAccessMyAppFile(File file)
            throws Exception {
        assertAccess(file, true, true /* canRead */,
                true /*canWrite */, true /* checkExists */);
    }

    private static void assertAccess(File file, boolean exists, boolean canRead, boolean canWrite,
            boolean checkExists) throws Exception {
        if (checkExists) {
            assertThat(file.exists()).isEqualTo(exists);
        }
        assertThat(file.canRead()).isEqualTo(canRead);
        assertThat(file.canWrite()).isEqualTo(canWrite);
        if (file.isDirectory()) {
            if (checkExists) {
                assertThat(file.canExecute()).isEqualTo(exists);
            }
        } else {
            assertThat(file.canExecute()).isFalse(); // Filesytem is mounted with MS_NOEXEC
        }

        // Test some combinations of mask.
        assertAccess(file, R_OK, canRead);
        assertAccess(file, W_OK, canWrite);
        assertAccess(file, R_OK | W_OK, canRead && canWrite);
        assertAccess(file, W_OK | F_OK, canWrite);

        if (checkExists) {
            assertAccess(file, F_OK, exists);
        }
    }

    private static void assertAccess(File file, int mask, boolean expected) throws Exception {
        if (expected) {
            assertThat(Os.access(file.getAbsolutePath(), mask)).isTrue();
        } else {
            assertThrows(ErrnoException.class, () -> { Os.access(file.getAbsolutePath(), mask); });
        }
    }
}
