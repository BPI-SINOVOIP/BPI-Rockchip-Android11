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
package com.android.tradefed.presubmit;

import static org.junit.Assert.fail;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.ConfigurationUtil;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationFactory;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.suite.ITestSuite;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil2;
import com.android.tradefed.util.testmapping.TestInfo;
import com.android.tradefed.util.testmapping.TestMapping;
import com.android.tradefed.util.testmapping.TestOption;

import com.google.common.base.Joiner;
import com.google.common.base.Strings;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import java.util.regex.Pattern;
import org.json.JSONObject;
import org.json.JSONArray;
import org.json.JSONException;
import org.junit.Assume;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Validation tests to run against the TEST_MAPPING files in tests_mappings.zip to ensure they
 * contains the essential suite settings and no conflict test options.
 *
 * <p>Do not add to UnitTests.java. This is meant to run standalone.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class TestMappingsValidation implements IBuildReceiver {

    // pattern used to identify java class names conforming to java naming conventions.
    private static final Pattern CLASS_OR_METHOD_REGEX =
            Pattern.compile(
                    "^([\\p{L}_$][\\p{L}\\p{N}_$]*\\.)*[\\p{Lu}_$][\\p{L}\\p{N}_$]*"
                            + "(#[\\p{L}_$][\\p{L}\\p{N}_$]*)?$");
    // pattern used to identify if this is regular expression with at least 1 '*' or '?'.
    private static final Pattern REGULAR_EXPRESSION = Pattern.compile("(\\?+)|(\\*+)");
    private static final String MODULE_INFO = "module-info.json";
    private static final String TEST_MAPPINGS_ZIP = "test_mappings.zip";
    private static final String INCLUDE_FILTER = "include-filter";
    private static final String EXCLUDE_FILTER = "exclude-filter";
    private static final String LOCAL_COMPATIBILITY_SUITES = "compatibility_suites";
    private static final String GENERAL_TESTS = "general-tests";
    private static final String DEVICE_TESTS = "device-tests";
    // Only Check the tests with group in presubmit or postsubmit.
    private static final Set<String> TEST_GROUPS_TO_VALIDATE =
            new HashSet<>(Arrays.asList("presubmit", "postsubmit"));

    private static final Set<String> MAINLINE_TEST_GROUPS_TO_VALIDATE =
            new HashSet<>(Arrays.asList("mainline-presubmit", "mainline-postsubmit"));

    // Check the mainline parameter configured in a test config must end with .apk, .apks, or .apex.
    private static final Set<String> MAINLINE_PARAMETERS_TO_VALIDATE =
            new HashSet<>(Arrays.asList(".apk", ".apks", ".apex"));

    private File testMappingsDir = null;
    private IConfigurationFactory mConfigFactory = null;
    private IDeviceBuildInfo deviceBuildInfo = null;
    private IBuildInfo mBuild;
    private JSONObject moduleInfo = null;
    private Map<String, Set<TestInfo>> allTests = null;

    /** Type of filters used in test options in TEST_MAPPING files. */
    enum Filters {
        // Test option is regular expression format.
        REGEX,
        // Test option is class/method format.
        CLASS_OR_METHOD,
        // Test option is package format.
        PACKAGE
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuild = buildInfo;
    }

    @Before
    public void setUp() throws IOException, JSONException {
        Assume.assumeTrue(mBuild instanceof IDeviceBuildInfo);
        mConfigFactory = ConfigurationFactory.getInstance();
        deviceBuildInfo = (IDeviceBuildInfo) mBuild;
        testMappingsDir =
                TestMapping.extractTestMappingsZip(deviceBuildInfo.getFile(TEST_MAPPINGS_ZIP));
        File file = deviceBuildInfo.getFile(MODULE_INFO);
        moduleInfo = new JSONObject(FileUtil.readStringFromFile(file));
        allTests = TestMapping.getAllTests(testMappingsDir);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(testMappingsDir);
    }

    /**
     * Test all the TEST_MAPPING files and make sure they are properly configured with
     * parameterized mainline modules.
     */
    @Test
    public void testValidTestMappingForParameterizedMainlineModules() {
        List<String> errors = new ArrayList<>();
        for (String testGroup : allTests.keySet()) {
            if (!MAINLINE_TEST_GROUPS_TO_VALIDATE.contains(testGroup)) {
                CLog.d("Check mainline group only, skip checking tests with group: %s", testGroup);
                continue;
            }
            for (TestInfo testInfo : allTests.get(testGroup)) {
                // If there are brackets existing in the name, that means it's for parameterized
                // mainline modules, so try to get the modules by indexing the brackets.
                String testName = testInfo.getName();
                int firstIndex = testName.indexOf("[");
                int lastIndex = testName.indexOf("]");
                if (firstIndex == -1 && lastIndex == -1) {
                    continue;
                }
                else if (firstIndex != -1 && lastIndex != -1) {
                    String mainlineParam = testName.substring(firstIndex+1, lastIndex);
                    String error =
                            validateMainlineModuleConfig(mainlineParam, testInfo.getSources());
                    if (!Strings.isNullOrEmpty(error)){
                        errors.add(error);
                    }
                }
                else {
                    errors.add(
                            String.format("Unmatched \"[]\" for \"%s\" configured in the %s. " +
                                "Parameter must contain [].", testName, testInfo.getSources()));
                }
            }
        }
        if (!errors.isEmpty()) {
            fail(String.format("Fail TEST_MAPPING check for parameterized mainline modules: \n%s",
                    Joiner.on("\n").join(errors)));
        }
    }

    /**
     * Test all the test config files and make sure they are properly configured with parameterized
     * mainline modules.
     */
    @Test
    public void testValidTestConfigForParameterizedMainlineModules() throws IOException {
        File configZip = deviceBuildInfo.getFile("general-tests_configs.zip");
        Assume.assumeTrue(configZip != null);
        List<String> errors = new ArrayList<>();
        List<String> testConfigs = new ArrayList<>();
        File testConfigDir = ZipUtil2.extractZipToTemp(configZip, "general-tests_configs");
        testConfigs.addAll(
                ConfigurationUtil.getConfigNamesFromDirs(null, Arrays.asList(testConfigDir)));
        for (String configName : testConfigs) {
            try {
                IConfiguration config =
                        mConfigFactory.createConfigurationFromArgs(new String[] {configName});
                List<String> params = config.getConfigurationDescription().getMetaData(
                        ITestSuite.MAINLINE_PARAMETER_KEY);
                if (params == null || params.isEmpty()) {
                    continue;
                }
                for (String param : params) {
                    String error = validateMainlineModuleConfig(param, Set.of(configName));
                    if (!Strings.isNullOrEmpty(error)){
                        errors.add(error);
                    }
                }
            } catch (ConfigurationException e) {
                errors.add(String.format("\t%s: %s", configName, e.getMessage()));
            }
        }

        if (!errors.isEmpty()) {
            fail(String.format("Fail Test config check for parameterized mainline module: \n%s",
                    Joiner.on("\n").join(errors)));
        }
    }

    /**
     * Test all the TEST_MAPPING files and make sure they contain the suite setting in
     * module-info.json.
     */
    @Test
    public void testTestSuiteSetting() throws JSONException {
        List<String> errors = new ArrayList<>();
        for (String testGroup : allTests.keySet()) {
            if (!TEST_GROUPS_TO_VALIDATE.contains(testGroup)) {
                CLog.d("Skip checking tests with group: %s", testGroup);
                continue;
            }
            for (TestInfo testInfo : allTests.get(testGroup)) {
                if (!validateSuiteSetting(testInfo.getName(), testInfo.getKeywords())) {
                    errors.add(
                            String.format(
                                    "Missing test_suite setting for test: %s, test group: %s, "
                                            + "TEST_MAPPING file path: %s",
                                    testInfo.getName(), testGroup, testInfo.getSources()));
                }
            }
        }
        if (!errors.isEmpty()) {
            fail(String.format("Fail test_suite setting check:\n%s", Joiner.on("\n").join(errors)));
        }
    }

    /**
     * Test all the tests by each test group and make sure the file options aren't conflict to AJUR
     * rules.
     */
    @Test
    public void testFilterOptions() {
        List<String> errors = new ArrayList<>();
        for (String testGroup : allTests.keySet()) {
            for (String moduleName : getModuleNames(testGroup)) {
                errors.addAll(validateFilterOption(moduleName, INCLUDE_FILTER, testGroup));
                errors.addAll(validateFilterOption(moduleName, EXCLUDE_FILTER, testGroup));
            }
        }
        if (!errors.isEmpty()) {
            fail(
                    String.format(
                            "Fail include/exclude filter setting check:\n%s",
                            Joiner.on("\n").join(errors)));
        }
    }

    /**
     * Validate if the filter option of a test contains both class/method and package. options.
     *
     * @param moduleName A {@code String} name of a test module.
     * @param filterOption A {@code String} of the filter option defined in TEST MAPPING file.
     * @param testGroup A {@code String} name of the test group.
     * @return A {@code List<String>} of the validation errors.
     */
    private List<String> validateFilterOption(
            String moduleName, String filterOption, String testGroup) {
        List<String> errors = new ArrayList<>();
        for (TestInfo test : getTestInfos(moduleName, testGroup)) {
            Set<Filters> filterTypes = new HashSet<>();
            Map<Filters, Set<TestInfo>> filterTestInfos = new HashMap<>();
            for (TestOption options : test.getOptions()) {
                if (options.getName().equals(filterOption)) {
                    Filters optionType = getOptionType(options.getValue());
                    // Add optionType with each TestInfo to get the detailed information.
                    filterTestInfos.computeIfAbsent(optionType, k -> new HashSet<>()).add(test);
                }
            }
            filterTypes = filterTestInfos.keySet();
            // If the options of a test in one TEST_MAPPING file contain either REGEX,
            // CLASS_OR_METHOD, or PACKAGE, it should be caught and output the tests
            // information.
            // TODO(b/128947872): List the type with fewest options first.
            if (filterTypes.size() > 1) {
                errors.add(
                        String.format(
                                "Mixed filter types found. Test: %s , TestGroup: %s, Details:\n"
                                        + "%s",
                                moduleName,
                                testGroup,
                                getDetailedErrors(filterOption, filterTestInfos)));
            }
        }
        return errors;
    }

    /**
     * Get the detailed validation errors.
     *
     * @param filterOption A {@code String} of the filter option defined in TEST MAPPING file.
     * @param filterTestInfos A {@code Map<Filters, Set<TestInfo>>} of tests with the given filter
     *     type and its child test information.
     * @return A {@code String} of the detailed errors.
     */
    private String getDetailedErrors(
            String filterOption, Map<Filters, Set<TestInfo>> filterTestInfos) {
        StringBuilder errors = new StringBuilder("");
        Set<Map.Entry<Filters, Set<TestInfo>>> entries = filterTestInfos.entrySet();
        for (Map.Entry<Filters, Set<TestInfo>> entry : entries) {
            Set<TestInfo> testInfos = entry.getValue();
            StringBuilder detailedErrors = new StringBuilder("");
            for (TestInfo test : testInfos) {
                for (TestOption options : test.getOptions()) {
                    if (options.getName().equals(filterOption)) {
                        detailedErrors.append(
                                String.format(
                                        "  %s (%s)\n", options.getValue(), test.getSources()));
                    }
                }
            }
            errors.append(
                    String.format(
                            "Options using %s filter:\n%s",
                            entry.getKey().toString(), detailedErrors));
        }
        return errors.toString();
    }

    /**
     * Determine whether optionValue represents regrex, test class or method, or package.
     *
     * @param optionValue A {@code String} containing either an individual test regrex, class/method
     *     or a package.
     * @return A {@code Filters} representing regrex, test class or method, or package.
     */
    private Filters getOptionType(String optionValue) {
        if (REGULAR_EXPRESSION.matcher(optionValue).find()) {
            return Filters.REGEX;
        } else if (CLASS_OR_METHOD_REGEX.matcher(optionValue).find()) {
            return Filters.CLASS_OR_METHOD;
        }
        return Filters.PACKAGE;
    }

    /**
     * Validate if the mainline module parameter is properly configured in the config.
     *
     * @param param A {@code String} name of the mainline module parameter.
     * @param paths A {@code Set<String>} path of the test config or TEST_MAPPING file.
     * @return A {@code String} of the errors.
     */
    private String validateMainlineModuleConfig(String param, Set<String> paths) {
        StringBuilder errors = new StringBuilder("");
        if (!isInAlphabeticalOrder(param)) {
            errors.append(
                    String.format(
                            "Illegal mainline module parameter: \"%s\" configured in the %s. " +
                                "Parameter must be configured in alphabetical order and with no " +
                                "duplicated modules.", param, paths));
        }
        else if (!isValidMainlineParam(param)) {
            errors.append(
                    String.format(
                            "Illegal mainline module parameter: \"%s\" configured in the %s. " +
                                "Parameter must end with .apk/.apex/.apks and have no any spaces " +
                                "configured.", param, paths));
        }
        return errors.toString();
    }

    /** Whether a mainline parameter configured in a test config is in alphabetical order or not. */
    boolean isInAlphabeticalOrder(String param) {
        String previousString = "";
        for (String currentString : param.split(String.format("\\+"))) {
            // This is to check if the parameter is in alphabetical order or duplicated.
            if (currentString.compareTo(previousString) <= 0) {
                return false;
            }
            previousString = currentString;
        }
        return true;
    }

    /** Whether the mainline parameter configured in the test config is valid or not. */
    boolean isValidMainlineParam(String param) {
        if (param.contains(" ")) {
            return false;
        }
        for (String m : param.split(String.format("\\+"))) {
            if (!MAINLINE_PARAMETERS_TO_VALIDATE.stream().anyMatch(entry -> m.endsWith(entry))) {
                return false;
            }
        }
        return true;
    }

    /**
     * Validate if the name exists in module-info.json and with the correct suite setting.
     *
     * @param name A {@code String} name of the test.
     * @param keywords A {@code Set<String>} keywords of the test.
     * @return true if name exists in module-info.json and matches either "general-tests" or
     *     "device-tests", or name doesn't exist in module-info.json.
     */
    private boolean validateSuiteSetting(String name, Set<String> keywords) throws JSONException {
        if (!moduleInfo.has(name)) {
            CLog.w("Test Module: %s can't be found in module-info.json. Ignore checking...", name);
            return true;
        }
        JSONArray compatibilitySuites =
                moduleInfo.getJSONObject(name).getJSONArray(LOCAL_COMPATIBILITY_SUITES);
        for (int i = 0; i < compatibilitySuites.length(); i++) {
            String suite = compatibilitySuites.optString(i);
            if (suite.equals(GENERAL_TESTS) || suite.equals(DEVICE_TESTS)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Get the module names for the given test group.
     *
     * @param testGroup A {@code String} name of the test group.
     * @return A {@code Set<String>} containing the module names for the given test group.
     */
    private Set<String> getModuleNames(String testGroup) {
        Set<String> moduleNames = new HashSet<>();
        for (TestInfo test : allTests.get(testGroup)) {
            moduleNames.add(test.getName());
        }
        return moduleNames;
    }

    /**
     * Get the test infos for the given module name and test group.
     *
     * @param moduleName A {@code String} name of a test module.
     * @param testGroup A {@code String} name of the test group.
     * @return A {@code Set<TestInfo>} of tests that each is for a unique test module.
     */
    private Set<TestInfo> getTestInfos(String moduleName, String testGroup) {
        Set<TestInfo> testInfos = new HashSet<>();
        for (TestInfo test : allTests.get(testGroup)) {
            if (test.getName().equals(moduleName)) {
                testInfos.add(test);
            }
        }
        return testInfos;
    }
}
