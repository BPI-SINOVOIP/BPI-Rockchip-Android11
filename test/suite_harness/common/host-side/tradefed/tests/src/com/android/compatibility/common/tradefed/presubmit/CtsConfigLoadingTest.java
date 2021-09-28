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
package com.android.compatibility.common.tradefed.presubmit;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.targetprep.ApkInstaller;
import com.android.compatibility.common.tradefed.targetprep.PreconditionPreparer;
import com.android.compatibility.common.tradefed.testtype.JarHostTest;
import com.android.tradefed.build.FolderBuildInfo;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.shard.token.TokenProperty;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.testtype.AndroidJUnitTest;
import com.android.tradefed.testtype.HostTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.suite.ITestSuite;
import com.android.tradefed.testtype.suite.params.ModuleParameters;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Test that configuration in CTS can load and have expected properties.
 */
@RunWith(JUnit4.class)
public class CtsConfigLoadingTest {

    private static final String METADATA_COMPONENT = "component";
    private static final Set<String> KNOWN_COMPONENTS =
            new HashSet<>(
                    Arrays.asList(
                            // modifications to the list below must be reviewed
                            "abuse",
                            "art",
                            "auth",
                            "auto",
                            "autofill",
                            "backup",
                            "bionic",
                            "bluetooth",
                            "camera",
                            "contentcapture",
                            "deviceinfo",
                            "deqp",
                            "devtools",
                            "framework",
                            "graphics",
                            "hdmi",
                            "inputmethod",
                            "libcore",
                            "location",
                            "media",
                            "metrics",
                            "misc",
                            "mocking",
                            "networking",
                            "neuralnetworks",
                            "print",
                            "renderscript",
                            "security",
                            "statsd",
                            "systems",
                            "sysui",
                            "telecom",
                            "tv",
                            "uitoolkit",
                            "vr",
                            "webview",
                            "wifi"));
    private static final Set<String> KNOWN_MISC_MODULES =
            new HashSet<>(
                    Arrays.asList(
                            // Modifications to the list below must be approved by someone in
                            // test/suite_harness/OWNERS.
                            "CtsSliceTestCases.config",
                            "CtsSampleDeviceTestCases.config",
                            "CtsUsbTests.config",
                            "CtsGpuToolsHostTestCases.config",
                            "CtsEdiHostTestCases.config",
                            "CtsClassLoaderFactoryPathClassLoaderTestCases.config",
                            "CtsSampleHostTestCases.config",
                            "CtsHardwareTestCases.config",
                            "CtsMonkeyTestCases.config",
                            "CtsAndroidAppTestCases.config",
                            "CtsClassLoaderFactoryInMemoryDexClassLoaderTestCases.config",
                            "CtsAppComponentFactoryTestCases.config",
                            "CtsSeccompHostTestCases.config"));

    /**
     * List of the officially supported runners in CTS, they meet all the interfaces criteria as
     * well as support sharding very well. Any new addition should go through a review.
     */
    private static final Set<String> SUPPORTED_CTS_TEST_TYPE = new HashSet<>(Arrays.asList(
            // Cts runners
            "com.android.compatibility.common.tradefed.testtype.JarHostTest",
            "com.android.compatibility.testtype.DalvikTest",
            "com.android.compatibility.testtype.LibcoreTest",
            "com.drawelements.deqp.runner.DeqpTestRunner",
            // Tradefed runners
            "com.android.tradefed.testtype.AndroidJUnitTest",
            "com.android.tradefed.testtype.HostTest",
            "com.android.tradefed.testtype.GTest"
    ));

    /**
     * In Most cases we impose the usage of the AndroidJUnitRunner because it supports all the
     * features required (filtering, sharding, etc.). We do not typically expect people to need a
     * different runner.
     */
    private static final Set<String> ALLOWED_INSTRUMENTATION_RUNNER_NAME = new HashSet<>();
    static {
        ALLOWED_INSTRUMENTATION_RUNNER_NAME.add("android.support.test.runner.AndroidJUnitRunner");
        ALLOWED_INSTRUMENTATION_RUNNER_NAME.add("androidx.test.runner.AndroidJUnitRunner");
    }
    private static final Set<String> RUNNER_EXCEPTION = new HashSet<>();
    static {
        // Used for a bunch of system-api cts tests
        RUNNER_EXCEPTION.add("repackaged.android.test.InstrumentationTestRunner");
        // Used by a UiRendering scenario where an activity is persisted between tests
        RUNNER_EXCEPTION.add("android.uirendering.cts.runner.UiRenderingRunner");
    }

    /**
     * Families of module parameterization that MUST be specified explicitly in the module
     * AndroidTest.xml.
     */
    private static final Set<String> MANDATORY_PARAMETERS_FAMILY = new HashSet<>();

    static {
        MANDATORY_PARAMETERS_FAMILY.add(ModuleParameters.INSTANT_APP_FAMILY);
        MANDATORY_PARAMETERS_FAMILY.add(ModuleParameters.MULTI_ABI_FAMILY);
        MANDATORY_PARAMETERS_FAMILY.add(ModuleParameters.SECONDARY_USER_FAMILY);
    }

