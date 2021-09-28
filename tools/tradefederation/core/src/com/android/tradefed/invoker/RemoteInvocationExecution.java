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
package com.android.tradefed.invoker;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.StubBuildProvider;
import com.android.tradefed.clearcut.ClearcutClient;
import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.command.CommandRunner;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceSelectionOptions;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.cloud.GceAvdInfo;
import com.android.tradefed.device.cloud.GceManager;
import com.android.tradefed.device.cloud.ManagedRemoteDevice;
import com.android.tradefed.device.cloud.RemoteFileUtil;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.proto.FileProtoResultReporter;
import com.android.tradefed.result.proto.ProtoResultParser;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.testtype.SubprocessTfLauncher;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.SystemUtil;
import com.android.tradefed.util.TimeUtil;
import com.android.tradefed.util.proto.TestRecordProtoUtil;

import com.google.common.base.Strings;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** Implementation of {@link InvocationExecution} that drives a remote execution. */
public class RemoteInvocationExecution extends InvocationExecution {

    public static final long PUSH_TF_TIMEOUT = 150000L;
    public static final long PULL_RESULT_TIMEOUT = 180000L;
    public static final long REMOTE_PROCESS_RUNNING_WAIT = 15000L;
    public static final long LAUNCH_EXTRA_DEVICE = 15 * 60 * 1000L;
    public static final long SETUP_REMOTE_DIR_TIMEOUT = 10 * 60 * 1000L;
    public static final long NEW_USER_TIMEOUT = 5 * 60 * 1000L;
    public static final long JOIN_CLEAN_TIMEOUT_MS = 2 * 60 * 1000L;

    public static final String REMOTE_USER_DIR = "/home/{$USER}/";
    public static final String PROTO_RESULT_NAME = "output.pb";
    public static final String STDOUT_FILE = "screen-VM_tradefed-stdout.txt";
    public static final String STDERR_FILE = "screen-VM_tradefed-stderr.txt";
    public static final String REMOTE_CONFIG = "configuration";
    public static final String GLOBAL_REMOTE_CONFIG = "global-remote-configuration";

    private static final int MAX_CONNECTION_REFUSED_COUNT = 3;
    private static final int MAX_PUSH_TF_ATTEMPTS = 3;
    private static final String TRADEFED_EARLY_TERMINATION =
            "Remote Tradefed might have terminated early.\nRemote Stderr:\n%s";

    private String mRemoteTradefedDir = null;
    private String mRemoteAdbPath = null;
    private ProtoResultParser mProtoParser = null;
    private String mRemoteConsoleStdErr = null;

    @Override
    public boolean fetchBuild(
            TestInformation testInfo,
            IConfiguration config,
            IRescheduler rescheduler,
            ITestInvocationListener listener)
            throws DeviceNotAvailableException, BuildRetrievalError {
        // TODO: handle multiple devices/build config
        StubBuildProvider stubProvider = new StubBuildProvider();

        String deviceName = config.getDeviceConfig().get(0).getDeviceName();
        OptionCopier.copyOptionsNoThrow(
                config.getDeviceConfig().get(0).getBuildProvider(), stubProvider);

        IBuildInfo info = stubProvider.getBuild();
        if (info == null) {
            return false;
        }
        testInfo.getContext().addDeviceBuildInfo(deviceName, info);
        updateBuild(info, config);
        return true;
    }

    @Override
    protected void customizeDevicePreInvocation(IConfiguration config, IInvocationContext context) {
        super.customizeDevicePreInvocation(config, context);

        if (config.getCommandOptions().getShardCount() != null
                && config.getCommandOptions().getShardIndex() == null) {
            ITestDevice device = context.getDevices().get(0);
            TestDeviceOptions options = device.getOptions();
            // Trigger the multi-tenant start in the VM
            options.addGceDriverParams("--num-avds-per-instance");
            String count = config.getCommandOptions().getShardCount().toString();
            options.addGceDriverParams(count);
            InvocationMetricLogger.addInvocationMetrics(
                    InvocationMetricKey.CF_INSTANCE_COUNT, count);
        }
    }

