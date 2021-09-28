/**
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

package android.scopedstorage.cts.lib;

import static android.scopedstorage.cts.lib.RedactionTestHelper.EXIF_METADATA_QUERY;

import static androidx.test.InstrumentationRegistry.getContext;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.Manifest;
import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import com.android.cts.install.lib.Install;
import com.android.cts.install.lib.InstallUtils;
import com.android.cts.install.lib.TestApp;
import com.android.cts.install.lib.Uninstall;

import com.google.common.io.ByteStreams;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/**
 * General helper functions for ScopedStorageTest tests.
 */
public class TestUtils {
    static final String TAG = "ScopedStorageTest";

    public static final String QUERY_TYPE = "android.scopedstorage.cts.queryType";
    public static final String INTENT_EXTRA_PATH = "android.scopedstorage.cts.path";
    public static final String INTENT_EXCEPTION = "android.scopedstorage.cts.exception";
    public static final String CREATE_FILE_QUERY = "android.scopedstorage.cts.createfile";
    public static final String CREATE_IMAGE_ENTRY_QUERY =
            "android.scopedstorage.cts.createimageentry";
    public static final String DELETE_FILE_QUERY = "android.scopedstorage.cts.deletefile";
    public static final String OPEN_FILE_FOR_READ_QUERY =
            "android.scopedstorage.cts.openfile_read";
    public static final String OPEN_FILE_FOR_WRITE_QUERY =
            "android.scopedstorage.cts.openfile_write";
    public static final String CAN_READ_WRITE_QUERY =
            "android.scopedstorage.cts.can_read_and_write";
    public static final String READDIR_QUERY = "android.scopedstorage.cts.readdir";
    public static final String SETATTR_QUERY = "android.scopedstorage.cts.setattr";

    public static final String STR_DATA1 = "Just some random text";
    public static final String STR_DATA2 = "More arbitrary stuff";

    public static final byte[] BYTES_DATA1 = STR_DATA1.getBytes();
    public static final byte[] BYTES_DATA2 = STR_DATA2.getBytes();

    // Root of external storage
    private static File sExternalStorageDirectory = Environment.getExternalStorageDirectory();
    private static String sStorageVolumeName = MediaStore.VOLUME_EXTERNAL;

    private static final long POLLING_TIMEOUT_MILLIS = TimeUnit.SECONDS.toMillis(20);
    private static final long POLLING_SLEEP_MILLIS = 100;

    /**
     * Creates the top level default directories.
     *
     * <p>Those are usually created by MediaProvider, but some naughty tests might delete them
     * and not restore them afterwards, so we make sure we create them before we make any
     * assumptions about their existence.
     */
    public static void setupDefaultDirectories() {
        for (File dir : getDefaultTopLevelDirs()) {
            dir.mkdir();
        }
    }

