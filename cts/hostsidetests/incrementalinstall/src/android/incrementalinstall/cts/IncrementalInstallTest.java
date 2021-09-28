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

package android.incrementalinstall.cts;

import static android.incrementalinstall.common.Consts.SupportedComponents.COMPRESSED_NATIVE_COMPONENT;
import static android.incrementalinstall.common.Consts.SupportedComponents.DYNAMIC_ASSET_COMPONENT;
import static android.incrementalinstall.common.Consts.SupportedComponents.DYNAMIC_CODE_COMPONENT;
import static android.incrementalinstall.common.Consts.SupportedComponents.ON_CREATE_COMPONENT;
import static android.incrementalinstall.common.Consts.SupportedComponents.ON_CREATE_COMPONENT_2;
import static android.incrementalinstall.common.Consts.SupportedComponents.UNCOMPRESSED_NATIVE_COMPONENT;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.incrementalinstall.common.Consts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.Log;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.ZipUtil;
import com.android.tradefed.util.zip.CentralDirectoryInfo;
import com.android.tradefed.util.zip.EndCentralDirectoryInfo;

import com.google.common.collect.Lists;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

@RunWith(DeviceJUnit4ClassRunner.class)
public class IncrementalInstallTest extends BaseHostJUnit4Test {

    private static final String FEATURE_INCREMENTAL_DELIVERY =
            "android.software.incremental_delivery";
    private static final String INCREMENTAL_ARG = "--incremental";
    private static final String TEST_RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    private static final String VALIDATION_HELPER_PKG =
            "android.incrementalinstall.inrementaltestappvalidation";
    private static final String VALIDATION_HELPER_CLASS =
            VALIDATION_HELPER_PKG + ".AppValidationTest";
    private static final String VALIDATION_HELPER_METHOD = "testAppComponentsInvoked";
    private static final String INSTALLATION_TYPE_HELPER_METHOD = "testInstallationTypeAndVersion";

    private static final String TEST_APP_PACKAGE_NAME =
            "android.incrementalinstall.incrementaltestapp";
    private static final String TEST_APP_BASE_APK_NAME = "IncrementalTestApp.apk";
    // apk for zero version update test (has version 1).
    private static final String TEST_APP_BASE_APK_2_V1_NAME = "IncrementalTestApp2_v1.apk";
    // apk for version update test (has version 2).
    private static final String TEST_APP_BASE_APK_2_V2_NAME = "IncrementalTestApp2_v2.apk";
    private static final String TEST_APP_DYNAMIC_ASSET_NAME = "IncrementalTestAppDynamicAsset.apk";
    private static final String TEST_APP_DYNAMIC_CODE_NAME = "IncrementalTestAppDynamicCode.apk";
    private static final String TEST_APP_COMPRESSED_NATIVE_NAME =
            "IncrementalTestAppCompressedNativeLib.apk";
    private static final String TEST_APP_UNCOMPRESSED_NATIVE_NAME =
            "IncrementalTestAppUncompressedNativeLib.apk";

    private static final String SIG_SUFFIX = ".idsig";
    private static final String INSTALL_SUCCESS_OUTPUT = "Success";
    private static final long DEFAULT_TEST_TIMEOUT_MS = 60 * 1000L;
    private static final long DEFAULT_MAX_TIMEOUT_TO_OUTPUT_MS = 60 * 1000L; // 1min
    private final int TEST_APP_V1_VERSION = 1;
    private final int TEST_APP_V2_VERSION = 2;
    private CompatibilityBuildHelper mBuildHelper;

    @Before
    public void setup() throws Exception {
        assumeTrue(hasIncrementalFeature());
        mBuildHelper = new CompatibilityBuildHelper(getBuild());
        assumeTrue(adbBinarySupportsIncremental());
        uninstallApp(TEST_APP_PACKAGE_NAME);
        assertFalse(isPackageInstalled(TEST_APP_PACKAGE_NAME));
    }

    @After
    public void teardown() throws Exception {
        uninstallApp(TEST_APP_PACKAGE_NAME);
        assertFalse(isPackageInstalled(TEST_APP_PACKAGE_NAME));
    }

