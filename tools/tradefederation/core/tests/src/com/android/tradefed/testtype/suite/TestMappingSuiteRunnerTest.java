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
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.HostTest;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.StubTest;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil;
import com.android.tradefed.util.testmapping.TestInfo;
import com.android.tradefed.util.testmapping.TestMapping;
import com.android.tradefed.util.testmapping.TestOption;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/** Unit tests for {@link TestMappingSuiteRunner}. */
@RunWith(JUnit4.class)
public class TestMappingSuiteRunnerTest {

    private static final String ABI_1 = "arm64-v8a";
    private static final String ABI_2 = "armeabi-v7a";
    private static final String DISABLED_PRESUBMIT_TESTS = "disabled-presubmit-tests";
    private static final String EMPTY_CONFIG = "empty";
    private static final String NON_EXISTING_DIR = "non-existing-dir";
    private static final String TEST_CONFIG_NAME = "test";
    private static final String TEST_DATA_DIR = "testdata";
    private static final String TEST_MAPPING = "TEST_MAPPING";
    private static final String TEST_MAPPINGS_ZIP = "test_mappings.zip";

    private TestMappingSuiteRunner mRunner;
    private OptionSetter mOptionSetter;
    private OptionSetter mMainlineOptionSetter;
    private TestMappingSuiteRunner mRunner2;
    private TestMappingSuiteRunner mMainlineRunner;
    private IDeviceBuildInfo mBuildInfo;
    private ITestDevice mMockDevice;
    private TestInformation mTestInfo;

    private static final String TEST_MAINLINE_CONFIG =
        "<configuration description=\"Runs a stub tests part of some suite\">\n"
            + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk\" />"
            + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod2.apk\" />"
            + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk+mod2.apk\" />"
            + "    <option name=\"config-descriptor:metadata\" key=\"mainline-param\" value=\"mod1.apk+mod2.apk+mod3.apk\" />"
            + "    <test class=\"com.android.tradefed.testtype.HostTest\" />\n"
            + "</configuration>";

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
        mRunner = new AbiTestMappingSuite();
        mRunner.setBuild(mBuildInfo);
        mRunner.setDevice(mMockDevice);

        mOptionSetter = new OptionSetter(mRunner);
        mOptionSetter.setOptionValue("suite-config-prefix", "suite");

        mRunner2 = new FakeTestMappingSuiteRunner();
        mRunner2.setBuild(mBuildInfo);
        mRunner2.setDevice(mMockDevice);

        mMainlineRunner = new FakeMainlineTMSR();
        mMainlineRunner.setBuild(mBuildInfo);
        mMainlineRunner.setDevice(mMockDevice);
        mMainlineOptionSetter = new OptionSetter(mMainlineRunner);

        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        context.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();

