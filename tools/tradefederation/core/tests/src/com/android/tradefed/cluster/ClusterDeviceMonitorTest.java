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
package com.android.tradefed.cluster;

import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link ClusterDeviceMonitor}. */
@RunWith(JUnit4.class)
public class ClusterDeviceMonitorTest {

    private static final String PRODCERTSTATUS_KEY = "LOAS status";
    private static final String KRBSTATUS_KEY = "Kerberos status";
    private static final String PRODCERTSTATUS_CMD = "prodcertstatus";
    private static final String KRBSTATUS_CMD = "krbstatus";
    private IRunUtil mRunUtil = null;
    private ClusterDeviceMonitor mClusterDeviceMonitor = null;
    private OptionSetter mClusterDeviceMonitorSetter = null;
    private ClusterDeviceMonitor.EventDispatcher mEventDispatcher = null;
    private IClusterOptions mClusterOptions = null;
    private IClusterEventUploader<ClusterHostEvent> mHostEventUploader = null;
    private OptionSetter mClusterOptionSetter = null;

    @Before
    public void setUp() throws Exception {
        mRunUtil = EasyMock.createMock(IRunUtil.class);
        mClusterOptions = new ClusterOptions();
        mHostEventUploader = EasyMock.createMock(IClusterEventUploader.class);
        mClusterDeviceMonitor =
                new ClusterDeviceMonitor() {
                    @Override
                    public IRunUtil getRunUtil() {
                        return mRunUtil;
                    }

                    @Override
                    List<DeviceDescriptor> listDevices() {
                        return new ArrayList<DeviceDescriptor>();
                    }
                };
        mClusterDeviceMonitorSetter = new OptionSetter(mClusterDeviceMonitor);
        mEventDispatcher =
                mClusterDeviceMonitor.new EventDispatcher() {
                    @Override
                    public IClusterOptions getClusterOptions() {
                        return mClusterOptions;
                    }

                    @Override
                    public IClusterEventUploader<ClusterHostEvent> getEventUploader() {
                        return mHostEventUploader;
                    }
                };
        mClusterOptionSetter = new OptionSetter(mClusterOptions);
        mClusterOptionSetter.setOptionValue("cluster:cluster", "cluster1");
        mClusterOptionSetter.setOptionValue("cluster:next-cluster", "cluster2");
        mClusterOptionSetter.setOptionValue("cluster:next-cluster", "cluster3");
        mClusterOptionSetter.setOptionValue("cluster:lab-name", "lab1");
    }

    @Test
    public void testDispatch() throws Exception {
        Capture<ClusterHostEvent> capture = new Capture<>();
        mHostEventUploader.postEvent(EasyMock.capture(capture));
        mHostEventUploader.flush();
        EasyMock.replay(mHostEventUploader);
        mEventDispatcher.dispatch();
        EasyMock.verify(mHostEventUploader);
        ClusterHostEvent hostEvent = capture.getValue();
        Assert.assertEquals("cluster1", hostEvent.getClusterId());
        Assert.assertEquals(Arrays.asList("cluster2", "cluster3"), hostEvent.getNextClusterIds());
        Assert.assertEquals("lab1", hostEvent.getLabName());
    }

    void setOptions() throws Exception {
        mClusterDeviceMonitorSetter.setOptionValue(
                "host-info-cmd", PRODCERTSTATUS_KEY, PRODCERTSTATUS_CMD);
        mClusterDeviceMonitorSetter.setOptionValue("host-info-cmd", KRBSTATUS_KEY, KRBSTATUS_CMD);
    }

    // Test getting additional host information
    @Test
    public void testGetAdditionalHostInfo() throws Exception {
        setOptions();
        String prodcertstatusOutput = "LOAS cert expires in 13h 5m";
        CommandResult prodcertstatusMockResult = new CommandResult();
        prodcertstatusMockResult.setStdout(prodcertstatusOutput);
        prodcertstatusMockResult.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mRunUtil.runTimedCmdSilently(
                                EasyMock.anyInt(), EasyMock.eq(PRODCERTSTATUS_CMD)))
                .andReturn(prodcertstatusMockResult)
                .times(1);

        String krbstatusOutput = "android-test ticket expires in 65d 19h";
        CommandResult krbstatusMockResult = new CommandResult();
        krbstatusMockResult.setStdout(krbstatusOutput);
        krbstatusMockResult.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(mRunUtil.runTimedCmdSilently(EasyMock.anyInt(), EasyMock.eq(KRBSTATUS_CMD)))
                .andReturn(krbstatusMockResult)
                .times(1);
        EasyMock.replay(mRunUtil);

        Map<String, String> expected = new HashMap<>();
        expected.put(PRODCERTSTATUS_KEY, prodcertstatusOutput);
        expected.put(KRBSTATUS_KEY, krbstatusOutput);

        Assert.assertEquals(expected, mClusterDeviceMonitor.getAdditionalHostInfo());
        EasyMock.verify(mRunUtil);
    }

    // Test getting additional host information with no commands to run
    @Test
    public void testGetAdditionalHostInfo_noCommands() {
        Map<String, String> expected = new HashMap<>();
        Assert.assertEquals(expected, mClusterDeviceMonitor.getAdditionalHostInfo());
    }

    // Test getting additional host information with failures
    @Test
    public void testGetAdditionalHostInfo_commandFailed() throws Exception {
        setOptions();
        String prodcertstatusOutput = "LOAS cert expires in 13h 5m";
        CommandResult prodcertstatusMockResult = new CommandResult();
        prodcertstatusMockResult.setStdout(prodcertstatusOutput);
        prodcertstatusMockResult.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mRunUtil.runTimedCmdSilently(
                                EasyMock.anyInt(), EasyMock.eq(PRODCERTSTATUS_CMD)))
                .andReturn(prodcertstatusMockResult)
                .times(1);

        String krbstatusOutput = "android-test ticket expires in 65d 19h";
        String krbstatusError = "Some terrible failure";
        CommandResult krbstatusMockResult = new CommandResult();
        krbstatusMockResult.setStdout(krbstatusOutput);
        krbstatusMockResult.setStderr(krbstatusError);
        krbstatusMockResult.setStatus(CommandStatus.FAILED);
        EasyMock.expect(mRunUtil.runTimedCmdSilently(EasyMock.anyInt(), EasyMock.eq(KRBSTATUS_CMD)))
                .andReturn(krbstatusMockResult)
                .times(1);
        EasyMock.replay(mRunUtil);

        Map<String, String> expected = new HashMap<>();
        expected.put(PRODCERTSTATUS_KEY, prodcertstatusOutput);
        expected.put(KRBSTATUS_KEY, krbstatusError);

        Assert.assertEquals(expected, mClusterDeviceMonitor.getAdditionalHostInfo());
        EasyMock.verify(mRunUtil);
    }
}
