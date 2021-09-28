/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.cts.writeexternalstorageapp;

import static com.android.cts.externalstorageapp.CommonExternalStorageTest.TAG;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.assertDirNoWriteAccess;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.assertDirReadWriteAccess;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.buildCommonChildDirs;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.buildProbeFile;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.deleteContents;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.getAllPackageSpecificPathsExceptMedia;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.getMountPaths;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.getPrimaryPackageSpecificPaths;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.getSecondaryPackageSpecificPaths;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.readInt;
import static com.android.cts.externalstorageapp.CommonExternalStorageTest.writeInt;

import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.system.Os;
import android.test.AndroidTestCase;
import android.util.Log;

import com.android.cts.externalstorageapp.CommonExternalStorageTest;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.List;
import java.util.Random;

/**
 * Test external storage from an application that has
 * {@link android.Manifest.permission#WRITE_EXTERNAL_STORAGE}.
 */
public class WriteExternalStorageTest extends AndroidTestCase {

    private static final File TEST_FILE = new File(
            Environment.getExternalStorageDirectory(), "meow");

    /**
     * Set of file paths that should all refer to the same location to verify
     * support for legacy paths.
     */
    private static final File[] IDENTICAL_FILES = {
            new File("/sdcard/caek"),
            new File(System.getenv("EXTERNAL_STORAGE"), "caek"),
            new File(Environment.getExternalStorageDirectory(), "caek"),
    };

    @Override
    protected void tearDown() throws Exception {
        try {
            TEST_FILE.delete();
            for (File file : IDENTICAL_FILES) {
                file.delete();
            }
        } finally {
            super.tearDown();
        }
    }

    private void assertExternalStorageMounted() {
        assertEquals(Environment.MEDIA_MOUNTED, Environment.getExternalStorageState());
    }

    public void testReadExternalStorage() throws Exception {
        assertExternalStorageMounted();
        Environment.getExternalStorageDirectory().list();
    }

    public void testWriteExternalStorage() throws Exception {
        final long newTimeMillis = 12345000;
        assertExternalStorageMounted();

        // Write a value and make sure we can read it back
        writeInt(TEST_FILE, 32);
        assertEquals(readInt(TEST_FILE), 32);

        assertTrue("Must be able to set last modified", TEST_FILE.setLastModified(newTimeMillis));

        // This uses the same fd, so info is cached by VFS.
        assertEquals(newTimeMillis, TEST_FILE.lastModified());

        // Obtain a new fd, using the low FS and check timestamp on it.
        ParcelFileDescriptor fd =
                getContext().getContentResolver().openFileDescriptor(
                        MediaStore.scanFile(getContext().getContentResolver(), TEST_FILE), "rw");

        long newTimeSeconds = newTimeMillis / 1000;
        assertEquals(newTimeSeconds, Os.fstat(fd.getFileDescriptor()).st_mtime);
    }

