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

package com.android.tradefed.testtype;

import com.android.ddmlib.IShellOutputReceiver;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.coverage.CoverageOptions;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.FileUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** The base class of gTest */
public abstract class GTestBase
        implements IRemoteTest,
                IConfigurationReceiver,
                ITestFilterReceiver,
                IRuntimeHintProvider,
                ITestCollector,
                IShardableTest,
                IAbiReceiver {

    private static final List<String> DEFAULT_FILE_EXCLUDE_FILTERS = new ArrayList<>();

    static {
        // Exclude .so by default as they are not runnable.
        DEFAULT_FILE_EXCLUDE_FILTERS.add(".*\\.so");
    }

    @Option(name = "run-disable-tests", description = "Determine to run disable tests or not.")
    private boolean mRunDisabledTests = false;

    @Option(name = "module-name", description = "The name of the native test module to run.")
    private String mTestModule = null;

    @Option(
            name = "file-exclusion-filter-regex",
            description = "Regex to exclude certain files from executing. Can be repeated")
    private List<String> mFileExclusionFilterRegex = new ArrayList<>(DEFAULT_FILE_EXCLUDE_FILTERS);

    @Option(
            name = "positive-testname-filter",
            description = "The GTest-based positive filter of the test name to run.")
    private String mTestNamePositiveFilter = null;

    @Option(
            name = "negative-testname-filter",
            description = "The GTest-based negative filter of the test name to run.")
    private String mTestNameNegativeFilter = null;

    @Option(
        name = "include-filter",
        description = "The GTest-based positive filter of the test names to run."
    )
    private Set<String> mIncludeFilters = new LinkedHashSet<>();

    @Option(
        name = "exclude-filter",
        description = "The GTest-based negative filter of the test names to run."
    )
    private Set<String> mExcludeFilters = new LinkedHashSet<>();

    @Option(
            name = "native-test-timeout",
            description =
                    "The max time for a gtest to run. Test run will be aborted if any test "
                            + "takes longer.",
            isTimeVal = true)
    private long mMaxTestTimeMs = 1 * 60 * 1000L;

    /** @deprecated use --coverage in CoverageOptions instead. */
    @Deprecated
    @Option(
        name = "coverage",
        description =
                "Collect code coverage for this test run. Note that the build under test must be a "
                        + "coverage build or else this will fail."
    )
    private boolean mCoverage = false;

    @Option(
            name = "prepend-filename",
            description = "Prepend filename as part of the classname for the tests.")
    private boolean mPrependFileName = false;

    @Option(name = "before-test-cmd", description = "adb shell command(s) to run before GTest.")
    private List<String> mBeforeTestCmd = new ArrayList<>();

    @Option(name = "after-test-cmd", description = "adb shell command(s) to run after GTest.")
    private List<String> mAfterTestCmd = new ArrayList<>();

    @Option(name = "run-test-as", description = "User to execute test binary as.")
    private String mRunTestAs = null;

    @Option(
            name = "ld-library-path",
            description = "LD_LIBRARY_PATH value to include in the GTest execution command.")
    private String mLdLibraryPath = null;

    @Option(
            name = "native-test-flag",
            description =
                    "Additional flag values to pass to the native test's shell command. "
                            + "Flags should be complete, including any necessary dashes: \"--flag=value\"")
    private List<String> mGTestFlags = new ArrayList<>();

    @Option(
            name = "runtime-hint",
            description = "The hint about the test's runtime.",
            isTimeVal = true)
    private long mRuntimeHint = 60000; // 1 minute

    @Option(
            name = "xml-output",
            description =
                    "Use gtest xml output for test results, "
                            + "if test binaries crash, no output will be available.")
    private boolean mEnableXmlOutput = false;

    @Option(
            name = "collect-tests-only",
            description =
                    "Only invoke the test binary to collect list of applicable test cases. "
                            + "All test run callbacks will be triggered, but test execution will "
                            + "not be actually carried out. This option ignores sharding parameters, so "
                            + "each shard will end up collecting all tests.")
    private boolean mCollectTestsOnly = false;

    @Option(
            name = "test-filter-key",
            description =
                    "run the gtest with the --gtest_filter populated with the filter from "
                            + "the json filter file associated with the binary, the filter file will have "
                            + "the same name as the binary with the .json extension.")
    private String mTestFilterKey = null;

    @Option(
            name = "disable-duplicate-test-check",
            description = "If set to true, it will not check that a method is only run once.")
    private boolean mDisableDuplicateCheck = false;

    // GTest flags...
    protected static final String GTEST_FLAG_PRINT_TIME = "--gtest_print_time";
    protected static final String GTEST_FLAG_FILTER = "--gtest_filter";
    protected static final String GTEST_FLAG_RUN_DISABLED_TESTS = "--gtest_also_run_disabled_tests";
    protected static final String GTEST_FLAG_LIST_TESTS = "--gtest_list_tests";
    protected static final String GTEST_FLAG_FILE = "--gtest_flagfile";
    protected static final String GTEST_XML_OUTPUT = "--gtest_output=xml:%s";
    // Expected extension for the filter file associated with the binary (json formatted file)
    @VisibleForTesting protected static final String FILTER_EXTENSION = ".filter";

    private int mShardCount = 0;
    private int mShardIndex = 0;
    private boolean mIsSharded = false;

    private IConfiguration mConfiguration = null;
    private IAbi mAbi;

    @Override
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    @Override
    public IAbi getAbi() {
        return mAbi;
    }

    /** {@inheritDoc} */
    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    /**
     * Set the Android native test module to run.
     *
     * @param moduleName The name of the native test module to run
     */
    public void setModuleName(String moduleName) {
        mTestModule = moduleName;
    }

    /**
     * Get the Android native test module to run.
     *
     * @return the name of the native test module to run, or null if not set
     */
    public String getModuleName() {
        return mTestModule;
    }

    /** Set whether GTest should run disabled tests. */
    protected void setRunDisabled(boolean runDisabled) {
        mRunDisabledTests = runDisabled;
    }

    /**
     * Get whether GTest should run disabled tests.
     *
     * @return True if disabled tests should be run, false otherwise
     */
    public boolean getRunDisabledTests() {
        return mRunDisabledTests;
    }

    /** Set the max time in ms for a gtest to run. */
    @VisibleForTesting
    void setMaxTestTimeMs(int timeout) {
        mMaxTestTimeMs = timeout;
    }

    /**
     * Adds an exclusion file filter regex.
     *
     * @param regex to exclude file.
     */
    @VisibleForTesting
    void addFileExclusionFilterRegex(String regex) {
        mFileExclusionFilterRegex.add(regex);
    }

    /** Sets the shard index of this test. */
    public void setShardIndex(int shardIndex) {
        mShardIndex = shardIndex;
    }

    /** Gets the shard index of this test. */
    public int getShardIndex() {
        return mShardIndex;
    }

    /** Sets the shard count of this test. */
    public void setShardCount(int shardCount) {
        mShardCount = shardCount;
    }

    /** Returns the current shard-count. */
    public int getShardCount() {
        return mShardCount;
    }

    /** {@inheritDoc} */
    @Override
    public long getRuntimeHint() {
        return mRuntimeHint;
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        if (mShardCount > 0) {
            // If we explicitly start giving filters to GTest, reset the shard-count. GTest first
            // applies filters then GTEST_TOTAL_SHARDS so it will probably end up not running
            // anything
            mShardCount = 0;
        }
        mIncludeFilters.add(cleanFilter(filter));
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        for (String filter : filters) {
            mIncludeFilters.add(cleanFilter(filter));
        }
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(cleanFilter(filter));
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        for (String filter : filters) {
            mExcludeFilters.add(cleanFilter(filter));
        }
    }

    /** {@inheritDoc} */
    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
        // Clear the filter file key, to not impact the base filters.
        mTestFilterKey = null;
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getIncludeFilters() {
        return mIncludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getExcludeFilters() {
        return mExcludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    /** Gets module name. */
    public String getTestModule() {
        return mTestModule;
    }

    /** Gets regex to exclude certain files from executing. */
    public List<String> getFileExclusionFilterRegex() {
        return mFileExclusionFilterRegex;
    }

    /** Gets the max time for a gtest to run. */
    public long getMaxTestTimeMs() {
        return mMaxTestTimeMs;
    }

    /** Gets shell command(s) to run before GTest. */
    public List<String> getBeforeTestCmd() {
        return mBeforeTestCmd;
    }

    /** Gets shell command(s) to run after GTest. */
    public List<String> getAfterTestCmd() {
        return mAfterTestCmd;
    }

    /** Gets Additional flag values to pass to the native test's shell command. */
    public List<String> getGTestFlags() {
        return mGTestFlags;
    }

    /** Gets test filter key. */
    public String getTestFilterKey() {
        return mTestFilterKey;
    }

    /** Gets use gtest xml output for test results or not. */
    public boolean isEnableXmlOutput() {
        return mEnableXmlOutput;
    }

    /** Gets only invoke the test binary to collect list of applicable test cases or not. */
    public boolean isCollectTestsOnly() {
        return mCollectTestsOnly;
    }

    /** Gets isSharded flag. */
    public boolean isSharded() {
        return mIsSharded;
    }

    /**
     * Define get filter method.
     *
     * <p>Sub class must implement how to get it's own filter.
     *
     * @param path the full path of the filter file.
     * @return filter string.
     */
    protected abstract String loadFilter(String path) throws DeviceNotAvailableException;

    /**
     * Helper to get the g-test filter of test to run.
     *
     * <p>Note that filters filter on the function name only (eg: Google Test "Test"); all Google
     * Test "Test Cases" will be considered.
     *
     * @param path the full path of the binary on the device.
     * @return the full filter flag to pass to the g-test, or an empty string if none have been
     *     specified
     */
    protected String getGTestFilters(String path) throws DeviceNotAvailableException {
        StringBuilder filter = new StringBuilder();
        if (mTestNamePositiveFilter != null) {
            mIncludeFilters.add(mTestNamePositiveFilter);
        }
        if (mTestNameNegativeFilter != null) {
            mExcludeFilters.add(mTestNameNegativeFilter);
        }
        if (mTestFilterKey != null) {
            String fileFilters = loadFilter(path);
            if (fileFilters != null && !fileFilters.isEmpty()) {
                if (fileFilters.startsWith("-")) {
                    for (String filterString : fileFilters.substring(1).split(":")) {
                        mExcludeFilters.add(filterString);
                    }
                } else {
                    String[] filterStrings = fileFilters.split("-");
                    for (String filterString : filterStrings[0].split(":")) {
                        mIncludeFilters.add(filterString);
                    }
                    if (filterStrings.length == 2) {
                        for (String filterString : filterStrings[1].split(":")) {
                            mExcludeFilters.add(filterString);
                        }
                    }
                }
            }
        }
        if (!mIncludeFilters.isEmpty() || !mExcludeFilters.isEmpty()) {
            filter.append(GTEST_FLAG_FILTER);
            filter.append("=");
            if (!mIncludeFilters.isEmpty()) {
                filter.append(ArrayUtil.join(":", mIncludeFilters));
            }
            if (!mExcludeFilters.isEmpty()) {
                filter.append("-");
                filter.append(ArrayUtil.join(":", mExcludeFilters));
            }
        }
        String filterFlag = filter.toString();
        // Handle long args
        if (filterFlag.length() > 500) {
            String tmpFlag = createFlagFile(filterFlag);
            if (tmpFlag != null) {
                return String.format("%s=%s", GTEST_FLAG_FILE, tmpFlag);
            }
        }

        return filterFlag;
    }

    /**
     * Create a file containing the filters that will be used via --gtest_flagfile to avoid any OS
     * limitation in args size.
     *
     * @param filter The filter string
     * @return The path to the file containing the filter.
     * @throws DeviceNotAvailableException
     */
    protected String createFlagFile(String filter) throws DeviceNotAvailableException {
        File tmpFlagFile = null;
        try {
            tmpFlagFile = FileUtil.createTempFile("flagfile", ".txt");
            FileUtil.writeToFile(filter, tmpFlagFile);
        } catch (IOException e) {
            FileUtil.deleteFile(tmpFlagFile);
            CLog.e(e);
            return null;
        }
        return tmpFlagFile.getAbsolutePath();
    }

    /**
     * Helper to get all the GTest flags to pass into the adb shell command.
     *
     * @param path the full path of the binary on the device.
     * @return the {@link String} of all the GTest flags that should be passed to the GTest
     */
    protected String getAllGTestFlags(String path) throws DeviceNotAvailableException {
        String flags = String.format("%s %s", GTEST_FLAG_PRINT_TIME, getGTestFilters(path));

        if (getRunDisabledTests()) {
            flags = String.format("%s %s", flags, GTEST_FLAG_RUN_DISABLED_TESTS);
        }

        if (isCollectTestsOnly()) {
            flags = String.format("%s %s", flags, GTEST_FLAG_LIST_TESTS);
        }

        for (String gTestFlag : getGTestFlags()) {
            flags = String.format("%s %s", flags, gTestFlag);
        }
        return flags;
    }

    /*
     * Conforms filters using a {@link TestDescription} format to be recognized by the GTest
     * executable.
     */
    public String cleanFilter(String filter) {
        return filter.replace('#', '.');
    }

    /**
     * Exposed for testing
     *
     * @param testRunName
     * @param listener
     * @return a {@link GTestXmlResultParser}
     */
    @VisibleForTesting
    GTestXmlResultParser createXmlParser(String testRunName, ITestInvocationListener listener) {
        return new GTestXmlResultParser(testRunName, listener);
    }

    /**
     * Factory method for creating a {@link IShellOutputReceiver} that parses test output and
     * forwards results to the result listener.
     *
     * @param listener
     * @param runName
     * @return a {@link IShellOutputReceiver}
     */
    @VisibleForTesting
    IShellOutputReceiver createResultParser(String runName, ITestInvocationListener listener) {
        IShellOutputReceiver receiver = null;
        if (mCollectTestsOnly) {
            GTestListTestParser resultParser = new GTestListTestParser(runName, listener);
            resultParser.setPrependFileName(mPrependFileName);
            receiver = resultParser;
        } else {
            GTestResultParser resultParser = new GTestResultParser(runName, listener);
            resultParser.setPrependFileName(mPrependFileName);
            receiver = resultParser;
        }
        // Erase the prepended binary name if needed
        erasePrependedFileName(mExcludeFilters, runName);
        erasePrependedFileName(mIncludeFilters, runName);
        return receiver;
    }

    /**
     * Helper method to build the gtest command to run.
     *
     * @param fullPath absolute file system path to gtest binary on device
     * @param flags gtest execution flags
     * @return the shell command line to run for the gtest
     */
    protected String getGTestCmdLine(String fullPath, String flags) {
        StringBuilder gTestCmdLine = new StringBuilder();
        if (mLdLibraryPath != null) {
            gTestCmdLine.append(String.format("LD_LIBRARY_PATH=%s ", mLdLibraryPath));
        }

        if (getCoverageOptions().isCoverageEnabled()) {
            gTestCmdLine.append("GCOV_PREFIX=/data/misc/trace/testcoverage ");
        }

        // su to requested user
        if (mRunTestAs != null) {
            gTestCmdLine.append(String.format("su %s ", mRunTestAs));
        }

        gTestCmdLine.append(String.format("%s %s", fullPath, flags));
        return gTestCmdLine.toString();
    }

    /** {@inheritDoc} */
    @Override
    public void setCollectTestsOnly(boolean shouldCollectTest) {
        mCollectTestsOnly = shouldCollectTest;
    }

    /** {@inheritDoc} */
    @Override
    public Collection<IRemoteTest> split(int shardCountHint) {
        if (shardCountHint <= 1 || mIsSharded) {
            return null;
        }
        if (mCollectTestsOnly) {
            // GTest cannot shard and use collect tests only, so prevent sharding in this case.
            return null;
        }
        Collection<IRemoteTest> tests = new ArrayList<>();
        for (int i = 0; i < shardCountHint; i++) {
            tests.add(getTestShard(shardCountHint, i));
        }
        return tests;
    }

    /**
     * Make a best effort attempt to retrieve a meaningful short descriptive message for given
     * {@link Exception}
     *
     * @param e the {@link Exception}
     * @return a short message
     */
    protected String getExceptionMessage(Exception e) {
        StringBuilder msgBuilder = new StringBuilder();
        if (e.getMessage() != null) {
            msgBuilder.append(e.getMessage());
        }
        if (e.getCause() != null) {
            msgBuilder.append(" cause:");
            msgBuilder.append(e.getCause().getClass().getSimpleName());
            if (e.getCause().getMessage() != null) {
                msgBuilder.append(" (");
                msgBuilder.append(e.getCause().getMessage());
                msgBuilder.append(")");
            }
        }
        return msgBuilder.toString();
    }

    protected void erasePrependedFileName(Set<String> filters, String filename) {
        if (!mPrependFileName) {
            return;
        }
        Set<String> copy = new LinkedHashSet<>();
        for (String filter : filters) {
            if (filter.startsWith(filename + ".")) {
                copy.add(filter.substring(filename.length() + 1));
            } else {
                copy.add(filter);
            }
        }
        filters.clear();
        filters.addAll(copy);
    }

    private IRemoteTest getTestShard(int shardCount, int shardIndex) {
        GTestBase shard = null;
        try {
            shard = this.getClass().getDeclaredConstructor().newInstance();
            OptionCopier.copyOptionsNoThrow(this, shard);
            shard.mShardIndex = shardIndex;
            shard.mShardCount = shardCount;
            shard.mIsSharded = true;
            // We approximate the runtime of each shard to be equal since we can't know.
            shard.mRuntimeHint = mRuntimeHint / shardCount;
            shard.mAbi = mAbi;
        } catch (InstantiationException
                | IllegalAccessException
                | InvocationTargetException
                | NoSuchMethodException e) {
            // This cannot happen because the class was already created once at that point.
            throw new RuntimeException(
                    String.format(
                            "%s (%s) when attempting to create shard object",
                            e.getClass().getSimpleName(), getExceptionMessage(e)));
        }
        return shard;
    }

    /**
     * Returns the test configuration.
     *
     * @return an IConfiguration
     */
    protected IConfiguration getConfiguration() {
        return mConfiguration;
    }

    /**
     * Returns the {@link CoverageOptions} for this test, if it exists. Otherwise returns a default
     * {@link CoverageOptions} object with all coverage disabled.
     */
    protected CoverageOptions getCoverageOptions() {
        if (mConfiguration != null) {
            return mConfiguration.getCoverageOptions();
        }
        return new CoverageOptions();
    }

    /**
     * Returns the {@link GTestListener} that provides extra debugging info, like detects and
     * reports duplicate tests if mDisabledDuplicateCheck is false. Otherwise, returns the passed-in
     * listener.
     */
    protected ITestInvocationListener getGTestListener(ITestInvocationListener listener) {
        if (mDisableDuplicateCheck) {
            return listener;
        }

        return new GTestListener(listener);
    }
}
