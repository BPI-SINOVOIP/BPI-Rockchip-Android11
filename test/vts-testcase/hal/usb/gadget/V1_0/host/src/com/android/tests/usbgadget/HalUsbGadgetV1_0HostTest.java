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

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import com.android.tests.usbgadget.libusb.ConfigDescriptor;
import com.android.tests.usbgadget.libusb.DeviceDescriptor;
import com.android.tests.usbgadget.libusb.IUsbNative;
import com.android.tests.usbgadget.libusb.Interface;
import com.android.tests.usbgadget.libusb.InterfaceDescriptor;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.BeforeClassWithInfo;
import com.google.common.base.Strings;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import java.util.Arrays;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;

/** A host-side test for USB Gadget HAL */
@RunWith(DeviceJUnit4ClassRunner.class)
public class HalUsbGadgetV1_0HostTest extends BaseHostJUnit4Test {
    private static final int WAIT_TIME = 3000;
    private static final String HAL_SERVICE = "android.hardware.usb.gadget@1.0::IUsbGadget";

    private static boolean mHasService;
    private static IUsbNative mUsb;
    private static Pointer mContext;

    @BeforeClassWithInfo
    public static void beforeClassWithDevice(TestInformation testInfo) throws Exception {
        String serviceFound =
                testInfo.getDevice()
                        .executeShellCommand(String.format("lshal | grep \"%s\"", HAL_SERVICE))
                        .trim();
        mHasService = !Strings.isNullOrEmpty(serviceFound);

        if (mHasService) {
            mUsb = (IUsbNative) Native.loadLibrary("usb-1.0", IUsbNative.class);
            PointerByReference context = new PointerByReference();
            mUsb.libusb_init(context);
            mContext = context.getValue();
        }
    }

    private static boolean checkProtocol(int usbClass, int usbSubClass, int usbProtocol) {
        PointerByReference list = new PointerByReference();
        int count = mUsb.libusb_get_device_list(mContext, list);
        Pointer[] devices = list.getValue().getPointerArray(0, count);
        for (Pointer device : devices) {
            DeviceDescriptor[] devDescriptors = new DeviceDescriptor[1];
            mUsb.libusb_get_device_descriptor(device, devDescriptors);
            for (int j = 0; j < devDescriptors[0].bNumConfigurations; j++) {
                PointerByReference configRef = new PointerByReference();
                int success = mUsb.libusb_get_config_descriptor(device, j, configRef);
                ConfigDescriptor config = new ConfigDescriptor(configRef.getValue());
                List<Interface> interfaces =
                        Arrays.asList(config.interfaces.toArray(config.bNumInterfaces));
                for (Interface interface_ : interfaces) {
                    List<InterfaceDescriptor> descriptors =
                            Arrays.asList(interface_.altsetting.toArray(interface_.num_altsetting));
                    for (InterfaceDescriptor d : descriptors) {
                        if (Byte.toUnsignedInt(d.bInterfaceClass) == usbClass
                                && Byte.toUnsignedInt(d.bInterfaceSubClass) == usbSubClass
                                && Byte.toUnsignedInt(d.bInterfaceProtocol) == usbProtocol) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    /** Check for ADB */
    @Test
    public void testAndroidUSB() throws Exception {
        assumeTrue(String.format("The device doesn't have service %s", HAL_SERVICE), mHasService);
        assertTrue("ADB not present", checkProtocol(255, 66, 1));
    }

    /**
     * Check for MTP.
     *
     * <p>Enables mtp and checks the host to see if mtp interface is present. MTP:
     * https://en.wikipedia.org/wiki/Media_Transfer_Protocol.
     */
    @Test
    public void testMtp() throws Exception {
        assumeTrue(String.format("The device doesn't have service %s", HAL_SERVICE), mHasService);
        getDevice().executeShellCommand("svc usb setFunctions mtp true");
        Thread.sleep(WAIT_TIME);
        assertTrue("MTP not present", checkProtocol(6, 1, 1));
    }

    /**
     * Check for PTP.
     *
     * <p>Enables ptp and checks the host to see if ptp interface is present. PTP:
     * https://en.wikipedia.org/wiki/Picture_Transfer_Protocol.
     */
    @Test
    public void testPtp() throws Exception {
        assumeTrue(String.format("The device doesn't have service %s", HAL_SERVICE), mHasService);
        getDevice().executeShellCommand("svc usb setFunctions ptp true");
        Thread.sleep(WAIT_TIME);
        assertTrue("PTP not present", checkProtocol(6, 1, 1));
    }

    /**
     * Check for MIDI.
     *
     * <p>Enables midi and checks the host to see if midi interface is present. MIDI:
     * https://en.wikipedia.org/wiki/MIDI.
     */
    @Test
    public void testMIDI() throws Exception {
        assumeTrue(String.format("The device doesn't have service %s", HAL_SERVICE), mHasService);
        getDevice().executeShellCommand("svc usb setFunctions midi true");
        Thread.sleep(WAIT_TIME);
        assertTrue("MIDI not present", checkProtocol(1, 3, 0));
    }

    /**
     * Check for RNDIS.
     *
     * <p>Enables rndis and checks the host to see if rndis interface is present. RNDIS:
     * https://en.wikipedia.org/wiki/RNDIS.
     */
    @Test
    public void testRndis() throws Exception {
        assumeTrue(String.format("The device doesn't have service %s", HAL_SERVICE), mHasService);
        getDevice().executeShellCommand("svc usb setFunctions rndis true");
        Thread.sleep(WAIT_TIME);
        assertTrue("RNDIS not present", checkProtocol(10, 0, 0));
    }
}
