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

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.cluster.ClusterHostEvent.HostEventType;
import com.android.tradefed.command.CommandScheduler;
import com.android.tradefed.command.ICommandScheduler;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.FreeDeviceState;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.NoDeviceException;
import com.android.tradefed.device.battery.BatteryController;
import com.android.tradefed.device.battery.IBatteryInfo;
import com.android.tradefed.device.battery.IBatteryInfo.BatteryState;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.ITestSummaryListener;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.TestSummary;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.QuotationAwareTokenizer;
import com.android.tradefed.util.StreamUtil;

import com.google.common.primitives.Ints;

import org.json.JSONException;

import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.RejectedExecutionHandler;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * A {@link ICommandScheduler} to support TFC (Tradefed Cluster). This scheduler runs commands from
 * TFC command-queue and uploads invocation events to TFC command-event-queue.
 */
public class ClusterCommandScheduler extends CommandScheduler {

    /** The {@link ScheduledThreadPoolExecutor} used to manage heartbeats. */
    private ScheduledThreadPoolExecutor mHeartbeatThreadPool = null;

    /** The {@link IClusterOptions} instance used to store cluster-related settings. */
    private IClusterOptions mClusterOptions;

    /** The {@link IClusterClient} instance used to interact with the TFC backend. */
    private IClusterClient mClusterClient;

    /**
     * A {@link ThreadFactory} which returns threads in a dedicated heartbeat group.
     *
     * <p>This class is used as a factory by {@code mHeartbeatThreadPool} in order to segregate
     * heartbeat threads from other "stray" threads to avoid tripping loose thread detection in
     * {@link CommandScheduler}.
     */
    private static class HeartbeatThreadFactory implements ThreadFactory {
        private static final ThreadGroup HB_GROUP;

        static {
            // fetch root thread group as this class may be initialized by an invocation thread
            ThreadGroup tg = Thread.currentThread().getThreadGroup();
            while (tg.getParent() != null) {
                tg = tg.getParent();
            }
            HB_GROUP = new ThreadGroup(tg, "ClusterCommandScheduler.heartbeat");
        }

        @Override
        public Thread newThread(Runnable r) {
            Thread thread = new Thread(HB_GROUP, r);
            // heartbeat should always get cancelled, but ensure it doesn't prevent JVM exit
            thread.setDaemon(true);
            return thread;
        }
    }

    /** {@inheritDoc} */
    @Override
    public void start() {
        UploadHostEventWithState(HostState.RUNNING);
        super.start();
    }

    /** {@inheritDoc} */
    @Override
    public void shutdown() {
        UploadHostEventWithState(HostState.QUITTING);
        getHeartbeatThreadPool().shutdown();
        super.shutdown();
    }

    @Override
    public synchronized void shutdownHard() {
        UploadHostEventWithState(HostState.KILLING);
        getHeartbeatThreadPool().shutdown();
        super.shutdownHard();
    }

