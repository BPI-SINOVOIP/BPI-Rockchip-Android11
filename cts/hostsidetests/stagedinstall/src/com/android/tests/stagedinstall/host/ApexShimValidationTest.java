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
 * limitations under the License
 */

package com.android.tests.stagedinstall.host;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeThat;

import android.platform.test.annotations.LargeTest;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.AaptParser;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.ZipUtil;

import org.hamcrest.CoreMatchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;
import java.util.stream.Collectors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Tests to validate that only what is considered a correct shim apex can be installed.
 *
 * <p>Shim apex is considered correct iff:
 * <ul>
 *     <li>It doesn't have any pre or post install hooks.</li>
 *     <li>It's {@code apex_payload.img} contains only a regular text file called
 *         {@code hash.txt}.</li>
 *     <li>It's {@code sha512} hash is whitelisted in the {@code hash.txt} of pre-installed on the
 *         {@code /system} partition shim apex.</li>
 * </ul>
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ApexShimValidationTest extends BaseHostJUnit4Test {

    private static final String SHIM_APEX_PACKAGE_NAME = "com.android.apex.cts.shim";
    private static final String SHIM_APK_CODE_PATH_PREFIX = "/apex/" + SHIM_APEX_PACKAGE_NAME + "/";
    private static final String STAGED_INSTALL_TEST_FILE_NAME = "StagedInstallTest.apk";
    private static final String APEX_FILE_SUFFIX = ".apex";
    private static final String DEAPEXER_ZIP_FILE_NAME = "deapexer.zip";
    private static final String DEAPEXING_FOLDER_NAME = "deapexing_";
    private static final String DEAPEXER_FILE_NAME = "deapexer";
    private static final String DEBUGFS_STATIC_FILE_NAME = "debugfs_static";

    private static final long DEFAULT_RUN_TIMEOUT_MS = 30 * 1000L;

    private static final List<String> ALLOWED_SHIM_PACKAGE_NAMES = Arrays.asList(
            "com.android.cts.ctsshim", "com.android.cts.priv.ctsshim");

    private File mDeapexingDir;
    private File mDeapexerZip;
    private File mAllApexesZip;

    /**
     * Runs the given phase of a test by calling into the device.
     * Throws an exception if the test phase fails.
     * <p>
     * For example, <code>runPhase("testInstallStagedApkCommit");</code>
     */
    private void runPhase(String phase) throws Exception {
        assertThat(runDeviceTests("com.android.tests.stagedinstall",
                "com.android.tests.stagedinstall.ApexShimValidationTest",
                phase)).isTrue();
    }

    private void cleanUp() throws Exception {
        assertThat(runDeviceTests("com.android.tests.stagedinstall",
                "com.android.tests.stagedinstall.StagedInstallTest",
                "cleanUp")).isTrue();
        if (mDeapexingDir != null) {
            FileUtil.recursiveDelete(mDeapexingDir);
        }
    }

    @Before
    public void setUp() throws Exception {
        final String updatable = getDevice().getProperty("ro.apex.updatable");
        assumeThat("Device doesn't support updating APEX", updatable, CoreMatchers.equalTo("true"));
        cleanUp();
        mDeapexerZip = getTestInformation().getDependencyFile(DEAPEXER_ZIP_FILE_NAME, false);
        mAllApexesZip = getTestInformation().getDependencyFile(STAGED_INSTALL_TEST_FILE_NAME,
                false);
    }

    @After
    public void tearDown() throws Exception {
        cleanUp();
    }

    @Test
    public void testShimApexIsPreInstalled() throws Exception {
        boolean isShimApexPreInstalled =
                getDevice().getActiveApexes().stream().anyMatch(
                        apex -> apex.name.equals(SHIM_APEX_PACKAGE_NAME));
        assertWithMessage("Shim APEX is not pre-installed").that(
                isShimApexPreInstalled).isTrue();
    }

    @Test
    public void testPackageNameOfShimApkIsAllowed() throws Exception {
        final List<String> shimPackages = getDevice().getAppPackageInfos().stream()
                .filter(pkg -> pkg.getCodePath().startsWith(SHIM_APK_CODE_PATH_PREFIX))
                .map(pkg -> pkg.getPackageName()).collect(Collectors.toList());
        assertWithMessage("Packages in the shim apex are not allowed")
                .that(shimPackages).containsExactlyElementsIn(ALLOWED_SHIM_PACKAGE_NAMES);
    }

    /**
     * Deapexing all the apexes bundled in the staged install test. Verifies the package name of
     * shim apk in the apex.
     */
    @Test
    public void testPackageNameOfShimApkInAllBundledApexesIsAllowed() throws Exception {
        mDeapexingDir = FileUtil.createTempDir(DEAPEXING_FOLDER_NAME);
        final File deapexer = extractDeapexer(mDeapexingDir);
        final File debugfs = new File(mDeapexingDir, DEBUGFS_STATIC_FILE_NAME);
        final List<File> apexes = extractApexes(mDeapexingDir);
        for (File apex : apexes) {
            final File outDir = new File(apex.getParent(), apex.getName().substring(
                    0, apex.getName().length() - APEX_FILE_SUFFIX.length()));
            try {
                runDeapexerExtract(deapexer, debugfs, apex, outDir);
                final List<File> apkFiles = FileUtil.findFiles(outDir, ".+\\.apk").stream()
                        .map(str -> new File(str)).collect(Collectors.toList());
                for (File apkFile : apkFiles) {
                    final AaptParser parser = AaptParser.parse(apkFile);
                    assertWithMessage("Apk " + apkFile + " in apex " + apex + " is not valid")
                            .that(parser).isNotNull();
                    assertWithMessage("Apk " + apkFile + " in apex " + apex
                            + " has incorrect package name " + parser.getPackageName())
                            .that(ALLOWED_SHIM_PACKAGE_NAMES).contains(parser.getPackageName());
                }
            } finally {
                FileUtil.recursiveDelete(outDir);
            }
        }
    }

    @Test
    @LargeTest
    public void testRejectsApexWithAdditionalFile() throws Exception {
        runPhase("testRejectsApexWithAdditionalFile_Commit");
        getDevice().reboot();
        runPhase("testInstallRejected_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testRejectsApexWithAdditionalFolder() throws Exception {
        runPhase("testRejectsApexWithAdditionalFolder_Commit");
        getDevice().reboot();
        runPhase("testInstallRejected_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testRejectsApexWithPostInstallHook() throws Exception {
        runPhase("testRejectsApexWithPostInstallHook_Commit");
        getDevice().reboot();
        runPhase("testInstallRejected_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testRejectsApexWithPreInstallHook() throws Exception {
        runPhase("testRejectsApexWithPreInstallHook_Commit");
        getDevice().reboot();
        runPhase("testInstallRejected_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testRejectsApexWrongSHA() throws Exception {
        runPhase("testRejectsApexWrongSHA_Commit");
        getDevice().reboot();
        runPhase("testInstallRejected_VerifyPostReboot");
    }

    /**
     * Extracts {@link #DEAPEXER_ZIP_FILE_NAME} into the destination folder. Updates executable
     * attribute for the binaries of deapexer and debugfs_static.
     *
     * @param destDir A tmp folder for the deapexing.
     * @return the deapexer file.
     */
    private File extractDeapexer(File destDir) throws IOException {
        ZipUtil.extractZip(new ZipFile(mDeapexerZip), destDir);
        final File deapexer = FileUtil.findFile(destDir, DEAPEXER_FILE_NAME);
        assertWithMessage("Can't find " + DEAPEXER_FILE_NAME + " binary file")
                .that(deapexer).isNotNull();
        deapexer.setExecutable(true);
        final File debugfs = FileUtil.findFile(destDir, DEBUGFS_STATIC_FILE_NAME);
        assertWithMessage("Can't find " + DEBUGFS_STATIC_FILE_NAME + " binary file")
                .that(debugfs).isNotNull();
        debugfs.setExecutable(true);
        return deapexer;
    }

    /**
     * Extracts all bundled apex files from {@link #STAGED_INSTALL_TEST_FILE_NAME} into the
     * destination folder.
     *
     * @param destDir A tmp folder for the deapexing.
     * @return A list of apex files.
     */
    private List<File> extractApexes(File destDir) throws IOException {
        final List<File> apexes = new ArrayList<>();
        final ZipFile apexZip = new ZipFile(mAllApexesZip);
        final Enumeration<? extends ZipEntry> entries = apexZip.entries();
        while (entries.hasMoreElements()) {
            final ZipEntry entry = entries.nextElement();
            if (entry.isDirectory() || !entry.getName().matches(
                    SHIM_APEX_PACKAGE_NAME + ".*\\" + APEX_FILE_SUFFIX)) {
                continue;
            }
            final File apex = new File(destDir, entry.getName());
            apex.getParentFile().mkdirs();
            FileUtil.writeToFile(apexZip.getInputStream(entry), apex);
            apexes.add(apex);
        }
        assertWithMessage("No apex file in the " + mAllApexesZip)
                .that(apexes).isNotEmpty();
        return apexes;
    }

    /**
     * Extracts all contents of the apex file into the {@code outDir} using the deapexer.
     *
     * @param deapexer The deapexer file.
     * @param debugfs The debugfs file.
     * @param apex The apex file to be extracted.
     * @param outDir The out folder.
     */
    private void runDeapexerExtract(File deapexer, File debugfs, File apex, File outDir) {
        final RunUtil runUtil = new RunUtil();
        final String os = System.getProperty("os.name").toLowerCase();
        final boolean isMacOs = (os.startsWith("mac") || os.startsWith("darwin"));
        if (isMacOs) {
            runUtil.setEnvVariable("DYLD_LIBRARY_PATH", mDeapexingDir.getAbsolutePath());
        } else {
            runUtil.setEnvVariable("LD_LIBRARY_PATH", mDeapexingDir.getAbsolutePath());
        }
        final CommandResult result = runUtil.runTimedCmd(DEFAULT_RUN_TIMEOUT_MS,
                deapexer.getAbsolutePath(),
                "--debugfs_path",
                debugfs.getAbsolutePath(),
                "extract",
                apex.getAbsolutePath(),
                outDir.getAbsolutePath());
        assertWithMessage("deapexer(" + apex + ") failed: " + result)
                .that(result.getStatus()).isEqualTo(CommandStatus.SUCCESS);
        assertWithMessage("deapexer(" + apex + ") failed: no outDir created")
                .that(outDir.exists()).isTrue();
    }
}
