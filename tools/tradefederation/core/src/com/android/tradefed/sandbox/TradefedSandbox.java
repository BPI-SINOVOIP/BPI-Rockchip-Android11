/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.sandbox;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationFactory;
import com.android.tradefed.config.IGlobalConfiguration;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.logger.CurrentInvocation;
import com.android.tradefed.invoker.logger.CurrentInvocation.InvocationInfo;
import com.android.tradefed.invoker.proto.InvocationContext.Context;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.proto.StreamProtoReceiver;
import com.android.tradefed.result.proto.StreamProtoResultReporter;
import com.android.tradefed.sandbox.SandboxConfigDump.DumpCmd;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.PrettyPrintDelimiter;
import com.android.tradefed.util.QuotationAwareTokenizer;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.SubprocessTestResultsParser;
import com.android.tradefed.util.SystemUtil;
import com.android.tradefed.util.keystore.IKeyStoreClient;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map.Entry;
import java.util.Set;

/**
 * Sandbox container that can run a Trade Federation invocation. TODO: Allow Options to be passed to
 * the sandbox.
 */
public class TradefedSandbox implements ISandbox {

    private static final String SANDBOX_PREFIX = "sandbox-";

    private File mStdoutFile = null;
    private File mStderrFile = null;
    private OutputStream mStdout = null;
    private FileOutputStream mStderr = null;
    private File mHeapDump = null;

    private File mSandboxTmpFolder = null;
    private File mRootFolder = null;
    private File mGlobalConfig = null;
    private File mSerializedContext = null;
    private File mSerializedConfiguration = null;

    private SubprocessTestResultsParser mEventParser = null;
    private StreamProtoReceiver mProtoReceiver = null;

    private IRunUtil mRunUtil;
    private boolean mCollectStdout = true;

    @Override
    public CommandResult run(IConfiguration config, ITestLogger logger) throws Throwable {
        List<String> mCmdArgs = new ArrayList<>();
        mCmdArgs.add(SystemUtil.getRunningJavaBinaryPath().getAbsolutePath());
        mCmdArgs.add(String.format("-Djava.io.tmpdir=%s", mSandboxTmpFolder.getAbsolutePath()));
        mCmdArgs.add(String.format("-DTF_JAR_DIR=%s", mRootFolder.getAbsolutePath()));
        // Setup heap dump collection
        try {
            mHeapDump = FileUtil.createTempDir("heap-dump", getWorkFolder());
            mCmdArgs.add("-XX:+HeapDumpOnOutOfMemoryError");
            mCmdArgs.add(String.format("-XX:HeapDumpPath=%s", mHeapDump.getAbsolutePath()));
        } catch (IOException e) {
            CLog.e(e);
        }
        mCmdArgs.addAll(getSandboxOptions(config).getJavaOptions());
        mCmdArgs.add("-cp");
        mCmdArgs.add(createClasspath(mRootFolder));
        mCmdArgs.add(TradefedSandboxRunner.class.getCanonicalName());
        mCmdArgs.add(mSerializedContext.getAbsolutePath());
        mCmdArgs.add(mSerializedConfiguration.getAbsolutePath());
        if (mProtoReceiver != null) {
            mCmdArgs.add("--" + StreamProtoResultReporter.PROTO_REPORT_PORT_OPTION);
            mCmdArgs.add(Integer.toString(mProtoReceiver.getSocketServerPort()));
        } else {
            mCmdArgs.add("--subprocess-report-port");
            mCmdArgs.add(Integer.toString(mEventParser.getSocketServerPort()));
        }
        if (config.getCommandOptions().shouldUseSandboxTestMode()) {
            // In test mode, re-add the --use-sandbox to trigger a sandbox run again in the process
            mCmdArgs.add("--" + CommandOptions.USE_SANDBOX);
        }

        long timeout = config.getCommandOptions().getInvocationTimeout();
        // Allow interruption, subprocess should handle signals itself
        mRunUtil.allowInterrupt(true);
        CommandResult result = null;
        try {
            result =
                    mRunUtil.runTimedCmd(
                            timeout, mStdout, mStderr, mCmdArgs.toArray(new String[0]));
        } catch (RuntimeException interrupted) {
            CLog.e("Sandbox runtimedCmd threw an exception");
            CLog.e(interrupted);
            result = new CommandResult(CommandStatus.EXCEPTION);
            result.setStdout(StreamUtil.getStackTrace(interrupted));
        }

        boolean failedStatus = false;
        String stderrText;
        try {
            stderrText = FileUtil.readStringFromFile(mStderrFile);
        } catch (IOException e) {
            stderrText = "Could not read the stderr output from process.";
        }
        if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
            failedStatus = true;
            result.setStderr(stderrText);
        }

