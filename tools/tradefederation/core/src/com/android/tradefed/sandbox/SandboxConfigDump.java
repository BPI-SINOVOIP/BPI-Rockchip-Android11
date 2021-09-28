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

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.config.SandboxConfigurationFactory;
import com.android.tradefed.device.IDeviceSelection;
import com.android.tradefed.log.FileLogger;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.result.FileSystemLogSaver;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.SubprocessResultsReporter;
import com.android.tradefed.result.proto.StreamProtoResultReporter;
import com.android.tradefed.testtype.SubprocessTfLauncher;
import com.android.tradefed.util.StreamUtil;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Runner class that creates a {@link IConfiguration} based on a command line and dump it to a file.
 * args: <DumpCmd> <output File> <remaing command line>
 */
public class SandboxConfigDump {

    public enum DumpCmd {
        /** The full xml based on the command line will be outputted */
        FULL_XML,
        /** Only non-versioned element of the xml will be outputted */
        NON_VERSIONED_CONFIG,
        /** A run-ready config will be outputted */
        RUN_CONFIG,
        /** Special mode that allows the sandbox to generate another layer of sandboxing. */
        TEST_MODE,
    }

    /**
     * We do not output the versioned elements to avoid causing the parent process to have issues
     * with them when trying to resolve them
     */
    public static final Set<String> VERSIONED_ELEMENTS = new HashSet<>();

    static {
        VERSIONED_ELEMENTS.add(Configuration.SYSTEM_STATUS_CHECKER_TYPE_NAME);
        VERSIONED_ELEMENTS.add(Configuration.DEVICE_METRICS_COLLECTOR_TYPE_NAME);
        VERSIONED_ELEMENTS.add(Configuration.MULTI_PRE_TARGET_PREPARER_TYPE_NAME);
        VERSIONED_ELEMENTS.add(Configuration.MULTI_PREPARER_TYPE_NAME);
        VERSIONED_ELEMENTS.add(Configuration.TARGET_PREPARER_TYPE_NAME);
        VERSIONED_ELEMENTS.add(Configuration.TEST_TYPE_NAME);
    }

    /**
     * Parse the args and creates a {@link IConfiguration} from it then dumps it to the result file.
     */
    public int parse(String[] args) {
        // TODO: add some more checking
        List<String> argList = new ArrayList<>(Arrays.asList(args));
        DumpCmd cmd = DumpCmd.valueOf(argList.remove(0));
        File resFile = new File(argList.remove(0));
        SandboxConfigurationFactory factory = SandboxConfigurationFactory.getInstance();
        PrintWriter pw = null;
        try {
            IConfiguration config =
                    factory.createConfigurationFromArgs(argList.toArray(new String[0]), cmd);
            if (DumpCmd.RUN_CONFIG.equals(cmd) || DumpCmd.TEST_MODE.equals(cmd)) {
                config.getCommandOptions().setShouldUseSandboxing(false);
                config.getConfigurationDescription().setSandboxed(true);
                // Don't use replication in the sandbox
                config.getCommandOptions().setReplicateSetup(false);
                // Set the reporter
                ITestInvocationListener reporter = null;
                if (getSandboxOptions(config).shouldUseProtoReporter()) {
                    reporter = new StreamProtoResultReporter();
                } else {
                    reporter = new SubprocessResultsReporter();
                    ((SubprocessResultsReporter) reporter).setOutputTestLog(true);
                }
                config.setTestInvocationListener(reporter);
                // Set log level for sandbox
                ILeveledLogOutput logger = config.getLogOutput();
                logger.setLogLevel(LogLevel.VERBOSE);
                if (logger instanceof FileLogger) {
                    // Ensure we get the stdout logging in FileLogger case.
                    ((FileLogger) logger).setLogLevelDisplay(LogLevel.VERBOSE);
                }

                ILogSaver logSaver = config.getLogSaver();
                if (logSaver instanceof FileSystemLogSaver) {
                    // Send the files directly, the parent will take care of compression if needed
                    ((FileSystemLogSaver) logSaver).setCompressFiles(false);
                }

                // Ensure in special conditions (placeholder devices) we can still allocate.
                secureDeviceAllocation(config);

                // Mark as subprocess
                config.getCommandOptions()
                        .getInvocationData()
                        .put(SubprocessTfLauncher.SUBPROCESS_TAG_NAME, "true");
            }
            if (DumpCmd.TEST_MODE.equals(cmd)) {
                // We allow one more layer of sandbox to be generated
                config.getCommandOptions().setShouldUseSandboxing(true);
                config.getConfigurationDescription().setSandboxed(false);
                // Ensure we turn off test mode afterward to avoid infinite sandboxing
                config.getCommandOptions().setUseSandboxTestMode(false);
            }
            pw = new PrintWriter(resFile);
            if (DumpCmd.NON_VERSIONED_CONFIG.equals(cmd)) {
                // Remove elements that are versioned.
                config.dumpXml(
                        pw,
                        new ArrayList<>(VERSIONED_ELEMENTS),
                        true, /* Don't print unchanged options */
                        false);
            } else {
                // FULL_XML in that case.
                config.dumpXml(pw);
            }
        } catch (ConfigurationException | IOException e) {
            e.printStackTrace();
            return 1;
        } finally {
            StreamUtil.close(pw);
        }
        return 0;
    }

    public static void main(final String[] mainArgs) {
        try {
            GlobalConfiguration.createGlobalConfiguration(new String[] {});
        } catch (ConfigurationException e) {
            e.printStackTrace();
            System.exit(1);
        }
        SandboxConfigDump configDump = new SandboxConfigDump();
        int code = configDump.parse(mainArgs);
        System.exit(code);
    }

    private SandboxOptions getSandboxOptions(IConfiguration config) {
        return (SandboxOptions)
                config.getConfigurationObject(Configuration.SANBOX_OPTIONS_TYPE_NAME);
    }

    private void secureDeviceAllocation(IConfiguration config) {
        for (IDeviceConfiguration deviceConfig : config.getDeviceConfig()) {
            IDeviceSelection requirements = deviceConfig.getDeviceRequirements();
            if (requirements.nullDeviceRequested()
                    || requirements.tcpDeviceRequested()
                    || requirements.gceDeviceRequested()) {
                // Reset serials, ensure any null/tcp/gce-device can be selected.
                requirements.setSerial();
            }
        }
    }
}
