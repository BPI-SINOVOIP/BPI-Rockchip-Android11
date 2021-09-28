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
package com.android.tradefed.suite.checker;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link EnforcedSeLinuxChecker}. */
@RunWith(JUnit4.class)
public class EnforcedSeLinuxCheckerTest {

    private EnforcedSeLinuxChecker mChecker;
    private ITestDevice mMockDevice;

    @Before
    public void setUp() {
        mChecker = new EnforcedSeLinuxChecker();
        mMockDevice = Mockito.mock(ITestDevice.class);
    }

    @Test
    public void testEnforced() throws Exception {
        OptionSetter setter = new OptionSetter(mChecker);
        setter.setOptionValue("expect-enforced", "true");

        doReturn("Enforcing").when(mMockDevice).executeShellCommand("getenforce");

        StatusCheckerResult result = mChecker.postExecutionCheck(mMockDevice);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
    }

    @Test
    public void testPermissive() throws Exception {
        OptionSetter setter = new OptionSetter(mChecker);
        setter.setOptionValue("expect-enforced", "false");

        doReturn("Permissive").when(mMockDevice).executeShellCommand("getenforce");

        StatusCheckerResult result = mChecker.postExecutionCheck(mMockDevice);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
    }

    @Test
    public void testEnforced_failed() throws Exception {
        OptionSetter setter = new OptionSetter(mChecker);
        setter.setOptionValue("expect-enforced", "true");

        doReturn("Permissive").when(mMockDevice).executeShellCommand("getenforce");

        StatusCheckerResult result = mChecker.postExecutionCheck(mMockDevice);
        assertEquals(CheckStatus.FAILED, result.getStatus());

        verify(mMockDevice).executeShellCommand("su root setenforce 1");
    }
}
