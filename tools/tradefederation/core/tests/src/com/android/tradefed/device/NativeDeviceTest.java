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
package com.android.tradefed.device;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;

import com.android.ddmlib.AdbCommandRejectedException;
import com.android.ddmlib.FileListingService.FileEntry;
import com.android.ddmlib.IDevice;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.ddmlib.ShellCommandUnresponsiveException;
import com.android.ddmlib.SyncException;
import com.android.ddmlib.SyncException.SyncError;
import com.android.ddmlib.SyncService;
import com.android.ddmlib.SyncService.ISyncProgressMonitor;
import com.android.ddmlib.TimeoutException;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.NativeDevice.RebootMode;
import com.android.tradefed.host.HostOptions;
import com.android.tradefed.host.IHostOptions;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.util.Bugreport;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.ProcessInfo;
import com.android.tradefed.util.StreamUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.time.Clock;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link NativeDevice}. */
@RunWith(JUnit4.class)
public class NativeDeviceTest {

    private static final String MOCK_DEVICE_SERIAL = "serial";
    private static final String FAKE_NETWORK_SSID = "FakeNet";
    private static final String FAKE_NETWORK_PASSWORD ="FakePass";

    private IDevice mMockIDevice;
    private TestableAndroidNativeDevice mTestDevice;
    private IDeviceRecovery mMockRecovery;
    private IDeviceStateMonitor mMockStateMonitor;
    private IRunUtil mMockRunUtil;
    private IWifiHelper mMockWifi;
    private IDeviceMonitor mMockDvcMonitor;
    private IHostOptions mHostOptions;

    /**
     * A {@link TestDevice} that is suitable for running tests against
     */
    private class TestableAndroidNativeDevice extends NativeDevice {

        public boolean wasCalled = false;

        public TestableAndroidNativeDevice() {
            super(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        }

        @Override
        public void postBootSetup() {
            // too annoying to mock out postBootSetup actions everyone, so do nothing
        }

        @Override
        protected IRunUtil getRunUtil() {
            return mMockRunUtil;
        }

        @Override
        IHostOptions getHostOptions() {
            return mHostOptions;
        }
    }

    @Before
    public void setUp() throws Exception {
        mHostOptions = new HostOptions();
        mMockIDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(mMockIDevice.getSerialNumber()).andReturn(MOCK_DEVICE_SERIAL).anyTimes();
        mMockRecovery = EasyMock.createMock(IDeviceRecovery.class);
        mMockStateMonitor = EasyMock.createMock(IDeviceStateMonitor.class);
        mMockDvcMonitor = EasyMock.createMock(IDeviceMonitor.class);
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mMockWifi = EasyMock.createMock(IWifiHelper.class);
        mMockWifi.cleanUp();
        EasyMock.expectLastCall().anyTimes();

        // A TestDevice with a no-op recoverDevice() implementation
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public void recoverDevice() throws DeviceNotAvailableException {
                // ignore
            }

            @Override
            public IDevice getIDevice() {
                return mMockIDevice;
            }

            @Override
            IWifiHelper createWifiHelper() {
                return mMockWifi;
            }
        };
        mTestDevice.setRecovery(mMockRecovery);
        mTestDevice.setCommandTimeout(100);
        mTestDevice.setLogStartDelay(-1);
    }

    /**
     * Test return exception for package installation {@link NativeDevice#installPackage(File,
     * boolean, String...)}.
     */
    @Test
    public void testInstallPackages_exception() throws Exception {
        try {
            mTestDevice.installPackage(new File(""), false);
            fail("installPackage should have thrown an exception");
        } catch (UnsupportedOperationException expected) {
            // Expected
        }
    }

    /**
     * Test return exception for package installation {@link NativeDevice#uninstallPackage(String)}.
     */
    @Test
    public void testUninstallPackages_exception() throws Exception {
        try {
            mTestDevice.uninstallPackage("");
            fail("uninstallPackageForUser should have thrown an exception");
        } catch (UnsupportedOperationException expected) {
            // Expected
        }
    }

    /**
     * Test return exception for package installation {@link NativeDevice#installPackage(File,
     * boolean, boolean, String...)}.
     */
    @Test
    public void testInstallPackagesBool_exception() throws Exception {
        try {
            mTestDevice.installPackage(new File(""), false, false);
            fail("installPackage should have thrown an exception");
        } catch (UnsupportedOperationException expected) {
            // Expected
        }
    }

    /**
     * Test return exception for package installation {@link
     * NativeDevice#installPackageForUser(File, boolean, int, String...)}.
     */
    @Test
    public void testInstallPackagesForUser_exception() throws Exception {
        try {
            mTestDevice.installPackageForUser(new File(""), false, 0);
            fail("installPackageForUser should have thrown an exception");
        } catch (UnsupportedOperationException expected) {
            // Expected
        }
    }

    /**
     * Test return exception for package installation {@link
     * NativeDevice#installPackageForUser(File, boolean, boolean, int, String...)}.
     */
    @Test
    public void testInstallPackagesForUserWithPermission_exception() throws Exception {
        try {
            mTestDevice.installPackageForUser(new File(""), false, false, 0);
            fail("installPackageForUser should have thrown an exception");
        } catch (UnsupportedOperationException expected) {
            // Expected
        }
    }

    /** Unit test for {@link NativeDevice#getInstalledPackageNames()}. */
    @Test
    public void testGetInstalledPackageNames_exception() throws Exception {
        try {
            mTestDevice.getInstalledPackageNames();
            fail("getInstalledPackageNames should have thrown an exception");
        } catch (UnsupportedOperationException expected) {
            // Expected
        }
    }

    /** Unit test for {@link NativeDevice#getActiveApexes()}. */
    @Test
    public void testGetActiveApexes_exception() throws Exception {
        try {
            mTestDevice.getActiveApexes();
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getActiveApexes should have thrown an exception");
    }

    /** Unit test for {@link NativeDevice#getScreenshot()}. */
    @Test
    public void testGetScreenshot_exception() throws Exception {
        try {
            mTestDevice.getScreenshot();
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getScreenshot should have thrown an exception");
    }

    /** Unit test for {@link NativeDevice#pushDir(File, String)}. */
    @Test
    public void testPushDir_notADir() throws Exception {
        assertFalse(mTestDevice.pushDir(new File(""), ""));
    }

    /** Unit test for {@link NativeDevice#pushDir(File, String)}. */
    @Test
    public void testPushDir_childFile() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean pushFile(File localFile, String remoteFilePath)
                    throws DeviceNotAvailableException {
                return true;
            }
        };
        File testDir = FileUtil.createTempDir("pushDirTest");
        FileUtil.createTempFile("test1", ".txt", testDir);
        assertTrue(mTestDevice.pushDir(testDir, ""));
        FileUtil.recursiveDelete(testDir);
    }