    /**
     * A {@link com.android.tradefed.command.ICommandScheduler.IScheduledInvocationListener} to
     * upload events to TFC.
     */
    class InvocationEventHandler extends CollectingTestListener
            implements IScheduledInvocationListener, ITestSummaryListener {

        private ScheduledFuture<?> mHeartbeat;
        private final ClusterCommand mCommandTask;
        private Set<String> mDeviceSerials = new HashSet<>();
        private String mSummary;
        private Set<String> processedSummaries = new HashSet<>();
        private String mError;
        private File mWorkDir;
        private InvocationStatus mInvocationStatus;

        /**
         * Creates a {@link InvocationEventHandler} to track the given {@link ClusterCommand}.
         *
         * @param commandTask the {@link ClusterCommand} to track.
         */
        public InvocationEventHandler(ClusterCommand commandTask) {
            mCommandTask = commandTask;
        }

        /**
         * Sets a work directory for an invocation.
         *
         * @param dir a work directory.
         */
        public void setWorkDir(File dir) {
            mWorkDir = dir;
        }

        private ClusterCommandEvent.Builder createEventBuilder() {
            final ClusterCommandEvent.Builder builder =
                    ClusterCommandEvent.createEventBuilder(mCommandTask)
                            .setHostName(ClusterHostUtil.getHostName());
            if (!mDeviceSerials.isEmpty()) {
                builder.setDeviceSerials(mDeviceSerials);
            }
            return builder;
        }

        private void updateInvocationStatus() {
            if (!getClusterOptions().shouldUploadInvocationStatus()) {
                return;
            }
            final InvocationStatus obj = new InvocationStatus();
            final Collection<TestRunResult> testRunResults = this.getMergedTestRunResults();
            for (final TestRunResult result : testRunResults) {
                final TestGroupStatus testGroupStatus =
                        new TestGroupStatus(
                                result.getName(),
                                result.getNumTests(),
                                result.getNumCompleteTests(),
                                result.getNumAllFailedTests(),
                                result.getNumTestsInState(TestStatus.PASSED),
                                result.isRunComplete(),
                                result.getElapsedTime());
                obj.addTestGroupStatus(testGroupStatus);
            }
            mInvocationStatus = obj;
        }

        /** {@inheritDoc} */
        @Override
        public void invocationInitiated(IInvocationContext context) {
            for (ITestDevice device : context.getDevices()) {
                mDeviceSerials.add(device.getSerialNumber());
            }
            final ClusterCommandEvent event =
                    createEventBuilder()
                            .setType(ClusterCommandEvent.Type.InvocationInitiated)
                            .build();
            getClusterClient().getCommandEventUploader().postEvent(event);
            getClusterClient().getCommandEventUploader().flush();
            mHeartbeat = startHeartbeat();
            // Check that devices are in charging state before starting the invocation.
            for (ITestDevice device : context.getDevices()) {
                try {
                    BatteryState state = BatteryController.getDeviceChargingState(device);
                    if (BatteryState.NOT_CHARGING.equals(state)) {
                        IBatteryInfo info = BatteryController.getBatteryInfoForDevice(device);
                        if (info != null) {
                            info.enableCharging(device);
                        }
                    }
                } catch (DeviceNotAvailableException e) {
                    CLog.e(e);
                }
            }
        }

        /** {@inheritDoc} */
        @Override
        public void invocationStarted(IInvocationContext context) {
            super.invocationStarted(context);
            final ClusterCommandEvent event =
                    createEventBuilder()
                            .setType(ClusterCommandEvent.Type.InvocationStarted)
                            .build();
            getClusterClient().getCommandEventUploader().postEvent(event);
            getClusterClient().getCommandEventUploader().flush();
        }

        @Override
        public void testRunStarted(String name, int numTests) {
            testRunStarted(name, numTests, 0);
        }

        @Override
        public void testRunStarted(String name, int numTests, int attemptNumber) {
            testRunStarted(name, numTests, attemptNumber, System.currentTimeMillis());
        }

        /** {@inheritDoc} */
        @Override
        public void testRunStarted(String name, int numTests, int attemptNumber, long startTime) {
            super.testRunStarted(name, numTests, attemptNumber, startTime);
            updateInvocationStatus();
        }

        /** {@inheritDoc} */
        @Override
        public void invocationFailed(Throwable cause) {
            super.invocationFailed(cause);

            mError = StreamUtil.getStackTrace(cause);
        }

        /** {@inheritDoc} */
        @Override
        public void invocationEnded(long elapsedTime) {
            super.invocationEnded(elapsedTime);

            ClusterCommandEvent event =
                    createEventBuilder()
                            .setType(ClusterCommandEvent.Type.InvocationEnded)
                            .setData(ClusterCommandEvent.DATA_KEY_ERROR, mError)
                            .build();
            getClusterClient().getCommandEventUploader().postEvent(event);
            getClusterClient().getCommandEventUploader().flush();
        }

        /** {@inheritDoc} */
        @Override
        public void invocationComplete(
                IInvocationContext metadata, Map<ITestDevice, FreeDeviceState> devicesStates) {
            if (mWorkDir != null) {
                FileUtil.recursiveDelete(mWorkDir);
            }

            // TODO: handle multi-device where only one of the build could be missing.
            if (getPrimaryBuildInfo() == null && mError == null) {
                mError = "build not found";
            }

            String fetchBuildTimeMillis = "-1";
            String setupTimeMillis = "-1";
            String lostDevice = null;
            if (metadata != null) {
                fetchBuildTimeMillis =
                        metadata.getAttributes()
                                .getUniqueMap()
                                .get(InvocationMetricKey.FETCH_BUILD.toString());
                setupTimeMillis =
                        metadata.getAttributes()
                                .getUniqueMap()
                                .get(InvocationMetricKey.SETUP.toString());
                lostDevice =
                        metadata.getAttributes()
                                .getUniqueMap()
                                .get(InvocationMetricKey.DEVICE_LOST_DETECTED.toString());
            }

            // Stop heartbeat thread before sending InvocationCompleted event.
            if (mHeartbeat != null) {
                mHeartbeat.cancel(true);
            }
            updateInvocationStatus();
            final ClusterCommandEvent.Builder eventBuilder =
                    createEventBuilder()
                            .setType(ClusterCommandEvent.Type.InvocationCompleted)
                            .setInvocationStatus(mInvocationStatus)
                            .setData(ClusterCommandEvent.DATA_KEY_ERROR, mError)
                            .setData(ClusterCommandEvent.DATA_KEY_SUMMARY, mSummary)
                            .setData(
                                    ClusterCommandEvent.DATA_KEY_FETCH_BUILD_TIME_MILLIS,
                                    fetchBuildTimeMillis)
                            .setData(
                                    ClusterCommandEvent.DATA_KEY_SETUP_TIME_MILLIS, setupTimeMillis)
                            .setData(
                                    ClusterCommandEvent.DATA_KEY_TOTAL_TEST_COUNT,
                                    Integer.toString(getNumTotalTests()))
                            .setData(
                                    ClusterCommandEvent.DATA_KEY_FAILED_TEST_COUNT,
                                    Integer.toString(getNumAllFailedTests()))
                            .setData(
                                    ClusterCommandEvent.DATA_KEY_PASSED_TEST_COUNT,
                                    Integer.toString(getNumTestsInState(TestStatus.PASSED)))
                            .setData(
                                    ClusterCommandEvent.DATA_KEY_FAILED_TEST_RUN_COUNT,
                                    Integer.toString(getNumAllFailedTestRuns()));
            if (lostDevice != null) {
                eventBuilder.setData(ClusterCommandEvent.DATA_KEY_LOST_DEVICE_DETECTED, lostDevice);
            }
            final ClusterCommandEvent event = eventBuilder.build();
            getClusterClient().getCommandEventUploader().postEvent(event);
            getClusterClient().getCommandEventUploader().flush();
        }

        /** {@inheritDoc} */
        @Override
        public void putEarlySummary(List<TestSummary> summaries) {
            if (getClusterOptions().shouldCollectEarlyTestSummary()) {
                putSummary(summaries);
            }
        }

        /** {@inheritDoc} */
        @Override
        public void putSummary(List<TestSummary> summaries) {
            StringBuilder sb = new StringBuilder();
            for (final TestSummary summary : summaries) {
                String summaryString = summary.getSummary().toString();
                if (!processedSummaries.contains(summaryString)) {
                    processedSummaries.add(summaryString);
                    sb.append(summaryString);
                    sb.append("\n");
                }
            }
            mSummary = mSummary == null ? sb.toString() : mSummary + sb.toString();
        }

        private ScheduledFuture<?> startHeartbeat() {
            return getHeartbeatThreadPool()
                    .scheduleAtFixedRate(
                            new HeartbeatSender(),
                            0,
                            getClusterOptions().getInvocationHeartbeatInterval(),
                            TimeUnit.MILLISECONDS);
        }

        class HeartbeatSender implements Runnable {
            @Override
            public void run() {
                try {
                    // check cluster command's status
                    if (getClusterOptions().checkCommandState()) {
                        ClusterCommand.State status =
                                getClusterClient()
                                        .getCommandState(
                                                mCommandTask.getRequestId(),
                                                mCommandTask.getCommandId());
                        if (ClusterCommand.State.CANCELED.equals(status)) {
                            // TODO: retrieve cancel reason from TFC.
                            String cause =
                                    String.format(
                                            "The cluster client %s has marked command "
                                                    + "(requestId=%s, commandId=%s) canceled",
                                            getClusterClient().getClass().getSimpleName(),
                                            mCommandTask.getRequestId(),
                                            mCommandTask.getCommandId());
                            CLog.w("Stop invocation due to: %s", cause);
                            Optional.ofNullable(getInvocationContext())
                                    .map(IInvocationContext::getInvocationId)
                                    .map(Ints::tryParse)
                                    .ifPresent(invocationId -> stopInvocation(invocationId, cause));
                        }
                    }

                    final ClusterCommandEvent event =
                            createEventBuilder()
                                    .setType(ClusterCommandEvent.Type.TestRunInProgress)
                                    .setInvocationStatus(mInvocationStatus)
                                    .build();
                    getClusterClient().getCommandEventUploader().postEvent(event);
                } catch (Exception e) {
                    CLog.e("Error sending heartbeat to TFC:");
                    CLog.e(e);
                }
            }
        }
    }

