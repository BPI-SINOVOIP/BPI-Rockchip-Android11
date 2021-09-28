/*
 * Copyright (C) 2010 The Android Open Source Project
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
package com.android.tradefed.config;

import static org.junit.Assert.assertNotEquals;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.command.ICommandOptions;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceRecovery;
import com.android.tradefed.device.IDeviceSelection;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TextResultReporter;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IDisableable;

import junit.framework.TestCase;

import org.easymock.EasyMock;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@link Configuration}.
 */
public class ConfigurationTest extends TestCase {

    private static final String CONFIG_NAME = "name";
    private static final String CONFIG_DESCRIPTION = "config description";
    private static final String CONFIG_OBJECT_TYPE_NAME = "object_name";
    private static final String OPTION_DESCRIPTION = "bool description";
    private static final String OPTION_NAME = "bool";
    private static final String ALT_OPTION_NAME = "map";

    /**
     * Interface for test object stored in a {@link IConfiguration}.
     */
    private static interface TestConfig {

        public boolean getBool();
    }

    private static class TestConfigObject implements TestConfig, IDisableable {

        @Option(name = OPTION_NAME, description = OPTION_DESCRIPTION, requiredForRerun = true)
        private boolean mBool;

        @Option(name = ALT_OPTION_NAME, description = OPTION_DESCRIPTION)
        private Map<String, Boolean> mBoolMap = new HashMap<String, Boolean>();

        @Option(name = "mandatory-option", mandatory = true)
        private String mMandatory = null;

        private boolean mIsDisabled = false;

        @Override
        public boolean getBool() {
            return mBool;
        }

        public Map<String, Boolean> getMap() {
            return mBoolMap;
        }

        @Override
        public void setDisable(boolean isDisabled) {
            mIsDisabled = isDisabled;
        }

        @Override
        public boolean isDisabled() {
            return mIsDisabled;
        }
    }

    private Configuration mConfig;

    /**
     * {@inheritDoc}
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mConfig = new Configuration(CONFIG_NAME, CONFIG_DESCRIPTION);

        try {
            GlobalConfiguration.createGlobalConfiguration(new String[] {"empty"});
        } catch (IllegalStateException ignored) {
        }
    }

    /**
     * Test that {@link Configuration#getConfigurationObject(String)} can retrieve
     * a previously stored object.
     */
    public void testGetConfigurationObject() throws ConfigurationException {
        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        Object fromConfig = mConfig.getConfigurationObject(CONFIG_OBJECT_TYPE_NAME);
        assertEquals(testConfigObject, fromConfig);
    }

    /**
     * Test {@link Configuration#getConfigurationObjectList(String)}
     */
    @SuppressWarnings("unchecked")
    public void testGetConfigurationObjectList() throws ConfigurationException  {
        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        List<TestConfig> configList = (List<TestConfig>)mConfig.getConfigurationObjectList(
                CONFIG_OBJECT_TYPE_NAME);
        assertEquals(testConfigObject, configList.get(0));
    }

    /**
     * Test that {@link Configuration#getConfigurationObject(String)} with a name that does
     * not exist.
     */
    public void testGetConfigurationObject_wrongname()  {
        assertNull(mConfig.getConfigurationObject("non-existent"));
    }

    /**
     * Test that calling {@link Configuration#getConfigurationObject(String)} for a built-in config
     * type that supports lists.
     */
    public void testGetConfigurationObject_typeIsList()  {
        try {
            mConfig.getConfigurationObject(Configuration.TEST_TYPE_NAME);
            fail("IllegalStateException not thrown");
        } catch (IllegalStateException e) {
            // expected
        }
    }

