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

import static org.junit.Assert.assertEquals;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.UUID;

/** Functional tests for {@link UserCleaner}. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class UserCleanerFuncTest extends BaseHostJUnit4Test {

    private UserCleaner mCleaner;

    @Before
    public void setUp() {
        mCleaner = new UserCleaner();
    }

    @After
    public void tearDown() throws DeviceNotAvailableException {
        // ensure additional users are removed
        ITestDevice device = getDevice();
        for (int id : device.listUsers()) {
            device.removeUser(id);
        }
    }

    @Test
    public void testTearDown() throws DeviceNotAvailableException {
        ITestDevice device = getDevice();
        assertEquals("one user initially", 1, device.listUsers().size());

        device.createUser(UUID.randomUUID().toString());
        device.createUser(UUID.randomUUID().toString());
        assertEquals("additional users created", 3, device.listUsers().size());

        mCleaner.tearDown(getTestInformation(), null);
        assertEquals("additional users removed", 1, device.listUsers().size());
    }
}