    synchronized ScheduledThreadPoolExecutor getHeartbeatThreadPool() {
        if (mHeartbeatThreadPool == null) {
            mHeartbeatThreadPool = new ScheduledThreadPoolExecutor(1, new HeartbeatThreadFactory());
            // instead of throwing some exception on shutdown we simply log it.
            mHeartbeatThreadPool.setRejectedExecutionHandler(
                    new RejectedExecutionHandler() {
                        @Override
                        public void rejectedExecution(Runnable r, ThreadPoolExecutor e) {
                            CLog.w(
                                    "Rejecting Task %s rejected from executor %s",
                                    r.toString(), e.toString());
                        }
                    });
            // continue existing heartbeats after shutdown (until invocation is complete)
            mHeartbeatThreadPool.setContinueExistingPeriodicTasksAfterShutdownPolicy(true);
        }
        return mHeartbeatThreadPool;
    }

    /** {@inheritDoc} */
    @Override
    protected void processReadyCommands(IDeviceManager manager) {
        super.processReadyCommands(manager);

        if (isShuttingDown()) {
            return;
        }

        List<ClusterCommand> commands = null;
        MultiMap<String, DeviceDescriptor> devices = getAvailableDevices(manager);
        if (devices.isEmpty()) {
            CLog.d("No devices are available for testing.");
            return;
        }
        // Lease command tasks through the leasehosttasks API.
        // Here we get all devices (available or not), so TFC will analyze the device tree to
        // decide which group is allocated and which group is available.
        devices = getDevices(manager, false);
        commands = fetchHostCommands(devices);
        if (commands.isEmpty()) {
            CLog.d("No commands available for testing.");
            return;
        }
        execCommands(commands);
    }