    /**
     * Test that calling {@link Configuration#getConfigurationObject(String)} for a config type
     * that is a list.
     */
    public void testGetConfigurationObject_forList() throws ConfigurationException  {
        List<TestConfigObject> list = new ArrayList<TestConfigObject>();
        list.add(new TestConfigObject());
        list.add(new TestConfigObject());
        mConfig.setConfigurationObjectList(CONFIG_OBJECT_TYPE_NAME, list);
        try {
            mConfig.getConfigurationObject(CONFIG_OBJECT_TYPE_NAME);
            fail("IllegalStateException not thrown");
        } catch (IllegalStateException e) {
            // expected
        }
    }

    /**
     * Test that setConfigurationObject throws a ConfigurationException when config object provided
     * is not the correct type
     */
    public void testSetConfigurationObject_wrongtype()  {
        try {
            // arbitrarily, use the "Test" type as expected type
            mConfig.setConfigurationObject(Configuration.TEST_TYPE_NAME, new TestConfigObject());
            fail("setConfigurationObject did not throw ConfigurationException");
        } catch (ConfigurationException e) {
            // expected
        }
    }

    /**
     * Test {@link Configuration#getConfigurationObjectList(String)} when config object
     * with given name does not exist.
     */
    public void testGetConfigurationObjectList_wrongname() {
        assertNull(mConfig.getConfigurationObjectList("non-existent"));
    }

    /**
     * Test {@link Configuration#setConfigurationObjectList(String, List)} when config object
     * is the wrong type
     */
    public void testSetConfigurationObjectList_wrongtype() {
        try {
            List<TestConfigObject> myList = new ArrayList<TestConfigObject>(1);
            myList.add(new TestConfigObject());
            // arbitrarily, use the "Test" type as expected type
            mConfig.setConfigurationObjectList(Configuration.TEST_TYPE_NAME, myList);
            fail("setConfigurationObject did not throw ConfigurationException");
        } catch (ConfigurationException e) {
            // expected
        }
    }

    /**
     * Test method for {@link Configuration#getBuildProvider()}.
     */
    public void testGetBuildProvider() throws BuildRetrievalError {
        // check that the default provider is present and doesn't blow up
        assertNotNull(mConfig.getBuildProvider().getBuild());
        // check set and get
        final IBuildProvider provider = EasyMock.createMock(IBuildProvider.class);
        mConfig.setBuildProvider(provider);
        assertEquals(provider, mConfig.getBuildProvider());
    }

    /**
     * Test method for {@link Configuration#getTargetPreparers()}.
     */
    public void testGetTargetPreparers() throws Exception {
        // check that the callback is working and doesn't blow up
        assertEquals(0, mConfig.getTargetPreparers().size());
        // test set and get
        final ITargetPreparer prep = EasyMock.createMock(ITargetPreparer.class);
        mConfig.setTargetPreparer(prep);
        assertEquals(prep, mConfig.getTargetPreparers().get(0));
    }

    /**
     * Test method for {@link Configuration#getTests()}.
     */
    public void testGetTests() throws DeviceNotAvailableException {
        // check that the default test is present and doesn't blow up
        mConfig.getTests()
                .get(0)
                .run(TestInformation.newBuilder().build(), new TextResultReporter());
        IRemoteTest test1 = EasyMock.createMock(IRemoteTest.class);
        mConfig.setTest(test1);
        assertEquals(test1, mConfig.getTests().get(0));
    }

    /**
     * Test method for {@link Configuration#getDeviceRecovery()}.
     */
    public void testGetDeviceRecovery() {
        // check that the default recovery is present
        assertNotNull(mConfig.getDeviceRecovery());
        final IDeviceRecovery recovery = EasyMock.createMock(IDeviceRecovery.class);
        mConfig.setDeviceRecovery(recovery);
        assertEquals(recovery, mConfig.getDeviceRecovery());
    }

    /**
     * Test method for {@link Configuration#getLogOutput()}.
     */
    public void testGetLogOutput() {
        // check that the default logger is present and doesn't blow up
        mConfig.getLogOutput().printLog(LogLevel.INFO, "testGetLogOutput", "test");
        final ILeveledLogOutput logger = EasyMock.createMock(ILeveledLogOutput.class);
        mConfig.setLogOutput(logger);
        assertEquals(logger, mConfig.getLogOutput());
    }