    /** Unit test for {@link NativeDevice#pushDir(File, String)}. */
    @Test
    public void testPushDir_childDir() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String executeShellCommand(String cmd) throws DeviceNotAvailableException {
                return "";
            }
            @Override
            public boolean pushFile(File localFile, String remoteFilePath)
                    throws DeviceNotAvailableException {
                return false;
            }
        };
        File testDir = FileUtil.createTempDir("pushDirTest");
        File subDir = FileUtil.createTempDir("testSubDir", testDir);
        FileUtil.createTempDir("test1", subDir);
        assertTrue(mTestDevice.pushDir(testDir, ""));
        FileUtil.recursiveDelete(testDir);
    }

    /** Unit test for {@link NativeDevice#pushDir(File, String, Set)}. */
    @Test
    public void testPushDir_childDir_filtered() throws Exception {
        File testDir = FileUtil.createTempDir("pushDirTest");
        Set<String> filter = new HashSet<>();
        try {
            File subDir = FileUtil.createTempDir("testSubDir", testDir);
            File childDir = FileUtil.createTempDir("test1", subDir);
            filter.add(childDir.getName());
            mTestDevice =
                    new TestableAndroidNativeDevice() {
                        @Override
                        public String executeShellCommand(String cmd)
                                throws DeviceNotAvailableException {
                            // Ensure we never attempt to push the excluded child.
                            assertFalse(cmd.contains(childDir.getName()));
                            return "";
                        }

                        @Override
                        public boolean pushFile(File localFile, String remoteFilePath)
                                throws DeviceNotAvailableException {
                            return false;
                        }
                    };

            assertTrue(mTestDevice.pushDir(testDir, "/", filter));
        } finally {
            FileUtil.recursiveDelete(testDir);
        }
    }

    /** Test {@link NativeDevice#pullDir(String, File)} when the remote directory is empty. */
    @Test
    public void testPullDir_nothingToDo() throws Exception {
        final IFileEntry fakeEntry = EasyMock.createMock(IFileEntry.class);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public IFileEntry getFileEntry(FileEntry path)
                            throws DeviceNotAvailableException {
                        return fakeEntry;
                    }

                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "drwxr-xr-x root     root    somedirectory";
                    }
                };
        File dir = FileUtil.createTempDir("tf-test");
        Collection<IFileEntry> childrens = new ArrayList<>();
        EasyMock.expect(fakeEntry.getChildren(false)).andReturn(childrens);
        // Empty list of childen
        EasyMock.replay(fakeEntry);
        try {
            boolean res = mTestDevice.pullDir("/some_device_path/screenshots/", dir);
            assertTrue(res);
            assertTrue(dir.list().length == 0);
        } finally {
            FileUtil.recursiveDelete(dir);
        }
        EasyMock.verify(fakeEntry);
    }

    /**
     * Test {@link NativeDevice#pullDir(String, File)} when the remote directory has a file and a
     * directory.
     */
    @Test
    public void testPullDir() throws Exception {
        final IFileEntry fakeEntry = EasyMock.createMock(IFileEntry.class);
        final IFileEntry fakeDir = EasyMock.createMock(IFileEntry.class);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    private boolean mFirstCall = true;

                    @Override
                    public IFileEntry getFileEntry(FileEntry path)
                            throws DeviceNotAvailableException {
                        if (mFirstCall) {
                            mFirstCall = false;
                            return fakeEntry;
                        } else {
                            return fakeDir;
                        }
                    }

                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "drwxr-xr-x root     root    somedirectory";
                    }

                    @Override
                    public boolean pullFile(String remoteFilePath, File localFile)
                            throws DeviceNotAvailableException {
                        try {
                            // Just touch the file to make it appear.
                            localFile.createNewFile();
                        } catch (IOException e) {
                            throw new RuntimeException(e);
                        }
                        return true;
                    }
                };
        File dir = FileUtil.createTempDir("tf-test");
        Collection<IFileEntry> children = new ArrayList<>();
        IFileEntry fakeFile = EasyMock.createMock(IFileEntry.class);
        children.add(fakeFile);
        EasyMock.expect(fakeFile.isDirectory()).andReturn(false);
        EasyMock.expect(fakeFile.getName()).andReturn("fakeFile");
        EasyMock.expect(fakeFile.getFullPath()).andReturn("/some_device_path/fakeFile");

        children.add(fakeDir);
        EasyMock.expect(fakeDir.isDirectory()).andReturn(true);
        EasyMock.expect(fakeDir.getName()).andReturn("fakeDir");
        EasyMock.expect(fakeDir.getFullPath()).andReturn("/some_device_path/fakeDir");
        // #pullDir is being called on dir fakeDir to pull everything recursively.
        Collection<IFileEntry> fakeDirChildren = new ArrayList<>();
        EasyMock.expect(fakeDir.getChildren(false)).andReturn(fakeDirChildren);
        EasyMock.expect(fakeEntry.getChildren(false)).andReturn(children);

        EasyMock.replay(fakeEntry, fakeFile, fakeDir);
        try {
            boolean res = mTestDevice.pullDir("/some_device_path/", dir);
            assertTrue(res);
            assertEquals(2, dir.list().length);
            assertTrue(Arrays.asList(dir.list()).contains("fakeFile"));
            assertTrue(Arrays.asList(dir.list()).contains("fakeDir"));
        } finally {
            FileUtil.recursiveDelete(dir);
        }
        EasyMock.verify(fakeEntry, fakeFile, fakeDir);
    }

    /** Test pulling a directory when one of the pull fails. */
    @Test
    public void testPullDir_pullFail() throws Exception {
        final IFileEntry fakeEntry = EasyMock.createMock(IFileEntry.class);
        final IFileEntry fakeDir = EasyMock.createMock(IFileEntry.class);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    private boolean mFirstCall = true;
                    private boolean mFirstPull = true;

                    @Override
                    public IFileEntry getFileEntry(FileEntry path)
                            throws DeviceNotAvailableException {
                        if (mFirstCall) {
                            mFirstCall = false;
                            return fakeEntry;
                        } else {
                            return fakeDir;
                        }
                    }

                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "drwxr-xr-x root     root    somedirectory";
                    }

                    @Override
                    public boolean pullFile(String remoteFilePath, File localFile)
                            throws DeviceNotAvailableException {
                        if (mFirstPull) {
                            mFirstPull = false;
                            try {
                                // Just touch the file to make it appear.
                                localFile.createNewFile();
                            } catch (IOException e) {
                                throw new RuntimeException(e);
                            }
                            return true;
                        } else {
                            return false;
                        }
                    }
                };
        File dir = FileUtil.createTempDir("tf-test");
        Collection<IFileEntry> children = new ArrayList<>();
        IFileEntry fakeFile = EasyMock.createMock(IFileEntry.class);
        children.add(fakeFile);
        EasyMock.expect(fakeFile.isDirectory()).andReturn(false);
        EasyMock.expect(fakeFile.getName()).andReturn("fakeFile");
        EasyMock.expect(fakeFile.getFullPath()).andReturn("/some_device_path/fakeFile");

        children.add(fakeDir);
        EasyMock.expect(fakeDir.isDirectory()).andReturn(true);
        EasyMock.expect(fakeDir.getName()).andReturn("fakeDir");
        EasyMock.expect(fakeDir.getFullPath()).andReturn("/some_device_path/fakeDir");
        // #pullDir is being called on dir fakeDir to pull everything recursively.
        Collection<IFileEntry> fakeDirChildren = new ArrayList<>();
        IFileEntry secondLevelChildren = EasyMock.createMock(IFileEntry.class);
        fakeDirChildren.add(secondLevelChildren);
        EasyMock.expect(fakeDir.getChildren(false)).andReturn(fakeDirChildren);
        EasyMock.expect(fakeEntry.getChildren(false)).andReturn(children);

        EasyMock.expect(secondLevelChildren.isDirectory()).andReturn(false);
        EasyMock.expect(secondLevelChildren.getName()).andReturn("secondLevelChildren");
        EasyMock.expect(secondLevelChildren.getFullPath())
                .andReturn("/some_device_path/fakeDir/secondLevelChildren");

        EasyMock.replay(fakeEntry, fakeFile, fakeDir, secondLevelChildren);
        try {
            boolean res = mTestDevice.pullDir("/some_device_path/", dir);
            // If one of the pull fails, the full command is considered failed.
            assertFalse(res);
            assertEquals(2, dir.list().length);
            assertTrue(Arrays.asList(dir.list()).contains("fakeFile"));
            // The subdir was created
            assertTrue(Arrays.asList(dir.list()).contains("fakeDir"));
            // The last file failed to pull, so the dir is empty.
            assertEquals(0, new File(dir, "fakeDir").list().length);
        } finally {
            FileUtil.recursiveDelete(dir);
        }
        EasyMock.verify(fakeEntry, fakeFile, fakeDir, secondLevelChildren);
    }

    /**
     * Test that if the requested path is not a directory on the device side, we just fail directly.
     */
    @Test
    public void testPullDir_invalidPath() throws Exception {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "-rwxr-xr-x root     root    somefile";
                    }
                };
        File dir = FileUtil.createTempDir("tf-test");
        try {
            assertFalse(mTestDevice.pullDir("somefile", dir));
            assertTrue(dir.list().length == 0);
        } finally {
            FileUtil.recursiveDelete(dir);
        }
    }

    /** Unit test for {@link NativeDevice#getCurrentUser()}. */
    @Test
    public void testGetCurrentUser_exception() throws Exception {
        try {
            mTestDevice.getScreenshot();
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getCurrentUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getUserFlags(int)}. */
    @Test
    public void testGetUserFlags_exception() throws Exception {
        try {
            mTestDevice.getUserFlags(0);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getUserFlags should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getUserSerialNumber(int)}. */
    @Test
    public void testGetUserSerialNumber_exception() throws Exception {
        try {
            mTestDevice.getUserSerialNumber(0);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getUserSerialNumber should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#switchUser(int)}. */
    @Test
    public void testSwitchUser_exception() throws Exception {
        try {
            mTestDevice.switchUser(10);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("switchUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#switchUser(int, long)}. */
    @Test
    public void testSwitchUserTimeout_exception() throws Exception {
        try {
            mTestDevice.switchUser(10, 5*1000);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("switchUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#stopUser(int)}. */
    @Test
    public void testStopUser_exception() throws Exception {
        try {
            mTestDevice.stopUser(0);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("stopUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#stopUser(int, boolean, boolean)}. */
    @Test
    public void testStopUserFlags_exception() throws Exception {
        try {
            mTestDevice.stopUser(0, true, true);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("stopUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#startUser(int, boolean)}. */
    @Test
    public void testStartUserFlags_exception() throws Exception {
        try {
            mTestDevice.startUser(0, true);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("startUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#isUserRunning(int)}. */
    @Test
    public void testIsUserIdRunning_exception() throws Exception {
        try {
            mTestDevice.isUserRunning(0);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("stopUser should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#hasFeature(String)}. */
    @Test
    public void testHasFeature_exception() throws Exception {
        try {
            mTestDevice.hasFeature("feature:test");
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("hasFeature should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getSetting(String, String)}. */
    @Test
    public void testGetSettingSystemUser_exception() throws Exception {
        try {
            mTestDevice.getSetting("global", "wifi_on");
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getSettings should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getSetting(int, String, String)}. */
    @Test
    public void testGetSetting_exception() throws Exception {
        try {
            mTestDevice.getSetting(0, "global", "wifi_on");
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getSettings should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getAllSettings(String)}. */
    @Test
    public void testGetAllSettingsSystemUser_exception() throws Exception {
        try {
            mTestDevice.getAllSettings("global");
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getAllSettings should have thrown an exception");
    }

    /** Unit test for {@link NativeDevice#setSetting(String, String, String)}. */
    @Test
    public void testSetSettingSystemUser_exception() throws Exception {
        try {
            mTestDevice.setSetting("global", "wifi_on", "0");
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("putSettings should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#setSetting(int, String, String, String)}. */
    @Test
    public void testSetSetting_exception() throws Exception {
        try {
            mTestDevice.setSetting(0, "global", "wifi_on", "0");
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("putSettings should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getAndroidId(int)}. */
    @Test
    public void testGetAndroidId_exception() throws Exception {
        try {
            mTestDevice.getAndroidId(0);
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getAndroidId should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#getAndroidIds()}. */
    @Test
    public void testGetAndroidIds_exception() throws Exception {
        try {
            mTestDevice.getAndroidIds();
        } catch (UnsupportedOperationException onse) {
            return;
        }
        fail("getAndroidIds should have thrown an exception.");
    }

    /** Unit test for {@link NativeDevice#connectToWifiNetworkIfNeeded(String, String)}. */
    @Test
    public void testConnectToWifiNetworkIfNeeded_alreadyConnected()
            throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.checkConnectivity(mTestDevice.getOptions().getConnCheckUrl()))
                .andReturn(true);
        EasyMock.replay(mMockWifi);
        assertTrue(mTestDevice.connectToWifiNetworkIfNeeded(FAKE_NETWORK_SSID,
                FAKE_NETWORK_PASSWORD));
        EasyMock.verify(mMockWifi);
    }

    /** Unit test for {@link NativeDevice#connectToWifiNetwork(String, String)}. */
    @Test
    public void testConnectToWifiNetwork_success() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.connectToNetwork(FAKE_NETWORK_SSID, FAKE_NETWORK_PASSWORD,
                mTestDevice.getOptions().getConnCheckUrl(), false)).andReturn(true);
        Map<String, String> fakeWifiInfo = new HashMap<String, String>();
        fakeWifiInfo.put("bssid", FAKE_NETWORK_SSID);
        EasyMock.expect(mMockWifi.getWifiInfo()).andReturn(fakeWifiInfo);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.connectToWifiNetwork(FAKE_NETWORK_SSID,
                FAKE_NETWORK_PASSWORD));
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /**
     * Unit test for {@link NativeDevice#connectToWifiNetwork(String, String)} for a failure to
     * connect case.
     */
    @Test
    public void testConnectToWifiNetwork_failure() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.connectToNetwork(FAKE_NETWORK_SSID, FAKE_NETWORK_PASSWORD,
                mTestDevice.getOptions().getConnCheckUrl(), false)).andReturn(false)
                .times(mTestDevice.getOptions().getWifiAttempts());
        Map<String, String> fakeWifiInfo = new HashMap<String, String>();
        fakeWifiInfo.put("bssid", FAKE_NETWORK_SSID);
        EasyMock.expect(mMockWifi.getWifiInfo()).andReturn(fakeWifiInfo)
                .times(mTestDevice.getOptions().getWifiAttempts());
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expectLastCall().times(mTestDevice.getOptions().getWifiAttempts() - 1);
        EasyMock.replay(mMockWifi, mMockIDevice, mMockRunUtil);
        assertFalse(mTestDevice.connectToWifiNetwork(FAKE_NETWORK_SSID,
                FAKE_NETWORK_PASSWORD));
        EasyMock.verify(mMockWifi, mMockIDevice, mMockRunUtil);
    }

    /**
     * Unit test for {@link NativeDevice#connectToWifiNetwork(String, String)} for limiting the time
     * trying to connect to wifi.
     */
    @Test
    public void testConnectToWifiNetwork_maxConnectTime()
            throws DeviceNotAvailableException, ConfigurationException {
        OptionSetter deviceOptionSetter = new OptionSetter(mTestDevice.getOptions());
        deviceOptionSetter.setOptionValue("max-wifi-connect-time", "10000");
        Clock mockClock = Mockito.mock(Clock.class);
        mTestDevice.setClock(mockClock);
        EasyMock.expect(
                        mMockWifi.connectToNetwork(
                                FAKE_NETWORK_SSID,
                                FAKE_NETWORK_PASSWORD,
                                mTestDevice.getOptions().getConnCheckUrl(),
                                false))
                .andReturn(false)
                .times(2);
        Mockito.when(mockClock.millis())
                .thenReturn(Long.valueOf(0), Long.valueOf(6000), Long.valueOf(12000));
        Map<String, String> fakeWifiInfo = new HashMap<String, String>();
        fakeWifiInfo.put("bssid", FAKE_NETWORK_SSID);
        EasyMock.expect(mMockWifi.getWifiInfo()).andReturn(fakeWifiInfo).times(2);

        EasyMock.replay(mMockWifi, mMockIDevice);
        assertFalse(mTestDevice.connectToWifiNetwork(FAKE_NETWORK_SSID, FAKE_NETWORK_PASSWORD));
        EasyMock.verify(mMockWifi, mMockIDevice);
        Mockito.verify(mockClock, Mockito.times(3)).millis();
    }

    /** Unit test for {@link NativeDevice#connectToWifiNetwork(String, String, boolean)}. */
    @Test
    public void testConnectToWifiNetwork_scanSsid() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.connectToNetwork(FAKE_NETWORK_SSID, FAKE_NETWORK_PASSWORD,
                mTestDevice.getOptions().getConnCheckUrl(), true)).andReturn(true);
        Map<String, String> fakeWifiInfo = new HashMap<String, String>();
        fakeWifiInfo.put("bssid", FAKE_NETWORK_SSID);
        EasyMock.expect(mMockWifi.getWifiInfo()).andReturn(fakeWifiInfo);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.connectToWifiNetwork(FAKE_NETWORK_SSID,
                FAKE_NETWORK_PASSWORD, true));
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#checkWifiConnection(String)}. */
    @Test
    public void testCheckWifiConnection() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.isWifiEnabled()).andReturn(true);
        EasyMock.expect(mMockWifi.getSSID()).andReturn("\"" + FAKE_NETWORK_SSID + "\"");
        EasyMock.expect(mMockWifi.hasValidIp()).andReturn(true);
        EasyMock.expect(mMockWifi.checkConnectivity(mTestDevice.getOptions().getConnCheckUrl()))
                .andReturn(true);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.checkWifiConnection(FAKE_NETWORK_SSID));
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#checkWifiConnection(String)} for a failure. */
    @Test
    public void testCheckWifiConnection_failure() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.isWifiEnabled()).andReturn(false);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertFalse(mTestDevice.checkWifiConnection(FAKE_NETWORK_SSID));
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#isWifiEnabled()}. */
    @Test
    public void testIsWifiEnabled() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.isWifiEnabled()).andReturn(true);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.isWifiEnabled());
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /**
     * Unit test for {@link NativeDevice#isWifiEnabled()} with runtime exception from wifihelper.
     */
    @Test
    public void testIsWifiEnabled_exception() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.isWifiEnabled()).andThrow(new RuntimeException());
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertFalse(mTestDevice.isWifiEnabled());
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#disconnectFromWifi()}. */
    @Test
    public void testDisconnectFromWifi() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.disconnectFromNetwork()).andReturn(true);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.disconnectFromWifi());
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#enableNetworkMonitor()}. */
    @Test
    public void testEnableNetworkMonitor() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.stopMonitor()).andReturn(null);
        EasyMock.expect(mMockWifi.startMonitor(EasyMock.anyLong(),
                EasyMock.eq(mTestDevice.getOptions().getConnCheckUrl()))).andReturn(true);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.enableNetworkMonitor());
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#enableNetworkMonitor()} in case of failure. */
    @Test
    public void testEnableNetworkMonitor_failure() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.stopMonitor()).andReturn(null);
        EasyMock.expect(mMockWifi.startMonitor(EasyMock.anyLong(),
                EasyMock.eq(mTestDevice.getOptions().getConnCheckUrl()))).andReturn(false);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertFalse(mTestDevice.enableNetworkMonitor());
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#disableNetworkMonitor()}. */
    @Test
    public void testDisableNetworkMonitor() throws DeviceNotAvailableException {
        List<Long> samples = new ArrayList<Long>();
        samples.add(Long.valueOf(42));
        samples.add(Long.valueOf(256));
        samples.add(Long.valueOf(-1)); // failure to connect
        EasyMock.expect(mMockWifi.stopMonitor()).andReturn(samples);
        EasyMock.replay(mMockWifi, mMockIDevice);
        assertTrue(mTestDevice.disableNetworkMonitor());
        EasyMock.verify(mMockWifi, mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#reconnectToWifiNetwork()}. */
    @Test
    public void testReconnectToWifiNetwork() throws DeviceNotAvailableException {
        EasyMock.expect(mMockWifi.checkConnectivity(mTestDevice.getOptions().getConnCheckUrl()))
                .andReturn(false);
        EasyMock.expect(mMockWifi.checkConnectivity(mTestDevice.getOptions().getConnCheckUrl()))
                .andReturn(true);
        mMockRunUtil.sleep(EasyMock.anyLong());
        EasyMock.expectLastCall();
        EasyMock.replay(mMockWifi, mMockIDevice, mMockRunUtil);
        try {
            mTestDevice.reconnectToWifiNetwork();
        } finally {
            EasyMock.verify(mMockWifi, mMockIDevice, mMockRunUtil);
        }
    }

    /** Unit test for {@link NativeDevice#isHeadless()}. */
    @Test
    public void testIsHeadless() throws DeviceNotAvailableException {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return "1\n";
            }
        };
        assertTrue(mTestDevice.isHeadless());
    }

    /** Unit test for {@link NativeDevice#isHeadless()}. */
    @Test
    public void testIsHeadless_notHeadless() throws DeviceNotAvailableException {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return null;
            }
        };
        assertFalse(mTestDevice.isHeadless());
    }

    /** Unit test for {@link NativeDevice#getDeviceDate()}. */
    @Test
    public void testGetDeviceDate() throws DeviceNotAvailableException {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String executeShellCommand(String name) throws DeviceNotAvailableException {
                return "21692641\n";
            }
        };
        assertEquals(21692641000L, mTestDevice.getDeviceDate());
    }

    /** Unit test for {@link NativeDevice#logBugreport(String, ITestLogger)}. */
    @Test
    public void testTestLogBugreport() {
        final String dataName = "test";
        final InputStreamSource stream = new ByteArrayInputStreamSource(null);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public InputStreamSource getBugreportz() {
                        return stream;
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }
                };
        ITestLogger listener = EasyMock.createMock(ITestLogger.class);
        listener.testLog(dataName, LogDataType.BUGREPORTZ, stream);
        EasyMock.replay(listener);
        assertTrue(mTestDevice.logBugreport(dataName, listener));
        EasyMock.verify(listener);
    }

    /** Unit test for {@link NativeDevice#logBugreport(String, ITestLogger)}. */
    @Test
    public void testTestLogBugreport_oldDevice() {
        final String dataName = "test";
        final InputStreamSource stream = new ByteArrayInputStreamSource(null);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public InputStreamSource getBugreportz() {
                        // Older device do not support bugreportz and return null
                        return null;
                    }

                    @Override
                    public InputStreamSource getBugreportInternal() {
                        return stream;
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        // no bugreportz support
                        return 23;
                    }
                };
        ITestLogger listener = EasyMock.createMock(ITestLogger.class);
        listener.testLog(dataName, LogDataType.BUGREPORT, stream);
        EasyMock.replay(listener);
        assertTrue(mTestDevice.logBugreport(dataName, listener));
        EasyMock.verify(listener);
    }

    /** Unit test for {@link NativeDevice#logBugreport(String, ITestLogger)}. */
    @Test
    public void testTestLogBugreport_fail() {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public InputStreamSource getBugreportz() {
                        return null;
                    }

                    @Override
                    protected InputStreamSource getBugreportInternal() {
                        return null;
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 23;
                    }
                };
        ITestLogger listener = EasyMock.createMock(ITestLogger.class);
        EasyMock.replay(listener);
        assertFalse(mTestDevice.logBugreport("test", listener));
        EasyMock.verify(listener);
    }

    /** Unit test for {@link NativeDevice#takeBugreport()}. */
    @Test
    public void testTakeBugreport_apiLevelFail() {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        throw new DeviceNotAvailableException("test", "serial");
                    }
                };
        // If we can't check API level it should return null.
        assertNull(mTestDevice.takeBugreport());
    }

    /** Unit test for {@link NativeDevice#takeBugreport()}. */
    @Test
    public void testTakeBugreport_oldDevice() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public int getApiLevel() throws DeviceNotAvailableException {
                return 19;
            }
        };
        Bugreport report = mTestDevice.takeBugreport();
        try {
            assertNotNull(report);
            // older device report a non zipped bugreport
            assertFalse(report.isZipped());
        } finally {
            report.close();
        }
    }

    /** Unit test for {@link NativeDevice#takeBugreport()}. */
    @Test
    public void testTakeBugreport() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public int getApiLevel() throws DeviceNotAvailableException {
                return 24;
            }
            @Override
            protected File getBugreportzInternal() {
                try {
                    return FileUtil.createTempFile("bugreportz", ".zip");
                } catch (IOException e) {
                    return null;
                }
            }
        };
        Bugreport report = mTestDevice.takeBugreport();
        try {
            assertNotNull(report);
            assertTrue(report.isZipped());
        } finally {
            report.close();
        }
    }

    /** Unit test for {@link NativeDevice#getDeviceDate()}. */
    @Test
    public void testGetDeviceDate_wrongformat() throws DeviceNotAvailableException {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String executeShellCommand(String name) throws DeviceNotAvailableException {
                return "WRONG\n";
            }
        };
        assertEquals(0, mTestDevice.getDeviceDate());
    }

    @Test
    public void testGetBugreport_deviceUnavail() throws Exception {
        final String expectedOutput = "this is the output\r\n in two lines\r\n";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public void executeShellCommand(
                    String command, IShellOutputReceiver receiver,
                    long maxTimeToOutputShellResponse, TimeUnit timeUnit, int retryAttempts)
                            throws DeviceNotAvailableException {
                String fakeRep = expectedOutput;
                receiver.addOutput(fakeRep.getBytes(), 0, fakeRep.getBytes().length);
            }
            @Override
            public int getApiLevel() throws DeviceNotAvailableException {
                return 22;
            }
        };

        // FIXME: this isn't actually causing a DeviceNotAvailableException to be thrown
        mMockRecovery.recoverDevice(EasyMock.eq(mMockStateMonitor), EasyMock.eq(false));
        EasyMock.expectLastCall().andThrow(new DeviceNotAvailableException("test", "serial"));
        EasyMock.replay(mMockRecovery, mMockIDevice);
        assertEquals(expectedOutput, StreamUtil.getStringFromStream(
                mTestDevice.getBugreport().createInputStream()));
    }

    @Test
    public void testGetBugreport_compatibility_deviceUnavail() throws Exception {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public void executeShellCommand(
                            String command,
                            IShellOutputReceiver receiver,
                            long maxTimeToOutputShellResponse,
                            TimeUnit timeUnit,
                            int retryAttempts)
                            throws DeviceNotAvailableException {
                        throw new DeviceNotAvailableException("test", "serial");
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }

                    @Override
                    public IFileEntry getFileEntry(String path) throws DeviceNotAvailableException {
                        return null;
                    }
                };
        EasyMock.replay(mMockRecovery, mMockIDevice);
        assertEquals(0, mTestDevice.getBugreport().size());
        EasyMock.verify(mMockRecovery, mMockIDevice);
    }

    @Test
    public void testGetBugreport_deviceUnavail_fallback() throws Exception {
        final IFileEntry fakeEntry = EasyMock.createMock(IFileEntry.class);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public void executeShellCommand(
                            String command,
                            IShellOutputReceiver receiver,
                            long maxTimeToOutputShellResponse,
                            TimeUnit timeUnit,
                            int retryAttempts)
                            throws DeviceNotAvailableException {
                        throw new DeviceNotAvailableException("test", "serial");
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }

                    @Override
                    public IFileEntry getFileEntry(String path) throws DeviceNotAvailableException {
                        return fakeEntry;
                    }

                    @Override
                    public File pullFile(String remoteFilePath) throws DeviceNotAvailableException {
                        try {
                            return FileUtil.createTempFile("bugreport", ".txt");
                        } catch (IOException e) {
                            return null;
                        }
                    }
                };
        List<IFileEntry> list = new ArrayList<>();
        list.add(fakeEntry);
        EasyMock.expect(fakeEntry.getChildren(false)).andReturn(list);
        EasyMock.expect(fakeEntry.getName()).andReturn("bugreport-NYC-2016-08-17-10-17-00.tmp");
        EasyMock.replay(fakeEntry, mMockRecovery, mMockIDevice);
        InputStreamSource res = null;
        try {
            res = mTestDevice.getBugreport();
            assertNotNull(res);
            EasyMock.verify(fakeEntry, mMockRecovery, mMockIDevice);
        } finally {
            StreamUtil.cancel(res);
        }
    }

    /** Unit test for {@link NativeDevice#getBugreportz()}. */
    @Test
    public void testGetBugreportz() throws IOException {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public void executeShellCommand(
                            String command,
                            IShellOutputReceiver receiver,
                            long maxTimeToOutputShellResponse,
                            TimeUnit timeUnit,
                            int retryAttempts)
                            throws DeviceNotAvailableException {
                        String fakeRep =
                                "OK:/data/0/com.android.shell/bugreports/bugreport1970-10-27.zip";
                        receiver.addOutput(fakeRep.getBytes(), 0, fakeRep.getBytes().length);
                    }

                    @Override
                    public boolean doesFileExist(String destPath)
                            throws DeviceNotAvailableException {
                        return true;
                    }

                    @Override
                    public boolean pullFile(String remoteFilePath, File localFile)
                            throws DeviceNotAvailableException {
                        return true;
                    }

                    @Override
                    public void deleteFile(String deviceFilePath)
                            throws DeviceNotAvailableException {
                        assertEquals("/data/0/com.android.shell/bugreports/*", deviceFilePath);
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }
                };
        FileInputStreamSource f = null;
        try {
            f = (FileInputStreamSource) mTestDevice.getBugreportz();
            assertNotNull(f);
            assertTrue(f.createInputStream().available() == 0);
        } finally {
            StreamUtil.cancel(f);
            if (f != null) {
                f.cleanFile();
            }
        }
    }

    /** Unit test for {@link NativeDevice#getBugreportz()}. */
    @Test
    public void testGetBugreportz_fails() {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }

                    @Override
                    protected File getBugreportzInternal() {
                        return null;
                    }

                    @Override
                    public IFileEntry getFileEntry(String path) throws DeviceNotAvailableException {
                        return null;
                    }
                };
        FileInputStreamSource f = null;
        try {
            f = (FileInputStreamSource) mTestDevice.getBugreportz();
            assertNull(f);
        } finally {
            StreamUtil.cancel(f);
            if (f != null) {
                f.cleanFile();
            }
        }
    }

    @Test
    public void testGetBugreportz_fallBack_validation() throws Exception {
        IFileEntry entryMock = EasyMock.createMock(IFileEntry.class);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }

                    @Override
                    protected File getBugreportzInternal() {
                        return null;
                    }

                    @Override
                    public IFileEntry getFileEntry(String path) throws DeviceNotAvailableException {
                        return entryMock;
                    }

                    @Override
                    public File pullFile(String remoteFilePath) throws DeviceNotAvailableException {
                        try {
                            // Return an empty zip file for the partial bugreportz
                            return FileUtil.createTempFile("bugreportz-test", ".zip");
                        } catch (IOException e) {
                            throw new RuntimeException();
                        }
                    }
                };
        IFileEntry childNode = EasyMock.createMock(IFileEntry.class);
        EasyMock.expect(entryMock.getChildren(false)).andReturn(Arrays.asList(childNode));
        EasyMock.expect(childNode.getName()).andReturn("bugreport-test-partial.zip");
        FileInputStreamSource f = null;
        EasyMock.replay(entryMock, childNode);
        try {
            f = (FileInputStreamSource) mTestDevice.getBugreportz();
            assertNull(f);
        } finally {
            StreamUtil.cancel(f);
            if (f != null) {
                f.cleanFile();
            }
        }
        EasyMock.verify(entryMock, childNode);
    }

    /**
     * Test that we can distinguish a newer file even with Timezone on the device. Seoul is GMT+9.
     */
    @Test
    public void testIsNewer() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String getProperty(String name) throws DeviceNotAvailableException {
                        return "Asia/Seoul";
                    }

                    @Override
                    public long getDeviceTimeOffset(Date date) throws DeviceNotAvailableException {
                        return 0;
                    }
                };
        File localFile = FileUtil.createTempFile("timezonetest", ".txt");
        try {
            localFile.setLastModified(1470906000000l); // Thu Aug 11 09:00:00 GMT 2016
            IFileEntry remoteFile = EasyMock.createMock(IFileEntry.class);
            EasyMock.expect(remoteFile.getDate()).andReturn("2016-08-11");
            EasyMock.expect(remoteFile.getTime()).andReturn("18:00");
            EasyMock.replay(remoteFile);
            assertTrue(testDevice.isNewer(localFile, remoteFile));
            EasyMock.verify(remoteFile);
        } finally {
            FileUtil.deleteFile(localFile);
        }
    }

    /**
     * Test that we can distinguish a newer file even with Timezone on the device. Seoul is GMT+9.
     * Clock on device is inaccurate and in advance of host.
     */
    @Test
    public void testIsNewer_timeOffset() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String getProperty(String name) throws DeviceNotAvailableException {
                        return "Asia/Seoul";
                    }

                    @Override
                    public long getDeviceTimeOffset(Date date) throws DeviceNotAvailableException {
                        return -15 * 60 * 1000; // Device in advance of 15m on host.
                    }
                };
        File localFile = FileUtil.createTempFile("timezonetest", ".txt");
        try {
            localFile.setLastModified(1470906000000l); // Thu, 11 Aug 2016 09:00:00 GMT
            IFileEntry remoteFile = EasyMock.createMock(IFileEntry.class);
            EasyMock.expect(remoteFile.getDate()).andReturn("2016-08-11");
            EasyMock.expect(remoteFile.getTime()).andReturn("18:15");
            EasyMock.replay(remoteFile);
            // Should sync because after time offset correction, file is older.
            assertTrue(testDevice.isNewer(localFile, remoteFile));
            EasyMock.verify(remoteFile);
        } finally {
            FileUtil.deleteFile(localFile);
        }
    }

    /**
     * Test that we can distinguish a newer file even with Timezone on the device. Seoul is GMT+9.
     * Local file is set to 10min earlier than remoteFile.
     */
    @Test
    public void testIsNewer_fails() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String getProperty(String name) throws DeviceNotAvailableException {
                        return "Asia/Seoul";
                    }

                    @Override
                    public long getDeviceTimeOffset(Date date) throws DeviceNotAvailableException {
                        return 0;
                    }
                };
        File localFile = FileUtil.createTempFile("timezonetest", ".txt");
        try {
            localFile.setLastModified(1470906000000l); // Thu, 11 Aug 2016 09:00:00 GMT
            IFileEntry remoteFile = EasyMock.createMock(IFileEntry.class);
            EasyMock.expect(remoteFile.getDate()).andReturn("2016-08-11");
            EasyMock.expect(remoteFile.getTime()).andReturn("18:10");
            EasyMock.replay(remoteFile);
            assertFalse(testDevice.isNewer(localFile, remoteFile));
            EasyMock.verify(remoteFile);
        } finally {
            FileUtil.deleteFile(localFile);
        }
    }

    /** Unit test for {@link NativeDevice#getBuildAlias()}. */
    @Test
    public void testGetBuildAlias() throws DeviceNotAvailableException {
        final String alias = "alias\n";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return alias;
            }
        };
        assertEquals(alias, mTestDevice.getBuildAlias());
    }

    /** Unit test for {@link NativeDevice#getBuildAlias()}. */
    @Test
    public void testGetBuildAlias_null() throws DeviceNotAvailableException {
        final String alias = null;
        final String buildId = "alias\n";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return alias;
            }
            @Override
            public String getBuildId() throws DeviceNotAvailableException {
                return buildId;
            }
        };
        assertEquals(buildId, mTestDevice.getBuildAlias());
    }

    /** Unit test for {@link NativeDevice#getBuildAlias()}. */
    @Test
    public void testGetBuildAlias_empty() throws DeviceNotAvailableException {
        final String alias = "";
        final String buildId = "alias\n";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return alias;
            }
            @Override
            public String getBuildId() throws DeviceNotAvailableException {
                return buildId;
            }
        };
        assertEquals(buildId, mTestDevice.getBuildAlias());
    }

    /** Unit test for {@link NativeDevice#getBuildId()}. */
    @Test
    public void testGetBuildId() throws DeviceNotAvailableException {
        final String buildId = "299865";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return buildId;
            }
        };
        assertEquals(buildId, mTestDevice.getBuildId());
    }

    /** Unit test for {@link NativeDevice#getBuildId()}. */
    @Test
    public void testGetBuildId_null() throws DeviceNotAvailableException {
        final String buildId = null;
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return buildId;
            }
        };
        assertEquals(IBuildInfo.UNKNOWN_BUILD_ID, mTestDevice.getBuildId());
    }

    /** Unit test for {@link NativeDevice#getBuildFlavor()}. */
    @Test
    public void testGetBuildFlavor() throws DeviceNotAvailableException {
        final String flavor = "ham-user";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                return flavor;
            }
        };
        assertEquals(flavor, mTestDevice.getBuildFlavor());
    }

    /** Unit test for {@link NativeDevice#getBuildFlavor()}. */
    @Test
    public void testGetBuildFlavor_null_flavor() throws DeviceNotAvailableException {
        final String productName = "prod";
        final String buildType = "buildtype";
        String expected = String.format("%s-%s", productName, buildType);
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                if ("ro.build.flavor".equals(name)) {
                    return null;
                } else if ("ro.product.name".equals(name)) {
                    return productName;
                } else if ("ro.build.type".equals(name)) {
                    return buildType;
                } else {
                    return null;
                }
            }
        };
        assertEquals(expected, mTestDevice.getBuildFlavor());
    }

    /** Unit test for {@link NativeDevice#getBuildFlavor()}. */
    @Test
    public void testGetBuildFlavor_null() throws DeviceNotAvailableException {
        final String productName = null;
        final String buildType = "buildtype";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public String getProperty(String name) throws DeviceNotAvailableException {
                if ("ro.build.flavor".equals(name)) {
                    return "";
                } else if ("ro.product.name".equals(name)) {
                    return productName;
                } else if ("ro.build.type".equals(name)) {
                    return buildType;
                } else {
                    return null;
                }
            }
        };
        assertNull(mTestDevice.getBuildFlavor());
    }

    /** Unit test for {@link NativeDevice#doAdbReboot(RebootMode, String)} )}. */
    @Test
    public void testDoAdbReboot_emulator() throws Exception {
        final String into = "bootloader";
        mMockIDevice.reboot(into);
        EasyMock.expectLastCall();
        EasyMock.replay(mMockIDevice);
        mTestDevice.doAdbReboot(RebootMode.REBOOT_INTO_BOOTLOADER, "");
        EasyMock.verify(mMockIDevice);
    }

    /** Unit test for {@link NativeDevice#doReboot(RebootMode, String)} ()}. */
    @Test
    public void testDoReboot() throws Exception {
        NativeDevice testDevice = new NativeDevice(mMockIDevice,
                mMockStateMonitor, mMockDvcMonitor) {
            @Override
            public TestDeviceState getDeviceState() {
                return TestDeviceState.ONLINE;
            }
        };
        mMockIDevice.reboot(null);
        EasyMock.expectLastCall();
        EasyMock.expect(mMockStateMonitor.waitForDeviceNotAvailable(EasyMock.anyLong()))
                .andReturn(true);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.doReboot(RebootMode.REBOOT_FULL, null);
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#doReboot(RebootMode, String)} ()}. */
    @Test
    public void testDoReboot_skipped() throws Exception {
        NativeDevice testDevice = new NativeDevice(mMockIDevice,
                mMockStateMonitor, mMockDvcMonitor) {
            @Override
            public TestDeviceState getDeviceState() {
                mOptions = new TestDeviceOptions() {
                    @Override
                    public boolean shouldDisableReboot() {
                        return true;
                    }
                };
                return TestDeviceState.ONLINE;
            }
        };
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.doReboot(RebootMode.REBOOT_FULL, null);
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#doReboot(RebootMode, String)} ()}. */
    @Test
    public void testDoReboot_fastboot() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public TestDeviceState getDeviceState() {
                return TestDeviceState.FASTBOOT;
            }
            @Override
            public CommandResult executeFastbootCommand(String... cmdArgs)
                    throws DeviceNotAvailableException, UnsupportedOperationException {
                wasCalled = true;
                return new CommandResult();
            }
        };
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        mTestDevice.doReboot(RebootMode.REBOOT_FULL, null);
        assertTrue(mTestDevice.wasCalled);
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#rebootIntoSideload()}}. */
    @Test
    public void testRebootIntoSideload() throws Exception {
        NativeDevice testDevice =
                new NativeDevice(mMockIDevice, mMockStateMonitor, mMockDvcMonitor) {
                    @Override
                    public TestDeviceState getDeviceState() {
                        return TestDeviceState.ONLINE;
                    }
                };
        String into = "sideload";
        mMockIDevice.reboot(into);
        EasyMock.expectLastCall();
        EasyMock.expect(mMockStateMonitor.waitForDeviceInSideload(EasyMock.anyLong()))
                .andReturn(true);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.rebootIntoSideload();
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#rebootIntoBootloader()}}. */
    @Test
    public void testRebootIntoBootloader() throws Exception {
        NativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public TestDeviceState getDeviceState() {
                        return TestDeviceState.ONLINE;
                    }

                    @Override
                    public String getFastbootSerialNumber() {
                        return MOCK_DEVICE_SERIAL;
                    }
                };
        String into = "bootloader";
        mMockIDevice.reboot(into);
        EasyMock.expectLastCall();
        mMockStateMonitor.setFastbootSerialNumber(MOCK_DEVICE_SERIAL);
        EasyMock.expect(mMockStateMonitor.waitForDeviceBootloader(EasyMock.anyLong()))
                .andReturn(true);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.rebootIntoBootloader();
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /**
     * Unit test for {@link NativeDevice#rebootIntoBootloader()}} when device is already in fastboot
     * mode.
     */
    @Test
    public void testRebootIntoBootloader_forceFastboot() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public TestDeviceState getDeviceState() {
                        return TestDeviceState.FASTBOOT;
                    }

                    @Override
                    public CommandResult executeFastbootCommand(String... cmdArgs)
                            throws DeviceNotAvailableException, UnsupportedOperationException {
                        if (cmdArgs[0].equals("reboot-bootloader")) {
                            wasCalled = true;
                        }
                        return new CommandResult();
                    }

                    @Override
                    public String getFastbootSerialNumber() {
                        return MOCK_DEVICE_SERIAL;
                    }
                };
        mMockStateMonitor.setFastbootSerialNumber(MOCK_DEVICE_SERIAL);
        EasyMock.expect(mMockStateMonitor.waitForDeviceBootloader(EasyMock.anyLong()))
                .andReturn(true);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.rebootIntoBootloader();
        assertTrue(testDevice.wasCalled);
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#rebootIntoFastbootd()}}. */
    @Test
    public void testRebootIntoFastbootd() throws Exception {
        NativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public TestDeviceState getDeviceState() {
                        return TestDeviceState.ONLINE;
                    }

                    @Override
                    public String getFastbootSerialNumber() {
                        return MOCK_DEVICE_SERIAL;
                    }
                };
        String into = "fastboot";
        mMockIDevice.reboot(into);
        EasyMock.expectLastCall();
        mMockStateMonitor.setFastbootSerialNumber(MOCK_DEVICE_SERIAL);
        EasyMock.expect(mMockStateMonitor.waitForDeviceBootloader(EasyMock.anyLong()))
                .andReturn(true);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.rebootIntoFastbootd();
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /**
     * Unit test for {@link NativeDevice#rebootIntoFastbootd()}} when device is already in fastboot
     * mode.
     */
    @Test
    public void testRebootIntoFastbootd_forceFastboot() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public TestDeviceState getDeviceState() {
                        return TestDeviceState.FASTBOOT;
                    }

                    @Override
                    public CommandResult executeFastbootCommand(String... cmdArgs)
                            throws DeviceNotAvailableException, UnsupportedOperationException {
                        if (cmdArgs[0].equals("reboot-fastboot")) {
                            wasCalled = true;
                        }
                        return new CommandResult();
                    }

                    @Override
                    public String getFastbootSerialNumber() {
                        return MOCK_DEVICE_SERIAL;
                    }
                };
        mMockStateMonitor.setFastbootSerialNumber(MOCK_DEVICE_SERIAL);
        EasyMock.expect(mMockStateMonitor.waitForDeviceBootloader(EasyMock.anyLong()))
                .andReturn(true);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        testDevice.rebootIntoFastbootd();
        assertTrue(testDevice.wasCalled);
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#unlockDevice()} already decrypted. */
    @Test
    public void testUnlockDevice_skipping() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean isEncryptionSupported() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean isDeviceEncrypted() throws DeviceNotAvailableException {
                return false;
            }
        };
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        assertTrue(mTestDevice.unlockDevice());
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#unlockDevice()}. */
    @Test
    public void testUnlockDevice() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean isEncryptionSupported() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean isDeviceEncrypted() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean enableAdbRoot() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public String executeShellCommand(String command) throws DeviceNotAvailableException {
                return "200 checkpw -1";
            }
        };
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        assertTrue(mTestDevice.unlockDevice());
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#unlockDevice()}. */
    @Test
    public void testUnlockDevice_garbageOutput() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean isEncryptionSupported() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean isDeviceEncrypted() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean enableAdbRoot() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public String executeShellCommand(String command) throws DeviceNotAvailableException {
                return "gdsgdgggsgdg not working";
            }
        };
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        assertFalse(mTestDevice.unlockDevice());
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#unlockDevice()}. */
    @Test
    public void testUnlockDevice_emptyOutput() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean isEncryptionSupported() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean isDeviceEncrypted() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean enableAdbRoot() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public String executeShellCommand(String command) throws DeviceNotAvailableException {
                return "";
            }
        };
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        assertFalse(mTestDevice.unlockDevice());
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /** Unit test for {@link NativeDevice#unlockDevice()}. */
    @Test
    public void testUnlockDevice_goodOutputPasswordEnteredCorrectly() throws Exception {
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean isEncryptionSupported() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean isDeviceEncrypted() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public boolean enableAdbRoot() throws DeviceNotAvailableException {
                return true;
            }
            @Override
            public String executeShellCommand(String command) throws DeviceNotAvailableException {
                return "200 encryption 0";
            }
        };
        EasyMock.expect(mMockStateMonitor.waitForDeviceAvailable()).andReturn(mMockIDevice);
        EasyMock.replay(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
        assertTrue(mTestDevice.unlockDevice());
        EasyMock.verify(mMockIDevice, mMockStateMonitor, mMockDvcMonitor);
    }

    /**
     * Test {NativeDevice#parseFreeSpaceFromModernOutput(String)} with a regular df output and mount
     * name of numbers.
     */
    @Test
    public void testParseDfOutput_mountWithNumber() {
        String dfOutput = "Filesystem     1K-blocks   Used Available Use% Mounted on\n" +
                "/dev/fuse       31154688 100576  31054112   1% /storage/3134-3433";
        Long res = mTestDevice.parseFreeSpaceFromModernOutput(dfOutput);
        assertNotNull(res);
        assertEquals(31054112L, res.longValue());
    }

    /**
     * Test {NativeDevice#parseFreeSpaceFromModernOutput(String)} with a regular df output and mount
     * name of mix of letters and numbers.
     */
    @Test
    public void testParseDfOutput() {
        String dfOutput = "Filesystem     1K-blocks   Used Available Use% Mounted on\n" +
                "/dev/fuse       31154688 100576  31054112   1% /storage/sdcard58";
        Long res = mTestDevice.parseFreeSpaceFromModernOutput(dfOutput);
        assertNotNull(res);
        assertEquals(31054112L, res.longValue());
    }

    /**
     * Test {NativeDevice#parseFreeSpaceFromModernOutput(String)} with a regular df output and mount
     * name with incorrect field.
     */
    @Test
    public void testParseDfOutput_wrongMount() {
        String dfOutput = "Filesystem     1K-blocks   Used Available Use% Mounted on\n" +
                "/dev/fuse       31154688 100576  31054112   1% \test\\wrongpath";
        Long res = mTestDevice.parseFreeSpaceFromModernOutput(dfOutput);
        assertNull(res);
    }

    /**
     * Test {NativeDevice#parseFreeSpaceFromModernOutput(String)} with a regular df output and a
     * filesytem name with numbers in it.
     */
    @Test
    public void testParseDfOutput_filesystemWithNumber() {
        String dfOutput = "Filesystem     1K-blocks   Used Available Use% Mounted on\n" +
                "/dev/dm-1       31154688 100576  31054112   1% /";
        Long res = mTestDevice.parseFreeSpaceFromModernOutput(dfOutput);
        assertNotNull(res);
        assertEquals(31054112L, res.longValue());
    }

    /**
     * Test that {@link NativeDevice#getDeviceTimeOffset(Date)} returns the proper offset forward
     */
    @Test
    public void testGetDeviceTimeOffset() throws DeviceNotAvailableException {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public long getDeviceDate() throws DeviceNotAvailableException {
                        return 1476958881000L;
                    }
                };
        Date date = new Date(1476958891000L);
        assertEquals(10000L, mTestDevice.getDeviceTimeOffset(date));
    }

    /**
     * Test that {@link NativeDevice#getDeviceTimeOffset(Date)}} returns the proper offset when
     * there is delay.
     */
    @Test
    public void testGetDeviceTimeOffset_delay() throws DeviceNotAvailableException {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public long getDeviceDate() throws DeviceNotAvailableException {
                        // DeviceDate is in second
                        return 1476958891000L;
                    }
                };
        // Date takes millisecond since Epoch
        Date date = new Date(1476958881000L);
        assertEquals(-10000L, mTestDevice.getDeviceTimeOffset(date));
    }

    /**
     * Test that {@link NativeDevice#setDate(Date)} returns the proper offset when there is delay
     * with api level above 24, posix format is used.
     */
    @Test
    public void testSetDate() throws DeviceNotAvailableException {
        Date date = new Date(1476958881000L);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 24;
                    }

                    @Override
                    public long getDeviceTimeOffset(Date date) throws DeviceNotAvailableException {
                        // right above set threshold
                        return NativeDevice.MAX_HOST_DEVICE_TIME_OFFSET + 1;
                    }

                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        CLog.e("%s", command);
                        assertEquals("TZ=UTC date -u 102010212016.21", command);
                        return command;
                    }
                };
        mTestDevice.setDate(date);
    }

    /**
     * Test that {@link NativeDevice#setDate(Date)} returns the proper offset when there is delay
     * with api level below 23, regular second format is used.
     */
    @Test
    public void testSetDate_lowApi() throws DeviceNotAvailableException {
        Date date = new Date(1476958881000L);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 22;
                    }

                    @Override
                    public long getDeviceTimeOffset(Date date) throws DeviceNotAvailableException {
                        // right above set threshold
                        return NativeDevice.MAX_HOST_DEVICE_TIME_OFFSET + 1;
                    }

                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        CLog.e("%s", command);
                        assertEquals("TZ=UTC date -u 1476958881", command);
                        return command;
                    }
                };
        mTestDevice.setDate(date);
    }

    /** Test that {@link NativeDevice#setDate(Date)} does not attemp to set if bellow threshold. */
    @Test
    public void testSetDate_NoAction() throws DeviceNotAvailableException {
        Date date = new Date(1476958881000L);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public long getDeviceTimeOffset(Date date) throws DeviceNotAvailableException {
                        // right below set threshold
                        return NativeDevice.MAX_HOST_DEVICE_TIME_OFFSET - 1;
                    }

                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        fail("Should not be called");
                        return command;
                    }
                };
        mTestDevice.setDate(date);
    }

    /** Test that {@link NativeDevice#getDeviceDescriptor()} returns the proper information. */
    @Test
    public void testGetDeviceDescriptor() {
        final String serial = "Test";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public IDevice getIDevice() {
                return new StubDevice(serial);
            }
        };
        DeviceDescriptor desc = mTestDevice.getDeviceDescriptor();
        assertTrue(desc.isStubDevice());
        assertEquals(serial, desc.getSerial());
        assertEquals("StubDevice", desc.getDeviceClass());
    }

    /**
     * Test that {@link NativeDevice#pullFile(String, File)} returns true when the pull is
     * successful.
     */
    @Test
    public void testPullFile() throws Exception {
        final String fakeRemotePath = "/test/";
        SyncService s = Mockito.mock(SyncService.class);
        EasyMock.expect(mMockIDevice.getSyncService()).andReturn(s);
        EasyMock.replay(mMockIDevice);
        File tmpFile = FileUtil.createTempFile("pull", ".test");
        try {
            boolean res = mTestDevice.pullFile(fakeRemotePath, tmpFile);
            EasyMock.verify(mMockIDevice);
            Mockito.verify(s)
                    .pullFile(
                            Mockito.eq(fakeRemotePath),
                            Mockito.eq(tmpFile.getAbsolutePath()),
                            Mockito.any(ISyncProgressMonitor.class));
            Mockito.verify(s).close();
            assertTrue(res);
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    /**
     * Test that {@link NativeDevice#pullFile(String, File)} returns false when it fails to pull the
     * file.
     */
    @Test
    public void testPullFile_fails() throws Exception {
        final String fakeRemotePath = "/test/";
        SyncService s = Mockito.mock(SyncService.class);
        EasyMock.expect(mMockIDevice.getSyncService()).andReturn(s);
        EasyMock.replay(mMockIDevice);
        File tmpFile = FileUtil.createTempFile("pull", ".test");
        doThrow(new SyncException(SyncError.CANCELED))
                .when(s)
                .pullFile(
                        Mockito.eq(fakeRemotePath),
                        Mockito.eq(tmpFile.getAbsolutePath()),
                        Mockito.any(ISyncProgressMonitor.class));
        try {
            boolean res = mTestDevice.pullFile(fakeRemotePath, tmpFile);
            EasyMock.verify(mMockIDevice);
            Mockito.verify(s)
                    .pullFile(
                            Mockito.eq(fakeRemotePath),
                            Mockito.eq(tmpFile.getAbsolutePath()),
                            Mockito.any(ISyncProgressMonitor.class));
            Mockito.verify(s).close();
            assertFalse(res);
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    /**
     * Test that {@link NativeDevice#pullFile(String)} returns a file when succeed pulling the file.
     */
    @Test
    public void testPullFile_returnFileSuccess() throws Exception {
        final String fakeRemotePath = "/test/";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean pullFile(String remoteFilePath, File localFile)
                    throws DeviceNotAvailableException {
                return true;
            }
        };
        File res = mTestDevice.pullFile(fakeRemotePath);
        try {
            assertNotNull(res);
        } finally {
            FileUtil.deleteFile(res);
        }
    }

    /**
     * Test that {@link NativeDevice#pullFile(String)} returns null when failed to pull the file.
     */
    @Test
    public void testPullFile_returnNull() throws Exception {
        final String fakeRemotePath = "/test/";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean pullFile(String remoteFilePath, File localFile)
                    throws DeviceNotAvailableException {
                return false;
            }
        };
        File res = mTestDevice.pullFile(fakeRemotePath);
        try {
            assertNull(res);
        } finally {
            FileUtil.deleteFile(res);
        }
    }

    /**
     * Test that {@link NativeDevice#pullFileContents(String)} returns a string when succeed pulling
     * the file.
     */
    @Test
    public void testPullFileContents_returnFileSuccess() throws Exception {
        final String fakeRemotePath = "/test/";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean pullFile(String remoteFilePath, File localFile)
                    throws DeviceNotAvailableException {
                return true;
            }
        };
        String res = mTestDevice.pullFileContents(fakeRemotePath);
        assertNotNull(res);
    }

    /**
     * Test that {@link NativeDevice#pullFileContents(String)} returns null when failed to pull the
     * file.
     */
    @Test
    public void testPullFileContents_returnNull() throws Exception {
        final String fakeRemotePath = "/test/";
        mTestDevice = new TestableAndroidNativeDevice() {
            @Override
            public boolean pullFile(String remoteFilePath, File localFile)
                    throws DeviceNotAvailableException {
                return false;
            }
        };
        String res = mTestDevice.pullFileContents(fakeRemotePath);
        assertNull(res);
    }

    /**
     * Test that {@link NativeDevice#pushFile(File, String)} returns true when the push is
     * successful.
     */
    @Test
    public void testPushFile() throws Exception {
        final String fakeRemotePath = "/test/";
        SyncService s = Mockito.mock(SyncService.class);
        EasyMock.expect(mMockIDevice.getSyncService()).andReturn(s);
        EasyMock.replay(mMockIDevice);
        File tmpFile = FileUtil.createTempFile("push", ".test");
        try {
            boolean res = mTestDevice.pushFile(tmpFile, fakeRemotePath);
            EasyMock.verify(mMockIDevice);
            Mockito.verify(s)
                    .pushFile(
                            Mockito.eq(tmpFile.getAbsolutePath()),
                            Mockito.eq(fakeRemotePath),
                            Mockito.any(ISyncProgressMonitor.class));
            Mockito.verify(s).close();
            assertTrue(res);
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    /**
     * Test that {@link NativeDevice#pushFile(File, String)} returns false when the push is
     * unsuccessful.
     */
    @Test
    public void testPushFile_fails() throws Exception {
        final String fakeRemotePath = "/test/";
        SyncService s = Mockito.mock(SyncService.class);
        EasyMock.expect(mMockIDevice.getSyncService()).andReturn(s);
        EasyMock.replay(mMockIDevice);
        File tmpFile = FileUtil.createTempFile("push", ".test");
        doThrow(new SyncException(SyncError.CANCELED))
                .when(s)
                .pushFile(
                        Mockito.eq(tmpFile.getAbsolutePath()),
                        Mockito.eq(fakeRemotePath),
                        Mockito.any(ISyncProgressMonitor.class));
        try {
            boolean res = mTestDevice.pushFile(tmpFile, fakeRemotePath);
            EasyMock.verify(mMockIDevice);
            Mockito.verify(s)
                    .pushFile(
                            Mockito.eq(tmpFile.getAbsolutePath()),
                            Mockito.eq(fakeRemotePath),
                            Mockito.any(ISyncProgressMonitor.class));
            Mockito.verify(s).close();
            assertFalse(res);
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    /** Test get Process pid by process name */
    @Test
    public void testGetProcessPid() throws Exception {
        final String fakePid = "914";
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(fakePid).when(spy).executeShellCommand("pidof system_server");
        EasyMock.replay(mMockIDevice);
        assertEquals(fakePid, spy.getProcessPid("system_server"));
        EasyMock.verify(mMockIDevice);
    }

    /** Test get Process pid by process name with adb shell return of extra new line */
    @Test
    public void testGetProcessPidWithNewLine() throws Exception {
        final String fakePid = "914";
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(fakePid + "\n").when(spy).executeShellCommand("pidof system_server");
        EasyMock.replay(mMockIDevice);
        assertEquals(fakePid, spy.getProcessPid("system_server"));
        EasyMock.verify(mMockIDevice);
    }

    /** Test get Process pid return null with invalid shell command output */
    @Test
    public void testGetProcessPidInvalidOutput() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("invalid output").when(spy).executeShellCommand("pidof system_server");
        EasyMock.replay(mMockIDevice);
        assertNull(spy.getProcessPid("system_server"));
        EasyMock.verify(mMockIDevice);
    }

    /** Test get Process pid return null with shell command empty output */
    @Test
    public void testGetProcessPidEmptyOutput() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("").when(spy).executeShellCommand("pidof system_server");
        EasyMock.replay(mMockIDevice);
        assertNull(spy.getProcessPid("system_server"));
        EasyMock.verify(mMockIDevice);
    }

    /** Test get ProcessInfo by process name */
    @Test
    public void testGetProcessByName() throws Exception {
        final String fakePid = "914";
        final String fakeCreationTime = "1559091922";
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(fakePid).when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p " + fakePid + " -o stime=");
        doReturn(fakeCreationTime).when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/" + fakePid);
        EasyMock.replay(mMockIDevice);
        assertEquals(Integer.parseInt(fakePid), spy.getProcessByName("system_server").getPid());
        assertEquals(
                Long.parseLong(fakeCreationTime),
                spy.getProcessByName("system_server").getStartTime());
        EasyMock.verify(mMockIDevice);
    }

    /** Test get ProcessInfo by process name return null for invalid process */
    @Test
    public void testGetProcessByNameInvalidProcess() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("").when(spy).executeShellCommand("pidof system_server");
        EasyMock.replay(mMockIDevice);
        assertNull(spy.getProcessByName("system_server"));
        EasyMock.verify(mMockIDevice);
    }

    /** Test get ProcessInfo by process name return null for invalid process */
    @Test
    public void testGetProcessByNameInvalidStartTime() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("120").when(spy).executeShellCommand("pidof system_server");
        doReturn("").when(spy).executeShellCommand("ps -p 120 -o stime=");
        EasyMock.replay(mMockIDevice);
        assertNull(spy.getProcessByName("system_server"));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetIntProperty() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("123").when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertEquals(123, spy.getIntProperty("ro.test.prop", -1));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetIntPropertyNotAnIntegerPropertyReturnsDefaultValue() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("not-an-int").when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertEquals(-1, spy.getIntProperty("ro.test.prop", -1));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetIntPropertyUnknownPropertyReturnsDefaultValue() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(null).when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertEquals(-1, spy.getIntProperty("ro.test.prop", -1));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetBooleanPropertyReturnsTrue() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("true").when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.getBooleanProperty("ro.test.prop", false));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetBooleanPropertyReturnsFalse() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("no").when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertFalse(spy.getBooleanProperty("ro.test.prop", true));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetBooleanPropertyUnknownPropertyReturnsDefaultValue() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(null).when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.getBooleanProperty("ro.test.prop", true));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetBooleanPropertyInvalidValueReturnsDefaultValue() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("123").when(spy).getProperty("ro.test.prop");
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.getBooleanProperty("ro.test.prop", true));
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetProperty_noOutput() throws Exception {
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        res.setStdout("\n");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                100,
                                (OutputStream) null,
                                null,
                                "adb",
                                "-s",
                                "serial",
                                "shell",
                                "getprop",
                                "test"))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil, mMockIDevice);
        assertNull(mTestDevice.getProperty("test"));
        EasyMock.verify(mMockRunUtil, mMockIDevice);
    }

    /** Test get boot history */
    @Test
    public void testGetBootHistory() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(
                        "kernel_panic,1556587278\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        Map<Long, String> history = new LinkedHashMap<Long, String>();
        history.put(1556587278L, "kernel_panic");
        history.put(1556238008L, "reboot");
        history.put(1556237796L, "reboot");
        history.put(1556237725L, "reboot");
        EasyMock.replay(mMockIDevice);
        assertEquals(history, spy.getBootHistory());
        EasyMock.verify(mMockIDevice);
    }

    /** Test get empty boot history */
    @Test
    public void testGetBootHistoryEmpty() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("").when(spy).getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.getBootHistory().isEmpty());
        EasyMock.verify(mMockIDevice);
    }

    /** Test get invalid boot history */
    @Test
    public void testGetBootHistoryInvalid() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("invalid output").when(spy).getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.getBootHistory().isEmpty());
        EasyMock.verify(mMockIDevice);
    }

    /** Test get boot history since */
    @Test
    public void testGetBootHistorySince() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(
                        "kernel_panic,1556587278\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        Map<Long, String> history = new LinkedHashMap<Long, String>();
        history.put(1556587278L, "kernel_panic");
        EasyMock.replay(mMockIDevice);
        assertEquals(history, spy.getBootHistorySince(1556238009L, TimeUnit.SECONDS));
        EasyMock.verify(mMockIDevice);
    }

    /** Test {@link NativeDevice#getBootHistorySince(long, TimeUnit)} on an edge condition. */
    @Test
    public void testGetBootHistorySince_limit() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("reboot,1579678463\n" + "        reboot,,1579678339\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        Map<Long, String> history = new LinkedHashMap<Long, String>();
        history.put(1579678463L, "reboot");
        EasyMock.replay(mMockIDevice);
        // For the same value we should expect it to be part of the reboot.
        assertEquals(history, spy.getBootHistorySince(1579678463, TimeUnit.SECONDS));
        EasyMock.verify(mMockIDevice);
    }

    /** Test get boot history since */
    @Test
    public void testGetBootHistorySinceInMillisecond() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(
                        "kernel_panic,1556587278\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        Map<Long, String> history = new LinkedHashMap<Long, String>();
        history.put(1556587278L, "kernel_panic");
        EasyMock.replay(mMockIDevice);
        assertEquals(history, spy.getBootHistorySince(1556238009000L, TimeUnit.MILLISECONDS));
        EasyMock.verify(mMockIDevice);
    }

    /** Test deviceSoftRestartedSince */
    @Test
    public void testDeviceSoftRestartedSince() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091922").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "kernel_panic,1556587278\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertFalse(spy.deviceSoftRestartedSince(1559091923L, TimeUnit.SECONDS));
        assertFalse(spy.deviceSoftRestartedSince(1559091923000L, TimeUnit.MILLISECONDS));
        assertFalse(spy.deviceSoftRestartedSince(1559091922L, TimeUnit.SECONDS));
        assertFalse(spy.deviceSoftRestartedSince(1559091922000L, TimeUnit.MILLISECONDS));
        assertTrue(spy.deviceSoftRestartedSince(1559091920L, TimeUnit.SECONDS));
        assertTrue(spy.deviceSoftRestartedSince(1559091920000L, TimeUnit.MILLISECONDS));
        EasyMock.verify(mMockIDevice);
    }

    /** Test deviceSoftRestartedSince return true with system_server stopped */
    @Test
    public void testDeviceSoftRestartedSinceWithSystemServerStopped() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("").when(spy).executeShellCommand("pidof system_server");
        assertTrue(spy.deviceSoftRestartedSince(1559091922L, TimeUnit.SECONDS));
    }

    /** Test deviceSoftRestartedSince throw RuntimeException with abnormal reboot */
    @Test
    public void testDeviceSoftRestartedSinceWithAbnormalReboot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091999").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "kernel_panic,1559091933\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        try {
            spy.deviceSoftRestartedSince(1559091922L, TimeUnit.SECONDS);
        } catch (RuntimeException e) {
            //expected
            return;
        }
        fail("RuntimeException is expected");
    }

    /** Test deviceSoftRestartedSince return false with normal reboot */
    @Test
    public void testDeviceSoftRestartedSinceNotAfterNormalReboot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091939").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "reboot,1559091933\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertFalse(spy.deviceSoftRestartedSince(1559091921L, TimeUnit.SECONDS));
        EasyMock.verify(mMockIDevice);
    }

    /** Test deviceSoftRestartedSince return false with normal reboot */
    @Test
    public void testDeviceSoftRestartedSinceAfterNormalReboot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091992").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "reboot,1559091933\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.deviceSoftRestartedSince(1559091921L, TimeUnit.SECONDS));
        EasyMock.verify(mMockIDevice);
    }

    /** Test deviceSoftRestarted given the previous system_server {@link ProcessInfo} */
    @Test
    public void testDeviceSoftRestarted() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        ProcessInfo prev1 = new ProcessInfo("system", 123, "system_server", 1559000000L);
        ProcessInfo prev2 = new ProcessInfo("system", 914, "system_server", 1559091922L);
        doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091922").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "kernel_panic,1556587278\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertTrue(spy.deviceSoftRestarted(prev1));
        assertFalse(spy.deviceSoftRestarted(prev2));
        EasyMock.verify(mMockIDevice);
    }

    /** Test deviceSoftRestarted return true with system_server stopped */
    @Test
    public void testDeviceSoftRestartedWithSystemServerStopped() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("").when(spy).executeShellCommand("pidof system_server");
        assertTrue(
                spy.deviceSoftRestarted(
                        new ProcessInfo("system", 123, "system_server", 1559000000L)));
    }

    /** Test deviceSoftRestarted throw RuntimeException with abnormal reboot */
    @Test
    public void testDeviceSoftRestartedWithAbnormalReboot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091999").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "kernel_panic,1559091933\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        try {
            spy.deviceSoftRestarted(new ProcessInfo("system", 123, "system_server", 1559000000L));
        } catch (RuntimeException e) {
            //expected
            return;
        }
        fail("Abnormal reboot is detected, RuntimeException is expected");
    }

    /** Test ddeviceSoftRestarted return false with normal reboot */
    @Test
    public void testDeviceSoftRestartedNotAfterNormalReboot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091935").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "reboot,,1559091933\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertFalse(
                spy.deviceSoftRestarted(
                        new ProcessInfo("system", 123, "system_server", 1559000000L)));
        EasyMock.verify(mMockIDevice);
    }

    /** Test deviceSoftRestarted return true if system_server restarted after normal reboot */
    @Test
    public void testDeviceSoftRestartedAfterNormalReboot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn("914").doReturn("914").when(spy).executeShellCommand("pidof system_server");
        doReturn("12:07:32").when(spy).executeShellCommand("ps -p 914 -o stime=");
        doReturn("1559091995").when(spy).executeShellCommand("date -d\"12:07:32\" +%s");
        doReturn("system").when(spy).executeShellCommand("stat -c%U /proc/914");
        doReturn(
                        "reboot,,1559091933\n"
                                + "        reboot,,1556238008\n"
                                + "        reboot,,1556237796\n"
                                + "        reboot,,1556237725\n")
                .when(spy)
                .getProperty(DeviceProperties.BOOT_REASON_HISTORY);
        EasyMock.replay(mMockIDevice);
        assertTrue(
                spy.deviceSoftRestarted(
                        new ProcessInfo("system", 123, "system_server", 1559000000L)));
        EasyMock.verify(mMockIDevice);
    }

    /** Test validating valid MAC addresses */
    @Test
    public void testIsMacAddress() {
        assertTrue(mTestDevice.isMacAddress("00:00:00:00:00:00"));
        assertTrue(mTestDevice.isMacAddress("00:15:E9:2B:99:3C"));
        assertTrue(mTestDevice.isMacAddress("FF:FF:FF:FF:FF:FF"));
        assertTrue(mTestDevice.isMacAddress("58:a2:b5:7d:49:24"));
    }

    /** Test validating invalid MAC addresses */
    @Test
    public void testIsMacAddress_invalidInput() {
        assertFalse(mTestDevice.isMacAddress(""));
        assertFalse(mTestDevice.isMacAddress("00-15-E9-2B-99-3C")); // Invalid delimiter
    }

    /** Test querying a device MAC address */
    @Test
    public void testGetMacAddress() throws Exception {
        String address = "58:a2:b5:7d:49:24";
        IDevice device = new StubDevice(MOCK_DEVICE_SERIAL) {
            @Override
            public void executeShellCommand(String command, IShellOutputReceiver receiver)
                    throws TimeoutException, AdbCommandRejectedException,
                    ShellCommandUnresponsiveException, IOException {
                receiver.addOutput(address.getBytes(), 0, address.length());
            }
        };
        mMockIDevice.executeShellCommand(EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expectLastCall().andDelegateTo(device).anyTimes();
        EasyMock.replay(mMockIDevice);
        assertEquals(address, mTestDevice.getMacAddress());
        EasyMock.verify(mMockIDevice);
    }

    /** Test querying a device MAC address when the device is in fastboot */
    @Test
    public void testGetMacAddress_fastboot() throws Exception {
        mTestDevice.setDeviceState(TestDeviceState.FASTBOOT);
        // Will fail if executeShellCommand is called. Which it should not.
        assertNull(mTestDevice.getMacAddress());
    }

    /** Test querying a device MAC address but failing to do so */
    @Test
    public void testGetMacAddress_failure() throws Exception {
        IDevice device = new StubDevice(MOCK_DEVICE_SERIAL) {
            @Override
            public void executeShellCommand(String command, IShellOutputReceiver receiver)
                    throws TimeoutException, AdbCommandRejectedException,
                    ShellCommandUnresponsiveException, IOException {
                throw new IOException();
            }
        };
        mMockIDevice.executeShellCommand(EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expectLastCall().andDelegateTo(device).anyTimes();
        EasyMock.replay(mMockIDevice);
        assertNull(mTestDevice.getMacAddress());
        EasyMock.verify(mMockIDevice);
    }

    /** Test querying a device MAC address for a stub device */
    @Test
    public void testGetMacAddress_stubDevice() throws Exception {
        String address = "58:a2:b5:7d:49:24";
        IDevice device = new StubDevice(MOCK_DEVICE_SERIAL) {
            @Override
            public void executeShellCommand(String command, IShellOutputReceiver receiver)
                    throws TimeoutException, AdbCommandRejectedException,
                    ShellCommandUnresponsiveException, IOException {
                receiver.addOutput(address.getBytes(), 0, address.length());
            }
        };
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public void recoverDevice() throws DeviceNotAvailableException {
                        // ignore
                    }

                    @Override
                    public IDevice getIDevice() {
                        return device;
                    }

                    @Override
                    IWifiHelper createWifiHelper() {
                        return mMockWifi;
                    }
                };
        mTestDevice.setIDevice(device);
        assertNull(mTestDevice.getMacAddress());
    }

    /** Test if valid shell output returns correct memory size. */
    @Test
    public void testGetTotalMemory() {
        final long expectSize = 1902936064;
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "MemTotal:        1858336 kB";
                    }
                };
        assertEquals(expectSize, mTestDevice.getTotalMemory());
    }

    /** Test if empty shell output returns -1. */
    @Test
    public void testGetTotalMemory_emptyString() {
        final long expectSize = -1;
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "";
                    }
                };
        assertEquals(expectSize, mTestDevice.getTotalMemory());
    }

    /** Test if unexpected shell output returns -1. */
    @Test
    public void testGetTotalMemory_unexpectedFormat() {
        final long expectSize = -1;
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "1858336 kB";
                    }
                };
        assertEquals(expectSize, mTestDevice.getTotalMemory());
    }

    /** Test if catching exception returns -1. */
    @Test
    public void testGetTotalMemory_exception() {
        final long expectSize = -1;
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        throw new DeviceNotAvailableException("test", "serial");
                    }
                };
        assertEquals(expectSize, mTestDevice.getTotalMemory());
    }

    /**
     * Test that when a {@link NativeDevice#getLogcatSince(long)} is requested a matching logcat
     * command is generated.
     */
    @Test
    public void testGetLogcatSince() throws Exception {
        long date = 1512990942000L; // 2017-12-11 03:15:42.015
        setGetPropertyExpectation("ro.build.version.sdk", "23");

        SimpleDateFormat format = new SimpleDateFormat("MM-dd HH:mm:ss.mmm");
        String dateFormatted = format.format(new Date(date));

        mMockIDevice.executeShellCommand(
                EasyMock.eq(String.format("logcat -v threadtime -t '%s'", dateFormatted)),
                EasyMock.anyObject());
        EasyMock.replay(mMockIDevice);
        mTestDevice.getLogcatSince(date);
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetProductVariant() throws Exception {
        setGetPropertyExpectation(DeviceProperties.VARIANT, "variant");
        EasyMock.replay(mMockIDevice);
        assertEquals("variant", mTestDevice.getProductVariant());
        EasyMock.verify(mMockIDevice);
    }

    @Test
    public void testGetProductVariant_legacyOmr1() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    protected String internalGetProperty(
                            String propName, String fastbootVar, String description)
                            throws DeviceNotAvailableException, UnsupportedOperationException {
                        if (DeviceProperties.VARIANT_LEGACY_O_MR1.equals(propName)) {
                            return "legacy_omr1";
                        }
                        return null;
                    }
                };

        assertEquals("legacy_omr1", testDevice.getProductVariant());
    }

    @Test
    public void testGetProductVariant_legacy() throws Exception {
        TestableAndroidNativeDevice testDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    protected String internalGetProperty(
                            String propName, String fastbootVar, String description)
                            throws DeviceNotAvailableException, UnsupportedOperationException {
                        if (DeviceProperties.VARIANT_LEGACY_LESS_EQUAL_O.equals(propName)) {
                            return "legacy";
                        }
                        return null;
                    }
                };

        assertEquals("legacy", testDevice.getProductVariant());
    }

    /** Test when {@link NativeDevice#executeShellV2Command(String)} returns a success. */
    @Test
    public void testExecuteShellV2Command() throws Exception {
        OutputStream stdout = null, stderr = null;
        CommandResult res = new CommandResult();
        res.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                100, stdout, stderr, "adb", "-s", "serial", "shell", "some",
                                "command"))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil, mMockIDevice);
        assertNotNull(mTestDevice.executeShellV2Command("some command"));
        EasyMock.verify(mMockRunUtil, mMockIDevice);
    }

    /**
     * Test when {@link NativeDevice#executeShellV2Command(String, long, TimeUnit, int)} fails and
     * repeat because of a timeout.
     */
    @Test
    public void testExecuteShellV2Command_timeout() throws Exception {
        OutputStream stdout = null, stderr = null;
        CommandResult res = new CommandResult();
        res.setStatus(CommandStatus.TIMED_OUT);
        res.setStderr("timed out");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                200L, stdout, stderr, "adb", "-s", "serial", "shell", "some",
                                "command"))
                .andReturn(res)
                .times(2);
        EasyMock.replay(mMockRunUtil, mMockIDevice);
        try {
            mTestDevice.executeShellV2Command("some command", 200L, TimeUnit.MILLISECONDS, 1);
            fail("Should have thrown an exception.");
        } catch (DeviceUnresponsiveException e) {
            // expected
        }
        EasyMock.verify(mMockRunUtil, mMockIDevice);
    }

    /**
     * Test when {@link NativeDevice#executeShellV2Command(String, long, TimeUnit, int)} fails and
     * output.
     */
    @Test
    public void testExecuteShellV2Command_fail() throws Exception {
        OutputStream stdout = null, stderr = null;
        CommandResult res = new CommandResult();
        res.setStatus(CommandStatus.FAILED);
        res.setStderr("timed out");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                200L, stdout, stderr, "adb", "-s", "serial", "shell", "some",
                                "command"))
                .andReturn(res)
                .times(1);
        EasyMock.replay(mMockRunUtil, mMockIDevice);
        CommandResult result =
                mTestDevice.executeShellV2Command("some command", 200L, TimeUnit.MILLISECONDS, 1);
        assertNotNull(result);
        // The final result is what RunUtil returned, so it contains full status information.
        assertSame(res, result);
        EasyMock.verify(mMockRunUtil, mMockIDevice);
    }

    /** Unit test for {@link INativeDevice#setProperty(String, String)}. */
    @Test
    public void testSetProperty() throws DeviceNotAvailableException {
        OutputStream stdout = null, stderr = null;
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public boolean isAdbRoot() throws DeviceNotAvailableException {
                        return true;
                    }
                };
        CommandResult res = new CommandResult();
        res.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                120000,
                                stdout,
                                stderr,
                                "adb",
                                "-s",
                                "serial",
                                "shell",
                                "setprop test 'value'"))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil, mMockIDevice);
        assertTrue(mTestDevice.setProperty("test", "value"));
        EasyMock.verify(mMockRunUtil, mMockIDevice);
    }

    /**
     * Verifies that {@link INativeDevice#isExecutable(String)} recognizes regular executable file
     *
     * @throws Exception
     */
    @Test
    public void testIsDeviceFileExecutable_executable_rwx() throws Exception {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "-rwxr-xr-x 1 root shell 42824 2009-01-01 00:00 /system/bin/ping";
                    }
                };
        assertTrue(mTestDevice.isExecutable("/system/bin/ping"));
    }

    /**
     * Verifies that {@link INativeDevice#isExecutable(String)} recognizes symlink'd executable file
     *
     * @throws Exception
     */
    @Test
    public void testIsDeviceFileExecutable_executable_lrwx() throws Exception {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "lrwxr-xr-x 1 root shell 7 2009-01-01 00:00 /system/bin/start -> toolbox";
                    }
                };
        assertTrue(mTestDevice.isExecutable("/system/bin/start"));
    }

    /**
     * Verifies that {@link INativeDevice#isExecutable(String)} recognizes non-executable file
     *
     * @throws Exception
     */
    @Test
    public void testIsDeviceFileExecutable_notExecutable() throws Exception {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "-rw-r--r-- 1 root root 5020 2009-01-01 00:00 /system/build.prop";
                    }
                };
        assertFalse(mTestDevice.isExecutable("/system/build.prop"));
    }

    /**
     * Verifies that {@link INativeDevice#isExecutable(String)} recognizes a directory listing
     *
     * @throws Exception
     */
    @Test
    public void testIsDeviceFileExecutable_directory() throws Exception {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public String executeShellCommand(String command)
                            throws DeviceNotAvailableException {
                        return "total 416\n"
                                + "drwxr-xr-x 74 root root    4096 2009-01-01 00:00 app\n"
                                + "drwxr-xr-x  3 root shell   8192 2009-01-01 00:00 bin\n"
                                + "-rw-r--r--  1 root root    5020 2009-01-01 00:00 build.prop\n"
                                + "drwxr-xr-x 15 root root    4096 2009-01-01 00:00 etc\n"
                                + "drwxr-xr-x  2 root root    4096 2009-01-01 00:00 fake-libs\n"
                                + "drwxr-xr-x  2 root root    8192 2009-01-01 00:00 fonts\n"
                                + "drwxr-xr-x  4 root root    4096 2009-01-01 00:00 framework\n"
                                + "drwxr-xr-x  6 root root    8192 2009-01-01 00:00 lib\n"
                                + "drwx------  2 root root    4096 1970-01-01 00:00 lost+found\n"
                                + "drwxr-xr-x  3 root root    4096 2009-01-01 00:00 media\n"
                                + "drwxr-xr-x 68 root root    4096 2009-01-01 00:00 priv-app\n"
                                + "-rw-r--r--  1 root root  137093 2009-01-01 00:00 recovery-from-boot.p\n"
                                + "drwxr-xr-x  9 root root    4096 2009-01-01 00:00 usr\n"
                                + "drwxr-xr-x  8 root shell   4096 2009-01-01 00:00 vendor\n"
                                + "drwxr-xr-x  2 root shell   4096 2009-01-01 00:00 xbin\n";
                    }
                };
        assertFalse(mTestDevice.isExecutable("/system"));
    }

    /** Test {@link NativeDevice#getTombstones()}. */
    @Test
    public void testGetTombstones_notRoot() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(false).when(spy).isAdbRoot();

        EasyMock.replay(mMockIDevice);
        List<File> result = spy.getTombstones();
        assertEquals(0, result.size());
        EasyMock.verify(mMockIDevice);
    }

    /** Test {@link NativeDevice#getTombstones()}. */
    @Test
    public void testGetTombstones() throws Exception {
        TestableAndroidNativeDevice spy = Mockito.spy(mTestDevice);
        doReturn(true).when(spy).isAdbRoot();

        String[] tombs = new String[] {"tomb1", "tomb2"};
        doReturn(tombs).when(spy).getChildren("/data/tombstones/");

        doReturn(new File("tomb1_test")).when(spy).pullFile("/data/tombstones/tomb1");
        doReturn(new File("tomb2_test")).when(spy).pullFile("/data/tombstones/tomb2");

        EasyMock.replay(mMockIDevice);
        List<File> result = spy.getTombstones();
        assertEquals(2, result.size());
        EasyMock.verify(mMockIDevice);
    }

    /** Test {@link NativeDevice#getLaunchApiLevel()} with ro.product.first_api_level being set. */
    @Test
    public void testGetLaunchApiLevel_w_first_api() throws DeviceNotAvailableException {
        setGetPropertyExpectation(DeviceProperties.FIRST_API_LEVEL, "23");
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 29;
                    }
                };
        EasyMock.replay(mMockIDevice);
        assertEquals(23, mTestDevice.getLaunchApiLevel());
        EasyMock.verify(mMockIDevice);
    }

    /**
     * Test {@link NativeDevice#getLaunchApiLevel()} without ro.product.first_api_level being set.
     */
    @Test
    public void testGetLaunchApiLevel_wo_first_api() throws DeviceNotAvailableException {
        setGetPropertyExpectation(DeviceProperties.FIRST_API_LEVEL, null);
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 29;
                    }
                };
        EasyMock.replay(mMockIDevice);
        assertEquals(29, mTestDevice.getLaunchApiLevel());
        EasyMock.verify(mMockIDevice);
    }

    /** Test {@link NativeDevice#getLaunchApiLevel()} with NumberFormatException be asserted. */
    @Test
    public void testGetLaunchApiLevel_w_exception() throws DeviceNotAvailableException {
        setGetPropertyExpectation(DeviceProperties.FIRST_API_LEVEL, "R");
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 29;
                    }
                };
        EasyMock.replay(mMockIDevice);
        assertEquals(29, mTestDevice.getLaunchApiLevel());
        EasyMock.verify(mMockIDevice);
    }

    private void setGetPropertyExpectation(String property, String value) {
        CommandResult stubResult = new CommandResult(CommandStatus.SUCCESS);
        stubResult.setStdout(value);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                (OutputStream) EasyMock.isNull(),
                                EasyMock.isNull(),
                                EasyMock.eq("adb"),
                                EasyMock.eq("-s"),
                                EasyMock.eq("serial"),
                                EasyMock.eq("shell"),
                                EasyMock.eq("getprop"),
                                EasyMock.eq(property)))
                .andReturn(stubResult);
        EasyMock.replay(mMockRunUtil);
    }

    /** Test {@link NativeDevice#getFastbootSerialNumber()} with USB adb. */
    @Test
    public void testGetFastbootSerialNumber_usb_adb() {
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    public boolean isAdbTcp() {
                        return false;
                    }
                };
        EasyMock.replay(mMockIDevice);
        assertEquals(MOCK_DEVICE_SERIAL, mTestDevice.getFastbootSerialNumber());
        EasyMock.verify(mMockIDevice);
    }

    /** Test {@link NativeDevice#getFastbootSerialNumber()} with non-root TCP adb. */
    @Test
    public void testGetFastbootSerialNumber_non_root_tcp_adb() throws Exception {
        String address = "00:30:1b:ba:81:28";
        IDevice device =
                new StubDevice(MOCK_DEVICE_SERIAL) {
                    @Override
                    public void executeShellCommand(String command, IShellOutputReceiver receiver)
                            throws TimeoutException, AdbCommandRejectedException,
                                    ShellCommandUnresponsiveException, IOException {
                        receiver.addOutput(address.getBytes(), 0, address.length());
                    }
                };
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    IHostOptions getHostOptions() {
                        return new HostOptions() {
                            @Override
                            public String getNetworkInterface() {
                                return "en0";
                            }
                        };
                    }

                    @Override
                    public boolean isAdbTcp() {
                        return true;
                    }

                    @Override
                    public boolean isAdbRoot() throws DeviceNotAvailableException {
                        return false;
                    }

                    @Override
                    public boolean enableAdbRoot() throws DeviceNotAvailableException {
                        return true;
                    }

                    @Override
                    public boolean disableAdbRoot() throws DeviceNotAvailableException {
                        return true;
                    }
                };
        mMockIDevice.executeShellCommand(EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expectLastCall().andDelegateTo(device).anyTimes();
        EasyMock.replay(mMockIDevice);
        assertEquals(
                "tcp:fe80:0:0:0:230:1bff:feba:8128%en0", mTestDevice.getFastbootSerialNumber());
        EasyMock.verify(mMockIDevice);
    }

    /** Test {@link NativeDevice#getFastbootSerialNumber()} with root TCP adb. */
    @Test
    public void testGetFastbootSerialNumber_root_tcp_adb() throws Exception {
        String address = "00:30:1b:ba:81:28";
        IDevice device =
                new StubDevice(MOCK_DEVICE_SERIAL) {
                    @Override
                    public void executeShellCommand(String command, IShellOutputReceiver receiver)
                            throws TimeoutException, AdbCommandRejectedException,
                                    ShellCommandUnresponsiveException, IOException {
                        receiver.addOutput(address.getBytes(), 0, address.length());
                    }
                };
        mTestDevice =
                new TestableAndroidNativeDevice() {
                    @Override
                    IHostOptions getHostOptions() {
                        return new HostOptions() {
                            @Override
                            public String getNetworkInterface() {
                                return "en0";
                            }
                        };
                    }

                    @Override
                    public boolean isAdbTcp() {
                        return true;
                    }

                    @Override
                    public boolean isAdbRoot() throws DeviceNotAvailableException {
                        return true;
                    }
                };

        mMockIDevice.executeShellCommand(EasyMock.anyObject(), EasyMock.anyObject());
        EasyMock.expectLastCall().andDelegateTo(device).anyTimes();
        EasyMock.replay(mMockIDevice);
        assertEquals(
                "tcp:fe80:0:0:0:230:1bff:feba:8128%en0", mTestDevice.getFastbootSerialNumber());
        EasyMock.verify(mMockIDevice);
    }
}
