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
package com.android.tradefed.presubmit;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.ConfigurationUtil;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationFactory;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TestAppInstallSetup;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.suite.ValidateSuiteConfigHelper;

import com.google.common.base.Joiner;

import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Validation tests to run against the configuration in general-tests.zip to ensure they can all
 * parse.
 *
 * <p>Do not add to UnitTests.java. This is meant to run standalone.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class GeneralTestsConfigValidation implements IBuildReceiver {

    private IBuildInfo mBuild;

    /**
     * List of the officially supported runners in general-tests. Any new addition should go through
     * a review to ensure all runners have a high quality bar.
     */
    private static final Set<String> SUPPORTED_TEST_RUNNERS =
            new HashSet<>(
                    Arrays.asList(
                            // Cts runners
                            "com.android.compatibility.common.tradefed.testtype.JarHostTest",
                            "com.android.compatibility.testtype.DalvikTest",
                            "com.android.compatibility.testtype.LibcoreTest",
                            "com.drawelements.deqp.runner.DeqpTestRunner",
                            // Tradefed runners
                            "com.android.tradefed.testtype.UiAutomatorTest",
                            "com.android.tradefed.testtype.InstrumentationTest",
                            "com.android.tradefed.testtype.AndroidJUnitTest",
                            "com.android.tradefed.testtype.HostTest",
                            "com.android.tradefed.testtype.GTest",
                            "com.android.tradefed.testtype.HostGTest",
                            "com.android.tradefed.testtype.GoogleBenchmarkTest",
                            "com.android.tradefed.testtype.IsolatedHostTest",
                            "com.android.tradefed.testtype.python.PythonBinaryHostTest",
                            "com.android.tradefed.testtype.binary.ExecutableHostTest",
                            "com.android.tradefed.testtype.rust.RustBinaryHostTest",
                            "com.android.tradefed.testtype.rust.RustBinaryTest",
                            "com.android.tradefed.testtype.StubTest",
                            "com.android.tradefed.testtype.ArtRunTest",
                            // Others
                            "com.google.android.deviceconfig.RebootTest"));

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuild = buildInfo;
    }

    /** Get all the configuration copied to the build tests dir and check if they load. */
    @Test
    public void testConfigsLoad() throws Exception {
        List<String> errors = new ArrayList<>();
        Assume.assumeTrue(mBuild instanceof IDeviceBuildInfo);

        IConfigurationFactory configFactory = ConfigurationFactory.getInstance();
        List<String> configs = new ArrayList<>();
        IDeviceBuildInfo deviceBuildInfo = (IDeviceBuildInfo) mBuild;
        File testsDir = deviceBuildInfo.getTestsDir();
        List<File> extraTestCasesDirs = Arrays.asList(testsDir);
        configs.addAll(ConfigurationUtil.getConfigNamesFromDirs(null, extraTestCasesDirs));
        for (String configName : configs) {
            try {
                IConfiguration c =
                        configFactory.createConfigurationFromArgs(new String[] {configName});
                // All configurations in general-tests.zip should be module since they are generated
                // from AndroidTest.xml
                ValidateSuiteConfigHelper.validateConfig(c);

                ensureApkUninstalled(configName, c.getTargetPreparers());
                // Check that all the tests runners are well supported.
                checkRunners(c.getTests(), "general-tests");

                // Add more checks if necessary
            } catch (ConfigurationException e) {
                errors.add(String.format("\t%s: %s", configName, e.getMessage()));
            }
        }

        // If any errors report them in a final exception.
        if (!errors.isEmpty()) {
            throw new ConfigurationException(
                    String.format("Fail configuration check:\n%s", Joiner.on("\n").join(errors)));
        }
    }

    private void ensureApkUninstalled(String config, List<ITargetPreparer> preparers)
            throws Exception {
        for (ITargetPreparer preparer : preparers) {
            if (preparer instanceof TestAppInstallSetup) {
                TestAppInstallSetup installer = (TestAppInstallSetup) preparer;
                if (!installer.isCleanUpEnabled()) {
                    throw new ConfigurationException(
                            String.format("Config: %s should set cleanup-apks=true.", config));
                }
            }
        }
    }

    public static void checkRunners(List<IRemoteTest> tests, String name)
            throws ConfigurationException {
        for (IRemoteTest test : tests) {
            // Check that all the tests runners are well supported.
            if (!SUPPORTED_TEST_RUNNERS.contains(test.getClass().getCanonicalName())) {
                throw new ConfigurationException(
                        String.format(
                                "testtype %s is not officially supported in %s. "
                                        + "The supported ones are: %s",
                                test.getClass().getCanonicalName(), name, SUPPORTED_TEST_RUNNERS));
            }
        }
    }
}
