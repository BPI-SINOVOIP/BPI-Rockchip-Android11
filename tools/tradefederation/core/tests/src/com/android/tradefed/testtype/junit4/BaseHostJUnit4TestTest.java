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
package com.android.tradefed.testtype.junit4;

import static org.easymock.EasyMock.getCurrentArguments;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.testrunner.IRemoteAndroidTestRunner;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.ITestLifeCycleReceiver;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.suite.SuiteApkInstaller;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.HostTest;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ListInstrumentationParser;

import org.easymock.EasyMock;
import org.easymock.IAnswer;
import org.junit.Assert;
import org.junit.AssumptionViolatedException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.util.Collection;
import java.util.HashMap;

/** Unit tests for {@link BaseHostJUnit4Test}. */
@RunWith(JUnit4.class)
public class BaseHostJUnit4TestTest {

    /** An implementation of the base class for testing purpose. */
    @RunWith(DeviceJUnit4ClassRunner.class)
    public static class TestableHostJUnit4Test extends BaseHostJUnit4Test {
        @Test
        public void testPass() {
            Assert.assertNotNull(getDevice());
            Assert.assertNotNull(getBuild());
        }

        @Override
        CollectingTestListener createListener() {
            CollectingTestListener listener = new CollectingTestListener();
            listener.testRunStarted("testRun", 1);
            TestDescription tid = new TestDescription("class", "test1");
            listener.testStarted(tid);
            listener.testEnded(tid, new HashMap<String, Metric>());
            listener.testRunEnded(500l, new HashMap<String, Metric>());
            return listener;
        }

        @Override
        ListInstrumentationParser getListInstrumentationParser() {
            ListInstrumentationParser parser = new ListInstrumentationParser();
            parser.processNewLines(
                    new String[] {
                        "instrumentation:com.package/"
                                + "android.support.test.runner.AndroidJUnitRunner "
                                + "(target=com.example2)"
                    });
            return parser;
        }
    }

    /**
     * An implementation of the base class that simulate a crashed instrumentation from an host test
     * run.
     */
    @RunWith(DeviceJUnit4ClassRunner.class)
    public static class FailureHostJUnit4Test extends TestableHostJUnit4Test {
        @Test
        public void testOne() {
            Assert.assertNotNull(getDevice());
            Assert.assertNotNull(getBuild());
        }

        @Override
        CollectingTestListener createListener() {
            CollectingTestListener listener = new CollectingTestListener();
            listener.testRunStarted("testRun", 1);
            listener.testRunFailed("instrumentation crashed");
            listener.testRunEnded(50L, new HashMap<String, Metric>());
            return listener;
        }
    }

    private static final String CLASSNAME =
            "com.android.tradefed.testtype.junit4.BaseHostJUnit4TestTest$TestableHostJUnit4Test";

    private ITestInvocationListener mMockListener;
    private IBuildInfo mMockBuild;
    private ITestDevice mMockDevice;
    private IInvocationContext mMockContext;
    private TestInformation mTestInfo;
    private HostTest mHostTest;

