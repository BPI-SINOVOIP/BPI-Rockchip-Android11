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
package com.android.tradefed.testtype.mobly;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.targetprep.adb.AdbStopServerPreparer;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.yaml.snakeyaml.Yaml;

/** Host test meant to run a mobly python binary file from the Android Build system (Soong) */
@OptionClass(alias = "mobly-host")
public class MoblyBinaryHostTest
        implements IRemoteTest, IDeviceTest, IBuildReceiver, IInvocationContextReceiver {

    private static final String ANDROID_SERIAL_VAR = "ANDROID_SERIAL";
    private static final String PATH_VAR = "PATH";
    private static final long PATH_TIMEOUT_MS = 60000L;
    private static final String MOBLY_TEST_SUMMARY = "test_summary.yaml";
    private static final String MOBLY_TEST_SUMMARY_XML = "converted_test_summary.xml";

    // TODO(b/159366744): merge this and next options.
    @Option(name = "par-file-name", description = "The binary names inside the build info to run.")
    private Set<String> mBinaryNames = new HashSet<>();

    @Option(
            name = "python-binaries",
            description = "The full path to a runnable python binary. Can be repeated.")
    private Set<File> mBinaries = new HashSet<>();

    @Option(
            name = "test-timeout",
            description = "Timeout for a single par file to terminate.",
            isTimeVal = true)
    private long mTestTimeout = 20 * 1000L;

    @Option(
            name = "inject-android-serial",
            description = "Whether or not to pass a ANDROID_SERIAL variable to the process.")
    private boolean mInjectAndroidSerialVar = true;

    @Option(
            name = "python-options",
            description = "Option string to be passed to the binary when running")
    private List<String> mTestOptions = new ArrayList<>();

    @Option(
            name = "mobly-config-file",
            description =
                    "Mobly config file absolute path."
                            + "If set, will append '--config=<config file path>' to the command for "
                            + "running binary.")
    private File mConfigFile;

    @Option(
            name = "test-bed",
            description =
                    "Name of the test bed to run the tests."
                            + "If set, will append '--test_bed=<test bed name>' to the command for "
                            + "running binary.")
    private String mTestBed;

    private ITestDevice mDevice;
    private IBuildInfo mBuildInfo;
    private IInvocationContext mContext;
    private File mLogDir;

    private IRunUtil mRunUtil;

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildInfo = buildInfo;
    }

    @Override
    public void setInvocationContext(IInvocationContext invocationContext) {
        mContext = invocationContext;
    }

    @Override
    public final void run(ITestInvocationListener listener) {
        List<File> parFilesList = findParFiles();
        for (File parFile : parFilesList) {
            // TODO(b/159365341): add a failure reporting for nonexistent binary.
            if (!parFile.exists()) {
                CLog.d(
                        "ignoring %s which doesn't look like a test file.",
                        parFile.getAbsolutePath());
                continue;
            }
            parFile.setExecutable(true);
            try {
                runSingleParFile(listener, parFile.getAbsolutePath());
                processTestResults(listener, parFile.getName());
            } finally {
                reportLogs(getLogDir(), listener);
            }
        }
    }

    private List<File> findParFiles() {
        File testsDir = null;
        if (mBuildInfo instanceof IDeviceBuildInfo) {
            testsDir = ((IDeviceBuildInfo) mBuildInfo).getTestsDir();
        }
        // TODO(b/159369297): make naming and log message more "mobly".
        List<File> files = new ArrayList<>();
        for (String binaryName : mBinaryNames) {
            File res = null;
            // search tests dir
            if (testsDir != null) {
                res = FileUtil.findFile(testsDir, binaryName);
            }
            if (res == null) {
                throw new RuntimeException(
                        String.format("Couldn't find a par file %s", binaryName));
            }
            files.add(res);
        }
        files.addAll(mBinaries);
        return files;
    }

    private void runSingleParFile(ITestInvocationListener listener, String parFilePath) {
        if (mInjectAndroidSerialVar) {
            getRunUtil().setEnvVariable(ANDROID_SERIAL_VAR, getDevice().getSerialNumber());
        }
        updateAdb();
        if (mConfigFile != null) {
            updateConfigFile();
        }
        CommandResult result =
                getRunUtil().runTimedCmd(getTestTimeout(), buildCommandLineArray(parFilePath));
        if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
            CLog.e(
                    "Something went wrong when running the python binary:\nstdout: "
                            + "%s\nstderr:%s\nStatus:%s",
                    result.getStdout(), result.getStderr(), result.getStatus());
        }
    }

    private void updateAdb() {
        File updatedAdb = mBuildInfo.getFile(AdbStopServerPreparer.ADB_BINARY_KEY);
        if (updatedAdb == null) {
            String adbPath = getAdbPath();
            // Don't check if it's the adb on the $PATH
            if (!adbPath.equals("adb")) {
                updatedAdb = new File(adbPath);
                if (!updatedAdb.exists()) {
                    updatedAdb = null;
                }
            }
        }
        if (updatedAdb != null) {
            CLog.d("Testing with adb binary at: %s", updatedAdb);
            // If a special adb version is used, pass it to the PATH
            CommandResult pathResult =
                    getRunUtil()
                            .runTimedCmd(PATH_TIMEOUT_MS, "/bin/bash", "-c", "echo $" + PATH_VAR);
            if (!CommandStatus.SUCCESS.equals(pathResult.getStatus())) {
                throw new RuntimeException(
                        String.format(
                                "Failed to get the $PATH. status: %s, stdout: %s, stderr: %s",
                                pathResult.getStatus(),
                                pathResult.getStdout(),
                                pathResult.getStderr()));
            }
            // Include the directory of the adb on the PATH to be used.
            String path =
                    String.format(
                            "%s:%s",
                            updatedAdb.getParentFile().getAbsolutePath(),
                            pathResult.getStdout().trim());
            CLog.d("Using $PATH with updated adb: %s", path);
            getRunUtil().setEnvVariable(PATH_VAR, path);
            // Log the version of adb seen
            CommandResult versionRes = getRunUtil().runTimedCmd(PATH_TIMEOUT_MS, "adb", "version");
            CLog.d("%s", versionRes.getStdout());
            CLog.d("%s", versionRes.getStderr());
        }
    }

    private void processTestResults(ITestInvocationListener listener, String runName) {
        // Convert yaml test summary to xml.
        File yamlSummaryFile = FileUtil.findFile(getLogDir(), MOBLY_TEST_SUMMARY);
        if (yamlSummaryFile == null) {
            throw new RuntimeException(
                    String.format(
                            "Fail to find test summary file %s under directory %s",
                            MOBLY_TEST_SUMMARY, getLogDir()));
        }

        MoblyYamlResultParser parser = new MoblyYamlResultParser(listener, runName);
        InputStream inputStream = null;
        try {
            inputStream = new FileInputStream(yamlSummaryFile);
            processYamlTestResults(inputStream, parser);
        } catch (FileNotFoundException ex) {
            // TODO(b/159367088): report a test failure.
            CLog.e("Fail processing test results: ", ex);
        } finally {
            StreamUtil.close(inputStream);
        }
    }

    @VisibleForTesting
    protected void processYamlTestResults(InputStream inputStream, MoblyYamlResultParser parser) {
        try {
            parser.parse(inputStream);
        } catch (MoblyYamlResultHandlerFactory.InvalidResultTypeException
                | IllegalAccessException
                | InstantiationException ex) {
            // TODO(b/159367088): report a test failure.
            CLog.e("Failed to parse result file: %s", ex);
        }
    }

    private void updateConfigFile() {
        InputStream inputStream = null;
        FileWriter fileWriter = null;
        // TODO(b/159369745): clean up the tmp files created.
        File localConfigFile = new File(getLogDir(), "local_config.yaml");
        try {
            inputStream = new FileInputStream(mConfigFile);
            fileWriter = new FileWriter(localConfigFile);
            updateConfigFile(inputStream, fileWriter, getDevice().getSerialNumber());
            // Update config file to local version.
            mConfigFile = localConfigFile;
        } catch (IOException ex) {
            throw new RuntimeException("Exception in updating config file: %s", ex);
        } finally {
            StreamUtil.close(inputStream);
            StreamUtil.close(fileWriter);
        }
    }

    @VisibleForTesting
    protected void updateConfigFile(InputStream configInputStream, Writer writer, String serial) {
        Yaml yaml = new Yaml();
        Map<String, Object> configMap = (Map<String, Object>) yaml.load(configInputStream);
        CLog.d("Loaded yaml config: \n%s", configMap);
        List<Object> testBedList = (List<Object>) configMap.get("TestBeds");
        Map<String, Object> targetTb = null;
        if (getTestBed() == null) {
            targetTb = (Map<String, Object>) testBedList.get(0);
        } else {
            for (Object tb : testBedList) {
                Map<String, Object> tbMap = (Map<String, Object>) tb;
                String tbName = (String) tbMap.get("Name");
                if (tbName.equalsIgnoreCase(getTestBed())) {
                    targetTb = tbMap;
                    break;
                }
            }
        }
        if (targetTb == null) {
            throw new RuntimeException(
                    String.format("Fail to find specified test bed: %s.", getTestBed()));
        }
        Map<String, Object> controllerMap = (Map<String, Object>) targetTb.get("Controllers");
        List<Object> androidDeviceList = (List<Object>) controllerMap.get("AndroidDevice");
        // Inject serial for the first device
        Map<String, Object> deviceMap = (Map<String, Object>) androidDeviceList.get(0);
        deviceMap.put("serial", serial);
        yaml.dump(configMap, writer);
    }

    private File getLogDir() {
        if (mLogDir == null) {
            try {
                mLogDir = FileUtil.createTempDir("host_tmp_mobly");
            } catch (IOException ex) {
                CLog.e("Failed to create temp dir with prefix host_tmp_mobly: %s", ex);
            }
            CLog.d("Mobly log path: %s", mLogDir.getAbsolutePath());
        }
        return mLogDir;
    }

    @VisibleForTesting
    String getLogDirAbsolutePath() {
        return getLogDir().getAbsolutePath();
    }

    @VisibleForTesting
    String getTestBed() {
        return mTestBed;
    }

    @VisibleForTesting
    protected String[] buildCommandLineArray(String filePath) {
        List<String> commandLine = new ArrayList<>();
        commandLine.add(filePath);
        commandLine.add("--");
        if (getConfigPath() != null) {
            commandLine.add("--config=" + getConfigPath());
        }
        if (getTestBed() != null) {
            commandLine.add("--test_bed=" + getTestBed());
        }
        commandLine.add("--device_serial=" + getDevice().getSerialNumber());
        commandLine.add("--log_path=" + getLogDirAbsolutePath());
        // Add all the other options
        commandLine.addAll(getTestOptions());
        return commandLine.toArray(new String[0]);
    }

    @VisibleForTesting
    protected void reportLogs(File logDir, ITestInvocationListener listener) {
        for (File subFile : logDir.listFiles()) {
            if (subFile.isDirectory()) {
                reportLogs(subFile, listener);
            } else {
                InputStreamSource dataStream = null;
                try {
                    dataStream = new FileInputStreamSource(subFile, true);
                    listener.testLog(subFile.getName(), LogDataType.TEXT, dataStream);
                } finally {
                    StreamUtil.close(dataStream);
                }
            }
        }
        FileUtil.deleteFile(logDir);
    }

    @VisibleForTesting
    String getConfigPath() {
        if (mConfigFile != null) {
            return mConfigFile.getAbsolutePath();
        }
        return null;
    }

    @VisibleForTesting
    List<String> getTestOptions() {
        return mTestOptions;
    }

    @VisibleForTesting
    IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = new RunUtil();
        }
        return mRunUtil;
    }

    @VisibleForTesting
    String getAdbPath() {
        return GlobalConfiguration.getDeviceManagerInstance().getAdbPath();
    }

    @VisibleForTesting
    long getTestTimeout() {
        return mTestTimeout;
    }
}