    /**
     * Test method for {@link Configuration#getTestInvocationListeners()}.
     * @throws ConfigurationException
     */
    public void testGetTestInvocationListeners() throws ConfigurationException {
        // check that the default listener is present and doesn't blow up
        ITestInvocationListener defaultListener = mConfig.getTestInvocationListeners().get(0);
        defaultListener.invocationStarted(new InvocationContext());
        defaultListener.invocationEnded(1);

        final ITestInvocationListener listener1 = EasyMock.createMock(
                ITestInvocationListener.class);
        mConfig.setTestInvocationListener(listener1);
        assertEquals(listener1, mConfig.getTestInvocationListeners().get(0));
    }

    /**
     * Test method for {@link Configuration#getCommandOptions()}.
     */
    public void testGetCommandOptions() {
        // check that the default object is present
        assertNotNull(mConfig.getCommandOptions());
        final ICommandOptions cmdOptions = EasyMock.createMock(ICommandOptions.class);
        mConfig.setCommandOptions(cmdOptions);
        assertEquals(cmdOptions, mConfig.getCommandOptions());
    }

    /**
     * Test method for {@link Configuration#getDeviceRequirements()}.
     */
    public void testGetDeviceRequirements() {
        // check that the default object is present
        assertNotNull(mConfig.getDeviceRequirements());
        final IDeviceSelection deviceSelection = EasyMock.createMock(
                IDeviceSelection.class);
        mConfig.setDeviceRequirements(deviceSelection);
        assertEquals(deviceSelection, mConfig.getDeviceRequirements());
    }

    /**
     * Test {@link Configuration#setConfigurationObject(String, Object)} with a
     * {@link IConfigurationReceiver}
     */
    public void testSetConfigurationObject_configReceiver() throws ConfigurationException {
        final IConfigurationReceiver mockConfigReceiver = EasyMock.createMock(
                IConfigurationReceiver.class);
        mockConfigReceiver.setConfiguration(mConfig);
        EasyMock.replay(mockConfigReceiver);
        mConfig.setConfigurationObject("example", mockConfigReceiver);
        EasyMock.verify(mockConfigReceiver);
    }

    /**
     * Test {@link Configuration#injectOptionValue(String, String)}
     */
    public void testInjectOptionValue() throws ConfigurationException {
        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        mConfig.injectOptionValue(OPTION_NAME, Boolean.toString(true));
        assertTrue(testConfigObject.getBool());
        assertEquals(1, mConfig.getConfigurationDescription().getRerunOptions().size());
        OptionDef optionDef = mConfig.getConfigurationDescription().getRerunOptions().get(0);
        assertEquals(OPTION_NAME, optionDef.name);
    }

    /**
     * Test {@link Configuration#injectOptionValue(String, String, String)}
     */
    public void testInjectMapOptionValue() throws ConfigurationException {
        final String key = "hello";

        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        assertEquals(0, testConfigObject.getMap().size());
        mConfig.injectOptionValue(ALT_OPTION_NAME, key, Boolean.toString(true));

        Map<String, Boolean> map = testConfigObject.getMap();
        assertEquals(1, map.size());
        assertNotNull(map.get(key));
        assertTrue(map.get(key).booleanValue());
    }

    /**
     * Test {@link Configuration#injectOptionValue(String, String)} is throwing an exception
     * for map options without no map key provided in the option value
     */
    public void testInjectParsedMapOptionValueNoKey() throws ConfigurationException {
        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        assertEquals(0, testConfigObject.getMap().size());

        try {
            mConfig.injectOptionValue(ALT_OPTION_NAME, "wrong_value");
            fail("ConfigurationException is not thrown for a map option without retrievable key");
        } catch (ConfigurationException ignore) {
            // expected
        }
    }

