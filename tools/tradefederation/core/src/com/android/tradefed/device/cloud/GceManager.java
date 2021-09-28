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

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.cloud.AcloudConfigParser.AcloudKeys;
import com.android.tradefed.device.cloud.GceAvdInfo.GceStatus;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.GoogleApiClientUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import com.google.api.client.auth.oauth2.Credential;
import com.google.api.client.googleapis.javanet.GoogleNetHttpTransport;
import com.google.api.client.json.JsonFactory;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.services.compute.Compute;
import com.google.api.services.compute.Compute.Instances.GetSerialPortOutput;
import com.google.api.services.compute.ComputeScopes;
import com.google.api.services.compute.model.SerialPortOutput;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.net.HostAndPort;

import java.io.File;
import java.io.IOException;
import java.lang.ProcessBuilder.Redirect;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Helper that manages the GCE calls to start/stop and collect logs from GCE. */
public class GceManager {
    public static final String GCE_INSTANCE_NAME_KEY = "gce-instance-name";
    public static final String GCE_INSTANCE_CLEANED_KEY = "gce-instance-clean-called";

    private static final long BUGREPORT_TIMEOUT = 15 * 60 * 1000L;
    private static final long REMOTE_FILE_OP_TIMEOUT = 10 * 60 * 1000L;
    private static final Pattern BUGREPORTZ_RESPONSE_PATTERN = Pattern.compile("(OK:)(.*)");
    private static final JsonFactory JSON_FACTORY = JacksonFactory.getDefaultInstance();
    private static final List<String> SCOPES = Arrays.asList(ComputeScopes.COMPUTE_READONLY);

    private DeviceDescriptor mDeviceDescriptor;
    private TestDeviceOptions mDeviceOptions;
    private IBuildInfo mBuildInfo;

    private String mGceInstanceName = null;
    private String mGceHost = null;
    private GceAvdInfo mGceAvdInfo = null;

    /**
     * Ctor
     *
     * @param deviceDesc The {@link DeviceDescriptor} that will be associated with the GCE device.
     * @param deviceOptions A {@link TestDeviceOptions} associated with the device.
     * @param buildInfo A {@link IBuildInfo} describing the gce build to start.
     */
    public GceManager(
            DeviceDescriptor deviceDesc, TestDeviceOptions deviceOptions, IBuildInfo buildInfo) {
        mDeviceDescriptor = deviceDesc;
        mDeviceOptions = deviceOptions;
        mBuildInfo = buildInfo;

        if (!deviceOptions.allowGceCmdTimeoutOverride()) {
            return;
        }
        int index = deviceOptions.getGceDriverParams().lastIndexOf("--boot-timeout");
        if (index != -1 && deviceOptions.getGceDriverParams().size() > index + 1) {
            String driverTimeoutStringSec = deviceOptions.getGceDriverParams().get(index + 1);
            try {
                // Add some extra time on top of Acloud: acloud boot the device then we expect
                // the Tradefed online check to take a bit of time, use 3min as a safe overhead
                long driverTimeoutMs =
                        Long.parseLong(driverTimeoutStringSec) * 1000 + 3 * 60 * 1000;
                long gceCmdTimeoutMs = deviceOptions.getGceCmdTimeout();
                deviceOptions.setGceCmdTimeout(driverTimeoutMs);
                CLog.i(
                        "Replacing --gce-boot-timeout %s by --boot-timeout %s.",
                        gceCmdTimeoutMs, driverTimeoutMs);
            } catch (NumberFormatException e) {
                CLog.e(e);
            }
        }
    }

    /** @deprecated Use other constructors, we keep this temporarily for backward compatibility. */
    @Deprecated
    public GceManager(
            DeviceDescriptor deviceDesc,
            TestDeviceOptions deviceOptions,
            IBuildInfo buildInfo,
            List<IBuildInfo> testResourceBuildInfos) {
        this(deviceDesc, deviceOptions, buildInfo);
    }