    public void testWriteExternalStorageDirs() throws Exception {
        final File probe = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM),
                "100CTS");

        assertFalse(probe.exists());
        assertTrue(probe.mkdirs());

        try {
            assertDirReadWriteAccess(probe);
        }
        finally {
            probe.delete();
            assertFalse(probe.exists());
        }
    }

    /**
     * Verify that legacy filesystem paths continue working, and that they all
     * point to same location.
     */
    public void testLegacyPaths() throws Exception {
        final Random r = new Random();
        for (File target : IDENTICAL_FILES) {
            // Ensure we're starting with clean slate
            for (File file : IDENTICAL_FILES) {
                file.delete();
            }

            // Write value to our current target
            final int value = r.nextInt();
            writeInt(target, value);

            // Ensure that identical files all contain the value
            for (File file : IDENTICAL_FILES) {
                assertEquals(readInt(file), value);
            }
        }
    }

    public void testPrimaryReadWrite() throws Exception {
        assertDirReadWriteAccess(Environment.getExternalStorageDirectory());
    }

    /**
     * Verify that above our package directories (on primary storage) we always
     * have write access.
     */
    public void testPrimaryWalkingUpTreeReadWrite() throws Exception {
        final List<File> paths = getPrimaryPackageSpecificPaths(getContext());
        final String packageName = getContext().getPackageName();

        for (File path : paths) {
            assertNotNull("Valid media must be inserted during CTS", path);
            assertEquals("Valid media must be inserted during CTS", Environment.MEDIA_MOUNTED,
                    Environment.getExternalStorageState(path));

            assertTrue(path.getAbsolutePath().contains(packageName));

            // Walk until we reach package specific directory.
            while (path.getAbsolutePath().contains(packageName)) {
                assertDirReadWriteAccess(path);
                path = path.getParentFile();
            }
        }
    }

    /**
     * Verify we have valid mount status until we leave the device.
     */
    public void testMountStatusWalkingUpTree() {
        final File top = Environment.getExternalStorageDirectory();
        File path = getContext().getExternalCacheDir();
        final String packageName = getContext().getPackageName();

        int depth = 0;
        while (depth++ < 32) {
            // Check read&write access for only package specific directories. We might not have
            // read/write access for directories like /storage/emulated/0/Android and
            // /storage/emulated/0/Android/<data|media|obb>.
            if (path.getAbsolutePath().contains(packageName)) {
                assertDirReadWriteAccess(path);
                assertEquals(Environment.MEDIA_MOUNTED, Environment.getExternalStorageState(path));
            }
            if (path.getAbsolutePath().equals(top.getAbsolutePath())) {
                break;
            }
            path = path.getParentFile();
        }

        // Make sure we hit the top
        assertEquals(top.getAbsolutePath(), path.getAbsolutePath());

        // And going one step further should be outside our reach
        path = path.getParentFile();
        assertDirNoWriteAccess(path);
        assertEquals(Environment.MEDIA_UNKNOWN, Environment.getExternalStorageState(path));
    }

    /**
     * Verify mount status for random paths.
     */
    public void testMountStatus() {
        assertEquals(Environment.MEDIA_UNKNOWN,
                Environment.getExternalStorageState(new File("/meow-should-never-exist")));

        // Internal data isn't a mount point
        assertEquals(Environment.MEDIA_UNKNOWN,
                Environment.getExternalStorageState(getContext().getCacheDir()));
    }

    /**
     * Verify that we have write access in our package-specific directories on
     * secondary storage devices, and it still has read-write access above them (except
     * /Android/[data|obb] dirs).
     */
    public void testSecondaryWalkingUpTreeReadWrite() throws Exception {
        final List<File> paths = getSecondaryPackageSpecificPaths(getContext());
        final String packageName = getContext().getPackageName();

        for (File path : paths) {
            assertNotNull("Valid media must be inserted during CTS", path);
            assertEquals("Valid media must be inserted during CTS", Environment.MEDIA_MOUNTED,
                    Environment.getExternalStorageState(path));

            assertTrue(path.getAbsolutePath().contains(packageName));

            // Walk up until we drop our package
            while (path.getAbsolutePath().contains(packageName)) {
                assertDirReadWriteAccess(path);
                path = path.getParentFile();
            }

            // Walk all the way up to root
            while (path != null) {
                if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState(path))) {
                    // /Android/data and /Android/obb is not write accessible on any volume
                    // (/storage/emulated/<user_id>/ or /storage/1234-ABCD/)
                    if (path.getAbsolutePath().endsWith("/Android/data")
                            || path.getAbsolutePath().endsWith("/Android/obb")) {
                        assertDirNoWriteAccess(path);
                    } else {
                        assertDirReadWriteAccess(path);
                    }
                } else {
                    assertDirNoWriteAccess(path);
                    assertDirNoWriteAccess(buildCommonChildDirs(path));
                }
                path = path.getParentFile();
            }
        }
    }

    /**
     * Verify that .nomedia is created correctly.
     */
    public void testVerifyNoMediaCreated() throws Exception {
        for (File file : getAllPackageSpecificPathsExceptMedia(getContext())) {
            deleteContents(file);
        }
        final List<File> paths = getAllPackageSpecificPathsExceptMedia(getContext());

        for (File path : paths) {
            MediaStore.scanFile(getContext().getContentResolver(), path);
        }

        // Require that .nomedia was created somewhere above each dir
        for (File path : paths) {
            assertNotNull("Valid media must be inserted during CTS", path);
            assertEquals("Valid media must be inserted during CTS", Environment.MEDIA_MOUNTED,
                    Environment.getExternalStorageState(path));

            final File start = path;

            boolean found = false;
            while (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState(path))) {
                final File test = new File(path, ".nomedia");
                if (test.exists()) {
                    found = true;
                    break;
                }
                path = path.getParentFile();
            }

            if (!found) {
                fail("Missing .nomedia file above package-specific directory " + start
                        + "; gave up at " + path);
            }
        }
    }

    /**
     * Secondary external storage mount points must always be read-only (unless mounted), per
     * CDD, <em>except</em> for the package specific directories tested by
     * {@link CommonExternalStorageTest#testAllPackageDirsWritable()}.
     */
    public void testSecondaryMountPoints() throws Exception {
        // Probe path could be /storage/emulated/0 or /storage/1234-5678
        final File probe = buildProbeFile(Environment.getExternalStorageDirectory());
        assertTrue(probe.createNewFile());

        final String userId = Integer.toString(android.os.Process.myUid() / 100000);
        final List<File> mountPaths = getMountPaths();
        for (File path : mountPaths) {
            // Mount points could be multi-user aware, so try probing both top
            // level and user-specific directory.
            final File userPath = new File(path, userId);

            final File testProbe = new File(path, probe.getName());
            final File testUserProbe = new File(userPath, probe.getName());

            if (testProbe.exists() || testUserProbe.exists()) {
                Log.d(TAG, "Primary external mountpoint " + path);
            } else {
                Log.d(TAG, "Other mountpoint " + path);
                if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState(path))) {
                    if (path.getAbsolutePath().endsWith("/Android/data")
                            || path.getAbsolutePath().endsWith("/Android/obb")) {
                        assertDirNoWriteAccess(path);
                    } else {
                        assertDirReadWriteAccess(path);
                        assertDirReadWriteAccess(buildCommonChildDirs(path));
                    }
                }
                else {
                    assertDirNoWriteAccess(path);
                    assertDirNoWriteAccess(userPath);
                }
            }
        }
    }

    public void testExternalStorageRename() throws Exception {
        final String name = "cts_" + System.nanoTime();

        // Stage some contents to move around
        File cur = Environment.getExternalStorageDirectory();
        try (FileOutputStream fos = new FileOutputStream(new File(cur, name))) {
            fos.write(42);
        }

        for (File next : new File[] {
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                getContext().getExternalCacheDir(),
                getContext().getExternalFilesDir(null),
                getContext().getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS),
                getContext().getExternalMediaDirs()[0],
        }) {
            next.mkdirs();

            final File before = new File(cur, name);
            final File after = new File(next, name);

            Log.v(TAG, "Moving " + before + " to " + after);
            // Os.rename will fail with EXDEV here, use renameTo which does copy delete behind the
            // scenes
            before.renameTo(after);

            cur = next;
        }

        // Make sure the data made the journey
        try (FileInputStream fis = new FileInputStream(new File(cur, name))) {
            assertEquals(42, fis.read());
        }
    }
}
