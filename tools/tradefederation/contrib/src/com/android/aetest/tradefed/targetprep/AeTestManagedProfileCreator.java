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
 * limitations under the License.
 */

package com.android.aetest.tradefed.targetprep;

import static com.android.tradefed.util.BuildTestsZipUtils.getApkFile;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.AltDirBehavior;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.ITargetCleaner;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * A {@link ITargetPreparer} that creates a managed profile in the testing device.
 *
 * <p>This was forked off Android Enterprise team's target preparer, where its method of resolving
 * the test APK was substituted with the platform's implementation. It is also modified to use the
 * standard TradeFed way of disabling a preparer. The two classes are otherwise the same.
 */
@OptionClass(alias = "ae-test-create-managed-profile")
public class AeTestManagedProfileCreator extends BaseTargetPreparer implements ITargetCleaner {

    @Option(
            name = "profile-owner-component",
            description = "Profile owner component to set; optional")
    private String mProfileOwnerComponent = null;

    @Option(name = "profile-owner-apk", description = "Profile owner apk path; optional")
    private String mProfileOwnerApk = null;

    @Option(
            name = "alt-dir",
            description =
                    "Alternate directory to look for the apk if the apk is not in the tests "
                            + "zip file. For each alternate dir, will look in //, //data/app, "
                            + "//DATA/app, //DATA/app/apk_name/ and //DATA/priv-app/apk_name/. "
                            + "Can be repeated. Look for apks in last alt-dir first.")
    private List<File> mAltDirs = new ArrayList<>();

    @Option(
            name = "alt-dir-behavior",
            description =
                    "The order of alternate directory to be used "
                            + "when searching for apks to install")
    private AltDirBehavior mAltDirBehavior = AltDirBehavior.FALLBACK;

    /** UserID for user in managed profile. */
    private int mManagedProfileUserId;

    /** {@inheritDoc} */
    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo)
            throws TargetSetupError, DeviceNotAvailableException {
        String pmCommand =
                "pm create-user --profileOf 0 --managed "
                        + "TestProfile_"
                        + System.currentTimeMillis();
        String pmCommandOutput = device.executeShellCommand(pmCommand);

        // Extract the id of the new user.
        String[] pmCmdTokens = pmCommandOutput.split("\\s+");
        if (!pmCmdTokens[0].contains("Success:")) {
            throw new TargetSetupError("Managed profile creation failed.");
        }
        mManagedProfileUserId = Integer.parseInt(pmCmdTokens[pmCmdTokens.length - 1]);

        // Start managed profile user.
        device.startUser(mManagedProfileUserId);

        CLog.i(String.format("New user created: %d", mManagedProfileUserId));

        if (mProfileOwnerComponent != null && mProfileOwnerApk != null) {
            device.installPackageForUser(
                    getApk(device, buildInfo, mProfileOwnerApk), true, mManagedProfileUserId);
            String command =
                    String.format(
                            "dpm set-profile-owner --user %d %s",
                            mManagedProfileUserId, mProfileOwnerComponent);
            String commandOutput = device.executeShellCommand(command);
            String[] cmdTokens = commandOutput.split("\\s+");
            if (!cmdTokens[0].contains("Success:")) {
                throw new RuntimeException(
                        String.format(
                                "Fail to setup %s as profile owner.", mProfileOwnerComponent));
            }

            CLog.i(
                    String.format(
                            "%s was set as profile owner of user %d",
                            mProfileOwnerComponent, mManagedProfileUserId));
        }

        // Reboot device to create the apps in managed profile.
        device.reboot();
        device.waitForDeviceAvailable();
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(ITestDevice testDevice, IBuildInfo buildInfo, Throwable throwable)
            throws DeviceNotAvailableException {
        testDevice.removeUser(mManagedProfileUserId);
    }

    /**
     * Get the {@link File} object that refers to the APK to install.
     *
     * <p>This is a replacement of the method with a similar signature in the original class's super
     * class, so that the above code can align as closely to the original class as possible.
     */
    private File getApk(ITestDevice device, IBuildInfo buildInfo, String fileName)
            throws TargetSetupError {
        try {
            File apkFile =
                    getApkFile(
                            buildInfo,
                            mProfileOwnerApk,
                            mAltDirs,
                            mAltDirBehavior,
                            false /* use resource as fallback */,
                            null /* device signing key */);
            if (apkFile == null) {
                throw new TargetSetupError(
                        String.format("Test app %s was not found.", mProfileOwnerApk),
                        device.getDeviceDescriptor());
            }
            return apkFile;
        } catch (IOException e) {
            throw new TargetSetupError(
                    String.format(
                            "failed to resolve apk path for apk %s in build %s",
                            mProfileOwnerApk, buildInfo.toString()),
                    e,
                    device.getDeviceDescriptor());
        }
    }
}
