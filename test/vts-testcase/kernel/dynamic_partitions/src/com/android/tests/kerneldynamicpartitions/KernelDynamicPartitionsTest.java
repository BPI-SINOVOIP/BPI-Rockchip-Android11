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
package com.android.tests.dynamic_partitions;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/* A test to check if dynamic partitions is enabled for new devices in Q and later. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class KernelDynamicPartitionsTest extends BaseHostJUnit4Test {
    // The property to indicate dynamic partitions are enabled.
    private static final String DYNAMIC_PARTITIONS_PROP = "ro.boot.dynamic_partitions";

    private ITestDevice mDevice;

    @Before
    public void setUp() throws Exception {
        mDevice = getDevice();
    }

    /* Checks ro.build.dynamic_partitions is 'true'. */
    @Test
    public void testDynamicPartitionsSysProp() throws Exception {
        Assert.assertEquals(DYNAMIC_PARTITIONS_PROP + " is not true", "true",
                mDevice.getProperty(DYNAMIC_PARTITIONS_PROP));
    }
}
