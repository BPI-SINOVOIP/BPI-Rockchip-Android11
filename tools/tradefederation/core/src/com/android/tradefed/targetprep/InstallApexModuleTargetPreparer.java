/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tradefed.targetprep;

import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.ApexInfo;
import com.android.tradefed.device.PackageInfo;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.targetprep.suite.SuiteApkInstaller;
import com.android.tradefed.util.AaptParser;
import com.android.tradefed.util.BundletoolUtil;
import com.android.tradefed.util.RunUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/*
 * A {@link TargetPreparer} that attempts to install mainline modules to device
 * and verify install success.
 */
@OptionClass(alias = "mainline-module-installer")
public class InstallApexModuleTargetPreparer extends SuiteApkInstaller {

    private static final String APEX_DATA_DIR = "/data/apex/active/";
    private static final String STAGING_DATA_DIR = "/data/app-staging/";
    private static final String SESSION_DATA_DIR = "/data/apex/sessions/";
    private static final String APEX_SUFFIX = ".apex";
    private static final String APK_SUFFIX = ".apk";
    private static final String SPLIT_APKS_SUFFIX = ".apks";
    private static final String TRAIN_WITH_APEX_INSTALL_OPTION = "install-multi-package";
    private static final String ACTIVATED_APEX_SOURCEDIR_PREFIX = "data";
    private static final int R_SDK_INT = 30;

    private List<ApexInfo> mTestApexInfoList = new ArrayList<>();
    private Set<String> mApkToInstall = new LinkedHashSet<>();
    private List<String> mApkInstalled = new ArrayList<>();
    private List<String> mSplitsInstallArgs = new ArrayList<>();
    private BundletoolUtil mBundletoolUtil;
    private String mDeviceSpecFilePath = "";

    @Option(name = "bundletool-file-name", description = "The file name of the bundletool jar.")
    private String mBundletoolFilename;

    @Option(
        name = "apex-staging-wait-time",
        description = "The time in ms to wait for apex staged session ready.",
        isTimeVal = true
    )
    private long mApexStagingWaitTime = 1 * 60 * 1000;

    @Option(
            name = "ignore-if-module-not-preloaded",
            description =
                    "Skip installing the module(s) when the module(s) that are not "
                            + "preloaded on device. Otherwise an exception will be thrown.")
    private boolean mIgnoreIfNotPreloaded = false;

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        setTestInformation(testInfo);
        ITestDevice device = testInfo.getDevice();

        if (getTestsFileName().isEmpty()) {
            CLog.i("No apk/apex module file to install. Skipping.");
            return;
        }

        cleanUpStagedAndActiveSession(device);

        Set<ApexInfo> activatedApexes = device.getActiveApexes();

        CLog.i("Activated apex packages list before module/train installation:");
        for (ApexInfo info : activatedApexes) {
            CLog.i("Activated apex: %s", info.toString());
        }

        List<File> testAppFiles = getModulesToInstall(testInfo);
        if (testAppFiles.isEmpty()) {
            CLog.i("No modules are preloaded on the device, so no modules will be installed.");
            return;
        }
        if (containsApks(testAppFiles)) {
            installUsingBundleTool(testInfo, testAppFiles);
            if (mTestApexInfoList.isEmpty()) {
                CLog.i("No Apex module in the train. Skipping reboot.");
                return;
            } else {
                RunUtil.getDefault().sleep(mApexStagingWaitTime);
                device.reboot();
            }
        } else {
            Map<File, String> appFilesAndPackages = resolveApkFiles(testInfo, testAppFiles);
            installer(testInfo, appFilesAndPackages);
            if (containsApex(appFilesAndPackages.keySet())
                    || containsPersistentApk(appFilesAndPackages.keySet(), testInfo)) {
                RunUtil.getDefault().sleep(mApexStagingWaitTime);
                device.reboot();
            }
            if (mTestApexInfoList.isEmpty()) {
                CLog.i("Train activation succeed.");
                return;
            }
        }

        activatedApexes = device.getActiveApexes();

        if (activatedApexes.isEmpty()) {
            throw new TargetSetupError(
                    String.format(
                            "Failed to retrieve activated apex on device %s. Empty set returned.",
                            device.getSerialNumber()),
                    device.getDeviceDescriptor());
        } else {
            CLog.i("Activated apex packages list after module/train installation:");
            for (ApexInfo info : activatedApexes) {
                CLog.i("Activated apex: %s", info.toString());
            }
        }

