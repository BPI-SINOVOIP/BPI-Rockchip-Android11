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
package com.android.tradefed.testtype.suite;

import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.testmapping.TestInfo;
import com.android.tradefed.util.testmapping.TestMapping;
import com.android.tradefed.util.testmapping.TestOption;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Implementation of {@link BaseTestSuite} to run tests specified by option include-filter, or
 * TEST_MAPPING files from build, as a suite.
 */
public class TestMappingSuiteRunner extends BaseTestSuite {

    @Option(
        name = "test-mapping-test-group",
        description =
                "Group of tests to run, e.g., presubmit, postsubmit. The suite runner "
                        + "shall load the tests defined in all TEST_MAPPING files in the source "
                        + "code, through build artifact test_mappings.zip."
    )
    private String mTestGroup = null;

    @Option(
        name = "test-mapping-keyword",
        description =
                "Keyword to be matched to the `keywords` setting of a test configured in "
                        + "a TEST_MAPPING file. The test will only run if it has all the keywords "
                        + "specified in the option. If option test-mapping-test-group is not set, "
                        + "test-mapping-keyword option is ignored as the tests to run are not "
                        + "loaded directly from TEST_MAPPING files but is supplied via the "
                        + "--include-filter arg."
    )
    private Set<String> mKeywords = new HashSet<>();

    @Option(
        name = "force-test-mapping-module",
        description =
                "Run the specified tests only. The tests loaded from all TEST_MAPPING files in "
                        + "the source code will be filtered again to force run the specified tests."
    )
    private Set<String> mTestModulesForced = new HashSet<>();

    @Option(
        name = "test-mapping-path",
        description = "Run tests according to the test mapping path."
    )
    private List<String> mTestMappingPaths = new ArrayList<>();

    @Option(
        name = "use-test-mapping-path",
        description = "Whether or not to run tests based on the given test mapping path."
    )
    private boolean mUseTestMappingPath = false;

    /** Special definition in the test mapping structure. */
    private static final String TEST_MAPPING_INCLUDE_FILTER = "include-filter";

    private static final String TEST_MAPPING_EXCLUDE_FILTER = "exclude-filter";

