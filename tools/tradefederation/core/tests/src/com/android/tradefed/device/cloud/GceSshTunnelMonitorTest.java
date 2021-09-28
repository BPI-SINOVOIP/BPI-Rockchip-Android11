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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.RemoteAvdIDevice;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import com.google.common.net.HostAndPort;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import java.io.File;
import java.util.List;

/** Unit tests for {@link GceSshTunnelMonitor}. */
@RunWith(JUnit4.class)
public class GceSshTunnelMonitorTest {

    private GceSshTunnelMonitor mMonitor;
    private ITestDevice mMockDevice;
    private IBuildInfo mMockBuildInfo;
    private HostAndPort mHost;
    private TestDeviceOptions mOptions;
    private IRunUtil mMockRunUtil;
    private File mSshKeyFile;
    private ITestLogger mLogger;

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = new BuildInfo();
        mHost = HostAndPort.fromHost("127.0.0.1");
        mMockRunUtil = Mockito.mock(IRunUtil.class);
        mOptions = new TestDeviceOptions();
        mSshKeyFile = FileUtil.createTempFile("fake-ssh-key", ".key");
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("gce-private-key-path", mSshKeyFile.getAbsolutePath());
        mMonitor =
                new GceSshTunnelMonitor(mMockDevice, mMockBuildInfo, mHost, mOptions) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
        mLogger = Mockito.mock(ITestLogger.class);
    }

    @After
    public void tearDown() {
        FileUtil.deleteFile(mSshKeyFile);
        mMonitor.logSshTunnelLogs(mLogger);
    }

    /**
     * Test that {@link GceSshTunnelMonitor#initGce()} throws an exception when ssh key is missing.
     */
    @Test
    public void testInitGce_noSsshKey() throws Exception {
        // delete file to simulate missing key.
        FileUtil.deleteFile(mSshKeyFile);
        EasyMock.replay(mMockDevice);
        try {
            mMonitor.initGce();
            fail("Should have thrown an exception");
        } catch (RuntimeException expected) {
            // expected
        }
        EasyMock.verify(mMockDevice);
    }

    /** Test that {@link GceSshTunnelMonitor#initGce()} on a success case. */
    @Test
    public void testInitGce() throws Exception {
        ArgumentCaptor<String> capture = ArgumentCaptor.forClass(String.class);
        CommandResult cmdRes = new CommandResult(CommandStatus.SUCCESS);
        doReturn(cmdRes)
                .when(mMockRunUtil)
                .runTimedCmdSilentlyRetry(
                        Mockito.anyLong(), Mockito.anyLong(), Mockito.anyInt(), Mockito.any());
        EasyMock.replay(mMockDevice);
        mMonitor.initGce();
        EasyMock.verify(mMockDevice);
        verify(mMockRunUtil, atLeastOnce())
                .runTimedCmdSilentlyRetry(
                        Mockito.anyLong(), Mockito.anyLong(), Mockito.anyInt(), capture.capture());
        List<String> args = capture.getAllValues();
        assertEquals("start", args.get(args.size() - 2));
        assertEquals("adbd", args.get(args.size() - 1));
    }

    /** Test that {@link GceSshTunnelMonitor#initGce()} when stop adbd fails. */
    @Test
    public void testInitGce_stopAdbdFail() throws Exception {
        CommandResult cmdRes = new CommandResult(CommandStatus.FAILED);
        doReturn(cmdRes)
                .when(mMockRunUtil)
                .runTimedCmdSilentlyRetry(
                        Mockito.anyLong(), Mockito.anyLong(), Mockito.anyInt(), Mockito.any());
        EasyMock.replay(mMockDevice);
        try {
            mMonitor.initGce();
            fail("Should have thrown an exception.");
        } catch (RuntimeException expected) {
            assertEquals("failed to stop adbd", expected.getMessage());
        }
        EasyMock.verify(mMockDevice);
    }

    /**
     * Test that {@link GceSshTunnelMonitor#createSshTunnel(ITestDevice, String, int)} correctly
     * allocate a process and set the device serial.
     */
    @Test
    public void testCreateSshTunnel() throws Exception {
        IManagedTestDevice mockDevice = EasyMock.createMock(IManagedTestDevice.class);
        EasyMock.expect(mockDevice.getSerialNumber()).andReturn("INIT_SERIAL");
        mockDevice.setIDevice((RemoteAvdIDevice) EasyMock.anyObject());
        mockDevice.setFastbootEnabled(false);
        doReturn(null).when(mMockRunUtil).runCmdInBackground(Mockito.anyList());
        EasyMock.replay(mockDevice);
        mMonitor.createSshTunnel(mockDevice, mHost.getHost(), mHost.getPortOrDefault(5555));
        EasyMock.verify(mockDevice);
        assertNotNull(
                mMockBuildInfo.getBuildAttributes().get(GceSshTunnelMonitor.VIRTUAL_DEVICE_SERIAL));
    }

    /**
     * Test that when key was available once, then not available, we wait to give it a chance to
     * reappear since key is usually on network.
     */
    @Test
    public void testCheckSshKey() {
        mMonitor.checkSshKey();
        FileUtil.deleteFile(mSshKeyFile);
        try {
            doNothing().when(mMockRunUtil).sleep(Mockito.anyLong());
            mMonitor.checkSshKey();
            fail("Should have thrown an exception.");
        } catch (RuntimeException expected) {
            // expected
        }
    }
}
