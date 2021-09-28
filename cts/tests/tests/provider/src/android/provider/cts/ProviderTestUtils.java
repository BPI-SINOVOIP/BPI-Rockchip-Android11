/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.provider.cts;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.FileUtils;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.UserManager;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.provider.MediaStore;
import android.provider.MediaStore.MediaColumns;
import android.provider.cts.media.MediaStoreUtils;
import android.provider.cts.media.MediaStoreUtils.PendingParams;
import android.provider.cts.media.MediaStoreUtils.PendingSession;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.Timeout;

import com.google.common.io.BaseEncoding;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Utility methods for provider cts tests.
 */
public class ProviderTestUtils {
    static final String TAG = "ProviderTestUtils";

    private static final int BACKUP_TIMEOUT_MILLIS = 4000;
    private static final Pattern BMGR_ENABLED_PATTERN = Pattern.compile(
            "^Backup Manager currently (enabled|disabled)$");

    private static final Pattern PATTERN_STORAGE_PATH = Pattern.compile(
            "(?i)^/storage/[^/]+/(?:[0-9]+/)?");

    private static final Timeout IO_TIMEOUT = new Timeout("IO_TIMEOUT", 2_000, 2, 2_000);

    public static Iterable<String> getSharedVolumeNames() {
        // We test both new and legacy volume names
        final HashSet<String> testVolumes = new HashSet<>();
        testVolumes.addAll(
                MediaStore.getExternalVolumeNames(InstrumentationRegistry.getTargetContext()));
        testVolumes.add(MediaStore.VOLUME_EXTERNAL);
        return testVolumes;
    }

    public static String resolveVolumeName(String volumeName) {
        if (MediaStore.VOLUME_EXTERNAL.equals(volumeName)) {
            return MediaStore.VOLUME_EXTERNAL_PRIMARY;
        } else {
            return volumeName;
        }
    }

    static void setDefaultSmsApp(boolean setToSmsApp, String packageName, UiAutomation uiAutomation)
            throws Exception {
        String mode = setToSmsApp ? "allow" : "default";
        String cmd = "appops set %s %s %s";
        executeShellCommand(String.format(cmd, packageName, "WRITE_SMS", mode), uiAutomation);
        executeShellCommand(String.format(cmd, packageName, "READ_SMS", mode), uiAutomation);
    }

    public static String executeShellCommand(String command) throws IOException {
        return executeShellCommand(command,
                InstrumentationRegistry.getInstrumentation().getUiAutomation());
    }