    /**
     * Returns a map containing available devices grouped by their types.
     *
     * @param manager a {@link IDeviceManager}.
     * @return a {@link MultiMap} of String to DeviceDescriptor containing available devices.
     */
    MultiMap<String, DeviceDescriptor> getAvailableDevices(IDeviceManager manager) {
        return getDevices(manager, true);
    }

    /**
     * Returns a map containing devices grouped by their types.
     *
     * @param manager a {@link IDeviceManager}.
     * @param availableOnly only return available devices or all devices.
     * @return a {@link MultiMap} of String to DeviceDescriptor containing available devices.
     */
    MultiMap<String, DeviceDescriptor> getDevices(IDeviceManager manager, boolean availableOnly) {
        // Getting available device types
        final MultiMap<String, DeviceDescriptor> devices = new MultiMap<>();
        for (final DeviceDescriptor device : manager.listAllDevices()) {
            if (availableOnly && device.getState() != DeviceAllocationState.Available) {
                continue;
            }
            if (ClusterHostUtil.isIpPort(device.getSerial())) {
                // Note(b/28802876): Skipping IP:PORT serials from cluster scheduling because they
                // behave differently from physical devices and are not fully supported by TF.
                continue;
            }
            String runTargetFormat = getClusterOptions().getRunTargetFormat();
            String runTarget =
                    ClusterHostUtil.getRunTarget(
                            device, runTargetFormat, getClusterOptions().getDeviceTag());
            CLog.d("%s is available", runTarget);
            devices.put(runTarget, device);
        }
        return devices;
    }

    /**
     * Get available flashing permits.
     *
     * @return the number of available flashing permits.
     */
    private int getAvailableFlashingPermits() {
        // By default there is no limit on available flashing permits.
        int availableFlashingPermits = Integer.MAX_VALUE;
        final IClusterOptions options = getClusterOptions();

        boolean checkFlashingPermitsLease = options.checkFlashingPermitsOnLease();
        if (checkFlashingPermitsLease) {
            availableFlashingPermits = getDeviceManager().getAvailableFlashingPermits();
            CLog.i("available flasher permits %d", availableFlashingPermits);
        }
        return availableFlashingPermits;
    }