    /**
     * Grants {@link Manifest.permission#GRANT_RUNTIME_PERMISSIONS} to the given package.
     */
    public static void grantPermission(String packageName, String permission) {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        uiAutomation.adoptShellPermissionIdentity("android.permission.GRANT_RUNTIME_PERMISSIONS");
        try {
            uiAutomation.grantRuntimePermission(packageName, permission);
            // Wait for OP_READ_EXTERNAL_STORAGE to get updated.
            SystemClock.sleep(1000);
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Revokes permissions from the given package.
     */
    public static void revokePermission(String packageName, String permission) {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        uiAutomation.adoptShellPermissionIdentity("android.permission.REVOKE_RUNTIME_PERMISSIONS");
        try {
            uiAutomation.revokeRuntimePermission(packageName, permission);
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Adopts shell permission identity for the given permissions.
     */
    public static void adoptShellPermissionIdentity(String... permissions) {
        InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                permissions);
    }

    /**
     * Drops shell permission identity for all permissions.
     */
    public static void dropShellPermissionIdentity() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /**
     * Executes a shell command.
     */
    public static String executeShellCommand(String command) throws IOException {
        int attempt = 0;
        while (attempt++ < 5) {
            try {
                return executeShellCommandInternal(command);
            } catch (InterruptedIOException e) {
                // Hmm, we had trouble executing the shell command; the best we
                // can do is try again a few more times
                Log.v(TAG, "Trouble executing " + command + "; trying again", e);
            }
        }
        throw new IOException("Failed to execute " + command);
    }

    private static String executeShellCommandInternal(String cmd) throws IOException {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try (FileInputStream output = new FileInputStream(
                     uiAutomation.executeShellCommand(cmd).getFileDescriptor())) {
            return new String(ByteStreams.toByteArray(output));
        }
    }

    /**
     * Makes the given {@code testApp} list the content of the given directory and returns the
     * result as an {@link ArrayList}
     */
    public static ArrayList<String> listAs(TestApp testApp, String dirPath) throws Exception {
        return getContentsFromTestApp(testApp, dirPath, READDIR_QUERY);
    }

    /**
     * Returns {@code true} iff the given {@code path} exists and is readable and
     * writable for for {@code testApp}.
     */
    public static boolean canReadAndWriteAs(TestApp testApp, String path) throws Exception {
        return getResultFromTestApp(testApp, path, CAN_READ_WRITE_QUERY);
    }

    /**
     * Makes the given {@code testApp} read the EXIF metadata from the given file and returns the
     * result as an {@link HashMap}
     */
    public static HashMap<String, String> readExifMetadataFromTestApp(
            TestApp testApp, String filePath) throws Exception {
        HashMap<String, String> res =
                getMetadataFromTestApp(testApp, filePath, EXIF_METADATA_QUERY);
        return res;
    }

    /**
     * Makes the given {@code testApp} create a file.
     *
     * <p>This method drops shell permission identity.
     */
    public static boolean createFileAs(TestApp testApp, String path) throws Exception {
        return getResultFromTestApp(testApp, path, CREATE_FILE_QUERY);
    }

    /**
     * Makes the given {@code testApp} create a mediastore DB entry under
     * {@code MediaStore.Media.Images}.
     *
     * The {@code path} argument is treated as a relative path and a name separated
     * by an {@code '/'}.
     */
    public static boolean createImageEntryAs(TestApp testApp, String path) throws Exception {
        return getResultFromTestApp(testApp, path, CREATE_IMAGE_ENTRY_QUERY);
    }

    /**
     * Makes the given {@code testApp} delete a file.
     *
     * <p>This method drops shell permission identity.
     */
    public static boolean deleteFileAs(TestApp testApp, String path) throws Exception {
        return getResultFromTestApp(testApp, path, DELETE_FILE_QUERY);
    }

    /**
     * Makes the given {@code testApp} delete a file. Doesn't throw in case of failure.
     */
    public static boolean deleteFileAsNoThrow(TestApp testApp, String path) {
        try {
            return deleteFileAs(testApp, path);
        } catch (Exception e) {
            Log.e(TAG,
                    "Error occurred while deleting file: " + path + " on behalf of app: " + testApp,
                    e);
            return false;
        }
    }

    /**
     * Makes the given {@code testApp} open {@code file} for read or write.
     *
     * <p>This method drops shell permission identity.
     */
    public static boolean openFileAs(TestApp testApp, File file, boolean forWrite)
            throws Exception {
        return openFileAs(testApp, file.getAbsolutePath(), forWrite);
    }

    /**
     * Makes the given {@code testApp} open a file for read or write.
     *
     * <p>This method drops shell permission identity.
     */
    public static boolean openFileAs(TestApp testApp, String path, boolean forWrite)
            throws Exception {
        return getResultFromTestApp(
                testApp, path, forWrite ? OPEN_FILE_FOR_WRITE_QUERY : OPEN_FILE_FOR_READ_QUERY);
    }

    /**
     * Makes the given {@code testApp} setattr for given file path.
     *
     * <p>This method drops shell permission identity.
     */
    public static boolean setAttrAs(TestApp testApp, String path)
            throws Exception {
        return getResultFromTestApp(testApp, path, SETATTR_QUERY);
    }

    /**
     * Installs a {@link TestApp} without storage permissions.
     */
    public static void installApp(TestApp testApp) throws Exception {
        installApp(testApp, /* grantStoragePermission */ false);
    }

    /**
     * Installs a {@link TestApp} with storage permissions.
     */
    public static void installAppWithStoragePermissions(TestApp testApp) throws Exception {
        installApp(testApp, /* grantStoragePermission */ true);
    }

    /**
     * Installs a {@link TestApp} and may grant it storage permissions.
     */
    public static void installApp(TestApp testApp, boolean grantStoragePermission)
            throws Exception {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            final String packageName = testApp.getPackageName();
            uiAutomation.adoptShellPermissionIdentity(
                    Manifest.permission.INSTALL_PACKAGES, Manifest.permission.DELETE_PACKAGES);
            if (InstallUtils.getInstalledVersion(packageName) != -1) {
                Uninstall.packages(packageName);
            }
            Install.single(testApp).commit();
            assertThat(InstallUtils.getInstalledVersion(packageName)).isEqualTo(1);
            if (grantStoragePermission) {
                grantPermission(packageName, Manifest.permission.READ_EXTERNAL_STORAGE);
            }
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Uninstalls a {@link TestApp}.
     */
    public static void uninstallApp(TestApp testApp) throws Exception {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            final String packageName = testApp.getPackageName();
            uiAutomation.adoptShellPermissionIdentity(Manifest.permission.DELETE_PACKAGES);

            Uninstall.packages(packageName);
            assertThat(InstallUtils.getInstalledVersion(packageName)).isEqualTo(-1);
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * Uninstalls a {@link TestApp}. Doesn't throw in case of failure.
     */
    public static void uninstallAppNoThrow(TestApp testApp) {
        try {
            uninstallApp(testApp);
        } catch (Exception e) {
            Log.e(TAG, "Exception occurred while uninstalling app: " + testApp, e);
        }
    }

    public static ContentResolver getContentResolver() {
        return getContext().getContentResolver();
    }

    /**
     * Inserts a file into the database using {@link MediaStore.MediaColumns#DATA}.
     */
    public static Uri insertFileUsingDataColumn(@NonNull File file) {
        final ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DATA, file.getPath());
        return getContentResolver().insert(MediaStore.Files.getContentUri(sStorageVolumeName),
                values);
    }

    /**
     * Returns the content URI for images based on the current storage volume.
     */
    public static Uri getImageContentUri() {
        return MediaStore.Images.Media.getContentUri(sStorageVolumeName);
    }

    /**
     * Renames the given file using {@link ContentResolver} and {@link MediaStore} and APIs.
     * This method uses the data column, and not all apps can use it.
     * @see MediaStore.MediaColumns#DATA
     */
    public static int renameWithMediaProvider(@NonNull File oldPath, @NonNull File newPath) {
        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DATA, newPath.getPath());
        return getContentResolver().update(MediaStore.Files.getContentUri(sStorageVolumeName),
                values, /*where*/ MediaStore.MediaColumns.DATA + "=?",
                /*whereArgs*/ new String[] {oldPath.getPath()});
    }

    /**
     * Queries {@link ContentResolver} for a file and returns the corresponding {@link Uri} for its
     * entry in the database. Returns {@code null} if file doesn't exist in the database.
     */
    @Nullable
    public static Uri getFileUri(@NonNull File file) {
        final Uri contentUri = MediaStore.Files.getContentUri(sStorageVolumeName);
        final int id = getFileRowIdFromDatabase(file);
        return id == -1 ? null : ContentUris.withAppendedId(contentUri, id);
    }

    /**
     * Queries {@link ContentResolver} for a file and returns the corresponding row ID for its
     * entry in the database. Returns {@code -1} if file is not found.
     */
    public static int getFileRowIdFromDatabase(@NonNull File file) {
        int id = -1;
        try (Cursor c = queryFile(file, MediaStore.MediaColumns._ID)) {
            if (c.moveToFirst()) {
                id = c.getInt(0);
            }
        }
        return id;
    }

    /**
     * Queries {@link ContentResolver} for a file and returns the corresponding owner package name
     * for its entry in the database.
     */
    @Nullable
    public static String getFileOwnerPackageFromDatabase(@NonNull File file) {
        String ownerPackage = null;
        try (Cursor c = queryFile(file, MediaStore.MediaColumns.OWNER_PACKAGE_NAME)) {
            if (c.moveToFirst()) {
                ownerPackage = c.getString(0);
            }
        }
        return ownerPackage;
    }

    /**
     * Queries {@link ContentResolver} for a file and returns the corresponding file size for its
     * entry in the database. Returns {@code -1} if file is not found.
     */
    @Nullable
    public static int getFileSizeFromDatabase(@NonNull File file) {
        int size = -1;
        try (Cursor c = queryFile(file, MediaStore.MediaColumns.SIZE)) {
            if (c.moveToFirst()) {
                size = c.getInt(0);
            }
        }
        return size;
    }

    /**
     * Queries {@link ContentResolver} for a video file and returns a {@link Cursor} with the given
     * columns.
     */
    @NonNull
    public static Cursor queryVideoFile(File file, String... projection) {
        return queryFile(MediaStore.Video.Media.getContentUri(sStorageVolumeName), file,
                /*includePending*/ true, projection);
    }

    /**
     * Queries {@link ContentResolver} for an image file and returns a {@link Cursor} with the given
     * columns.
     */
    @NonNull
    public static Cursor queryImageFile(File file, String... projection) {
        return queryFile(MediaStore.Images.Media.getContentUri(sStorageVolumeName), file,
                /*includePending*/ true, projection);
    }

    /**
     * Queries {@link ContentResolver} for a file and returns the corresponding mime type for its
     * entry in the database.
     */
    @NonNull
    public static String getFileMimeTypeFromDatabase(@NonNull File file) {
        String mimeType = "";
        try (Cursor c = queryFile(file, MediaStore.MediaColumns.MIME_TYPE)) {
            if (c.moveToFirst()) {
                mimeType = c.getString(0);
            }
        }
        return mimeType;
    }

    /**
     * Sets {@link AppOpsManager#MODE_ALLOWED} for the given {@code ops} and the given {@code uid}.
     *
     * <p>This method drops shell permission identity.
     */
    public static void allowAppOpsToUid(int uid, @NonNull String... ops) {
        setAppOpsModeForUid(uid, AppOpsManager.MODE_ALLOWED, ops);
    }

    /**
     * Sets {@link AppOpsManager#MODE_ERRORED} for the given {@code ops} and the given {@code uid}.
     *
     * <p>This method drops shell permission identity.
     */
    public static void denyAppOpsToUid(int uid, @NonNull String... ops) {
        setAppOpsModeForUid(uid, AppOpsManager.MODE_ERRORED, ops);
    }

    /**
     * Deletes the given file through {@link ContentResolver} and {@link MediaStore} APIs,
     * and asserts that the file was successfully deleted from the database.
     */
    public static void deleteWithMediaProvider(@NonNull File file) {
        Bundle extras = new Bundle();
        extras.putString(ContentResolver.QUERY_ARG_SQL_SELECTION,
                MediaStore.MediaColumns.DATA + " = ?");
        extras.putStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS,
                new String[] {file.getPath()});
        extras.putInt(MediaStore.QUERY_ARG_MATCH_PENDING, MediaStore.MATCH_INCLUDE);
        extras.putInt(MediaStore.QUERY_ARG_MATCH_TRASHED, MediaStore.MATCH_INCLUDE);
        assertThat(getContentResolver().delete(
                MediaStore.Files.getContentUri(sStorageVolumeName), extras)).isEqualTo(1);
    }

    /**
     * Deletes db rows and files corresponding to uri through {@link ContentResolver} and
     * {@link MediaStore} APIs.
     */
    public static void deleteWithMediaProviderNoThrow(Uri... uris) {
        for (Uri uri : uris) {
            if (uri == null) continue;

            try {
                getContentResolver().delete(uri, Bundle.EMPTY);
            } catch (Exception ignored) {
            }
        }
    }

    /**
     * Renames the given file through {@link ContentResolver} and {@link MediaStore} APIs,
     * and asserts that the file was updated in the database.
     */
    public static void updateDisplayNameWithMediaProvider(Uri uri, String relativePath,
            String oldDisplayName, String newDisplayName) {
        String selection = MediaStore.MediaColumns.RELATIVE_PATH + " = ? AND "
                + MediaStore.MediaColumns.DISPLAY_NAME + " = ?";
        String[] selectionArgs = {relativePath + '/', oldDisplayName};
        Bundle extras = new Bundle();
        extras.putString(ContentResolver.QUERY_ARG_SQL_SELECTION, selection);
        extras.putStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS, selectionArgs);
        extras.putInt(MediaStore.QUERY_ARG_MATCH_PENDING, MediaStore.MATCH_INCLUDE);
        extras.putInt(MediaStore.QUERY_ARG_MATCH_TRASHED, MediaStore.MATCH_INCLUDE);

        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, newDisplayName);

        assertThat(getContentResolver().update(uri, values, extras)).isEqualTo(1);
    }

