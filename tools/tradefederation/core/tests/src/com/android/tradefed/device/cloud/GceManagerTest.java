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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.cloud.GceAvdInfo.GceStatus;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.ArrayUtil;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import com.google.common.net.HostAndPort;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.ProcessBuilder.Redirect;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link GceManager} */
@RunWith(JUnit4.class)
public class GceManagerTest {

    private GceManager mGceManager;
    private DeviceDescriptor mMockDeviceDesc;
    private TestDeviceOptions mOptions;
    private IBuildInfo mMockBuildInfo;
    private IRunUtil mMockRunUtil;
    private File mAvdBinary;

    @Before
    public void setUp() throws Exception {
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mMockDeviceDesc = Mockito.mock(DeviceDescriptor.class);
        mMockBuildInfo = new BuildInfo();
        mOptions = new TestDeviceOptions();
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("wait-gce-teardown", "true");
        mAvdBinary = FileUtil.createTempFile("acloud", ".par");
        mAvdBinary.setExecutable(true);
        mOptions.setAvdDriverBinary(mAvdBinary);
        mOptions.setAvdConfigFile(mAvdBinary);
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
    }

    @After
    public void tearDown() {
        FileUtil.deleteFile(mAvdBinary);
    }

    /** Test {@link GceManager#extractInstanceName(String)} with a typical gce log. */
    @Test
    public void testExtractNameFromLog() {
        String log =
                "2016-09-13 00:05:08,261 |INFO| gcompute_client:283| "
                        + "Image image-gce-x86-phone-userdebug-fastbuild-linux-3266697-1f7cc554 "
                        + "has been created.\n2016-09-13 00:05:08,261 |INFO| gstorage_client:102| "
                        + "Deleting file: bucket: android-artifacts-cache, object: 9a76b1f96c7e4da"
                        + "19b90b0c4e97f9450-avd-system.tar.gz\n2016-09-13 00:05:08,412 |INFO| gst"
                        + "orage_client:107| Deleted file: bucket: android-artifacts-cache, object"
                        + ": 9a76b1f96c7e4da19b90b0c4e97f9450-avd-system.tar.gz\n2016-09-13 00:05:"
                        + "09,331 |INFO| gcompute_client:728| Creating instance: project android-t"
                        + "reehugger, zone us-central1-f, body:{'networkInterfaces': [{'network': "
                        + "u'https://www.googleapis.com/compute/v1/projects/android-treehugger/glo"
                        + "bal/networks/default', 'accessConfigs': [{'type': 'ONE_TO_ONE_NAT', 'na"
                        + "me': 'External NAT'}]}], 'name': u'gce-x86-phone-userdebug-fastbuild-lin"
                        + "ux-3266697-144fcf59', 'serviceAccounts': [{'email': 'default', 'scopes'"
                        + ": ['https://www.googleapis.com/auth/devstorage.read_only', 'https://www"
                        + ".googleapis.com/auth/logging.write']}], 'disks': [{'autoDelete': True, "
                        + "'boot': True, 'mode': 'READ_WRITE', 'initializeParams': {'diskName': 'g"
                        + "ce-x86-phone-userdebug-fastbuild-linux-3266697-144fcf59', 'sourceImage'"
                        + ": u'https://www.googleapis.com/compute/v1/projects/a";
        String result = mGceManager.extractInstanceName(log);
        assertEquals("gce-x86-phone-userdebug-fastbuild-linux-3266697-144fcf59", result);
    }

    /** Test {@link GceManager#extractInstanceName(String)} with a typical gce log. */
    @Test
    public void testExtractNameFromLog_newFormat() {
        String log =
                "2016-09-20 08:11:02,287 |INFO| gcompute_client:728| Creating instance: "
                        + "project android-treehugger, zone us-central1-f, body:{'name': "
                        + "'ins-80bd5bd1-3708674-gce-x86-phone-userdebug-fastbuild3c-linux', "
                        + "'disks': [{'autoDelete': True, 'boot': True, 'mode': 'READ_WRITE', "
                        + "'initializeParams': {'diskName': 'gce-x86-phone-userdebug-fastbuild-"
                        + "linux-3286354-eb1fd2e3', 'sourceImage': u'https://www.googleapis.com"
                        + "compute/v1/projects/android-treehugger/global/images/image-gce-x86-ph"
                        + "one-userdebug-fastbuild-linux-3286354-b6b99338'}, 'type': 'PERSISTENT'"
                        + "}, {'autoDelete': True, 'deviceName': 'gce-x86-phone-userdebug-fastbuil"
                        + "d-linux-3286354-eb1fd2e3-data', 'interface': 'SCSI', 'mode': 'READ_WRI"
                        + "TE', 'type': 'PERSISTENT', 'boot': False, 'source': u'projects/andro"
                        + "id-treehugger/zones/us-c}]}";
        String result = mGceManager.extractInstanceName(log);
        assertEquals("ins-80bd5bd1-3708674-gce-x86-phone-userdebug-fastbuild3c-linux", result);
    }