        EasyMock.expect(mBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR)).andReturn(null)
                .anyTimes();
        EasyMock.expect(mBuildInfo.getTestsDir()).andReturn(new File(NON_EXISTING_DIR)).anyTimes();
        EasyMock.expect(mMockDevice.getProperty(EasyMock.anyObject())).andReturn(ABI_1);
        EasyMock.expect(mMockDevice.getProperty(EasyMock.anyObject())).andReturn(ABI_2);
        EasyMock.replay(mBuildInfo, mMockDevice);
    }

    /**
     * Test TestMappingSuiteRunner that hardcodes the abis to avoid failures related to running the
     * tests against a particular abi build of tradefed.
     */
    public static class AbiTestMappingSuite extends TestMappingSuiteRunner {

        @Override
        public Set<IAbi> getAbis(ITestDevice device) throws DeviceNotAvailableException {
            Set<IAbi> abis = new LinkedHashSet<>();
            abis.add(new Abi(ABI_1, AbiUtils.getBitness(ABI_1)));
            abis.add(new Abi(ABI_2, AbiUtils.getBitness(ABI_2)));
            return abis;
        }
    }

    /**
     * Test TestMappingSuiteRunner that create a fake IConfiguration with fake a test object.
     */
    public static class FakeTestMappingSuiteRunner extends TestMappingSuiteRunner {
        @Override
        public Set<IAbi> getAbis(ITestDevice device) throws DeviceNotAvailableException {
            Set<IAbi> abis = new HashSet<>();
            abis.add(new Abi(ABI_1, AbiUtils.getBitness(ABI_1)));
            abis.add(new Abi(ABI_2, AbiUtils.getBitness(ABI_2)));
            return abis;
        }

        @Override
        public LinkedHashMap<String, IConfiguration> loadingStrategy(Set<IAbi> abis,
            List<File> testsDirs, String suitePrefix, String suiteTag) {
            LinkedHashMap<String, IConfiguration> testConfig = new LinkedHashMap<>();
            try {
                IConfiguration config =
                        ConfigurationFactory.getInstance()
                                .createConfigurationFromArgs(new String[] {EMPTY_CONFIG});
                config.setTest(new StubTest());
                config.getConfigurationDescription().setModuleName(TEST_CONFIG_NAME);
                testConfig.put(TEST_CONFIG_NAME, config);

            } catch (ConfigurationException e) {
                throw new RuntimeException(e);
            }
            return testConfig;
        }
    }

    /**
     * Test TestMappingSuiteRunner that create a fake IConfiguration with fake a test object.
     */
    public static class FakeMainlineTMSR extends TestMappingSuiteRunner {
        @Override
        public Set<IAbi> getAbis(ITestDevice device) throws DeviceNotAvailableException {
            Set<IAbi> abis = new HashSet<>();
            abis.add(new Abi(ABI_1, AbiUtils.getBitness(ABI_1)));
            return abis;
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} to fail when both options include-filter
     * and test-mapping-test-group are set.
     */
    @Test(expected = RuntimeException.class)
    public void testLoadTests_conflictTestGroup() throws Exception {
        mOptionSetter.setOptionValue("include-filter", "test1");
        mOptionSetter.setOptionValue("test-mapping-test-group", "group");
        mRunner.loadTests();
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} to fail when both options include-filter
     * and test-mapping-path are set.
     */
    @Test(expected = RuntimeException.class)
    public void testLoadTests_conflictOptions() throws Exception {
        mOptionSetter.setOptionValue("include-filter", "test1");
        mOptionSetter.setOptionValue("test-mapping-path", "path1");
        mRunner.loadTests();
    }

    /** Test for {@link TestMappingSuiteRunner#loadTests()} to fail when no test option is set. */
    @Test(expected = RuntimeException.class)
    public void testLoadTests_noOption() throws Exception {
        mRunner.loadTests();
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} to fail when option test-mapping-keyword
     * is used but test-mapping-test-group is not set.
     */
    @Test(expected = RuntimeException.class)
    public void testLoadTests_conflictKeyword() throws Exception {
        mOptionSetter.setOptionValue("include-filter", "test1");
        mOptionSetter.setOptionValue("test-mapping-keyword", "key1");
        mRunner.loadTests();
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} for loading tests from test_mappings.zip.
     */
    @Test
    public void testLoadTests_testMappingsZip() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "postsubmit");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile =
                    File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, DISABLED_PRESUBMIT_TESTS);

            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andStubReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir())
                    .andStubReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();

            assertEquals(4, configMap.size());
            assertTrue(configMap.containsKey(ABI_1 + " suite/stub1"));
            assertTrue(configMap.containsKey(ABI_1 + " suite/stub2"));
            assertTrue(configMap.containsKey(ABI_2 + " suite/stub1"));
            assertTrue(configMap.containsKey(ABI_2 + " suite/stub2"));

            // Confirm test sources are stored in test's ConfigurationDescription.
            Map<String, Integer> testSouceCount = new HashMap<>();
            testSouceCount.put("suite/stub1", 1);
            testSouceCount.put("suite/stub2", 1);

            for (IConfiguration config : configMap.values()) {
                assertTrue(testSouceCount.containsKey(config.getName()));
                assertEquals(
                        testSouceCount.get(config.getName()).intValue(),
                        config.getConfigurationDescription()
                                .getMetaData(TestMapping.TEST_SOURCES)
                                .size());
            }

            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} for loading tests matching keywords
     * setting from test_mappings.zip.
     */
    @Test
    public void testLoadTests_testMappingsZipFoundTestsWithKeywords() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-keyword", "key_1");
            mOptionSetter.setOptionValue("test-mapping-test-group", "presubmit");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile =
                    File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, DISABLED_PRESUBMIT_TESTS);

            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andStubReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir())
                    .andStubReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();

            // Only suite/stub2 should be listed as it contains key_1 in keywords.
            assertTrue(mRunner.getIncludeFilter().contains("suite/stub2"));

            assertEquals(2, configMap.size());
            assertTrue(configMap.containsKey(ABI_1 + " suite/stub2"));
            assertTrue(configMap.containsKey(ABI_2 + " suite/stub2"));

            // Confirm test sources are stored in test's ConfigurationDescription.
            // Only the test in test_mapping_1 has keywords matched, so there should be only 1 test
            // source for the test.
            Map<String, Integer> testSouceCount = new HashMap<>();
            testSouceCount.put("suite/stub2", 1);

            for (IConfiguration config : configMap.values()) {
                assertTrue(testSouceCount.containsKey(config.getName()));
                assertEquals(
                        testSouceCount.get(config.getName()).intValue(),
                        config.getConfigurationDescription()
                                .getMetaData(TestMapping.TEST_SOURCES)
                                .size());
            }

            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} for loading tests matching keywords
     * setting from test_mappings.zip and no test should be found.
     */
    @Test(expected = RuntimeException.class)
    public void testLoadTests_testMappingsZipFailWithKeywords() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-keyword", "key_2");
            mOptionSetter.setOptionValue("test-mapping-test-group", "presubmit");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile =
                    File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, DISABLED_PRESUBMIT_TESTS);

            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir()).andReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            // No test should be found with keyword key_2, loadTests method shall raise
            // RuntimeException.
            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} for loading host tests from
     * test_mappings.zip.
     */
    @Test
    public void testLoadTests_testMappingsZipHostTests() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "presubmit");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile =
                    File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, tempDir, DISABLED_PRESUBMIT_TESTS);

            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.HOST_LINKED_DIR))
                    .andReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir()).andReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            mRunner.setPrioritizeHostConfig(true);
            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();

            // Test configs in test_mapping_1 doesn't exist, but should be listed in
            // include-filters.
            assertTrue(mRunner.getIncludeFilter().contains("test1"));
            assertEquals(1, mRunner.getIncludeFilter().size());

            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} for loading tests from test_mappings.zip
     * and run with shard.
     */
    @Test
    public void testLoadTests_shard() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "postsubmit");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(srcDir, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andStubReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir())
                    .andStubReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);
            EasyMock.expect(mockBuildInfo.getRemoteFiles()).andReturn(null).once();

            mTestInfo
                    .getContext()
                    .addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            Collection<IRemoteTest> tests = mRunner.split(2, mTestInfo);
            assertEquals(4, tests.size());
            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} for loading tests from test_mappings.zip
     * and run with shard, and no test is split due to exclude-filter.
     */
    @Test
    public void testLoadTests_shardNoTest() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(srcDir, zipFile);

            mOptionSetter.setOptionValue("test-mapping-test-group", "postsubmit");
            mOptionSetter.setOptionValue("test-mapping-path", srcDir.getName());
            mOptionSetter.setOptionValue("exclude-filter", "suite/stub1");

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andStubReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir())
                    .andStubReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mTestInfo
                    .getContext()
                    .addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            Collection<IRemoteTest> tests = mRunner.split(2, mTestInfo);
            assertEquals(null, tests);
            assertEquals(2, mRunner.getIncludeFilter().size());
            assertEquals(null, mRunner.getTestGroup());
            assertEquals(0, mRunner.getTestMappingPaths().size());
            assertEquals(false, mRunner.getUseTestMappingPath());
            EasyMock.verify(mockBuildInfo);
        } finally {
            // Clean up the static variable due to the usage of option `test-mapping-path`.
            TestMapping.setTestMappingPaths(new ArrayList<String>());
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /** Test for {@link TestMappingSuiteRunner#loadTests()} to fail when no test is found. */
    @Test(expected = RuntimeException.class)
    public void testLoadTests_noTest() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "none-exist");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(srcDir, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getTestsDir()).andReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            mRunner.loadTests();
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} that when a test config supports
     * IAbiReceiver, multiple instances of the config are queued up.
     */
    @Test
    public void testLoadTestsForMultiAbi() throws Exception {
        mOptionSetter.setOptionValue("include-filter", "suite/stubAbi");

        ITestDevice mockDevice = EasyMock.createMock(ITestDevice.class);
        mRunner.setDevice(mockDevice);
        EasyMock.replay(mockDevice);

        LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();

        assertEquals(2, configMap.size());
        assertTrue(configMap.containsKey(ABI_1 + " suite/stubAbi"));
        assertTrue(configMap.containsKey(ABI_2 + " suite/stubAbi"));
        EasyMock.verify(mockDevice);
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} that when force-test-mapping-module is
     * specified, tests would be filtered.
     */
    @Test
    public void testLoadTestsWithModule() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "postsubmit");
            mOptionSetter.setOptionValue("force-test-mapping-module", "suite/stub1");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile =
                    File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, DISABLED_PRESUBMIT_TESTS);

            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andStubReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir())
                    .andStubReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
            assertEquals(2, configMap.size());
            assertTrue(configMap.containsKey(ABI_1 + " suite/stub1"));
            assertTrue(configMap.containsKey(ABI_2 + " suite/stub1"));
            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} that when multi force-test-mapping-module
     * are specified, tests would be filtered.
     */
    @Test
    public void testLoadTestsWithMultiModules() throws Exception {
        File tempDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "postsubmit");
            mOptionSetter.setOptionValue("force-test-mapping-module", "suite/stub1");
            mOptionSetter.setOptionValue("force-test-mapping-module", "suite/stub2");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile =
                File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, DISABLED_PRESUBMIT_TESTS);

            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<File> filesToZip =
                Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andStubReturn(null);
            EasyMock.expect(mockBuildInfo.getTestsDir())
                    .andStubReturn(new File("non-existing-dir"));
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile);

            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
            assertEquals(4, configMap.size());
            assertTrue(configMap.containsKey(ABI_1 + " suite/stub1"));
            assertTrue(configMap.containsKey(ABI_1 + " suite/stub2"));
            assertTrue(configMap.containsKey(ABI_2 + " suite/stub1"));
            assertTrue(configMap.containsKey(ABI_2 + " suite/stub2"));
            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#getTestInfos(Set, String)} that when a module is
     * specified, tests would be still found correctly.
     */
    @Test
    public void testGetTestInfos() throws Exception {
        Set<TestInfo> testInfos = new HashSet<>();
        testInfos.add(createTestInfo("test", "path"));
        testInfos.add(createTestInfo("test", "path2"));
        testInfos.add(createTestInfo("test2", "path2"));

        assertEquals(2, mRunner.getTestInfos(testInfos, "test").size());
        assertEquals(1, mRunner.getTestInfos(testInfos, "test2").size());
    }

    /**
     * Test for {@link TestMappingSuiteRunner#dedupTestInfos(Set)} that tests with the same test
     * options would be filtered out.
     */
    @Test
    public void testDedupTestInfos() throws Exception {
        Set<TestInfo> testInfos = new HashSet<>();
        testInfos.add(createTestInfo("test", "path"));
        testInfos.add(createTestInfo("test", "path2"));
        assertEquals(1, mRunner.dedupTestInfos(testInfos).size());

        TestInfo anotherInfo = new TestInfo("test", "folder3", false);
        anotherInfo.addOption(new TestOption("include-filter", "value1"));
        testInfos.add(anotherInfo);
        assertEquals(2, mRunner.dedupTestInfos(testInfos).size());
    }

    /**
     * Test for {@link TestMappingSuiteRunner#getTestSources(Set)} that test sources would be found
     * correctly.
     */
    @Test
    public void testGetTestSources() throws Exception {
        Set<TestInfo> testInfos = new HashSet<>();
        testInfos.add(createTestInfo("test", "path"));
        testInfos.add(createTestInfo("test", "path2"));
        List<String> results = mRunner.getTestSources(testInfos);
        assertEquals(2, results.size());
    }

    /**
     * Test for {@link TestMappingSuiteRunner#parseOptions(TestInfo)} that the test options are
     * injected correctly.
     */
    @Test
    public void testParseOptions() throws Exception {
        TestInfo info = createTestInfo("test", "path");
        mRunner.parseOptions(info);
        assertEquals(1, mRunner.getIncludeFilter().size());
        assertEquals(1, mRunner.getExcludeFilter().size());
    }

    /**
     * Test for {@link TestMappingSuiteRunner#createIndividualTests(Set, String)} that IRemoteTest
     * object are created according to the test infos with different test options.
     */
    @Test
    public void testCreateIndividualTestsWithDifferentTestInfos() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("tmp");
            File moduleConfig = new File(tempDir, "module_name.config");
            moduleConfig.createNewFile();
            Set<TestInfo> testInfos = new HashSet<>();
            testInfos.add(createTestInfo("test", "path"));
            testInfos.add(createTestInfo("test2", "path"));
            String configPath = moduleConfig.getAbsolutePath();
            assertEquals(2, mRunner2.createIndividualTests(testInfos, configPath, null).size());
            assertEquals(1, mRunner2.getIncludeFilter().size());
            assertEquals(1, mRunner2.getExcludeFilter().size());
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#createIndividualTests(Set, String, IAbi)} that
     * IRemoteTest object are created according to the test infos with multiple test options.
     */
    @Test
    public void testCreateIndividualTestsWithDifferentTestOptions() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("tmp");
            File moduleConfig = new File(tempDir, "module_name.config");
            moduleConfig.createNewFile();
            Set<TestInfo> testInfos = new HashSet<>();
            testInfos.add(createTestInfo("test", "path"));
            TestInfo info = new TestInfo("test", "path", false);
            info.addOption(new TestOption("include-filter", "include-filter"));
            testInfos.add(info);
            String configPath = moduleConfig.getAbsolutePath();
            assertEquals(2, mRunner2.createIndividualTests(testInfos, configPath, null).size());
            assertEquals(1, mRunner2.getIncludeFilter().size());
            assertEquals(0, mRunner2.getExcludeFilter().size());
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    @Test
    public void testLoadTests_moduleDifferentoptions() throws Exception {
        File tempDir = null;
        File tempTestsDir = null;
        try {
            mOptionSetter.setOptionValue("test-mapping-test-group", "presubmit");

            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(srcDir, zipFile);

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andReturn(null)
                    .anyTimes();
            EasyMock.expect(mockBuildInfo.getTestsDir()).andReturn(tempTestsDir).anyTimes();
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile).anyTimes();
            EasyMock.expect(mockBuildInfo.getBuildBranch()).andReturn("branch").anyTimes();
            EasyMock.expect(mockBuildInfo.getBuildFlavor()).andReturn("flavor").anyTimes();
            EasyMock.expect(mockBuildInfo.getBuildId()).andReturn("id").anyTimes();

            IInvocationContext mContext = new InvocationContext();
            mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mockBuildInfo);
            mRunner.setInvocationContext(mContext);
            mRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);
            LinkedHashMap<String, IConfiguration> configMap = mRunner.loadTests();
            assertEquals(2, configMap.size());
            assertTrue(configMap.keySet().contains("armeabi-v7a suite/stub1"));
            assertTrue(configMap.keySet().contains("arm64-v8a suite/stub1"));

            for (Entry<String, IConfiguration> config : configMap.entrySet()) {
                IConfiguration currentConfig = config.getValue();
                IAbi abi = currentConfig.getConfigurationDescription().getAbi();
                // Ensure that all the sub-tests abi match the module abi
                for (IRemoteTest test : currentConfig.getTests()) {
                    if (test instanceof IAbiReceiver) {
                        assertEquals(abi, ((IAbiReceiver) test).getAbi());
                    }
                }
            }
            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
            FileUtil.recursiveDelete(tempTestsDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#loadTests()} that IRemoteTest
     * object are created according to the test infos with multiple test options.
     */
    @Test
    public void testLoadTestsForMainline() throws Exception {
        File tempDir = null;
        File tempTestsDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            tempTestsDir = FileUtil.createTempDir("test_mapping_testcases");

            File zipFile = createTestMappingZip(tempDir);
            createMainlineModuleConfig(tempTestsDir.getAbsolutePath());

            IDeviceBuildInfo mockBuildInfo = EasyMock.createMock(IDeviceBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR))
                    .andReturn(null).anyTimes();
            EasyMock.expect(mockBuildInfo.getTestsDir()).andReturn(tempTestsDir).anyTimes();
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile).anyTimes();
            EasyMock.expect(mockBuildInfo.getBuildBranch()).andReturn("branch").anyTimes();
            EasyMock.expect(mockBuildInfo.getBuildFlavor()).andReturn("flavor").anyTimes();
            EasyMock.expect(mockBuildInfo.getBuildId()).andReturn("id").anyTimes();

            IInvocationContext mContext = new InvocationContext();
            mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mockBuildInfo);
            mMainlineRunner.setInvocationContext(mContext);
            mMainlineRunner.setBuild(mockBuildInfo);
            EasyMock.replay(mockBuildInfo);

            mMainlineOptionSetter.setOptionValue("enable-mainline-parameterized-modules", "true");
            mMainlineOptionSetter.setOptionValue("skip-loading-config-jar", "true");
            mMainlineOptionSetter.setOptionValue("test-mapping-test-group", "mainline-presubmit");
            LinkedHashMap<String, IConfiguration> configMap = mMainlineRunner.loadTests();

            assertEquals(3, configMap.size());
            assertTrue(configMap.containsKey(ABI_1 + " test[mod1.apk]"));
            assertTrue(configMap.containsKey(ABI_1 + " test[mod2.apk]"));
            assertTrue(configMap.containsKey(ABI_1 + " test[mod1.apk+mod2.apk]"));
            HostTest test = (HostTest) configMap.get(ABI_1 + " test[mod1.apk]").getTests().get(0);
            assertTrue(test.getIncludeFilters().contains("test-filter"));

            test = (HostTest) configMap.get(ABI_1 + " test[mod2.apk]").getTests().get(0);
            assertTrue(test.getIncludeFilters().contains("test-filter2"));

            test = (HostTest) configMap.get(ABI_1 + " test[mod1.apk+mod2.apk]").getTests().get(0);
            assertTrue(test.getIncludeFilters().isEmpty());
            assertEquals(1, test.getExcludeAnnotations().size());
            assertEquals("test-annotation", test.getExcludeAnnotations().iterator().next());

            EasyMock.verify(mockBuildInfo);
        } finally {
            FileUtil.recursiveDelete(tempDir);
            FileUtil.recursiveDelete(tempTestsDir);
        }
    }

    /**
     * Test for {@link TestMappingSuiteRunner#createIndividualTests(Set, String, IAbi)} that
     * IRemoteTest object are created according to the test infos with the same test options and
     * name.
     */
    @Test
    public void testCreateIndividualTestsWithSameTestInfos() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("tmp");
            File moduleConfig = new File(tempDir, "module_name.config");
            moduleConfig.createNewFile();
            String configPath = moduleConfig.getAbsolutePath();
            Set<TestInfo> testInfos = new HashSet<>();
            testInfos.add(createTestInfo("test", "path"));
            testInfos.add(createTestInfo("test", "path"));
            assertEquals(1, mRunner2.createIndividualTests(testInfos, configPath, null).size());
            assertEquals(1, mRunner2.getIncludeFilter().size());
            assertEquals(1, mRunner2.getExcludeFilter().size());
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /** Helper to create specific test infos. */
    private TestInfo createTestInfo(String name, String source) {
        TestInfo info = new TestInfo(name, source, false);
        info.addOption(new TestOption("include-filter", name));
        info.addOption(new TestOption("exclude-filter", name));
        info.addOption(new TestOption("other", name));
        return info;
    }

    /** Helper to create test_mappings.zip . */
    private File createTestMappingZip(File tempDir) throws IOException {
        File srcDir = FileUtil.createTempDir("src", tempDir);
        String srcFile =
            File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
        InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
        FileUtil.saveResourceFile(resourceStream, srcDir, DISABLED_PRESUBMIT_TESTS);

        srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_with_mainline";
        resourceStream = this.getClass().getResourceAsStream(srcFile);
        FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
        File subDir = FileUtil.createTempDir("sub_dir", srcDir);
        srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
        resourceStream = this.getClass().getResourceAsStream(srcFile);
        FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

        List<File> filesToZip =
            Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));
        File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
        ZipUtil.createZip(filesToZip, zipFile);

        return zipFile;
    }

    /** Helper to create module config with parameterized mainline modules . */
    private File createMainlineModuleConfig(String tempTestsDir) throws IOException {
        File moduleConfig = new File(tempTestsDir, "test" + SuiteModuleLoader.CONFIG_EXT);
        FileUtil.writeToFile(TEST_MAINLINE_CONFIG, moduleConfig);
        return moduleConfig;
    }
}