    /**
     * Opens the given file through {@link ContentResolver} and {@link MediaStore} APIs.
     */
    @NonNull
    public static ParcelFileDescriptor openWithMediaProvider(@NonNull File file, String mode)
            throws Exception {
        final Uri fileUri = getFileUri(file);
        assertThat(fileUri).isNotNull();
        Log.i(TAG, "Uri: " + fileUri + ". Data: " + file.getPath());
        ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(fileUri, mode);
        assertThat(pfd).isNotNull();
        return pfd;
    }

    /**
     * Asserts the given operation throws an exception of type {@code T}.
     */
    public static <T extends Exception> void assertThrows(Class<T> clazz, Operation<Exception> r)
            throws Exception {
        assertThrows(clazz, "", r);
    }

    /**
     * Asserts the given operation throws an exception of type {@code T}.
     */
    public static <T extends Exception> void assertThrows(
            Class<T> clazz, String errMsg, Operation<Exception> r) throws Exception {
        try {
            r.run();
            fail("Expected " + clazz + " to be thrown");
        } catch (Exception e) {
            if (!clazz.isAssignableFrom(e.getClass()) || !e.getMessage().contains(errMsg)) {
                Log.e(TAG, "Expected " + clazz + " exception with error message: " + errMsg, e);
                throw e;
            }
        }
    }