        try {
            boolean joinResult = false;
            long waitTime = getSandboxOptions(config).getWaitForEventsTimeout();
            if (mProtoReceiver != null) {
                joinResult = mProtoReceiver.joinReceiver(waitTime);
            } else {
                joinResult = mEventParser.joinReceiver(waitTime);
            }

            if (!joinResult) {
                if (!failedStatus) {
                    result.setStatus(CommandStatus.EXCEPTION);
                }
                result.setStderr(
                        String.format("Event receiver thread did not complete.:\n%s", stderrText));
            }
            if (mProtoReceiver != null) {
                mProtoReceiver.completeModuleEvents();
            }
            PrettyPrintDelimiter.printStageDelimiter(
                    String.format(
                            "Execution of the tests occurred in the sandbox, you can find its logs "
                                    + "under the name pattern '%s*'",
                            SANDBOX_PREFIX));
        } finally {
            // Log the configuration used to run
            try (InputStreamSource configFile =
                    new FileInputStreamSource(mSerializedConfiguration)) {
                logger.testLog("sandbox-config", LogDataType.XML, configFile);
            }
            try (InputStreamSource contextFile = new FileInputStreamSource(mSerializedContext)) {
                logger.testLog("sandbox-context", LogDataType.PB, contextFile);
            }
            // Log stdout and stderr
            if (mStdoutFile != null) {
                try (InputStreamSource sourceStdOut = new FileInputStreamSource(mStdoutFile)) {
                    logger.testLog("sandbox-stdout", LogDataType.TEXT, sourceStdOut);
                }
            }
            try (InputStreamSource sourceStdErr = new FileInputStreamSource(mStderrFile)) {
                logger.testLog("sandbox-stderr", LogDataType.TEXT, sourceStdErr);
            }
            // Collect heap dump if any
            logAndCleanHeapDump(mHeapDump, logger);
            mHeapDump = null;
        }