    @Test
    public void testBaseApkAdbInstall() throws Exception {
        assertTrue(
                installWithAdbInstaller(TEST_APP_BASE_APK_NAME).contains(INSTALL_SUCCESS_OUTPUT));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT);
    }

    @Test
    public void testBaseApkAdbUninstall() throws Exception {
        verifyInstallCommandSuccess(installWithAdbInstaller(TEST_APP_BASE_APK_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT);
        uninstallApp(TEST_APP_PACKAGE_NAME);
        verifyPackageNotInstalled(TEST_APP_PACKAGE_NAME);
    }

    @Test
    public void testBaseApkMissingSignatureAdbInstall() throws Exception {
        String newApkName = String.format("base%d.apk", new Random().nextInt());
        // Create a copy of original apk but not its idsig.
        copyTestFile(TEST_APP_BASE_APK_NAME, null, newApkName);
        // Make sure it installs.
        assertTrue(
                installWithAdbInstaller(TEST_APP_BASE_APK_NAME).contains(INSTALL_SUCCESS_OUTPUT));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT);

    }

    @Test
    public void testBaseApkInvalidSignatureAdbInstall() throws Exception {
        String newApkName = String.format("base%d.apk", new Random().nextInt());
        String sigSuffix = ".idsig";
        File destApk = copyTestFile(TEST_APP_BASE_APK_NAME, null, newApkName);
        copyTestFile(TEST_APP_BASE_APK_NAME + sigSuffix, destApk.getParentFile(),
                newApkName + sigSuffix);
        try (RandomAccessFile raf = new RandomAccessFile(
                getFilePathFromBuildInfo(newApkName + sigSuffix), "rw")) {
            // Contaminate signature by complementing a random byte.
            int byteToContaminate = new Random().nextInt((int) raf.length());
            LogUtil.CLog.logAndDisplay(Log.LogLevel.INFO,
                    "CtsIncrementalInstallHostTestCases#testBaseApkInvalidSignatureAdbInstall: "
                            + "Contaminating byte: %d of signature: %s",
                    byteToContaminate, TEST_APP_BASE_APK_NAME + sigSuffix);
            raf.seek(byteToContaminate);
            raf.writeByte((byte) (~raf.readByte()));
        }
        verifyInstallCommandFailure(installWithAdbInstaller(newApkName));
        verifyPackageNotInstalled(TEST_APP_PACKAGE_NAME);
    }

    @Test
    public void testDynamicAssetMultiSplitAdbInstall() throws Exception {
        verifyInstallCommandSuccess(
                installWithAdbInstaller(TEST_APP_BASE_APK_NAME, TEST_APP_DYNAMIC_ASSET_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT, DYNAMIC_ASSET_COMPONENT);
    }

    @Test
    public void testDynamicCodeMultiSplitAdbInstall() throws Exception {
        verifyInstallCommandSuccess(
                installWithAdbInstaller(TEST_APP_BASE_APK_NAME, TEST_APP_DYNAMIC_CODE_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT, DYNAMIC_CODE_COMPONENT);
    }

    @Test
    public void testCompressedNativeLibMultiSplitAdbInstall() throws Exception {
        assertTrue(checkNativeLibInApkCompression(TEST_APP_COMPRESSED_NATIVE_NAME,
                "libcompressednativeincrementaltest.so", true));
        verifyInstallCommandSuccess(
                installWithAdbInstaller(TEST_APP_BASE_APK_NAME, TEST_APP_COMPRESSED_NATIVE_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT, COMPRESSED_NATIVE_COMPONENT);
    }

    @Test
    public void testUncompressedNativeLibMultiSplitAdbInstall() throws Exception {
        assertTrue(checkNativeLibInApkCompression(TEST_APP_COMPRESSED_NATIVE_NAME,
                "libuncompressednativeincrementaltest.so", false));
        verifyInstallCommandSuccess(
                installWithAdbInstaller(TEST_APP_BASE_APK_NAME, TEST_APP_UNCOMPRESSED_NATIVE_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT,
                UNCOMPRESSED_NATIVE_COMPONENT);
    }

    @Test
    public void testAddSplitToExistingInstallNonIfsMigration() throws Exception {
        verifyInstallCommandSuccess(installWithAdbInstaller(TEST_APP_BASE_APK_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT);
        // Adb cannot add a split to an existing install, so we'll use pm to install just the
        // dynamic code split.
        String deviceLocalPath = "/data/local/tmp/";
        getDevice().executeAdbCommand("push", getFilePathFromBuildInfo(TEST_APP_DYNAMIC_CODE_NAME),
                deviceLocalPath);
        getDevice().executeShellCommand(String.format("pm install -p %s %s", TEST_APP_PACKAGE_NAME,
                deviceLocalPath + TEST_APP_DYNAMIC_CODE_NAME));
        // Verify IFS->NonIFS migration.
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ false,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT, DYNAMIC_CODE_COMPONENT);
    }

    @Test
    public void testZeroVersionUpdateAdbInstall() throws Exception {
        verifyInstallCommandSuccess(installWithAdbInstaller(TEST_APP_BASE_APK_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT);
        // Install second implementation of app with the same version code.
        verifyInstallCommandSuccess(installWithAdbInstaller(TEST_APP_BASE_APK_2_V1_NAME));
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT_2);
    }

    @Test
    public void testVersionUpdateAdbInstall() throws Exception {
        verifyInstallCommandSuccess(installWithAdbInstaller(TEST_APP_BASE_APK_NAME));
        verifyPackageInstalled(TEST_APP_PACKAGE_NAME);
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V1_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT);
        // Install second implementation of app with the same version code.
        verifyInstallCommandSuccess(installWithAdbInstaller(TEST_APP_BASE_APK_2_V2_NAME));
        verifyInstallationTypeAndVersion(TEST_APP_PACKAGE_NAME, /* isIncfs= */ true,
                TEST_APP_V2_VERSION);
        validateAppLaunch(TEST_APP_PACKAGE_NAME, ON_CREATE_COMPONENT_2);
    }

    private void verifyInstallationTypeAndVersion(String packageName, boolean isIncfs,
            int versionCode) throws Exception {
        Map<String, String> args = new HashMap<>();
        args.put(Consts.PACKAGE_TO_LAUNCH_TAG, packageName);
        args.put(Consts.IS_INCFS_INSTALLATION_TAG, Boolean.toString(isIncfs));
        args.put(Consts.INSTALLED_VERSION_CODE_TAG, Integer.toString(versionCode));
        boolean result = runDeviceTests(
                getDevice(), TEST_RUNNER, VALIDATION_HELPER_PKG, VALIDATION_HELPER_CLASS,
                INSTALLATION_TYPE_HELPER_METHOD,
                null, DEFAULT_TEST_TIMEOUT_MS, DEFAULT_MAX_TIMEOUT_TO_OUTPUT_MS,
                0L, true, false, args);
        assertTrue(String.format(
                "Failed: %s#%s failed while verifying installation type for package: %s",
                VALIDATION_HELPER_CLASS, INSTALLATION_TYPE_HELPER_METHOD, packageName), result);
    }

    private void validateAppLaunch(String packageName, String... componentsToCheck)
            throws Exception {
        Map<String, String> args = new HashMap<>();
        args.put(Consts.PACKAGE_TO_LAUNCH_TAG, packageName);
        args.put(Consts.LOADED_COMPONENTS_TAG, String.join(",", componentsToCheck));
        List<String> notLoadedComponents = Lists.newArrayList(
                Consts.SupportedComponents.getAllComponents());
        notLoadedComponents.removeAll(Lists.newArrayList(componentsToCheck));
        args.put(Consts.NOT_LOADED_COMPONENTS_TAG, String.join(",", notLoadedComponents));

        boolean result = runDeviceTests(
                getDevice(), TEST_RUNNER, VALIDATION_HELPER_PKG, VALIDATION_HELPER_CLASS,
                VALIDATION_HELPER_METHOD,
                null, DEFAULT_TEST_TIMEOUT_MS, DEFAULT_MAX_TIMEOUT_TO_OUTPUT_MS,
                0L, true, false, args);
        assertTrue(String.format("Failed: %s#%s failed while validating package: %s",
                VALIDATION_HELPER_CLASS, VALIDATION_HELPER_METHOD, packageName), result);
    }

    private void verifyPackageInstalled(String packageName) throws Exception {
        assertTrue(String.format("Failed: %s is not installed.", packageName),
                isPackageInstalled(packageName));
    }

    private void verifyPackageNotInstalled(String packageName) throws Exception {
        assertFalse(
                String.format("Failed: %s is installed, when it shouldn't have been.", packageName),
                isPackageInstalled(packageName));
    }

    private String installWithAdbInstaller(String... filenames)
            throws Exception {
        return installWithAdbInstaller(/* shouldUpdate= */ false, filenames);
    }

    /**
     * @return stderr+stdout of the adb installation.
     */
    private String installWithAdbInstaller(boolean shouldUpdate, String... filenames)
            throws Exception {
        assertTrue(filenames.length > 0);
        String installMultipleArg =
                filenames.length > 1 ? "install-multiple" : "";
        String updateArg =
                shouldUpdate ? "-r" : "";
        List<String> adbCmd = new ArrayList<>();
        adbCmd.add("adb");
        adbCmd.add("-s");
        adbCmd.add(getDevice().getSerialNumber());
        adbCmd.add("install");
        adbCmd.add(updateArg);
        adbCmd.add(INCREMENTAL_ARG);
        adbCmd.add(installMultipleArg);
        adbCmd.addAll(getFilePathsFromBuildInfo(filenames));

        // Using runUtil instead of executeAdbCommand() because the latter doesn't provide the
        // option to get stderr or redirect stderr to stdout.
        File outFile = FileUtil.createTempFile("stdoutredirect", ".txt");
        OutputStream stdout = new FileOutputStream(outFile);
        RunUtil runUtil = new RunUtil();
        runUtil.setRedirectStderrToStdout(true);
        runUtil.runTimedCmd(DEFAULT_TEST_TIMEOUT_MS, stdout, /* stderr= */ null,
                adbCmd.toArray(new String[adbCmd.size()]));
        return FileUtil.readStringFromFile(outFile);
    }

    private List<String> getFilePathsFromBuildInfo(String... filenames) throws IOException {
        List<String> filePaths = new ArrayList<>();
        for (String filename : filenames) {
            filePaths.add(getFilePathFromBuildInfo(filename));
        }
        return filePaths;
    }

    private String getFilePathFromBuildInfo(String filename) throws IOException {
        return mBuildHelper.getTestFile(filename).getAbsolutePath();
    }

    private File copyTestFile(String sourceFilename, File destPath, String destFilename) throws IOException {
        File source = new File(getFilePathFromBuildInfo(sourceFilename));
        File dest = new File(destPath != null ? destPath : source.getParentFile(), destFilename);
        FileUtil.copyFile(source, dest);
        return dest;
    }

    private void uninstallApp(String packageName) throws Exception {
        getDevice().uninstallPackage(packageName);
    }

    private boolean checkNativeLibInApkCompression(String apkName, String nativeLibName,
            boolean compression)
            throws Exception {
        boolean conformsToCompressionStatus = true;
        File apk = new File(getFilePathFromBuildInfo(apkName));
        EndCentralDirectoryInfo endCentralDirectoryInfo = new EndCentralDirectoryInfo(apk);
        List<CentralDirectoryInfo> centralDirectoryInfos = ZipUtil.getZipCentralDirectoryInfos(apk,
                endCentralDirectoryInfo, endCentralDirectoryInfo.getCentralDirOffset());
        for (CentralDirectoryInfo directoryInfo : centralDirectoryInfos) {
            if (directoryInfo.getFileName().endsWith(nativeLibName)) {
                // No early exit. Multilib build will have the same .so file for different
                // architectures, check all of them.
                conformsToCompressionStatus &= compression ==
                        (directoryInfo.getCompressionMethod() != 0); // 0 is compression NONE
            }
        }
        return conformsToCompressionStatus;
    }

    private void verifyInstallCommandSuccess(String adbOutput) {
        logInstallCommandOutput(adbOutput);
        assertTrue(adbOutput.contains(INSTALL_SUCCESS_OUTPUT));
    }

    private void verifyInstallCommandFailure(String adbOutput) {
        logInstallCommandOutput(adbOutput);
        assertFalse(adbOutput.contains(INSTALL_SUCCESS_OUTPUT));
    }

    private void logInstallCommandOutput(String adbOutput) {
        // Get calling method for logging
        StackTraceElement[] stacktrace = Thread.currentThread().getStackTrace();
        StackTraceElement e = stacktrace[3]; //test method
        String methodName = e.getMethodName();
        LogUtil.CLog.logAndDisplay(Log.LogLevel.INFO,
                String.format("CtsIncrementalInstallHostTestCases#%s: adb install output: %s",
                        methodName, adbOutput));
    }

    private boolean hasIncrementalFeature() throws Exception {
        return hasDeviceFeature(FEATURE_INCREMENTAL_DELIVERY);
    }

    private boolean adbBinarySupportsIncremental() throws Exception {
        return !installWithAdbInstaller(TEST_APP_BASE_APK_NAME).contains(
                "Unknown option --incremental");
    }
}
