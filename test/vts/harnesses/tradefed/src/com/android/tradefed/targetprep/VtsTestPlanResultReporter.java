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

package com.android.tradefed.targetprep;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.VtsDashboardUtil;
import com.android.tradefed.util.VtsVendorConfigFileUtil;
import com.android.vts.proto.VtsComponentSpecificationMessage.ComponentSpecificationMessage;
import com.android.vts.proto.VtsComponentSpecificationMessage.FunctionSpecificationMessage;
import com.android.vts.proto.VtsReportMessage.ApiCoverageReportMessage;
import com.android.vts.proto.VtsReportMessage.DashboardPostMessage;
import com.android.vts.proto.VtsReportMessage.HalInterfaceMessage;
import com.android.vts.proto.VtsReportMessage.TestPlanReportMessage;
import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.nio.file.Files;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
/**
 * Uploads the VTS test plan execution result to the web DB using a RESTful API and
 * an OAuth2 credential kept in a json file.
 */
@OptionClass(alias = "vts-plan-result")
public class VtsTestPlanResultReporter implements ITargetPreparer, ITargetCleaner {
    private static final String PLUS_ME = "https://www.googleapis.com/auth/plus.me";
    private static final String TEST_PLAN_EXECUTION_RESULT = "vts-test-plan-execution-result";
    private static final String TEST_PLAN_REPORT_FILE = "TEST_PLAN_REPORT_FILE";
    private static final String TEST_PLAN_REPORT_FILE_NAME = "status.json";
    private static VtsVendorConfigFileUtil configReader = null;
    private static VtsDashboardUtil dashboardUtil = null;
    private static final int BASE_TIMEOUT_MSECS = 1000 * 60;
    IRunUtil mRunUtil = new RunUtil();

    @Option(name = "plan-name", description = "The plan name") private String mPlanName = null;

    @Option(name = "file-path",
            description = "The path of a VTS vendor config file (format: json).")
    private String mVendorConfigFilePath = null;

    @Option(name = "default-type",
            description = "The default config file type, e.g., `prod` or `staging`.")
    private String mDefaultType = VtsVendorConfigFileUtil.VENDOR_TEST_CONFIG_DEFAULT_TYPE;