    /**
     * Whitelist to start enforcing metadata on modules. No additional entry will be allowed! This
     * is meant to burn down the remaining modules definition.
     */
    private static final Set<String> WHITELIST_MODULE_PARAMETERS = new HashSet<>();

    static {
        WHITELIST_MODULE_PARAMETERS.add("CtsAccessibilityServiceTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsActivityManagerBackgroundActivityTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsAppOpsTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsCarrierApiTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsContentCaptureServiceTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsDeqpTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsHiddenApiKillswitchDebugClassTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsHiddenApiKillswitchWhitelistTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsHiddenApiKillswitchWildcardTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsLocationTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsLocation2TestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsMediaTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsMediaV2TestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsOpenGlPerfTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsOsTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsPermission2TestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsPermissionTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsProviderUiTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsRsBlasTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsSkQPTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsWrapNoWrapTestCases.config");
        WHITELIST_MODULE_PARAMETERS.add("CtsWrapWrapDebugMallocDebugTestCases.config");
    }

    /**
     * Test that configuration shipped in Tradefed can be parsed.
     * -> Exclude deprecated ApkInstaller.
     * -> Check if host-side tests are non empty.
     */
    @Test
    public void testConfigurationLoad() throws Exception {
        String ctsRoot = System.getProperty("CTS_ROOT");
        File testcases = new File(ctsRoot, "/android-cts/testcases/");
        if (!testcases.exists()) {
            fail(String.format("%s does not exists", testcases));
            return;
        }
        File[] listConfig = testcases.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String name) {
                if (name.endsWith(".config")) {
                    return true;
                }
                return false;
            }
        });
        assertTrue(listConfig.length > 0);
        // Create a FolderBuildInfo to similate the CompatibilityBuildProvider
        FolderBuildInfo stubFolder = new FolderBuildInfo("-1", "-1");
        stubFolder.setRootDir(new File(ctsRoot));
        stubFolder.addBuildAttribute(CompatibilityBuildHelper.SUITE_NAME, "CTS");
        stubFolder.addBuildAttribute("ROOT_DIR", ctsRoot);
        TestInformation stubTestInfo = TestInformation.newBuilder().build();
        stubTestInfo.executionFiles().put(FilesKey.TESTS_DIRECTORY, new File(ctsRoot));

        List<String> missingMandatoryParameters = new ArrayList<>();
        // We expect to be able to load every single config in testcases/
        for (File config : listConfig) {
            IConfiguration c = ConfigurationFactory.getInstance()
                    .createConfigurationFromArgs(new String[] {config.getAbsolutePath()});
            // Ensure the deprecated ApkInstaller is not used anymore.
            for (ITargetPreparer prep : c.getTargetPreparers()) {
                if (prep.getClass().isAssignableFrom(ApkInstaller.class)) {
                    throw new ConfigurationException(
                            String.format("%s: Use com.android.tradefed.targetprep.suite."
                                    + "SuiteApkInstaller instead of com.android.compatibility."
                                    + "common.tradefed.targetprep.ApkInstaller, options will be "
                                    + "the same.", config));
                }
                if (prep.getClass().isAssignableFrom(PreconditionPreparer.class)) {
                    throw new ConfigurationException(
                            String.format(
                                    "%s: includes a PreconditionPreparer (%s) which is not allowed"
                                            + " in modules.",
                                    config.getName(), prep.getClass()));
                }
            }
            // We can ensure that Host side tests are not empty.
            for (IRemoteTest test : c.getTests()) {
                // Check that all the tests runners are well supported.
                if (!SUPPORTED_CTS_TEST_TYPE.contains(test.getClass().getCanonicalName())) {
                    throw new ConfigurationException(
                            String.format(
                                    "testtype %s is not officially supported by CTS. "
                                            + "The supported ones are: %s",
                                    test.getClass().getCanonicalName(), SUPPORTED_CTS_TEST_TYPE));
                }
                if (test instanceof HostTest) {
                    HostTest hostTest = (HostTest) test;
                    // We inject a made up folder so that it can find the tests.
                    hostTest.setBuild(stubFolder);
                    hostTest.setTestInformation(stubTestInfo);
                    int testCount = hostTest.countTestCases();
                    if (testCount == 0) {
                        throw new ConfigurationException(
                                String.format("%s: %s reports 0 test cases.",
                                        config.getName(), test));
                    }
                }
                // Tests are expected to implement that interface.
                if (!(test instanceof ITestFilterReceiver)) {
                    throw new IllegalArgumentException(String.format(
                            "Test in module %s must implement ITestFilterReceiver.",
                            config.getName()));
                }
                // Ensure that the device runner is the AJUR one if explicitly specified.
                if (test instanceof AndroidJUnitTest) {
                    AndroidJUnitTest instru = (AndroidJUnitTest) test;
                    if (instru.getRunnerName() != null &&
                            !ALLOWED_INSTRUMENTATION_RUNNER_NAME.contains(instru.getRunnerName())) {
                        // Some runner are exempt
                        if (!RUNNER_EXCEPTION.contains(instru.getRunnerName())) {
                            throw new ConfigurationException(
                                    String.format("%s: uses '%s' instead of on of '%s' that are "
                                            + "expected", config.getName(), instru.getRunnerName(),
                                            ALLOWED_INSTRUMENTATION_RUNNER_NAME));
                        }
                    }
                }
            }
            ConfigurationDescriptor cd = c.getConfigurationDescription();
            Assert.assertNotNull(config + ": configuration descriptor is null", cd);
            List<String> component = cd.getMetaData(METADATA_COMPONENT);
            Assert.assertNotNull(String.format("Missing module metadata field \"component\", "
                    + "please add the following line to your AndroidTest.xml:\n"
                    + "<option name=\"config-descriptor:metadata\" key=\"component\" "
                    + "value=\"...\" />\nwhere \"value\" must be one of: %s\n"
                    + "config: %s", KNOWN_COMPONENTS, config),
                    component);
            Assert.assertEquals(String.format("Module config contains more than one \"component\" "
                    + "metadata field: %s\nconfig: %s", component, config),
                    1, component.size());
            String cmp = component.get(0);
            Assert.assertTrue(String.format("Module config contains unknown \"component\" metadata "
                    + "field \"%s\", supported ones are: %s\nconfig: %s",
                    cmp, KNOWN_COMPONENTS, config), KNOWN_COMPONENTS.contains(cmp));

            if ("misc".equals(cmp)) {
                String configFileName = config.getName();
                Assert.assertTrue(
                        String.format(
                                "Adding new module %s to \"misc\" component is restricted, "
                                        + "please pick a component that your module fits in",
                                configFileName),
                        KNOWN_MISC_MODULES.contains(configFileName));
            }

            // Check that specified parameters are expected
            boolean res =
                    checkModuleParameters(
                            config.getName(), cd.getMetaData(ITestSuite.PARAMETER_KEY));
            if (!res) {
                missingMandatoryParameters.add(config.getName());
            }
            // Check that specified tokens are expected
            checkTokens(config.getName(), cd.getMetaData(ITestSuite.TOKEN_KEY));

            // Ensure each CTS module is tagged with <option name="test-suite-tag" value="cts" />
            Assert.assertTrue(String.format(
                    "Module config %s does not contains "
                    + "'<option name=\"test-suite-tag\" value=\"cts\" />'", config.getName()),
                    cd.getSuiteTags().contains("cts"));

            // Check not-shardable: JarHostTest cannot create empty shards so it should never need
            // to be not-shardable.
            if (cd.isNotShardable()) {
                for (IRemoteTest test : c.getTests()) {
                    if (test.getClass().isAssignableFrom(JarHostTest.class)) {
                        throw new ConfigurationException(
                                String.format("config: %s. JarHostTest does not need the "
                                    + "not-shardable option.", config.getName()));
                    }
                }
            }
            // Ensure options have been set
            c.validateOptions();
        }

        // Exempt the whitelist
        missingMandatoryParameters.removeAll(WHITELIST_MODULE_PARAMETERS);
        // Ensure the mandatory fields are filled
        if (!missingMandatoryParameters.isEmpty()) {
            String msg =
                    String.format(
                            "The following %s modules are missing some of the mandatory "
                                    + "parameters [instant_app, not_instant_app, "
                                    + "multi_abi, not_multi_abi, "
                                    + "secondary_user, not_secondary_user]: '%s'",
                            missingMandatoryParameters.size(), missingMandatoryParameters);
            throw new ConfigurationException(msg);
        }
    }

    /** Test that all parameter metadata can be resolved. */
    private boolean checkModuleParameters(String configName, List<String> parameters)
            throws ConfigurationException {
        if (parameters == null) {
            return false;
        }
        Map<String, Boolean> families = createFamilyCheckMap();
        for (String param : parameters) {
            try {
                ModuleParameters p = ModuleParameters.valueOf(param.toUpperCase());
                if (families.containsKey(p.getFamily())) {
                    families.put(p.getFamily(), true);
                }
            } catch (IllegalArgumentException e) {
                throw new ConfigurationException(
                        String.format("Config: %s includes an unknown parameter '%s'.",
                                configName, param));
            }
        }
        if (families.containsValue(false)) {
            return false;
        }
        return true;
    }

    /** Test that all tokens can be resolved. */
    private void checkTokens(String configName, List<String> tokens) throws ConfigurationException {
        if (tokens == null) {
            return;
        }
        for (String token : tokens) {
            try {
                TokenProperty.valueOf(token.toUpperCase());
            } catch (IllegalArgumentException e) {
                throw new ConfigurationException(
                        String.format(
                                "Config: %s includes an unknown token '%s'.", configName, token));
            }
        }
    }

    private Map<String, Boolean> createFamilyCheckMap() {
        Map<String, Boolean> families = new HashMap<>();
        for (String family : MANDATORY_PARAMETERS_FAMILY) {
            families.put(family, false);
        }
        return families;
    }
}
