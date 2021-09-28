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
package com.android.compatibility.common.tradefed.testtype.retry;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.testtype.suite.CompatibilityTestSuite;
import com.android.compatibility.common.tradefed.util.RetryFilterHelper;
import com.android.compatibility.common.tradefed.util.RetryType;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.suite.checker.ISystemStatusChecker;
import com.android.tradefed.suite.checker.ISystemStatusCheckerReceiver;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IShardableTest;
import com.android.tradefed.testtype.suite.BaseTestSuite;
import com.android.tradefed.util.MultiMap;

import com.google.common.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Runner that creates a {@link CompatibilityTestSuite} to re-run some previous results. Only the
 * 'cts' plan is supported. TODO: explore other new way to build the retry (instead of relying on
 * one massive pair of include/exclude filters)
 *
 * @deprecated A new retry has been implemented.
 */
@Deprecated
@OptionClass(alias = "compatibility")
public class RetryFactoryTest
        implements IRemoteTest,
                IDeviceTest,
                IBuildReceiver,
                ISystemStatusCheckerReceiver,
                IInvocationContextReceiver,
                IShardableTest,
                IConfigurationReceiver {

    /**
     * Mirror the {@link CompatibilityTestSuite} options in order to create it.
     */
    public static final String RETRY_OPTION = "retry";
    public static final String RETRY_TYPE_OPTION = "retry-type";

    @Option(name = RETRY_OPTION,
            shortName = 'r',
            description = "retry a previous session's failed and not executed tests.",
            mandatory = true)
    private Integer mRetrySessionId = null;

    @Option(name = CompatibilityTestSuite.SUBPLAN_OPTION,
            description = "the subplan to run",
            importance = Importance.IF_UNSET)
    protected String mSubPlan;

    @Option(name = CompatibilityTestSuite.INCLUDE_FILTER_OPTION,
            description = "the include module filters to apply.",
            importance = Importance.ALWAYS)
    protected Set<String> mIncludeFilters = new HashSet<>();

    @Option(name = CompatibilityTestSuite.EXCLUDE_FILTER_OPTION,
            description = "the exclude module filters to apply.",
            importance = Importance.ALWAYS)
    protected Set<String> mExcludeFilters = new HashSet<>();

    @Option(name = CompatibilityTestSuite.ABI_OPTION,
            shortName = 'a',
            description = "the abi to test.",
            importance = Importance.IF_UNSET)
    protected String mAbiName = null;

    @Option(name = CompatibilityTestSuite.PRIMARY_ABI_RUN,
            description =
            "Whether to run tests with only the device primary abi. "
                  + "This will override the --abi option.")
    protected boolean mPrimaryAbiRun = false;

    @Option(name = CompatibilityTestSuite.MODULE_OPTION,
            shortName = 'm',
            description = "the test module to run.",
            importance = Importance.IF_UNSET)
    protected String mModuleName = null;

    @Option(name = CompatibilityTestSuite.TEST_OPTION,
            shortName = CompatibilityTestSuite.TEST_OPTION_SHORT_NAME,
            description = "the test run.",
            importance = Importance.IF_UNSET)
    protected String mTestName = null;

    @Option(
        name = "module-arg",
        description =
                "the arguments to pass to a module. The expected format is"
                        + "\"<module-name>:<arg-name>:[<arg-key>:=]<arg-value>\"",
        importance = Importance.ALWAYS
    )
    private List<String> mModuleArgs = new ArrayList<>();

    @Option(name = CompatibilityTestSuite.TEST_ARG_OPTION,
            description = "the arguments to pass to a test. The expected format is "
                    + "\"<test-class>:<arg-name>:[<arg-key>:=]<arg-value>\"",
            importance = Importance.ALWAYS)
    private List<String> mTestArgs = new ArrayList<>();

    @Option(name = RETRY_TYPE_OPTION,
            description = "used with " + RETRY_OPTION + ", retry tests"
            + " of a certain status. Possible values include \"failed\" and \"not_executed\".",
            importance = Importance.IF_UNSET)
    protected RetryType mRetryType = null;

    @Option(
        name = BaseTestSuite.CONFIG_PATTERNS_OPTION,
        description =
                "The pattern(s) of the configurations that should be loaded from a directory."
                        + " If none is explicitly specified, .*.xml and .*.config will be used."
                        + " Can be repeated."
    )
    private List<String> mConfigPatterns = new ArrayList<>();

    @Option(
        name = "module-metadata-include-filter",
        description =
                "Include modules for execution based on matching of metadata fields: for any of "
                        + "the specified filter name and value, if a module has a metadata field "
                        + "with the same name and value, it will be included. When both module "
                        + "inclusion and exclusion rules are applied, inclusion rules will be "
                        + "evaluated first. Using this together with test filter inclusion rules "
                        + "may result in no tests to execute if the rules don't overlap."
    )
    private MultiMap<String, String> mModuleMetadataIncludeFilter = new MultiMap<>();

    @Option(
        name = "module-metadata-exclude-filter",
        description =
                "Exclude modules for execution based on matching of metadata fields: for any of "
                        + "the specified filter name and value, if a module has a metadata field "
                        + "with the same name and value, it will be excluded. When both module "
                        + "inclusion and exclusion rules are applied, inclusion rules will be "
                        + "evaluated first."
    )
    private MultiMap<String, String> mModuleMetadataExcludeFilter = new MultiMap<>();

    private List<ISystemStatusChecker> mStatusCheckers;
    private IBuildInfo mBuildInfo;
    private ITestDevice mDevice;
    private IInvocationContext mContext;
    private IConfiguration mMainConfiguration;

    @Override
    public void setSystemStatusChecker(List<ISystemStatusChecker> systemCheckers) {
        mStatusCheckers = systemCheckers;
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildInfo = buildInfo;
    }

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    @Override
    public void setInvocationContext(IInvocationContext invocationContext) {
        mContext = invocationContext;
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mMainConfiguration = configuration;
    }

    /** Build a CompatibilityTest with appropriate filters to run only the tests of interests. */
    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        CompatibilityTestSuite test = loadSuite();
        // run the retry run.
        test.run(testInfo, listener);
    }

    @Override
    public Collection<IRemoteTest> split(int shardCountHint) {
        try {
            CompatibilityTestSuite test = loadSuite();
            return test.split(shardCountHint);
        } catch (DeviceNotAvailableException e) {
            CLog.e("Failed to shard the retry run.");
            CLog.e(e);
        }
        return null;
    }

    /**
     * Helper to create a {@link CompatibilityTestSuite} from previous results.
     */
    private CompatibilityTestSuite loadSuite() throws DeviceNotAvailableException {
        // Create a compatibility test and set it to run only what we want.
        CompatibilityTestSuite test = createTest();

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mBuildInfo);
        // Create the helper with all the options needed.
        RetryFilterHelper helper = createFilterHelper(buildHelper);
        // TODO: we have access to the original command line, we should accommodate more re-run
        // scenario like when the original cts.xml config was not used.
        helper.validateBuildFingerprint(mDevice);
        // ResultReporter when creating the xml will use the retry command line
        helper.setBuildInfoRetryCommand(mBuildInfo);
        helper.setCommandLineOptionsFor(test);
        helper.setCommandLineOptionsFor(this);
        helper.populateRetryFilters();

        try {
            OptionSetter setter = new OptionSetter(test);
            for (String moduleArg : mModuleArgs) {
                setter.setOptionValue("compatibility:module-arg", moduleArg);
            }
            for (String testArg : mTestArgs) {
                setter.setOptionValue("compatibility:test-arg", testArg);
            }
        } catch (ConfigurationException e) {
            throw new RuntimeException(e);
        }

        test.setIncludeFilter(helper.getIncludeFilters());
        test.setExcludeFilter(helper.getExcludeFilters());
        test.setDevice(mDevice);
        test.setBuild(mBuildInfo);
        test.setAbiName(mAbiName);
        test.setPrimaryAbiRun(mPrimaryAbiRun);
        test.setSystemStatusChecker(mStatusCheckers);
        test.setInvocationContext(mContext);
        test.setConfiguration(mMainConfiguration);
        test.addConfigPatterns(mConfigPatterns);
        test.addModuleMetadataIncludeFilters(mModuleMetadataIncludeFilter);
        test.addModuleMetadataExcludeFilters(mModuleMetadataExcludeFilter);
        // reset the retry id - Ensure that retry of retry does not throw
        test.resetRetryId();
        // clean the helper
        helper.tearDown();
        return test;
    }

    /**
     * @return a {@link RetryFilterHelper} created from the attributes of this object.
     */
    protected RetryFilterHelper createFilterHelper(CompatibilityBuildHelper buildHelper) {
        return new RetryFilterHelper(buildHelper, mRetrySessionId, mSubPlan, mIncludeFilters,
                mExcludeFilters, mAbiName, mModuleName, mTestName, mRetryType);
    }

    @VisibleForTesting
    CompatibilityTestSuite createTest() {
        return new CompatibilityTestSuite();
    }

    /**
     * @return the ID of the session to be retried.
     */
    protected Integer getRetrySessionId() {
        return mRetrySessionId;
    }
}