    /**
     * A functional interface representing an operation that takes no arguments,
     * returns no arguments and might throw an {@link Exception} of any kind.
     *
     * @param T the subclass of {@link java.lang.Exception} that this operation might throw.
     */
    @FunctionalInterface
    public interface Operation<T extends Exception> {
        /**
         * This is the method that gets called for any object that implements this interface.
         */
        void run() throws T;
    }

    /**
     * Deletes the given file. If the file is a directory, then deletes all of its children (files
     * or directories) recursively.
     */
    public static boolean deleteRecursively(@NonNull File path) {
        if (path.isDirectory()) {
            for (File child : path.listFiles()) {
                if (!deleteRecursively(child)) {
                    return false;
                }
            }
        }
        return path.delete();
    }

    /**
     * Asserts can rename file.
     */
    public static void assertCanRenameFile(File oldFile, File newFile) {
        assertCanRenameFile(oldFile, newFile, /* checkDB */ true);
    }

    /**
     * Asserts can rename file and optionally checks if the database is updated after rename.
     */
    public static void assertCanRenameFile(File oldFile, File newFile, boolean checkDatabase) {
        assertThat(oldFile.renameTo(newFile)).isTrue();
        assertThat(oldFile.exists()).isFalse();
        assertThat(newFile.exists()).isTrue();
        if (checkDatabase) {
            assertThat(getFileRowIdFromDatabase(oldFile)).isEqualTo(-1);
            assertThat(getFileRowIdFromDatabase(newFile)).isNotEqualTo(-1);
        }
    }