    /**
     * Fetches commands for devices from the Tradefed Cluster's leasehosttasks API.
     *
     * @param devices a {@link MultiMap} of String to DeviceDescriptor containing devices.
     * @return a list of {@link ClusterCommand}s.
     */
    List<ClusterCommand> fetchHostCommands(final MultiMap<String, DeviceDescriptor> devices) {
        CLog.d("fetching cluster host commands from leasehosttasks...");
        int availableFlashingPermits = getAvailableFlashingPermits();

        // Don't try to lease if there are no flasher permits available
        if (availableFlashingPermits == 0) {
            CLog.i("There is no available flashing permits. Not lease any additional commands.");
            return Collections.<ClusterCommand>emptyList();
        }

        final IClusterOptions options = getClusterOptions();
        final MultiMap<String, String> deviceGroups = options.getDeviceGroup();
        final Map<String, String> deviceToGroup = new HashMap<>();
        for (String group : deviceGroups.keySet()) {
            for (String deviceSerial : deviceGroups.get(group)) {
                deviceToGroup.put(deviceSerial, group);
            }
        }
        List<ClusterDeviceInfo> deviceInfos = new LinkedList<>();
        for (String runTarget : devices.keySet()) {
            for (DeviceDescriptor d : devices.get(runTarget)) {
                String groupName = deviceToGroup.getOrDefault(d.getSerial(), null);
                ClusterDeviceInfo deviceInfo =
                        new ClusterDeviceInfo.Builder()
                                .setDeviceDescriptor(d)
                                .setRunTarget(runTarget)
                                .setGroupName(groupName)
                                .build();
                deviceInfos.add(deviceInfo);
            }
        }
        try {
            int count = Math.min(deviceInfos.size(), availableFlashingPermits);
            List<ClusterCommand> commands =
                    getClusterClient()
                            .leaseHostCommands(
                                    options.getClusterId(),
                                    ClusterHostUtil.getHostName(),
                                    deviceInfos,
                                    options.getNextClusterIds(),
                                    count);
            return commands;
        } catch (JSONException e) {
            CLog.e(e);
            return Collections.<ClusterCommand>emptyList();
        }
    }

    /**
     * Executes commands fetched from the cluster command queue.
     *
     * @param commands a list of {@link ClusterCommand}s fetched from the cluster command queue.
     */
    void execCommands(final List<ClusterCommand> commands) {
        for (final ClusterCommand commandTask : commands) {
            try {
                final InvocationEventHandler handler = new InvocationEventHandler(commandTask);
                switch (commandTask.getRequestType()) {
                    case UNMANAGED:
                        execClusterCommand(commandTask, handler);
                        break;
                    case MANAGED:
                        execManagedClusterCommand(commandTask, handler);
                        break;
                    default:
                        throw new UnsupportedOperationException();
                }
            } catch (NoDeviceException e) {
                CLog.w(
                        "no device meets requirements for cluster command [%s]; returning...",
                        commandTask.getTaskId());
                CLog.w(e);
                IClusterEventUploader<ClusterCommandEvent> eventUploader =
                        getClusterClient().getCommandEventUploader();
                eventUploader.postEvent(
                        ClusterCommandEvent.createEventBuilder(commandTask)
                                .setHostName(ClusterHostUtil.getHostName())
                                .setType(ClusterCommandEvent.Type.AllocationFailed)
                                .build());
                eventUploader.flush();
            } catch (ConfigurationException | IOException | JSONException e) {
                CLog.w("failed to execute cluster command [%s]: %s", commandTask.getTaskId(), e);
                CLog.w(e);
                IClusterEventUploader<ClusterCommandEvent> eventUploader =
                        getClusterClient().getCommandEventUploader();
                eventUploader.postEvent(
                        ClusterCommandEvent.createEventBuilder(commandTask)
                                .setHostName(ClusterHostUtil.getHostName())
                                .setType(ClusterCommandEvent.Type.ConfigurationError)
                                .setData(ClusterCommandEvent.DATA_KEY_ERROR, e.toString())
                                .build());
                eventUploader.flush();
            }
        }
    }

    void execClusterCommand(ClusterCommand commandTask, InvocationEventHandler handler)
            throws ConfigurationException, IllegalArgumentException, NoDeviceException {
        String cmdLine = commandTask.getCommandLine();
        String[] args = QuotationAwareTokenizer.tokenizeLine(cmdLine);
        // If it is a dry run command skip execution.
        if (dryRunCommand(handler, args)) {
            return;
        }
        // Append device serials to command.
        // By assigning all applicable serials, TF will try one by one until allocation
        // succeeds (or fails for all). This mitigates the issue where a single bad
        // device can starve tests.
        if (commandTask.getTargetDeviceSerials() != null) {
            for (String serial : commandTask.getTargetDeviceSerials()) {
                cmdLine += " --serial ";
                cmdLine += serial;
            }
        }
        CLog.i("executing cluster command: [%s] %s", commandTask.getTaskId(), cmdLine);
        execCommand(handler, QuotationAwareTokenizer.tokenizeLine(cmdLine));
    }

