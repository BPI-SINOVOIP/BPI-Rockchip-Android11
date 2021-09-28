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
package com.android.uicd.tests;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.RunUtil;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * The class enables user to run their pre-recorded UICD tests on tradefed. Go to
 * https://github.com/google/android-uiconductor/releases/tag/v0.1.1 to download the uicd_cli.tar.gz
 * and extract the jar and apks required for the tests. Please look at the sample xmls in
 * res/config/uicd to configure your tests.
 */
public class UiConductorTest implements IRemoteTest {

    @Option(
        name = "uicd-cli-jar",
        description = "The cli jar that runs the user provided tests in commandline",
        importance = Importance.IF_UNSET
    )
    private File cliJar;

    @Option(
        name = "commandline-action-executable",
        description =
                "the filesystem path of the binaries that are ran through command line actions on UICD. Can be repeated.",
        importance = Importance.IF_UNSET
    )
    private Collection<File> binaries = new ArrayList<File>();

    @Option(
        name = "global-variables",
        description = "Global variable (uicd_key1=value1,uicd_key2=value2)",
        importance = Importance.ALWAYS
    )
    private MultiMap<String, String> globalVariables = new MultiMap<>();

    @Option(
        name = "play-mode",
        description = "Play Mode (SINGLE|MULTIDEVICE|PLAYALL).",
        importance = Importance.ALWAYS
    )
    private String playMode = "SINGLE";

    @Option(name = "test-name", description = "Name of the test.", importance = Importance.ALWAYS)
    private String testName = "Your test results are here";

    // Same key can have multiple test files because global-variables can be referenced using the
    // that particular key and shared across different tests.
    // Refer res/config/uicd/uiconductor-globalvariable-sample.xml for more information.
    @Option(
        name = "uicd-test",
        description =
                "the filesystem path of the json test files or directory of multiple json test files that needs to be run on devices. Can be repeated.",
        importance = Importance.IF_UNSET
    )
    private MultiMap<String, File> uicdTest = new MultiMap<>();

    @Option(
        name = "test-timeout",
        description = "Time out for each test.",
        importance = Importance.IF_UNSET
    )
    private int testTimeout = 1800000;

    private static final String BINARY_RELATIVE_PATH = "binary";

    private static final String OUTPUT_RELATIVE_PATH = "output";

    private static final String TESTS_RELATIVE_PATH = "tests";

    private static final String RESULTS_RELATIVE_PATH = "result";

    private static final String OPTION_SYMBOL = "-";
    private static final String INPUT_OPTION_SHORT_NAME = "i";
    private static final String OUTPUT_OPTION_SHORT_NAME = "o";
    private static final String DEVICES_OPTION_SHORT_NAME = "d";
    private static final String MODE_OPTION_SHORT_NAME = "m";
    private static final String GLOBAL_VARIABLE_OPTION_SHORT_NAME = "g";

    private static final String CHILDRENRESULT_ATTRIBUTE = "childrenResult";
    private static final String PLAYSTATUS_ATTRIBUTE = "playStatus";
    private static final String VALIDATIONDETAILS_ATTRIBUTE = "validationDetails";

    private static final String EXECUTABLE = "u+x";

    private static String baseFilePath = System.getenv("HOME") + "/tmp/uicd-on-tf";

    Map<ITestDevice, IBuildInfo> deviceInfos;

    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        deviceInfos = testInfo.getContext().getDeviceBuildMap();
        CLog.i("Starting the UIConductor tests:\n");
        String runId = UUID.randomUUID().toString();
        baseFilePath = Paths.get(baseFilePath, runId).toString();
        String jarFileDir = Paths.get(baseFilePath, BINARY_RELATIVE_PATH).toString();
        String testFilesDir = Paths.get(baseFilePath, TESTS_RELATIVE_PATH).toString();
        String binaryFilesDir = Paths.get(baseFilePath).toString();
        File jarFile;
        MultiMap<String, File> copiedTestFileMap = new MultiMap<>();
        if (cliJar == null || !cliJar.exists()) {
            CLog.e("Unable to fetch provided binary.\n");
            return;
        }
        try {
            jarFile = copyFile(cliJar.getAbsolutePath(), jarFileDir);
            FileUtil.chmod(jarFile, EXECUTABLE);

            for (Map.Entry<String, File> testFileOrDirEntry : uicdTest.entries()) {
                copiedTestFileMap.putAll(
                        copyFile(
                                testFileOrDirEntry.getKey(),
                                testFileOrDirEntry.getValue().getAbsolutePath(),
                                testFilesDir));
            }

            for (File binaryFile : binaries) {
                File binary = copyFile(binaryFile.getAbsolutePath(), binaryFilesDir);
                FileUtil.chmod(binary, EXECUTABLE);
            }
        } catch (IOException ex) {
            throw new RuntimeException(ex);
        }