        List<ApexInfo> failToActivateApex = getModulesFailToActivate(activatedApexes);

        if (!failToActivateApex.isEmpty()) {
            throw new TargetSetupError(
                    String.format(
                            "Failed to activate %s on device %s.",
                            listApexInfo(failToActivateApex).toString(), device.getSerialNumber()),
                    device.getDeviceDescriptor());
        }
        CLog.i("Train activation succeed.");
    }

    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        if (e instanceof DeviceNotAvailableException) {
            CLog.e("Device %s is not available. Teardown() skipped.", device.getSerialNumber());
            return;
        } else {
            if (mTestApexInfoList.isEmpty() && getApkInstalled().isEmpty()) {
                super.tearDown(testInfo, e);
            } else {
                for (String apkPkgName : getApkInstalled()) {
                    super.uninstallPackage(device, apkPkgName);
                }
                if (!mTestApexInfoList.isEmpty()) {
                    cleanUpStagedAndActiveSession(device);
                }
            }
        }
    }

    /**
     * Initializes the bundletool util for this class.
     *
     * @param testInfo the {@link TestInformation} for the invocation.
     * @throws TargetSetupError if bundletool cannot be found.
     */
    private void initBundletoolUtil(TestInformation testInfo) throws TargetSetupError {
        if (mBundletoolUtil != null) {
            return;
        }
        File bundletoolJar = getLocalPathForFilename(testInfo, getBundletoolFileName());
        if (bundletoolJar == null) {
            throw new TargetSetupError(
                    String.format("Failed to find bundletool jar %s.", getBundletoolFileName()),
                    testInfo.getDevice().getDeviceDescriptor(),
                    InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
        }
        mBundletoolUtil = new BundletoolUtil(bundletoolJar);
    }

    /**
     * Initializes the path to the device spec file.
     *
     * @param device the {@link ITestDevice} to install the train.
     * @throws TargetSetupError if fails to generate the device spec file.
     */
    private void initDeviceSpecFilePath(ITestDevice device) throws TargetSetupError {
        if (!mDeviceSpecFilePath.equals("")) {
            return;
        }
        try {
            mDeviceSpecFilePath = getBundletoolUtil().generateDeviceSpecFile(device);
        } catch (IOException e) {
            throw new TargetSetupError(
                    String.format(
                            "Failed to generate device spec file on %s.", device.getSerialNumber()),
                    e,
                    device.getDeviceDescriptor());
        }
    }

    /**
     * Extracts and returns splits for the specified apks.
     *
     * @param testInfo the {@link TestInformation}
     * @param moduleFile The module file to extract the splits from.
     * @return a File[] containing the splits.
     * @throws TargetSetupError if bundletool cannot be found or device spec file fails to generate.
     */
    private List<File> getSplitsForApks(TestInformation testInfo, File moduleFile)
            throws TargetSetupError {
        initBundletoolUtil(testInfo);
        initDeviceSpecFilePath(testInfo.getDevice());
        File splitsDir =
                getBundletoolUtil()
                        .extractSplitsFromApks(
                                moduleFile,
                                mDeviceSpecFilePath,
                                testInfo.getDevice(),
                                testInfo.getBuildInfo());
        if (splitsDir == null || splitsDir.listFiles() == null) {
            return null;
        }
        return Arrays.asList(splitsDir.listFiles());
    }

    /**
     * Gets the modules that should be installed on the train, based on the modules preloaded on the
     * device. Modules that are not preloaded will not be installed.
     *
     * @param testInfo the {@link TestInformation}
     * @return List<String> of the modules that should be installed on the device.
     * @throws DeviceNotAvailableException when device is not available.
     * @throws TargetSetupError when mandatory modules are not installed, or module cannot be
     *     installed.
     */
    public List<File> getModulesToInstall(TestInformation testInfo)
            throws DeviceNotAvailableException, TargetSetupError {
        // Get all preloaded modules for the device.
        ITestDevice device = testInfo.getDevice();
        Set<String> installedPackages = new HashSet<>(device.getInstalledPackageNames());
        Set<ApexInfo> installedApexes = new HashSet<>(device.getActiveApexes());
        for (ApexInfo installedApex : installedApexes) {
            installedPackages.add(installedApex.name);
        }
        List<File> moduleFileNames = getTestsFileName();
        List<File> moduleNamesToInstall = new ArrayList<>();
        for (File moduleFileName : moduleFileNames) {
            // getLocalPathForFilename throws if apk not found
            File moduleFile = getLocalPathForFilename(testInfo, moduleFileName.getName());
            String modulePackageName = "";
            if (moduleFile.getName().endsWith(SPLIT_APKS_SUFFIX)) {
                List<File> splits = getSplitsForApks(testInfo, moduleFile);
                if (splits == null) {
                    // Bundletool failed to extract splits.
                    CLog.w(
                            "Apks %s is not available on device %s and will not be installed.",
                            moduleFileName, mDeviceSpecFilePath);
                    continue;
                }
                modulePackageName = parsePackageName(splits.get(0), device.getDeviceDescriptor());
            } else {
                modulePackageName = parsePackageName(moduleFile, device.getDeviceDescriptor());
            }
            if (installedPackages.contains(modulePackageName)) {
                CLog.i("Found preloaded module for %s.", modulePackageName);
                moduleNamesToInstall.add(moduleFile);
                installedPackages.remove(modulePackageName);
            } else {
                if (!mIgnoreIfNotPreloaded) {
                    if (!installedPackages.isEmpty()) {
                        CLog.i(
                                "The following modules are preloaded on the device %s",
                                installedPackages);
                    }
                    throw new TargetSetupError(
                            String.format(
                                    "Mainline module %s is not preloaded on the device "
                                            + "but is in the input lists.",
                                    modulePackageName),
                            device.getDeviceDescriptor());
                }
                CLog.i(
                        "The module package %s is not preloaded on the device but is included in "
                                + "the train.",
                        modulePackageName);
            }
        }
        // Log the modules that are not included in the train.
        if (!installedPackages.isEmpty()) {
            CLog.i(
                    "The following modules are preloaded on the device, but not included in the "
                            + "train: %s",
                    installedPackages);
        }
        return moduleNamesToInstall;
    }

    // TODO(b/124461631): Remove after ddmlib supports install-multi-package.
    @Override
    protected void installer(TestInformation testInfo, Map<File, String> testAppFileNames)
            throws TargetSetupError, DeviceNotAvailableException {
        if (containsApex(testAppFileNames.keySet())) {
            mTestApexInfoList = collectApexInfoFromApexModules(testAppFileNames, testInfo);
        }
        if (containsPersistentApk(testAppFileNames.keySet(), testInfo)) {
            // When there is a persistent apk in the train, use '--staged' to install full train
            // Otherwise, do normal install without '--staged'
            installTrain(
                    testInfo,
                    new ArrayList<>(testAppFileNames.keySet()),
                    new String[] {"--staged"});
            return;
        }
        installTrain(testInfo, new ArrayList<>(testAppFileNames.keySet()), new String[] {});
    }

    /**
     * Attempts to install a mainline train containing apex on the device.
     *
     * @param testInfo the {@link TestInformation}
     * @param moduleFilenames List of String. The list of filenames of the mainline modules to be
     *     installed.
     */
    protected void installTrain(
            TestInformation testInfo, List<File> moduleFilenames, final String[] extraArgs)
            throws TargetSetupError, DeviceNotAvailableException {
        // TODO(b/137883918):remove after new adb is released, which supports installing
        // single apk/apex using 'install-multi-package'
        ITestDevice device = testInfo.getDevice();
        if (moduleFilenames.size() == 1) {
            device.installPackage(moduleFilenames.get(0), true, extraArgs);
            if (moduleFilenames.get(0).getName().endsWith(APK_SUFFIX)) {
                String packageName =
                        parsePackageName(moduleFilenames.get(0), device.getDeviceDescriptor());
                mApkInstalled.add(packageName);
            }
            return;
        }

        List<String> apkPackageNames = new ArrayList<>();
        List<String> trainInstallCmd = new ArrayList<>();

        trainInstallCmd.add(TRAIN_WITH_APEX_INSTALL_OPTION);
        if (extraArgs != null) {
            for (String arg : extraArgs) {
                trainInstallCmd.add(arg);
            }
        }

        for (File moduleFile : moduleFilenames) {
            trainInstallCmd.add(moduleFile.getAbsolutePath());
            if (moduleFile.getName().endsWith(APK_SUFFIX)) {
                String packageName = parsePackageName(moduleFile, device.getDeviceDescriptor());
                apkPackageNames.add(packageName);
            }
        }
        String log = device.executeAdbCommand(trainInstallCmd.toArray(new String[0]));

        // Wait until all apexes are fully staged and ready.
        // TODO: should have adb level solution b/130039562
        RunUtil.getDefault().sleep(mApexStagingWaitTime);

        if (log.contains("Success")) {
            CLog.d(
                    "Train is staged successfully. Cmd: %s, Output: %s.",
                    trainInstallCmd.toString(), log);
        } else {
            throw new TargetSetupError(
                    String.format(
                            "Failed to install %s on %s. Error log: '%s'",
                            moduleFilenames.toString(), device.getSerialNumber(), log),
                    device.getDeviceDescriptor(),
                    DeviceErrorIdentifier.APK_INSTALLATION_FAILED);
        }
        mApkInstalled.addAll(apkPackageNames);
    }

    /**
     * Attempts to install mainline module(s) using bundletool.
     *
     * @param testInfo the {@link TestInformation}
     * @param testAppFileNames the filenames of the preloaded modules to install.
     */
    protected void installUsingBundleTool(TestInformation testInfo, List<File> testAppFileNames)
            throws TargetSetupError, DeviceNotAvailableException {
        initBundletoolUtil(testInfo);
        initDeviceSpecFilePath(testInfo.getDevice());

        if (testAppFileNames.size() == 1) {
            // Installs single .apks module.
            installSingleModuleUsingBundletool(
                    testInfo, mDeviceSpecFilePath, testAppFileNames.get(0));
        } else {
            installMultipleModuleUsingBundletool(testInfo, mDeviceSpecFilePath, testAppFileNames);
        }

        mApkInstalled.addAll(mApkToInstall);
    }

    /**
     * Attempts to install a single mainline module(.apks) using bundletool.
     *
     * @param testInfo the {@link TestInformation}
     * @param deviceSpecFilePath the spec file of the test device
     * @param apkFile the file of the .apks
     */
    private void installSingleModuleUsingBundletool(
            TestInformation testInfo, String deviceSpecFilePath, File apkFile)
            throws TargetSetupError, DeviceNotAvailableException {
        // No need to resolve we have the single .apks file needed.
        File apks = apkFile;
        // Rename the extracted files and add the file to filename list.
        List<File> splits = getSplitsForApks(testInfo, apks);
        ITestDevice device = testInfo.getDevice();
        if (splits == null || splits.isEmpty()) {
            throw new TargetSetupError(
                    String.format("Extraction for %s failed. No apk/apex is extracted.", apkFile),
                    device.getDeviceDescriptor());
        }
        // Install .apks that contain apex module.
        if (containsApex(splits)) {
            Map<File, String> appFilesAndPackages = new LinkedHashMap<>();
            appFilesAndPackages.put(
                    splits.get(0), parsePackageName(splits.get(0), device.getDeviceDescriptor()));
            super.installer(testInfo, appFilesAndPackages);
        } else {
            // Install .apks that contain apk module.
            getBundletoolUtil().installApks(apks, device);
            mApkToInstall.add(parsePackageName(splits.get(0), device.getDeviceDescriptor()));
        }
        return;
    }

    /**
     * Attempts to install multiple mainline modules using bundletool. Modules can be any
     * combination of .apk, .apex or .apks.
     *
     * @param testInfo the {@link TestInformation}
     * @param deviceSpecFilePath the spec file of the test device
     * @param testAppFileNames the list of preloaded modules to install.
     */
    private void installMultipleModuleUsingBundletool(
            TestInformation testInfo, String deviceSpecFilePath, List<File> testAppFileNames)
            throws TargetSetupError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        for (File moduleFileName : testAppFileNames) {
            File moduleFile = getLocalPathForFilename(testInfo, moduleFileName.getName());
            if (moduleFileName.getName().endsWith(SPLIT_APKS_SUFFIX)) {
                List<File> splits = getSplitsForApks(testInfo, moduleFile);
                String splitsArgs = createInstallArgsForSplit(splits, device);
                mSplitsInstallArgs.add(splitsArgs);
            } else {
                if (moduleFileName.getName().endsWith(APEX_SUFFIX)) {
                    ApexInfo apexInfo = retrieveApexInfo(moduleFile, device.getDeviceDescriptor());
                    mTestApexInfoList.add(apexInfo);
                } else {
                    mApkToInstall.add(parsePackageName(moduleFile, device.getDeviceDescriptor()));
                }
                mSplitsInstallArgs.add(moduleFile.getAbsolutePath());
            }
        }

        List<String> installCmd = new ArrayList<>();

        installCmd.add(TRAIN_WITH_APEX_INSTALL_OPTION);
        for (String arg : mSplitsInstallArgs) {
            installCmd.add(arg);
        }
        device.waitForDeviceAvailable();

        String log = device.executeAdbCommand(installCmd.toArray(new String[0]));
        if (log.contains("Success")) {
            CLog.d("Train is staged successfully. Output: %s.", log);
        } else {
            throw new TargetSetupError(
                    String.format(
                            "Failed to stage train on device %s. Cmd is: %s. Error log: %s.",
                            device.getSerialNumber(), installCmd.toString(), log),
                    device.getDeviceDescriptor());
        }
    }

    /**
     * Retrieves ApexInfo which contains packageName and versionCode from the given apex file.
     *
     * @param testApexFile The apex file we retrieve information from.
     * @return an {@link ApexInfo} containing the packageName and versionCode of the given file
     * @throws TargetSetupError if aapt parser failed to parse the file.
     */
    @VisibleForTesting
    protected ApexInfo retrieveApexInfo(File testApexFile, DeviceDescriptor deviceDescriptor)
            throws TargetSetupError {
        AaptParser parser = AaptParser.parse(testApexFile);
        if (parser == null) {
            throw new TargetSetupError(
                    "apex installed but AaptParser failed",
                    deviceDescriptor,
                    DeviceErrorIdentifier.AAPT_PARSER_FAILED);
        }
        return new ApexInfo(parser.getPackageName(), Long.parseLong(parser.getVersionCode()));
    }

    /**
     * Gets the keyword (e.g., 'tzdata' for com.android.tzdata.apex) from the apex package name.
     *
     * @param packageName The package name of the apex file.
     * @return a string The keyword of the apex package name.
     */
    protected String getModuleKeywordFromApexPackageName(String packageName) {
        String[] components = packageName.split("\\.");
        return components[components.length - 1];
    }

    /* Helper method to format List<ApexInfo> to List<String>. */
    private ArrayList<String> listApexInfo(List<ApexInfo> list) {
        ArrayList<String> res = new ArrayList<String>();
        for (ApexInfo testApexInfo : list) {
            res.add(testApexInfo.toString());
        }
        return res;
    }

    /* Checks if the app file is apex or not */
    private boolean isApex(File file) {
        if (file.getName().endsWith(APEX_SUFFIX)) {
            return true;
        }
        return false;
    }

    /** Checks if the apps need to be installed contains apex. */
    private boolean containsApex(Collection<File> testFileNames) {
        for (File filename : testFileNames) {
            if (filename.getName().endsWith(APEX_SUFFIX)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Checks if the apps need to be installed contains apex.
     *
     * @param testFileNames The list of the test modules
     */
    private boolean containsApks(List<File> testFileNames) {
        for (File filename : testFileNames) {
            if (filename.getName().endsWith(SPLIT_APKS_SUFFIX)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Cleans up data/apex/active. data/apex/sessions, data/app-staging.
     *
     * @param device The test device
     */
    private void cleanUpStagedAndActiveSession(ITestDevice device)
            throws DeviceNotAvailableException {
        boolean reboot = false;
        if (!mTestApexInfoList.isEmpty()) {
            device.deleteFile(APEX_DATA_DIR + "*");
            device.deleteFile(STAGING_DATA_DIR + "*");
            device.deleteFile(SESSION_DATA_DIR + "*");
            reboot = true;
        } else {
            if (!device.executeShellV2Command("ls " + APEX_DATA_DIR).getStdout().isEmpty()) {
                device.deleteFile(APEX_DATA_DIR + "*");
                reboot = true;
            }
            if (!device.executeShellV2Command("ls " + STAGING_DATA_DIR).getStdout().isEmpty()) {
                device.deleteFile(STAGING_DATA_DIR + "*");
                reboot = true;
            }
            if (!device.executeShellV2Command("ls " + SESSION_DATA_DIR).getStdout().isEmpty()) {
                device.deleteFile(SESSION_DATA_DIR + "*");
                reboot = true;
            }
        }
        if (reboot) {
            device.reboot();
        }
    }

    /**
     * Creates the install args for the split .apks.
     *
     * @param splits The directory that split apk/apex get extracted to
     * @param device The test device
     * @return a {@link String} representing the install args for the split apks.
     */
    private String createInstallArgsForSplit(List<File> splits, ITestDevice device)
            throws TargetSetupError {
        String splitsArgs = "";
        for (File f : splits) {
            if (f.getName().endsWith(APEX_SUFFIX)) {
                ApexInfo apexInfo = retrieveApexInfo(f, device.getDeviceDescriptor());
                mTestApexInfoList.add(apexInfo);
            }
            if (f.getName().endsWith(APK_SUFFIX)) {
                mApkToInstall.add(parsePackageName(f, device.getDeviceDescriptor()));
            }
            if (!splitsArgs.isEmpty()) {
                splitsArgs += ":" + f.getAbsolutePath();
            } else {
                splitsArgs += f.getAbsolutePath();
            }
        }
        return splitsArgs;
    }

    /**
     * Checks if the input files contain any persistent apk.
     *
     * @param testAppFileNames The list of the file names of the modules to install
     * @param testInfo The {@link TestInformation}
     * @return <code>true</code> if the input files contains a persistent apk module.
     */
    protected boolean containsPersistentApk(
            Collection<File> testAppFileNames, TestInformation testInfo)
            throws TargetSetupError, DeviceNotAvailableException {
        for (File moduleFileName : testAppFileNames) {
            if (isPersistentApk(moduleFileName, testInfo)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Checks if an apk is a persistent apk.
     *
     * @param filename The apk module file to check
     * @param testInfo The {@link TestInformation}
     * @return <code>true</code> if this is a persistent apk module.
     */
    protected boolean isPersistentApk(File filename, TestInformation testInfo)
            throws TargetSetupError, DeviceNotAvailableException {
        if (!filename.getName().endsWith(APK_SUFFIX)) {
            return false;
        }
        PackageInfo pkgInfo =
                testInfo.getDevice()
                        .getAppPackageInfo(
                                parsePackageName(
                                        filename, testInfo.getDevice().getDeviceDescriptor()));
        return pkgInfo.isPersistentApp();
    }

    /**
     * Collects apex info from the apex modules for activation check.
     *
     * @param testAppFileNames The list of the file names of the modules to install
     * @param testInfo The {@link TestInformation}
     * @return a list containing the apexinfo of the apex modules in the input file lists
     */
    protected List<ApexInfo> collectApexInfoFromApexModules(
            Map<File, String> testAppFileNames, TestInformation testInfo) throws TargetSetupError {
        List<ApexInfo> apexInfoList = new ArrayList<>();

        for (File appFile : testAppFileNames.keySet()) {
            if (isApex(appFile)) {
                ApexInfo apexInfo =
                        retrieveApexInfo(appFile, testInfo.getDevice().getDeviceDescriptor());
                apexInfoList.add(apexInfo);
            }
        }
        return apexInfoList;
    }

    /**
     * Get modules that failed to be activated.
     *
     * @param activatedApexes The set of the active apexes on device
     * @return a list containing the apexinfo of the input apex modules that failed to be activated.
     */
    protected List<ApexInfo> getModulesFailToActivate(Set<ApexInfo> activatedApexes)
            throws DeviceNotAvailableException, TargetSetupError {
        List<ApexInfo> failToActivateApex = new ArrayList<ApexInfo>();
        HashMap<String, ApexInfo> activatedApexInfo = new HashMap<>();
        for (ApexInfo info : activatedApexes) {
            activatedApexInfo.put(info.name, info);
        }
        for (ApexInfo testApexInfo : mTestApexInfoList) {
            if (!activatedApexInfo.containsKey(testApexInfo.name)) {
                failToActivateApex.add(testApexInfo);
            } else if (activatedApexInfo.get(testApexInfo.name).versionCode
                    != testApexInfo.versionCode) {
                failToActivateApex.add(testApexInfo);
            } else {
                String sourceDir = activatedApexInfo.get(testApexInfo.name).sourceDir;
                // Activated apex sourceDir starts with "/data"
                if (getDevice().checkApiLevelAgainstNextRelease(R_SDK_INT)
                        && !sourceDir.startsWith(ACTIVATED_APEX_SOURCEDIR_PREFIX, 1)) {
                    failToActivateApex.add(testApexInfo);
                }
            }
        }
        return failToActivateApex;
    }

    @VisibleForTesting
    protected String getBundletoolFileName() {
        return mBundletoolFilename;
    }

    @VisibleForTesting
    protected BundletoolUtil getBundletoolUtil() {
        return mBundletoolUtil;
    }

    @VisibleForTesting
    protected List<String> getApkInstalled() {
        return mApkInstalled;
    }
}
