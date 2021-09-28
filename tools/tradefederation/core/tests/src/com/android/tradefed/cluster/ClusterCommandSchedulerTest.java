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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.RETURNS_DEEP_STUBS;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.cluster.ClusterCommand.RequestType;
import com.android.tradefed.cluster.ClusterCommandScheduler.InvocationEventHandler;
import com.android.tradefed.command.CommandScheduler;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.FreeDeviceState;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.MockDeviceManager;
import com.android.tradefed.device.NoDeviceException;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ConsoleResultReporter;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestSummary;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner.TestLogData;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRestApiHelper;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.ZipUtil;
import com.android.tradefed.util.keystore.IKeyStoreClient;
import com.android.tradefed.util.keystore.StubKeyStoreClient;

import com.google.api.client.http.GenericUrl;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.http.HttpResponse;
import com.google.api.client.http.LowLevelHttpRequest;
import com.google.api.client.http.LowLevelHttpResponse;
import com.google.api.client.testing.http.MockHttpTransport;
import com.google.api.client.testing.http.MockLowLevelHttpRequest;
import com.google.api.client.testing.http.MockLowLevelHttpResponse;
import com.google.common.collect.ImmutableMap;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.easymock.IArgumentMatcher;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Stack;
import java.util.TreeMap;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ClusterCommandScheduler}. */
@RunWith(JUnit4.class)
public class ClusterCommandSchedulerTest {

    private static final String CLUSTER_ID = "free_pool";
    private static final String REQUEST_ID = "request_id";
    private static final String COMMAND_ID = "command_id";
    private static final String ATTEMPT_ID = "attempt_id";
    private static final String TASK_ID = "task_id";
    private static final String CMD_LINE = "test";
    private static final String DEVICE_SERIAL = "serial";

    @Rule public TestLogData mTestLog = new TestLogData();

    private IDeviceManager mMockDeviceManager;
    private IRestApiHelper mMockApiHelper;
    private IClusterClient mMockClusterClient;
    private ClusterOptions mMockClusterOptions;
    private IClusterEventUploader<ClusterCommandEvent> mMockEventUploader;
    private ClusterCommandScheduler mScheduler;
    private IClusterEventUploader<ClusterHostEvent> mMockHostUploader;
    // Test variable to store the args of last execCommand called by CommandScheduler.
    Stack<ArrayList<String>> mExecCmdArgs = new Stack<>();

    String[] getExecCommandArgs() {
        ArrayList<String> execCmdArgs = mExecCmdArgs.pop();
        String[] args = new String[execCmdArgs.size()];
        return execCmdArgs.toArray(args);
    }

    // Explicitly define this, so we can mock it
    private static interface ICommandEventUploader
            extends IClusterEventUploader<ClusterCommandEvent> {}

    @Before
    public void setUp() throws Exception {
        mMockHostUploader = EasyMock.createMock(IClusterEventUploader.class);
        mMockDeviceManager = EasyMock.createMock(IDeviceManager.class);
        mMockApiHelper = EasyMock.createMock(IRestApiHelper.class);
        mMockEventUploader = EasyMock.createMock(ICommandEventUploader.class);
        mMockClusterOptions = new ClusterOptions();
        mMockClusterOptions.setCheckFlashingPermitsLease(false);
        mMockClusterOptions.setClusterId(CLUSTER_ID);
        mMockClusterClient =
                new ClusterClient() {
                    @Override
                    public IClusterEventUploader<ClusterCommandEvent> getCommandEventUploader() {
                        return mMockEventUploader;
                    }

                    @Override
                    public IClusterEventUploader<ClusterHostEvent> getHostEventUploader() {
                        return mMockHostUploader;
                    }

                    @Override
                    public IClusterOptions getClusterOptions() {
                        return mMockClusterOptions;
                    }

                    @Override
                    IRestApiHelper getApiHelper() {
                        return mMockApiHelper;
                    }
                };

        mScheduler =
                new ClusterCommandScheduler() {

                    @Override
                    public IClusterOptions getClusterOptions() {
                        return mMockClusterOptions;
                    }

                    @Override
                    IClusterClient getClusterClient() {
                        return mMockClusterClient;
                    }

                    @Override
                    protected IDeviceManager getDeviceManager() {
                        return mMockDeviceManager;
                    }

                    @Override
                    public void execCommand(IScheduledInvocationListener listener, String[] args)
                            throws ConfigurationException, NoDeviceException {
                        ArrayList<String> execCmdArgs = new ArrayList<>();
                        for (String arg : args) {
                            execCmdArgs.add(arg);
                        }
                        mExecCmdArgs.push(execCmdArgs);
                    }

                    @Override
                    protected boolean dryRunCommand(
                            final InvocationEventHandler handler, String[] args) {
                        return false;
                    }
                };
    }

    private static class FakeHttpTransport extends MockHttpTransport {

        private byte[] mResponseBytes;

        public FakeHttpTransport(byte[] responseBytes) {
            mResponseBytes = responseBytes;
        }

        @Override
        public LowLevelHttpRequest buildRequest(String method, String url) throws IOException {
            return new MockLowLevelHttpRequest() {
                @Override
                public LowLevelHttpResponse execute() throws IOException {
                    MockLowLevelHttpResponse response = new MockLowLevelHttpResponse();
                    response.setContent(new ByteArrayInputStream(mResponseBytes));
                    return response;
                }
            };
        }
    }

    private HttpResponse buildHttpResponse(String response) throws IOException {
        HttpRequestFactory factory =
                new FakeHttpTransport(response.getBytes()).createRequestFactory();
        // The method and url aren't used by our fake transport, but they can't be null
        return factory.buildRequest("GET", new GenericUrl("http://example.com"), null).execute();
    }

    @After
    public void tearDown() throws Exception {
        mExecCmdArgs.clear();
    }

    private DeviceDescriptor createDevice(
            String product, String variant, DeviceAllocationState state) {
        return createDevice(DEVICE_SERIAL, product, variant, state);
    }

    private DeviceDescriptor createDevice(
            String serial, String product, String variant, DeviceAllocationState state) {
        return new DeviceDescriptor(
                serial, false, state, product, variant, "sdkVersion", "buildId", "batteryLevel");
    }

