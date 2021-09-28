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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.ApexInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.BundletoolUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.FileUtil;

import com.google.common.collect.ImmutableSet;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit test for {@link InstallApexModuleTargetPreparer} */
@RunWith(JUnit4.class)
public class InstallApexModuleTargetPreparerTest {

    private static final String SERIAL = "serial";
    private InstallApexModuleTargetPreparer mInstallApexModuleTargetPreparer;
    private IBuildInfo mMockBuildInfo;
    private ITestDevice mMockDevice;
    private TestInformation mTestInfo;
    private BundletoolUtil mMockBundletoolUtil;
    private File mFakeApex;
    private File mFakeApk;
    private File mFakeApk2;
    private File mFakePersistentApk;
    private File mFakeApkApks;
    private File mFakeApexApks;
    private File mBundletoolJar;
    private OptionSetter mSetter;
    private static final String APEX_PACKAGE_NAME = "com.android.FAKE_APEX_PACKAGE_NAME";
    private static final String APK_PACKAGE_NAME = "com.android.FAKE_APK_PACKAGE_NAME";
    private static final String APK2_PACKAGE_NAME = "com.android.FAKE_APK2_PACKAGE_NAME";
    private static final String PERSISTENT_APK_PACKAGE_NAME = "com.android.PERSISTENT_PACKAGE_NAME";
    private static final String SPLIT_APEX_PACKAGE_NAME =
            "com.android.SPLIT_FAKE_APEX_PACKAGE_NAME";
    private static final String SPLIT_APK_PACKAGE_NAME =
        "com.android.SPLIT_FAKE_APK_PACKAGE_NAME";
    private static final String APEX_PACKAGE_KEYWORD = "FAKE_APEX_PACKAGE_NAME";
    private static final long APEX_VERSION = 1;
    private static final String APEX_NAME = "fakeApex.apex";
    private static final String APK_NAME = "fakeApk.apk";
    private static final String APK2_NAME = "fakeSecondApk.apk";
    private static final String PERSISTENT_APK_NAME = "fakePersistentApk.apk";
    private static final String SPLIT_APEX_APKS_NAME = "fakeApex.apks";
    private static final String SPLIT_APK__APKS_NAME = "fakeApk.apks";
    private static final String BUNDLETOOL_JAR_NAME = "bundletool.jar";
    private static final String APEX_DATA_DIR = "/data/apex/active/";
    private static final String STAGING_DATA_DIR = "/data/app-staging/";
    private static final String SESSION_DATA_DIR = "/data/apex/sessions/";
    private static final String APEX_STAGING_WAIT_TIME = "10";

