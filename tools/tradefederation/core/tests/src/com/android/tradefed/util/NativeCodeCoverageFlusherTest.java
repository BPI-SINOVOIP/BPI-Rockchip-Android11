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

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
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

@RunWith(JUnit4.class)
public final class NativeCodeCoverageFlusherTest {

    @Mock ITestDevice mMockDevice;

    // Object under test
    NativeCodeCoverageFlusher mFlusher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testClearCoverageMeasurements_rmCommandCalled() throws DeviceNotAvailableException {
        doReturn(true).when(mMockDevice).isAdbRoot();

        mFlusher = new NativeCodeCoverageFlusher(mMockDevice, ImmutableList.of());
        mFlusher.resetCoverage();

        // Verify that the rm command was executed.
        verify(mMockDevice).executeShellCommand("rm -rf /data/misc/trace/*");
    }

    @Test
    public void testNoAdbRootClearCoverageMeasurements_noOp() throws DeviceNotAvailableException {
        doReturn(false).when(mMockDevice).isAdbRoot();

        try {
            mFlusher = new NativeCodeCoverageFlusher(mMockDevice, ImmutableList.of());
            mFlusher.resetCoverage();
            fail("Should have thrown an exception");
        } catch (IllegalStateException e) {
            // Expected
        }

        // Verify that no shell command was executed.
        verify(mMockDevice, never()).executeShellCommand(anyString());
    }

    @Test
    public void testFlushCoverageAllProcesses_flushAllCommandCalled()
            throws DeviceNotAvailableException {
        doReturn(true).when(mMockDevice).isAdbRoot();

        mFlusher = new NativeCodeCoverageFlusher(mMockDevice, ImmutableList.of());
        mFlusher.forceCoverageFlush();

        // Verify that the flush command for all processes was called.
        verify(mMockDevice).executeShellCommand("kill -37 -1");
    }

    @Test
    public void testFlushCoverageSpecificProcesses_flushSpecificCommandCalled()
            throws DeviceNotAvailableException {
        List<String> processes = ImmutableList.of("mediaserver", "mediaextractor");

        doReturn(true).when(mMockDevice).isAdbRoot();
        doReturn("12").when(mMockDevice).getProcessPid(processes.get(0));
        doReturn("789").when(mMockDevice).getProcessPid(processes.get(1));

        mFlusher = new NativeCodeCoverageFlusher(mMockDevice, processes);
        mFlusher.forceCoverageFlush();

        // Verify that the flush command for the specific processes was called.
        verify(mMockDevice).executeShellCommand("kill -37 12 789");
    }

    @Test
    public void testNoAdbRootFlush_noOp() throws DeviceNotAvailableException {
        doReturn(false).when(mMockDevice).isAdbRoot();

        try {
            mFlusher = new NativeCodeCoverageFlusher(mMockDevice, ImmutableList.of("mediaserver"));
            mFlusher.forceCoverageFlush();
            fail("Should have thrown an exception");
        } catch (IllegalStateException e) {
            // Expected
        }

        // Verify no shell commands or pid lookups were executed.
        verify(mMockDevice, never()).executeShellCommand(anyString());
        verify(mMockDevice, never()).getProcessPid(anyString());
    }
}
