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
 * limitations under the License.
 */
package com.android.tradefed.util;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.contains;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.List;

/** Unit tests for {@link JavaCodeCoverageFlusher}. */
@RunWith(JUnit4.class)
public final class JavaCodeCoverageFlusherTest {

    private static final String PS_OUTPUT =
            "USER       PID   PPID  VSZ   RSS   WCHAN       PC  S NAME\n"
                    + "bluetooth   123  1366  123   456   SyS_epoll+   0  S com.android.bluetooth\n"
                    + "u0_a80     4567  1366  456   789   SyS_epoll+   0  S com.google.android.gms.persistent\n"
                    + "radio       890     1 7890   123   binder_io+   0  S com.android.phone\n"
                    + "root         11  1234  567   890   binder_io+   0  S not.a.java.package\n";

    private static final String PM_LIST_PACKAGES_OUTPUT =
            "package:com.android.bluetooth\n"
                    + "package:com.google.android.gms.persistent\n"
                    + "package:com.android.not.used\n"
                    + "package:com.android.phone\n";

    @Mock ITestDevice mMockDevice;

    // Object under test
    JavaCodeCoverageFlusher mFlusher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testResetAll_calledForAllProcesses() throws DeviceNotAvailableException {
        mFlusher = new JavaCodeCoverageFlusher(mMockDevice, ImmutableList.of());

        doReturn(PS_OUTPUT).when(mMockDevice).executeShellCommand("ps -e");
        doReturn(PM_LIST_PACKAGES_OUTPUT)
                .when(mMockDevice)
                .executeShellCommand("pm list packages -a");
        doReturn("123").when(mMockDevice).getProcessPid("com.android.bluetooth");
        doReturn("4567").when(mMockDevice).getProcessPid("com.google.android.gms.persistent");
        doReturn("890").when(mMockDevice).getProcessPid("com.android.phone");

        mFlusher.resetCoverage();

        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.android.bluetooth "
                                + "/system/lib/libdumpcoverage.so=reset");
        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.google.android.gms.persistent "
                                + "/system/lib/libdumpcoverage.so=reset");
        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.android.phone /system/lib/libdumpcoverage.so=reset");

        verify(mMockDevice).executeShellCommand("cmd coverage reset");
    }

    @Test
    public void testResetSpecific_calledForSpecificProcesses() throws DeviceNotAvailableException {
        mFlusher = new JavaCodeCoverageFlusher(mMockDevice, ImmutableList.of("com.android.phone"));

        doReturn(PS_OUTPUT).when(mMockDevice).executeShellCommand("ps -e");
        doReturn(PM_LIST_PACKAGES_OUTPUT)
                .when(mMockDevice)
                .executeShellCommand("pm list packages -a");
        doReturn("123").when(mMockDevice).getProcessPid("com.android.phone");

        mFlusher.resetCoverage();

        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.android.phone /system/lib/libdumpcoverage.so=reset");

        verify(mMockDevice).executeShellCommand("cmd coverage reset");
    }

    @Test
    public void testDumpAll_calledForAllProcesses() throws DeviceNotAvailableException {
        mFlusher = new JavaCodeCoverageFlusher(mMockDevice, ImmutableList.of());

        doReturn(PS_OUTPUT).when(mMockDevice).executeShellCommand("ps -e");
        doReturn(PM_LIST_PACKAGES_OUTPUT)
                .when(mMockDevice)
                .executeShellCommand("pm list packages -a");
        doReturn("123").when(mMockDevice).getProcessPid("com.android.bluetooth");
        doReturn("4567").when(mMockDevice).getProcessPid("com.google.android.gms.persistent");
        doReturn("890").when(mMockDevice).getProcessPid("com.android.phone");

        List<String> coverageFiles = mFlusher.forceCoverageFlush();

        assertThat(coverageFiles)
                .containsExactly(
                        "/data/misc/trace/com.android.bluetooth-123.ec",
                        "/data/misc/trace/com.google.android.gms.persistent-4567.ec",
                        "/data/misc/trace/com.android.phone-890.ec",
                        "/data/misc/trace/system_server.ec");

        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.android.bluetooth /system/lib/libdumpcoverage.so="
                                + "dump:/data/misc/trace/com.android.bluetooth-123.ec");
        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.google.android.gms.persistent "
                                + "/system/lib/libdumpcoverage.so=dump:"
                                + "/data/misc/trace/com.google.android.gms.persistent-4567.ec");
        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.android.phone /system/lib/libdumpcoverage.so="
                                + "dump:/data/misc/trace/com.android.phone-890.ec");

        verify(mMockDevice).executeShellCommand(contains("cmd coverage dump"));
    }

    @Test
    public void testDumpSpecific_calledForSpecificProcesses() throws DeviceNotAvailableException {
        mFlusher =
                new JavaCodeCoverageFlusher(
                        mMockDevice,
                        ImmutableList.of(
                                "com.android.bluetooth", "com.google.android.gms.persistent"));

        doReturn(PM_LIST_PACKAGES_OUTPUT)
                .when(mMockDevice)
                .executeShellCommand("pm list packages -a");
        doReturn("234").when(mMockDevice).getProcessPid("com.android.bluetooth");

        // Request coverage for com.android.bluetooth (valid process) and
        // com.google.android.gms.persistent (not a valid process, coverage is not flushed).
        List<String> coverageFiles = mFlusher.forceCoverageFlush();

        assertThat(coverageFiles)
                .containsExactly(
                        "/data/misc/trace/com.android.bluetooth-234.ec",
                        "/data/misc/trace/system_server.ec");

        verify(mMockDevice).executeShellCommand("pm list packages -a");
        verify(mMockDevice)
                .executeShellCommand(
                        "am attach-agent com.android.bluetooth /system/lib/libdumpcoverage.so"
                                + "=dump:/data/misc/trace/com.android.bluetooth-234.ec");
        verify(mMockDevice).executeShellCommand(contains("cmd coverage dump"));
    }
}