        RunUtil rUtil = new RunUtil();
        rUtil.setWorkingDir(new File(baseFilePath));
        long runStartTime = System.currentTimeMillis();
        listener.testRunStarted(testName, copiedTestFileMap.values().size());
        for (Map.Entry<String, File> testFileEntry : copiedTestFileMap.entries()) {
            runTest(
                    listener,
                    rUtil,
                    jarFile,
                    testFileEntry.getKey(),
                    testFileEntry.getValue().getName());
        }

        listener.testRunEnded(
                System.currentTimeMillis() - runStartTime, new HashMap<String, String>());
        FileUtil.recursiveDelete(new File(baseFilePath));
        CLog.i("Finishing the ui conductor tests\n");
    }

    public void runTest(
            ITestInvocationListener listener,
            RunUtil rUtil,
            File jarFile,
            String key,
            String testFileName) {
        TestDescription testDesc =
                new TestDescription(this.getClass().getSimpleName(), testFileName);
        listener.testStarted(testDesc, System.currentTimeMillis());

        String testId = UUID.randomUUID().toString();
        CommandResult cmndRes =
                rUtil.runTimedCmd(testTimeout, getCommand(jarFile, testFileName, testId, key));
        logInfo(testId, "STD", cmndRes.getStdout());
        logInfo(testId, "ERR", cmndRes.getStderr());

        File resultsFile =
                new File(
                        Paths.get(
                                        baseFilePath,
                                        OUTPUT_RELATIVE_PATH,
                                        testId,
                                        RESULTS_RELATIVE_PATH,
                                        "action_execution_result")
                                .toString());

        if (resultsFile.exists()) {
            try {
                String content = FileUtil.readStringFromFile(resultsFile);
                JSONObject result = new JSONObject(content);
                List<String> errors = new ArrayList<>();
                errors = parseResult(errors, result);
                if (!errors.isEmpty()) {
                    listener.testFailed(testDesc, errors.get(0));
                    CLog.i("Test %s failed due to following errors: \n", testDesc.getTestName());
                    for (String error : errors) {
                        CLog.i(error + "\n");
                    }
                }
            } catch (IOException | JSONException e) {
                CLog.e(e);
            }
            String testResultFileName = testFileName + "_action_execution_result";
            try (InputStreamSource iSSource = new FileInputStreamSource(resultsFile)) {
                listener.testLog(testResultFileName, LogDataType.TEXT, iSSource);
            }
        }
        listener.testEnded(testDesc, System.currentTimeMillis(), new HashMap<String, String>());
    }

    private void logInfo(String testId, String cmdOutputType, String content) {
        CLog.i(
                "==========================="
                        + cmdOutputType
                        + " logs for "
                        + testId
                        + " starts===========================\n");
        CLog.i(content);
        CLog.i(
                "==========================="
                        + cmdOutputType
                        + " logs for "
                        + testId
                        + " ends===========================\n");
    }

    private List<String> parseResult(List<String> errors, JSONObject result) throws JSONException {

        if (result != null) {
            if (result.has(CHILDRENRESULT_ATTRIBUTE)) {
                JSONArray childResults = result.getJSONArray(CHILDRENRESULT_ATTRIBUTE);
                for (int i = 0; i < childResults.length(); i++) {
                    errors = parseResult(errors, childResults.getJSONObject(i));
                }
            }

            if (result.has(PLAYSTATUS_ATTRIBUTE)
                    && result.getString(PLAYSTATUS_ATTRIBUTE).equalsIgnoreCase("FAIL")) {
                if (result.has(VALIDATIONDETAILS_ATTRIBUTE)) {
                    errors.add(result.getString(VALIDATIONDETAILS_ATTRIBUTE));
                }
            }
        }
        return errors;
    }

    private File copyFile(String srcFilePath, String destDirPath) throws IOException {
        File srcFile = new File(srcFilePath);
        File destDir = new File(destDirPath);
        if (srcFile.isDirectory()) {
            for (File file : srcFile.listFiles()) {
                copyFile(file.getAbsolutePath(), Paths.get(destDirPath, file.getName()).toString());
            }
        }
        if (!destDir.isDirectory() && !destDir.mkdirs()) {
            throw new IOException(
                    String.format("Could not create directory %s", destDir.getAbsolutePath()));
        }
        File destFile = new File(Paths.get(destDir.toString(), srcFile.getName()).toString());
        FileUtil.copyFile(srcFile, destFile);
        return destFile;
    }

    // copy file to destDirPath while maintaining a map of key that refers to that src file
    private MultiMap<String, File> copyFile(String key, String srcFilePath, String destDirPath)
            throws IOException {
        MultiMap<String, File> copiedTestFileMap = new MultiMap<>();
        File srcFile = new File(srcFilePath);
        File destDir = new File(destDirPath);
        if (srcFile.isDirectory()) {
            for (File file : srcFile.listFiles()) {
                copiedTestFileMap.putAll(
                        copyFile(
                                key,
                                file.getAbsolutePath(),
                                Paths.get(destDirPath, file.getName()).toString()));
            }
        }
        if (!destDir.isDirectory() && !destDir.mkdirs()) {
            throw new IOException(
                    String.format("Could not create directory %s", destDir.getAbsolutePath()));
        }
        if (srcFile.isFile()) {
            File destFile = new File(Paths.get(destDir.toString(), srcFile.getName()).toString());
            FileUtil.copyFile(srcFile, destFile);
            copiedTestFileMap.put(key, destFile);
        }
        return copiedTestFileMap;
    }

    private String getTestFilesArgsForUicdBin(String testFilesDir, String filename) {
        return (!testFilesDir.isEmpty() && !filename.isEmpty())
                ? Paths.get(testFilesDir, filename).toString()
                : "";
    }

    private String getOutFilesArgsForUicdBin(String outFilesDir) {
        return !outFilesDir.isEmpty() ? outFilesDir : "";
    }

    private String getPlaymodeArgForUicdBin() {
        return !playMode.isEmpty() ? playMode : "";
    }

    private String getDevIdsArgsForUicdBin() {
        List<String> devIds = new ArrayList<>();
        for (ITestDevice device : deviceInfos.keySet()) {
            devIds.add(device.getSerialNumber());
        }
        return String.join(",", devIds);
    }

    private String[] getCommand(File jarFile, String testFileName, String testId, String key) {
        List<String> command = new ArrayList<>();
        command.add("java");
        command.add("-jar");
        command.add(jarFile.getAbsolutePath());
        if (!getTestFilesArgsForUicdBin(TESTS_RELATIVE_PATH, testFileName).isEmpty()) {
            command.add(OPTION_SYMBOL + INPUT_OPTION_SHORT_NAME);
            command.add(getTestFilesArgsForUicdBin(TESTS_RELATIVE_PATH, testFileName));
        }
        if (!getOutFilesArgsForUicdBin(OUTPUT_RELATIVE_PATH + "/" + testId).isEmpty()) {
            command.add(OPTION_SYMBOL + OUTPUT_OPTION_SHORT_NAME);
            command.add(getOutFilesArgsForUicdBin(OUTPUT_RELATIVE_PATH + "/" + testId));
        }
        if (!getPlaymodeArgForUicdBin().isEmpty()) {
            command.add(OPTION_SYMBOL + MODE_OPTION_SHORT_NAME);
            command.add(getPlaymodeArgForUicdBin());
        }
        if (!getDevIdsArgsForUicdBin().isEmpty()) {
            command.add(OPTION_SYMBOL + DEVICES_OPTION_SHORT_NAME);
            command.add(getDevIdsArgsForUicdBin());
        }
        if (globalVariables.containsKey(key)) {
            command.add(OPTION_SYMBOL + GLOBAL_VARIABLE_OPTION_SHORT_NAME);
            command.add(String.join(",", globalVariables.get(key)));
        }
        return command.toArray(new String[] {});
    }
}
