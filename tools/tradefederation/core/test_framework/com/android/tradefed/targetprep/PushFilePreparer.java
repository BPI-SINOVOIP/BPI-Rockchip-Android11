/*
 * Copyright (C) 2011 The Android Open Source Project
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

import com.android.ddmlib.IDevice;
import com.android.ddmlib.Log;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A {@link ITargetPreparer} that attempts to push any number of files from any host path to any
 * device path.
 *
 * <p>Should be performed *after* a new build is flashed, and *after* DeviceSetup is run (if
 * enabled)
 */
@OptionClass(alias = "push-file")
public class PushFilePreparer extends BaseTargetPreparer
        implements IAbiReceiver, IInvocationContextReceiver {
    private static final String LOG_TAG = "PushFilePreparer";
    private static final String MEDIA_SCAN_INTENT =
            "am broadcast -a android.intent.action.MEDIA_MOUNTED -d file://%s "
                    + "--receiver-include-background";

    private IAbi mAbi;

    @Deprecated
    @Option(
        name = "push",
        description =
                "Deprecated. Please use push-file instead. A push-spec, formatted as "
                        + "'/localpath/to/srcfile.txt->/devicepath/to/destfile.txt' "
                        + "or '/localpath/to/srcfile.txt->/devicepath/to/destdir/'. "
                        + "May be repeated. The local path may be relative to the test cases "
                        + "build out directories "
                        + "($ANDROID_HOST_OUT_TESTCASES / $ANDROID_TARGET_OUT_TESTCASES)."
    )
    private Collection<String> mPushSpecs = new ArrayList<>();

    @Option(
            name = "push-file",
            description =
                    "A push-spec, specifying the local file to the path where it should be pushed on "
                            + "device. May be repeated. If multiple files are configured to be pushed "
                            + "to the same remote path, the latest one will be pushed.")
    private MultiMap<File, String> mPushFileSpecs = new MultiMap<>();

    @Option(
            name = "backup-file",
            description =
                    "A key/value pair, the with key specifying a device file path to be backed up, "
                            + "and the value a device file path indicating where to save the file. "
                            + "During tear-down, the values will be executed in reverse, "
                            + "restoring the backup file location to the initial location. "
                            + "May be repeated.")
    private Map<String, String> mBackupFileSpecs = new LinkedHashMap<>();

    @Option(name="post-push", description=
            "A command to run on the device (with `adb shell (yourcommand)`) after all pushes " +
            "have been attempted.  Will not be run if a push fails with abort-on-push-failure " +
            "enabled.  May be repeated.")
    private Collection<String> mPostPushCommands = new ArrayList<>();

    @Option(name="abort-on-push-failure", description=
            "If false, continue if pushes fail.  If true, abort the Invocation on any failure.")
    private boolean mAbortOnFailure = true;

    @Option(name="trigger-media-scan", description=
            "After pushing files, trigger a media scan of external storage on device.")
    private boolean mTriggerMediaScan = false;

    @Option(name="cleanup", description = "Whether files pushed onto device should be cleaned up "
            + "after test. Note that the preparer does not verify that files/directories have "
            + "been deleted.")
    private boolean mCleanup = false;

    @Option(
            name = "remount-system",
            description =
                    "Remounts system partition to be writable "
                            + "so that files could be pushed there too")
    private boolean mRemountSystem = false;

    @Option(
            name = "remount-vendor",
            description =
                    "Remounts vendor partition to be writable "
                            + "so that files could be pushed there too")
    private boolean mRemountVendor = false;

    private Set<String> mFilesPushed = null;
    /** If the preparer is part of a module, we can use the test module name as a search criteria */
    private String mModuleName = null;

    /**
     * Helper method to only throw if mAbortOnFailure is enabled. Callers should behave as if this
     * method may return.
     */
    private void fail(String message, DeviceDescriptor descriptor) throws TargetSetupError {
        if (shouldAbortOnFailure()) {
            throw new TargetSetupError(message, descriptor);
        } else {
            // Log the error and return
            Log.w(LOG_TAG, message);
        }
    }

    /** Create the list of files to be pushed. */
    public final Map<String, File> getPushSpecs(DeviceDescriptor descriptor)
            throws TargetSetupError {
        Map<String, File> remoteToLocalMapping = new LinkedHashMap<>();
        for (String pushspec : mPushSpecs) {
            String[] pair = pushspec.split("->");
            if (pair.length != 2) {
                fail(String.format("Invalid pushspec: '%s'", Arrays.asList(pair)), descriptor);
                continue;
            }
            remoteToLocalMapping.put(pair[1], new File(pair[0]));
        }
        // Push the file structure
        for (File local : mPushFileSpecs.keySet()) {
            for (String remoteLocation : mPushFileSpecs.get(local)) {
                remoteToLocalMapping.put(remoteLocation, local);
            }
        }
        return remoteToLocalMapping;
    }

    /** Whether or not to abort on push failure. */
    public boolean shouldAbortOnFailure() {
        return mAbortOnFailure;
    }

    /** {@inheritDoc} */
    @Override
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    /** {@inheritDoc} */
    @Override
    public IAbi getAbi() {
        return mAbi;
    }

    /** {@inheritDoc} */
    @Override
    public void setInvocationContext(IInvocationContext invocationContext) {
        if (invocationContext.getAttributes().get(ModuleDefinition.MODULE_NAME) != null) {
            // Only keep the module name
            mModuleName =
                    invocationContext.getAttributes().get(ModuleDefinition.MODULE_NAME).get(0);
        }
    }

    /**
     * Resolve relative file path via {@link IBuildInfo} and test cases directories.
     *
     * @param buildInfo the build artifact information
     * @param fileName relative file path to be resolved
     * @return the file from the build info or test cases directories
     */
    public File resolveRelativeFilePath(IBuildInfo buildInfo, String fileName) {
        File src = null;
        if (buildInfo != null) {
            src = buildInfo.getFile(fileName);
            if (src != null && src.exists()) {
                return src;
            }
        }
        if (buildInfo instanceof IDeviceBuildInfo) {
            IDeviceBuildInfo deviceBuild = (IDeviceBuildInfo) buildInfo;
            File testDir = deviceBuild.getTestsDir();
            List<File> scanDirs = new ArrayList<>();
            // If it exists, always look first in the ANDROID_TARGET_OUT_TESTCASES
            File targetTestCases = deviceBuild.getFile(BuildInfoFileKey.TARGET_LINKED_DIR);
            if (targetTestCases != null) {
                scanDirs.add(targetTestCases);
            }
            if (testDir != null) {
                scanDirs.add(testDir);
            }

            if (mModuleName != null) {
                // Use module name as a discriminant to find some files
                if (testDir != null) {
                    try {
                        File moduleDir =
                                FileUtil.findDirectory(
                                        mModuleName, scanDirs.toArray(new File[] {}));
                        if (moduleDir != null) {
                            // If the spec is pushing the module itself
                            if (mModuleName.equals(fileName)) {
                                // If that's the main binary generated by the target, we push the
                                // full directory
                                return moduleDir;
                            }
                            // Search the module directory if it exists use it in priority
                            src = FileUtil.findFile(fileName, null, moduleDir);
                            if (src != null) {
                                // Search again with filtering on ABI
                                File srcWithAbi = FileUtil.findFile(fileName, mAbi, moduleDir);
                                if (srcWithAbi != null
                                        && !srcWithAbi
                                                .getAbsolutePath()
                                                .startsWith(src.getAbsolutePath())) {
                                    // When multiple matches are found, return the one with matching
                                    // ABI unless src is its parent directory.
                                    return srcWithAbi;
                                }
                                return src;
                            }
                        } else {
                            CLog.d("Did not find any module directory for '%s'", mModuleName);
                        }

                    } catch (IOException e) {
                        CLog.w(
                                "Something went wrong while searching for the module '%s' "
                                        + "directory.",
                                mModuleName);
                    }
                }
            }
            // Search top-level matches
            for (File searchDir : scanDirs) {
                try {
                    Set<File> allMatch = FileUtil.findFilesObject(searchDir, fileName);
                    if (allMatch.size() > 1) {
                        CLog.d(
                                "Several match for filename '%s', searching for top-level match.",
                                fileName);
                        for (File f : allMatch) {
                            // Bias toward direct child / top level nodes
                            if (f.getParent().equals(searchDir.getAbsolutePath())) {
                                return f;
                            }
                        }
                    } else if (allMatch.size() == 1) {
                        return allMatch.iterator().next();
                    }
                } catch (IOException e) {
                    CLog.w("Failed to find test files from directory.");
                }
            }
            // Fall-back to searching everything
            try {
                // Search the full tests dir if no target dir is available.
                src = FileUtil.findFile(fileName, null, scanDirs.toArray(new File[] {}));
                if (src != null) {
                    // Search again with filtering on ABI
                    File srcWithAbi =
                            FileUtil.findFile(fileName, mAbi, scanDirs.toArray(new File[] {}));
                    if (srcWithAbi != null
                            && !srcWithAbi.getAbsolutePath().startsWith(src.getAbsolutePath())) {
                        // When multiple matches are found, return the one with matching
                        // ABI unless src is its parent directory.
                        return srcWithAbi;
                    }
                    return src;
                }
            } catch (IOException e) {
                CLog.w("Failed to find test files from directory.");
                src = null;
            }

            if (src == null && testDir != null) {
                // TODO(b/138416078): Once build dependency can be fixed and test required
                // APKs are all under the test module directory, we can remove this fallback
                // approach to do individual download from remote artifact.
                // Try to stage the files from remote zip files.
                src = buildInfo.stageRemoteFile(fileName, testDir);
            }
        }
        return src;
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        mFilesPushed = new HashSet<>();
        ITestDevice device = testInfo.getDevice();
        if (mRemountSystem) {
            device.remountSystemWritable();
        }
        if (mRemountVendor) {
            device.remountVendorWritable();
        }

        // Backup files
        for (Map.Entry<String, String> entry : mBackupFileSpecs.entrySet()) {
            device.executeShellCommand(
                    "mv \"" + entry.getKey() + "\" \"" + entry.getValue() + "\"");
        }

        Map<String, File> remoteToLocalMapping = getPushSpecs(device.getDeviceDescriptor());
        for (String remotePath : remoteToLocalMapping.keySet()) {
            File local = remoteToLocalMapping.get(remotePath);
            Log.d(
                    LOG_TAG,
                    String.format(
                            "Trying to push local '%s' to remote '%s'",
                            local.getPath(), remotePath));
            evaluatePushingPair(device, testInfo.getBuildInfo(), local, remotePath);
        }

        for (String command : mPostPushCommands) {
            device.executeShellCommand(command);
        }

        if (mTriggerMediaScan) {
            String mountPoint = device.getMountPoint(IDevice.MNT_EXTERNAL_STORAGE);
            device.executeShellCommand(String.format(MEDIA_SCAN_INTENT, mountPoint));
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        if (!(e instanceof DeviceNotAvailableException) && mCleanup && mFilesPushed != null) {
            if (mRemountSystem) {
                device.remountSystemWritable();
            }
            if (mRemountVendor) {
                device.remountVendorWritable();
            }
            for (String devicePath : mFilesPushed) {
                device.deleteFile(devicePath);
            }
            // Restore files
            for (Map.Entry<String, String> entry : mBackupFileSpecs.entrySet()) {
                device.executeShellCommand(
                        "mv \"" + entry.getValue() + "\" \"" + entry.getKey() + "\"");
            }
        }
    }

    private void evaluatePushingPair(
            ITestDevice device, IBuildInfo buildInfo, File src, String remotePath)
            throws TargetSetupError, DeviceNotAvailableException {
        String localPath = src.getPath();
        if (!src.isAbsolute()) {
            src = resolveRelativeFilePath(buildInfo, localPath);
        }
        if (src == null || !src.exists()) {
            fail(
                    String.format("Local source file '%s' does not exist", localPath),
                    device.getDeviceDescriptor());
            return;
        }
        if (src.isDirectory()) {
            boolean deleteContentOnly = true;
            if (!device.doesFileExist(remotePath)) {
                device.executeShellCommand(String.format("mkdir -p \"%s\"", remotePath));
                deleteContentOnly = false;
            } else if (!device.isDirectory(remotePath)) {
                // File exists and is not a directory
                throw new TargetSetupError(
                        String.format(
                                "Attempting to push dir '%s' to an existing device file '%s'",
                                src.getAbsolutePath(), remotePath),
                        device.getDeviceDescriptor());
            }
            Set<String> filter = new HashSet<>();
            if (mAbi != null) {
                String currentArch = AbiUtils.getArchForAbi(mAbi.getName());
                filter.addAll(AbiUtils.getArchSupported());
                filter.remove(currentArch);
            }
            // TODO: Look into using syncFiles but that requires improving sync to work for unroot
            if (!device.pushDir(src, remotePath, filter)) {
                fail(
                        String.format(
                                "Failed to push local '%s' to remote '%s'", localPath, remotePath),
                        device.getDeviceDescriptor());
                return;
            } else {
                if (deleteContentOnly) {
                    remotePath += "/*";
                }
                mFilesPushed.add(remotePath);
            }
        } else {
            if (!device.pushFile(src, remotePath)) {
                fail(
                        String.format(
                                "Failed to push local '%s' to remote '%s'", localPath, remotePath),
                        device.getDeviceDescriptor());
                return;
            } else {
                mFilesPushed.add(remotePath);
            }
        }
    }
}
