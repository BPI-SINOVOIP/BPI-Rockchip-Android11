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

import com.android.annotations.VisibleForTesting;
import com.android.helper.aoa.UsbDevice;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileIdleMonitor;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.QuotationAwareTokenizer;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.StringEscapeUtils;
import com.android.tradefed.util.StringUtil;
import com.android.tradefed.util.SubprocessTestResultsParser;
import com.android.tradefed.util.SystemUtil;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.time.Duration;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * A {@link IRemoteTest} class to launch a command from TFC via a subprocess TF. FIXME: this needs
 * to be extended to support multi-device tests.
 */
@OptionClass(alias = "cluster", global_namespace = false)
public class ClusterCommandLauncher
        implements IRemoteTest, IInvocationContextReceiver, IConfigurationReceiver {

    public static final String TF_JAR_DIR = "TF_JAR_DIR";
    public static final String TF_PATH = "TF_PATH";
    public static final String TEST_WORK_DIR = "TEST_WORK_DIR";

    private static final Duration MAX_EVENT_RECEIVER_WAIT_TIME = Duration.ofMinutes(10);

    @Option(name = "root-dir", description = "A root directory", mandatory = true)
    private File mRootDir;

    @Option(name = "env-var", description = "Environment variables")
    private Map<String, String> mEnvVars = new LinkedHashMap<>();

    @Option(name = "setup-script", description = "Setup scripts")
    private List<String> mSetupScripts = new ArrayList<>();

    @Option(name = "script-timeout", description = "Script execution timeout", isTimeVal = true)
    private long mScriptTimeout = 30 * 60 * 1000;

    @Option(name = "jvm-option", description = "JVM options")
    private List<String> mJvmOptions = new ArrayList<>();

    @Option(name = "java-property", description = "Java properties")
    private Map<String, String> mJavaProperties = new LinkedHashMap<>();

    @Option(name = "command-line", description = "A command line to launch.", mandatory = true)
    private String mCommandLine = null;

    @Option(
            name = "original-command-line",
            description =
                    "Original command line. It may differ from command-line in retry invocations.")
    private String mOriginalCommandLine = null;

    @Option(name = "use-subprocess-reporting", description = "Use subprocess reporting.")
    private boolean mUseSubprocessReporting = false;

    @Option(
            name = "output-idle-timeout",
            description = "Maximum time to wait for an idle subprocess",
            isTimeVal = true)
    private long mOutputIdleTimeout = 0L;

    private IInvocationContext mInvocationContext;
    private IConfiguration mConfiguration;
    private IRunUtil mRunUtil;

    @Override
    public void setInvocationContext(IInvocationContext invocationContext) {
        mInvocationContext = invocationContext;
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    private String getEnvVar(String key) {
        return getEnvVar(key, null);
    }

    private String getEnvVar(String key, String defaultValue) {
        String value = mEnvVars.getOrDefault(key, defaultValue);
        if (value != null) {
            value = StringUtil.expand(value, mEnvVars);
        }
        return value;
    }

    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        // Get an expanded TF_PATH value.
        String tfPath = getEnvVar(TF_PATH, System.getProperty(TF_JAR_DIR));
        if (tfPath == null) {
            throw new RuntimeException("cannot find TF path!");
        }

        // Construct a Java class path based on TF_PATH value.
        // This expects TF_PATH to be a colon(:) separated list of paths where each path
        // points to a specific jar file or folder.
        // (example: path/to/tradefed.jar:path/to/tradefed/folder:...)
        final Set<String> jars = new LinkedHashSet<>();
        for (final String path : tfPath.split(":")) {
            final File jarFile = new File(path);
            if (!jarFile.exists()) {
                CLog.w("TF_PATH %s doesn't exist; ignoring", path);
                continue;
            }
            if (jarFile.isFile()) {
                jars.add(jarFile.getAbsolutePath());
            } else {
                jars.add(new File(path, "*").getAbsolutePath());
            }
        }

        IRunUtil runUtil = getRunUtil();
        runUtil.setWorkingDir(mRootDir);
        // clear the TF_GLOBAL_CONFIG env, so another tradefed will not reuse the global config file
        runUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE);
        for (final String key : mEnvVars.keySet()) {
            runUtil.setEnvVariable(key, getEnvVar(key));
        }

        final File testWorkDir = new File(getEnvVar(TEST_WORK_DIR, mRootDir.getAbsolutePath()));
        final File logDir = new File(mRootDir, "logs");
        logDir.mkdirs();
        File stdoutFile = new File(logDir, "stdout.txt");
        File stderrFile = new File(logDir, "stderr.txt");
        FileIdleMonitor monitor = createFileMonitor(stdoutFile, stderrFile);

        SubprocessTestResultsParser subprocessEventParser = null;
        try (FileOutputStream stdout = new FileOutputStream(stdoutFile);
                FileOutputStream stderr = new FileOutputStream(stderrFile)) {
            long timeout = mScriptTimeout;
            long startTime = System.currentTimeMillis();
            for (String script : mSetupScripts) {
                script = StringUtil.expand(script, mEnvVars);
                CLog.i("Running a setup script: %s", script);
                // FIXME: Refactor command execution into a helper function.
                CommandResult result =
                        runUtil.runTimedCmd(
                                timeout,
                                stdout,
                                stderr,
                                QuotationAwareTokenizer.tokenizeLine(script));
                if (!result.getStatus().equals(CommandStatus.SUCCESS)) {
                    String error = null;
                    if (result.getStatus().equals(CommandStatus.TIMED_OUT)) {
                        error = "timeout";
                    } else {
                        error = FileUtil.readStringFromFile(stderrFile);
                    }
                    throw new RuntimeException(String.format("Script failed to run: %s", error));
                }
                timeout -= (System.currentTimeMillis() - startTime);
                if (timeout < 0) {
                    throw new RuntimeException(
                            String.format("Setup scripts failed to run in %sms", mScriptTimeout));
                }
            }

            String classpath = ArrayUtil.join(":", jars);
            String commandLine = mCommandLine;
            if (classpath.isEmpty()) {
                throw new RuntimeException(
                        String.format("cannot find any TF jars from %s!", tfPath));
            }

            if (mOriginalCommandLine != null && !mOriginalCommandLine.equals(commandLine)) {
                // Make sure a wrapper XML of the original command is available because retries
                // try to run original commands in Q+. If the original command was run with
                // subprocess reporting, a recorded command would be one with .xml suffix.
                new SubprocessConfigBuilder()
                        .setWorkingDir(testWorkDir)
                        .setOriginalConfig(
                                QuotationAwareTokenizer.tokenizeLine(mOriginalCommandLine)[0])
                        .build();
            }
            if (mUseSubprocessReporting) {
                SubprocessReportingHelper mHelper = new SubprocessReportingHelper();
                // Create standalone jar for subprocess result reporter, which is used
                // for pre-O cts. The created jar is put in front position of the class path to
                // override class with the same name.
                classpath =
                        String.format(
                                "%s:%s",
                                mHelper.createSubprocessReporterJar(mRootDir).getAbsolutePath(),
                                classpath);
                subprocessEventParser =
                        createSubprocessTestResultsParser(listener, true, mInvocationContext);
                String port = Integer.toString(subprocessEventParser.getSocketServerPort());
                commandLine = mHelper.buildNewCommandConfig(commandLine, port, testWorkDir);
            }

            List<String> javaCommandArgs = buildJavaCommandArgs(classpath, commandLine);
            CLog.i("Running a command line: %s", commandLine);
            CLog.i("args = %s", javaCommandArgs);
            CLog.i("test working directory = %s", testWorkDir);

            monitor.start();
            runUtil.setWorkingDir(testWorkDir);
            CommandResult result =
                    runUtil.runTimedCmd(
                            mConfiguration.getCommandOptions().getInvocationTimeout(),
                            stdout,
                            stderr,
                            javaCommandArgs.toArray(new String[javaCommandArgs.size()]));
            if (!result.getStatus().equals(CommandStatus.SUCCESS)) {
                String error = null;
                if (result.getStatus().equals(CommandStatus.TIMED_OUT)) {
                    error = "timeout";
                } else {
                    error = FileUtil.readStringFromFile(stderrFile);
                }
                throw new RuntimeException(String.format("Command failed to run: %s", error));
            }
            CLog.i("Successfully ran a command");

        } catch (IOException e) {
            throw new RuntimeException(e);
        } finally {
            monitor.stop();
            if (subprocessEventParser != null) {
                subprocessEventParser.joinReceiver(
                        MAX_EVENT_RECEIVER_WAIT_TIME.toMillis(), /* wait for connection */ false);
                StreamUtil.close(subprocessEventParser);
            }
        }
    }

    /** Build a shell command line to invoke a TF process. */
    private List<String> buildJavaCommandArgs(String classpath, String tfCommandLine) {
        // Build a command line to invoke a TF process.
        final List<String> cmdArgs = new ArrayList<>();
        cmdArgs.add(SystemUtil.getRunningJavaBinaryPath().getAbsolutePath());
        cmdArgs.add("-cp");
        cmdArgs.add(classpath);
        cmdArgs.addAll(mJvmOptions);

        // Pass Java properties as -D options.
        for (final Entry<String, String> entry : mJavaProperties.entrySet()) {
            cmdArgs.add(
                    String.format(
                            "-D%s=%s",
                            entry.getKey(), StringUtil.expand(entry.getValue(), mEnvVars)));
        }
        cmdArgs.add("com.android.tradefed.command.CommandRunner");
        tfCommandLine = StringUtil.expand(tfCommandLine, mEnvVars);
        cmdArgs.addAll(StringEscapeUtils.paramsToArgs(ArrayUtil.list(tfCommandLine)));

        final Integer shardCount = mConfiguration.getCommandOptions().getShardCount();
        final Integer shardIndex = mConfiguration.getCommandOptions().getShardIndex();
        if (shardCount != null && shardCount > 1) {
            cmdArgs.add("--shard-count");
            cmdArgs.add(Integer.toString(shardCount));
            if (shardIndex != null) {
                cmdArgs.add("--shard-index");
                cmdArgs.add(Integer.toString(shardIndex));
            }
        }

        for (final ITestDevice device : mInvocationContext.getDevices()) {
            // FIXME: Find a better way to support non-physical devices as well.
            cmdArgs.add("--serial");
            cmdArgs.add(device.getSerialNumber());
        }

        return cmdArgs;
    }

    /** Creates a file monitor which will perform a USB port reset if the subprocess is idle. */
    private FileIdleMonitor createFileMonitor(File... files) {
        // treat zero or negative timeout as infinite
        long timeout = mOutputIdleTimeout > 0 ? mOutputIdleTimeout : Long.MAX_VALUE;
        // reset USB ports if files are idle for too long
        // TODO(peykov): consider making the callback customizable
        return new FileIdleMonitor(Duration.ofMillis(timeout), this::resetUsbPorts, files);
    }

    /** Performs a USB port reset on all devices. */
    private void resetUsbPorts() {
        CLog.i("Subprocess output idle for %d ms, attempting USB port reset.", mOutputIdleTimeout);
        try (UsbHelper usb = new UsbHelper()) {
            for (String serial : mInvocationContext.getSerials()) {
                try (UsbDevice device = usb.getDevice(serial)) {
                    if (device == null) {
                        CLog.w("Device '%s' not found during USB reset.", serial);
                        continue;
                    }
                    CLog.d("Resetting USB port for device '%s'", serial);
                    device.reset();
                }
            }
        }
    }

    @VisibleForTesting
    IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = new RunUtil();
        }
        return mRunUtil;
    }

    @VisibleForTesting
    SubprocessTestResultsParser createSubprocessTestResultsParser(
            ITestInvocationListener listener, boolean streaming, IInvocationContext context)
            throws IOException {
        return new SubprocessTestResultsParser(listener, streaming, context);
    }

    @VisibleForTesting
    Map<String, String> getEnvVars() {
        return mEnvVars;
    }

    @VisibleForTesting
    List<String> getSetupScripts() {
        return mSetupScripts;
    }

    @VisibleForTesting
    List<String> getJvmOptions() {
        return mJvmOptions;
    }

    @VisibleForTesting
    Map<String, String> getJavaProperties() {
        return mJavaProperties;
    }

    @VisibleForTesting
    String getCommandLine() {
        return mCommandLine;
    }

    @VisibleForTesting
    boolean useSubprocessReporting() {
        return mUseSubprocessReporting;
    }
}