    /**
     * Asserts cannot rename file.
     */
    public static void assertCantRenameFile(File oldFile, File newFile) {
        final int rowId = getFileRowIdFromDatabase(oldFile);
        assertThat(oldFile.renameTo(newFile)).isFalse();
        assertThat(oldFile.exists()).isTrue();
        assertThat(getFileRowIdFromDatabase(oldFile)).isEqualTo(rowId);
    }

    /**
     * Asserts can rename directory.
     */
    public static void assertCanRenameDirectory(File oldDirectory, File newDirectory,
            @Nullable File[] oldFilesList, @Nullable File[] newFilesList) {
        assertThat(oldDirectory.renameTo(newDirectory)).isTrue();
        assertThat(oldDirectory.exists()).isFalse();
        assertThat(newDirectory.exists()).isTrue();
        for (File file : oldFilesList != null ? oldFilesList : new File[0]) {
            assertThat(file.exists()).isFalse();
            assertThat(getFileRowIdFromDatabase(file)).isEqualTo(-1);
        }
        for (File file : newFilesList != null ? newFilesList : new File[0]) {
            assertThat(file.exists()).isTrue();
            assertThat(getFileRowIdFromDatabase(file)).isNotEqualTo(-1);
        }
    }

    /**
     * Asserts cannot rename directory.
     */
    public static void assertCantRenameDirectory(
            File oldDirectory, File newDirectory, @Nullable File[] oldFilesList) {
        assertThat(oldDirectory.renameTo(newDirectory)).isFalse();
        assertThat(oldDirectory.exists()).isTrue();
        for (File file : oldFilesList != null ? oldFilesList : new File[0]) {
            assertThat(file.exists()).isTrue();
            assertThat(getFileRowIdFromDatabase(file)).isNotEqualTo(-1);
        }
    }

    /**
     * Returns whether we can open the file.
     */
    public static boolean canOpen(File file, boolean forWrite) {
        if (forWrite) {
            try (FileOutputStream fis = new FileOutputStream(file)) {
                return true;
            } catch (IOException expected) {
                return false;
            }
        } else {
            try (FileInputStream fis = new FileInputStream(file)) {
                return true;
            } catch (IOException expected) {
                return false;
            }
        }
    }

    /**
     * Polls for external storage to be mounted.
     */
    public static void pollForExternalStorageState() throws Exception {
        pollForCondition(
                () -> Environment.getExternalStorageState(getExternalStorageDir())
                        .equals(Environment.MEDIA_MOUNTED),
                "Timed out while waiting for ExternalStorageState to be MEDIA_MOUNTED");
    }

    /**
     * Polls until we're granted or denied a given permission.
     */
    public static void pollForPermission(String perm, boolean granted) throws Exception {
        pollForCondition(() -> granted == checkPermissionAndAppOp(perm),
                "Timed out while waiting for permission " + perm + " to be "
                        + (granted ? "granted" : "revoked"));
    }

