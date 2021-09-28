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
package com.android.tradefed.util.statsd;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.ArgumentMatchers.startsWith;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.os.AtomsProto.Atom;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.util.CommandResult;
import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;

/** Unit tests for {@link ConfigUtil} */
@RunWith(JUnit4.class)
public class ConfigUtilTest {
    private static final String STATS_CONFIG_UPDATE_MATCHER =
            "cat \\/data\\/local\\/tmp\\/statsdconfig.*config \\| cmd stats config update .*";
    private static final String STATS_CONFIG_REMOVE_MATCHER = "cmd stats config remove";
    private static final String REMOTE_PATH_MATCHER = "/data/local/tmp/.*";
    private static final String PARSING_ERROR_MSG = "Error parsing proto stream for StatsConfig.";

    private ITestDevice mTestDevice;
    private File mConfigFile;

    @Before
    public void setUpMocks() {
        mTestDevice = Mockito.mock(ITestDevice.class);
        mConfigFile = Mockito.mock(File.class);
        when(mConfigFile.getName()).thenReturn("statsdconfig.config");
    }

    /** Tests that a configuration is pushed to the device by making the expected calls. */
    @Test
    public void testPushDeviceConfig() throws DeviceNotAvailableException, IOException {
        // Mock the push file, update config, and rm commands.
        when(mTestDevice.pushFile(any(File.class), anyString())).thenReturn(true);
        CommandResult mockResult = new CommandResult();
        mockResult.setStderr("");
        when(mTestDevice.executeShellV2Command(matches(STATS_CONFIG_UPDATE_MATCHER)))
                .thenReturn(mockResult);
        // Push a file with some atoms to the device. If it fails to push or parse the file, or
        // any of the commands do not match, the utility will throw an exception and fail this test.
        long configId =
                ConfigUtil.pushStatsConfig(
                        mTestDevice,
                        ImmutableList.of(
                                Atom.SCREEN_STATE_CHANGED_FIELD_NUMBER,
                                Atom.SHUTDOWN_SEQUENCE_REPORTED_FIELD_NUMBER,
                                Atom.BOOT_SEQUENCE_REPORTED_FIELD_NUMBER));
        // Verify that key methods and commands are called on the device.
        verify(mTestDevice, times(1)).pushFile(any(File.class), anyString());
        verify(mTestDevice, times(1)).executeShellV2Command(matches(STATS_CONFIG_UPDATE_MATCHER));
        verify(mTestDevice, times(1)).deleteFile(matches(REMOTE_PATH_MATCHER));
    }

    /** Tests that an invalid configuration is reported as failing when unparseable. */
    @Test
    public void testPushInvalidDeviceConfig() throws DeviceNotAvailableException, IOException {
        // Mock the push file, update config, and rm commands.
        when(mTestDevice.pushFile(any(File.class), anyString())).thenReturn(true);
        CommandResult mockResult = new CommandResult();
        mockResult.setStderr(PARSING_ERROR_MSG);
        when(mTestDevice.executeShellV2Command(matches(STATS_CONFIG_UPDATE_MATCHER)))
                .thenReturn(mockResult);
        try {
            // Push a file with some atoms to the device and expect parsing failure.
            long configId =
                    ConfigUtil.pushStatsConfig(
                            mTestDevice,
                            ImmutableList.of(
                                    Atom.SCREEN_STATE_CHANGED_FIELD_NUMBER,
                                    Atom.SHUTDOWN_SEQUENCE_REPORTED_FIELD_NUMBER,
                                    Atom.BOOT_SEQUENCE_REPORTED_FIELD_NUMBER));
            fail("The configuration utility should fail to parse and throw.");
        } catch (RuntimeException e) {
            // Expect a failure to parse the file on the device.
            assertThat(e.getMessage()).contains("Failed to parse");
        }
    }

    /** Tests that a statsd config proto binary is pushed to the device with expected calls. */
    @Test
    public void testPushBinaryDeviceConfig() throws DeviceNotAvailableException, IOException {
        // Mock the file operations, push binary file, update config and rm commands.
        when(mConfigFile.exists()).thenReturn(true);
        when(mTestDevice.pushFile(any(File.class), anyString())).thenReturn(true);
        CommandResult mockResult = new CommandResult();
        mockResult.setStderr("");
        when(mTestDevice.executeShellV2Command(matches(STATS_CONFIG_UPDATE_MATCHER)))
                .thenReturn(mockResult);
        // Push the config binary file to statsd.
        long configId = ConfigUtil.pushBinaryStatsConfig(mTestDevice, mConfigFile);
        // Verify that key methods and commands are called on the device
        verify(mTestDevice, times(1)).pushFile(eq(mConfigFile), anyString());
        verify(mTestDevice, times(1)).executeShellV2Command(matches(STATS_CONFIG_UPDATE_MATCHER));
        verify(mTestDevice, times(1)).deleteFile(matches(REMOTE_PATH_MATCHER));
    }

    @Test
    public void testPushBinaryDeviceConfigBinary_throwsWhenNotExists()
            throws IOException, DeviceNotAvailableException {
        // Mock the file does not exists
        when(mConfigFile.exists()).thenReturn(false);
        try {
            long configId = ConfigUtil.pushBinaryStatsConfig(mTestDevice, mConfigFile);
            fail("The configuration utility should fail and report file not found error");
        } catch (IOException e) {
            assertThat(e.getMessage()).contains("File not found");
        }
    }

    /** Test that the remove config shell command is executed on the device with the config id. */
    @Test
    public void testRemoveDeviceConfig() throws DeviceNotAvailableException {
        // Mock the remove config commands.
        when(mTestDevice.executeShellCommand(startsWith(STATS_CONFIG_REMOVE_MATCHER)))
                .thenReturn("");
        long configId = 111;
        // Call and verify that the remove config command is executed with the config id.
        ConfigUtil.removeConfig(mTestDevice, configId);
        verify(mTestDevice, times(1))
                .executeShellCommand(
                        eq(
                                String.join(
                                        " ",
                                        STATS_CONFIG_REMOVE_MATCHER,
                                        String.valueOf(configId))));
    }
}