    public static String executeShellCommand(String command, UiAutomation uiAutomation)
            throws IOException {
        Log.v(TAG, "$ " + command);
        ParcelFileDescriptor pfd = uiAutomation.executeShellCommand(command.toString());
        BufferedReader br = null;
        try (InputStream in = new FileInputStream(pfd.getFileDescriptor());) {
            br = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8));
            String str = null;
            StringBuilder out = new StringBuilder();
            while ((str = br.readLine()) != null) {
                Log.v(TAG, "> " + str);
                out.append(str);
            }
            return out.toString();
        } finally {
            if (br != null) {
                br.close();
            }
        }
    }

    static String setBackupTransport(String transport, UiAutomation uiAutomation) throws Exception {
        String output = executeShellCommand("bmgr transport " + transport, uiAutomation);
        Pattern pattern = Pattern.compile("\\(formerly (.*)\\)$");
        Matcher matcher = pattern.matcher(output);
        if (matcher.find()) {
            return matcher.group(1);
        } else {
            throw new Exception("non-parsable output setting bmgr transport: " + output);
        }
    }

    static boolean setBackupEnabled(boolean enable, UiAutomation uiAutomation) throws Exception {
        // Check to see the previous state of the backup service
        boolean previouslyEnabled = false;
        String output = executeShellCommand("bmgr enabled", uiAutomation);
        Matcher matcher = BMGR_ENABLED_PATTERN.matcher(output.trim());
        if (matcher.find()) {
            previouslyEnabled = "enabled".equals(matcher.group(1));
        } else {
            throw new RuntimeException("Backup output format changed.  No longer matches"
                    + " expected regex: " + BMGR_ENABLED_PATTERN + "\nactual: '" + output + "'");
        }

        executeShellCommand("bmgr enable " + enable, uiAutomation);
        return previouslyEnabled;
    }

    static boolean hasBackupTransport(String transport, UiAutomation uiAutomation)
            throws Exception {
        String output = executeShellCommand("bmgr list transports", uiAutomation);
        for (String t : output.split(" ")) {
            if ("*".equals(t)) {
                // skip the current selection marker.
                continue;
            } else if (Objects.equals(transport, t)) {
                return true;
            }
        }
        return false;
    }

    static void runBackup(String packageName, UiAutomation uiAutomation) throws Exception {
        executeShellCommand("bmgr backupnow " + packageName, uiAutomation);
        Thread.sleep(BACKUP_TIMEOUT_MILLIS);
    }

    static void runRestore(String packageName, UiAutomation uiAutomation) throws Exception {
        executeShellCommand("bmgr restore 1 " + packageName, uiAutomation);
        Thread.sleep(BACKUP_TIMEOUT_MILLIS);
    }

    static void wipeBackup(String backupTransport, String packageName, UiAutomation uiAutomation)
            throws Exception {
        executeShellCommand("bmgr wipe " + backupTransport + " " + packageName, uiAutomation);
    }

    public static void waitForIdle() {
        MediaStore.waitForIdle(InstrumentationRegistry.getTargetContext().getContentResolver());
    }

    /**
     * Waits until a file exists, or fails.
     *
     * @return existing file.
     */
    public static File waitUntilExists(File file) throws IOException {
        try {
            return IO_TIMEOUT.run("file '" + file + "' doesn't exist yet", () -> {
                return file.exists() ? file : null; // will retry if it returns null
            });
        } catch (Exception e) {
            throw new IOException(e);
        }
    }

    public static File getVolumePath(String volumeName) {
        final Context context = InstrumentationRegistry.getTargetContext();
        return context.getSystemService(StorageManager.class)
                .getStorageVolume(MediaStore.Files.getContentUri(volumeName)).getDirectory();
    }

    public static File stageDir(String volumeName) throws IOException {
        if (MediaStore.VOLUME_EXTERNAL.equals(volumeName)) {
            volumeName = MediaStore.VOLUME_EXTERNAL_PRIMARY;
        }
        final StorageVolume vol = InstrumentationRegistry.getTargetContext()
                .getSystemService(StorageManager.class)
                .getStorageVolume(MediaStore.Files.getContentUri(volumeName));
        File dir = Environment.buildPath(vol.getDirectory(), "Android", "media",
                "android.provider.cts");
        Log.d(TAG, "stageDir(" + volumeName + "): returning " + dir);
        return dir;
    }

    public static File stageDownloadDir(String volumeName) throws IOException {
        if (MediaStore.VOLUME_EXTERNAL.equals(volumeName)) {
            volumeName = MediaStore.VOLUME_EXTERNAL_PRIMARY;
        }
        final StorageVolume vol = InstrumentationRegistry.getTargetContext()
                .getSystemService(StorageManager.class)
                .getStorageVolume(MediaStore.Files.getContentUri(volumeName));
        return Environment.buildPath(vol.getDirectory(),
                Environment.DIRECTORY_DOWNLOADS, "android.provider.cts");
    }

    public static File stageFile(int resId, File file) throws IOException {
        // The caller may be trying to stage into a location only available to
        // the shell user, so we need to perform the entire copy as the shell
        final Context context = InstrumentationRegistry.getTargetContext();
        UserManager userManager = context.getSystemService(UserManager.class);
        if (userManager.isSystemUser() &&
                    FileUtils.contains(Environment.getStorageDirectory(), file)) {
            executeShellCommand("mkdir -p " + file.getParent());
            waitUntilExists(file.getParentFile());
            try (AssetFileDescriptor afd = context.getResources().openRawResourceFd(resId)) {
                final File source = ParcelFileDescriptor.getFile(afd.getFileDescriptor());
                final long skip = afd.getStartOffset();
                final long count = afd.getLength();

                try {
                    // Try to create the file as calling package so that calling package remains
                    // as owner of the file.
                    file.createNewFile();
                } catch (IOException ignored) {
                    // Apps can't create files in other app's private directories, but shell can. If
                    // file creation fails, we ignore and let `dd` command create it instead.
                }

                executeShellCommand(String.format("dd bs=1 if=%s skip=%d count=%d of=%s",
                        source.getAbsolutePath(), skip, count, file.getAbsolutePath()));

                // Force sync to try updating other views
                executeShellCommand("sync");
            }
        } else {
            final File dir = file.getParentFile();
            dir.mkdirs();
            if (!dir.exists()) {
                throw new FileNotFoundException("Failed to create parent for " + file);
            }
            try (InputStream source = context.getResources().openRawResource(resId);
                    OutputStream target = new FileOutputStream(file)) {
                FileUtils.copy(source, target);
            }
        }
        return waitUntilExists(file);
    }

    public static Uri stageMedia(int resId, Uri collectionUri) throws IOException {
        return stageMedia(resId, collectionUri, "image/png");
    }

    public static Uri stageMedia(int resId, Uri collectionUri, String mimeType) throws IOException {
        final Context context = InstrumentationRegistry.getTargetContext();
        final String displayName = "cts" + System.nanoTime();
        final PendingParams params = new PendingParams(collectionUri, displayName, mimeType);
        final Uri pendingUri = MediaStoreUtils.createPending(context, params);
        try (PendingSession session = MediaStoreUtils.openPending(context, pendingUri)) {
            try (InputStream source = context.getResources().openRawResource(resId);
                    OutputStream target = session.openOutputStream()) {
                FileUtils.copy(source, target);
            }
            return session.publish();
        }
    }

    public static Uri scanFile(File file) throws Exception {
        final Uri uri = MediaStore
                .scanFile(InstrumentationRegistry.getTargetContext().getContentResolver(), file);
        assertWithMessage("no URI for '%s'", file).that(uri).isNotNull();
        return uri;
    }

    public static Uri scanFileFromShell(File file) throws Exception {
        return scanFile(file);
    }

    public static void scanVolume(File file) throws Exception {
        final StorageVolume vol = InstrumentationRegistry.getTargetContext()
                .getSystemService(StorageManager.class).getStorageVolume(file);
        MediaStore.scanVolume(InstrumentationRegistry.getTargetContext().getContentResolver(),
                vol.getMediaStoreVolumeName());
    }

    public static void setOwner(Uri uri, String packageName) throws Exception {
        executeShellCommand("content update"
                + " --user " + InstrumentationRegistry.getTargetContext().getUserId()
                + " --uri " + uri
                + " --bind owner_package_name:s:" + packageName);
    }

    public static void clearOwner(Uri uri) throws Exception {
        executeShellCommand("content update"
                + " --user " + InstrumentationRegistry.getTargetContext().getUserId()
                + " --uri " + uri
                + " --bind owner_package_name:n:");
    }

    public static byte[] hash(InputStream in) throws Exception {
        try (DigestInputStream digestIn = new DigestInputStream(in,
                MessageDigest.getInstance("SHA-1"));
                OutputStream out = new FileOutputStream(new File("/dev/null"))) {
            FileUtils.copy(digestIn, out);
            return digestIn.getMessageDigest().digest();
        }
    }

    /**
     * Extract the average overall color of the given bitmap.
     * <p>
     * Internally takes advantage of gaussian blurring that is naturally applied
     * when downscaling an image.
     */
    public static int extractAverageColor(Bitmap bitmap) {
        final Bitmap res = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(res);
        final Rect src = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());
        final Rect dst = new Rect(0, 0, 1, 1);
        canvas.drawBitmap(bitmap, src, dst, null);
        return res.getPixel(0, 0);
    }

    public static void assertColorMostlyEquals(int expected, int actual) {
        assertTrue("Expected " + Integer.toHexString(expected) + " but was "
                + Integer.toHexString(actual), isColorMostlyEquals(expected, actual));
    }

    public static void assertColorMostlyNotEquals(int expected, int actual) {
        assertFalse("Expected " + Integer.toHexString(expected) + " but was "
                + Integer.toHexString(actual), isColorMostlyEquals(expected, actual));
    }

    private static boolean isColorMostlyEquals(int expected, int actual) {
        final float[] expectedHSV = new float[3];
        final float[] actualHSV = new float[3];
        Color.colorToHSV(expected, expectedHSV);
        Color.colorToHSV(actual, actualHSV);

        // Fail if more than a 10% difference in any component
        if (Math.abs(expectedHSV[0] - actualHSV[0]) > 36) return false;
        if (Math.abs(expectedHSV[1] - actualHSV[1]) > 0.1f) return false;
        if (Math.abs(expectedHSV[2] - actualHSV[2]) > 0.1f) return false;
        return true;
    }

    public static void assertExists(String path) throws IOException {
        assertExists(null, path);
    }

    public static void assertExists(File file) throws IOException {
        assertExists(null, file.getAbsolutePath());
    }

    public static void assertExists(String msg, String path) throws IOException {
        if (!access(path)) {
            fail(msg);
        }
    }

    public static void assertNotExists(String path) throws IOException {
        assertNotExists(null, path);
    }

    public static void assertNotExists(File file) throws IOException {
        assertNotExists(null, file.getAbsolutePath());
    }

    public static void assertNotExists(String msg, String path) throws IOException {
        if (access(path)) {
            fail(msg);
        }
    }

    private static boolean access(String path) throws IOException {
        // The caller may be trying to stage into a location only available to
        // the shell user, so we need to perform the entire copy as the shell
        if (FileUtils.contains(Environment.getStorageDirectory(), new File(path))) {
            return executeShellCommand("ls -la " + path).contains(path);
        } else {
            try {
                Os.access(path, OsConstants.F_OK);
                return true;
            } catch (ErrnoException e) {
                if (e.errno == OsConstants.ENOENT) {
                    return false;
                } else {
                    throw new IOException(e.getMessage());
                }
            }
        }
    }

    public static boolean containsId(Uri uri, long id) {
        return containsId(uri, null, id);
    }

    public static boolean containsId(Uri uri, Bundle extras, long id) {
        try (Cursor c = InstrumentationRegistry.getTargetContext().getContentResolver().query(uri,
                new String[] { MediaColumns._ID }, extras, null)) {
            while (c.moveToNext()) {
                if (c.getLong(0) == id) return true;
            }
        }
        return false;
    }

    public static File getRawFile(Uri uri) throws Exception {
        final String res = ProviderTestUtils.executeShellCommand("content query --uri " + uri
                + " --user " + InstrumentationRegistry.getTargetContext().getUserId()
                + " --projection _data",
                InstrumentationRegistry.getInstrumentation().getUiAutomation());
        final int i = res.indexOf("_data=");
        if (i >= 0) {
            return new File(res.substring(i + 6));
        } else {
            throw new FileNotFoundException("Failed to find _data for " + uri + "; found " + res);
        }
    }

    public static String getRawFileHash(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-1");
        try (InputStream in = new BufferedInputStream(Files.newInputStream(file.toPath()))) {
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) >= 0) {
                digest.update(buf, 0, n);
            }
        }

        byte[] hash = digest.digest();
        return BaseEncoding.base16().encode(hash);
    }

    public static File getRelativeFile(Uri uri) throws Exception {
        final String path = getRawFile(uri).getAbsolutePath();
        final Matcher matcher = PATTERN_STORAGE_PATH.matcher(path);
        if (matcher.find()) {
            return new File(path.substring(matcher.end()));
        } else {
            throw new IllegalArgumentException();
        }
    }

    /** Revokes ACCESS_MEDIA_LOCATION from the test app */
    public static void revokeMediaLocationPermission(Context context) throws Exception {
        try {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .adoptShellPermissionIdentity("android.permission.MANAGE_APP_OPS_MODES",
                            "android.permission.REVOKE_RUNTIME_PERMISSIONS");

            // Revoking ACCESS_MEDIA_LOCATION permission will kill the test app.
            // Deny access_media_permission App op to revoke this permission.
            PackageManager packageManager = context.getPackageManager();
            String packageName = context.getPackageName();
            if (packageManager.checkPermission(android.Manifest.permission.ACCESS_MEDIA_LOCATION,
                    packageName) == PackageManager.PERMISSION_GRANTED) {
                context.getPackageManager().updatePermissionFlags(
                        android.Manifest.permission.ACCESS_MEDIA_LOCATION, packageName,
                        PackageManager.FLAG_PERMISSION_REVOKED_COMPAT,
                        PackageManager.FLAG_PERMISSION_REVOKED_COMPAT, context.getUser());
                context.getSystemService(AppOpsManager.class).setUidMode(
                        "android:access_media_location", Process.myUid(),
                        AppOpsManager.MODE_IGNORED);
            }
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation().
                    dropShellPermissionIdentity();
        }
    }
}
