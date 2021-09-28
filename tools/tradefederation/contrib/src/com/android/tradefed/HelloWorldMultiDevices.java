/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.tradefed;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;

import org.junit.Assert;

/**
 * Hello world example of Multiple Devices support in Trade Federation. We implements the existing
 * {@link IRemoteTest} interface to be a TradeFed Tests, and you can implement {@link
 * IInvocationContextReceiver} to get the full invocation metadata. In this example we implement
 * both but you should only implement one or the other.
 */
public class HelloWorldMultiDevices implements IRemoteTest {

    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        IInvocationContext context = testInfo.getContext();
        // We can also use the IInvocationContext information, which have various functions to
        // access the ITestDevice or IBuildInfo.
        for (ITestDevice device : context.getDevices()) {
            CLog.i(
                    "Hello World!  device '%s' from context with build '%s'",
                    device.getSerialNumber(), context.getBuildInfo(device));
        }

        // We can do a look up by the device name in the configuration using the IInvocationContext
        for (String deviceName : context.getDeviceConfigNames()) {
            CLog.i(
                    "device '%s' has the name '%s' in the config.",
                    context.getDevice(deviceName).getSerialNumber(), deviceName);
        }

        // if the device name is known, doing a direct look up is possible.
        Assert.assertNotNull(context.getDevice("device1"));
        CLog.i(
                "device named device1 direct look up is '%s'",
                context.getDevice("device1").getSerialNumber());
    }
}