    /**
     * Ctor, variation that can be used to provide the GCE instance name to use directly.
     *
     * @param deviceDesc The {@link DeviceDescriptor} that will be associated with the GCE device.
     * @param deviceOptions A {@link TestDeviceOptions} associated with the device
     * @param buildInfo A {@link IBuildInfo} describing the gce build to start.
     * @param gceInstanceName The instance name to use.
     * @param gceHost The host name or ip of the instance to use.
     */
    public GceManager(
            DeviceDescriptor deviceDesc,
            TestDeviceOptions deviceOptions,
            IBuildInfo buildInfo,
            String gceInstanceName,
            String gceHost) {
        this(deviceDesc, deviceOptions, buildInfo);
        mGceInstanceName = gceInstanceName;
        mGceHost = gceHost;
    }

    public GceAvdInfo startGce() throws TargetSetupError {
        return startGce(null);
    }

    /**
     * Attempt to start a gce instance
     *
     * @return a {@link GceAvdInfo} describing the GCE instance. Could be a BOOT_FAIL instance.
     * @throws TargetSetupError
     */
    public GceAvdInfo startGce(String ipDevice) throws TargetSetupError {
        mGceAvdInfo = null;
        // For debugging purposes bypass.
        if (mGceHost != null && mGceInstanceName != null) {
            mGceAvdInfo =
                    new GceAvdInfo(
                            mGceInstanceName,
                            HostAndPort.fromString(mGceHost)
                                    .withDefaultPort(mDeviceOptions.getRemoteAdbPort()));
            return mGceAvdInfo;
        }
        // Add extra args.
        File reportFile = null;
        try {
            reportFile = FileUtil.createTempFile("gce_avd_driver", ".json");
            List<String> gceArgs = buildGceCmd(reportFile, mBuildInfo, ipDevice);

            long driverTimeoutMs = getTestDeviceOptions().getGceCmdTimeout();
            if (!getTestDeviceOptions().allowGceCmdTimeoutOverride()) {
                long driverTimeoutSec =
                        Duration.ofMillis(driverTimeoutMs - 3 * 60 * 1000).toSeconds();
                // --boot-timeout takes a value in seconds
                gceArgs.add("--boot-timeout");
                gceArgs.add(Long.toString(driverTimeoutSec));
                driverTimeoutMs = driverTimeoutSec * 1000;
            }

            CLog.i("Launching GCE with %s", gceArgs.toString());
            CommandResult cmd =
                    getRunUtil()
                            .runTimedCmd(
                                    getTestDeviceOptions().getGceCmdTimeout(),
                                    gceArgs.toArray(new String[gceArgs.size()]));
            CLog.i("GCE driver stderr: %s", cmd.getStderr());
            String instanceName = extractInstanceName(cmd.getStderr());
            if (instanceName != null) {
                mBuildInfo.addBuildAttribute(GCE_INSTANCE_NAME_KEY, instanceName);
            } else {
                CLog.w("Could not extract an instance name for the gce device.");
            }
            if (CommandStatus.TIMED_OUT.equals(cmd.getStatus())) {
                String errors =
                        String.format(
                                "acloud errors: timeout after %dms, acloud did not return",
                                driverTimeoutMs);
                if (instanceName != null) {
                    // If we managed to parse the instance name, report the boot failure so it
                    // can be shutdown.
                    mGceAvdInfo = new GceAvdInfo(instanceName, null, errors, GceStatus.BOOT_FAIL);
                    return mGceAvdInfo;
                }
                throw new TargetSetupError(errors, mDeviceDescriptor);
            } else if (!CommandStatus.SUCCESS.equals(cmd.getStatus())) {
                CLog.w("Error when booting the Gce instance, reading output of gce driver");
                mGceAvdInfo =
                        GceAvdInfo.parseGceInfoFromFile(
                                reportFile, mDeviceDescriptor, mDeviceOptions.getRemoteAdbPort());
                String errors = "";
                if (mGceAvdInfo != null) {
                    // We always return the GceAvdInfo describing the instance when possible
                    // The caller can decide actions to be taken.
                    return mGceAvdInfo;
                } else {
                    errors =
                            "Could not get a valid instance name, check the gce driver's output."
                                    + "The instance may not have booted up at all.";
                    CLog.e(errors);
                    throw new TargetSetupError(
                            String.format("acloud errors: %s", errors), mDeviceDescriptor);
                }
            }
            mGceAvdInfo =
                    GceAvdInfo.parseGceInfoFromFile(
                            reportFile, mDeviceDescriptor, mDeviceOptions.getRemoteAdbPort());
            return mGceAvdInfo;
        } catch (IOException e) {
            throw new TargetSetupError("failed to create log file", e, mDeviceDescriptor);
        } finally {
            FileUtil.deleteFile(reportFile);
        }
    }

