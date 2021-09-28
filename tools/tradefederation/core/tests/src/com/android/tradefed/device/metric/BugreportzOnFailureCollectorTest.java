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
package com.android.tradefed.device.metric;

import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

/** Unit tests for {@link BugreportzOnFailureCollector}. */
@RunWith(JUnit4.class)
public class BugreportzOnFailureCollectorTest {
    private BugreportzOnFailureCollector mCollector;
    private ITestInvocationListener mMockListener;
    private ITestDevice mMockDevice;

    private ITestInvocationListener mTestListener;
    private IInvocationContext mContext;

    @Before
    public void setUp() {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mCollector = new BugreportzOnFailureCollector();
        mContext = new InvocationContext();
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mTestListener = mCollector.init(mContext, mMockListener);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("serial");
    }

    @Test
    public void testCollect() throws Exception {
        TestDescription test = new TestDescription("class", "test");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testFailed(EasyMock.eq(test), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.eq(test),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());

        EasyMock.expect(
                        mMockDevice.logBugreport(
                                EasyMock.eq("class#test-serial-bugreportz-on-failure"),
                                EasyMock.anyObject()))
                .andReturn(true);

        EasyMock.replay(mMockListener, mMockDevice);
        mTestListener.testStarted(test);
        mTestListener.testFailed(test, "I failed");
        mTestListener.testEnded(test, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice);
    }
}