    /**
     * Test {@link GceManager#extractInstanceName(String)} with a log that does not contains the gce
     * name.
     */
    @Test
    public void testExtractNameFromLog_notfound() {
        String log =
                "2016-09-20 08:11:02,287 |INFO| gcompute_client:728| Creating instance: "
                        + "project android-treehugger, zone us-central1-f, body:{'name': "
                        + "'name-80bd5bd1-3708674-gce-x86-phone-userdebug-fastbuild3c-linux',"
                        + "[{'autoDelete': True, 'boot': True, 'mode': 'READ_WRITE', 'initia "
                        + "{'diskName': 'gce-x86-phone-userdebug-fastbuild-linux-3286354-eb1 "
                        + "'sourceImage': u'https://www.googleapis.com/compute/v1/projects/an"
                        + "treehugger/global/images/image-gce-x86-phone-userdebug-fastbuild-g";
        String result = mGceManager.extractInstanceName(log);
        assertNull(result);
    }

    /** Test {@link GceManager#buildGceCmd(File, IBuildInfo, String)}. */
    @Test
    public void testBuildGceCommand() throws IOException {
        IBuildInfo mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        EasyMock.expect(mMockBuildInfo.getBuildAttributes())
                .andReturn(Collections.<String, String>emptyMap());
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn("FLAVOR");
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn("BRANCH");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn("BUILDID");
        EasyMock.replay(mMockBuildInfo);
        File reportFile = null;
        try {
            reportFile = FileUtil.createTempFile("test-gce-cmd", "report");
            List<String> result = mGceManager.buildGceCmd(reportFile, mMockBuildInfo, null);
            List<String> expected =
                    ArrayUtil.list(
                            mOptions.getAvdDriverBinary().getAbsolutePath(),
                            "create",
                            "--build_target",
                            "FLAVOR",
                            "--branch",
                            "BRANCH",
                            "--build_id",
                            "BUILDID",
                            "--config_file",
                            mGceManager.getAvdConfigFile().getAbsolutePath(),
                            "--report_file",
                            reportFile.getAbsolutePath(),
                            "-v");
            assertEquals(expected, result);
        } finally {
            FileUtil.deleteFile(reportFile);
        }
        EasyMock.verify(mMockBuildInfo);
    }

    /** Test {@link GceManager#buildGceCmd(File, IBuildInfo, String)} with json key file set. */
    @Test
    public void testBuildGceCommand_withServiceAccountJsonKeyFile() throws Exception {
        IBuildInfo mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        EasyMock.expect(mMockBuildInfo.getBuildAttributes())
                .andReturn(Collections.<String, String>emptyMap());
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn("FLAVOR");
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn("BRANCH");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn("BUILDID");
        EasyMock.replay(mMockBuildInfo);
        File reportFile = null;
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("gce-driver-service-account-json-key-path", "/path/to/key.json");
        try {
            reportFile = FileUtil.createTempFile("test-gce-cmd", "report");
            List<String> result = mGceManager.buildGceCmd(reportFile, mMockBuildInfo, null);
            List<String> expected =
                    ArrayUtil.list(
                            mOptions.getAvdDriverBinary().getAbsolutePath(),
                            "create",
                            "--build_target",
                            "FLAVOR",
                            "--branch",
                            "BRANCH",
                            "--build_id",
                            "BUILDID",
                            "--config_file",
                            mGceManager.getAvdConfigFile().getAbsolutePath(),
                            "--service_account_json_private_key_path",
                            "/path/to/key.json",
                            "--report_file",
                            reportFile.getAbsolutePath(),
                            "-v");
            assertEquals(expected, result);
        } finally {
            FileUtil.deleteFile(reportFile);
        }
        EasyMock.verify(mMockBuildInfo);
    }