    /**
     * Asserts the entire content of the file equals exactly {@code expectedContent}.
     */
    public static void assertFileContent(File file, byte[] expectedContent) throws IOException {
        try (FileInputStream fis = new FileInputStream(file)) {
            assertInputStreamContent(fis, expectedContent);
        }
    }

    /**
     * Asserts the entire content of the file equals exactly {@code expectedContent}.
     * <p>Sets {@code fd} to beginning of file first.
     */
    public static void assertFileContent(FileDescriptor fd, byte[] expectedContent)
            throws IOException, ErrnoException {
        Os.lseek(fd, 0, OsConstants.SEEK_SET);
        try (FileInputStream fis = new FileInputStream(fd)) {
            assertInputStreamContent(fis, expectedContent);
        }
    }

    /**
     * Asserts that {@code dir} is a directory and that it doesn't contain any of
     * {@code unexpectedContent}
     */
    public static void assertDirectoryDoesNotContain(@NonNull File dir, File... unexpectedContent) {
        assertThat(dir.isDirectory()).isTrue();
        assertThat(Arrays.asList(dir.listFiles())).containsNoneIn(unexpectedContent);
    }

    /**
     * Asserts that {@code dir} is a directory and that it contains all of {@code expectedContent}
     */
    public static void assertDirectoryContains(@NonNull File dir, File... expectedContent) {
        assertThat(dir.isDirectory()).isTrue();
        assertThat(Arrays.asList(dir.listFiles())).containsAllIn(expectedContent);
    }

    public static File getExternalStorageDir() {
        return sExternalStorageDirectory;
    }

    public static void setExternalStorageVolume(@NonNull String volName) {
        sStorageVolumeName = volName.toLowerCase(Locale.ROOT);
        sExternalStorageDirectory = new File("/storage/" + volName);
    }

    /**
     * Resets the root directory of external storage to the default.
     *
     * @see Environment#getExternalStorageDirectory()
     */
    public static void resetDefaultExternalStorageVolume() {
        sStorageVolumeName = MediaStore.VOLUME_EXTERNAL;
        sExternalStorageDirectory = Environment.getExternalStorageDirectory();
    }

    /**
     * Creates and returns the Android data sub-directory belonging to the calling package.
     */
    public static File getExternalFilesDir() {
        final String packageName = getContext().getPackageName();
        final File res = new File(getAndroidDataDir(), packageName + "/files");
        if (!res.equals(getContext().getExternalFilesDir(null))) {
            res.mkdirs();
        }
        return res;
    }

    /**
     * Creates and returns the Android media sub-directory belonging to the calling package.
     */
    public static File getExternalMediaDir() {
        final String packageName = getContext().getPackageName();
        final File res = new File(getAndroidMediaDir(), packageName);
        if (!res.equals(getContext().getExternalMediaDirs()[0])) {
            res.mkdirs();
        }
        return res;
    }