    @Test
    public void testGetAvailableDevices() {
        final List<DeviceDescriptor> deviceList = new ArrayList<>();
        deviceList.add(createDevice("product1", "variant1", DeviceAllocationState.Available));
        deviceList.add(createDevice("product2", "variant2", DeviceAllocationState.Available));
        deviceList.add(createDevice("product2", "variant2", DeviceAllocationState.Allocated));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Available));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Allocated));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Unavailable));
        EasyMock.expect(mMockDeviceManager.listAllDevices()).andReturn(deviceList);

        EasyMock.replay(mMockDeviceManager);
        final MultiMap<String, DeviceDescriptor> deviceMap =
                mScheduler.getDevices(mMockDeviceManager, false);
        EasyMock.verify(mMockDeviceManager);

        assertTrue(deviceMap.containsKey("product1:variant1"));
        assertEquals(1, deviceMap.get("product1:variant1").size());
        assertTrue(deviceMap.containsKey("product2:variant2"));
        assertEquals(2, deviceMap.get("product2:variant2").size());
        assertTrue(deviceMap.containsKey("product3:variant3"));
        assertEquals(3, deviceMap.get("product3:variant3").size());
    }

    @Test
    public void testGetDevices_available() {
        final List<DeviceDescriptor> deviceList = new ArrayList<>();
        deviceList.add(createDevice("product1", "variant1", DeviceAllocationState.Available));
        deviceList.add(createDevice("product2", "variant2", DeviceAllocationState.Available));
        deviceList.add(createDevice("product2", "variant2", DeviceAllocationState.Allocated));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Available));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Allocated));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Unavailable));
        EasyMock.expect(mMockDeviceManager.listAllDevices()).andReturn(deviceList);

        EasyMock.replay(mMockDeviceManager);
        final MultiMap<String, DeviceDescriptor> deviceMap =
                mScheduler.getDevices(mMockDeviceManager, true);
        EasyMock.verify(mMockDeviceManager);

        assertTrue(deviceMap.containsKey("product1:variant1"));
        assertEquals(1, deviceMap.get("product1:variant1").size());
        assertTrue(deviceMap.containsKey("product2:variant2"));
        assertEquals(1, deviceMap.get("product2:variant2").size());
        assertTrue(deviceMap.containsKey("product3:variant3"));
        assertEquals(1, deviceMap.get("product3:variant3").size());
    }

    @Test
    public void testGetDevices_LocalhostIpDevices() {
        final List<DeviceDescriptor> deviceList = new ArrayList<>();
        deviceList.add(
                createDevice(
                        "127.0.0.1:101", "product1", "variant1", DeviceAllocationState.Available));
        deviceList.add(
                createDevice(
                        "127.0.0.1:102", "product1", "variant1", DeviceAllocationState.Available));
        deviceList.add(createDevice("product2", "variant2", DeviceAllocationState.Allocated));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Available));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Unavailable));
        EasyMock.expect(mMockDeviceManager.listAllDevices()).andReturn(deviceList);

        EasyMock.replay(mMockDeviceManager);
        final MultiMap<String, DeviceDescriptor> deviceMap =
                mScheduler.getDevices(mMockDeviceManager, true);
        EasyMock.verify(mMockDeviceManager);

        assertFalse(deviceMap.containsKey("product1:variant1"));
        assertFalse(deviceMap.containsKey("product2:variant2"));
        assertTrue(deviceMap.containsKey("product3:variant3"));
        assertEquals(1, deviceMap.get("product3:variant3").size());
    }

    @Test
    public void testGetDevices_NoAvailableDevices() {
        final List<DeviceDescriptor> deviceList = new ArrayList<>();
        deviceList.add(createDevice("product1", "variant1", DeviceAllocationState.Allocated));
        deviceList.add(createDevice("product2", "variant2", DeviceAllocationState.Unavailable));
        deviceList.add(createDevice("product3", "variant3", DeviceAllocationState.Ignored));
        EasyMock.expect(mMockDeviceManager.listAllDevices()).andReturn(deviceList);

        EasyMock.replay(mMockDeviceManager);
        final MultiMap<String, DeviceDescriptor> deviceMap =
                mScheduler.getDevices(mMockDeviceManager, true);
        EasyMock.verify(mMockDeviceManager);

        assertTrue(deviceMap.isEmpty());
    }

    private JSONObject createCommandTask(
            String requestId, String commandId, String taskId, String attemptId, String commandLine)
            throws JSONException {

        JSONObject ret = new JSONObject();
        ret.put(REQUEST_ID, requestId);
        ret.put(COMMAND_ID, commandId);
        ret.put(ATTEMPT_ID, attemptId);
        ret.put(TASK_ID, taskId);
        ret.put("command_line", commandLine);
        return ret;
    }

    private JSONObject createLeaseResponse(JSONObject... tasks) throws JSONException {
        JSONObject response = new JSONObject();
        JSONArray array = new JSONArray();
        for (JSONObject task : tasks) {
            array.put(task);
        }
        response.put("tasks", array);
        return response;
    }

    @Test
    public void testFetchHostCommands() throws Exception {
        // Create some devices to fetch tasks for
        mMockClusterOptions.getDeviceGroup().put("group1", "s1");
        mMockClusterOptions.getDeviceGroup().put("group1", "s2");
        DeviceDescriptor d1 =
                createDevice("s1", "product1", "variant1", DeviceAllocationState.Available);
        DeviceDescriptor d2 =
                createDevice("s2", "product2", "variant2", DeviceAllocationState.Available);
        DeviceDescriptor d3 =
                createDevice("s3", "product2", "variant2", DeviceAllocationState.Available);
        String runTarget1 = "product1:variant1";
        String runTarget2 = "product2:variant2";
        final MultiMap<String, DeviceDescriptor> deviceMap = new MultiMap<>();
        deviceMap.put(runTarget1, d1);
        deviceMap.put(runTarget2, d2);
        deviceMap.put(runTarget2, d3);

        mMockClusterOptions.getNextClusterIds().add("cluster2");
        mMockClusterOptions.getNextClusterIds().add("cluster3");

        Capture<JSONObject> capture = new Capture<>();
        // Create some mock responses for the expected REST API calls
        JSONObject product1Response =
                createLeaseResponse(createCommandTask("1", "2", "3", "4", "command line 1"));
        EasyMock.expect(
                        mMockApiHelper.execute(
                                EasyMock.eq("POST"),
                                EasyMock.aryEq(new String[] {"tasks", "leasehosttasks"}),
                                EasyMock.eq(
                                        ImmutableMap.of(
                                                "cluster",
                                                CLUSTER_ID,
                                                "hostname",
                                                ClusterHostUtil.getHostName(),
                                                "num_tasks",
                                                Integer.toString(3))),
                                EasyMock.capture(capture)))
                .andReturn(buildHttpResponse(product1Response.toString()));
        // Actually fetch commands
        EasyMock.replay(mMockApiHelper, mMockEventUploader);
        final List<ClusterCommand> commands = mScheduler.fetchHostCommands(deviceMap);

        // Verity the http request body is correct.
        JSONArray deviceInfos = capture.getValue().getJSONArray("device_infos");
        JSONArray clusterIds = capture.getValue().getJSONArray("next_cluster_ids");
        assertEquals("group1", deviceInfos.getJSONObject(0).get("group_name"));
        assertEquals("s1", deviceInfos.getJSONObject(0).get("device_serial"));
        assertEquals("group1", deviceInfos.getJSONObject(1).get("group_name"));
        assertEquals("s2", deviceInfos.getJSONObject(1).get("device_serial"));
        assertFalse(deviceInfos.getJSONObject(2).has("group_name"));
        assertEquals("s3", deviceInfos.getJSONObject(2).get("device_serial"));
        assertEquals("cluster2", clusterIds.getString(0));
        assertEquals("cluster3", clusterIds.getString(1));
        // Verify that the commands were fetched
        EasyMock.verify(mMockApiHelper, mMockEventUploader);
        // expect 1 command allocated per device type based on availability and fetching algorithm
        assertEquals("commands size mismatch", 1, commands.size());
        ClusterCommand command = commands.get(0);
        assertEquals("1", command.getRequestId());
        assertEquals("2", command.getCommandId());
        assertEquals("3", command.getTaskId());
        assertEquals("4", command.getAttemptId());
        assertEquals("command line 1", command.getCommandLine());
    }

    @Test
    public void testFetchHostCommands_withFlashingPermitCheck() throws Exception {
        // Create some devices to fetch tasks for
        DeviceDescriptor d1 =
                createDevice("s1", "product1", "variant1", DeviceAllocationState.Available);
        String runTarget1 = "product1:variant1";
        DeviceDescriptor d2 =
                createDevice("s2", "product1", "variant1", DeviceAllocationState.Available);
        final MultiMap<String, DeviceDescriptor> deviceMap = new MultiMap<>();
        deviceMap.put(runTarget1, d1);
        deviceMap.put(runTarget1, d2);

        mMockClusterOptions.getNextClusterIds().add("cluster2");
        mMockClusterOptions.setCheckFlashingPermitsLease(true);
        Capture<JSONObject> capture = new Capture<>();
        // Create some mock responses for the expected REST API calls
        JSONObject product1Response =
                createLeaseResponse(createCommandTask("1", "2", "3", "4", "command line 1"));
        EasyMock.expect(
                        mMockApiHelper.execute(
                                EasyMock.eq("POST"),
                                EasyMock.aryEq(new String[] {"tasks", "leasehosttasks"}),
                                EasyMock.eq(
                                        ImmutableMap.of(
                                                "cluster",
                                                CLUSTER_ID,
                                                "hostname",
                                                ClusterHostUtil.getHostName(),
                                                "num_tasks",
                                                Integer.toString(1))),
                                EasyMock.capture(capture)))
                .andReturn(buildHttpResponse(product1Response.toString()));
        EasyMock.expect(mMockDeviceManager.getAvailableFlashingPermits()).andReturn(1);

        // Actually fetch commands
        EasyMock.replay(mMockApiHelper, mMockEventUploader, mMockDeviceManager);
        final List<ClusterCommand> commands = mScheduler.fetchHostCommands(deviceMap);
        assertEquals(1, commands.size());
        ClusterCommand command = commands.get(0);
        assertEquals("1", command.getRequestId());
        assertEquals("2", command.getCommandId());
        assertEquals("3", command.getTaskId());
        assertEquals("4", command.getAttemptId());
        assertEquals("command line 1", command.getCommandLine());
    }

    /** Test when the run target pattern contains repeated patterns. */
    @Test
    public void testRepeatedPattern() {
        String format = "foo-{PRODUCT}-{PRODUCT}:{PRODUCT_VARIANT}";
        mMockClusterOptions.setRunTargetFormat(format);
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        assertEquals(
                "foo-product-product:productVariant",
                ClusterHostUtil.getRunTarget(device, format, null));
    }

    /** Test default behavior when device serial is not set for command task. */
    @Test
    public void testExecCommandsWithNoSerials() {
        List<ClusterCommand> cmds = new ArrayList<>();
        ClusterCommand cmd = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        cmds.add(cmd);
        mScheduler.execCommands(cmds);
        assertEquals(CMD_LINE, cmds.get(0).getCommandLine());
        assertArrayEquals(new String[] {CMD_LINE}, getExecCommandArgs());
    }

    /** If device serial is specified for a command task append serial to it. */
    @Test
    public void testExecCommandWithSerial() {
        List<ClusterCommand> cmds = new ArrayList<>();
        ClusterCommand cmd = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        cmd.setTargetDeviceSerials(ArrayUtil.list("deviceSerial"));
        cmds.add(cmd);
        mScheduler.execCommands(cmds);
        assertEquals(CMD_LINE, cmds.get(0).getCommandLine());
        assertArrayEquals(
                new String[] {CMD_LINE, "--serial", "deviceSerial"}, getExecCommandArgs());
    }

    /** Multiple serials specified for a command task. */
    @Test
    public void testExecCommandWithMultipleSerials() {
        List<ClusterCommand> cmds = new ArrayList<>();
        ClusterCommand cmd = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        cmd.setTargetDeviceSerials(
                ArrayUtil.list("deviceSerial0", "deviceSerial1", "deviceSerial2"));
        cmds.add(cmd);
        mScheduler.execCommands(cmds);
        assertEquals(CMD_LINE, cmds.get(0).getCommandLine());
        assertArrayEquals(
                new String[] {
                    CMD_LINE,
                    "--serial",
                    "deviceSerial0",
                    "--serial",
                    "deviceSerial1",
                    "--serial",
                    "deviceSerial2"
                },
                getExecCommandArgs());
    }

    /** Multiple serials specified for multiple commands. */
    @Test
    public void testExecCommandWithMultipleCommandsAndSerials() {
        List<String> serials = ArrayUtil.list("deviceSerial0", "deviceSerial1", "deviceSerial2");
        List<ClusterCommand> cmds = new ArrayList<>();
        ClusterCommand cmd0 = new ClusterCommand("command_id0", "task_id0", CMD_LINE);
        cmd0.setTargetDeviceSerials(serials);
        cmds.add(cmd0);
        ClusterCommand cmd1 = new ClusterCommand("command_id1", "task_id1", "test1");
        cmd1.setTargetDeviceSerials(serials);
        cmds.add(cmd1);
        mScheduler.execCommands(cmds);
        assertEquals(CMD_LINE, cmds.get(0).getCommandLine());
        assertEquals("test1", cmds.get(1).getCommandLine());
        assertArrayEquals(
                new String[] {
                    "test1",
                    "--serial",
                    "deviceSerial0",
                    "--serial",
                    "deviceSerial1",
                    "--serial",
                    "deviceSerial2"
                },
                getExecCommandArgs());
        assertArrayEquals(
                new String[] {
                    CMD_LINE,
                    "--serial",
                    "deviceSerial0",
                    "--serial",
                    "deviceSerial1",
                    "--serial",
                    "deviceSerial2"
                },
                getExecCommandArgs());
    }

    static ClusterCommandEvent checkClusterCommandEvent(
            ClusterCommandEvent.Type type, Set<String> deviceSerials) {
        EasyMock.reportMatcher(
                new IArgumentMatcher() {
                    @Override
                    public boolean matches(Object object) {
                        if (!(object instanceof ClusterCommandEvent)) {
                            return false;
                        }

                        ClusterCommandEvent actual = (ClusterCommandEvent) object;
                        return (TASK_ID.equals(actual.getCommandTaskId())
                                && deviceSerials.equals(actual.getDeviceSerials())
                                && actual.getType() == type);
                    }

                    @Override
                    public void appendTo(StringBuffer buffer) {
                        buffer.append("checkEvent(");
                        buffer.append(type);
                        buffer.append(")");
                    }
                });
        return null;
    }

    static ClusterCommandEvent checkClusterCommandEvent(ClusterCommandEvent.Type type) {
        Set<String> deviceSerials = new HashSet<String>();
        deviceSerials.add(DEVICE_SERIAL);
        return checkClusterCommandEvent(type, deviceSerials);
    }

    @Test
    public void testInvocationEventHandler() {
        ClusterCommand mockCommand = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        IInvocationContext context = new InvocationContext();
        ITestDevice mockTestDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockTestDevice.getSerialNumber()).andReturn(DEVICE_SERIAL);
        EasyMock.expect(mockTestDevice.getIDevice()).andReturn(new StubDevice(DEVICE_SERIAL));
        context.addAllocatedDevice("", mockTestDevice);
        IBuildInfo mockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        context.addDeviceBuildInfo("", mockBuildInfo);
        ClusterCommandScheduler.InvocationEventHandler handler =
                mScheduler.new InvocationEventHandler(mockCommand);
        mMockClusterOptions.setCollectEarlyTestSummary(true);

        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationInitiated));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationStarted));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.TestRunInProgress));
        EasyMock.expectLastCall().anyTimes();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationEnded));
        mMockEventUploader.flush();
        Capture<ClusterCommandEvent> capture = new Capture<>();
        mMockEventUploader.postEvent(EasyMock.capture(capture));
        mMockEventUploader.flush();

        EasyMock.replay(mMockEventUploader, mockBuildInfo, mockTestDevice);
        handler.invocationInitiated(context);
        List<TestSummary> summaries = new ArrayList<>();
        summaries.add(
                new TestSummary(new TestSummary.TypedString("http://uri", TestSummary.Type.URI)));
        handler.putEarlySummary(summaries);
        handler.putSummary(summaries);
        handler.invocationStarted(context);
        handler.testRunStarted("test run", 1);
        handler.testStarted(new TestDescription("class", CMD_LINE));
        handler.testEnded(new TestDescription("class", CMD_LINE), new HashMap<String, Metric>());
        handler.testRunEnded(10L, new HashMap<String, Metric>());
        handler.invocationEnded(100L);
        context.addAllocatedDevice(DEVICE_SERIAL, mockTestDevice);
        context.addInvocationAttribute(InvocationMetricKey.FETCH_BUILD.toString(), "100");
        context.addInvocationAttribute(InvocationMetricKey.SETUP.toString(), "200");
        context.addInvocationAttribute(InvocationMetricKey.DEVICE_LOST_DETECTED.toString(), "1");
        Map<ITestDevice, FreeDeviceState> releaseMap = new HashMap<>();
        releaseMap.put(mockTestDevice, FreeDeviceState.AVAILABLE);
        handler.invocationComplete(context, releaseMap);
        EasyMock.verify(mMockEventUploader, mockBuildInfo, mockTestDevice);
        ClusterCommandEvent capturedEvent = capture.getValue();
        assertTrue(capturedEvent.getType().equals(ClusterCommandEvent.Type.InvocationCompleted));
        // Ensure we have not raised an unexpected error
        assertNull(capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_ERROR));
        assertEquals(
                "0", capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_FAILED_TEST_COUNT));
        assertEquals(
                "1", capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_PASSED_TEST_COUNT));
        assertEquals(
                "100",
                capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_FETCH_BUILD_TIME_MILLIS));
        assertEquals(
                "200", capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_SETUP_TIME_MILLIS));
        assertEquals(
                "URI: http://uri\n",
                capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_SUMMARY));
        assertEquals(
                "1",
                capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_LOST_DEVICE_DETECTED));
    }

    /** Test that the error count is the proper one. */
    @Test
    public void testInvocationEventHandler_counting() {
        ClusterCommand mockCommand = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        IInvocationContext context = new InvocationContext();
        ITestDevice mockTestDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockTestDevice.getSerialNumber()).andReturn(DEVICE_SERIAL);
        EasyMock.expect(mockTestDevice.getIDevice()).andReturn(new StubDevice(DEVICE_SERIAL));
        IBuildInfo mockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        context.addAllocatedDevice("", mockTestDevice);
        context.addDeviceBuildInfo("", mockBuildInfo);
        ClusterCommandScheduler.InvocationEventHandler handler =
                mScheduler.new InvocationEventHandler(mockCommand);

        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationInitiated));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationStarted));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.TestRunInProgress));
        EasyMock.expectLastCall().anyTimes();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationEnded));
        mMockEventUploader.flush();
        Capture<ClusterCommandEvent> capture = new Capture<>();
        mMockEventUploader.postEvent(EasyMock.capture(capture));
        mMockEventUploader.flush();

        EasyMock.replay(mMockEventUploader, mockBuildInfo, mockTestDevice);
        handler.invocationInitiated(context);
        handler.invocationStarted(context);
        handler.testRunStarted("test run", 1);
        TestDescription tid = new TestDescription("class", CMD_LINE);
        handler.testStarted(tid);
        handler.testFailed(tid, "failed");
        handler.testEnded(tid, new HashMap<String, Metric>());
        TestDescription tid2 = new TestDescription("class", "test2");
        handler.testStarted(tid2);
        handler.testAssumptionFailure(tid2, "I assume I failed");
        handler.testEnded(tid2, new HashMap<String, Metric>());
        handler.testRunEnded(10L, new HashMap<String, Metric>());
        handler.testRunStarted("failed test run", 1);
        TestDescription tid3 = new TestDescription("class", "test3");
        handler.testStarted(tid3);
        handler.testFailed(tid3, "test terminated without result");
        handler.testRunFailed("test runner crashed");
        handler.testRunEnded(10L, new HashMap<String, Metric>());
        handler.invocationEnded(100L);
        context.addAllocatedDevice(DEVICE_SERIAL, mockTestDevice);
        Map<ITestDevice, FreeDeviceState> releaseMap = new HashMap<>();
        releaseMap.put(mockTestDevice, FreeDeviceState.AVAILABLE);
        handler.invocationComplete(context, releaseMap);
        EasyMock.verify(mMockEventUploader, mockBuildInfo, mockTestDevice);
        ClusterCommandEvent capturedEvent = capture.getValue();
        assertTrue(capturedEvent.getType().equals(ClusterCommandEvent.Type.InvocationCompleted));
        // Ensure we have not raised an unexpected error
        assertNull(capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_ERROR));
        // We only count test failure and not assumption failures.
        assertEquals(
                "2", capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_FAILED_TEST_COUNT));
        assertEquals(
                "0", capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_PASSED_TEST_COUNT));
        assertEquals(
                "1",
                capturedEvent.getData().get(ClusterCommandEvent.DATA_KEY_FAILED_TEST_RUN_COUNT));
    }

    @Test
    public void testInvocationEventHandler_longTestRun() {
        ClusterCommand mockCommand = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        IInvocationContext context = new InvocationContext();
        ITestDevice mockTestDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockTestDevice.getSerialNumber()).andReturn(DEVICE_SERIAL);
        EasyMock.expect(mockTestDevice.getIDevice()).andReturn(new StubDevice(DEVICE_SERIAL));
        context.addAllocatedDevice("", mockTestDevice);
        IBuildInfo mockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        context.addDeviceBuildInfo("", mockBuildInfo);
        ClusterCommandScheduler.InvocationEventHandler handler =
                mScheduler.new InvocationEventHandler(mockCommand);
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationInitiated));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationStarted));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.TestRunInProgress));
        EasyMock.expectLastCall().anyTimes();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationEnded));
        mMockEventUploader.flush();
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(ClusterCommandEvent.Type.InvocationCompleted));
        mMockEventUploader.flush();

        EasyMock.replay(mMockEventUploader, mockBuildInfo, mockTestDevice);
        handler.invocationInitiated(context);
        handler.invocationStarted(context);
        handler.testRunStarted("test run", 1);
        handler.testStarted(new TestDescription("class", CMD_LINE));
        handler.testEnded(new TestDescription("class", CMD_LINE), new HashMap<String, Metric>());
        handler.testRunEnded(10L, new HashMap<String, Metric>());
        handler.invocationEnded(100L);
        context.addAllocatedDevice(DEVICE_SERIAL, mockTestDevice);
        Map<ITestDevice, FreeDeviceState> releaseMap = new HashMap<>();
        releaseMap.put(mockTestDevice, FreeDeviceState.AVAILABLE);
        handler.invocationComplete(context, releaseMap);
        EasyMock.verify(mMockEventUploader, mockBuildInfo, mockTestDevice);
    }

    @Test
    public void testInvocationEventHandler_multiDevice() {
        ClusterCommand mockCommand = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        IInvocationContext context = new InvocationContext();

        ITestDevice mockTestDevice1 = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockTestDevice1.getSerialNumber()).andReturn(DEVICE_SERIAL);
        EasyMock.expect(mockTestDevice1.getIDevice()).andReturn(new StubDevice(DEVICE_SERIAL));
        context.addAllocatedDevice("device1", mockTestDevice1);
        ITestDevice mockTestDevice2 = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mockTestDevice2.getSerialNumber()).andReturn("s2");
        EasyMock.expect(mockTestDevice2.getIDevice()).andReturn(new StubDevice("s2"));
        context.addAllocatedDevice("device2", mockTestDevice2);
        IBuildInfo mockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        context.addDeviceBuildInfo("", mockBuildInfo);
        ClusterCommandScheduler.InvocationEventHandler handler =
                mScheduler.new InvocationEventHandler(mockCommand);

        Set<String> deviceSerials = new HashSet<>();
        deviceSerials.add(DEVICE_SERIAL);
        deviceSerials.add("s2");
        mMockEventUploader.postEvent(
                checkClusterCommandEvent(
                        ClusterCommandEvent.Type.InvocationInitiated, deviceSerials));
        mMockEventUploader.flush();

        EasyMock.replay(mMockEventUploader, mockBuildInfo, mockTestDevice1, mockTestDevice2);
        handler.invocationInitiated(context);
    }

    /**
     * Test that when dry-run is used we validate the config and no ConfigurationException gets
     * thrown.
     */
    @Test
    public void testExecCommandsWithDryRun() {
        mScheduler =
                new ClusterCommandScheduler() {

                    @Override
                    public IClusterOptions getClusterOptions() {
                        return mMockClusterOptions;
                    }

                    @Override
                    IClusterClient getClusterClient() {
                        return mMockClusterClient;
                    }

                    @Override
                    public void execCommand(IScheduledInvocationListener listener, String[] args)
                            throws ConfigurationException, NoDeviceException {
                        ArrayList<String> execCmdArgs = new ArrayList<>();
                        for (String arg : args) {
                            execCmdArgs.add(arg);
                        }
                        mExecCmdArgs.push(execCmdArgs);
                    }

                    @Override
                    protected IKeyStoreClient getKeyStoreClient() {
                        return new StubKeyStoreClient();
                    }
                };
        ClusterCommand cmd = new ClusterCommand(COMMAND_ID, TASK_ID, "empty --dry-run");
        mScheduler.execCommands(Arrays.asList(cmd));
        assertEquals("empty --dry-run", cmd.getCommandLine());
        // Nothing gets executed
        assertTrue(mExecCmdArgs.isEmpty());
    }

    // Helper class for more functional like tests.
    private class TestableClusterCommandScheduler extends ClusterCommandScheduler {

        private IDeviceManager manager = new MockDeviceManager(1);
        // track whether stopInvocation was called
        private boolean stopInvocationCalled;

        @Override
        public IClusterOptions getClusterOptions() {
            return mMockClusterOptions;
        }

        @Override
        IClusterClient getClusterClient() {
            return mMockClusterClient;
        }

        @Override
        protected boolean dryRunCommand(final InvocationEventHandler handler, String[] args) {
            return false;
        }

        @Override
        protected IDeviceManager getDeviceManager() {
            return manager;
        }

        // Direct getter to avoid making getDeviceManager public.
        public IDeviceManager getTestManager() {
            return manager;
        }

        @Override
        protected void initLogging() {
            // ignore
        }

        @Override
        protected void cleanUp() {
            // ignore
        }

        @Override
        public boolean stopInvocation(int invocationId, String cause) {
            return (stopInvocationCalled = true);
        }

        public boolean wasStopInvocationCalled() {
            return stopInvocationCalled;
        }
    }

    /** Test that when a provider returns a null build, we still handle it gracefully. */
    @Test
    public void testExecCommands_nullBuild() throws Exception {
        try {
            GlobalConfiguration.createGlobalConfiguration(new String[] {});
        } catch (IllegalStateException e) {
            // in case Global config is already initialized.
        }
        File tmpLogDir = FileUtil.createTempDir("clusterschedulertest");
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        List<ClusterCommand> cmds = new ArrayList<>();
        // StubBuildProvider of empty.xml can return a null instead of build with --return-null
        ClusterCommand cmd =
                new ClusterCommand(
                        COMMAND_ID,
                        TASK_ID,
                        "empty --return-null " + "--log-file-path " + tmpLogDir.getAbsolutePath());
        cmds.add(cmd);
        IDeviceManager m = scheduler.getTestManager();
        scheduler.start();
        try {
            // execCommands is going to allocate a device to execute the command.
            scheduler.execCommands(cmds);
            assertEquals(0, ((MockDeviceManager) m).getQueueOfAvailableDeviceSize());
            assertEquals(
                    "empty --return-null --log-file-path " + tmpLogDir.getAbsolutePath(),
                    cmds.get(0).getCommandLine());
            scheduler.shutdownOnEmpty();
            scheduler.join(2000);
            // Give it a bit of time as the device re-appearing can be slow.
            RunUtil.getDefault().sleep(200L);
            // There is only one device so allocation should succeed if device was released.
            assertEquals(1, ((MockDeviceManager) m).getQueueOfAvailableDeviceSize());
            ITestDevice device = m.allocateDevice();
            assertNotNull(device);
        } finally {
            scheduler.shutdown();
            File zipLog = ZipUtil.createZip(tmpLogDir);
            try (FileInputStreamSource source = new FileInputStreamSource(zipLog, true)) {
                mTestLog.addTestLog("testExecCommands_nullBuild", LogDataType.ZIP, source);
            }
            FileUtil.recursiveDelete(tmpLogDir);
        }
    }

    /** Test that when a provider throws a build error retrieval, we still handle it gracefully. */
    @Test
    public void testExecCommands_buildRetrievalError() throws Exception {
        try {
            // we need to initialize the GlobalConfiguration when running directly in IDE.
            GlobalConfiguration.createGlobalConfiguration(new String[] {});
        } catch (IllegalStateException e) {
            // in case Global config is already initialized, when running in the infra.
        }
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        File tmpLogDir = FileUtil.createTempDir("clusterschedulertest");
        List<ClusterCommand> cmds = new ArrayList<>();
        // StubBuildProvider of empty.xml can throw a build error if requested via
        // --throw-build-error
        ClusterCommand cmd =
                new ClusterCommand(
                        COMMAND_ID,
                        TASK_ID,
                        "empty --throw-build-error --log-file-path " + tmpLogDir.getAbsolutePath());
        cmds.add(cmd);
        IDeviceManager m = scheduler.getTestManager();
        scheduler.start();
        try {
            scheduler.execCommands(cmds);
            assertEquals(0, ((MockDeviceManager) m).getQueueOfAvailableDeviceSize());
            scheduler.shutdownOnEmpty();
            scheduler.join(5000);
            // Give it a bit of time as the device re-appearing can be slow.
            RunUtil.getDefault().sleep(200L);
            // There is only one device so allocation should succeed if device was released.
            assertEquals(1, ((MockDeviceManager) m).getQueueOfAvailableDeviceSize());
            ITestDevice device = m.allocateDevice();
            assertNotNull(device);
        } finally {
            scheduler.shutdown();
            FileUtil.recursiveDelete(tmpLogDir);
        }
    }

    private ClusterCommand createMockManagedCommand(int numDevices) {
        return createMockManagedCommand(numDevices, null, null);
    }

    private ClusterCommand createMockManagedCommand(
            int numDevices, Integer shardCount, Integer shardIndex) {
        ClusterCommand cmd =
                new ClusterCommand(
                        REQUEST_ID,
                        COMMAND_ID,
                        TASK_ID,
                        "command",
                        UUID.randomUUID().toString(),
                        RequestType.MANAGED,
                        shardCount,
                        shardIndex);
        for (int i = 0; i < numDevices; i++) {
            cmd.getTargetDeviceSerials().add(String.format("serial%d", i));
        }
        return cmd;
    }

    private TestEnvironment createMockTestEnvironment() {
        TestEnvironment testEnvironment = new TestEnvironment();
        testEnvironment.addEnvVar("ENV1", "env1");
        testEnvironment.addEnvVar("ENV2", "env2");
        testEnvironment.addSetupScripts("script1");
        testEnvironment.addSetupScripts("script2");
        testEnvironment.addJvmOption("option1");
        testEnvironment.addJavaProperty("JAVA1", "java1");
        testEnvironment.addJavaProperty("JAVA2", "java2");
        testEnvironment.setOutputFileUploadUrl("output_file_upload_url");
        testEnvironment.addOutputFilePattern("output_file_pattern1");
        testEnvironment.addOutputFilePattern("output_file_pattern2");
        return testEnvironment;
    }

    private List<TestResource> createMockTestResources() {
        List<TestResource> testResources = new ArrayList<TestResource>();
        testResources.add(new TestResource("name1", "url2"));
        testResources.add(new TestResource("name2", "url2"));
        return testResources;
    }

    private void verifyConfig(
            IConfiguration config,
            ClusterCommand cmd,
            TestEnvironment testEnvironment,
            List<TestResource> testResources,
            File workDir) {
        List<IDeviceConfiguration> deviceConfigs = config.getDeviceConfig();
        assertEquals(cmd.getTargetDeviceSerials().size(), deviceConfigs.size());
        for (int i = 0; i < cmd.getTargetDeviceSerials().size(); i++) {
            String serial = cmd.getTargetDeviceSerials().get(i);
            Collection<String> serials =
                    deviceConfigs.get(i).getDeviceRequirements().getSerials(null);
            assertTrue(serials.size() == 1 && serials.contains(serial));
        }

        ClusterBuildProvider buildProvider =
                (ClusterBuildProvider) deviceConfigs.get(0).getBuildProvider();
        assertEquals(testResources.size(), buildProvider.getTestResources().size());
        for (TestResource r : testResources) {
            assertEquals(r.getUrl(), buildProvider.getTestResources().get(r.getName()));
        }
        ClusterCommandLauncher test = (ClusterCommandLauncher) config.getTests().get(0);
        assertEquals(cmd.getCommandLine(), test.getCommandLine());

        Map<String, String> envVars = new TreeMap<>();
        envVars.put("TF_WORK_DIR", workDir.getAbsolutePath());
        envVars.putAll(testEnvironment.getEnvVars());
        assertEquals(envVars, test.getEnvVars());
        assertEquals(testEnvironment.getJvmOptions(), test.getJvmOptions());
        assertEquals(testEnvironment.getSetupScripts(), test.getSetupScripts());
        assertEquals(testEnvironment.getJavaProperties(), test.getJavaProperties());
        assertEquals(testEnvironment.useSubprocessReporting(), test.useSubprocessReporting());
        ClusterLogSaver logSaver = (ClusterLogSaver) config.getLogSaver();
        assertEquals(cmd.getAttemptId(), logSaver.getAttemptId());
        assertEquals(
                String.format(
                        "%s/%s/%s/",
                        testEnvironment.getOutputFileUploadUrl(),
                        cmd.getCommandId(),
                        cmd.getAttemptId()),
                logSaver.getOutputFileUploadUrl());
        assertEquals(testEnvironment.getOutputFilePatterns(), logSaver.getOutputFilePatterns());

        // Verify all Tradefed config objects.
        for (TradefedConfigObject.Type type : TradefedConfigObject.Type.values()) {
            String typeName;
            switch (type) {
                case TARGET_PREPARER:
                    typeName = Configuration.TARGET_PREPARER_TYPE_NAME;
                    break;
                case RESULT_REPORTER:
                    typeName = Configuration.RESULT_REPORTER_TYPE_NAME;
                    break;
                default:
                    continue;
            }
            List<Object> configObjs;
            if (TradefedConfigObject.Type.TARGET_PREPARER.equals(type)) {
                configObjs =
                        new ArrayList<>(
                                config.getDeviceConfig().get(0).getAllObjectOfType(typeName));
            } else {
                configObjs = new ArrayList<>(config.getAllConfigurationObjectsOfType(typeName));
            }
            for (TradefedConfigObject configDef : testEnvironment.getTradefedConfigObjects()) {
                if (configDef.getType() != type) {
                    continue;
                }
                // If configObjs is empty, it means we failed find objects for some configDefs.
                assertFalse(configObjs.isEmpty());
                while (!configObjs.isEmpty()) {
                    Object configObj = configObjs.remove(0);
                    if (!configObj.getClass().getName().equals(configDef.getClassName())) {
                        continue;
                    }
                }
            }
        }
    }

    /** Tests an execution of a managed cluster command. */
    @Test
    public void testExecManagedClusterCommand() throws Exception {
        File workDir = null;
        try {
            ClusterCommand cmd = createMockManagedCommand(1);
            workDir = new File(System.getProperty("java.io.tmpdir"), cmd.getAttemptId());
            TestEnvironment testEnvironment = createMockTestEnvironment();
            List<TestResource> testResources = createMockTestResources();
            TestContext testContext = new TestContext();
            mMockClusterClient = Mockito.spy(mMockClusterClient);
            Mockito.doReturn(testEnvironment)
                    .when(mMockClusterClient)
                    .getTestEnvironment(REQUEST_ID);
            Mockito.doReturn(testResources).when(mMockClusterClient).getTestResources(REQUEST_ID);
            Mockito.doReturn(testContext)
                    .when(mMockClusterClient)
                    .getTestContext(REQUEST_ID, COMMAND_ID);
            InvocationEventHandler invocationEventHandler =
                    mScheduler.new InvocationEventHandler(cmd);

            mScheduler.execManagedClusterCommand(cmd, invocationEventHandler);

            String[] args = getExecCommandArgs();
            assertTrue(args.length > 0);
            IConfiguration config =
                    ConfigurationFactory.getInstance().createConfigurationFromArgs(args);
            verifyConfig(config, cmd, testEnvironment, testResources, workDir);
        } finally {
            if (workDir != null) {
                // Clean up work directory
                FileUtil.recursiveDelete(workDir);
            }
        }
    }

    /** Tests an execution of a managed cluster command for multiple devices. */
    @Test
    public void testExecManagedClusterCommand_multiDeviceTest() throws Exception {
        File workDir = null;
        try {
            ClusterCommand cmd = createMockManagedCommand(5);
            workDir = new File(System.getProperty("java.io.tmpdir"), cmd.getAttemptId());
            TestEnvironment testEnvironment = createMockTestEnvironment();
            List<TestResource> testResources = createMockTestResources();
            TestContext testContext = new TestContext();
            mMockClusterClient = Mockito.spy(mMockClusterClient);
            Mockito.doReturn(testEnvironment)
                    .when(mMockClusterClient)
                    .getTestEnvironment(REQUEST_ID);
            Mockito.doReturn(testResources).when(mMockClusterClient).getTestResources(REQUEST_ID);
            Mockito.doReturn(testContext)
                    .when(mMockClusterClient)
                    .getTestContext(REQUEST_ID, COMMAND_ID);
            InvocationEventHandler invocationEventHandler =
                    mScheduler.new InvocationEventHandler(cmd);

            mScheduler.execManagedClusterCommand(cmd, invocationEventHandler);

            String[] args = getExecCommandArgs();
            assertTrue(args.length > 0);
            IConfiguration config =
                    ConfigurationFactory.getInstance().createConfigurationFromArgs(args);
            verifyConfig(config, cmd, testEnvironment, testResources, workDir);
        } finally {
            if (workDir != null) {
                // Clean up work directory
                FileUtil.recursiveDelete(workDir);
            }
        }
    }

    /** Tests an execution of a sharded managed cluster command. */
    @Test
    public void testExecManagedClusterCommand_shardedTest() throws Exception {
        File workDir = null;
        try {
            ClusterCommand cmd = createMockManagedCommand(1, 100, 0);
            workDir = new File(System.getProperty("java.io.tmpdir"), cmd.getAttemptId());
            TestEnvironment testEnvironment = createMockTestEnvironment();
            List<TestResource> testResources = createMockTestResources();
            TestContext testContext = new TestContext();
            mMockClusterClient = Mockito.spy(mMockClusterClient);
            Mockito.doReturn(testEnvironment)
                    .when(mMockClusterClient)
                    .getTestEnvironment(REQUEST_ID);
            Mockito.doReturn(testResources).when(mMockClusterClient).getTestResources(REQUEST_ID);
            Mockito.doReturn(testContext)
                    .when(mMockClusterClient)
                    .getTestContext(REQUEST_ID, COMMAND_ID);
            InvocationEventHandler invocationEventHandler =
                    mScheduler.new InvocationEventHandler(cmd);

            mScheduler.execManagedClusterCommand(cmd, invocationEventHandler);

            String[] args = getExecCommandArgs();
            assertTrue(args.length > 0);
            IConfiguration config =
                    ConfigurationFactory.getInstance().createConfigurationFromArgs(args);
            verifyConfig(config, cmd, testEnvironment, testResources, workDir);
        } finally {
            if (workDir != null) {
                // Clean up work directory
                FileUtil.recursiveDelete(workDir);
            }
        }
    }

    /** Tests an execution of a managed cluster command for a IO exception case. */
    @Test
    public void testExecManagedClusterCommand_ioException() throws Exception {
        ClusterCommand cmd = createMockManagedCommand(1);
        File workDir = new File(System.getProperty("java.io.tmpdir"), cmd.getAttemptId());
        mMockClusterClient = Mockito.spy(mMockClusterClient);
        Mockito.doThrow(new IOException()).when(mMockClusterClient).getTestEnvironment(REQUEST_ID);
        InvocationEventHandler invocationEventHandler = mScheduler.new InvocationEventHandler(cmd);

        try {
            mScheduler.execManagedClusterCommand(cmd, invocationEventHandler);
            fail("IOException not thrown");
        } catch (IOException e) {
        }
        assertFalse("work directory was not cleaned up", workDir.exists());
    }

    /** A mock target preparer to test injected TF config objects. */
    public static class MockTargetPreparer implements ITargetPreparer {

        @Option(name = "int", description = "An int value")
        private int mInt;

        @Option(name = "string", description = "A string value")
        private String mString;

        @Option(name = "list", description = "A list of values")
        private List<String> mList = new ArrayList<>();

        @Option(name = "map", description = "A map of key/value pairs")
        private Map<String, String> mMap = new TreeMap<>();

        @Override
        public void setUp(ITestDevice device, IBuildInfo buildInfo)
                throws TargetSetupError, BuildError, DeviceNotAvailableException {
            // Do nothing
        }

        public int getInt() {
            return mInt;
        }

        public String getString() {
            return mString;
        }

        public List<String> getList() {
            return mList;
        }

        public Map<String, String> getMap() {
            return mMap;
        }
    }

    /** Tests an execution of a managed cluster command with addition config objects. */
    @Test
    public void testExecManagedClusterCommand_withTradefedConfigObjects() throws Exception {
        File workDir = null;
        try {
            ClusterCommand cmd = createMockManagedCommand(1);
            workDir = new File(System.getProperty("java.io.tmpdir"), cmd.getAttemptId());
            TestEnvironment testEnvironment = createMockTestEnvironment();
            MultiMap<String, String> optionMap = new MultiMap<>();
            optionMap.put("int", "1000");
            optionMap.put("string", "foo");
            optionMap.put("list", "foo");
            optionMap.put("list", "bar");
            optionMap.put("list", "zzz");
            optionMap.put("map", "foo=bar");
            testEnvironment.addTradefedConfigObject(
                    new TradefedConfigObject(
                            TradefedConfigObject.Type.TARGET_PREPARER,
                            MockTargetPreparer.class.getName(),
                            optionMap));
            testEnvironment.addTradefedConfigObject(
                    new TradefedConfigObject(
                            TradefedConfigObject.Type.RESULT_REPORTER,
                            ConsoleResultReporter.class.getName(),
                            new MultiMap<>()));
            List<TestResource> testResources = createMockTestResources();
            TestContext testContext = new TestContext();
            mMockClusterClient = Mockito.spy(mMockClusterClient);
            Mockito.doReturn(testEnvironment)
                    .when(mMockClusterClient)
                    .getTestEnvironment(REQUEST_ID);
            Mockito.doReturn(testResources).when(mMockClusterClient).getTestResources(REQUEST_ID);
            Mockito.doReturn(testContext)
                    .when(mMockClusterClient)
                    .getTestContext(REQUEST_ID, COMMAND_ID);
            InvocationEventHandler invocationEventHandler =
                    mScheduler.new InvocationEventHandler(cmd);

            mScheduler.execManagedClusterCommand(cmd, invocationEventHandler);

            String[] args = getExecCommandArgs();
            assertTrue(args.length > 0);
            IConfiguration config =
                    ConfigurationFactory.getInstance().createConfigurationFromArgs(args);
            verifyConfig(config, cmd, testEnvironment, testResources, workDir);

            // Verify option values.
            Collection<Object> configObjs =
                    config.getAllConfigurationObjectsOfType(
                            Configuration.TARGET_PREPARER_TYPE_NAME);
            MockTargetPreparer configObj =
                    (MockTargetPreparer)
                            configObjs
                                    .stream()
                                    .filter(
                                            (obj) -> {
                                                return obj instanceof MockTargetPreparer;
                                            })
                                    .findFirst()
                                    .get();
            assertEquals(1000, configObj.getInt());
            assertEquals("foo", configObj.getString());
            assertEquals(Arrays.asList("foo", "bar", "zzz"), configObj.getList());
            assertEquals("bar", configObj.getMap().get("foo"));
        } finally {
            // Clean up work directory
            FileUtil.recursiveDelete(workDir);
        }
    }

    @Test
    public void testShutdown_stopsHeartbeat() {
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        scheduler.start();
        assertFalse(scheduler.getHeartbeatThreadPool().isTerminated());
        scheduler.shutdown();
        assertTrue(scheduler.getHeartbeatThreadPool().isTerminated());
    }

    @Test
    public void testShutdownHard_stopsHeartbeat() {
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        scheduler.start();
        assertFalse(scheduler.getHeartbeatThreadPool().isTerminated());
        scheduler.shutdownHard();
        assertTrue(scheduler.getHeartbeatThreadPool().isTerminated());
    }

    /**
     * Ensure that we do not thrown an exception from scheduling the heartbeat after calling
     * shutdown on the thread pool.
     */
    @Test
    public void testShutdownHearbeat() throws Exception {
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        scheduler.getHeartbeatThreadPool().shutdown();
        scheduler
                .getHeartbeatThreadPool()
                .scheduleAtFixedRate(
                        new Runnable() {
                            @Override
                            public void run() {
                                RunUtil.getDefault().sleep(500);
                            }
                        },
                        0,
                        100,
                        TimeUnit.MILLISECONDS);
        boolean res = scheduler.getHeartbeatThreadPool().awaitTermination(5, TimeUnit.SECONDS);
        assertTrue("HeartBeat scheduler did not terminate.", res);
    }

    /** Tests whether the checkCommandStatus option is respected. */
    @Test
    public void testCheckCommandState_option() {
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();

        // create new heartbeat
        ClusterCommand command = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        InvocationEventHandler handler = scheduler.new InvocationEventHandler(command);
        Runnable heartbeat = handler.new HeartbeatSender();

        // populate invocation context
        IInvocationContext context = Mockito.mock(IInvocationContext.class, RETURNS_DEEP_STUBS);
        Mockito.when(context.getInvocationId()).thenReturn("1");
        handler.invocationStarted(context);

        // command status is CANCELED
        mMockClusterClient = Mockito.mock(IClusterClient.class, RETURNS_DEEP_STUBS);
        Mockito.when(mMockClusterClient.getCommandState(any(), any()))
                .thenReturn(ClusterCommand.State.CANCELED);

        // not stopped if check is disabled
        mMockClusterOptions.setCheckCommandState(false);
        heartbeat.run();
        assertFalse(scheduler.wasStopInvocationCalled());

        // stopped if check is enabled
        mMockClusterOptions.setCheckCommandState(true);
        heartbeat.run();
        assertTrue(scheduler.wasStopInvocationCalled());

        Mockito.verify(mMockClusterClient, Mockito.times(1)).getCommandState(any(), any());
    }

    /** Tests whether the heartbeat can determine the invocationId to stop. */
    @Test
    public void testCheckCommandState_invocationId() {
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        mMockClusterOptions.setCheckCommandState(true);

        // create new heartbeat
        ClusterCommand command = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        InvocationEventHandler handler = scheduler.new InvocationEventHandler(command);
        Runnable heartbeat = handler.new HeartbeatSender();

        // command status is CANCELED
        mMockClusterClient = Mockito.mock(IClusterClient.class, RETURNS_DEEP_STUBS);
        Mockito.when(mMockClusterClient.getCommandState(any(), any()))
                .thenReturn(ClusterCommand.State.CANCELED);

        // not stopped without invocation context
        heartbeat.run();
        assertFalse(scheduler.wasStopInvocationCalled());

        // not stopped if invocation ID missing
        IInvocationContext context = Mockito.mock(IInvocationContext.class, RETURNS_DEEP_STUBS);
        handler.invocationStarted(context);
        heartbeat.run();
        assertFalse(scheduler.wasStopInvocationCalled());

        // not stopped if invocation ID is non-numeric
        Mockito.when(context.getInvocationId()).thenReturn("ID");
        heartbeat.run();
        assertFalse(scheduler.wasStopInvocationCalled());

        // stopped if invocation ID is numeric
        Mockito.when(context.getInvocationId()).thenReturn("1");
        heartbeat.run();
        assertTrue(scheduler.wasStopInvocationCalled());

        Mockito.verify(mMockClusterClient, Mockito.times(4)).getCommandState(any(), any());
    }

    /** Tests whether the heartbeat can determine the cluster command state. */
    @Test
    public void testCheckCommandState_status() throws IOException {
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        mMockClusterOptions.setCheckCommandState(true);

        // create new heartbeat
        ClusterCommand command = new ClusterCommand(COMMAND_ID, TASK_ID, CMD_LINE);
        InvocationEventHandler handler = scheduler.new InvocationEventHandler(command);
        Runnable heartbeat = handler.new HeartbeatSender();

        // populate invocation context
        IInvocationContext context = Mockito.mock(IInvocationContext.class, RETURNS_DEEP_STUBS);
        Mockito.when(context.getInvocationId()).thenReturn("1");
        handler.invocationStarted(context);

        mMockApiHelper = Mockito.mock(IRestApiHelper.class);

        // not stopped if status is RUNNING
        Mockito.when(mMockApiHelper.execute(any(), any(), any(), any()))
                .thenReturn(buildHttpResponse("{\"state\": \"RUNNING\"}"));
        heartbeat.run();
        assertFalse(scheduler.wasStopInvocationCalled());

        // not stopped if status is UNKNOWN
        Mockito.when(mMockApiHelper.execute(any(), any(), any(), any()))
                .thenReturn(buildHttpResponse("{\"state\": \"INVALID\"}"));
        heartbeat.run();
        assertFalse(scheduler.wasStopInvocationCalled());

        // stopped if status is CANCELED
        Mockito.when(mMockApiHelper.execute(any(), any(), any(), any()))
                .thenReturn(buildHttpResponse("{\"state\": \"CANCELED\"}"));
        heartbeat.run();
        assertTrue(scheduler.wasStopInvocationCalled());

        Mockito.verify(mMockApiHelper, Mockito.times(3)).execute(any(), any(), any(), any());
    }

    /** Tests upload events with specific host state. */
    @Test
    public void testUploadHostEventWithState() {
        Capture<ClusterHostEvent> capture = new Capture<>();
        mMockHostUploader.postEvent(EasyMock.capture(capture));
        mMockHostUploader.flush();
        EasyMock.replay(mMockHostUploader);
        // Ignore exceptions here, only test uploading host states.
        TestableClusterCommandScheduler scheduler = new TestableClusterCommandScheduler();
        scheduler.start();
        EasyMock.verify(mMockHostUploader);
        ClusterHostEvent hostEvent = capture.getValue();
        assertEquals(CLUSTER_ID, hostEvent.getClusterId());
        assertEquals(CommandScheduler.HostState.RUNNING, hostEvent.getHostState());
        scheduler.stop();
    }
}