    /**
     * Load the tests configuration that will be run. Each tests is defined by a {@link
     * IConfiguration} and a unique name under which it will report results. There are 2 ways to
     * load tests for {@link TestMappingSuiteRunner}:
     *
     * <p>1. --test-mapping-test-group, which specifies the group of tests in TEST_MAPPING files.
     * The runner will parse all TEST_MAPPING files in the source code through build artifact
     * test_mappings.zip, and load tests grouped under the given test group.
     *
     * <p>2. --include-filter, which specifies the name of the test to run. The use case is for
     * presubmit check to only run a list of tests related to the Cls to be verifies. The list of
     * tests are compiled from the related TEST_MAPPING files in modified source code.
     *
     * @return a map of test name to the {@link IConfiguration} object of each test.
     */
    @Override
    public LinkedHashMap<String, IConfiguration> loadTests() {
        Set<String> includeFilter = getIncludeFilter();
        // Name of the tests
        Set<String> testNames = new HashSet<>();
        Set<TestInfo> testInfosToRun = new HashSet<>();
        if (mTestGroup == null && includeFilter.isEmpty()) {
            throw new RuntimeException(
                    "At least one of the options, --test-mapping-test-group or --include-filter, "
                            + "should be set.");
        }
        if (mTestGroup == null && !mKeywords.isEmpty()) {
            throw new RuntimeException(
                    "Must specify --test-mapping-test-group when applying --test-mapping-keyword.");
        }
        if (mTestGroup == null && !mTestModulesForced.isEmpty()) {
            throw new RuntimeException(
                    "Must specify --test-mapping-test-group when applying "
                            + "--force-test-mapping-module.");
        }
        if (mTestGroup != null && !includeFilter.isEmpty()) {
            throw new RuntimeException(
                    "If options --test-mapping-test-group is set, option --include-filter should "
                            + "not be set.");
        }
        if (!includeFilter.isEmpty() && !mTestMappingPaths.isEmpty()) {
            throw new RuntimeException(
                    "If option --include-filter is set, option --test-mapping-path should "
                            + "not be set.");
        }

        if (mTestGroup != null) {
            if (!mTestMappingPaths.isEmpty()) {
                TestMapping.setTestMappingPaths(mTestMappingPaths);
            }
            testInfosToRun =
                    TestMapping.getTests(
                            getBuildInfo(), mTestGroup, getPrioritizeHostConfig(), mKeywords);
            if (!mTestModulesForced.isEmpty()) {
                CLog.i("Filtering tests for the given names: %s", mTestModulesForced);
                testInfosToRun =
                        testInfosToRun
                                .stream()
                                .filter(testInfo -> mTestModulesForced.contains(testInfo.getName()))
                                .collect(Collectors.toSet());
            }
            if (testInfosToRun.isEmpty()) {
                throw new RuntimeException(
                        String.format("No test found for the given group: %s.", mTestGroup));
            }
            for (TestInfo testInfo : testInfosToRun) {
                testNames.add(testInfo.getName());
            }
            setIncludeFilter(testNames);
            // With include filters being set, the test no longer needs group and path settings.
            // Clear the settings to avoid conflict when the test is running in a shard.
            mTestGroup = null;
            mTestMappingPaths.clear();
            mUseTestMappingPath = false;
        }

        // load all the configurations with include-filter injected.
        LinkedHashMap<String, IConfiguration> testConfigs = super.loadTests();

        // Create and inject individual tests by calling super.loadTests() with each test info.
        for (Map.Entry<String, IConfiguration> entry : testConfigs.entrySet()) {
            List<IRemoteTest> allTests = new ArrayList<>();
            IConfiguration moduleConfig = entry.getValue();
            ConfigurationDescriptor configDescriptor =
                    moduleConfig.getConfigurationDescription();
            IAbi abi = configDescriptor.getAbi();
            // Get the parameterized module name by striping the abi information out.
            String moduleName = entry.getKey().replace(String.format("%s ", abi.getName()), "");
            String configPath = moduleConfig.getName();
            Set<TestInfo> testInfos = getTestInfos(testInfosToRun, moduleName);
            // Only keep the same matching abi runner
            allTests.addAll(createIndividualTests(testInfos, configPath, abi));
            if (!allTests.isEmpty()) {
                // Set back to IConfiguration only if IRemoteTests are created.
                moduleConfig.setTests(allTests);
                // Set test sources to ConfigurationDescriptor.
                List<String> testSources = getTestSources(testInfos);
                configDescriptor.addMetadata(TestMapping.TEST_SOURCES, testSources);
            }
        }
        return testConfigs;
    }

    @VisibleForTesting
    String getTestGroup() {
        return mTestGroup;
    }

    @VisibleForTesting
    List<String> getTestMappingPaths() {
        return mTestMappingPaths;
    }

    @VisibleForTesting
    boolean getUseTestMappingPath() {
        return mUseTestMappingPath;
    }

    /**
     * Create individual tests with test infos for a module.
     *
     * @param testInfos A {@code Set<TestInfo>} containing multiple test options.
     * @param configPath A {@code String} of configuration path.
     * @return The {@link List} that are injected with the test options.
     */
    @VisibleForTesting
    List<IRemoteTest> createIndividualTests(Set<TestInfo> testInfos, String configPath, IAbi abi) {
        List<IRemoteTest> tests = new ArrayList<>();
        if (configPath == null) {
            throw new RuntimeException(String.format("Configuration path is null."));
        }
        File configFie = new File(configPath);
        if (!configFie.exists()) {
            configFie = null;
        }
        // De-duplicate test infos so that there won't be duplicate test options.
        testInfos = dedupTestInfos(testInfos);
        for (TestInfo testInfo : testInfos) {
            // Clean up all the test options injected in SuiteModuleLoader.
            super.cleanUpSuiteSetup();
            super.clearModuleArgs();
            if (configFie != null) {
                clearConfigPaths();
                // Set config path to BaseTestSuite to limit the search.
                addConfigPaths(configFie);
            }
            // Inject the test options from each test info to SuiteModuleLoader.
            parseOptions(testInfo);
            LinkedHashMap<String, IConfiguration> config = super.loadTests();
            for (Map.Entry<String, IConfiguration> entry : config.entrySet()) {
                if (entry.getValue().getConfigurationDescription().getAbi() != null
                        && !entry.getValue().getConfigurationDescription().getAbi().equals(abi)) {
                    continue;
                }
                tests.addAll(entry.getValue().getTests());
            }
        }
        return tests;
    }