    public static File getAlarmsDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_ALARMS);
    }

    public static File getAndroidDir() {
        return new File(getExternalStorageDir(),
                "Android");
    }

    public static File getAudiobooksDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_AUDIOBOOKS);
    }

    public static File getDcimDir() {
        return new File(getExternalStorageDir(), Environment.DIRECTORY_DCIM);
    }

    public static File getDocumentsDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_DOCUMENTS);
    }

    public static File getDownloadDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_DOWNLOADS);
    }

    public static File getMusicDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_MUSIC);
    }

    public static File getMoviesDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_MOVIES);
    }

    public static File getNotificationsDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_NOTIFICATIONS);
    }

    public static File getPicturesDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_PICTURES);
    }

    public static File getPodcastsDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_PODCASTS);
    }

    public static File getRingtonesDir() {
        return new File(getExternalStorageDir(),
                Environment.DIRECTORY_RINGTONES);
    }

    public static File getAndroidDataDir() {
        return new File(getAndroidDir(), "data");
    }

    public static File getAndroidMediaDir() {
        return new File(getAndroidDir(), "media");
    }

    public static File[] getDefaultTopLevelDirs() {
        return new File [] { getAlarmsDir(), getAndroidDir(), getAudiobooksDir(), getDcimDir(),
                getDocumentsDir(), getDownloadDir(), getMusicDir(), getMoviesDir(),
                getNotificationsDir(), getPicturesDir(), getPodcastsDir(), getRingtonesDir() };
    }

    private static void assertInputStreamContent(InputStream in, byte[] expectedContent)
            throws IOException {
        assertThat(ByteStreams.toByteArray(in)).isEqualTo(expectedContent);
    }

    /**
     * Checks if the given {@code permission} is granted and corresponding AppOp is MODE_ALLOWED.
     */
    private static boolean checkPermissionAndAppOp(String permission) {
        final int pid = Os.getpid();
        final int uid = Os.getuid();
        final Context context = getContext();
        final String packageName = context.getPackageName();
        if (context.checkPermission(permission, pid, uid) != PackageManager.PERMISSION_GRANTED) {
            return false;
        }

        final String op = AppOpsManager.permissionToOp(permission);
        // No AppOp associated with the given permission, skip AppOp check.
        if (op == null) {
            return true;
        }

        final AppOpsManager appOps = context.getSystemService(AppOpsManager.class);
        try {
            appOps.checkPackage(uid, packageName);
        } catch (SecurityException e) {
            return false;
        }

        return appOps.unsafeCheckOpNoThrow(op, uid, packageName) == AppOpsManager.MODE_ALLOWED;
    }

    /**
     * <p>This method drops shell permission identity.
     */
    private static void forceStopApp(String packageName) throws Exception {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            uiAutomation.adoptShellPermissionIdentity(Manifest.permission.FORCE_STOP_PACKAGES);

            getContext().getSystemService(ActivityManager.class).forceStopPackage(packageName);
            Thread.sleep(1000);
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    /**
     * <p>This method drops shell permission identity.
     */
    private static void sendIntentToTestApp(TestApp testApp, String dirPath, String actionName,
            BroadcastReceiver broadcastReceiver, CountDownLatch latch) throws Exception {
        final String packageName = testApp.getPackageName();
        forceStopApp(packageName);
        // Register broadcast receiver
        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(actionName);
        intentFilter.addCategory(Intent.CATEGORY_DEFAULT);
        getContext().registerReceiver(broadcastReceiver, intentFilter);

        // Launch the test app.
        final Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(packageName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(QUERY_TYPE, actionName);
        intent.putExtra(INTENT_EXTRA_PATH, dirPath);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        getContext().startActivity(intent);
        if (!latch.await(POLLING_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)) {
            final String errorMessage = "Timed out while waiting to receive " + actionName
                    + " intent from " + packageName;
            throw new TimeoutException(errorMessage);
        }
        getContext().unregisterReceiver(broadcastReceiver);
    }

    /**
     * Gets images/video metadata from a test app.
     *
     * <p>This method drops shell permission identity.
     */
    private static HashMap<String, String> getMetadataFromTestApp(
            TestApp testApp, String dirPath, String actionName) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        final HashMap<String, String> appOutputList = new HashMap<>();
        final Exception[] exception = new Exception[1];
        exception[0] = null;
        final BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.hasExtra(INTENT_EXCEPTION)) {
                    exception[0] = (Exception) (intent.getExtras().get(INTENT_EXCEPTION));
                } else if (intent.hasExtra(actionName)) {
                    HashMap<String, String> res =
                            (HashMap<String, String>) intent.getExtras().get(actionName);
                    appOutputList.putAll(res);
                }
                latch.countDown();
            }
        };
        sendIntentToTestApp(testApp, dirPath, actionName, broadcastReceiver, latch);
        if (exception[0] != null) {
            throw exception[0];
        }
        return appOutputList;
    }

    /**
     * <p>This method drops shell permission identity.
     */
    private static ArrayList<String> getContentsFromTestApp(
            TestApp testApp, String dirPath, String actionName) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        final ArrayList<String> appOutputList = new ArrayList<String>();
        final Exception[] exception = new Exception[1];
        exception[0] = null;
        final BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.hasExtra(INTENT_EXCEPTION)) {
                    exception[0] = (Exception) (intent.getSerializableExtra(INTENT_EXCEPTION));
                } else if (intent.hasExtra(actionName)) {
                    appOutputList.addAll(intent.getStringArrayListExtra(actionName));
                }
                latch.countDown();
            }
        };

        sendIntentToTestApp(testApp, dirPath, actionName, broadcastReceiver, latch);
        if (exception[0] != null) {
            throw exception[0];
        }
        return appOutputList;
    }

    /**
     * <p>This method drops shell permission identity.
     */
    private static boolean getResultFromTestApp(TestApp testApp, String dirPath, String actionName)
            throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        final boolean[] appOutput = new boolean[1];
        final Exception[] exception = new Exception[1];
        exception[0] = null;
        BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.hasExtra(INTENT_EXCEPTION)) {
                    exception[0] = (Exception) (intent.getSerializableExtra(INTENT_EXCEPTION));
                } else if (intent.hasExtra(actionName)) {
                    appOutput[0] = intent.getBooleanExtra(actionName, false);
                }
                latch.countDown();
            }
        };

        sendIntentToTestApp(testApp, dirPath, actionName, broadcastReceiver, latch);
        if (exception[0] != null) {
            throw exception[0];
        }
        return appOutput[0];
    }

    /**
     * Sets {@code mode} for the given {@code ops} and the given {@code uid}.
     *
     * <p>This method drops shell permission identity.
     */
    private static void setAppOpsModeForUid(int uid, int mode, @NonNull String... ops) {
        adoptShellPermissionIdentity(null);
        try {
            for (String op : ops) {
                getContext().getSystemService(AppOpsManager.class).setUidMode(op, uid, mode);
            }
        } finally {
            dropShellPermissionIdentity();
        }
    }

    /**
     * Queries {@link ContentResolver} for a file IS_PENDING=0 and returns a {@link Cursor} with the
     * given columns.
     */
    @NonNull
    public static Cursor queryFileExcludingPending(@NonNull File file, String... projection) {
        return queryFile(MediaStore.Files.getContentUri(sStorageVolumeName), file,
                /*includePending*/ false, projection);
    }

    @NonNull
    public static Cursor queryFile(@NonNull File file, String... projection) {
        return queryFile(MediaStore.Files.getContentUri(sStorageVolumeName), file,
                /*includePending*/ true, projection);
    }

    @NonNull
    private static Cursor queryFile(@NonNull Uri uri, @NonNull File file, boolean includePending,
            String... projection) {
        Bundle queryArgs = new Bundle();
        queryArgs.putString(ContentResolver.QUERY_ARG_SQL_SELECTION,
                MediaStore.MediaColumns.DATA + " = ?");
        queryArgs.putStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS,
                new String[] { file.getAbsolutePath() });
        queryArgs.putInt(MediaStore.QUERY_ARG_MATCH_TRASHED, MediaStore.MATCH_INCLUDE);

        if (includePending) {
            queryArgs.putInt(MediaStore.QUERY_ARG_MATCH_PENDING, MediaStore.MATCH_INCLUDE);
        } else {
            queryArgs.putInt(MediaStore.QUERY_ARG_MATCH_PENDING, MediaStore.MATCH_EXCLUDE);
        }

        final Cursor c = getContentResolver().query(uri, projection, queryArgs, null);
        assertThat(c).isNotNull();
        return c;
    }

    /**
     * Creates a new virtual public volume and returns the volume's name.
     */
    public static void createNewPublicVolume() throws Exception {
        executeShellCommand("sm set-force-adoptable on");
        executeShellCommand("sm set-virtual-disk true");
        Thread.sleep(2000);
        pollForCondition(TestUtils::partitionDisk, "Timed out while waiting for disk partitioning");
    }

    private static boolean partitionDisk() {
        try {
            final String listDisks = executeShellCommand("sm list-disks").trim();
            executeShellCommand("sm partition " + listDisks + " public");
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Gets the name of the public volume.
     */
    public static String getPublicVolumeName() throws Exception {
        final String[] volName = new String[1];
        pollForCondition(() -> {
            volName[0] = getPublicVolumeNameInternal();
            return volName[0] != null;
        }, "Timed out while waiting for public volume to be ready");

        return volName[0];
    }

    private static String getPublicVolumeNameInternal() {
        final String[] allVolumeDetails;
        try {
            allVolumeDetails = executeShellCommand("sm list-volumes")
                    .trim().split("\n");
        } catch (Exception e) {
            Log.e(TAG, "Failed to execute shell command", e);
            return null;
        }
        for (String volDetails : allVolumeDetails) {
            if (volDetails.startsWith("public")) {
                final String[] publicVolumeDetails = volDetails.trim().split(" ");
                String res = publicVolumeDetails[publicVolumeDetails.length - 1];
                if ("null".equals(res)) {
                    continue;
                }
                return res;
            }
        }
        return null;
    }

    /**
     * Returns the content URI of the volume on which the test is running.
     */
    public static Uri getTestVolumeFileUri() {
        return MediaStore.Files.getContentUri(sStorageVolumeName);
    }

    private static void pollForCondition(Supplier<Boolean> condition, String errorMessage)
            throws Exception {
        for (int i = 0; i < POLLING_TIMEOUT_MILLIS / POLLING_SLEEP_MILLIS; i++) {
            if (condition.get()) {
                return;
            }
            Thread.sleep(POLLING_SLEEP_MILLIS);
        }
        throw new TimeoutException(errorMessage);
    }

    /**
     * Polls for all files access to be allowed.
     */
    public static void pollForManageExternalStorageAllowed() throws Exception {
        pollForCondition(
                () -> Environment.isExternalStorageManager(),
                "Timed out while waiting for MANAGE_EXTERNAL_STORAGE");
    }
}
