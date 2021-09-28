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
 * limitations under the License.
 */
package com.android.compatibility.common.tradefed.targetprep;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.AaptParser;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Installs device interaction helper implementations on the target.
 *
 * <p>Device interaction helpers allow xTS tests to drive aspects of a device that are not covered
 * by public APIs but are necessary to make a test work, such as clicking a button to initiate
 * printing in a test that exercises printing APIs. xTS tests running on the device will load
 * concrete implementations at runtime using {@link HelperManager}, with a fallback to default
 * implementations that interact with the default Android UI. See {@link
 * DeviceInteractionHelperRule} for an example of connecting helpers to a test at runtime.
 *
 * <p>If device-specific implementations are needed, the device must contain a property listing the
 * packages that will be searched at runtime. DeviceInteractionHelperInstaller will read this same
 * property to determine which packages it will install. The default/fallback helpers are always
 * installed.
 *
 * <p>See cts/libs/helpers and cts/helpers for more details and examples of device interaction
 * helpers..
 */
@OptionClass(alias = "device-interaction-helper", global_namespace = false)
public class DeviceInteractionHelperInstaller extends BaseTargetPreparer {

    @Option(
            name = "default-package",
            description = "name of the package containing fallback device interaction helpers")
    private String mDefaultHelperPackage = "com.android.cts.helpers.aosp";

    @Option(
            name = "property-name",
            description = "name of a device property listing necessary OEM helper packages")
    private String mHelperPackagePropertyKey = "ro.vendor.cts_interaction_helper_packages";

    private Set<String> mInstalledPackages = new HashSet<String>();

    /** {@inheritDoc} */
    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", device);
        context.addDeviceBuildInfo("device", buildInfo);
        TestInformation testInfo =
                TestInformation.newBuilder().setInvocationContext(context).build();
        setUp(testInfo);
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();

        // Default helpers are always required.  Push them first so we can fail early if missing.
        String defaultHelperName = mDefaultHelperPackage + ".apk";
        try {
            File defaultHelpers = testInfo.getDependencyFile(defaultHelperName, true);
            installHelperApk(device, defaultHelpers, mDefaultHelperPackage);
        } catch (FileNotFoundException e) {
            throw new BuildError(
                    "Unable to find "
                            + defaultHelperName
                            + ". Make sure the file is present in your testcases directory or set "
                            + " an explicit path with default-interaction-helper-package.",
                    device.getDeviceDescriptor());
        }

        // Parse the search path from the device property .  It is not an error if the property is
        // empty.  This just means that the default helpers installed above will be used.
        String searchPath = device.getProperty(mHelperPackagePropertyKey);
        if (searchPath == null || searchPath.isEmpty()) {
            return;
        }

        // Search locally for each package listed in the device package search property and install
        // everything found.  If a requested package is not found, that is not a fatal error because
        // the particular test being run may not have required helpers from that package anyway.
        for (String pkg : searchPath.split(":")) {
            if (pkg.isEmpty()) {
                continue;
            }
            if (mInstalledPackages.contains(pkg)) {
                continue; // Already installed.
            }

            try {
                String apkName = pkg + ".apk";
                File apkFile = testInfo.getDependencyFile(apkName, true);
                checkApkFile(device, apkFile, pkg);
                installHelperApk(device, apkFile, pkg);
            } catch (FileNotFoundException e) {
                CLog.w("Unable to find apk for %s", pkg);
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable t) throws DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        for (String pkg : mInstalledPackages) {
            String msg = device.uninstallPackage(pkg);
            if (msg != null) {
                CLog.w(String.format("Error uninstalling package '%s': %s", pkg, msg));
            }
        }
    }

    /**
     * Checks that an apk is readable and contains a valid package name.
     *
     * @param device the ITestDevice where the apk will be installed.
     * @param apkFile the apk file to check.
     * @param expectedPackage the expected name of the package in the apk.
     * @throws BuildError if the apk isn't readable or isn't valid.
     */
    private void checkApkFile(ITestDevice device, File apkFile, String expectedPackage)
            throws BuildError {
        String path = apkFile.getPath();
        if (!apkFile.canRead()) {
            throw new BuildError(
                    "Helper " + path + " does not exist or is unreadable.",
                    device.getDeviceDescriptor());
        }
        AaptParser parser = parseApk(apkFile);
        if (parser == null) {
            throw new BuildError(
                    "Unable to parse helper apk " + path, device.getDeviceDescriptor());
        }
        String apkPackage = parser.getPackageName();
        if (apkPackage == null || apkPackage.isEmpty()) {
            throw new BuildError(
                    "Unable to parse helper apk " + path, device.getDeviceDescriptor());
        }
        if (!expectedPackage.equals(apkPackage)) {
            throw new BuildError(
                    String.format(
                            "Helper apk %s declares package %s but was expected to declare %s",
                            path, apkPackage, expectedPackage),
                    device.getDeviceDescriptor());
        }
    }

    protected AaptParser parseApk(File apkFile) {
        return AaptParser.parse(apkFile);
    }

    /** Installs an apk containing DeviceInteractionHelper implementations. */
    private void installHelperApk(ITestDevice device, File apkFile, String apkPackage)
            throws BuildError, DeviceNotAvailableException, TargetSetupError {
        if (apkFile == null) {
            throw new BuildError("Invalid apk name", device.getDeviceDescriptor());
        }
        CLog.logAndDisplay(
                LogLevel.INFO, "Installing %s from %s", apkFile.getName(), apkFile.getPath());
        List<String> extraArgs = new ArrayList<String>();
        if (device.isAppEnumerationSupported()) {
            extraArgs.add("--force-queryable");
        }
        String msg = device.installPackage(apkFile, true, extraArgs.toArray(new String[] {}));
        if (msg != null) {
            throw new TargetSetupError(
                    String.format("Failed to install %s: %s", apkFile.getName(), msg),
                    device.getDeviceDescriptor());
        }
        mInstalledPackages.add(apkPackage);
    }
}
