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

package com.android.tradefed.targetprep;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;

import com.google.common.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link UserCleaner}. */
@RunWith(JUnit4.class)
public class UserCleanerTest {

    private ITestDevice mMockDevice;
    private UserCleaner mCleaner;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mMockDevice = mock(ITestDevice.class);
        mCleaner = new UserCleaner();
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testRemoveUsers() throws DeviceNotAvailableException {
        when(mMockDevice.getPrimaryUserId()).thenReturn(0);
        when(mMockDevice.listUsers()).thenReturn(Lists.newArrayList(0, 1, 2, 3));

        mCleaner.tearDown(mTestInfo, null);

        // switch to primary user
        verify(mMockDevice, times(1)).switchUser(0);
        // remove remaining
        verify(mMockDevice, never()).removeUser(eq(0));
        verify(mMockDevice, times(1)).removeUser(eq(1));
        verify(mMockDevice, times(1)).removeUser(eq(2));
        verify(mMockDevice, times(1)).removeUser(eq(3));
    }
}
