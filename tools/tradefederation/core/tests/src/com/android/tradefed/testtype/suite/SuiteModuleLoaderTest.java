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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.InstallApexModuleTargetPreparer;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFileFilterReceiver;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link SuiteModuleLoader}. */
@RunWith(JUnit4.class)
public class SuiteModuleLoaderTest {

    private static final String TEST_CONFIG =
            "<configuration description=\"Runs a stub tests part of some suite\">\n"
                    + "    <target_preparer class=\"com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$PreparerInject\" />\n"
                    + "    <test class=\"com.android.tradefed.testtype.suite.SuiteModuleLoaderTest"
                    + "$TestInject\" />\n"
                    + "</configuration>";

    private static final String TEST_INSTANT_CONFIG =
            "<configuration description=\"Runs a stub tests part of some suite\">\n"
                    + "    <option name=\"config-descriptor:metadata\" key=\"parameter\" value=\"instant_app\" />"
                    // Duplicate parameter should not have impact
                    + "    <option name=\"config-descriptor:metadata\" key=\"parameter\" value=\"instant_app\" />"
                    + "    <test class=\"com.android.tradefed.testtype.suite.TestSuiteStub\" />\n"
                    + "</configuration>";

    private static final String TEST_MAINLINE_CONFIG =
            "<configuration description=\"Runs a stub tests part of some suite\">\n"
                    + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk\" />"
                    // Duplicate parameter should not have impact
                    + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod2.apk\" />"
                    + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk+mod2.apk\" />"
                    + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk+mod2.apk\" />"
                    + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk\" />"
                    + "    <test class=\"com.android.tradefed.testtype.suite.TestSuiteStub\" />\n"
                    + "</configuration>";

    private SuiteModuleLoader mRepo;
    private File mTestsDir;
    private Set<IAbi> mAbis;
    private IInvocationContext mContext;
    private IBuildInfo mMockBuildInfo;

