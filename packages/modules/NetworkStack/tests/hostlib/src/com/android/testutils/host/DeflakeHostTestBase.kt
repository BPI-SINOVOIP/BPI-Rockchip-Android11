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
 * limitations under the License
 */

package com.android.testutils.host

import com.android.tests.util.ModuleTestUtils
import com.android.tradefed.config.Option
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions
import com.android.tradefed.util.AaptParser
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.test.fail

private data class TestFailure(val description: String, val stacktrace: String)

/**
 * Base class for host-driven tests to deflake a test package.
 *
 * <p>Classes implementing this base class must define a test APK to be run, and default run
 * count, timeout and test classes. In manual runs, the run count, timeout and test classes can be
 * overridden via command-line parameters, such as:
 *
 * <pre>
 * atest TestName -- \
 *   --test-arg com.android.tradefed.testtype.HostTest:set-option:deflake_run_count:10 \
 *   --test-arg com.android.tradefed.testtype.HostTest:set-option:deflake_single_run_timeout:10s \
 *   --test-arg \
 *      com.android.tradefed.testtype.HostTest:set-option:deflake_test:one.test.Class \
 *   --test-arg \
 *      com.android.tradefed.testtype.HostTest:set-option:deflake_test:another.test.Class
 * </pre>
 */
@RunWith(DeviceJUnit4ClassRunner::class)
abstract class DeflakeHostTestBase : BaseHostJUnit4Test() {

    /**
     * Number of times the device test will be run.
     */
    protected abstract val runCount: Int

    @Option(name = "deflake_run_count",
            description = "How many times to run each test case.",
            importance = Option.Importance.ALWAYS)
    private var mRunCountOption: Int? = null

    /**
     * Filename of the APK to run as part of the test.
     *
     * <p>Typically the java_test_host build rule will have a 'data: [":DeviceTest"]' dependency
     * on the build rule for the device tests. In that case the filename will be "DeviceTest.apk".
     */
    protected abstract val testApkFilename: String

    /**
     * Timeout for each run of the test, in milliseconds. The host-driven test will fail if any run
     * takes more than the specified timeout.
     */
    protected open val singleRunTimeoutMs = 5 * 60_000L

    @Option(name = "deflake_single_run_timeout",
            description = "Timeout for each single run.",
            importance = Option.Importance.ALWAYS,
            isTimeVal = true)
    private var mSingleRunTimeoutMsOption: Long? = null

    /**
     * List of classes to run in the test package. If empty, all classes in the package will be run.
     */
    protected open val testClasses: List<String> = emptyList()

    // TODO: also support single methods, not just whole classes
    @Option(name = "deflake_test",
            description = "Test class to deflake. Can be repeated. " +
                    "Default classes configured for the test are run if omitted.",
            importance = Option.Importance.ALWAYS)
    private var mTestClassesOption: ArrayList<String?> = ArrayList()

    @Before
    fun setUp() {
        // APK will be auto-cleaned
        installPackage(testApkFilename)
    }

    @Test
    fun testDeflake() {
        val apkFile = ModuleTestUtils(this).getTestFile(testApkFilename)
        val pkgName = AaptParser.parse(apkFile)?.packageName
                ?: fail("Could not parse test package name")

        val classes = mTestClassesOption.filterNotNull().ifEmpty { testClasses }
                .ifEmpty { listOf(null) } // null class name runs all classes in the package
        val runOptions = DeviceTestRunOptions(pkgName)
                .setDevice(device)
                .setTestTimeoutMs(mSingleRunTimeoutMsOption ?: singleRunTimeoutMs)
                .setCheckResults(false)
        // Pair is (test identifier, last stacktrace)
        val failures = ArrayList<TestFailure>()
        val count = mRunCountOption ?: runCount
        repeat(count) {
            classes.forEach { testClass ->
                runDeviceTests(runOptions.setTestClassName(testClass))
                failures += getLastRunFailures()
            }
        }
        if (failures.isEmpty()) return
        val failuresByTest = failures.groupBy(TestFailure::description)
        val failMessage = failuresByTest.toList().fold("") { msg, (testDescription, failures) ->
            val stacktraces = formatStacktraces(failures)
            msg + "\n$testDescription: ${failures.count()}/$count failures. " +
                    "Stacktraces:\n$stacktraces"
        }
        fail("Some tests failed:$failMessage")
    }

    private fun getLastRunFailures(): List<TestFailure> {
        with(lastDeviceRunResults) {
            if (isRunFailure) {
                return listOf(TestFailure("All tests in run", runFailureMessage))
            }

            return failedTests.map {
                val stackTrace = testResults[it]?.stackTrace
                        ?: fail("Missing stacktrace for failed test $it")
                TestFailure(it.toString(), stackTrace)
            }
        }
    }

    private fun formatStacktraces(failures: List<TestFailure>): String {
        // Calculate list of (stacktrace, frequency) pairs ordered from most to least frequent
        val frequencies = failures.groupingBy(TestFailure::stacktrace).eachCount().toList()
                .sortedByDescending { it.second }
        // Print each stacktrace with its frequency
        return frequencies.fold("") { msg, (stacktrace, numFailures) ->
            "$msg\n$numFailures failures:\n$stacktrace"
        }
    }
}