    /**
     * Test {@link Configuration#injectOptionValue(String, String)} is throwing an exception
     * for map options with ambiguous map key provided in the option value (multiple equal signs)
     */
    public void testInjectParsedMapOptionValueAmbiguousKey() throws ConfigurationException {
        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        assertEquals(0, testConfigObject.getMap().size());

        try {
            mConfig.injectOptionValue(ALT_OPTION_NAME, "a=b=c");
            fail("ConfigurationException is not thrown for a map option with ambiguous key");
        } catch (ConfigurationException ignore) {
            // expected
        }
    }

    /**
     * Test {@link Configuration#injectOptionValue(String, String)} is correctly parsing map options
     */
    public void testInjectParsedMapOptionValue() throws ConfigurationException {
        final String key = "hello\\=key";

        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        assertEquals(0, testConfigObject.getMap().size());
        mConfig.injectOptionValue(ALT_OPTION_NAME, key + "=" + Boolean.toString(true));

        Map<String, Boolean> map = testConfigObject.getMap();
        assertEquals(1, map.size());
        assertNotNull(map.get(key));
        assertTrue(map.get(key));
    }

    /**
     * Test {@link Configuration#injectOptionValues(List)}
     */
    public void testInjectOptionValues() throws ConfigurationException {
        final String key = "hello";
        List<OptionDef> options = new ArrayList<>();
        options.add(new OptionDef(OPTION_NAME, Boolean.toString(true), null));
        options.add(new OptionDef(ALT_OPTION_NAME, key, Boolean.toString(true), null));

        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        mConfig.injectOptionValues(options);

        assertTrue(testConfigObject.getBool());
        Map<String, Boolean> map = testConfigObject.getMap();
        assertEquals(1, map.size());
        assertNotNull(map.get(key));
        assertTrue(map.get(key).booleanValue());
        assertEquals(1, mConfig.getConfigurationDescription().getRerunOptions().size());
        OptionDef optionDef = mConfig.getConfigurationDescription().getRerunOptions().get(0);
        assertEquals(OPTION_NAME, optionDef.name);
    }

    /**
     * Basic test for {@link Configuration#printCommandUsage(boolean, java.io.PrintStream)}.
     */
    public void testPrintCommandUsage() throws ConfigurationException {
        TestConfigObject testConfigObject = new TestConfigObject();
        mConfig.setConfigurationObject(CONFIG_OBJECT_TYPE_NAME, testConfigObject);
        // dump the print stream results to the ByteArrayOutputStream, so contents can be evaluated
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        PrintStream mockPrintStream = new PrintStream(outputStream);
        mConfig.printCommandUsage(false, mockPrintStream);

        // verifying exact contents would be prone to high-maintenance, so instead, just validate
        // all expected names are present
        final String usageString = outputStream.toString();
        assertTrue("Usage text does not contain config name", usageString.contains(CONFIG_NAME));
        assertTrue("Usage text does not contain config description", usageString.contains(
                CONFIG_DESCRIPTION));
        assertTrue("Usage text does not contain object name", usageString.contains(
                CONFIG_OBJECT_TYPE_NAME));
        assertTrue("Usage text does not contain option name", usageString.contains(OPTION_NAME));
        assertTrue("Usage text does not contain option description",
                usageString.contains(OPTION_DESCRIPTION));

        // ensure help prints out options from default config types
        assertTrue("Usage text does not contain --serial option name",
                usageString.contains("serial"));
    }

    /**
     * Test that {@link Configuration#validateOptions()} doesn't throw when all mandatory fields
     * are set.
     */
    public void testValidateOptions() throws ConfigurationException {
        mConfig.validateOptions();
    }

