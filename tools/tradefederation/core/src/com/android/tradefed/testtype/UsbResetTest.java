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
package com.android.tradefed.testtype;

import com.android.helper.aoa.UsbDevice;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.TestDeviceState;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import com.google.common.annotations.VisibleForTesting;

/**
 * An {@link IRemoteTest} that reset the device USB and checks whether the device comes back online
 * afterwards.
 */
@OptionClass(alias = "usb-reset-test")
public class UsbResetTest implements IRemoteTest {

    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        try (UsbHelper usb = getUsbHelper()) {
            String serial = device.getSerialNumber();
            try (UsbDevice usbDevice = usb.getDevice(serial)) {
                if (usbDevice == null) {
                    throw new DeviceNotAvailableException(
                            String.format("Device '%s' not found during USB reset.", serial),
                            serial,
                            DeviceErrorIdentifier.DEVICE_UNAVAILABLE);
                } else {
                    CLog.d("Resetting USB port for device '%s'", serial);
                    usbDevice.reset();
                    getRunUtil().sleep(500);

                    TestDeviceState state = device.getDeviceState();
                    if (TestDeviceState.RECOVERY.equals(state)
                            || device.isStateBootloaderOrFastbootd()) {
                        CLog.d("Device state is '%s', attempting reboot.", state);
                        device.reboot();
                    } else {
                        device.waitForDeviceOnline();
                        // If device fails to reboot it will throw an exception and be left
                        // unavailable
                        // again.
                        device.reboot();
                    }
                }
            }
        }
    }

    @VisibleForTesting
    UsbHelper getUsbHelper() {
        return new UsbHelper();
    }

    @VisibleForTesting
    IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }
}