    @Before
    public void setUp() throws Exception {
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockBuild = EasyMock.createMock(IBuildInfo.class);
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.isAppEnumerationSupported()).andStubReturn(false);
        mMockContext = new InvocationContext();
        mMockContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mMockContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockBuild);

        EasyMock.expect(mMockDevice.checkApiLevelAgainstNextRelease(EasyMock.anyInt()))
                .andStubReturn(false);

        mHostTest = new HostTest();
        mHostTest.setBuild(mMockBuild);
        mHostTest.setDevice(mMockDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(mMockContext).build();
        OptionSetter setter = new OptionSetter(mHostTest);
        // Disable pretty logging for testing
        setter.setOptionValue("enable-pretty-logs", "false");
    }

    /** Test that we are able to run the test as a JUnit4. */
    @Test
    public void testSimpleRun() throws Exception {
        OptionSetter setter = new OptionSetter(mHostTest);
        setter.setOptionValue("class", CLASSNAME);
        mMockListener.testRunStarted(EasyMock.anyObject(), EasyMock.eq(1));
        TestDescription tid = new TestDescription(CLASSNAME, "testPass");
        mMockListener.testStarted(tid);
        mMockListener.testEnded(tid, new HashMap<String, Metric>());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.replay(mMockListener, mMockBuild, mMockDevice);
        mHostTest.run(mTestInfo, mMockListener);
        EasyMock.verify(mMockListener, mMockBuild, mMockDevice);
    }

    /**
     * Test that {@link BaseHostJUnit4Test#runDeviceTests(String, String)} properly trigger an
     * instrumentation run.
     */
    @Test
    public void testRunDeviceTests() throws Exception {
        TestableHostJUnit4Test test = new TestableHostJUnit4Test();
        test.setTestInformation(mTestInfo);
        mMockDevice.executeShellCommand(
                EasyMock.eq("pm list instrumentation"), EasyMock.anyObject());
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(new StubDevice("serial"));
        EasyMock.expect(
                        mMockDevice.runInstrumentationTests(
                                (IRemoteAndroidTestRunner) EasyMock.anyObject(),
                                EasyMock.<Collection<ITestLifeCycleReceiver>>anyObject()))
                .andReturn(true);
        EasyMock.replay(mMockBuild, mMockDevice);
        try {
            test.runDeviceTests("com.package", "testClass");
        } catch (AssumptionViolatedException e) {
            // Ensure that the Assume logic in the test does not make a false pass for the unit test
            throw new RuntimeException("Should not have thrown an Assume exception.", e);
        }
        EasyMock.verify(mMockBuild, mMockDevice);
    }

    /** Test that we carry the assumption failure messages. */
    @Test
    public void testRunDeviceTests_assumptionFailure() throws Exception {
        TestableHostJUnit4Test test = new TestableHostJUnit4Test();
        test.setTestInformation(mTestInfo);
        mMockDevice.executeShellCommand(
                EasyMock.eq("pm list instrumentation"), EasyMock.anyObject());
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(new StubDevice("serial"));
        EasyMock.expect(
                        mMockDevice.runInstrumentationTests(
                                (IRemoteAndroidTestRunner) EasyMock.anyObject(),
                                EasyMock.<Collection<ITestLifeCycleReceiver>>anyObject()))
                .andAnswer(
                        new IAnswer<Boolean>() {
                            @SuppressWarnings("unchecked")
                            @Override
                            public Boolean answer() throws Throwable {
                                Collection<ITestLifeCycleReceiver> receivers =
                                        (Collection<ITestLifeCycleReceiver>)
                                                getCurrentArguments()[1];
                                for (ITestLifeCycleReceiver i : receivers) {
                                    i.testRunStarted("runName", 2);
                                    i.testStarted(new TestDescription("class", "test1"));
                                    i.testAssumptionFailure(
                                            new TestDescription("class", "test1"), "assumpFail");
                                    i.testEnded(
                                            new TestDescription("class", "test1"),
                                            new HashMap<String, Metric>());

                                    i.testStarted(new TestDescription("class", "test2"));
                                    i.testAssumptionFailure(
                                            new TestDescription("class", "test2"), "assumpFail2");
                                    i.testEnded(
                                            new TestDescription("class", "test2"),
                                            new HashMap<String, Metric>());
                                }
                                return true;
                            }
                        });
        EasyMock.replay(mMockBuild, mMockDevice);
        try {
            test.runDeviceTests("com.package", "testClass");
            fail("Should have thrown an Assume exception.");
        } catch (AssumptionViolatedException e) {
            assertEquals("assumpFail\n\nassumpFail2", e.getMessage());
        }
        EasyMock.verify(mMockBuild, mMockDevice);
    }

    /** Test that when running an instrumentation, the abi is properly passed. */
    @Test
    public void testRunDeviceTests_abi() throws Exception {
        RemoteAndroidTestRunner runner = Mockito.mock(RemoteAndroidTestRunner.class);
        TestableHostJUnit4Test test =
                new TestableHostJUnit4Test() {
                    @Override
                    RemoteAndroidTestRunner createTestRunner(
                            String packageName, String runnerName, ITestDevice device) {
                        return runner;
                    }
                };
        test.setTestInformation(mTestInfo);
        test.setAbi(new Abi("arm", "32"));
        EasyMock.expect(
                        mMockDevice.runInstrumentationTests(
                                (IRemoteAndroidTestRunner) EasyMock.anyObject(),
                                EasyMock.<Collection<ITestLifeCycleReceiver>>anyObject()))
                .andReturn(true);
        EasyMock.replay(mMockBuild, mMockDevice);
        try {
            test.runDeviceTests("com.package", "testClass");
        } catch (AssumptionViolatedException e) {
            // Ensure that the Assume logic in the test does not make a false pass for the unit test
            throw new RuntimeException("Should not have thrown an Assume exception.", e);
        }
        EasyMock.verify(mMockBuild, mMockDevice);
        // Verify that the runner options were properly set.
        Mockito.verify(runner).setRunOptions("--abi arm");
    }

    /**
     * Test that {@link BaseHostJUnit4Test#runDeviceTests(String, String)} properly trigger an
     * instrumentation run as a user.
     */
    @Test
    public void testRunDeviceTests_asUser() throws Exception {
        TestableHostJUnit4Test test = new TestableHostJUnit4Test();
        test.setTestInformation(mTestInfo);
        mMockDevice.executeShellCommand(
                EasyMock.eq("pm list instrumentation"), EasyMock.anyObject());
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(new StubDevice("serial"));
        EasyMock.expect(
                        mMockDevice.runInstrumentationTestsAsUser(
                                (IRemoteAndroidTestRunner) EasyMock.anyObject(),
                                EasyMock.eq(0),
                                EasyMock.<Collection<ITestLifeCycleReceiver>>anyObject()))
                .andReturn(true);
        EasyMock.replay(mMockBuild, mMockDevice);
        try {
            test.runDeviceTests("com.package", "class", 0, null);
        } catch (AssumptionViolatedException e) {
            // Ensure that the Assume logic in the test does not make a false pass for the unit test
            throw new RuntimeException("Should not have thrown an Assume exception.", e);
        }
        EasyMock.verify(mMockBuild, mMockDevice);
    }

    /**
     * Test that {@link BaseHostJUnit4Test#runDeviceTests(DeviceTestRunOptions)} properly trigger an
     * instrumentation run.
     */
    @Test
    public void testRunDeviceTestsWithOptions() throws Exception {
        RemoteAndroidTestRunner mockRunner = Mockito.mock(RemoteAndroidTestRunner.class);
        TestableHostJUnit4Test test =
                new TestableHostJUnit4Test() {
                    @Override
                    RemoteAndroidTestRunner createTestRunner(
                            String packageName, String runnerName, ITestDevice device)
                            throws DeviceNotAvailableException {
                        return mockRunner;
                    }
                };
        test.setTestInformation(mTestInfo);
        EasyMock.expect(
                        mMockDevice.runInstrumentationTests(
                                (IRemoteAndroidTestRunner) EasyMock.anyObject(),
                                EasyMock.<Collection<ITestLifeCycleReceiver>>anyObject()))
                .andReturn(true);
        EasyMock.replay(mMockBuild, mMockDevice);
        try {
            test.runDeviceTests(
                    new DeviceTestRunOptions("com.package")
                            .setTestClassName("testClass")
                            .addInstrumentationArg("test", "value")
                            .addInstrumentationArg("test2", "value2"));
        } catch (AssumptionViolatedException e) {
            // Ensure that the Assume logic in the test does not make a false pass for the unit test
            throw new RuntimeException("Should not have thrown an Assume exception.", e);
        }
        // Our args are translated to the runner
        Mockito.verify(mockRunner).addInstrumentationArg("test", "value");
        Mockito.verify(mockRunner).addInstrumentationArg("test2", "value2");
        EasyMock.verify(mMockBuild, mMockDevice);
    }

    /**
     * Test that if the instrumentation crash directly we report it as a failure and not an
     * AssumptionFailure (which would improperly categorize the failure).
     */
    @Test
    public void testRunDeviceTests_crashedInstrumentation() throws Exception {
        FailureHostJUnit4Test test = new FailureHostJUnit4Test();
        test.setTestInformation(mTestInfo);
        mMockDevice.executeShellCommand(
                EasyMock.eq("pm list instrumentation"), EasyMock.anyObject());
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(new StubDevice("serial"));
        EasyMock.expect(
                        mMockDevice.runInstrumentationTests(
                                (IRemoteAndroidTestRunner) EasyMock.anyObject(),
                                EasyMock.<Collection<ITestLifeCycleReceiver>>anyObject()))
                .andReturn(true);
        EasyMock.replay(mMockBuild, mMockDevice);
        try {
            test.runDeviceTests("com.package", "class");
        } catch (AssumptionViolatedException e) {
            // Ensure that the Assume logic in the test does not make a false pass for the unit test
            throw new RuntimeException("Should not have thrown an Assume exception.", e);
        } catch (AssertionError expected) {
            assertTrue(expected.getMessage().contains("instrumentation crashed"));
        }
        EasyMock.verify(mMockBuild, mMockDevice);
    }

    /** An implementation of the base class for testing purpose of installation of apk. */
    @RunWith(DeviceJUnit4ClassRunner.class)
    public static class InstallApkHostJUnit4Test extends BaseHostJUnit4Test {
        @Test
        public void testInstall() throws Exception {
            installPackage("apkFileName");
        }

        @Override
        SuiteApkInstaller createSuiteApkInstaller() {
            return new SuiteApkInstaller() {
                @Override
                protected String parsePackageName(
                        File testAppFile, DeviceDescriptor deviceDescriptor)
                        throws TargetSetupError {
                    return "fakepackage";
                }
            };
        }
    }

    /**
     * Test that when running a test that use the {@link BaseHostJUnit4Test#installPackage(String,
     * String...)} the package is properly auto uninstalled.
     */
    @Test
    public void testInstallUninstall() throws Exception {
        File fakeTestsDir = FileUtil.createTempDir("fake-base-host-dir");
        mTestInfo.executionFiles().put(FilesKey.TESTS_DIRECTORY, fakeTestsDir);
        try {
            File apk = new File(fakeTestsDir, "apkFileName");
            apk.createNewFile();
            HostTest test = new HostTest();
            test.setBuild(mMockBuild);
            test.setDevice(mMockDevice);
            OptionSetter setter = new OptionSetter(test);
            // Disable pretty logging for testing
            setter.setOptionValue("enable-pretty-logs", "false");
            setter.setOptionValue("class", InstallApkHostJUnit4Test.class.getName());
            mMockListener.testRunStarted(InstallApkHostJUnit4Test.class.getName(), 1);
            TestDescription description =
                    new TestDescription(InstallApkHostJUnit4Test.class.getName(), "testInstall");
            mMockListener.testStarted(description);
            EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null).anyTimes();

            EasyMock.expect(mMockDevice.installPackage(apk, true)).andReturn(null);
            // Ensure that the auto-uninstall is triggered
            EasyMock.expect(mMockDevice.uninstallPackage("fakepackage")).andReturn(null);
            mMockListener.testEnded(description, new HashMap<String, Metric>());
            mMockListener.testRunEnded(
                    EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

            EasyMock.replay(mMockBuild, mMockDevice, mMockListener);
            test.run(mTestInfo, mMockListener);
            EasyMock.verify(mMockBuild, mMockDevice, mMockListener);
        } finally {
            FileUtil.recursiveDelete(fakeTestsDir);
        }
    }
}