    @Before
    public void setUp() throws Exception {
        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        new ArrayList<>());
        mTestsDir = FileUtil.createTempDir("suite-module-loader-tests");
        mAbis = new HashSet<>();
        mAbis.add(new Abi("armeabi-v7a", "32"));
        mContext = new InvocationContext();
        mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andStubReturn("branch");
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andStubReturn("flavor");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andStubReturn("id");
        EasyMock.replay(mMockBuildInfo);
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockBuildInfo);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mTestsDir);
    }

    private void createModuleConfig(String moduleName) throws IOException {
        File module = new File(mTestsDir, moduleName + SuiteModuleLoader.CONFIG_EXT);
        FileUtil.writeToFile(TEST_CONFIG, module);
    }

    private void createInstantModuleConfig(String moduleName) throws IOException {
        File module = new File(mTestsDir, moduleName + SuiteModuleLoader.CONFIG_EXT);
        FileUtil.writeToFile(TEST_INSTANT_CONFIG, module);
    }

    private void createMainlineModuleConfig(String moduleName) throws IOException {
        File moduleConfig = new File(mTestsDir, moduleName + SuiteModuleLoader.CONFIG_EXT);
        FileUtil.writeToFile(TEST_MAINLINE_CONFIG, moduleConfig);
    }

    @OptionClass(alias = "preparer-inject")
    public static class PreparerInject extends BaseTargetPreparer {
        @Option(name = "preparer-string")
        public String preparer = null;
    }

    @OptionClass(alias = "test-inject")
    public static class TestInject
            implements IRemoteTest, ITestFileFilterReceiver, ITestFilterReceiver {
        @Option(name = "simple-string")
        public String test = null;

        @Option(name = "empty-string")
        public String testEmpty = null;

        @Option(name = "alias-option")
        public String testAlias = null;

        @Option(name = "list-string")
        public List<String> testList = new ArrayList<>();

        @Option(name = "map-string")
        public Map<String, String> testMap = new HashMap<>();

        public File mIncludeTestFile;
        public File mExcludeTestFile;

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {}

        @Override
        public void setIncludeTestFile(File testFile) {
            mIncludeTestFile = testFile;
        }

        @Override
        public void setExcludeTestFile(File testFile) {
            mExcludeTestFile = testFile;
        }

        @Override
        public void addIncludeFilter(String filter) {
            // NA
        }

        @Override
        public void addAllIncludeFilters(Set<String> filters) {
            // NA
        }

        @Override
        public void addExcludeFilter(String filter) {
            // NA
        }

        @Override
        public void addAllExcludeFilters(Set<String> filters) {
            // NA
        }

        @Override
        public Set<String> getIncludeFilters() {
            return null;
        }

        @Override
        public Set<String> getExcludeFilters() {
            return null;
        }

        @Override
        public void clearIncludeFilters() {
            // NA
        }

        @Override
        public void clearExcludeFilters() {
            // NA
        }
    }

    /** Test an end-to-end injection of --module-arg. */
    @Test
    public void testInjectConfigOptions_moduleArgs() throws Exception {
        List<String> moduleArgs = new ArrayList<>();
        moduleArgs.add("module1[test]:simple-string:value1");
        moduleArgs.add("module1[test]:empty-string:"); // value is the empty string

        moduleArgs.add("module1[test]:list-string:value2");
        moduleArgs.add("module1[test]:list-string:value3");
        moduleArgs.add("module1[test]:list-string:set-option:moreoption");
        moduleArgs.add("module1[test]:list-string:"); // value is the empty string
        moduleArgs.add("module1[test]:map-string:set-option:=moreoption");
        moduleArgs.add("module1[test]:map-string:empty-option:="); // value is the empty string

        createModuleConfig("module1[test]");

        Map<String, List<SuiteTestFilter>> includeFilter = new LinkedHashMap<>();
        SuiteTestFilter filter = SuiteTestFilter.createFrom("armeabi-v7a module1[test] test#test");
        includeFilter.put("armeabi-v7a module1[test]", Arrays.asList(filter));
        mRepo =
                new SuiteModuleLoader(
                        includeFilter,
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        moduleArgs);
        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertNotNull(res.get("armeabi-v7a module1[test]"));
        IConfiguration config = res.get("armeabi-v7a module1[test]");

        TestInject checker = (TestInject) config.getTests().get(0);
        assertEquals("value1", checker.test);
        assertEquals("", checker.testEmpty);
        // Check list
        assertTrue(checker.testList.size() == 4);
        assertTrue(checker.testList.contains("value2"));
        assertTrue(checker.testList.contains("value3"));
        assertTrue(checker.testList.contains("set-option:moreoption"));
        assertTrue(checker.testList.contains(""));
        // Chech map
        assertTrue(checker.testMap.size() == 2);
        assertEquals("moreoption", checker.testMap.get("set-option"));
        assertEquals("", checker.testMap.get("empty-option"));
        // Check filters
        assertNotNull(checker.mIncludeTestFile);
        assertNull(checker.mExcludeTestFile);
        assertTrue(checker.mIncludeTestFile.getName().contains("armeabi-v7a%20module1%5Btest%5"));
        FileUtil.deleteFile(checker.mIncludeTestFile);
    }

    /** Test an end-to-end injection of --test-arg. */
    @Test
    public void testInjectConfigOptions_testArgs() throws Exception {
        List<String> testArgs = new ArrayList<>();
        // Value for ITargetPreparer
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$PreparerInject:"
                        + "preparer-string:preparer");
        // Values for IRemoteTest
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "simple-string:value1");
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "empty-string:"); // value is the empty string
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "list-string:value2");
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "list-string:value3");
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "list-string:set-option:moreoption");
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "list-string:"); // value is the empty string
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "map-string:set-option:=moreoption");
        testArgs.add(
                "com.android.tradefed.testtype.suite.SuiteModuleLoaderTest$TestInject:"
                        + "map-string:empty-option:="); // value is the empty string

        createModuleConfig("module1");

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        testArgs,
                        new ArrayList<>());
        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertNotNull(res.get("armeabi-v7a module1"));
        IConfiguration config = res.get("armeabi-v7a module1");

        PreparerInject preparer = (PreparerInject) config.getTargetPreparers().get(0);
        assertEquals("preparer", preparer.preparer);

        TestInject checker = (TestInject) config.getTests().get(0);
        assertEquals("value1", checker.test);
        assertEquals("", checker.testEmpty);
        // Check list
        assertTrue(checker.testList.size() == 4);
        assertTrue(checker.testList.contains("value2"));
        assertTrue(checker.testList.contains("value3"));
        assertTrue(checker.testList.contains("set-option:moreoption"));
        assertTrue(checker.testList.contains(""));
        // Chech map
        assertTrue(checker.testMap.size() == 2);
        assertEquals("moreoption", checker.testMap.get("set-option"));
        assertEquals("", checker.testMap.get("empty-option"));
    }

    @Test
    public void testInjectConfigOptions_moduleArgs_alias() throws Exception {
        List<String> moduleArgs = new ArrayList<>();
        moduleArgs.add("module1:{test-inject}alias-option:value1");

        createModuleConfig("module1");

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        moduleArgs);
        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertNotNull(res.get("armeabi-v7a module1"));
        IConfiguration config = res.get("armeabi-v7a module1");

        TestInject checker = (TestInject) config.getTests().get(0);
        assertEquals("value1", checker.testAlias);
    }

    /**
     * Test that if the base module is excluded in full, the filters of parameterized modules are
     * still populated with the proper filters.
     */
    @Test
    public void testFilterParameterized() throws Exception {
        Map<String, List<SuiteTestFilter>> excludeFilters = new LinkedHashMap<>();
        createInstantModuleConfig("basemodule");
        SuiteTestFilter fullFilter = SuiteTestFilter.createFrom("armeabi-v7a basemodule");
        excludeFilters.put("armeabi-v7a basemodule", Arrays.asList(fullFilter));

        SuiteTestFilter instantMethodFilter =
                SuiteTestFilter.createFrom(
                        "armeabi-v7a basemodule[instant] NativeDnsAsyncTest#Async_Cancel");
        excludeFilters.put("armeabi-v7a basemodule[instant]", Arrays.asList(instantMethodFilter));

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        excludeFilters,
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(1, res.size());
        // Full module was excluded completely
        IConfiguration instantModule = res.get("armeabi-v7a basemodule[instant]");
        assertNotNull(instantModule);
        TestSuiteStub stubTest = (TestSuiteStub) instantModule.getTests().get(0);
        assertEquals(1, stubTest.getExcludeFilters().size());
        assertEquals(
                "NativeDnsAsyncTest#Async_Cancel", stubTest.getExcludeFilters().iterator().next());
        // Ensure that appropriate metadata are set on the module config descriptor
        ConfigurationDescriptor descriptor = instantModule.getConfigurationDescription();
        assertEquals(
                1,
                descriptor
                        .getAllMetaData()
                        .get(ConfigurationDescriptor.ACTIVE_PARAMETER_KEY)
                        .size());
        assertEquals(
                "instant",
                descriptor
                        .getAllMetaData()
                        .getUniqueMap()
                        .get(ConfigurationDescriptor.ACTIVE_PARAMETER_KEY));
        assertEquals("armeabi-v7a", descriptor.getAbi().getName());
    }

    /**
     * Test that if the base module is excluded in full, the filters of parameterized modules are
     * still populated with the proper filters.
     */
    @Test
    public void testFilterParameterized_excludeFilter_parameter() throws Exception {
        Map<String, List<SuiteTestFilter>> excludeFilters = new LinkedHashMap<>();
        createInstantModuleConfig("basemodule");
        SuiteTestFilter fullFilter = SuiteTestFilter.createFrom("armeabi-v7a basemodule[instant]");
        excludeFilters.put("basemodule[instant]", Arrays.asList(fullFilter));

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        excludeFilters,
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
            mRepo.loadConfigsFromDirectory(
                Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(1, res.size());
        // Full module was excluded completely
        IConfiguration instantModule = res.get("armeabi-v7a basemodule[instant]");
        assertNull(instantModule);
    }

    @Test
    public void testFilterParameterized_includeFilter_base() throws Exception {
        Map<String, List<SuiteTestFilter>> includeFilters = new LinkedHashMap<>();
        createInstantModuleConfig("basemodule");
        SuiteTestFilter fullFilter = SuiteTestFilter.createFrom("armeabi-v7a basemodule");
        includeFilters.put("armeabi-v7a basemodule", Arrays.asList(fullFilter));

        mRepo =
                new SuiteModuleLoader(
                        includeFilters,
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(1, res.size());
        // Parameterized module was excluded completely
        IConfiguration baseModule = res.get("armeabi-v7a basemodule");
        assertNotNull(baseModule);
    }

    @Test
    public void testFilterParameterized_includeFilter_param() throws Exception {
        Map<String, List<SuiteTestFilter>> includeFilters = new LinkedHashMap<>();
        createInstantModuleConfig("basemodule");
        SuiteTestFilter fullFilter = SuiteTestFilter.createFrom("armeabi-v7a basemodule[instant]");
        includeFilters.put("armeabi-v7a basemodule[instant]", Arrays.asList(fullFilter));

        mRepo =
                new SuiteModuleLoader(
                        includeFilters,
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(1, res.size());
        // Full module was excluded completely
        IConfiguration instantModule = res.get("armeabi-v7a basemodule[instant]");
        assertNotNull(instantModule);
    }

    @Test
    public void testFilterParameterized_WithModuleArg() throws Exception {
        List<String> moduleArgs = new ArrayList<>();
        createInstantModuleConfig("basemodule");
        moduleArgs.add("basemodule[instant]:exclude-annotation:test-annotation");

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        moduleArgs);
        mRepo.setParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(2, res.size());
        IConfiguration instantModule = res.get("armeabi-v7a basemodule[instant]");
        assertNotNull(instantModule);
        TestSuiteStub stubTest = (TestSuiteStub) instantModule.getTests().get(0);
        assertEquals(2, stubTest.getExcludeAnnotations().size());
        List<String> expected =
                Arrays.asList("android.platform.test.annotations.AppModeFull", "test-annotation");
        assertTrue(stubTest.getExcludeAnnotations().containsAll(expected));
    }

    /**
     * Test that the configuration can be found if specifying specific path.
     */
    @Test
    public void testLoadConfigsFromSpecifiedPaths_OneModule() throws Exception {
        createModuleConfig("module1");
        File module1 = new File(mTestsDir, "module1" + SuiteModuleLoader.CONFIG_EXT);

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        new ArrayList<>());

        LinkedHashMap<String, IConfiguration> res =
            mRepo.loadConfigsFromSpecifiedPaths(
                Arrays.asList(module1), mAbis, null);
        assertEquals(1, res.size());
        assertNotNull(res.get("armeabi-v7a module1"));
    }

    /**
     * Test that multiple configurations can be found if specifying specific paths.
     */
    @Test
    public void testLoadConfigsFromSpecifiedPaths_MultipleModules() throws Exception {
        createModuleConfig("module1");
        File module1 = new File(mTestsDir, "module1" + SuiteModuleLoader.CONFIG_EXT);
        createModuleConfig("module2");
        File module2 = new File(mTestsDir, "module2" + SuiteModuleLoader.CONFIG_EXT);

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        new ArrayList<>());

        LinkedHashMap<String, IConfiguration> res =
            mRepo.loadConfigsFromSpecifiedPaths(
                Arrays.asList(module1, module2), mAbis, null);
        assertEquals(2, res.size());
        assertNotNull(res.get("armeabi-v7a module1"));
        assertNotNull(res.get("armeabi-v7a module2"));
    }

    /**
     * Test that configuration can be found correctly if specifying specific paths but someone is
     * excluded.
     */
    @Test
    public void testLoadConfigsFromSpecifiedPaths_WithExcludeFilter() throws Exception {
        createModuleConfig("module1");
        File module1 = new File(mTestsDir, "module1" + SuiteModuleLoader.CONFIG_EXT);
        createModuleConfig("module2");
        File module2 = new File(mTestsDir, "module2" + SuiteModuleLoader.CONFIG_EXT);

        Map<String, List<SuiteTestFilter>> excludeFilters = new LinkedHashMap<>();
        SuiteTestFilter filter =
            SuiteTestFilter.createFrom(
                "armeabi-v7a module2");
        excludeFilters.put("armeabi-v7a module2", Arrays.asList(filter));

        mRepo =
            new SuiteModuleLoader(
                new LinkedHashMap<String, List<SuiteTestFilter>>(),
                excludeFilters,
                new ArrayList<>(),
                new ArrayList<>());

        LinkedHashMap<String, IConfiguration> res =
            mRepo.loadConfigsFromSpecifiedPaths(
                Arrays.asList(module1, module2), mAbis, null);
        assertEquals(1, res.size());
        assertNotNull(res.get("armeabi-v7a module1"));
        assertNull(res.get("armeabi-v7a module2"));
    }

    /**
     * Test deduplicate the given mainline parameters.
     */
    @Test
    public void testDedupMainlineParameters() throws Exception {
        List<String> parameters = new ArrayList<>();
        parameters.add("mod1.apk");
        parameters.add("mod1.apk");
        parameters.add("mod1.apk+mod2.apk");
        parameters.add("mod1.apk+mod2.apk");
        Set<String> results = mRepo.dedupMainlineParameters(parameters, "configName");
        assertEquals(2, results.size());

        boolean IsEqual = true;
        for (String result : results) {
            if (!(result.equals("mod1.apk") || result.equals("mod1.apk+mod2.apk"))) {
                IsEqual = false;
            }
        }
        assertTrue(IsEqual);
    }

    /**
     * Test deduplicate the given mainline parameters with invalid spaces configured.
     */
    @Test
    public void testDedupMainlineParameters_WithSpaces() throws Exception {
        List<String> parameters = new ArrayList<>();
        parameters.add("mod1.apk");
        parameters.add(" mod1.apk");
        parameters.add("mod1.apk+mod2.apk ");
        try {
            mRepo.dedupMainlineParameters(parameters, "configName");
            fail("Should have thrown an exception.");
        } catch (ConfigurationException expected) {
            // expected
            assertTrue(expected.getMessage().contains("Illegal mainline module parameter:"));
        }
    }

    /**
     * Test deduplicate the given mainline parameters end with invalid extension.
     */
    @Test
    public void testDedupMainlineParameters_WithInvalidExtension() throws Exception {
        List<String> parameters = new ArrayList<>();
        parameters.add("mod1.apk");
        parameters.add("mod1.apk+mod2.unknown");
        try {
            mRepo.dedupMainlineParameters(parameters, "configName");
            fail("Should have thrown an exception.");
        } catch (ConfigurationException expected) {
            // expected
            assertTrue(expected.getMessage().contains("Illegal mainline module parameter:"));
        }
    }

    /**
     * Test deduplicate the given mainline parameters end with invalid format.
     */
    @Test
    public void testDedupMainlineParameters_WithInvalidFormat() throws Exception {
        List<String> parameters = new ArrayList<>();
        parameters.add("mod1.apk");
        parameters.add("+mod2.apex");
        try {
            mRepo.dedupMainlineParameters(parameters, "configName");
            fail("Should have thrown an exception.");
        } catch (ConfigurationException expected) {
            // expected
            assertTrue(expected.getMessage().contains("Illegal mainline module parameter:"));
        }
    }

    /**
     * Test deduplicate the given mainline parameter with duplicated modules configured.
     */
    @Test
    public void testDedupMainlineParameters_WithDuplicatedMainlineModules() throws Exception {
        List<String> parameters = new ArrayList<>();
        parameters.add("mod1.apk+mod1.apk");
        try {
            mRepo.dedupMainlineParameters(parameters, "configName");
            fail("Should have thrown an exception.");
        } catch (ConfigurationException expected) {
            // expected
            assertTrue(expected.getMessage().contains("Illegal mainline module parameter:"));
        }
    }

    /**
     * Test deduplicate the given mainline parameters are not configured in alphabetical order.
     */
    @Test
    public void testDedupMainlineParameters_ParameterNotInAlphabeticalOrder() throws Exception {
        List<String> parameters = new ArrayList<>();
        parameters.add("mod1.apk");
        parameters.add("mod2.apex+mod1.apk");
        try {
            mRepo.dedupMainlineParameters(parameters, "configName");
            fail("Should have thrown an exception.");
        } catch (ConfigurationException expected) {
            // expected
            assertTrue(expected.getMessage().contains("Illegal mainline module parameter:"));
        }
    }

    /**
     * Test get the mainline parameters defined in the test config.
     */
    @Test
    public void testGetMainlineModuleParameters() throws Exception {
        createMainlineModuleConfig("mainline_module");
        IConfiguration config =
                ConfigurationFactory.getInstance().createConfigurationFromArgs(
                        new String[] {mTestsDir.getAbsolutePath() + "/mainline_module.config"});

        List<String> results = mRepo.getMainlineModuleParameters(config);
        assertEquals(3, results.size());

        boolean IsEqual = true;
        for (String id : results) {
            if (!(id.equals("mod1.apk") ||
                  id.equals("mod2.apk") ||
                  id.equals("mod1.apk+mod2.apk"))) {
                IsEqual = false;
            }
        }
        assertTrue(IsEqual);
    }

    /**
     * Test that generate the correct IConfiguration objects based on the defined mainline modules.
     */
    @Test
    public void testLoadParameterizedMainlineModules() throws Exception {
        createMainlineModuleConfig("basemodule");
        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setInvocationContext(mContext);
        mRepo.setMainlineParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(3, res.size());
        IConfiguration module1 = res.get("armeabi-v7a basemodule[mod1.apk]");
        assertNotNull(module1);
        assertTrue(
                module1.getTargetPreparers().get(0) instanceof InstallApexModuleTargetPreparer);
        EasyMock.verify(mMockBuildInfo);
    }

    /**
     * Test that generate the correct IConfiguration objects based on the defined mainline modules
     * with given exclude-filter.
     */
    @Test
    public void testLoadParameterizedMainlineModule_WithFilters() throws Exception {
        Map<String, List<SuiteTestFilter>> excludeFilters = new LinkedHashMap<>();
        createMainlineModuleConfig("basemodule");
        SuiteTestFilter fullFilter = SuiteTestFilter.createFrom("armeabi-v7a basemodule");
        excludeFilters.put("armeabi-v7a basemodule", Arrays.asList(fullFilter));

        SuiteTestFilter filter =
                SuiteTestFilter.createFrom(
                        "armeabi-v7a basemodule[mod1.apk] class#method");
        excludeFilters.put("armeabi-v7a basemodule[mod1.apk]", Arrays.asList(filter));

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        excludeFilters,
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setInvocationContext(mContext);
        mRepo.setMainlineParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(3, res.size());
        IConfiguration module1 = res.get("armeabi-v7a basemodule[mod1.apk]");
        assertNotNull(module1);
        TestSuiteStub stubTest = (TestSuiteStub) module1.getTests().get(0);
        assertEquals(1, stubTest.getExcludeFilters().size());
        assertEquals("class#method", stubTest.getExcludeFilters().iterator().next());
        EasyMock.verify(mMockBuildInfo);
    }

    /**
     * Test that generate the correct IConfiguration objects based on the defined mainline modules
     * with given module args.
     */
    @Test
    public void testLoadParameterizedMainlineModule_WithModuleArgs() throws Exception {
        List<String> moduleArgs = new ArrayList<>();
        moduleArgs.add("basemodule[mod1.apk]:exclude-annotation:test-annotation");
        createMainlineModuleConfig("basemodule");

        mRepo =
                new SuiteModuleLoader(
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new LinkedHashMap<String, List<SuiteTestFilter>>(),
                        new ArrayList<>(),
                        moduleArgs);
        mRepo.setInvocationContext(mContext);
        mRepo.setMainlineParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(3, res.size());
        IConfiguration module1 = res.get("armeabi-v7a basemodule[mod1.apk]");
        assertNotNull(module1);
        TestSuiteStub stubTest = (TestSuiteStub) module1.getTests().get(0);
        assertEquals(1, stubTest.getExcludeAnnotations().size());
        assertEquals("test-annotation", stubTest.getExcludeAnnotations().iterator().next());
        EasyMock.verify(mMockBuildInfo);
    }

    /**
     * Test that generate the correct IConfiguration objects based on the defined mainline modules
     * with given include-filter and exclude-filter.
     */
    @Test
    public void testLoadParameterizedMainlineModules_WithMultipleFilters() throws Exception {
        Map<String, List<SuiteTestFilter>> includeFilters = new LinkedHashMap<>();
        Map<String, List<SuiteTestFilter>> excludeFilters = new LinkedHashMap<>();
        createMainlineModuleConfig("basemodule");
        SuiteTestFilter filter = SuiteTestFilter.createFrom("armeabi-v7a basemodule[mod1.apk]");
        includeFilters.put("armeabi-v7a basemodule[mod1.apk]", Arrays.asList(filter));

        filter = SuiteTestFilter.createFrom("armeabi-v7a basemodule[[mod2.apk]]");
        excludeFilters.put("armeabi-v7a basemodule[mod2.apk]", Arrays.asList(filter));

        mRepo =
                new SuiteModuleLoader(
                        includeFilters,
                        excludeFilters,
                        new ArrayList<>(),
                        new ArrayList<>());
        mRepo.setInvocationContext(mContext);
        mRepo.setMainlineParameterizedModules(true);

        List<String> patterns = new ArrayList<>();
        patterns.add(".*.config");
        patterns.add(".*.xml");
        LinkedHashMap<String, IConfiguration> res =
                mRepo.loadConfigsFromDirectory(
                        Arrays.asList(mTestsDir), mAbis, null, null, patterns);
        assertEquals(1, res.size());

        IConfiguration module1 = res.get("armeabi-v7a basemodule[mod1.apk]");
        assertNotNull(module1);

        module1 = res.get("armeabi-v7a basemodule[mod2.apk]");
        assertNull(module1);

        module1 = res.get("armeabi-v7a basemodule[mod1.apk+mod2.apk]");
        assertNull(module1);
        EasyMock.verify(mMockBuildInfo);
    }

    /**
     * Test that the mainline parameter configured in the test config is valid.
     */
    @Test
    public void testIsValidMainlineParam() throws Exception {
        assertTrue(mRepo.isValidMainlineParam("mod1.apk"));
        assertTrue(mRepo.isValidMainlineParam("mod1.apk+mod2.apex"));
        assertFalse(mRepo.isValidMainlineParam("  mod1.apk"));
        assertFalse(mRepo.isValidMainlineParam("+mod1.apk"));
        assertFalse(mRepo.isValidMainlineParam("mod1.apeks"));
        assertFalse(mRepo.isValidMainlineParam("mod1.apk +mod2.apex"));
        assertFalse(mRepo.isValidMainlineParam("mod1.apk+mod2.apex "));
    }

    /**
     * Test that the mainline parameter configured in the test config is in alphabetical order.
     */
    @Test
    public void testIsInAlphabeticalOrder() throws Exception {
        assertTrue(mRepo.isInAlphabeticalOrder("mod1.apk"));
        assertTrue(mRepo.isInAlphabeticalOrder("mod1.apk+mod2.apex"));
        assertFalse(mRepo.isInAlphabeticalOrder("mod2.apk+mod1.apex"));
        assertFalse(mRepo.isInAlphabeticalOrder("mod1.apk+mod1.apk"));
        assertTrue(mRepo.isInAlphabeticalOrder(
                "com.android.cellbroadcast.apex+com.android.ipsec.apex+com.android.permission.apex")
        );
        assertFalse(mRepo.isInAlphabeticalOrder(
                "com.android.permission.apex+com.android.ipsec.apex+com.android.cellbroadcast.apex")
        );
    }
}