    @Override
    public void runTests(
            TestInformation info, IConfiguration config, ITestInvocationListener listener)
            throws Throwable {
        ManagedRemoteDevice device = (ManagedRemoteDevice) info.getDevice();
        GceAvdInfo gceInfo = device.getRemoteAvdInfo();

        // Run remote TF (new tests?)
        IRunUtil runUtil = new RunUtil();

        TestDeviceOptions options = device.getOptions();
        String mainRemoteDir = getRemoteMainDir(options);
        mRemoteAdbPath = String.format("/home/%s/bin/adb", options.getInstanceUser());
        // Select the TF version that should be pushed to the remote VM
        File tfToPush = getLocalTradefedPath(listener, options.getRemoteTf());
        if (tfToPush == null) {
            return;
        }

        mRemoteTradefedDir = mainRemoteDir + "tradefed/";
        CommandResult createRemoteDir =
                GceManager.remoteSshCommandExecution(
                        gceInfo, options, runUtil, 120000L, "mkdir", "-p", mRemoteTradefedDir);
        if (!CommandStatus.SUCCESS.equals(createRemoteDir.getStatus())) {
            listener.invocationFailed(
                    createInvocationFailure(
                            "Failed to create remote dir.", FailureStatus.INFRA_FAILURE));
            return;
        }

        // Push Tradefed to the remote
        int attempt = 0;
        boolean result = false;
        while (!result && attempt < MAX_PUSH_TF_ATTEMPTS) {
            result =
                    RemoteFileUtil.pushFileToRemote(
                            gceInfo,
                            options,
                            Arrays.asList("-r"),
                            runUtil,
                            PUSH_TF_TIMEOUT,
                            mRemoteTradefedDir,
                            tfToPush);
            attempt++;
        }
        if (!result) {
            CLog.e("Failed to push Tradefed.");
            listener.invocationFailed(
                    createInvocationFailure(
                            "Failed to push Tradefed.", FailureStatus.INFRA_FAILURE));
            return;
        }

        mRemoteTradefedDir = mRemoteTradefedDir + tfToPush.getName() + "/";
        CommandResult listRemoteDir =
                GceManager.remoteSshCommandExecution(
                        gceInfo, options, runUtil, 120000L, "ls", "-l", mRemoteTradefedDir);
        CLog.d("stdout: %s", listRemoteDir.getStdout());
        CLog.d("stderr: %s", listRemoteDir.getStderr());

        File configFile = createRemoteConfig(config, listener, mRemoteTradefedDir);
        File globalConfig = null;
        try {
            CLog.d("Pushing Tradefed XML configuration to remote.");
            boolean resultPush =
                    RemoteFileUtil.pushFileToRemote(
                            gceInfo,
                            options,
                            null,
                            runUtil,
                            PUSH_TF_TIMEOUT,
                            mRemoteTradefedDir,
                            configFile);
            if (!resultPush) {
                CLog.e("Failed to push Tradefed Configuration.");
                listener.invocationFailed(
                        createInvocationFailure(
                                "Failed to push Tradefed Configuration.",
                                FailureStatus.INFRA_FAILURE));
                return;
            }

            String[] whitelistConfigs =
                    new String[] {
                        GlobalConfiguration.SANDBOX_FACTORY_TYPE_NAME,
                        GlobalConfiguration.HOST_OPTIONS_TYPE_NAME,
                        "android-build"
                    };
            try {
                globalConfig =
                        GlobalConfiguration.getInstance()
                                .cloneConfigWithFilter(new HashSet<>(), whitelistConfigs);
            } catch (IOException e) {
                listener.invocationFailed(createInvocationFailure(e, FailureStatus.INFRA_FAILURE));
                return;
            }
            try (InputStreamSource source = new FileInputStreamSource(globalConfig)) {
                listener.testLog(GLOBAL_REMOTE_CONFIG, LogDataType.XML, source);
            }
            // Push the global configuration
            boolean resultPushGlobal =
                    RemoteFileUtil.pushFileToRemote(
                            gceInfo,
                            options,
                            null,
                            runUtil,
                            PUSH_TF_TIMEOUT,
                            mRemoteTradefedDir,
                            globalConfig);
            if (!resultPushGlobal) {
                CLog.e("Failed to push Tradefed Global Configuration.");
                listener.invocationFailed(
                        createInvocationFailure(
                                "Failed to push Tradefed Global Configuration.",
                                FailureStatus.INFRA_FAILURE));
                return;
            }

            resetAdb(gceInfo, options, runUtil);
            runRemote(
                    listener,
                    info.getContext(),
                    configFile,
                    gceInfo,
                    options,
                    runUtil,
                    config,
                    globalConfig);
            collectAdbLogs(gceInfo, options, runUtil, listener);
        } finally {
            FileUtil.recursiveDelete(configFile);
            FileUtil.recursiveDelete(globalConfig);
        }
    }