    /**
     * Retrieve the instance name from the gce boot logs. Search for the 'name': 'gce-<name>'
     * pattern to extract the name of it. We extract from the logs instead of result file because on
     * gce boot failure, the attempted instance name won't show in json.
     */
    protected String extractInstanceName(String bootupLogs) {
        if (bootupLogs != null) {
            final String pattern = "'name': u?'(((gce-)|(ins-))(.*?))'";
            Pattern namePattern = Pattern.compile(pattern);
            Matcher matcher = namePattern.matcher(bootupLogs);
            if (matcher.find()) {
                return matcher.group(1);
            }
        }
        return null;
    }

    /** Build and return the command to launch GCE. Exposed for testing. */
    protected List<String> buildGceCmd(File reportFile, IBuildInfo b, String ipDevice) {
        File avdDriverFile = getTestDeviceOptions().getAvdDriverBinary();
        if (!avdDriverFile.exists()) {
            throw new RuntimeException(
                    String.format(
                            "Could not find the Acloud driver at %s",
                            avdDriverFile.getAbsolutePath()));
        }
        if (!avdDriverFile.canExecute()) {
            // Set the executable bit if needed
            FileUtil.chmodGroupRWX(avdDriverFile);
        }
        List<String> gceArgs = ArrayUtil.list(avdDriverFile.getAbsolutePath());
        gceArgs.add(
                TestDeviceOptions.getCreateCommandByInstanceType(
                        getTestDeviceOptions().getInstanceType()));
        // Handle the build id related params
        List<String> gceDriverParams = getTestDeviceOptions().getGceDriverParams();

        if (TestDeviceOptions.InstanceType.CHEEPS.equals(
                getTestDeviceOptions().getInstanceType())) {
            gceArgs.add("--avd-type");
            gceArgs.add("cheeps");

            if (getTestDeviceOptions().getCrosUser() != null
                    && getTestDeviceOptions().getCrosPassword() != null) {
                gceArgs.add("--user");
                gceArgs.add(getTestDeviceOptions().getCrosUser());
                gceArgs.add("--password");
                gceArgs.add(getTestDeviceOptions().getCrosPassword());
            }
        }

        // If args passed by gce-driver-param do not contain build_id or branch,
        // use build_id and branch from device BuildInfo
        if (!gceDriverParams.contains("--build_id") && !gceDriverParams.contains("--branch")) {
            gceArgs.add("--build_target");
            if (b.getBuildAttributes().containsKey("build_target")) {
                // If BuildInfo contains the attribute for a build target, use that.
                gceArgs.add(b.getBuildAttributes().get("build_target"));
            } else {
                gceArgs.add(b.getBuildFlavor());
            }
            gceArgs.add("--branch");
            gceArgs.add(b.getBuildBranch());
            gceArgs.add("--build_id");
            gceArgs.add(b.getBuildId());
        }
        // Add additional args passed by gce-driver-param.
        gceArgs.addAll(gceDriverParams);
        // Get extra params by instance type
        gceArgs.addAll(
                TestDeviceOptions.getExtraParamsByInstanceType(
                        getTestDeviceOptions().getInstanceType(),
                        getTestDeviceOptions().getBaseImage()));
        if (ipDevice == null) {
            gceArgs.add("--config_file");
            gceArgs.add(getAvdConfigFile().getAbsolutePath());
            if (getTestDeviceOptions().getServiceAccountJsonKeyFile() != null) {
                gceArgs.add("--service_account_json_private_key_path");
                gceArgs.add(
                        getTestDeviceOptions().getServiceAccountJsonKeyFile().getAbsolutePath());
            }
        } else {
            gceArgs.add("--host");
            gceArgs.add(ipDevice);
        }
        gceArgs.add("--report_file");
        gceArgs.add(reportFile.getAbsolutePath());
        switch (getTestDeviceOptions().getGceDriverLogLevel()) {
            case DEBUG:
                gceArgs.add("-v");
                break;
            case VERBOSE:
                gceArgs.add("-vv");
                break;
            default:
                break;
        }
        if (getTestDeviceOptions().getGceAccount() != null) {
            gceArgs.add("--email");
            gceArgs.add(getTestDeviceOptions().getGceAccount());
        }
        // Do not pass flags --logcat_file and --serial_log_file to collect logcat and serial logs.

        return gceArgs;
    }

