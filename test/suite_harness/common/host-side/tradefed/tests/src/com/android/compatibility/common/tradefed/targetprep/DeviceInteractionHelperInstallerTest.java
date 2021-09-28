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

import static org.junit.Assert.fail;
import static org.mockito.Mockito.when;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.AaptParser;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link DeviceInteractionHelperInstaller} */
@RunWith(JUnit4.class)
public class DeviceInteractionHelperInstallerTest {
    private DeviceInteractionHelperInstaller mPreparer = null;
    private ITestDevice mMockDevice = null;
    private OptionSetter mOptionSetter = null;

    private Map<File, AaptParser> mAaptParsers = new HashMap<File, AaptParser>();

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createStrictMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andStubReturn(null);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("SERIAL");
        mPreparer =
                new DeviceInteractionHelperInstaller() {
                    @Override
                    protected AaptParser parseApk(File apkFile) {
                        return mAaptParsers.getOrDefault(apkFile, AaptParser.parse(apkFile));
                    }
                };
        mOptionSetter = new OptionSetter(mPreparer);
    }

    private void expectHelperInstall(File file) throws DeviceNotAvailableException {
        EasyMock.expect(mMockDevice.isAppEnumerationSupported()).andReturn(true);
        EasyMock.expect(
                        mMockDevice.installPackage(
                                EasyMock.eq(file), EasyMock.anyBoolean(), EasyMock.anyObject()))
                .andReturn(null);
    }

    private void expectDeviceProperty(String value) throws DeviceNotAvailableException {
        EasyMock.expect(mMockDevice.getProperty("ro.vendor.cts_interaction_helper_packages"))
                .andReturn(value);
    }

    /** Creates a local file in dir and makes testInfo.getDependencyFile() return it. */
    private File createFindableFile(File dir, String name, TestInformation testInfo)
            throws FileNotFoundException, IOException {
        File f = new File(dir, name);
        f.createNewFile();
        if (testInfo != null) {
            testInfo.executionFiles().put(name, f);
        }
        return f;
    }

    /** Creates a mock AaptParser that will return a desired package name for a file. */
    private void mockApkPackage(File apk, String packageName) {
        AaptParser mockParser = Mockito.mock(AaptParser.class);
        when(mockParser.getPackageName()).thenReturn(packageName);
        mAaptParsers.put(apk, mockParser);
    }

    private TestInformation makeTestInfo(ITestDevice device) {
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", device);
        context.addDeviceBuildInfo("device", new BuildInfo());
        return TestInformation.newBuilder().setInvocationContext(context).build();
    }

    // With no options or device property, only the default helpers should be installed.
    @Test
    public void testNoOptionsNoDeviceProperty() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);

            expectHelperInstall(baseApk);
            expectDeviceProperty(null);

            EasyMock.replay(mMockDevice);
            mPreparer.setUp(testInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // With no options and an empty device property, only the default helpers should be installed.
    @Test
    public void testNoOptionsEmptyDeviceProperty() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);

            expectHelperInstall(baseApk);
            expectDeviceProperty("");

            EasyMock.replay(mMockDevice);
            mPreparer.setUp(testInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // With no options and a valid device property, the default helpers and the helpers listed in
    // the property should all be installed.
    @Test
    public void testNoOptionsValidDeviceProperty() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);

            // Extra apks should be found based on device property.
            File extraApk = createFindableFile(testsDir, "com.helper1.apk", testInfo);
            mockApkPackage(extraApk, "com.helper1");
            File extraApk2 = createFindableFile(testsDir, "com.helper2.apk", testInfo);
            mockApkPackage(extraApk2, "com.helper2");

            expectHelperInstall(baseApk);
            expectDeviceProperty("com.helper1:com.helper2");
            EasyMock.checkOrder(mMockDevice, false); // Package install order is nondeterministic.
            expectHelperInstall(extraApk);
            expectHelperInstall(extraApk2);

            EasyMock.replay(mMockDevice);
            mPreparer.setUp(testInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // If an explicit default package name is passed, it should be used to install the default
    // helpers instead of the default name.
    @Test
    public void testFallbackPackageOptionOverridesDefault() throws Exception {
        mOptionSetter.setOptionValue("default-package", "com.fallback");
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);
            File otherApk = createFindableFile(testsDir, "com.fallback.apk", testInfo);
            mockApkPackage(otherApk, "com.fallback");

            expectHelperInstall(otherApk);
            expectDeviceProperty(null);

            EasyMock.replay(mMockDevice);
            mPreparer.setUp(testInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // If an explicit device property name is passed, that property should be read instead of the
    // default.  The default helpers will still be installed.
    @Test
    public void testDevicePropertyOptionOverridesDefault() throws Exception {
        mOptionSetter.setOptionValue("property-name", "ro.other_property");
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);

            // Extra apks should be found based on device property.
            File extraApk = createFindableFile(testsDir, "com.helper1.apk", testInfo);
            mockApkPackage(extraApk, "com.helper1");
            File extraApk2 = createFindableFile(testsDir, "com.helper2.apk", testInfo);
            mockApkPackage(extraApk2, "com.helper2");

            expectHelperInstall(baseApk);
            EasyMock.expect(mMockDevice.getProperty("ro.other_property"))
                    .andReturn("com.helper1:com.helper2");
            EasyMock.checkOrder(mMockDevice, false); // Package install order is nondeterministic.
            expectHelperInstall(extraApk);
            expectHelperInstall(extraApk2);

            EasyMock.replay(mMockDevice);
            mPreparer.setUp(testInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // Attempting to install an apk that can't be parsed will throw BuildError.
    @Test
    public void testInvalidApkWontInstall() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");

        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);
            File extraApk = createFindableFile(testsDir, "MyCtsHelpers.apk", testInfo);

            expectHelperInstall(baseApk);
            expectDeviceProperty("MyCtsHelpers");

            EasyMock.replay(mMockDevice);
            try {
                mPreparer.setUp(testInfo);
                fail("BuildError not thrown");
            } catch (BuildError e) {
                // expected
            }

            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // Attempting to install an apk that doesn't contain a package name will throw BuildError.
    @Test
    public void testApkWithoutPackageWontInstall() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");

        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);
            File extraApk = createFindableFile(testsDir, "MyCtsHelpers.apk", testInfo);
            mockApkPackage(extraApk, "");

            expectHelperInstall(baseApk);
            expectDeviceProperty("MyCtsHelpers");

            EasyMock.replay(mMockDevice);
            try {
                mPreparer.setUp(testInfo);
                fail("BuildError not thrown");
            } catch (BuildError e) {
                // expected
            }

            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // Attempting to install an apk with a package name that doesn't match the file name will
    // throw BuildError because the file name came from the device search path.
    @Test
    public void testApkWithBadPackageWontInstall() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");

        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);
            File extraApk = createFindableFile(testsDir, "MyCtsHelpers.apk", testInfo);
            mockApkPackage(extraApk, "NotMyCtsHelpers");

            expectHelperInstall(baseApk);
            expectDeviceProperty("MyCtsHelpers");

            EasyMock.replay(mMockDevice);
            try {
                mPreparer.setUp(testInfo);
                fail("BuildError not thrown");
            } catch (BuildError e) {
                // expected
            }
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // If a package listed in the device's search path is not found, the default helpers and other
    // requested packages will still be installed.  No exception is thrown.
    @Test
    public void testMissingPackageFromDevicePropertyDoesntAbort() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);
            File extraApk = createFindableFile(testsDir, "com.helper2.apk", testInfo);
            mockApkPackage(extraApk, "com.helper2");

            expectHelperInstall(baseApk);
            expectDeviceProperty("MyMissingHelpers:com.helper2");
            // Skips installing MyMissingHelpers and installs remaining packages in the list.
            expectHelperInstall(extraApk);

            EasyMock.replay(mMockDevice);
            mPreparer.setUp(testInfo);
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // If the fallback helpers cannot be found, BuildError is thrown and no other packages
    // will be installed.
    @Test
    public void testDefaultHelpersNotFound() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);

            // Should throw TargetSetupError after failing to find the default helpers.
            // No search for other files.

            EasyMock.replay(mMockDevice);
            try {
                mPreparer.setUp(testInfo);
                fail("BuildError not thrown");
            } catch (BuildError e) {
                // expected
            }
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    // If a package can't be installed, TargetSetupError is thrown.
    @Test
    public void testFailedInstallAborts() throws Exception {
        File testsDir = FileUtil.createTempDir("tests_dir");
        try {
            TestInformation testInfo = makeTestInfo(mMockDevice);
            File baseApk =
                    createFindableFile(testsDir, "com.android.cts.helpers.aosp.apk", testInfo);

            EasyMock.expect(mMockDevice.isAppEnumerationSupported()).andReturn(true);
            EasyMock.expect(
                            mMockDevice.installPackage(
                                    EasyMock.eq(baseApk),
                                    EasyMock.anyBoolean(),
                                    EasyMock.anyObject()))
                    .andReturn("Install failed");
            // Should throw TargetSetupError after failing to install and not do any further steps.

            EasyMock.replay(mMockDevice);
            try {
                mPreparer.setUp(testInfo);
                fail("TargetSetupError not thrown");
            } catch (TargetSetupError e) {
                // expected
            }
            EasyMock.verify(mMockDevice);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }
}