    @Before
    public void setUp() throws Exception {
        mFakeApex = FileUtil.createTempFile("fakeApex", ".apex");
        mFakeApk = FileUtil.createTempFile("fakeApk", ".apk");
        mFakeApk2 = FileUtil.createTempFile("fakeSecondApk", ".apk");
        mFakePersistentApk = FileUtil.createTempFile("fakePersistentApk", ".apk");
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        mMockBundletoolUtil = Mockito.mock(BundletoolUtil.class);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn(SERIAL);
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andStubReturn(null);
        EasyMock.expect(mMockDevice.checkApiLevelAgainstNextRelease(30)).andStubReturn(true);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();

        mInstallApexModuleTargetPreparer =
                new InstallApexModuleTargetPreparer() {
                    @Override
                    protected String getModuleKeywordFromApexPackageName(String packageName) {
                        return APEX_PACKAGE_KEYWORD;
                    }

                    @Override
                    protected String getBundletoolFileName() {
                        return BUNDLETOOL_JAR_NAME;
                    }

                    @Override
                    protected BundletoolUtil getBundletoolUtil() {
                        return mMockBundletoolUtil;
                    }

                    @Override
                    protected File getLocalPathForFilename(
                            TestInformation testInfo, String appFileName) throws TargetSetupError {
                        if (appFileName.endsWith(".apex")) {
                            return mFakeApex;
                        }
                        if (appFileName.endsWith(".apk")) {
                            if (appFileName.contains("Second")) {
                                return mFakeApk2;
                            } else if (appFileName.contains("Persistent")) {
                                return mFakePersistentApk;
                            } else {
                                return mFakeApk;
                            }
                        }
                        if (appFileName.endsWith(".apks")) {
                            if (appFileName.contains("Apex")) {
                                return mFakeApexApks;
                            }
                            if (appFileName.contains("Apk")) {
                                return mFakeApkApks;
                            }
                        }
                        if (appFileName.endsWith(".jar")) {
                            return mBundletoolJar;
                        }
                        return null;
                    }

                    @Override
                    protected String parsePackageName(
                            File testAppFile, DeviceDescriptor deviceDescriptor) {
                        if (testAppFile.getName().endsWith(".apex")) {
                            return APEX_PACKAGE_NAME;
                        }
                        if (testAppFile.getName().endsWith(".apk")
                                && !testAppFile.getName().contains("Split")) {
                            if (testAppFile.getName().contains("Second")) {
                                return APK2_PACKAGE_NAME;
                            } else if (testAppFile.getName().contains("Persistent")) {
                                return PERSISTENT_APK_PACKAGE_NAME;
                            } else {
                                return APK_PACKAGE_NAME;
                            }
                        }
                        if (testAppFile.getName().endsWith(".apk")
                                && testAppFile.getName().contains("Split")) {
                            return SPLIT_APK_PACKAGE_NAME;
                        }
                        if (testAppFile.getName().endsWith(".apks")
                                && testAppFile.getName().contains("fakeApk")) {
                            return SPLIT_APK_PACKAGE_NAME;
                        }
                        return null;
                    }

                    @Override
                    protected ApexInfo retrieveApexInfo(
                            File apex, DeviceDescriptor deviceDescriptor) {
                        ApexInfo apexInfo;
                        if (apex.getName().contains("Split")) {
                            apexInfo = new ApexInfo(SPLIT_APEX_PACKAGE_NAME, APEX_VERSION);
                        } else {
                            apexInfo = new ApexInfo(APEX_PACKAGE_NAME, APEX_VERSION);
                        }
                        return apexInfo;
                    }

                    @Override
                    protected boolean isPersistentApk(File filename, TestInformation testInfo)
                            throws TargetSetupError {
                        if (filename.getName().contains("Persistent")) {
                            return true;
                        }
                        return false;
                    }
                };

        mSetter = new OptionSetter(mInstallApexModuleTargetPreparer);
        mSetter.setOptionValue("cleanup-apks", "true");
        mSetter.setOptionValue("apex-staging-wait-time", APEX_STAGING_WAIT_TIME);
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.deleteFile(mFakeApex);
        FileUtil.deleteFile(mFakeApk);
        FileUtil.deleteFile(mFakeApk2);
        FileUtil.deleteFile(mFakePersistentApk);
    }