    /**
     * Shutdown the Gce instance associated with the {@link #startGce()}.
     *
     * @return returns true if gce shutdown was requested as non-blocking.
     */
    public boolean shutdownGce() {
        if (!getTestDeviceOptions().getAvdDriverBinary().canExecute()) {
            mGceAvdInfo = null;
            throw new RuntimeException(
                    String.format(
                            "GCE launcher %s is invalid",
                            getTestDeviceOptions().getAvdDriverBinary()));
        }
        String instanceName = null;
        boolean notFromGceAvd = false;
        if (mGceAvdInfo != null) {
            instanceName = mGceAvdInfo.instanceName();
        }
        if (instanceName == null) {
            instanceName = mBuildInfo.getBuildAttributes().get(GCE_INSTANCE_NAME_KEY);
            notFromGceAvd = true;
        }
        if (instanceName == null) {
            CLog.d("No instance to shutdown.");
            return false;
        }
        try {
            boolean res = AcloudShutdown(getTestDeviceOptions(), getRunUtil(), instanceName);
            // Be more lenient if instance name was not reported officially and we still attempt
            // to clean it.
            if (res || notFromGceAvd) {
                mBuildInfo.addBuildAttribute(GCE_INSTANCE_CLEANED_KEY, "true");
            }
            return res;
        } finally {
            mGceAvdInfo = null;
        }
    }

    /**
     * Actual Acloud run to shutdown the virtual device.
     *
     * @param options The {@link TestDeviceOptions} for the Acloud options
     * @param runUtil The {@link IRunUtil} to run Acloud
     * @param instanceName The instance to shutdown.
     * @return True if successful
     */
    public static boolean AcloudShutdown(
            TestDeviceOptions options, IRunUtil runUtil, String instanceName) {
        List<String> gceArgs = ArrayUtil.list(options.getAvdDriverBinary().getAbsolutePath());
        gceArgs.add("delete");
        // Add extra args.
        File f = null;
        File config = null;
        try {
            config = FileUtil.createTempFile(options.getAvdConfigFile().getName(), "config");
            gceArgs.add("--instance_names");
            gceArgs.add(instanceName);
            gceArgs.add("--config_file");
            // Copy the config in case it comes from a dynamic file. In order to ensure Acloud has
            // the file until it's done with it.
            FileUtil.copyFile(options.getAvdConfigFile(), config);
            gceArgs.add(config.getAbsolutePath());
            if (options.getServiceAccountJsonKeyFile() != null) {
                gceArgs.add("--service_account_json_private_key_path");
                gceArgs.add(options.getServiceAccountJsonKeyFile().getAbsolutePath());
            }
            f = FileUtil.createTempFile("gce_avd_driver", ".json");
            gceArgs.add("--report_file");
            gceArgs.add(f.getAbsolutePath());
            CLog.i("Tear down of GCE with %s", gceArgs.toString());
            if (options.waitForGceTearDown()) {
                CommandResult cmd =
                        runUtil.runTimedCmd(
                                options.getGceCmdTimeout(),
                                gceArgs.toArray(new String[gceArgs.size()]));
                FileUtil.deleteFile(config);
                if (!CommandStatus.SUCCESS.equals(cmd.getStatus())) {
                    CLog.w(
                            "Failed to tear down GCE %s with the following arg: %s."
                                    + "\nstdout:%s\nstderr:%s",
                            instanceName, gceArgs, cmd.getStdout(), cmd.getStderr());
                    return false;
                }
            } else {
                // Discard the output so the process is not linked to the parent and doesn't die
                // if the JVM exit.
                Process p = runUtil.runCmdInBackground(Redirect.DISCARD, gceArgs);
                AcloudDeleteCleaner cleaner = new AcloudDeleteCleaner(p, config);
                cleaner.start();
            }
        } catch (IOException | RuntimeException e) {
            CLog.e("failed to create log file for GCE Teardown");
            CLog.e(e);
            FileUtil.deleteFile(config);
            return false;
        } finally {
            FileUtil.deleteFile(f);
        }
        return true;
    }

