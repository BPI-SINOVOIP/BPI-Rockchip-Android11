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

package android.scopedstorage.cts.host;

import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Runs the ScopedStorageTest tests.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class ScopedStorageHostTest extends BaseHostJUnit4Test {
    private boolean mIsExternalStorageSetup = false;

    /**
     * Runs the given phase of ScopedStorageTest by calling into the device.
     * Throws an exception if the test phase fails.
     */
    void runDeviceTest(String phase) throws Exception {
        assertTrue(runDeviceTests("android.scopedstorage.cts",
                "android.scopedstorage.cts.ScopedStorageTest", phase));

    }

    /**
     * Runs the given phase of ScopedStorageTest by calling into the device with {@code
     * --no-isolated-storage} flag.
     * Throws an exception if the test phase fails.
     */
    void runDeviceTestWithDisabledIsolatedStorage(String phase) throws Exception {
        runDeviceTests(new DeviceTestRunOptions("android.scopedstorage.cts")
            .setDevice(getDevice())
            .setTestClassName("android.scopedstorage.cts.ScopedStorageTest")
            .setTestMethodName(phase)
            .setDisableIsolatedStorage(true));
    }

    String executeShellCommand(String cmd) throws Exception {
        return getDevice().executeShellCommand(cmd);
    }

    private void setupExternalStorage() throws Exception {
        if (!mIsExternalStorageSetup) {
            runDeviceTest("setupExternalStorage");
            mIsExternalStorageSetup = true;
        }
    }

    @Before
    public void setup() throws Exception {
        setupExternalStorage();
        executeShellCommand("mkdir /sdcard/Android/data/com.android.shell -m 2770");
        executeShellCommand("mkdir /sdcard/Android/data/com.android.shell/files -m 2770");
    }

    @Before
    public void revokeStoragePermissions() throws Exception {
        revokePermissions("android.permission.WRITE_EXTERNAL_STORAGE",
                "android.permission.READ_EXTERNAL_STORAGE");
    }

    @After
    public void tearDown() throws Exception {
        executeShellCommand("rm -r /sdcard/Android/data/com.android.shell");
    }

    @Test
    public void testTypePathConformity() throws Exception {
        runDeviceTest("testTypePathConformity");
    }

    @Test
    public void testCreateFileInAppExternalDir() throws Exception {
        runDeviceTest("testCreateFileInAppExternalDir");
    }

    @Test
    public void testCreateFileInOtherAppExternalDir() throws Exception {
        runDeviceTest("testCreateFileInOtherAppExternalDir");
    }

    @Test
    public void testReadWriteFilesInOtherAppExternalDir() throws Exception {
        runDeviceTest("testReadWriteFilesInOtherAppExternalDir");
    }

    @Test
    public void testContributeMediaFile() throws Exception {
        runDeviceTest("testContributeMediaFile");
    }

    @Test
    public void testCreateAndDeleteEmptyDir() throws Exception {
        runDeviceTest("testCreateAndDeleteEmptyDir");
    }

    @Test
    public void testCantDeleteOtherAppsContents() throws Exception {
        runDeviceTest("testCantDeleteOtherAppsContents");
    }

    @Test
    public void testDeleteAlreadyUnlinkedFile() throws Exception {
        runDeviceTest("testDeleteAlreadyUnlinkedFile");

    }
    @Test
    public void testOpendirRestrictions() throws Exception {
        runDeviceTest("testOpendirRestrictions");
    }

    @Test
    public void testLowLevelFileIO() throws Exception {
        runDeviceTest("testLowLevelFileIO");
    }

    @Test
    public void testListDirectoriesWithMediaFiles() throws Exception {
        runDeviceTest("testListDirectoriesWithMediaFiles");
    }

    @Test
    public void testListDirectoriesWithNonMediaFiles() throws Exception {
        runDeviceTest("testListDirectoriesWithNonMediaFiles");
    }

    @Test
    public void testListFilesFromExternalFilesDirectory() throws Exception {
        runDeviceTest("testListFilesFromExternalFilesDirectory");
    }

    @Test
    public void testListFilesFromExternalMediaDirectory() throws Exception {
        runDeviceTest("testListFilesFromExternalMediaDirectory");
    }

    @Test
    public void testListUnsupportedFileType() throws Exception {
        runDeviceTest("testListUnsupportedFileType");
    }

    @Test
    public void testMetaDataRedaction() throws Exception {
        runDeviceTest("testMetaDataRedaction");
    }

    @Test
    public void testVfsCacheConsistency() throws Exception {
        runDeviceTest("testOpenFilePathFirstWriteContentResolver");
        runDeviceTest("testOpenContentResolverFirstWriteContentResolver");
        runDeviceTest("testOpenFilePathFirstWriteFilePath");
        runDeviceTest("testOpenContentResolverFirstWriteFilePath");
        runDeviceTest("testOpenContentResolverWriteOnly");
        runDeviceTest("testOpenContentResolverDup");
        runDeviceTest("testContentResolverDelete");
        runDeviceTest("testContentResolverUpdate");
        runDeviceTest("testOpenContentResolverClose");
    }

    @Test
    public void testCaseInsensitivity() throws Exception {
        runDeviceTest("testCreateLowerCaseDeleteUpperCase");
        runDeviceTest("testCreateUpperCaseDeleteLowerCase");
        runDeviceTest("testCreateMixedCaseDeleteDifferentMixedCase");
        runDeviceTest("testAndroidDataObbDoesNotForgetMount");
        runDeviceTest("testCacheConsistencyForCaseInsensitivity");
    }

    @Test
    public void testCallingIdentityCacheInvalidation() throws Exception {
        // General IO access
        runDeviceTest("testReadStorageInvalidation");
        runDeviceTest("testWriteStorageInvalidation");
        // File manager access
        runDeviceTest("testManageStorageInvalidation");
        // Default gallery
        runDeviceTest("testWriteImagesInvalidation");
        runDeviceTest("testWriteVideoInvalidation");
        // EXIF access
        runDeviceTest("testAccessMediaLocationInvalidation");

        runDeviceTest("testAppUpdateInvalidation");
        runDeviceTest("testAppReinstallInvalidation");
    }

    @Test
    public void testRenameFile() throws Exception {
        runDeviceTest("testRenameFile");
    }

    @Test
    public void testRenameFileType() throws Exception {
        runDeviceTest("testRenameFileType");
    }

    @Test
    public void testRenameAndReplaceFile() throws Exception {
        runDeviceTest("testRenameAndReplaceFile");
    }

    @Test
    public void testRenameFileNotOwned() throws Exception {
        runDeviceTest("testRenameFileNotOwned");
    }

    @Test
    public void testRenameDirectory() throws Exception {
        runDeviceTest("testRenameDirectory");
    }

    @Test
    public void testRenameDirectoryNotOwned() throws Exception {
        runDeviceTest("testRenameDirectoryNotOwned");
    }

    @Test
    public void testRenameEmptyDirectory() throws Exception {
        runDeviceTest("testRenameEmptyDirectory");
    }

    @Test
    public void testSystemGalleryAppHasFullAccessToImages() throws Exception {
        runDeviceTest("testSystemGalleryAppHasFullAccessToImages");
    }

    @Test
    public void testSystemGalleryAppHasNoFullAccessToAudio() throws Exception {
        runDeviceTest("testSystemGalleryAppHasNoFullAccessToAudio");
    }

    @Test
    public void testSystemGalleryCanRenameImagesAndVideos() throws Exception {
        runDeviceTest("testSystemGalleryCanRenameImagesAndVideos");
    }

    @Test
    public void testManageExternalStorageCanCreateFilesAnywhere() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testManageExternalStorageCanCreateFilesAnywhere");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testManageExternalStorageCanDeleteOtherAppsContents() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testManageExternalStorageCanDeleteOtherAppsContents");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testManageExternalStorageReaddir() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testManageExternalStorageReaddir");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testManageExternalStorageCanRenameOtherAppsContents() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testManageExternalStorageCanRenameOtherAppsContents");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testManageExternalStorageCantReadWriteOtherAppExternalDir() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testManageExternalStorageCantReadWriteOtherAppExternalDir");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testCantAccessOtherAppsContents() throws Exception {
        runDeviceTest("testCantAccessOtherAppsContents");
    }

    @Test
    public void testCanCreateHiddenFile() throws Exception {
        runDeviceTest("testCanCreateHiddenFile");
    }

    @Test
    public void testCanRenameHiddenFile() throws Exception {
        runDeviceTest("testCanRenameHiddenFile");
    }

    @Test
    public void testHiddenDirectory() throws Exception {
        runDeviceTest("testHiddenDirectory");
    }

    @Test
    public void testHiddenDirectory_nomedia() throws Exception {
        runDeviceTest("testHiddenDirectory_nomedia");
    }

    @Test
    public void testListHiddenFile() throws Exception {
        runDeviceTest("testListHiddenFile");
    }

    @Test
    public void testOpenPendingAndTrashed() throws Exception {
        runDeviceTest("testOpenPendingAndTrashed");
    }

    @Test
    public void testDeletePendingAndTrashed() throws Exception {
        runDeviceTest("testDeletePendingAndTrashed");
    }

    @Test
    public void testListPendingAndTrashed() throws Exception {
        runDeviceTest("testListPendingAndTrashed");
    }

    @Test
    public void testCanCreateDefaultDirectory() throws Exception {
        runDeviceTest("testCanCreateDefaultDirectory");
    }

    @Test
    public void testManageExternalStorageQueryOtherAppsFile() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testManageExternalStorageQueryOtherAppsFile");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testSystemGalleryQueryOtherAppsFiles() throws Exception {
        runDeviceTest("testSystemGalleryQueryOtherAppsFiles");
    }

    @Test
    public void testQueryOtherAppsFiles() throws Exception {
        runDeviceTest("testQueryOtherAppsFiles");
    }

    @Test
    public void testSystemGalleryCanRenameImageAndVideoDirs() throws Exception {
        runDeviceTest("testSystemGalleryCanRenameImageAndVideoDirs");
    }

    @Test
    public void testCreateCanRestoreDeletedRowId() throws Exception {
        runDeviceTest("testCreateCanRestoreDeletedRowId");
    }

    @Test
    public void testRenameCanRestoreDeletedRowId() throws Exception {
        runDeviceTest("testRenameCanRestoreDeletedRowId");
    }

    @Test
    public void testCantCreateOrRenameFileWithInvalidName() throws Exception {
        runDeviceTest("testCantCreateOrRenameFileWithInvalidName");
    }

    @Test
    public void testPendingFromFuse() throws Exception {
        runDeviceTest("testPendingFromFuse");
    }

    @Test
    public void testOpenOtherPendingFilesFromFuse() throws Exception {
        grantPermissions("android.permission.READ_EXTERNAL_STORAGE");
        try {
            runDeviceTest("testOpenOtherPendingFilesFromFuse");
        } finally {
            revokePermissions("android.permission.READ_EXTERNAL_STORAGE");
        }
    }

    @Test
    public void testCantSetAttrOtherAppsFile() throws Exception {
        runDeviceTest("testCantSetAttrOtherAppsFile");
    }

    @Test
    public void testAccess_file() throws Exception {
        grantPermissions("android.permission.READ_EXTERNAL_STORAGE");
        try {
            runDeviceTest("testAccess_file");
        } finally {
            revokePermissions("android.permission.READ_EXTERNAL_STORAGE");
        }
    }

    @Test
    public void testAccess_directory() throws Exception {
        grantPermissions("android.permission.READ_EXTERNAL_STORAGE",
                "android.permission.WRITE_EXTERNAL_STORAGE");
        try {
            runDeviceTest("testAccess_directory");
        } finally {
            revokePermissions("android.permission.READ_EXTERNAL_STORAGE",
                    "android.permission.WRITE_EXTERNAL_STORAGE");
        }
    }

    @Test
    public void testAndroidMedia() throws Exception {
        grantPermissions("android.permission.READ_EXTERNAL_STORAGE");
        try {
            runDeviceTest("testAndroidMedia");
        } finally {
            revokePermissions("android.permission.READ_EXTERNAL_STORAGE");
        }
    }

    @Test
    public void testWallpaperApisNoPermission() throws Exception {
        runDeviceTest("testWallpaperApisNoPermission");
    }

    @Test
    public void testWallpaperApisReadExternalStorage() throws Exception {
        grantPermissions("android.permission.READ_EXTERNAL_STORAGE");
        try {
            runDeviceTest("testWallpaperApisReadExternalStorage");
        } finally {
            revokePermissions("android.permission.READ_EXTERNAL_STORAGE");
        }
    }

    @Test
    public void testWallpaperApisManageExternalStorageAppOp() throws Exception {
        allowAppOps("android:manage_external_storage");
        try {
            runDeviceTest("testWallpaperApisManageExternalStorageAppOp");
        } finally {
            denyAppOps("android:manage_external_storage");
        }
    }

    @Test
    public void testWallpaperApisManageExternalStoragePrivileged() throws Exception {
        runDeviceTest("testWallpaperApisManageExternalStoragePrivileged");
    }

    @Test
    public void testNoIsolatedStorageInstrumentationFlag() throws Exception {
        runDeviceTestWithDisabledIsolatedStorage("testNoIsolatedStorageCanCreateFilesAnywhere");
        runDeviceTestWithDisabledIsolatedStorage(
                "testNoIsolatedStorageCantReadWriteOtherAppExternalDir");
        runDeviceTestWithDisabledIsolatedStorage("testNoIsolatedStorageStorageReaddir");
        runDeviceTestWithDisabledIsolatedStorage("testNoIsolatedStorageQueryOtherAppsFile");

        // Check that appop is revoked after instrumentation is over.
        runDeviceTest("testCreateFileInAppExternalDir");
        runDeviceTest("testCreateFileInOtherAppExternalDir");
        runDeviceTest("testReadWriteFilesInOtherAppExternalDir");
    }

    @Test
    public void testRenameFromShell() throws Exception {
        final ITestDevice device = getDevice();
        final boolean isAdbRoot = device.isAdbRoot() ? true : false;
        try {
            if (isAdbRoot) {
                device.disableAdbRoot();
            }
            runDeviceTest("testRenameFromShell");
        } finally {
            if (isAdbRoot) {
                device.enableAdbRoot();
            }
        }
    }

    private void grantPermissions(String... perms) throws Exception {
        for (String perm : perms) {
            executeShellCommand("pm grant android.scopedstorage.cts " + perm);
        }
    }

    private void revokePermissions(String... perms) throws Exception {
        for (String perm : perms) {
            executeShellCommand("pm revoke android.scopedstorage.cts " + perm);
        }
    }

    private void allowAppOps(String... ops) throws Exception {
        for (String op : ops) {
            executeShellCommand("cmd appops set --uid android.scopedstorage.cts " + op + " allow");
        }
    }

    private void denyAppOps(String... ops) throws Exception {
        for (String op : ops) {
            executeShellCommand("cmd appops set --uid android.scopedstorage.cts " + op + " deny");
        }
    }
}