    /**
     * Test that {@link Configuration#validateOptions()} throw when all mandatory fields are not set
     * and object is not disabled.
     */
    public void testValidateOptions_nonDisabledObject() throws ConfigurationException {
        TestConfigObject object = new TestConfigObject();
        object.setDisable(false);
        mConfig.setConfigurationObject("helper", object);
        try {
            mConfig.validateOptions();
            fail("Should have thrown an exception.");
        } catch (ConfigurationException expected) {
            assertTrue(expected.getMessage().contains("Found missing mandatory options"));
        }
    }

    /**
     * Test that {@link Configuration#validateOptions()} doesn't throw when all mandatory fields are
     * not set but the object is disabled.
     */
    public void testValidateOptions_disabledObject() throws ConfigurationException {
        TestConfigObject object = new TestConfigObject();
        object.setDisable(true);
        mConfig.setConfigurationObject("helper", object);
        mConfig.validateOptions();
    }

    /**
     * Test that {@link Configuration#validateOptions()} throws a config exception when shard
     * count is negative number.
     */
    public void testValidateOptionsShardException() throws ConfigurationException {
        ICommandOptions option = new CommandOptions() {
            @Override
            public Integer getShardCount() {return -1;}
        };
        mConfig.setConfigurationObject(Configuration.CMD_OPTIONS_TYPE_NAME, option);
        try {
            mConfig.validateOptions();
            fail("Should have thrown an exception.");
        } catch(ConfigurationException expected) {
            assertEquals("a shard count must be a positive number", expected.getMessage());
        }
    }

    /**
     * Test that {@link Configuration#validateOptions()} throws a config exception when shard
     * index is not valid.
     */
    public void testValidateOptionsShardIndexException() throws ConfigurationException {
        ICommandOptions option = new CommandOptions() {
            @Override
            public Integer getShardIndex() {
                return -1;
            }
        };
        mConfig.setConfigurationObject(Configuration.CMD_OPTIONS_TYPE_NAME, option);
        try {
            mConfig.validateOptions();
            fail("Should have thrown an exception.");
        } catch(ConfigurationException expected) {
            assertEquals("a shard index must be in range [0, shard count)", expected.getMessage());
        }
    }

    /**
     * Test that {@link Configuration#validateOptions()} throws a config exception when shard
     * index is above the shard count.
     */
    public void testValidateOptionsShardIndexAboveShardCount() throws ConfigurationException {
        ICommandOptions option = new CommandOptions() {
            @Override
            public Integer getShardIndex() {
                return 3;
            }
            @Override
            public Integer getShardCount() {
                return 2;
            }
        };
        mConfig.setConfigurationObject(Configuration.CMD_OPTIONS_TYPE_NAME, option);
        try {
            mConfig.validateOptions();
            fail("Should have thrown an exception.");
        } catch(ConfigurationException expected) {
            assertEquals("a shard index must be in range [0, shard count)", expected.getMessage());
        }
    }

    /**
     * Ensure that dynamic file download is not triggered in the parent invocation of local
     * sharding. If that was the case, the downloaded files would be cleaned up right after the
     * shards are kicked-off in new invocations.
     */
    public void testValidateOptions_localSharding_skipDownload() throws Exception {
        mConfig =
                new Configuration(CONFIG_NAME, CONFIG_DESCRIPTION) {
                    @Override
                    protected boolean isRemoteEnvironment() {
                        return false;
                    }
                };
        CommandOptions options = new CommandOptions();
        options.setShardCount(5);
        options.setShardIndex(null);
        mConfig.setCommandOptions(options);
        TestDeviceOptions deviceOptions = new TestDeviceOptions();
        File fakeConfigFile = new File("gs://bucket/remote/file/path");
        deviceOptions.setAvdConfigFile(fakeConfigFile);
        mConfig.setDeviceOptions(deviceOptions);

        // No exception for download is thrown because no download occurred.
        mConfig.validateOptions();
        mConfig.resolveDynamicOptions(new DynamicRemoteFileResolver());
        // Dynamic file is not resolved.
        assertEquals(fakeConfigFile, deviceOptions.getAvdConfigFile());
    }