    @Test
    public void testSetupSuccess_removeExistingStagedApexSuccess() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupSuccess_noDataUnderApexDataDirs() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        CommandResult res = new CommandResult();
        res.setStdout("");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);
        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupSuccess_getActivatedPackageSuccess() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test(expected = TargetSetupError.class)
    public void testSetupFail_getActivatedPackageSuccessThrowModuleNotPreloaded() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(2);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(new HashSet<>());

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupFail_getActivatedPackageFail() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(new HashSet<ApexInfo>()).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        try {
            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            fail("Should have thrown a TargetSetupError.");
        } catch (TargetSetupError expected) {
            assertTrue(
                    expected.getMessage()
                            .contains("Failed to retrieve activated apex on device serial."));
        } finally {
            EasyMock.verify(mMockBuildInfo, mMockDevice);
        }
    }

    @Test
    public void testSetupFail_apexActivationFailPackageNameWrong() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME_TO_FAIL",
                        1,
                        "/system/apex/com.android.FAKE_APEX_PACKAGE_NAME_TO_FAIL.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        try {
            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            fail("Should have thrown a TargetSetupError.");
        } catch (TargetSetupError expected) {
            String failureMsg =
                    String.format(
                            "packageName: %s, versionCode: %d", APEX_PACKAGE_NAME, APEX_VERSION);
            assertTrue(expected.getMessage().contains(failureMsg));
        } finally {
            EasyMock.verify(mMockBuildInfo, mMockDevice);
        }
    }

    @Test
    public void testSetupFail_apexActivationFailVersionWrong() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        0,
                        "/system/apex/com.android.FAKE_APEX_PACKAGE_NAME.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        try {
            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            fail("Should have thrown a TargetSetupError.");
        } catch (TargetSetupError expected) {
            String failureMsg =
                    String.format(
                            "packageName: %s, versionCode: %d", APEX_PACKAGE_NAME, APEX_VERSION);
            assertTrue(expected.getMessage().contains(failureMsg));
        } finally {
            EasyMock.verify(mMockBuildInfo, mMockDevice);
        }
    }

    @Test
    public void testSetupFail_apexActivationFailSourceDirWrong() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/system/apex/com.android.FAKE_APEX_PACKAGE_NAME.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        try {
            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            fail("Should have thrown a TargetSetupError.");
        } catch (TargetSetupError expected) {
            String failureMsg =
                    String.format(
                            "packageName: %s, versionCode: %d", APEX_PACKAGE_NAME, APEX_VERSION);
            assertTrue(expected.getMessage().contains(failureMsg));
        } finally {
            EasyMock.verify(mMockBuildInfo, mMockDevice);
        }
    }

    @Test
    public void testSetupSuccess_activatedSuccessOnQ() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        EasyMock.expect(mMockDevice.checkApiLevelAgainstNextRelease(EasyMock.anyInt()))
                .andReturn(false);
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(new ApexInfo("com.android.FAKE_APEX_PACKAGE_NAME", 1, ""));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupAndTearDown_SingleApk() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        //TODO:add back once new adb is deployed to the lab
        // List<String> trainInstallCmd = new ArrayList<>();
        // trainInstallCmd.add("install-multi-package");
        // trainInstallCmd.add(mFakeApk.getAbsolutePath());
        // EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
        //         .andReturn("Success")
        //         .once();
        EasyMock.expect(mMockDevice.installPackage((File) EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null)
                .once();
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(new HashSet<ApexInfo>()).times(1);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APK_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(ImmutableSet.of());

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupAndTearDown_InstallMultipleApk() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK2_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        List<File> apks = new ArrayList<>();
        apks.add(mFakeApk);
        apks.add(mFakeApk2);
        mockSuccessfulInstallMultiApkWithoutReboot(apks);
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        EasyMock.expect(mMockDevice.uninstallPackage(APK2_PACKAGE_NAME)).andReturn(null).once();
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(new HashSet<ApexInfo>()).times(1);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APK_PACKAGE_NAME);
        installableModules.add(APK2_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(ImmutableSet.of());

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupAndTearDown_InstallMultipleApkContainingPersistentApk() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK2_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(PERSISTENT_APK_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        List<String> trainInstallCmd = new ArrayList<>();
        trainInstallCmd.add("install-multi-package");
        trainInstallCmd.add("--staged");
        trainInstallCmd.add(mFakeApk.getAbsolutePath());
        trainInstallCmd.add(mFakeApk2.getAbsolutePath());
        trainInstallCmd.add(mFakePersistentApk.getAbsolutePath());
        EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
                .andReturn("Success")
                .once();
        mMockDevice.reboot();
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        EasyMock.expect(mMockDevice.uninstallPackage(APK2_PACKAGE_NAME)).andReturn(null).once();
        EasyMock.expect(mMockDevice.uninstallPackage(PERSISTENT_APK_PACKAGE_NAME))
                .andReturn(null)
                .once();
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(new HashSet<ApexInfo>()).times(1);
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APK_PACKAGE_NAME);
        installableModules.add(APK2_PACKAGE_NAME);
        installableModules.add(PERSISTENT_APK_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(ImmutableSet.of());
        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupAndTearDown_ApkAndApks() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(SPLIT_APK__APKS_NAME);
        mFakeApkApks = File.createTempFile("fakeApk", ".apks");

        File fakeSplitApkApks = File.createTempFile("ApkSplits", "");
        fakeSplitApkApks.delete();
        fakeSplitApkApks.mkdir();
        File splitApk1 = File.createTempFile("fakeSplitApk1", ".apk", fakeSplitApkApks);
        mBundletoolJar = File.createTempFile("bundletool", ".jar");
        File splitApk2 = File.createTempFile("fakeSplitApk2", ".apk", fakeSplitApkApks);
        try {
            mMockDevice.deleteFile(APEX_DATA_DIR + "*");
            EasyMock.expectLastCall().times(1);
            mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
            EasyMock.expectLastCall().times(1);
            mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
            EasyMock.expectLastCall().times(1);
            CommandResult res = new CommandResult();
            res.setStdout("test.apex");
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR))
                .andReturn(res);
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR))
                .andReturn(res);
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR))
                .andReturn(res);
            mMockDevice.reboot();
            EasyMock.expectLastCall();
            when(mMockBundletoolUtil.generateDeviceSpecFile(Mockito.any(ITestDevice.class)))
                .thenReturn("serial.json");

            assertTrue(fakeSplitApkApks != null);
            assertTrue(mFakeApkApks != null);
            assertEquals(2, fakeSplitApkApks.listFiles().length);

            when(mMockBundletoolUtil.extractSplitsFromApks(
                Mockito.eq(mFakeApkApks),
                Mockito.anyString(),
                Mockito.any(ITestDevice.class),
                Mockito.any(IBuildInfo.class)))
                .thenReturn(fakeSplitApkApks);

            mMockDevice.waitForDeviceAvailable();

            List<String> trainInstallCmd = new ArrayList<>();
            trainInstallCmd.add("install-multi-package");
            trainInstallCmd.add(mFakeApk.getAbsolutePath());
            String cmd = "";
            for (File f : fakeSplitApkApks.listFiles()) {
                if (!cmd.isEmpty()) {
                    cmd += ":" + f.getParentFile().getAbsolutePath() + "/" + f.getName();
                } else {
                    cmd += f.getParentFile().getAbsolutePath() + "/" + f.getName();
                }
            }
            trainInstallCmd.add(cmd);
            EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
                .andReturn("Success")
                .once();

            EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
            EasyMock.expect(mMockDevice.uninstallPackage(SPLIT_APK_PACKAGE_NAME))
                .andReturn(null)
                .once();
            EasyMock.expect(mMockDevice.getActiveApexes())
                    .andReturn(new HashSet<ApexInfo>())
                    .times(1);
            Set<String> installableModules = new HashSet<>();
            installableModules.add(APK_PACKAGE_NAME);
            installableModules.add(SPLIT_APK_PACKAGE_NAME);
            EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);
            EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(ImmutableSet.of());

            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
            Mockito.verify(mMockBundletoolUtil, times(1))
                .generateDeviceSpecFile(Mockito.any(ITestDevice.class));
            // Extract splits 1 time to get the package name for the module, and again during
            // installation.
            Mockito.verify(mMockBundletoolUtil, times(2))
                    .extractSplitsFromApks(
                            Mockito.eq(mFakeApkApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class));
            EasyMock.verify(mMockBuildInfo, mMockDevice);
            assertTrue(!mInstallApexModuleTargetPreparer.getApkInstalled().isEmpty());
        } finally {
            FileUtil.deleteFile(mFakeApexApks);
            FileUtil.deleteFile(mFakeApkApks);
            FileUtil.recursiveDelete(fakeSplitApkApks);
            FileUtil.deleteFile(fakeSplitApkApks);
            FileUtil.deleteFile(mBundletoolJar);
        }
    }

    @Test
    public void testSetupAndTearDown() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallPackageAndReboot(mFakeApex);
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);


        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testGetModuleKeyword() {
        mInstallApexModuleTargetPreparer = new InstallApexModuleTargetPreparer();
        final String testApex1PackageName = "com.android.foo";
        final String testApex2PackageName = "com.android.bar_test";
        assertEquals(
                "foo",
                mInstallApexModuleTargetPreparer.getModuleKeywordFromApexPackageName(
                        testApex1PackageName));
        assertEquals(
                "bar_test",
                mInstallApexModuleTargetPreparer.getModuleKeywordFromApexPackageName(
                        testApex2PackageName));
    }

    @Test
    public void testSetupAndTearDown_MultiInstall() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallMultiPackageAndReboot();
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        installableModules.add(APK_PACKAGE_NAME);

        EasyMock.expect(mMockDevice.getInstalledPackageNames())
                .andReturn(installableModules)
                .once();

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testInstallUsingBundletool() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(SPLIT_APEX_APKS_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(SPLIT_APK__APKS_NAME);
        mFakeApexApks = File.createTempFile("fakeApex", ".apks");
        mFakeApkApks = File.createTempFile("fakeApk", ".apks");

        File fakeSplitApexApks = File.createTempFile("ApexSplits", "");
        fakeSplitApexApks.delete();
        fakeSplitApexApks.mkdir();
        File splitApex = File.createTempFile("fakeSplitApex", ".apex", fakeSplitApexApks);

        File fakeSplitApkApks = File.createTempFile("ApkSplits", "");
        fakeSplitApkApks.delete();
        fakeSplitApkApks.mkdir();
        File splitApk1 = File.createTempFile("fakeSplitApk1", ".apk", fakeSplitApkApks);
        mBundletoolJar = File.createTempFile("bundletool", ".jar");
        File splitApk2 = File.createTempFile("fakeSplitApk2", ".apk", fakeSplitApkApks);
        try {
            mMockDevice.deleteFile(APEX_DATA_DIR + "*");
            EasyMock.expectLastCall().times(2);
            mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
            EasyMock.expectLastCall().times(2);
            mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
            EasyMock.expectLastCall().times(2);
            CommandResult res = new CommandResult();
            res.setStdout("test.apex");
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR))
                    .andReturn(res);
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR))
                    .andReturn(res);
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR))
                    .andReturn(res);
            mMockDevice.reboot();
            EasyMock.expectLastCall();
            when(mMockBundletoolUtil.generateDeviceSpecFile(Mockito.any(ITestDevice.class)))
                    .thenReturn("serial.json");

            assertTrue(fakeSplitApexApks != null);
            assertTrue(fakeSplitApkApks != null);
            assertTrue(mFakeApexApks != null);
            assertTrue(mFakeApkApks != null);
            assertEquals(1, fakeSplitApexApks.listFiles().length);
            assertEquals(2, fakeSplitApkApks.listFiles().length);

            when(mMockBundletoolUtil.extractSplitsFromApks(
                            Mockito.eq(mFakeApexApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class)))
                    .thenReturn(fakeSplitApexApks);

            when(mMockBundletoolUtil.extractSplitsFromApks(
                            Mockito.eq(mFakeApkApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class)))
                    .thenReturn(fakeSplitApkApks);

            mMockDevice.waitForDeviceAvailable();

            List<String> trainInstallCmd = new ArrayList<>();
            trainInstallCmd.add("install-multi-package");
            trainInstallCmd.add(splitApex.getAbsolutePath());
            String cmd = "";
            for (File f : fakeSplitApkApks.listFiles()) {
                if (!cmd.isEmpty()) {
                    cmd += ":" + f.getParentFile().getAbsolutePath() + "/" + f.getName();
                } else {
                    cmd += f.getParentFile().getAbsolutePath() + "/" + f.getName();
                }
            }
            trainInstallCmd.add(cmd);
            EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
                    .andReturn("Success")
                    .once();
            mMockDevice.reboot();
            Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
            activatedApex.add(
                    new ApexInfo(
                            SPLIT_APEX_PACKAGE_NAME,
                            1,
                            "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
            EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
            EasyMock.expect(mMockDevice.uninstallPackage(SPLIT_APK_PACKAGE_NAME))
                .andReturn(null)
                .once();
            mMockDevice.reboot();
            EasyMock.expectLastCall();
            Set<String> installableModules = new HashSet<>();
            installableModules.add(APEX_PACKAGE_NAME);
            installableModules.add(SPLIT_APK_PACKAGE_NAME);
            EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
            Mockito.verify(mMockBundletoolUtil, times(1))
                    .generateDeviceSpecFile(Mockito.any(ITestDevice.class));
            // Extract splits 1 time to get the package name for the module, and again during
            // installation.
            Mockito.verify(mMockBundletoolUtil, times(2))
                    .extractSplitsFromApks(
                            Mockito.eq(mFakeApexApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class));
            Mockito.verify(mMockBundletoolUtil, times(2))
                    .extractSplitsFromApks(
                            Mockito.eq(mFakeApkApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class));
            EasyMock.verify(mMockBuildInfo, mMockDevice);
        } finally {
            FileUtil.deleteFile(mFakeApexApks);
            FileUtil.deleteFile(mFakeApkApks);
            FileUtil.recursiveDelete(fakeSplitApexApks);
            FileUtil.deleteFile(fakeSplitApexApks);
            FileUtil.recursiveDelete(fakeSplitApkApks);
            FileUtil.deleteFile(fakeSplitApkApks);
            FileUtil.deleteFile(mBundletoolJar);
        }
    }

    @Test
    public void testInstallUsingBundletool_skipModuleNotPreloaded() throws Exception {
        mSetter.setOptionValue("ignore-if-module-not-preloaded", "true");
        mInstallApexModuleTargetPreparer.addTestFileName(SPLIT_APEX_APKS_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(SPLIT_APK__APKS_NAME);
        mFakeApexApks = File.createTempFile("fakeApex", ".apks");
        mFakeApkApks = File.createTempFile("fakeApk", ".apks");

        File fakeSplitApexApks = File.createTempFile("ApexSplits", "");
        fakeSplitApexApks.delete();
        fakeSplitApexApks.mkdir();
        File splitApex = File.createTempFile("fakeSplitApex", ".apex", fakeSplitApexApks);

        File fakeSplitApkApks = File.createTempFile("ApkSplits", "");
        fakeSplitApkApks.delete();
        fakeSplitApkApks.mkdir();
        File splitApk1 = File.createTempFile("fakeSplitApk1", ".apk", fakeSplitApkApks);
        mBundletoolJar = File.createTempFile("bundletool", ".jar");
        File splitApk2 = File.createTempFile("fakeSplitApk2", ".apk", fakeSplitApkApks);
        try {
            mMockDevice.deleteFile(APEX_DATA_DIR + "*");
            EasyMock.expectLastCall().times(1);
            mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
            EasyMock.expectLastCall().times(1);
            mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
            EasyMock.expectLastCall().times(1);
            CommandResult res = new CommandResult();
            res.setStdout("test.apex");
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR))
                    .andReturn(res);
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR))
                    .andReturn(res);
            EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR))
                    .andReturn(res);
            mMockDevice.reboot();
            EasyMock.expectLastCall();
            when(mMockBundletoolUtil.generateDeviceSpecFile(Mockito.any(ITestDevice.class)))
                    .thenReturn("serial.json");

            assertTrue(fakeSplitApexApks != null);
            assertTrue(fakeSplitApkApks != null);
            assertTrue(mFakeApexApks != null);
            assertTrue(mFakeApkApks != null);
            assertEquals(1, fakeSplitApexApks.listFiles().length);
            assertEquals(2, fakeSplitApkApks.listFiles().length);

            when(mMockBundletoolUtil.extractSplitsFromApks(
                            Mockito.eq(mFakeApexApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class)))
                    .thenReturn(fakeSplitApexApks);

            when(mMockBundletoolUtil.extractSplitsFromApks(
                            Mockito.eq(mFakeApkApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class)))
                    .thenReturn(fakeSplitApkApks);

            Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
            activatedApex.add(
                    new ApexInfo(
                            SPLIT_APEX_PACKAGE_NAME,
                            1,
                            "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
            EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(2);
            EasyMock.expect(mMockDevice.uninstallPackage(SPLIT_APK_PACKAGE_NAME))
                    .andReturn(null)
                    .once();
            Set<String> installableModules = new HashSet<>();
            installableModules.add(SPLIT_APK_PACKAGE_NAME);
            EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(installableModules);

            EasyMock.replay(mMockBuildInfo, mMockDevice);
            mInstallApexModuleTargetPreparer.setUp(mTestInfo);
            mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
            Mockito.verify(mMockBundletoolUtil, times(1))
                    .generateDeviceSpecFile(Mockito.any(ITestDevice.class));
            // Extract splits 1 time to get the package name for the module, does not attempt to
            // install.
            Mockito.verify(mMockBundletoolUtil, times(1))
                    .extractSplitsFromApks(
                            Mockito.eq(mFakeApexApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class));
            Mockito.verify(mMockBundletoolUtil, times(2))
                    .extractSplitsFromApks(
                            Mockito.eq(mFakeApkApks),
                            Mockito.anyString(),
                            Mockito.any(ITestDevice.class),
                            Mockito.any(IBuildInfo.class));
            EasyMock.verify(mMockBuildInfo, mMockDevice);
        } finally {
            FileUtil.deleteFile(mFakeApexApks);
            FileUtil.deleteFile(mFakeApkApks);
            FileUtil.recursiveDelete(fakeSplitApexApks);
            FileUtil.deleteFile(fakeSplitApexApks);
            FileUtil.recursiveDelete(fakeSplitApkApks);
            FileUtil.deleteFile(fakeSplitApkApks);
            FileUtil.deleteFile(mBundletoolJar);
        }
    }

    /** Test that teardown without setup does not cause a NPE. */
    @Test
    public void testTearDown() throws Exception {
        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    private void mockSuccessfulInstallPackageAndReboot(File f) throws Exception {
        //TODO:add back once new adb is deployed to the lab
        // List<String> trainInstallCmd = new ArrayList<>();
        // trainInstallCmd.add("install-multi-package");
        // trainInstallCmd.add(f.getAbsolutePath());
        // EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
        //         .andReturn("Success")
        //         .once();
        EasyMock.expect(mMockDevice.installPackage((File) EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null)
                .once();
        mMockDevice.reboot();
        EasyMock.expectLastCall().once();
    }

    private void mockSuccessfulInstallMultiApkWithoutReboot(List<File> apks) throws Exception {
        List<String> trainInstallCmd = new ArrayList<>();
        trainInstallCmd.add("install-multi-package");
        for (File apk : apks) {
            trainInstallCmd.add(apk.getAbsolutePath());
        }
        EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
                .andReturn("Success")
                .once();
    }

    private void mockSuccessfulInstallMultiPackageAndReboot() throws Exception {
        List<String> trainInstallCmd = new ArrayList<>();
        trainInstallCmd.add("install-multi-package");
        trainInstallCmd.add(mFakeApex.getAbsolutePath());
        trainInstallCmd.add(mFakeApk.getAbsolutePath());
        EasyMock.expect(mMockDevice.executeAdbCommand(trainInstallCmd.toArray(new String[0])))
                .andReturn("Success")
                .once();
        mMockDevice.reboot();
        EasyMock.expectLastCall().once();
    }

    @Test
    public void testSetupAndTearDown_noModulesPreloaded() throws Exception {
        mSetter.setOptionValue("ignore-if-module-not-preloaded", "true");
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(1);
        EasyMock.expect(mMockDevice.getInstalledPackageNames()).andReturn(new HashSet<String>());
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(ImmutableSet.of());
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(new HashSet<ApexInfo>()).times(1);
        mMockDevice.reboot();
        EasyMock.expectLastCall();

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupAndTearDown_skipModulesNotPreloaded() throws Exception {
        mSetter.setOptionValue("ignore-if-module-not-preloaded", "true");
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        // Module not preloaded.
        mInstallApexModuleTargetPreparer.addTestFileName(APK2_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        mockSuccessfulInstallMultiPackageAndReboot();
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/data/apex/active/com.android.FAKE_APEX_PACKAGE_NAME@1.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        installableModules.add(APK_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames())
                .andReturn(installableModules)
                .once();

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test(expected = TargetSetupError.class)
    public void testSetupAndTearDown_throwExceptionModulesNotPreloaded() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APEX_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(APK2_NAME);
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall().times(2);
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/system/apex/com.android.FAKE_APEX_PACKAGE_NAME.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(3);
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        mMockDevice.reboot();
        EasyMock.expectLastCall();
        Set<String> installableModules = new HashSet<>();
        installableModules.add(APEX_PACKAGE_NAME);
        installableModules.add(APK_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames())
                .andReturn(installableModules)
                .once();

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);
    }

    @Test
    public void testSetupAndTearDown_skipModulesThatFailToExtract() throws Exception {
        mInstallApexModuleTargetPreparer.addTestFileName(APK_NAME);
        mInstallApexModuleTargetPreparer.addTestFileName(SPLIT_APK__APKS_NAME);
        mFakeApkApks = File.createTempFile("fakeApk", ".apks");
        mBundletoolJar = File.createTempFile("bundletool", ".jar");
        mMockDevice.deleteFile(APEX_DATA_DIR + "*");
        EasyMock.expectLastCall();
        mMockDevice.deleteFile(SESSION_DATA_DIR + "*");
        EasyMock.expectLastCall();
        mMockDevice.deleteFile(STAGING_DATA_DIR + "*");
        EasyMock.expectLastCall();
        CommandResult res = new CommandResult();
        res.setStdout("test.apex");
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + APEX_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + SESSION_DATA_DIR)).andReturn(res);
        EasyMock.expect(mMockDevice.executeShellV2Command("ls " + STAGING_DATA_DIR)).andReturn(res);
        mMockDevice.reboot();
        Set<ApexInfo> activatedApex = new HashSet<ApexInfo>();
        activatedApex.add(
                new ApexInfo(
                        "com.android.FAKE_APEX_PACKAGE_NAME",
                        1,
                        "/system/apex/com.android.FAKE_APEX_PACKAGE_NAME.apex"));
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex);
        EasyMock.expectLastCall();
        Set<String> installableModules = new HashSet<>();
        installableModules.add(SPLIT_APK_PACKAGE_NAME);
        installableModules.add(APK_PACKAGE_NAME);
        EasyMock.expect(mMockDevice.getInstalledPackageNames())
                .andReturn(installableModules)
                .once();
        when(mMockBundletoolUtil.generateDeviceSpecFile(Mockito.any(ITestDevice.class)))
                .thenReturn("serial.json");

        when(mMockBundletoolUtil.extractSplitsFromApks(
                        Mockito.eq(mFakeApkApks),
                        Mockito.anyString(),
                        Mockito.any(ITestDevice.class),
                        Mockito.any(IBuildInfo.class)))
                .thenReturn(null);

        // Only install apk, throw no error for apks.
        EasyMock.expect(mMockDevice.installPackage((File) EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null)
                .once();
        EasyMock.expect(mMockDevice.uninstallPackage(APK_PACKAGE_NAME)).andReturn(null).once();
        EasyMock.expect(mMockDevice.getActiveApexes()).andReturn(activatedApex).times(1);

        EasyMock.replay(mMockBuildInfo, mMockDevice);
        mInstallApexModuleTargetPreparer.setUp(mTestInfo);
        mInstallApexModuleTargetPreparer.tearDown(mTestInfo, null);
        EasyMock.verify(mMockBuildInfo, mMockDevice);

        FileUtil.deleteFile(mFakeApkApks);
        FileUtil.deleteFile(mBundletoolJar);
    }
}
