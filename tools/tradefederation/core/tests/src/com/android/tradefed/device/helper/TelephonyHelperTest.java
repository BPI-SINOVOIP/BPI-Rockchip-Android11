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
package com.android.tradefed.device.helper;

import static org.easymock.EasyMock.getCurrentArguments;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.ddmlib.IDevice;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.helper.TelephonyHelper.SimCardInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;

import org.easymock.EasyMock;
import org.easymock.IAnswer;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

/** Unit tests for {@link TelephonyHelper}. */
@RunWith(JUnit4.class)
public class TelephonyHelperTest {

    private ITestDevice mDevice;
    private IDevice mMockIDevice;

    @Before
    public void setUp() {
        mDevice = EasyMock.createMock(ITestDevice.class);
        mMockIDevice = EasyMock.createMock(IDevice.class);

        EasyMock.expect(mDevice.getIDevice()).andStubReturn(mMockIDevice);
    }

    @Test
    public void testGetSimInfo() throws Exception {
        EasyMock.expect(mDevice.installPackage(EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null);
        EasyMock.expect(mDevice.uninstallPackage(TelephonyHelper.PACKAGE_NAME)).andReturn(null);

        EasyMock.expect(
                        mDevice.runInstrumentationTests(
                                EasyMock.anyObject(),
                                (ITestInvocationListener) EasyMock.anyObject()))
                .andAnswer(
                        new IAnswer<Boolean>() {

                            @Override
                            public Boolean answer() throws Throwable {
                                ITestInvocationListener collector =
                                        (ITestInvocationListener) getCurrentArguments()[1];
                                collector.testRunStarted("android.telephony.utility", 1);
                                collector.testStarted(TelephonyHelper.SIM_TEST);
                                HashMap<String, String> testMetrics = new HashMap<>();
                                testMetrics.put(TelephonyHelper.SIM_STATE_KEY, "5");
                                testMetrics.put(TelephonyHelper.CARRIER_PRIVILEGES_KEY, "true");
                                testMetrics.put(TelephonyHelper.SECURED_ELEMENT_KEY, "false");
                                testMetrics.put(TelephonyHelper.SE_SERVICE_KEY, "false");
                                collector.testEnded(TelephonyHelper.SIM_TEST, testMetrics);
                                collector.testRunEnded(500L, new HashMap<String, Metric>());
                                return true;
                            }
                        });
        EasyMock.replay(mDevice, mMockIDevice);
        SimCardInformation info = TelephonyHelper.getSimInfo(mDevice);
        assertTrue(info.mCarrierPrivileges);
        assertTrue(info.mHasTelephonySupport);
        assertFalse(info.mHasSecuredElement);
        assertFalse(info.mHasSeService);
        assertEquals("5", info.mSimState);
        EasyMock.verify(mDevice, mMockIDevice);
    }

    @Test
    public void testGetSimInfo_installFail() throws Exception {
        EasyMock.expect(mDevice.installPackage(EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn("Failed to install");

        EasyMock.replay(mDevice, mMockIDevice);
        assertNull(TelephonyHelper.getSimInfo(mDevice));
        EasyMock.verify(mDevice, mMockIDevice);
    }

    @Test
    public void testGetSimInfo_instrumentationFailed() throws Exception {
        EasyMock.expect(mDevice.installPackage(EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null);
        EasyMock.expect(mDevice.uninstallPackage(TelephonyHelper.PACKAGE_NAME)).andReturn(null);

        EasyMock.expect(
                        mDevice.runInstrumentationTests(
                                EasyMock.anyObject(),
                                (ITestInvocationListener) EasyMock.anyObject()))
                .andAnswer(
                        new IAnswer<Boolean>() {

                            @Override
                            public Boolean answer() throws Throwable {
                                ITestInvocationListener collector =
                                        (ITestInvocationListener) getCurrentArguments()[1];
                                collector.testRunStarted("android.telephony.utility", 1);
                                collector.testRunFailed("Couldn't run the instrumentation.");
                                collector.testRunEnded(500L, new HashMap<String, Metric>());
                                return true;
                            }
                        });
        EasyMock.replay(mDevice, mMockIDevice);
        assertNull(TelephonyHelper.getSimInfo(mDevice));
        EasyMock.verify(mDevice, mMockIDevice);
    }

    @Test
    public void testGetSimInfo_simTest_not_run() throws Exception {
        EasyMock.expect(mDevice.installPackage(EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null);
        EasyMock.expect(mDevice.uninstallPackage(TelephonyHelper.PACKAGE_NAME)).andReturn(null);

        EasyMock.expect(
                        mDevice.runInstrumentationTests(
                                EasyMock.anyObject(),
                                (ITestInvocationListener) EasyMock.anyObject()))
                .andAnswer(
                        new IAnswer<Boolean>() {

                            @Override
                            public Boolean answer() throws Throwable {
                                ITestInvocationListener collector =
                                        (ITestInvocationListener) getCurrentArguments()[1];
                                collector.testRunStarted("android.telephony.utility", 1);
                                collector.testRunEnded(500L, new HashMap<String, Metric>());
                                return true;
                            }
                        });
        EasyMock.replay(mDevice, mMockIDevice);
        assertNull(TelephonyHelper.getSimInfo(mDevice));
        EasyMock.verify(mDevice, mMockIDevice);
    }

    @Test
    public void testGetSimInfo_simTest_failed() throws Exception {
        EasyMock.expect(mDevice.installPackage(EasyMock.anyObject(), EasyMock.eq(true)))
                .andReturn(null);
        EasyMock.expect(mDevice.uninstallPackage(TelephonyHelper.PACKAGE_NAME)).andReturn(null);

        EasyMock.expect(
                        mDevice.runInstrumentationTests(
                                EasyMock.anyObject(),
                                (ITestInvocationListener) EasyMock.anyObject()))
                .andAnswer(
                        new IAnswer<Boolean>() {

                            @Override
                            public Boolean answer() throws Throwable {
                                ITestInvocationListener collector =
                                        (ITestInvocationListener) getCurrentArguments()[1];
                                collector.testRunStarted("android.telephony.utility", 1);
                                collector.testStarted(TelephonyHelper.SIM_TEST);
                                collector.testFailed(
                                        TelephonyHelper.SIM_TEST, "No TelephonyManager");
                                collector.testEnded(
                                        TelephonyHelper.SIM_TEST, new HashMap<String, String>());
                                collector.testRunEnded(500L, new HashMap<String, Metric>());
                                return true;
                            }
                        });
        EasyMock.replay(mDevice, mMockIDevice);
        SimCardInformation info = TelephonyHelper.getSimInfo(mDevice);
        assertFalse(info.mHasTelephonySupport);
        EasyMock.verify(mDevice, mMockIDevice);
    }
}