    /**
     * Get a bugreportz from the device using ssh to avoid any adb connection potential issue.
     *
     * @param gceAvd The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @return A file pointing to the zip bugreport, or null if an issue occurred.
     * @throws IOException
     */
    public static File getBugreportzWithSsh(
            GceAvdInfo gceAvd, TestDeviceOptions options, IRunUtil runUtil) throws IOException {
        String output = remoteSshCommandExec(gceAvd, options, runUtil, "bugreportz");
        Matcher match = BUGREPORTZ_RESPONSE_PATTERN.matcher(output);
        if (!match.find()) {
            CLog.e("Something went wrong during bugreportz collection: '%s'", output);
            return null;
        }
        String remoteFilePath = match.group(2);
        File localTmpFile = FileUtil.createTempFile("bugreport-ssh", ".zip");
        if (!RemoteFileUtil.fetchRemoteFile(
                gceAvd, options, runUtil, REMOTE_FILE_OP_TIMEOUT, remoteFilePath, localTmpFile)) {
            FileUtil.deleteFile(localTmpFile);
            return null;
        }
        return localTmpFile;
    }

    /**
     * Get a bugreport via ssh for a nested instance. This requires requesting the adb in the nested
     * virtual instance.
     *
     * @param gceAvd The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @return A file pointing to the zip bugreport, or null if an issue occurred.
     * @throws IOException
     */
    public static File getNestedDeviceSshBugreportz(
            GceAvdInfo gceAvd, TestDeviceOptions options, IRunUtil runUtil) throws IOException {
        if (gceAvd == null || gceAvd.hostAndPort() == null) {
            return null;
        }
        String output =
                remoteSshCommandExec(
                        gceAvd,
                        options,
                        runUtil,
                        "./bin/adb",
                        "wait-for-device",
                        "shell",
                        "bugreportz");
        Matcher match = BUGREPORTZ_RESPONSE_PATTERN.matcher(output);
        if (!match.find()) {
            CLog.e("Something went wrong during bugreportz collection: '%s'", output);
            return null;
        }
        String deviceFilePath = match.group(2);
        String pullOutput =
                remoteSshCommandExec(gceAvd, options, runUtil, "./bin/adb", "pull", deviceFilePath);
        CLog.d(pullOutput);
        String remoteFilePath = "./" + new File(deviceFilePath).getName();
        File localTmpFile = FileUtil.createTempFile("bugreport-ssh", ".zip");
        if (!RemoteFileUtil.fetchRemoteFile(
                gceAvd, options, runUtil, REMOTE_FILE_OP_TIMEOUT, remoteFilePath, localTmpFile)) {
            FileUtil.deleteFile(localTmpFile);
            return null;
        }
        return localTmpFile;
    }

    /**
     * Fetch a remote file from a nested instance and log it.
     *
     * @param logger The {@link ITestLogger} where to log the file.
     * @param gceAvd The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param remoteFilePath The remote path where to find the file.
     * @param type the {@link LogDataType} of the logged file.
     */
    public static void logNestedRemoteFile(
            ITestLogger logger,
            GceAvdInfo gceAvd,
            TestDeviceOptions options,
            IRunUtil runUtil,
            String remoteFilePath,
            LogDataType type) {
        logNestedRemoteFile(logger, gceAvd, options, runUtil, remoteFilePath, type, null);
    }