    /**
     * Test that {@link Configuration#dumpXml(PrintWriter)} produce the xml output.
     */
    public void testDumpXml() throws IOException {
        File test = FileUtil.createTempFile("dumpxml", "xml");
        try {
            PrintWriter out = new PrintWriter(test);
            mConfig.dumpXml(out);
            out.flush();
            String content = FileUtil.readStringFromFile(test);
            assertTrue(content.length() > 100);
            assertTrue(content.contains("<configuration>"));
            assertTrue(content.contains("<test class"));
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    /**
     * Test that {@link Configuration#dumpXml(PrintWriter)} produce the xml output without objects
     * that have been filtered.
     */
    public void testDumpXml_withFilter() throws IOException {
        File test = FileUtil.createTempFile("dumpxml", "xml");
        try {
            PrintWriter out = new PrintWriter(test);
            List<String> filters = new ArrayList<>();
            filters.add(Configuration.TEST_TYPE_NAME);
            mConfig.dumpXml(out, filters);
            out.flush();
            String content = FileUtil.readStringFromFile(test);
            assertTrue(content.length() > 100);
            assertTrue(content.contains("<configuration>"));
            assertFalse(content.contains("<test class"));
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    /**
     * Test that {@link Configuration#dumpXml(PrintWriter)} produce the xml output even for a multi
     * device situation.
     */
    public void testDumpXml_multi_device() throws Exception {
        List<IDeviceConfiguration> deviceObjectList = new ArrayList<IDeviceConfiguration>();
        deviceObjectList.add(new DeviceConfigurationHolder("device1"));
        deviceObjectList.add(new DeviceConfigurationHolder("device2"));
        mConfig.setConfigurationObjectList(Configuration.DEVICE_NAME, deviceObjectList);
        File test = FileUtil.createTempFile("dumpxml", "xml");
        try {
            PrintWriter out = new PrintWriter(test);
            mConfig.dumpXml(out);
            out.flush();
            String content = FileUtil.readStringFromFile(test);
            assertTrue(content.length() > 100);
            assertTrue(content.contains("<device name=\"device1\">"));
            assertTrue(content.contains("<device name=\"device2\">"));
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    /**
     * Test that {@link Configuration#dumpXml(PrintWriter)} produce the xml output even for a multi
     * device situation when one of the device is fake.
     */
    public void testDumpXml_multi_device_fake() throws Exception {
        List<IDeviceConfiguration> deviceObjectList = new ArrayList<IDeviceConfiguration>();
        deviceObjectList.add(new DeviceConfigurationHolder("device1", true));
        deviceObjectList.add(new DeviceConfigurationHolder("device2"));
        mConfig.setConfigurationObjectList(Configuration.DEVICE_NAME, deviceObjectList);
        File test = FileUtil.createTempFile("dumpxml", "xml");
        try {
            PrintWriter out = new PrintWriter(test);
            mConfig.dumpXml(out);
            out.flush();
            String content = FileUtil.readStringFromFile(test);
            assertTrue(content.length() > 100);
            assertTrue(content.contains("<device name=\"device1\" isFake=\"true\">"));
            assertTrue(content.contains("<device name=\"device2\">"));
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    /** Ensure that the dump xml only considere trully changed option on the same object. */
    public void testDumpChangedOption() throws Exception {
        CommandOptions options1 = new CommandOptions();
        Configuration one = new Configuration("test", "test");
        one.setCommandOptions(options1);
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        one.dumpXml(pw, new ArrayList<>(), true, false);
        String noOption = sw.toString();
        assertTrue(
                noOption.contains(
                        "<cmd_options class=\"com.android.tradefed.command.CommandOptions\" />"));

        OptionSetter setter = new OptionSetter(options1);
        setter.setOptionValue("test-tag", "tag-value");
        sw = new StringWriter();
        pw = new PrintWriter(sw);
        one.dumpXml(pw, new ArrayList<>(), true, false);
        String withOption = sw.toString();
        assertTrue(withOption.contains("<option name=\"test-tag\" value=\"tag-value\" />"));

        CommandOptions options2 = new CommandOptions();
        one.setCommandOptions(options2);
        sw = new StringWriter();
        pw = new PrintWriter(sw);
        one.dumpXml(pw, new ArrayList<>(), true, false);
        String differentObject = sw.toString();
        assertTrue(
                differentObject.contains(
                        "<cmd_options class=\"com.android.tradefed.command.CommandOptions\" />"));
    }

    /** Ensure we print modified option if they are structures. */
    public void testDumpChangedOption_structure() throws Exception {
        CommandOptions options1 = new CommandOptions();
        Configuration one = new Configuration("test", "test");
        one.setCommandOptions(options1);
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        one.dumpXml(pw, new ArrayList<>(), true, false);
        String noOption = sw.toString();
        assertTrue(
                noOption.contains(
                        "<cmd_options class=\"com.android.tradefed.command.CommandOptions\" />"));

        OptionSetter setter = new OptionSetter(options1);
        setter.setOptionValue("invocation-data", "key", "value");
        setter.setOptionValue("auto-collect", "LOGCAT_ON_FAILURE");
        sw = new StringWriter();
        pw = new PrintWriter(sw);
        one.dumpXml(pw, new ArrayList<>(), true, false);
        String withOption = sw.toString();
        assertTrue(
                withOption.contains(
                        "<option name=\"invocation-data\" key=\"key\" value=\"value\" />"));
        assertTrue(
                withOption.contains(
                        "<option name=\"auto-collect\" value=\"LOGCAT_ON_FAILURE\" />"));
    }

    public void testDeepClone() throws Exception {
        Configuration original =
                (Configuration)
                        ConfigurationFactory.getInstance()
                                .createConfigurationFromArgs(
                                        new String[] {"instrumentations"}, null, null);
        IConfiguration copy =
                original.partialDeepClone(
                        Arrays.asList(Configuration.DEVICE_NAME, Configuration.TEST_TYPE_NAME),
                        null);
        assertNotEquals(
                original.getDeviceConfigByName(ConfigurationDef.DEFAULT_DEVICE_NAME),
                copy.getDeviceConfigByName(ConfigurationDef.DEFAULT_DEVICE_NAME));
        assertNotEquals(original.getTargetPreparers().get(0), copy.getTargetPreparers().get(0));
        assertNotEquals(
                original.getDeviceConfig().get(0).getTargetPreparers().get(0),
                copy.getDeviceConfig().get(0).getTargetPreparers().get(0));
        assertNotEquals(original.getTests().get(0), copy.getTests().get(0));
        copy.validateOptions();
    }

    public void testDeepClone_innerDevice() throws Exception {
        Configuration original =
                (Configuration)
                        ConfigurationFactory.getInstance()
                                .createConfigurationFromArgs(
                                        new String[] {"instrumentations"}, null, null);
        IConfiguration copy =
                original.partialDeepClone(
                        Arrays.asList(
                                Configuration.TARGET_PREPARER_TYPE_NAME,
                                Configuration.TEST_TYPE_NAME),
                        null);
        assertNotEquals(
                original.getDeviceConfigByName(ConfigurationDef.DEFAULT_DEVICE_NAME),
                copy.getDeviceConfigByName(ConfigurationDef.DEFAULT_DEVICE_NAME));
        assertNotEquals(original.getTargetPreparers().get(0), copy.getTargetPreparers().get(0));
        assertNotEquals(
                original.getDeviceConfig().get(0).getTargetPreparers().get(0),
                copy.getDeviceConfig().get(0).getTargetPreparers().get(0));
        assertNotEquals(original.getTests().get(0), copy.getTests().get(0));
        copy.validateOptions();
    }
}