        return result;
    }

    @Override
    public Exception prepareEnvironment(
            IInvocationContext context, IConfiguration config, ITestInvocationListener listener) {
        // Check for local sharding, avoid redirecting several stdout (from each shards) to the
        // sandbox stdout as it creates a lot of I/O to the same output.
        if (config.getCommandOptions().getShardCount() != null
                && config.getCommandOptions().getShardIndex() == null) {
            mCollectStdout = false;
        }
        // Create our temp directories.
        try {
            if (mCollectStdout) {
                mStdoutFile =
                        FileUtil.createTempFile("stdout_subprocess_", ".log", getWorkFolder());
                mStdout = new FileOutputStream(mStdoutFile);
            } else {
                mStdout =
                        new OutputStream() {
                            @Override
                            public void write(int b) throws IOException {
                                // Ignore stdout
                            }
                        };
            }

            mStderrFile = FileUtil.createTempFile("stderr_subprocess_", ".log", getWorkFolder());
            mStderr = new FileOutputStream(mStderrFile);

            mSandboxTmpFolder = FileUtil.createTempDir("tradefed-container", getWorkFolder());
        } catch (IOException e) {
            return e;
        }
        // Unset the current global environment
        mRunUtil = createRunUtil();
        mRunUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE);
        mRunUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_SERVER_CONFIG_VARIABLE);
        // TODO: add handling of setting and creating the subprocess global configuration
        if (getSandboxOptions(config).shouldEnableDebugThread()) {
            mRunUtil.setEnvVariable(TradefedSandboxRunner.DEBUG_THREAD_KEY, "true");
        }
        for (Entry<String, String> envEntry :
                getSandboxOptions(config).getEnvVariables().entrySet()) {
            mRunUtil.setEnvVariable(envEntry.getKey(), envEntry.getValue());
        }

        try {
            mRootFolder =
                    getTradefedSandboxEnvironment(
                            context,
                            config,
                            QuotationAwareTokenizer.tokenizeLine(
                                    config.getCommandLine(),
                                    /** no logging */
                                    false));
        } catch (ConfigurationException e) {
            return e;
        }

        PrettyPrintDelimiter.printStageDelimiter("Sandbox Configuration Preparation");
        // Prepare the configuration
        Exception res = prepareConfiguration(context, config, listener);
        if (res != null) {
            return res;
        }
        // Prepare the context
        try {
            mSerializedContext = prepareContext(context, config);
        } catch (IOException e) {
            return e;
        }

        return null;
    }

    @Override
    public void tearDown() {
        StreamUtil.close(mEventParser);
        StreamUtil.close(mProtoReceiver);
        StreamUtil.close(mStdout);
        StreamUtil.close(mStderr);
        FileUtil.deleteFile(mStdoutFile);
        FileUtil.deleteFile(mStderrFile);
        FileUtil.recursiveDelete(mSandboxTmpFolder);
        FileUtil.deleteFile(mSerializedContext);
        FileUtil.deleteFile(mSerializedConfiguration);
        FileUtil.deleteFile(mGlobalConfig);
    }

    @Override
    public File getTradefedSandboxEnvironment(
            IInvocationContext context, IConfiguration nonVersionedConfig, String[] args)
            throws ConfigurationException {
        SandboxOptions options = getSandboxOptions(nonVersionedConfig);
        // Check that we have no args conflicts.
        if (options.getSandboxTfDirectory() != null && options.getSandboxBuildId() != null) {
            throw new ConfigurationException(
                    String.format(
                            "Sandbox options %s and %s cannot be set at the same time",
                            SandboxOptions.TF_LOCATION, SandboxOptions.SANDBOX_BUILD_ID));
        }

        if (options.getSandboxTfDirectory() != null) {
            return options.getSandboxTfDirectory();
        }
        String tfDir = System.getProperty("TF_JAR_DIR");
        if (tfDir == null || tfDir.isEmpty()) {
            throw new ConfigurationException(
                    "Could not read TF_JAR_DIR to get current Tradefed instance.");
        }
        return new File(tfDir);
    }

    /**
     * Create a classpath based on the environment and the working directory returned by {@link
     * #getTradefedSandboxEnvironment(IInvocationContext, IConfiguration, String[])}.
     *
     * @param workingDir the current working directory for the sandbox.
     * @return The classpath to be use.
     */
    @Override
    public String createClasspath(File workingDir) throws ConfigurationException {
        // Get the classpath property.
        String classpathStr = System.getProperty("java.class.path");
        if (classpathStr == null) {
            throw new ConfigurationException(
                    "Could not find the classpath property: java.class.path");
        }
        return classpathStr;
    }

    /**
     * Prepare the {@link IConfiguration} that will be passed to the subprocess and will drive the
     * container execution.
     *
     * @param context The current {@link IInvocationContext}.
     * @param config the {@link IConfiguration} to be prepared.
     * @param listener The current invocation {@link ITestInvocationListener}.
     * @return an Exception if anything went wrong, null otherwise.
     */
    protected Exception prepareConfiguration(
            IInvocationContext context, IConfiguration config, ITestInvocationListener listener) {
        try {
            // TODO: switch reporting of parent and subprocess to proto
            String commandLine = config.getCommandLine();
            if (getSandboxOptions(config).shouldUseProtoReporter()) {
                mProtoReceiver =
                        new StreamProtoReceiver(listener, context, false, false, SANDBOX_PREFIX);
                // Force the child to the same mode as the parent.
                commandLine = commandLine + " --" + SandboxOptions.USE_PROTO_REPORTER;
            } else {
                mEventParser = new SubprocessTestResultsParser(listener, true, context);
                commandLine = commandLine + " --no-" + SandboxOptions.USE_PROTO_REPORTER;
            }
            String[] args =
                    QuotationAwareTokenizer.tokenizeLine(commandLine, /* No Logging */ false);
            mGlobalConfig = dumpGlobalConfig(config, new HashSet<>());
            try (InputStreamSource source = new FileInputStreamSource(mGlobalConfig)) {
                listener.testLog("sandbox-global-config", LogDataType.XML, source);
            }
            DumpCmd mode = DumpCmd.RUN_CONFIG;
            if (config.getCommandOptions().shouldUseSandboxTestMode()) {
                mode = DumpCmd.TEST_MODE;
            }

            try {
                mSerializedConfiguration =
                        SandboxConfigUtil.dumpConfigForVersion(
                                createClasspath(mRootFolder), mRunUtil, args, mode, mGlobalConfig);
            } catch (SandboxConfigurationException e) {
                // TODO: Improve our detection of that scenario
                CLog.e(e);
                CLog.e("%s", args[0]);
                if (e.getMessage().contains(String.format("Can not find local config %s", args[0]))
                        || e.getMessage()
                                .contains(
                                        String.format(
                                                "Could not find configuration '%s'", args[0]))) {
                    CLog.w(
                            "Child version doesn't contains '%s'. Attempting to backfill missing parent configuration.",
                            args[0]);
                    File parentConfig = handleChildMissingConfig(args);
                    if (parentConfig != null) {
                        try (InputStreamSource source = new FileInputStreamSource(parentConfig)) {
                            listener.testLog("sandbox-parent-config", LogDataType.XML, source);
                        }
                        try {
                            mSerializedConfiguration =
                                    SandboxConfigUtil.dumpConfigForVersion(
                                            createClasspath(mRootFolder),
                                            mRunUtil,
                                            new String[] {parentConfig.getAbsolutePath()},
                                            mode,
                                            mGlobalConfig);
                        } finally {
                            FileUtil.deleteFile(parentConfig);
                        }
                        return null;
                    }
                }
                throw e;
            }
            // Turn off some of the invocation level options that would be duplicated in the
            // child sandbox subprocess.
            config.getCommandOptions().setBugreportOnInvocationEnded(false);
            config.getCommandOptions().setBugreportzOnInvocationEnded(false);
        } catch (IOException | ConfigurationException e) {
            StreamUtil.close(mEventParser);
            StreamUtil.close(mProtoReceiver);
            return e;
        }
        return null;
    }

    @VisibleForTesting
    IRunUtil createRunUtil() {
        return new RunUtil();
    }

    /**
     * Prepare and serialize the {@link IInvocationContext}.
     *
     * @param context the {@link IInvocationContext} to be prepared.
     * @param config The {@link IConfiguration} of the sandbox.
     * @return the serialized {@link IInvocationContext}.
     * @throws IOException
     */
    protected File prepareContext(IInvocationContext context, IConfiguration config)
            throws IOException {
        // In test mode we need to keep the context unlocked for the next layer.
        if (config.getCommandOptions().shouldUseSandboxTestMode()) {
            try {
                Method unlock = InvocationContext.class.getDeclaredMethod("unlock");
                unlock.setAccessible(true);
                unlock.invoke(context);
                unlock.setAccessible(false);
            } catch (NoSuchMethodException
                    | SecurityException
                    | IllegalAccessException
                    | IllegalArgumentException
                    | InvocationTargetException e) {
                throw new IOException("Couldn't unlock the context.", e);
            }
        }
        File protoFile =
                FileUtil.createTempFile(
                        "context-proto", "." + LogDataType.PB.getFileExt(), mSandboxTmpFolder);
        Context contextProto = context.toProto();
        contextProto.writeDelimitedTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    /** Dump the global configuration filtered from some objects. */
    protected File dumpGlobalConfig(IConfiguration config, Set<String> exclusionPatterns)
            throws IOException, ConfigurationException {
        SandboxOptions options = getSandboxOptions(config);
        if (options.getChildGlobalConfig() != null) {
            IConfigurationFactory factory = ConfigurationFactory.getInstance();
            IGlobalConfiguration globalConfig =
                    factory.createGlobalConfigurationFromArgs(
                            new String[] {options.getChildGlobalConfig()}, new ArrayList<>());
            CLog.d(
                    "Using %s directly as global config without filtering",
                    options.getChildGlobalConfig());
            return globalConfig.cloneConfigWithFilter();
        }
        return SandboxConfigUtil.dumpFilteredGlobalConfig(exclusionPatterns);
    }

    /** {@inheritDoc} */
    @Override
    public IConfiguration createThinLauncherConfig(
            String[] args, IKeyStoreClient keyStoreClient, IRunUtil runUtil, File globalConfig) {
        // Default thin launcher cannot do anything, since this sandbox uses the same version as
        // the parent version.
        return null;
    }

    private SandboxOptions getSandboxOptions(IConfiguration config) {
        return (SandboxOptions)
                config.getConfigurationObject(Configuration.SANBOX_OPTIONS_TYPE_NAME);
    }

    private File handleChildMissingConfig(String[] args) {
        IConfiguration parentConfig = null;
        File tmpParentConfig = null;
        PrintWriter pw = null;
        try {
            tmpParentConfig = FileUtil.createTempFile("parent-config", ".xml", mSandboxTmpFolder);
            pw = new PrintWriter(tmpParentConfig);
            parentConfig = ConfigurationFactory.getInstance().createConfigurationFromArgs(args);
            // Do not print deprecated options to avoid compatibility issues, and do not print
            // unchanged options.
            parentConfig.dumpXml(pw, new ArrayList<>(), false, false);
            return tmpParentConfig;
        } catch (ConfigurationException | IOException e) {
            CLog.e("Parent doesn't understand the command either:");
            CLog.e(e);
            FileUtil.deleteFile(tmpParentConfig);
            return null;
        } finally {
            StreamUtil.close(pw);
        }
    }

    private void logAndCleanHeapDump(File heapDumpDir, ITestLogger logger) {
        try {
            if (heapDumpDir != null && heapDumpDir.listFiles().length != 0) {
                for (File f : heapDumpDir.listFiles()) {
                    FileInputStreamSource fileInput = new FileInputStreamSource(f);
                    logger.testLog(f.getName(), LogDataType.HPROF, fileInput);
                    StreamUtil.cancel(fileInput);
                }
            }
        } finally {
            FileUtil.recursiveDelete(heapDumpDir);
        }
    }

    private File getWorkFolder() {
        File workfolder = CurrentInvocation.getInfo(InvocationInfo.WORK_FOLDER);
        if (workfolder == null || !workfolder.exists()) {
            return null;
        }
        return workfolder;
    }
}
