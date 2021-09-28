/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.util.executor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import com.android.tradefed.device.ITestDevice;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ParallelDeviceExecutor}. */
@RunWith(JUnit4.class)
public class ParallelDeviceExecutorTest {

    private ParallelDeviceExecutor<Boolean> mExecutor;
    private List<ITestDevice> mDevices;
    private ITestDevice mDevice1;
    private ITestDevice mDevice2;

    @Before
    public void setUp() {
        mDevices = new ArrayList<>();
        mDevice1 = Mockito.mock(ITestDevice.class);
        mDevice2 = Mockito.mock(ITestDevice.class);
        mDevices.add(mDevice1);
        mDevices.add(mDevice2);
        mExecutor = new ParallelDeviceExecutor<>(mDevices);
    }

    @Test
    public void testSimpleExecution() throws Exception {
        List<Callable<Boolean>> callableTasks = new ArrayList<>();
        for (ITestDevice device : mDevices) {
            Callable<Boolean> callableTask =
                    () -> {
                        return device.isAdbRoot();
                    };
            callableTasks.add(callableTask);
        }

        doReturn(true).when(mDevice1).isAdbRoot();
        doReturn(false).when(mDevice2).isAdbRoot();

        List<Boolean> results = mExecutor.invokeAll(callableTasks, 1, TimeUnit.SECONDS);
        assertEquals(2, results.size());
        assertTrue(results.get(0));
        assertFalse(results.get(1));
    }

    @Test
    public void testExecution_errors() throws Exception {
        List<Callable<Boolean>> callableTasks = new ArrayList<>();
        for (ITestDevice device : mDevices) {
            Callable<Boolean> callableTask =
                    () -> {
                        throw new RuntimeException(device.getSerialNumber());
                    };
            callableTasks.add(callableTask);
        }

        doReturn("one").when(mDevice1).getSerialNumber();
        doReturn("two").when(mDevice2).getSerialNumber();

        List<Boolean> results = mExecutor.invokeAll(callableTasks, 1, TimeUnit.SECONDS);
        assertEquals(0, results.size());
        assertTrue(mExecutor.hasErrors());
        assertEquals(2, mExecutor.getErrors().size());
        assertTrue(mExecutor.getErrors().get(0).getMessage().contains("one"));
        assertTrue(mExecutor.getErrors().get(1).getMessage().contains("two"));
    }
}