    @Override
    public void doSetup(TestInformation testInfo, IConfiguration config, ITestLogger logger)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        // Skip
    }

    @Override
    public void doTeardown(
            TestInformation testInfo,
            IConfiguration config,
            ITestLogger logger,
            Throwable exception)
            throws Throwable {
            super.runDevicePostInvocationTearDown(testInfo.getContext(), config, exception);
    }

    @Override
    public void doCleanUp(IInvocationContext context, IConfiguration config, Throwable exception) {
        // Skip
    }

    @Override
    protected String getAdbVersion() {
        // Do not report the adb version from the parent, the remote child will remote its own.
        return null;
    }

    private void runRemote(
            ITestInvocationListener currentInvocationListener,
            IInvocationContext context,
            File configFile,
            GceAvdInfo info,
            TestDeviceOptions options,
            IRunUtil runUtil,
            IConfiguration config,
            File globalConfig)
            throws InvalidProtocolBufferException, IOException {
        List<String> remoteTfCommand = new ArrayList<>();
        remoteTfCommand.add("pushd");
        remoteTfCommand.add(mRemoteTradefedDir + ";");
        remoteTfCommand.add(String.format("PATH=%s:$PATH", new File(mRemoteAdbPath).getParent()));
        remoteTfCommand.add("screen -dmSU tradefed sh -c");

        StringBuilder tfCmdBuilder =
                new StringBuilder("TF_GLOBAL_CONFIG=" + globalConfig.getName());
        // Set an env variable to notify that this a remote environment.
        tfCmdBuilder.append(" " + SystemUtil.REMOTE_VM_VARIABLE + "=1");
        // Disable clearcut in the remote
        tfCmdBuilder.append(" " + ClearcutClient.DISABLE_CLEARCUT_KEY + "=1");
        tfCmdBuilder.append(" ENTRY_CLASS=" + CommandRunner.class.getCanonicalName());
        tfCmdBuilder.append(" ./tradefed.sh " + mRemoteTradefedDir + configFile.getName());
        if (config.getCommandOptions().shouldUseRemoteSandboxMode()) {
            tfCmdBuilder.append(" --" + CommandOptions.USE_SANDBOX);
        }
        tfCmdBuilder.append(" > " + STDOUT_FILE + " 2> " + STDERR_FILE);
        remoteTfCommand.add("\"" + tfCmdBuilder.toString() + "\"");
        // Kick off the actual remote run
        CommandResult resultRemoteExecution =
                GceManager.remoteSshCommandExecution(
                        info, options, runUtil, 0L, remoteTfCommand.toArray(new String[0]));
        if (!CommandStatus.SUCCESS.equals(resultRemoteExecution.getStatus())) {
            CLog.e("Error running the remote command: %s", resultRemoteExecution.getStdout());
            currentInvocationListener.invocationFailed(
                    createInvocationFailure(
                            resultRemoteExecution.getStderr(), FailureStatus.INFRA_FAILURE));
            return;
        }
        // Sleep a bit to let the process start
        RunUtil.getDefault().sleep(10000L);

        mProtoParser = new ProtoResultParser(currentInvocationListener, context, false, "remote-");
        // Print when parsing
        mProtoParser.setQuiet(false);
        // Monitor the remote invocation to ensure it's completing. Block until timeout or stops
        // running.
        boolean stillRunning = true;
        try {
            stillRunning =
                    isStillRunning(
                            currentInvocationListener, configFile, info, options, runUtil, config);
        } finally {
            // Fetch the logs for debugging
            File stdout =
                    fetchRemoteAndLogFile(
                            currentInvocationListener,
                            STDOUT_FILE,
                            STDOUT_FILE,
                            info,
                            options,
                            runUtil);
            FileUtil.recursiveDelete(stdout);
            File stderr =
                    fetchRemoteAndLogFile(
                            currentInvocationListener,
                            STDERR_FILE,
                            STDERR_FILE,
                            info,
                            options,
                            runUtil);
            if (stderr != null && stderr.exists()) {
                mRemoteConsoleStdErr = FileUtil.readStringFromFile(stderr);
                FileUtil.recursiveDelete(stderr);
            } else {
                mRemoteConsoleStdErr = "Failed to fetch stderr from remote.";
            }
        }

        // If not result in progress are reported, parse the full results at the end.
        if (!config.getCommandOptions().shouldReportModuleProgression()) {
            fetchAndProcessResults(
                    stillRunning,
                    currentInvocationListener,
                    info,
                    options,
                    runUtil,
                    mRemoteTradefedDir);
        } else {
            if (!mProtoParser.invocationEndedReached()) {
                String message =
                        String.format(
                                "Parsing of results protos might be incomplete: invocation ended "
                                        + "of remote execution was not found. "
                                        + TRADEFED_EARLY_TERMINATION,
                                mRemoteConsoleStdErr);
                currentInvocationListener.invocationFailed(
                        createInvocationFailure(message, FailureStatus.INFRA_FAILURE));
            }
        }
    }

    private boolean isStillRunning(
            ITestInvocationListener currentInvocationListener,
            File configFile,
            GceAvdInfo info,
            TestDeviceOptions options,
            IRunUtil runUtil,
            IConfiguration config)
            throws IOException {
        long maxTimeout = config.getCommandOptions().getInvocationTimeout();
        Long endTime = null;
        if (maxTimeout > 0L) {
            endTime = System.currentTimeMillis() + maxTimeout;
        }
        boolean stillRunning = true;
        int errorConnectCount = 0;
        int currentIndex = 0;
        long currentTimeOnProto = 0L;
        while (stillRunning) {
            if (config.getCommandOptions().shouldReportModuleProgression()) {
                File resultFile =
                        RemoteFileUtil.fetchRemoteFile(
                                info,
                                options,
                                runUtil,
                                PULL_RESULT_TIMEOUT,
                                mRemoteTradefedDir + PROTO_RESULT_NAME + currentIndex);
                if (resultFile != null) {
                    currentIndex++;
                    currentTimeOnProto = System.currentTimeMillis();
                    try {
                        mProtoParser.processFileProto(resultFile);
                    } finally {
                        FileUtil.deleteFile(resultFile);
                    }
                    // Don't sleep in that case since we might have more file to process, this will
                    // sleep next time we don't find a file to process on the remote.
                    continue;
                }
            }
            if (System.currentTimeMillis() - currentTimeOnProto > 7200000) { // 2 hours
                // If we are stuck on waiting the same proto for over 2 hours, collect some logs
                File stdout =
                        fetchRemoteAndLogFile(
                                currentInvocationListener,
                                STDOUT_FILE,
                                STDOUT_FILE + "-early",
                                info,
                                options,
                                runUtil);
                FileUtil.recursiveDelete(stdout);
                currentTimeOnProto = System.currentTimeMillis();
            }

            CommandResult psRes =
                    GceManager.remoteSshCommandExecution(
                            info,
                            options,
                            runUtil,
                            120000L,
                            "ps",
                            "-ef",
                            "| grep",
                            CommandRunner.class.getCanonicalName());
            if (!CommandStatus.SUCCESS.equals(psRes.getStatus())) {
                errorConnectCount++;
                // If we get several connection errors in a row, give up.
                if (errorConnectCount > MAX_CONNECTION_REFUSED_COUNT) {
                    CLog.e("Failed to connect to the remote to check running status.");
                    return false;
                }
            } else {
                // Reset the error count
                errorConnectCount = 0;
                CLog.d("ps -ef: stdout: %s\nstderr: %s\n", psRes.getStdout(), psRes.getStderr());
                stillRunning = psRes.getStdout().contains(configFile.getName());
                CLog.d("still running: %s", stillRunning);
                if (endTime != null && System.currentTimeMillis() > endTime) {
                    currentInvocationListener.invocationFailed(
                            createInvocationFailure(
                                    String.format(
                                            "Remote invocation timeout after %s",
                                            TimeUtil.formatElapsedTime(maxTimeout)),
                                    FailureStatus.TIMED_OUT));
                    break;
                }
            }
            if (stillRunning) {
                RunUtil.getDefault().sleep(REMOTE_PROCESS_RUNNING_WAIT);
            }
        }

        File resultFile = null;
        if (config.getCommandOptions().shouldReportModuleProgression()) {
            // Process all remaining proto files available
            do {
                resultFile =
                        RemoteFileUtil.fetchRemoteFile(
                                info,
                                options,
                                runUtil,
                                PULL_RESULT_TIMEOUT,
                                mRemoteTradefedDir + PROTO_RESULT_NAME + currentIndex);
                if (resultFile != null) {
                    currentIndex++;
                    try {
                        mProtoParser.processFileProto(resultFile);
                    } finally {
                        FileUtil.deleteFile(resultFile);
                    }
                }
            } while (resultFile != null);
        }
        return stillRunning;
    }

    /** Returns the main remote working directory. */
    private String getRemoteMainDir(TestDeviceOptions options) {
        return REMOTE_USER_DIR.replace("{$USER}", options.getInstanceUser());
    }

    /**
     * Sometimes remote adb version is a bit weird and is not running properly the first time. Try
     * it out once to ensure it starts.
     */
    private void resetAdb(GceAvdInfo info, TestDeviceOptions options, IRunUtil runUtil) {
        CommandResult probAdb =
                GceManager.remoteSshCommandExecution(
                        info, options, runUtil, 120000L, mRemoteAdbPath, "devices");
        CLog.d("remote adb prob: %s", probAdb.getStdout());
        CLog.d("%s", probAdb.getStderr());

        CommandResult versionAdb =
                GceManager.remoteSshCommandExecution(
                        info, options, runUtil, 120000L, mRemoteAdbPath, "version");
        CLog.d("version adb: %s", versionAdb.getStdout());
        CLog.d("%s", versionAdb.getStderr());
    }

    /**
     * Remote invocation relies on the adb of the remote, so always collect its logs to make sure we
     * can debug it appropriately.
     */
    private void collectAdbLogs(
            GceAvdInfo info, TestDeviceOptions options, IRunUtil runUtil, ITestLogger logger) {
        CommandResult tmpDirFolder =
                GceManager.remoteSshCommandExecution(
                        info, options, runUtil, 120000L, "bash -c \"echo \\$TMPDIR\"");
        String folder = tmpDirFolder.getStdout().trim();
        CLog.d("Remote TMPDIR folder is: %s", folder);
        if (Strings.isNullOrEmpty(folder)) {
            // If TMPDIR is not set, default to /tmp/ location.
            folder = "/tmp";
        }
        CommandResult uid =
                GceManager.remoteSshCommandExecution(
                        info, options, new RunUtil(), 120000L, "bash -c \"echo \\$UID\"");
        String uidString = uid.getStdout().trim();
        CLog.d("Remote $UID for adb is: %s", uidString);

        if (Strings.isNullOrEmpty(uidString)) {
            CLog.w("Could not determine adb log path.");
            return;
        }

        GceManager.logNestedRemoteFile(
                logger,
                info,
                options,
                runUtil,
                folder + "/adb." + uidString + ".log",
                LogDataType.TEXT,
                "full_adb.log");
    }

    /**
     * Create the configuration that will run in the remote VM.
     *
     * @param config The main {@link IConfiguration}.
     * @param logger A logger where to save the XML configuration for debugging.
     * @param resultDirPath the remote result dir where results should be saved.
     * @return A file containing the dumped remote XML configuration.
     * @throws IOException
     */
    @VisibleForTesting
    File createRemoteConfig(IConfiguration config, ITestLogger logger, String resultDirPath)
            throws IOException, ConfigurationException {
        // Setup the remote reporting to a proto file
        List<ITestInvocationListener> reporters = new ArrayList<>();
        FileProtoResultReporter protoReporter = new FileProtoResultReporter();
        OptionSetter protoResSetter = new OptionSetter(protoReporter);
        if (config.getCommandOptions().shouldReportModuleProgression()) {
            protoResSetter.setOptionValue(
                    FileProtoResultReporter.PERIODIC_PROTO_WRITING_OPTION, "true");
        }
        protoResSetter.setOptionValue(
                FileProtoResultReporter.PROTO_OUTPUT_FILE,
                new File(resultDirPath + PROTO_RESULT_NAME).getPath());
        reporters.add(protoReporter);

        config.setTestInvocationListeners(reporters);

        for (IDeviceConfiguration deviceConfig : config.getDeviceConfig()) {
            deviceConfig.getDeviceRequirements().setSerial();
            if (deviceConfig.getDeviceRequirements() instanceof DeviceSelectionOptions) {
                ((DeviceSelectionOptions) deviceConfig.getDeviceRequirements())
                        .setDeviceTypeRequested(null);
            }
        }

        if (config.getCommandOptions().getShardCount() != null
                && config.getCommandOptions().getShardIndex() == null) {
            config.getCommandOptions().setReplicateSetup(true);
        }

        // Mark the remote invocation as subprocess
        config.getCommandOptions()
                .getInvocationData()
                .put(SubprocessTfLauncher.SUBPROCESS_TAG_NAME, "true");

        // Unset remote-tf-version to avoid re-downloading from remote VM.
        OptionSetter deviceOptions =
                new OptionSetter(config.getDeviceConfig().get(0).getDeviceOptions());
        deviceOptions.setOptionValue(TestDeviceOptions.REMOTE_TF_VERSION_OPTION, "");

        // Dump and log the configuration
        File configFile = FileUtil.createTempFile(config.getName(), ".xml");
        config.dumpXml(
                new PrintWriter(configFile),
                new ArrayList<String>(),
                /* print deprecated */ true,
                /* print unchanged*/ false);
        try (InputStreamSource source = new FileInputStreamSource(configFile)) {
            logger.testLog(REMOTE_CONFIG, LogDataType.XML, source);
        }
        return configFile;
    }

    /** Returns the Tradefed version that should be pushed to the remote to drive the invocation. */
    private File getLocalTradefedPath(ITestInvocationListener listener, File remoteTf) {
        if (remoteTf != null && remoteTf.exists()) {
            return remoteTf;
        }

        String tfPath = System.getProperty("TF_JAR_DIR");
        if (tfPath == null) {
            listener.invocationFailed(
                    createInvocationFailure(
                            "Failed to find $TF_JAR_DIR.", FailureStatus.INFRA_FAILURE));
            return null;
        }
        File currentTf = new File(tfPath).getAbsoluteFile();
        if (tfPath.equals(".")) {
            currentTf = new File("").getAbsoluteFile();
        }
        return currentTf;
    }

    private void fetchAndProcessResults(
            boolean wasStillRunning,
            ITestInvocationListener invocationListener,
            GceAvdInfo info,
            TestDeviceOptions options,
            IRunUtil runUtil,
            String resultDirPath)
            throws InvalidProtocolBufferException, IOException {
        File resultFile = null;
        if (wasStillRunning) {
            CLog.d("Remote invocation was still running. No result can be pulled.");
            return;
        }
        resultFile =
                RemoteFileUtil.fetchRemoteFile(
                        info,
                        options,
                        runUtil,
                        PULL_RESULT_TIMEOUT,
                        resultDirPath + PROTO_RESULT_NAME);
        if (resultFile == null) {
            invocationListener.invocationFailed(
                    createInvocationFailure(
                            String.format(
                                    "Could not find remote result file at %s. "
                                            + TRADEFED_EARLY_TERMINATION,
                                    resultDirPath + PROTO_RESULT_NAME,
                                    mRemoteConsoleStdErr),
                            FailureStatus.INFRA_FAILURE));
            return;
        }
        CLog.d("Fetched remote result file!");
        // Report result to listener.
        try {
            mProtoParser.processFinalizedProto(TestRecordProtoUtil.readFromFile(resultFile));
        } finally {
            FileUtil.deleteFile(resultFile);
        }
    }

    private File fetchRemoteAndLogFile(
            ITestLogger logger,
            String fileName,
            String logName,
            GceAvdInfo info,
            TestDeviceOptions options,
            IRunUtil runUtil) {
        File file =
                RemoteFileUtil.fetchRemoteFile(
                        info, options, runUtil, PULL_RESULT_TIMEOUT, mRemoteTradefedDir + fileName);
        if (file != null) {
            try (InputStreamSource source = new FileInputStreamSource(file, false)) {
                logger.testLog(logName, LogDataType.TEXT, source);
            }
        }
        return file;
    }

    private FailureDescription createInvocationFailure(String errorMessage, FailureStatus status) {
        FailureDescription failure = FailureDescription.create(errorMessage);
        failure.setFailureStatus(status);
        failure.setCause(new RuntimeException(errorMessage));
        return failure;
    }

    private FailureDescription createInvocationFailure(Exception e, FailureStatus status) {
        FailureDescription failure = FailureDescription.create(e.getMessage());
        failure.setFailureStatus(status);
        failure.setCause(e);
        return failure;
    }
}
