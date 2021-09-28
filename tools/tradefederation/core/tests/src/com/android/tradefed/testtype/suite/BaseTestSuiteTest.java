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

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.hasItem;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.targetprep.CreateUserPreparer;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.suite.params.ModuleParameters;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Set;

/** Unit tests for {@link BaseTestSuite}. */
@RunWith(JUnit4.class)
public class BaseTestSuiteTest {
    private BaseTestSuite mRunner;
    private IDeviceBuildInfo mBuildInfo;
    private ITestDevice mMockDevice;
    private TestInformation mTestInfo;

    private static final String TEST_MODULE = "test-module";

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mBuildInfo = new DeviceBuildInfo();
        mRunner = new AbiBaseTestSuite();
        mRunner.setBuild(mBuildInfo);
        mRunner.setDevice(mMockDevice);

        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        context.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();

        EasyMock.expect(mMockDevice.getProperty(EasyMock.anyObject())).andReturn("arm64-v8a");
        EasyMock.expect(mMockDevice.getProperty(EasyMock.anyObject())).andReturn("armeabi-v7a");
        EasyMock.replay(mMockDevice);
    }

    /**
     * Test BaseTestSuite that hardcodes the abis to avoid failures related to running the tests
     * against a particular abi build of tradefed.
     */
    public static class AbiBaseTestSuite extends BaseTestSuite {
        @Override
        public Set<IAbi> getAbis(ITestDevice device) throws DeviceNotAvailableException {
            Set<IAbi> abis = new LinkedHashSet<>();
            abis.add(new Abi("arm64-v8a", AbiUtils.getBitness("arm64-v8a")));
            abis.add(new Abi("armeabi-v7a", AbiUtils.getBitness("armeabi-v7a")));
            return abis;
        }
    }

    /** Test for {@link BaseTestSuite#setupFilters(File)} implementation, no modules match. */
    @Test
    public void testSetupFilters_noMatch() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        File moduleConfig = new File(tmpDir, "module_name.config");
        moduleConfig.createNewFile();
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("module", "my_module");
            mRunner.setupFilters(tmpDir);
            fail("Should have thrown exception");
        } catch (IllegalArgumentException expected) {
            assertEquals("No modules found matching my_module", expected.getMessage());
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /** Test that a module name can be matched and accept its parameterized version of args. */
    @Test
    public void testSetupFilters_match_parameterized() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        createConfig(tmpDir, "CtsGestureTestCases.config");
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("module", "Gesture");
            Set<String> excludeFilter = new HashSet<>();
            excludeFilter.add("arm64-v8a CtsGestureTestCases[instant]");
            mRunner.setExcludeFilter(excludeFilter);
            mRunner.setupFilters(tmpDir);
            assertEquals(1, mRunner.getIncludeFilter().size());
            assertThat(
                    mRunner.getIncludeFilter(),
                    hasItem(
                            new SuiteTestFilter(
                                            mRunner.getRequestedAbi(), "CtsGestureTestCases", null)
                                    .toString()));
            assertThat(
                    mRunner.getExcludeFilter(),
                    hasItem(
                            new SuiteTestFilter("arm64-v8a", "CtsGestureTestCases[instant]", null)
                                    .toString()));
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test that we create a module and the parameterized version of it for the include filter if
     * not explicitly excluded.
     */
    @Test
    public void testSetupFilters_parameterized_filter() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        createConfig(tmpDir, "CtsGestureTestCases.config");
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("enable-parameterized-modules", "true");
            // The Gesture module has a parameter "instant".
            setter.setOptionValue("module", "Gesture");
            mRunner.setupFilters(tmpDir);
            assertEquals(2, mRunner.getIncludeFilter().size());
            assertThat(
                    mRunner.getIncludeFilter(),
                    hasItem(
                            new SuiteTestFilter(
                                            mRunner.getRequestedAbi(), "CtsGestureTestCases", null)
                                    .toString()));
            assertThat(
                    mRunner.getIncludeFilter(),
                    hasItem(
                            new SuiteTestFilter(
                                            mRunner.getRequestedAbi(),
                                            "CtsGestureTestCases[instant]",
                                            null)
                                    .toString()));
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    @Test
    public void testSetupFilters_match() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        createConfig(tmpDir, "CtsGestureTestCases.config");
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("module", "Gesture");
            mRunner.setupFilters(tmpDir);
            assertEquals(1, mRunner.getIncludeFilter().size());
            assertThat(
                    mRunner.getIncludeFilter(),
                    hasItem(
                            new SuiteTestFilter(
                                            mRunner.getRequestedAbi(), "CtsGestureTestCases", null)
                                    .toString()));
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test for {@link BaseTestSuite#setupFilters(File)} implementation, only one module matches.
     */
    @Test
    public void testSetupFilters_oneMatch() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        createConfig(tmpDir, "module_name.config");
        createConfig(tmpDir, "module_name2.config");
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("module", "module_name2");
            mRunner.setupFilters(tmpDir);
            assertEquals(1, mRunner.getIncludeFilter().size());
            assertThat(
                    mRunner.getIncludeFilter(),
                    hasItem(
                            new SuiteTestFilter(mRunner.getRequestedAbi(), "module_name2", null)
                                    .toString()));
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test for {@link BaseTestSuite#setupFilters(File)} implementation, multi modules match prefix
     * but don't exact match.
     */
    @Test
    public void testSetupFilters_multiMatchNoExactMatch() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        createConfig(tmpDir, "module_name1.config");
        createConfig(tmpDir, "module_name2.config");
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("module", "module_name");
            mRunner.setupFilters(tmpDir);
            fail("Should have thrown exception");
        } catch (IllegalArgumentException expected) {
            assertThat(
                    expected.getMessage(),
                    containsString("Multiple modules found matching module_name:"));
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test for {@link BaseTestSuite#setupFilters(File)} implementation, multi modules match prefix
     * and one matches exactly.
     */
    @Test
    public void testSetupFilters_multiMatchOneExactMatch() throws Exception {
        File tmpDir = FileUtil.createTempDir(TEST_MODULE);
        createConfig(tmpDir, "module_name.config");
        createConfig(tmpDir, "module_name2.config");
        try {
            OptionSetter setter = new OptionSetter(mRunner);
            setter.setOptionValue("module", "module_name");
            mRunner.setupFilters(tmpDir);
            assertEquals(1, mRunner.getIncludeFilter().size());
            assertThat(
                    mRunner.getIncludeFilter(),
                    hasItem(
                            new SuiteTestFilter(mRunner.getRequestedAbi(), "module_name", null)
                                    .toString()));
        } finally {
            FileUtil.recursiveDelete(tmpDir);
        }
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} implementation, for basic example configurations.
     */
    @Test
    public void testLoadTests() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite");
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(4, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub1"));
        assertTrue(configMap.containsKey("arm64-v8a suite/stub2"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub1"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub2"));
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} implementation, only stub1.xml is part of this
     * suite.
     */
    @Test
    public void testLoadTests_suite2() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite2");
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(2, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub1"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub1"));
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} implementation, for configuration with include
     * filter set to a non-existent test.
     */
    @Test
    public void testLoadTests_emptyFilter() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("include-filter", "Doesntexist");
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite");
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(0, configMap.size());
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} implementation, for configuration with include
     * filter set to a non-existent test and enabled failure on empty test set.
     */
    @Test
    public void testLoadTests_emptyFilterWithFailOnEmpty() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("include-filter", "Doesntexist");
        setter.setOptionValue("fail-on-everything-filtered", "true");
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite");
        try {
            mRunner.loadTests();
            fail("Should have thrown exception");
        } catch (IllegalStateException ex) {
            assertEquals(
                    "Include filter '{arm64-v8a Doesntexist=[Doesntexist], armeabi-v7a "
                            + "Doesntexist=[Doesntexist]}' was specified but resulted in "
                            + "an empty test set.",
                    ex.getMessage());
        }
    }

    /** Test that when splitting, the instance of the implementation is used. */
    @Test
    public void testSplit() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite");
        Collection<IRemoteTest> tests = mRunner.split(2, mTestInfo);
        assertEquals(4, tests.size());
        for (IRemoteTest test : tests) {
            assertTrue(test instanceof BaseTestSuite);
        }
    }

    /**
     * Test that when {@link BaseTestSuite} run-suite-tag is not set we cannot shard since there is
     * no configuration.
     */
    @Test
    public void testSplit_nothingToLoad() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "doesnotexists");
        setter.setOptionValue("run-suite-tag", "doesnotexists");
        assertNull(mRunner.split(2));
    }

    /** Ensure that during sharding we don't attempt to log the filter files. */
    @Test
    public void testSplit_withFilters() throws Exception {
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite");
        Set<String> excludeModule = new HashSet<>();
        for (int i = 0; i < 25; i++) {
            excludeModule.add("arm64-v8a suite/load-filter-test" + i);
        }
        mRunner.setExcludeFilter(excludeModule);
        ITestLogger logger = EasyMock.createMock(ITestLogger.class);
        mRunner.setTestLogger(logger);

        EasyMock.replay(logger);
        Collection<IRemoteTest> tests = mRunner.split(2, mTestInfo);
        assertEquals(4, tests.size());
        for (IRemoteTest test : tests) {
            assertTrue(test instanceof BaseTestSuite);
        }
        EasyMock.verify(logger);
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} that when a test config supports IAbiReceiver,
     * multiple instances of the config are queued up.
     */
    @Test
    public void testLoadTestsForMultiAbi() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-abi");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(2, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stubAbi"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stubAbi"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} when loading a configuration with parameterized
     * metadata.
     */
    @Test
    public void testLoadTests_parameterizedModule() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        // We only create the primary abi of the parameterized module version.
        assertEquals(3, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized"));
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized[instant]"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub-parameterized"));
        EasyMock.verify(mockDevice);

        TestSuiteStub testSuiteStub =
                (TestSuiteStub)
                        configMap
                                .get("arm64-v8a suite/stub-parameterized[instant]")
                                .getTests()
                                .get(0);
        assertEquals(0, testSuiteStub.getIncludeAnnotations().size());
        // This is added by InstantAppHandler to avoid running full mode tests in instant mode
        assertTrue(
                testSuiteStub
                        .getExcludeAnnotations()
                        .contains("android.platform.test.annotations.AppModeFull"));
        // This should not be set. When coming from the suite or anywhere else, in instant mode
        // that filter is eliminated to properly run instant mode.
        assertFalse(
                testSuiteStub
                        .getExcludeAnnotations()
                        .contains("android.platform.test.annotations.AppModeInstant"));
    }

    /** Ensure parameterized modules are created properly even when main abi is filtered. */
    @Test
    public void testLoadTests_parameterizedModule_load_with_filter() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        Set<String> excludeModule = new HashSet<>();
        excludeModule.add("arm64-v8a suite/load-filter-test");
        mRunner.setExcludeFilter(excludeModule);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "test-filter-load");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(2, configMap.size());
        // Config main abi non-parameterized is filtered, this shouldn't prevent the parameterized
        // version from being created, and the other abi.
        assertTrue(configMap.containsKey("arm64-v8a suite/load-filter-test[instant]"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/load-filter-test"));
        EasyMock.verify(mockDevice);
    }

    /** Ensure parameterized modules are filtered when requested. */
    @Test
    public void testLoadTests_parameterizedModule_load_with_filter_param() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        Set<String> excludeModule = new HashSet<>();
        excludeModule.add("arm64-v8a suite/load-filter-test[instant]");
        mRunner.setExcludeFilter(excludeModule);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "test-filter-load");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(2, configMap.size());
        // Config main abi parameterized is filtered, this shouldn't prevent the non-parameterized
        // version from being created, and the other abi.
        assertTrue(configMap.containsKey("arm64-v8a suite/load-filter-test"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/load-filter-test"));
        EasyMock.verify(mockDevice);
    }

    /**
     * If include-filters specify a base module id but we forced a specific parameter we can
     * implicitly decide to match it as a parameterized one.
     */
    @Test
    public void testLoadTests_forcedModule_load_with_filter_param() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        mRunner.setModuleParameter(ModuleParameters.INSTANT_APP);
        Set<String> includeModule = new HashSet<>();
        includeModule.add("arm64-v8a suite/load-filter-test");
        mRunner.setIncludeFilter(includeModule);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "test-filter-load");
        setter.setOptionValue("enable-parameterized-modules", "true");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(1, configMap.size());
        // Config main abi parameterized is filtered, this shouldn't prevent the non-parameterized
        // version from being created, and the other abi.
        assertTrue(configMap.containsKey("arm64-v8a suite/load-filter-test[instant]"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test loading a parameterized config with a multi_abi specification. In this case all abis are
     * created.
     */
    @Test
    public void testLoadTests_parameterizedModule_multiAbi() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-abi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(5, configMap.size());
        // stub-parameterized-abi2 is not parameterized so by default both abi are created.
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi2"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub-parameterized-abi2"));
        // stub-parameterized-abi is parameterized and multi_abi so it creates all the combinations.
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi"));
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi[instant]"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub-parameterized-abi"));
        EasyMock.verify(mockDevice);
    }

    @Test
    public void testLoadTests_parameterizedModule_multiAbi_filter() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        Set<String> includeFilters = new HashSet<>();
        includeFilters.add("suite/stub-parameterized-abi[instant]");
        mRunner.setIncludeFilter(includeFilters);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "multi-params");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(1, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi[instant]"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test when a config supports multi_abi and is forced to run one parameter. In this case only
     * the parameter is run.
     */
    @Test
    public void testLoadTests_parameterizedModule_multiAbi_forced() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-abi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("module-parameter", "INSTANT_APP");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(1, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi[instant]"));
        EasyMock.verify(mockDevice);
    }

    @Test
    public void testLoadTests_parameterizedModule_only_instant() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-abi-alone");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("module-parameter", "INSTANT_APP");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(0, configMap.size());
        EasyMock.verify(mockDevice);
    }

    /**
     * Test when the config supports multi_abi and is run with a parameter to ignore its
     * parameterization. In this case all standard abi are created and not parameter.
     */
    @Test
    public void testLoadTests_parameterizedModule_multiAbi_forcedNotInstant() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-abi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("module-parameter", "NOT_INSTANT_APP");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(4, configMap.size());
        // stub-parameterized-abi2 is not parameterized so by default both abi are created.
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi2"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub-parameterized-abi2"));
        // stub-parameterized-abi is parameterized and multi_abi so it creates all the combinations.
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi"));
        //assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi[instant]"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub-parameterized-abi"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test loading a parameterized config with a not_multi_abi specification. In this case, only
     * the primary abi is created.
     */
    @Test
    public void testLoadTests_parameterizedModule_notMultiAbi() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-not-multi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(2, configMap.size());
        // stub-parameterized-abi is parameterized and not multi_abi so it creates only one abi
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi4"));
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi4[instant]"));
        EasyMock.verify(mockDevice);
    }

    @Test
    public void testLoadTests_parameterizedModule_notMultiAbi_withFilter() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-not-multi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        Set<String> includeModules = new HashSet<>();
        includeModules.add("suite/stub-parameterized-abi4");
        mRunner.setIncludeFilter(includeModules);
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(1, configMap.size());
        // stub-parameterized-abi is parameterized and not multi_abi so it creates only one abi
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi4"));
        EasyMock.verify(mockDevice);
    }

    @Test
    public void testLoadTests_parameterizedModule_filter() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-not-multi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("exclude-module-parameters", "instant_app");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(1, configMap.size());
        // stub-parameterized-abi is parameterized and not multi_abi so it creates only one abi
        // instant_app is filtered so only the regular version of it is created.
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi4"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test that when we filter some parameterization they are correctly excluded from the run
     * setup.
     */
    @Test
    public void testLoadTests_parameterizedModule_forced() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-not-multi");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("module-parameter", "INSTANT_APP");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(1, configMap.size());
        // stub-parameterized-abi is parameterized and not multi_abi so it creates only one abi
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized-abi4[instant]"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test loading a parameterized config with a multiple parameter from the same family. This gets
     * rejected as they are mutually exclusive.
     */
    @Test
    public void testLoadTests_parameterizedModule_mutuallyExclusiveFamily() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters-fail");
        setter.setOptionValue("enable-parameterized-modules", "true");
        EasyMock.replay(mockDevice);
        try {
            mRunner.loadTests();
            fail("Should have thrown an exception.");
        } catch (RuntimeException expected) {
            // expected
            assertEquals(
                    "Error parsing configuration: suite/stub-parameterized-abi3: "
                            + "'Module suite/stub-parameterized-abi3 is declaring parameter: "
                            + "not_instant_app and instant_app when only one expected.'",
                    expected.getMessage());
        }
        EasyMock.verify(mockDevice);
    }

    /** Test that loading the option parameterization is gated by the option. */
    @Test
    public void testLoadTests_optionalParameterizedModule() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("enable-optional-parameterization", "true");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        assertEquals(4, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized"));
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized[instant]"));
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized[secondary_user]"));
        assertTrue(configMap.containsKey("armeabi-v7a suite/stub-parameterized"));
        EasyMock.verify(mockDevice);
    }

    /** Test that we can explicitly request the option parameterization type. */
    @Test
    public void testLoadTests_optionalParameterizedModule_filter() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("enable-optional-parameterization", "true");
        setter.setOptionValue("module-parameter", "SECONDARY_USER");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.testtype.suite.TestSuiteStub:"
                        + "exclude-annotation:android.platform.test.annotations.AppModeInstant");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        // Only the secondary_user requested is created
        assertEquals(1, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized[secondary_user]"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test for {@link BaseTestSuite#loadTests()} when loading a configuration with parameterized
     * metadata that target preparer options are injected after preparers are added to the config.
     */
    @Test
    public void testLoadTests_parameterizedModule_optionSetupAfterConfigAdded() throws Exception {
        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        OptionSetter setter = new OptionSetter(mRunner);
        setter.setOptionValue("suite-config-prefix", "suite");
        setter.setOptionValue("run-suite-tag", "example-suite-parameters");
        setter.setOptionValue("enable-parameterized-modules", "true");
        setter.setOptionValue("enable-optional-parameterization", "true");
        setter.setOptionValue("module-parameter", "SECONDARY_USER");
        setter.setOptionValue(
                "test-arg",
                "com.android.tradefed.targetprep.CreateUserPreparer:reuse-test-user:true");
        EasyMock.replay(mockDevice);
        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        // We only create the primary abi of the parameterized module version.
        assertEquals(1, configMap.size());
        assertTrue(configMap.containsKey("arm64-v8a suite/stub-parameterized[secondary_user]"));
        EasyMock.verify(mockDevice);

        IConfiguration config = configMap.get("arm64-v8a suite/stub-parameterized[secondary_user]");

        // Check if the target preparer was correctly inserted in addParameterSpecificConfig.
        assertTrue(config.getTargetPreparers().get(0) instanceof CreateUserPreparer);

        // Check if the option mReuseTestUser was injected on the preparer in setUpConfig.
        Field reuseTestUser = CreateUserPreparer.class.getDeclaredField("mReuseTestUser");
        reuseTestUser.setAccessible(true);
        assertTrue(reuseTestUser.getBoolean(config.getTargetPreparers().get(0)));
        reuseTestUser.setAccessible(false);
    }

    private void createConfig(File tmpDir, String name) throws IOException {
        File moduleConfig = new File(tmpDir, name);
        FileUtil.writeToFile("<configuration></configuration>", moduleConfig);
    }
}