    /** Test {@link GceManager#buildGceCmd(File, IBuildInfo, String)}. */
    @Test
    public void testBuildGceCommandWithEmulatorBuild() throws Exception {
        IBuildInfo mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        EasyMock.expect(mMockBuildInfo.getBuildAttributes())
                .andReturn(Collections.<String, String>emptyMap());
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn("TARGET");
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn("BRANCH");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn("BUILDID");
        EasyMock.replay(mMockBuildInfo);
        File reportFile = null;

        try {
            OptionSetter setter = new OptionSetter(mOptions);
            setter.setOptionValue("gce-driver-param", "--emulator-build-id");
            setter.setOptionValue("gce-driver-param", "EMULATOR_BUILD_ID");
            mGceManager =
                    new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo) {
                        @Override
                        IRunUtil getRunUtil() {
                            return mMockRunUtil;
                        }
                    };
            reportFile = FileUtil.createTempFile("test-gce-cmd", "report");
            List<String> result = mGceManager.buildGceCmd(reportFile, mMockBuildInfo, null);
            List<String> expected =
                    ArrayUtil.list(
                            mOptions.getAvdDriverBinary().getAbsolutePath(),
                            "create",
                            "--build_target",
                            "TARGET",
                            "--branch",
                            "BRANCH",
                            "--build_id",
                            "BUILDID",
                            "--emulator-build-id",
                            "EMULATOR_BUILD_ID",
                            "--config_file",
                            mGceManager.getAvdConfigFile().getAbsolutePath(),
                            "--report_file",
                            reportFile.getAbsolutePath(),
                            "-v");
            assertEquals(expected, result);
        } finally {
            FileUtil.deleteFile(reportFile);
        }
        EasyMock.verify(mMockBuildInfo);
    }

    /** Test {@link GceManager#buildGceCmd(File, IBuildInfo, String)}. */
    @Test
    public void testBuildGceCommandWithGceDriverParam() throws Exception {
        IBuildInfo mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        EasyMock.expect(mMockBuildInfo.getBuildAttributes())
                .andReturn(Collections.<String, String>emptyMap());
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn("FLAVOR");
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn("BRANCH");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn("BUILDID");
        EasyMock.replay(mMockBuildInfo);
        File reportFile = null;
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("gce-driver-param", "--report-internal-ip");
        setter.setOptionValue("gce-driver-param", "--no-autoconnect");
        try {
            reportFile = FileUtil.createTempFile("test-gce-cmd", "report");
            List<String> result = mGceManager.buildGceCmd(reportFile, mMockBuildInfo, null);
            List<String> expected =
                    ArrayUtil.list(
                            mOptions.getAvdDriverBinary().getAbsolutePath(),
                            "create",
                            "--build_target",
                            "FLAVOR",
                            "--branch",
                            "BRANCH",
                            "--build_id",
                            "BUILDID",
                            "--report-internal-ip",
                            "--no-autoconnect",
                            "--config_file",
                            mGceManager.getAvdConfigFile().getAbsolutePath(),
                            "--report_file",
                            reportFile.getAbsolutePath(),
                            "-v");
            assertEquals(expected, result);
        } finally {
            FileUtil.deleteFile(reportFile);
        }
        EasyMock.verify(mMockBuildInfo);
    }

    /** Ensure exception is thrown after a timeout from the acloud command. */
    @Test
    public void testStartGce_timeout() throws Exception {
        mOptions.getGceDriverParams().add("--boot-timeout");
        mOptions.getGceDriverParams().add("900");
        OptionSetter setter = new OptionSetter(mOptions);
        // Boot-time on Acloud params will be overridden by TF option.
        setter.setOptionValue("allow-gce-boot-timeout-override", "false");
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    protected List<String> buildGceCmd(
                            File reportFile, IBuildInfo b, String ipDevice) {
                        List<String> tmp = new ArrayList<String>();
                        tmp.add("");
                        return tmp;
                    }
                };
        final String expectedException =
                "acloud errors: timeout after 1620000ms, acloud did not return null";
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.TIMED_OUT);
        cmd.setStdout("output err");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.eq(1800000L),
                                EasyMock.anyObject(),
                                EasyMock.eq("--boot-timeout"),
                                EasyMock.eq("1620")))
                .andReturn(cmd);
        EasyMock.replay(mMockRunUtil);
        doReturn(null).when(mMockDeviceDesc).toString();
        try {
            mGceManager.startGce();
            fail("A TargetSetupError should have been thrown");
        } catch (TargetSetupError expected) {
            assertEquals(expectedException, expected.getMessage());
        }
        EasyMock.verify(mMockRunUtil);
    }

    /** Test {@link GceManager#buildGceCmd(File, IBuildInfo, String)}. */
    @Test
    public void testBuildGceCommandWithKernelBuild() throws Exception {
        IBuildInfo mMockBuildInfo = EasyMock.createMock(IBuildInfo.class);
        EasyMock.expect(mMockBuildInfo.getBuildAttributes())
                .andReturn(Collections.<String, String>emptyMap());
        EasyMock.expect(mMockBuildInfo.getBuildFlavor()).andReturn("FLAVOR");
        EasyMock.expect(mMockBuildInfo.getBuildBranch()).andReturn("BRANCH");
        EasyMock.expect(mMockBuildInfo.getBuildId()).andReturn("BUILDID");
        EasyMock.replay(mMockBuildInfo);
        File reportFile = null;
        try {
            OptionSetter setter = new OptionSetter(mOptions);
            setter.setOptionValue("gce-driver-param", "--kernel_build_id");
            setter.setOptionValue("gce-driver-param", "KERNELBUILDID");
            reportFile = FileUtil.createTempFile("test-gce-cmd", "report");
            List<String> result = mGceManager.buildGceCmd(reportFile, mMockBuildInfo, null);
            List<String> expected =
                    ArrayUtil.list(
                            mOptions.getAvdDriverBinary().getAbsolutePath(),
                            "create",
                            "--build_target",
                            "FLAVOR",
                            "--branch",
                            "BRANCH",
                            "--build_id",
                            "BUILDID",
                            "--kernel_build_id",
                            "KERNELBUILDID",
                            "--config_file",
                            mGceManager.getAvdConfigFile().getAbsolutePath(),
                            "--report_file",
                            reportFile.getAbsolutePath(),
                            "-v");
            assertEquals(expected, result);
        } finally {
            FileUtil.deleteFile(reportFile);
        }
        EasyMock.verify(mMockBuildInfo);
    }

    /**
     * Test that a {@link com.android.tradefed.device.cloud.GceAvdInfo} is properly created when the
     * output of acloud and runutil is fine.
     */
    @Test
    public void testStartGce() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    protected List<String> buildGceCmd(
                            File reportFile, IBuildInfo b, String ipDevice) {
                        String valid =
                                " {\n"
                                        + "\"data\": {\n"
                                        + "\"devices\": [\n"
                                        + "{\n"
                                        + "\"ip\": \"104.154.62.236\",\n"
                                        + "\"instance_name\": \"gce-x86-phone-userdebug-22\"\n"
                                        + "}\n"
                                        + "]\n"
                                        + "},\n"
                                        + "\"errors\": [],\n"
                                        + "\"command\": \"create\",\n"
                                        + "\"status\": \"SUCCESS\"\n"
                                        + "}";
                        try {
                            FileUtil.writeToFile(valid, reportFile);
                        } catch (IOException e) {
                            throw new RuntimeException(e);
                        }
                        List<String> tmp = new ArrayList<String>();
                        tmp.add("");
                        return tmp;
                    }
                };
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.SUCCESS);
        cmd.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), (String[]) EasyMock.anyObject()))
                .andReturn(cmd);

        EasyMock.replay(mMockRunUtil);
        GceAvdInfo res = mGceManager.startGce();
        EasyMock.verify(mMockRunUtil);
        assertNotNull(res);
        assertEquals(GceStatus.SUCCESS, res.getStatus());
    }

    /**
     * Test that in case of improper output from acloud we throw an exception since we could not get
     * the valid information we are looking for.
     */
    @Test
    public void testStartGce_failed() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    protected List<String> buildGceCmd(
                            File reportFile, IBuildInfo b, String ipDevice) {
                        // We delete the potential report file to create an issue.
                        FileUtil.deleteFile(reportFile);
                        List<String> tmp = new ArrayList<String>();
                        tmp.add("");
                        return tmp;
                    }
                };
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.FAILED);
        cmd.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), (String[]) EasyMock.anyObject()))
                .andReturn(cmd);
        EasyMock.replay(mMockRunUtil);
        try {
            mGceManager.startGce();
            fail("Should have thrown an exception");
        } catch (TargetSetupError expected) {

        }
        EasyMock.verify(mMockRunUtil);
    }

    /**
     * Test that even in case of BOOT_FAIL if we can get some valid information about the GCE
     * instance, then we still return a GceAvdInfo to describe it.
     */
    @Test
    public void testStartGce_bootFail() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    protected List<String> buildGceCmd(
                            File reportFile, IBuildInfo b, String ipDevice) {
                        String validFail =
                                " {\n"
                                        + "\"data\": {\n"
                                        + "\"devices_failing_boot\": [\n"
                                        + "{\n"
                                        + "\"ip\": \"104.154.62.236\",\n"
                                        + "\"instance_name\": \"ins-x86-phone-userdebug-229\"\n"
                                        + "}\n"
                                        + "]\n"
                                        + "},\n"
                                        + "\"errors\": [\"device did not boot\"],\n"
                                        + "\"command\": \"create\",\n"
                                        + "\"status\": \"BOOT_FAIL\"\n"
                                        + "}";
                        try {
                            FileUtil.writeToFile(validFail, reportFile);
                        } catch (IOException e) {
                            throw new RuntimeException(e);
                        }
                        List<String> tmp = new ArrayList<String>();
                        tmp.add("");
                        return tmp;
                    }
                };
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.FAILED);
        cmd.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), (String[]) EasyMock.anyObject()))
                .andReturn(cmd);
        EasyMock.replay(mMockRunUtil);
        GceAvdInfo res = mGceManager.startGce();
        EasyMock.verify(mMockRunUtil);
        assertNotNull(res);
        assertEquals(GceStatus.BOOT_FAIL, res.getStatus());
    }

    /**
     * Test for {@link GceManager#shutdownGce() }.
     *
     * @throws Exception
     */
    @Test
    public void testShutdownGce() throws Exception {
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo, "instance1", "host1") {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
        mGceManager.startGce();
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.SUCCESS);
        cmd.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq(mOptions.getAvdDriverBinary().getAbsolutePath()),
                                EasyMock.eq("delete"),
                                EasyMock.eq("--instance_names"),
                                EasyMock.eq("instance1"),
                                EasyMock.eq("--config_file"),
                                EasyMock.contains(mGceManager.getAvdConfigFile().getAbsolutePath()),
                                EasyMock.eq("--report_file"),
                                EasyMock.anyObject()))
                .andReturn(cmd);

        EasyMock.replay(mMockRunUtil);
        mGceManager.shutdownGce();
        EasyMock.verify(mMockRunUtil);
        // Attributes are marked when successful
        assertTrue(
                mMockBuildInfo
                        .getBuildAttributes()
                        .containsKey(GceManager.GCE_INSTANCE_CLEANED_KEY));
    }

    @Test
    public void testShutdownGce_noWait() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("wait-gce-teardown", "false");
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo, "instance1", "host1") {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
        mGceManager.startGce();
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.SUCCESS);
        cmd.setStdout("output");
        Capture<List<String>> capture = new Capture<>();
        EasyMock.expect(
                        mMockRunUtil.runCmdInBackground(
                                EasyMock.eq(Redirect.DISCARD),
                                EasyMock.<List<String>>capture(capture)))
                .andReturn(Mockito.mock(Process.class));

        EasyMock.replay(mMockRunUtil);
        mGceManager.shutdownGce();
        EasyMock.verify(mMockRunUtil);

        List<String> args = capture.getValue();
        assertTrue(args.get(5).contains(mAvdBinary.getName()));
    }

    /**
     * Test for {@link GceManager#shutdownGce() }.
     *
     * @throws Exception
     */
    @Test
    public void testShutdownGce_withJsonKeyFile() throws Exception {
        mGceManager =
                new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo, "instance1", "host1") {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("gce-driver-service-account-json-key-path", "/path/to/key.json");
        mGceManager.startGce();
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.SUCCESS);
        cmd.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq(mOptions.getAvdDriverBinary().getAbsolutePath()),
                                EasyMock.eq("delete"),
                                EasyMock.eq("--instance_names"),
                                EasyMock.eq("instance1"),
                                EasyMock.eq("--config_file"),
                                EasyMock.contains(mGceManager.getAvdConfigFile().getAbsolutePath()),
                                EasyMock.eq("--service_account_json_private_key_path"),
                                EasyMock.eq("/path/to/key.json"),
                                EasyMock.eq("--report_file"),
                                EasyMock.anyObject()))
                .andReturn(cmd);

        EasyMock.replay(mMockRunUtil);
        mGceManager.shutdownGce();
        EasyMock.verify(mMockRunUtil);
    }

    /** Test a success case for collecting the bugreport with ssh. */
    @Test
    public void testGetSshBugreport() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        res.setStdout("bugreport success!\nOK:/bugreports/bugreport.zip\n");
        OutputStream stdout = null;
        OutputStream stderr = null;
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq(stdout),
                                EasyMock.eq(stderr),
                                EasyMock.eq("ssh"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1"),
                                EasyMock.eq("bugreportz")))
                .andReturn(res);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1:/bugreports/bugreport.zip"),
                                EasyMock.anyObject()))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);
        File bugreport = null;
        try {
            bugreport = GceManager.getBugreportzWithSsh(fakeInfo, mOptions, mMockRunUtil);
            assertNotNull(bugreport);
        } finally {
            EasyMock.verify(mMockRunUtil);
            FileUtil.deleteFile(bugreport);
        }
    }

    /**
     * Test pulling a bugreport of a remote nested instance. This requires a middle step of dumping
     * and pulling the bugreport to the remote virtual box then scp-ing from it.
     */
    @Test
    public void testGetNestedSshBugreport() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        res.setStdout("bugreport success!\nOK:/bugreports/bugreport.zip\n");
        OutputStream stdout = null;
        OutputStream stderr = null;
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq(stdout),
                                EasyMock.eq(stderr),
                                EasyMock.eq("ssh"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1"),
                                EasyMock.eq("./bin/adb"),
                                EasyMock.eq("wait-for-device"),
                                EasyMock.eq("shell"),
                                EasyMock.eq("bugreportz")))
                .andReturn(res);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq(stdout),
                                EasyMock.eq(stderr),
                                EasyMock.eq("ssh"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1"),
                                EasyMock.eq("./bin/adb"),
                                EasyMock.eq("pull"),
                                EasyMock.eq("/bugreports/bugreport.zip")))
                .andReturn(res);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1:./bugreport.zip"),
                                EasyMock.anyObject()))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);
        File bugreport = null;
        try {
            bugreport = GceManager.getNestedDeviceSshBugreportz(fakeInfo, mOptions, mMockRunUtil);
            assertNotNull(bugreport);
        } finally {
            EasyMock.verify(mMockRunUtil);
            FileUtil.deleteFile(bugreport);
        }
    }

    /**
     * Test a case where bugreportz command timeout or may have failed but we still get an output.
     * In this case we still proceed and try to get the bugreport.
     */
    @Test
    public void testGetSshBugreport_Fail() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        CommandResult res = new CommandResult(CommandStatus.FAILED);
        res.setStdout("bugreport failed!\n");
        OutputStream stdout = null;
        OutputStream stderr = null;
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq(stdout),
                                EasyMock.eq(stderr),
                                EasyMock.eq("ssh"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1"),
                                EasyMock.eq("bugreportz")))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);
        File bugreport = null;
        try {
            bugreport = GceManager.getBugreportzWithSsh(fakeInfo, mOptions, mMockRunUtil);
            assertNull(bugreport);
        } finally {
            FileUtil.deleteFile(bugreport);
            EasyMock.verify(mMockRunUtil);
        }
    }

    /**
     * Test that if the instance bring up reach a timeout but we are able to find a device instance
     * in the logs, we raise it as a failure and attempt to clean up the instance.
     */
    @Test
    public void testStartGce_timeoutAndClean() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        DeviceDescriptor desc = null;
        mGceManager =
                new GceManager(desc, mOptions, mMockBuildInfo) {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    protected List<String> buildGceCmd(
                            File reportFile, IBuildInfo b, String ipDevice) {
                        // We delete the potential report file to create an issue.
                        FileUtil.deleteFile(reportFile);
                        List<String> tmp = new ArrayList<String>();
                        tmp.add("");
                        return tmp;
                    }
                };
        CommandResult cmd = new CommandResult();
        cmd.setStatus(CommandStatus.TIMED_OUT);
        cmd.setStderr(
                "2016-09-20 08:11:02,287 |INFO| gcompute_client:728| Creating instance: "
                        + "project android-treehugger, zone us-central1-f, body:{'name': "
                        + "'ins-fake-instance-linux', "
                        + "'disks': [{'autoDelete': True, 'boot': True, 'mode': 'READ_WRITE', "
                        + "'initializeParams': {'diskName': 'gce-x86-phone-userdebug-fastbuild-"
                        + "linux-3286354-eb1fd2e3', 'sourceImage': u'https://www.googleapis.com"
                        + "compute/v1/projects/android-treehugger/global/images/image-gce-x86-ph"
                        + "one-userdebug-fastbuild-linux-3286354-b6b99338'}, 'type': 'PERSISTENT'"
                        + "}, {'autoDelete': True, 'deviceName': 'gce-x86-phone-userdebug-fastbuil"
                        + "d-linux-3286354-eb1fd2e3-data', 'interface': 'SCSI', 'mode': 'READ_WRI"
                        + "TE', 'type': 'PERSISTENT', 'boot': False, 'source': u'projects/andro"
                        + "id-treehugger/zones/us-c}]}");
        cmd.setStdout("output");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(), (String[]) EasyMock.anyObject()))
                .andReturn(cmd);
        // Ensure that the instance can be shutdown.
        CommandResult shutdownResult = new CommandResult();
        shutdownResult.setStatus(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.anyObject(),
                                EasyMock.eq("delete"),
                                EasyMock.eq("--instance_names"),
                                EasyMock.eq("ins-fake-instance-linux"),
                                EasyMock.eq("--config_file"),
                                EasyMock.anyObject(),
                                EasyMock.eq("--report_file"),
                                EasyMock.anyObject()))
                .andReturn(shutdownResult);

        EasyMock.replay(mMockRunUtil);
        GceAvdInfo gceAvd = mGceManager.startGce();
        assertEquals(
                "acloud errors: timeout after 1800000ms, acloud did not return",
                gceAvd.getErrors());
        // If we attempt to clean up afterward
        mGceManager.shutdownGce();
        EasyMock.verify(mMockRunUtil);
    }

    @Test
    public void testUpdateTimeout() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        mOptions.getGceDriverParams().add("--boot-timeout");
        mOptions.getGceDriverParams().add("900");
        assertEquals(1800000L, mOptions.getGceCmdTimeout());
        mGceManager = new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo);
        assertEquals(1080000L, mOptions.getGceCmdTimeout());
    }

    @Test
    public void testUpdateTimeout_multiBootTimeout() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        mOptions.getGceDriverParams().add("--boot-timeout");
        mOptions.getGceDriverParams().add("900");
        mOptions.getGceDriverParams().add("--boot-timeout");
        mOptions.getGceDriverParams().add("450");
        assertEquals(1800000L, mOptions.getGceCmdTimeout());
        mGceManager = new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo);
        // The last specified boot-timeout is used.
        assertEquals(630000L, mOptions.getGceCmdTimeout());
    }

    @Test
    public void testUpdateTimeout_noBootTimeout() throws Exception {
        OptionSetter setter = new OptionSetter(mOptions);
        setter.setOptionValue("allow-gce-boot-timeout-override", "true");
        mOptions.getGceDriverParams().add("--someargs");
        mOptions.getGceDriverParams().add("900");
        assertEquals(1800000L, mOptions.getGceCmdTimeout());
        mGceManager = new GceManager(mMockDeviceDesc, mOptions, mMockBuildInfo);
        assertEquals(1800000L, mOptions.getGceCmdTimeout());
    }
}