    /**
     * Get a list of path of TEST_MAPPING for a module.
     *
     * @param testInfos A {@code Set<TestInfo>} containing multiple test options.
     * @return A {@code List<String>} of TEST_MAPPING path.
     */
    @VisibleForTesting
    List<String> getTestSources(Set<TestInfo> testInfos) {
        List<String> testSources = new ArrayList<>();
        for (TestInfo testInfo : testInfos) {
            testSources.addAll(testInfo.getSources());
        }
        return testSources;
    }

    /**
     * Parse the test options for the test info.
     *
     * @param testInfo A {@code Set<TestInfo>} containing multiple test options.
     */
    @VisibleForTesting
    void parseOptions(TestInfo testInfo) {
        Set<String> mappingIncludeFilters = new HashSet<>();
        Set<String> mappingExcludeFilters = new HashSet<>();
        // module-arg options compiled from test options for each test.
        Set<String> moduleArgs = new HashSet<>();
        Set<String> testNames = new HashSet<>();
        for (TestOption option : testInfo.getOptions()) {
            switch (option.getName()) {
                // Handle include and exclude filter at the suite level to hide each
                // test runner specific implementation and option names related to filtering
                case TEST_MAPPING_INCLUDE_FILTER:
                    mappingIncludeFilters.add(
                            String.format("%s %s", testInfo.getName(), option.getValue()));
                    break;
                case TEST_MAPPING_EXCLUDE_FILTER:
                    mappingExcludeFilters.add(
                            String.format("%s %s", testInfo.getName(), option.getValue()));
                    break;
                default:
                    String moduleArg =
                            String.format("%s:%s", testInfo.getName(), option.getName());
                    if (option.getValue() != null && !option.getValue().isEmpty()) {
                        moduleArg = String.format("%s:%s", moduleArg, option.getValue());
                    }
                    moduleArgs.add(moduleArg);
                    break;
            }
        }

        if (mappingIncludeFilters.isEmpty()) {
            testNames.add(testInfo.getName());
            setIncludeFilter(testNames);
        } else {
            setIncludeFilter(mappingIncludeFilters);
        }
        if (!mappingExcludeFilters.isEmpty()) {
            setExcludeFilter(mappingExcludeFilters);
        }
        addModuleArgs(moduleArgs);
    }

    /**
     * De-duplicate test infos with the same test options.
     *
     * @param testInfos A {@code Set<TestInfo>} containing multiple test options.
     * @return A {@code Set<TestInfo>} of tests without duplicated test options.
     */
    @VisibleForTesting
    Set<TestInfo> dedupTestInfos(Set<TestInfo> testInfos) {
        Set<String> nameOptions = new HashSet<>();
        Set<TestInfo> dedupTestInfos = new HashSet<>();
        for (TestInfo testInfo : testInfos) {
            String nameOption = testInfo.getName() + testInfo.getOptions().toString();
            if (!nameOptions.contains(nameOption)) {
                dedupTestInfos.add(testInfo);
                nameOptions.add(nameOption);
            }
        }
        return dedupTestInfos;
    }

    /**
     * Get the test infos for the given module name.
     *
     * @param testInfos A {@code Set<TestInfo>} containing multiple test options.
     * @param moduleName A {@code String} name of a test module.
     * @return A {@code Set<TestInfo>} of tests for a module.
     */
    @VisibleForTesting
    Set<TestInfo> getTestInfos(Set<TestInfo> testInfos, String moduleName) {
        return testInfos
                .stream()
                .filter(testInfo -> moduleName.equals(testInfo.getName()))
                .collect(Collectors.toSet());
    }
}
