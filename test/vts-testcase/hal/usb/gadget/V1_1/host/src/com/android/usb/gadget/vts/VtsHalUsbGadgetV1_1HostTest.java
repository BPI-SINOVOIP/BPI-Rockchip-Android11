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

package com.android.tests.usbgadget;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.BeforeClassWithInfo;
import com.google.common.base.Strings;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public final class VtsHalUsbGadgetV1_1HostTest extends BaseHostJUnit4Test {
    public static final String TAG = VtsHalUsbGadgetV1_1HostTest.class.getSimpleName();
    private static final String HAL_SERVICE = "android.hardware.usb.gadget@1.1::IUsbGadget";

    private static final long CONN_TIMEOUT = 5000;

    private static boolean mHasService;
    private ITestDevice mDevice;
    private boolean mReconnected = false;

    @Before
    public void setUp() {
        CLog.i("setUp");

        mDevice = getDevice();
    }

    @BeforeClassWithInfo
    public static void beforeClassWithDevice(TestInformation testInfo) throws Exception {
        String serviceFound =
                testInfo.getDevice()
                        .executeShellCommand(String.format("lshal | grep \"%s\"", HAL_SERVICE))
                        .trim();
        mHasService = !Strings.isNullOrEmpty(serviceFound);
    }

    @Test
    public void testResetUsbGadget() throws Exception {
        Assume.assumeTrue(
                String.format("The device doesn't have service %s", HAL_SERVICE), mHasService);
        Assert.assertNotNull("Target device does not exist", mDevice);

        String deviceSerialNumber = mDevice.getSerialNumber();

        CLog.i("testResetUsbGadget on device [%s]", deviceSerialNumber);

        new Thread(new Runnable() {
            public void run() {
                try {
                    mDevice.waitForDeviceNotAvailable(CONN_TIMEOUT);
                    Thread.sleep(300);
                    mDevice.waitForDeviceAvailable(CONN_TIMEOUT);
                    mReconnected = true;
                } catch (DeviceNotAvailableException dnae) {
                    CLog.e("Device is not available");
                } catch (InterruptedException ie) {
                    CLog.w("Thread.sleep interrupted");
                }
            }
        }).start();

        Thread.sleep(100);
        String cmd = "svc usb resetUsbGadget";
        CLog.i("Invoke shell command [" + cmd + "]");
        long startTime = System.currentTimeMillis();
        mDevice.executeShellCommand("svc usb resetUsbGadget");
        while (!mReconnected && System.currentTimeMillis() - startTime < CONN_TIMEOUT) {
            Thread.sleep(100);
        }

        Assert.assertTrue("usb not reconnect", mReconnected);
    }
}