    private File mStatusDir;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo) {
        try {
            mStatusDir = FileUtil.createTempDir(TEST_PLAN_EXECUTION_RESULT);
            if (mStatusDir != null) {
                File statusFile = new File(mStatusDir, TEST_PLAN_REPORT_FILE_NAME);
                buildInfo.setFile(TEST_PLAN_REPORT_FILE, statusFile, buildInfo.getBuildId());
            }
        } catch (IOException ex) {
            CLog.e("Can't create a temp dir to store test execution results.");
            return;
        }

        // Use IBuildInfo to pass the uesd vendor config information to the rest of workflow.
        buildInfo.addBuildAttribute(
                VtsVendorConfigFileUtil.KEY_VENDOR_TEST_CONFIG_DEFAULT_TYPE, mDefaultType);
        if (mVendorConfigFilePath != null) {
            buildInfo.addBuildAttribute(VtsVendorConfigFileUtil.KEY_VENDOR_TEST_CONFIG_FILE_PATH,
                    mVendorConfigFilePath);
        }
        configReader = new VtsVendorConfigFileUtil();
        configReader.LoadVendorConfig(buildInfo);
        dashboardUtil = new VtsDashboardUtil(configReader);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void tearDown(ITestDevice device, IBuildInfo buildInfo, Throwable e) {
        File reportFile = buildInfo.getFile(TEST_PLAN_REPORT_FILE);
        if (reportFile == null) {
            CLog.e("Couldn't find %s to post results. Skipping tearDown.",
                    TEST_PLAN_REPORT_FILE_NAME);
            // Just in case clean up the directory.
            FileUtil.recursiveDelete(mStatusDir);
            return;
        }
        String repotFilePath = reportFile.getAbsolutePath();
        DashboardPostMessage.Builder postMessage = DashboardPostMessage.newBuilder();
        TestPlanReportMessage.Builder testPlanMessage = TestPlanReportMessage.newBuilder();
        testPlanMessage.setTestPlanName(mPlanName);
        boolean found = false;
        try (BufferedReader br = new BufferedReader(new FileReader(repotFilePath))) {
            String currentLine;
            while ((currentLine = br.readLine()) != null) {
                String[] lineWords = currentLine.split("\\s+");
                if (lineWords.length != 2) {
                    CLog.e(String.format("Invalid keys %s", currentLine));
                    continue;
                }
                testPlanMessage.addTestModuleName(lineWords[0]);
                testPlanMessage.addTestModuleStartTimestamp(Long.parseLong(lineWords[1]));
                found = true;
            }
        } catch (IOException ex) {
            CLog.d("Can't read the test plan result file %s", repotFilePath);
            return;
        }
        // For profiling test plan, add the baseline API report.
        if (mPlanName.endsWith("profiling")) {
            addApiReportToTestPlanMessage(device, buildInfo, testPlanMessage);
        }
        CLog.d("Delete report dir %s", mStatusDir.getAbsolutePath());
        FileUtil.recursiveDelete(mStatusDir);
        postMessage.addTestPlanReport(testPlanMessage.build());
        if (found) {
            dashboardUtil.Upload(postMessage);
        }
    }

    /**
     * Method to parse all packaged .vts specs and get the list of all defined APIs
     *
     * @param device the target device.
     * @param buildInfo the build info of the target device.
     * @param testPlanMessage testPlanMessage builder for updating the report message.
     */
    private void addApiReportToTestPlanMessage(ITestDevice device, IBuildInfo buildInfo,
            TestPlanReportMessage.Builder testPlanMessage) {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(buildInfo);
        Map<String, String> hal_release_map = new HashMap<String, String>();
        File test_dir;
        try {
            test_dir = buildHelper.getTestsDir();
        } catch (FileNotFoundException e) {
            CLog.e("Failed to get test dir, error: " + e.getMessage());
            return;
        }
        // Get the interface --> release_version mapping.
        File hal_release_path = new File(test_dir, "DATA/etc/hidl_hals_for_release.json");
        hal_release_map = creatHalRelaseMap(hal_release_path.getAbsolutePath());
        if (hal_release_map == null) {
            CLog.e("Failed to create hal releas map");
            return;
        }

        List<File> specFiles = new ArrayList<File>();
        try {
            File specDir = new File(test_dir, "spec/hardware/interfaces");
            Files.walk(specDir.toPath())
                    .filter(p
                            -> (p.toString().endsWith(".vts")
                                    && !p.toString().endsWith("types.vts")))
                    .forEach(p -> specFiles.add(p.toFile()));
        } catch (IOException e) {
            CLog.e("Failed to get spec files, error: " + e.getMessage());
            return;
        }

        for (File specFile : specFiles) {
            try {
                InputStreamReader reader =
                        new InputStreamReader(new FileInputStream(specFile), "ASCII");
                ComponentSpecificationMessage.Builder spec_builder =
                        ComponentSpecificationMessage.newBuilder();
                TextFormat.merge(reader, spec_builder);
                ComponentSpecificationMessage spec = spec_builder.build();
                String full_api_name = spec.getPackage().toString("ASCII") + '@'
                        + Integer.toString(spec.getComponentTypeVersionMajor()) + '.'
                        + Integer.toString(spec.getComponentTypeVersionMinor())
                        + "::" + spec.getComponentName().toString("ASCII");

                HalInterfaceMessage.Builder halInterfaceMsg = HalInterfaceMessage.newBuilder();
                halInterfaceMsg.setHalPackageName(spec.getPackage());
                halInterfaceMsg.setHalVersionMajor(spec.getComponentTypeVersionMajor());
                halInterfaceMsg.setHalVersionMinor(spec.getComponentTypeVersionMinor());
                halInterfaceMsg.setHalInterfaceName(spec.getComponentName());
                String release_level = hal_release_map.get(full_api_name);
                if (release_level == null) {
                    CLog.w("could not get the release level for %s", full_api_name);
                    continue;
                }
                halInterfaceMsg.setHalReleaseLevel(ByteString.copyFrom(release_level.getBytes()));
                ApiCoverageReportMessage.Builder apiReport = ApiCoverageReportMessage.newBuilder();
                apiReport.setHalInterface(halInterfaceMsg.build());
                for (FunctionSpecificationMessage api : spec.getInterface().getApiList()) {
                    if (!api.getIsInherited())
                        apiReport.addHalApi(api.getName());
                }
                testPlanMessage.addHalApiReport(apiReport.build());
            } catch (Exception e) {
                CLog.i("Failed to parse spec file: " + specFile.getAbsolutePath()
                        + " error: " + e.getMessage());
                continue;
            }
        }
    }

    /**
     * Method to parse the hal release file and create a mapping between HAL interfaces and their
     * corresponding release version.
     *
     * @param hal_release_file_path path to the hal release file.
     * @return a map for HAL interfaces and their corresponding release version.
     */
    private Map<String, String> creatHalRelaseMap(String hal_release_file_path) {
        Map<String, String> hal_release_map = new HashMap<String, String>();
        try {
            InputStream hal_release = new FileInputStream(hal_release_file_path);
            if (hal_release == null) {
                CLog.e("hal_release file %s does not exist", hal_release_file_path);
                return hal_release_map;
            }
            String content = StreamUtil.getStringFromStream(hal_release);
            JSONObject hal_release_json = new JSONObject(content);
            Iterator<String> keys = hal_release_json.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                JSONArray value = (JSONArray) hal_release_json.get(key);
                for (int i = 0; i < value.length(); i++) {
                    String type = (String) value.get(i);
                    hal_release_map.put(type, key);
                }
            }
        } catch (IOException | JSONException e) {
            CLog.e("Failed to parse hal release file, error: " + e.getMessage());
            return null;
        }
        return hal_release_map;
    }
}