    void execManagedClusterCommand(ClusterCommand commandTask, InvocationEventHandler handler)
            throws IOException, JSONException, ConfigurationException, NoDeviceException {
        File workDir = null;
        try {
            workDir = new File(System.getProperty("java.io.tmpdir"), commandTask.getAttemptId());
            workDir.mkdirs();
            final String requestId = commandTask.getRequestId();
            final String commandId = commandTask.getCommandId();
            final IClusterClient client = getClusterClient();
            final TestEnvironment testEnvironment = client.getTestEnvironment(requestId);
            final List<TestResource> testResources = client.getTestResources(requestId);
            final TestContext testContext = client.getTestContext(requestId, commandId);
            testResources.addAll(testContext.getTestResources());
            final File configFile =
                    new ClusterCommandConfigBuilder()
                            .setWorkDir(workDir)
                            .setClusterCommand(commandTask)
                            .setTestEnvironment(testEnvironment)
                            .setTestResources(testResources)
                            .setTestContext(testContext)
                            .build();
            CLog.i("executing cluster command: [%s] %s", commandTask.getTaskId(), configFile);
            CLog.d("configFile: %s", FileUtil.readStringFromFile(configFile));
            // FIXME: Find a way to upload a config file after an invocation is completed for
            // debugging.
            handler.setWorkDir(workDir);
            execCommand(handler, new String[] {configFile.getAbsolutePath()});
            // Unset workDir to avoid being cleaned up
            workDir = null;
        } finally {
            if (workDir != null) {
                FileUtil.recursiveDelete(workDir);
            }
        }
    }

    /**
     * Determines if a given command is a dry-run. If the command is a dry-run, validate it. If
     * there are any configs issue, it will throw a ConfigurationException.
     *
     * @param handler {@link InvocationEventHandler} to report events for dry-run validation.
     * @param args the command to validate.
     * @return true if the command are a dry run, false otherwise.
     * @throws ConfigurationException
     */
    protected boolean dryRunCommand(final InvocationEventHandler handler, String[] args)
            throws ConfigurationException {
        IConfiguration config =
                getConfigFactory().createConfigurationFromArgs(args, null, getKeyStoreClient());
        if (config.getCommandOptions().isDryRunMode()) {
            IInvocationContext context = new InvocationContext();
            context.addDeviceBuildInfo("stub", new BuildInfo());
            handler.invocationStarted(context);
            config.validateOptions();
            handler.invocationEnded(0);
            IInvocationContext nullMeta = null;
            handler.invocationComplete(nullMeta, null);
            return true;
        }
        return false;
    }

    /** Get the {@link IClusterOptions} instance used to store cluster-related settings. */
    IClusterOptions getClusterOptions() {
        if (mClusterOptions == null) {
            mClusterOptions = ClusterHostUtil.getClusterOptions();
        }
        return mClusterOptions;
    }

    /** Get the {@link IClusterClient} instance used to interact with the TFC backend. */
    IClusterClient getClusterClient() {
        if (mClusterClient == null) {
            mClusterClient = ClusterHostUtil.getClusterClient();
        }
        return mClusterClient;
    }

    /** Event triggered, to upload host states */
    private void UploadHostEventWithState(HostState state) {
        try {
            IClusterEventUploader<ClusterHostEvent> Uploader =
                    getClusterClient().getHostEventUploader();
            ClusterHostEvent.Builder builder =
                    new ClusterHostEvent.Builder()
                            .setClusterId(getClusterOptions().getClusterId())
                            .setHostEventType(HostEventType.HostStateChanged)
                            .setHostName(ClusterHostUtil.getHostName())
                            .setHostState(state);
            CLog.d("event uploading with state %s", state.toString());
            ClusterHostEvent event = builder.build();
            Uploader.postEvent(event);
            CLog.d("event %s uploaded with state %s", event.toString(), state.toString());
            Uploader.flush();
        } catch (RuntimeException e) {
            CLog.e("failed to upload host state %s to TFC: %s", state.toString(), e);
        }
    }
}