    /**
     * Fetch a remote file from a nested instance and log it.
     *
     * @param logger The {@link ITestLogger} where to log the file.
     * @param gceAvd The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param remoteFilePath The remote path where to find the file.
     * @param type the {@link LogDataType} of the logged file.
     * @param baseName The base name to use to log the file. If null the actual file name will be
     *     used.
     */
    public static void logNestedRemoteFile(
            ITestLogger logger,
            GceAvdInfo gceAvd,
            TestDeviceOptions options,
            IRunUtil runUtil,
            String remoteFilePath,
            LogDataType type,
            String baseName) {
        File remoteFile =
                RemoteFileUtil.fetchRemoteFile(
                        gceAvd, options, runUtil, REMOTE_FILE_OP_TIMEOUT, remoteFilePath);
        if (remoteFile != null) {
            // If we happened to fetch a directory, log all the subfiles
            logFile(remoteFile, baseName, logger, type);
        }
    }

    private static void logFile(
            File remoteFile, String baseName, ITestLogger logger, LogDataType type) {
        if (remoteFile.isDirectory()) {
            for (File f : remoteFile.listFiles()) {
                logFile(f, null, logger, type);
            }
        } else {
            try (InputStreamSource remoteFileStream = new FileInputStreamSource(remoteFile, true)) {
                String name = baseName;
                if (name == null) {
                    name = remoteFile.getName();
                }
                logger.testLog(name, type, remoteFileStream);
            }
        }
    }

    /**
     * Execute the remote command via ssh on an instance.
     *
     * @param gceAvd The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param timeoutMs The timeout in millisecond for the command. 0 means no timeout.
     * @param command The remote command to execute.
     * @return {@link CommandResult} containing the result of the execution.
     */
    public static CommandResult remoteSshCommandExecution(
            GceAvdInfo gceAvd,
            TestDeviceOptions options,
            IRunUtil runUtil,
            long timeoutMs,
            String... command) {
        return RemoteSshUtil.remoteSshCommandExec(gceAvd, options, runUtil, timeoutMs, command);
    }

    private static String remoteSshCommandExec(
            GceAvdInfo gceAvd, TestDeviceOptions options, IRunUtil runUtil, String... command) {
        CommandResult res =
                remoteSshCommandExecution(gceAvd, options, runUtil, BUGREPORT_TIMEOUT, command);
        // We attempt to get a clean output from our command
        String output = res.getStdout().trim();
        if (!CommandStatus.SUCCESS.equals(res.getStatus())) {
            CLog.e("issue when attempting to execute '%s':", Arrays.asList(command));
            CLog.e("Stderr: %s", res.getStderr());
        } else if (output.isEmpty()) {
            CLog.e("Stdout from '%s' was empty", Arrays.asList(command));
            CLog.e("Stderr: %s", res.getStderr());
        }
        return output;
    }

    /**
     * Reads the current content of the Gce Avd instance serial log.
     *
     * @param infos The {@link GceAvdInfo} describing the instance.
     * @param avdConfigFile the avd config file
     * @param jsonKeyFile the service account json key file.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @return The serial log output or null if something goes wrong.
     */
    public static String getInstanceSerialLog(
            GceAvdInfo infos, File avdConfigFile, File jsonKeyFile, IRunUtil runUtil) {
        AcloudConfigParser config = AcloudConfigParser.parseConfig(avdConfigFile);
        if (config == null) {
            CLog.e("Failed to parse our acloud config.");
            return null;
        }
        if (infos == null) {
            return null;
        }
        try {
            Credential credential = createCredential(config, jsonKeyFile);
            String project = config.getValueForKey(AcloudKeys.PROJECT);
            String zone = config.getValueForKey(AcloudKeys.ZONE);
            String instanceName = infos.instanceName();
            Compute compute =
                    new Compute.Builder(
                                    GoogleNetHttpTransport.newTrustedTransport(),
                                    JSON_FACTORY,
                                    null)
                            .setApplicationName(project)
                            .setHttpRequestInitializer(credential)
                            .build();
            GetSerialPortOutput outputPort =
                    compute.instances().getSerialPortOutput(project, zone, instanceName);
            SerialPortOutput output = outputPort.execute();
            return output.getContents();
        } catch (GeneralSecurityException | IOException e) {
            CLog.e(e);
            return null;
        }
    }

    private static Credential createCredential(AcloudConfigParser config, File jsonKeyFile)
            throws GeneralSecurityException, IOException {
        if (jsonKeyFile != null) {
            return GoogleApiClientUtil.createCredentialFromJsonKeyFile(jsonKeyFile, SCOPES);
        } else if (config.getValueForKey(AcloudKeys.SERVICE_ACCOUNT_JSON_PRIVATE_KEY) != null) {
            jsonKeyFile =
                    new File(config.getValueForKey(AcloudKeys.SERVICE_ACCOUNT_JSON_PRIVATE_KEY));
            return GoogleApiClientUtil.createCredentialFromJsonKeyFile(jsonKeyFile, SCOPES);
        } else {
            String serviceAccount = config.getValueForKey(AcloudKeys.SERVICE_ACCOUNT_NAME);
            String serviceKey = config.getValueForKey(AcloudKeys.SERVICE_ACCOUNT_PRIVATE_KEY);
            return GoogleApiClientUtil.createCredentialFromP12File(
                    serviceAccount, new File(serviceKey), SCOPES);
        }
    }

    public void cleanUp() {
        // Clean up logs file if any was created.
    }

    /** Returns the instance of the {@link IRunUtil}. */
    @VisibleForTesting
    IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }

    /**
     * Log the serial output of a device described by {@link GceAvdInfo}.
     *
     * @param infos The {@link GceAvdInfo} describing the instance.
     * @param logger The {@link ITestLogger} where to log the serial log.
     */
    public void logSerialOutput(GceAvdInfo infos, ITestLogger logger) {
        String output =
                GceManager.getInstanceSerialLog(
                        infos,
                        getAvdConfigFile(),
                        getTestDeviceOptions().getServiceAccountJsonKeyFile(),
                        getRunUtil());
        if (output == null) {
            CLog.w("Failed to collect the instance serial logs.");
            return;
        }
        try (ByteArrayInputStreamSource source =
                new ByteArrayInputStreamSource(output.getBytes())) {
            logger.testLog("gce_full_serial_log", LogDataType.TEXT, source);
        }
    }

    /** Log the information related to the stable host image used. */
    public void logStableHostImageInfos(IBuildInfo build) {
        AcloudConfigParser config = AcloudConfigParser.parseConfig(getAvdConfigFile());
        if (config == null) {
            CLog.e("Failed to parse our acloud config.");
            return;
        }
        if (build == null) {
            return;
        }
        if (config.getValueForKey(AcloudKeys.STABLE_HOST_IMAGE_NAME) != null) {
            build.addBuildAttribute(
                    AcloudKeys.STABLE_HOST_IMAGE_NAME.toString(),
                    config.getValueForKey(AcloudKeys.STABLE_HOST_IMAGE_NAME));
        }
        if (config.getValueForKey(AcloudKeys.STABLE_HOST_IMAGE_PROJECT) != null) {
            build.addBuildAttribute(
                    AcloudKeys.STABLE_HOST_IMAGE_PROJECT.toString(),
                    config.getValueForKey(AcloudKeys.STABLE_HOST_IMAGE_PROJECT));
        }
    }

    /**
     * Returns the {@link TestDeviceOptions} associated with the device that the gce manager was
     * initialized with.
     */
    private TestDeviceOptions getTestDeviceOptions() {
        return mDeviceOptions;
    }

    @VisibleForTesting
    File getAvdConfigFile() {
        return getTestDeviceOptions().getAvdConfigFile();
    }

    /**
     * Thread that helps cleaning the copied config when the process is done. This ensures acloud is
     * not missing its config until its done.
     */
    private static class AcloudDeleteCleaner extends Thread {
        private Process mProcess;
        private File mConfigFile;

        public AcloudDeleteCleaner(Process p, File config) {
            setDaemon(true);
            setName("acloud-delete-cleaner");
            mProcess = p;
            mConfigFile = config;
        }

        @Override
        public void run() {
            try {
                mProcess.waitFor();
            } catch (InterruptedException e) {
                CLog.e(e);
            }
            FileUtil.deleteFile(mConfigFile);
        }
    }
}
