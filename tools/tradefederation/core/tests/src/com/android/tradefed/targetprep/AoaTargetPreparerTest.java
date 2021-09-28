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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.ignoreStubs;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import com.android.helper.aoa.AoaDevice;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.TestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnitRunner;

import java.awt.Point;
import java.time.Duration;
import java.util.List;

/** Unit tests for {@link AoaTargetPreparer} */
@RunWith(MockitoJUnitRunner.class)
public class AoaTargetPreparerTest {

    @Spy private AoaTargetPreparer mPreparer;
    private OptionSetter mOptionSetter;

    @Mock private TestDevice mTestDevice;
    @Mock private IBuildInfo mBuildInfo;
    @Mock private UsbHelper mUsb;
    @Mock private AoaDevice mDevice;

    private TestInformation mTestInfo;

    @Before
    public void setUp() throws ConfigurationException {
        when(mUsb.getAoaDevice(any(), any())).thenReturn(mDevice);
        doReturn(mUsb).when(mPreparer).getUsbHelper();
        mOptionSetter = new OptionSetter(mPreparer);
        when(mDevice.getSerialNumber()).thenReturn("SERIAL");

        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mTestDevice);
        context.addDeviceBuildInfo("device", mBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testSetUp() throws Exception {
        mOptionSetter.setOptionValue("action", "wake");
        mOptionSetter.setOptionValue("device-timeout", "1s");
        mOptionSetter.setOptionValue("wait-for-device-online", "true");

        mPreparer.setUp(mTestInfo);
        // fetched device, executed actions, and verified status
        verify(mUsb).getAoaDevice(any(), eq(Duration.ofSeconds(1L)));
        verify(mPreparer).execute(eq(mDevice), eq("wake"));
        verify(mTestDevice).waitForDeviceOnline();
    }

    @Test
    public void testSetUp_noActions() throws Exception {
        mPreparer.setUp(mTestInfo);
        // no-op if no actions provided
        verifyZeroInteractions(mUsb);
        verify(mPreparer, never()).execute(eq(mDevice), any());
        verifyZeroInteractions(mTestDevice);
    }

    @Test(expected = DeviceNotAvailableException.class)
    public void testSetUp_noDevice() throws Exception {
        mOptionSetter.setOptionValue("action", "wake");
        when(mUsb.getAoaDevice(any(), any())).thenReturn(null); // device not found or incompatible
        mPreparer.setUp(mTestInfo);
    }

    @Test
    public void testSetUp_skipDeviceCheck() throws Exception {
        mOptionSetter.setOptionValue("action", "wake");
        mOptionSetter.setOptionValue("wait-for-device-online", "false"); // device check disabled

        mPreparer.setUp(mTestInfo);
        // actions executed, but status check skipped
        verify(mPreparer).execute(eq(mDevice), eq("wake"));
        verify(mTestDevice, never()).waitForDeviceOnline();
    }

    @Test
    public void testClick() {
        mPreparer.execute(mDevice, "click 1 23");

        verify(mDevice).click(eq(new Point(1, 23)));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testLongClick() {
        mPreparer.execute(mDevice, "longClick 23 4");

        verify(mDevice).longClick(eq(new Point(23, 4)));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testSwipe() {
        mPreparer.execute(mDevice, "swipe 3 45 123 6 78");

        verify(mDevice)
                .swipe(eq(new Point(3, 45)), eq(new Point(6, 78)), eq(Duration.ofMillis(123)));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testWrite() {
        mPreparer.execute(mDevice, "write Test 0123");

        verify(mDevice).pressKeys(List.of(0x17, 0x08, 0x16, 0x17, 0x2C, 0x27, 0x1E, 0x1F, 0x20));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testKeys() {
        mPreparer.execute(mDevice, "key 43"); // accepts decimal values
        mPreparer.execute(mDevice, "key 0x2B"); // accepts hexadecimal values
        mPreparer.execute(mDevice, "key tab"); // accepts key descriptions

        verify(mDevice, times(3)).pressKeys(List.of(0x2B));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testKeys_combination() {
        mPreparer.execute(mDevice, "key 2*a 3*down 2*0x2B");

        verify(mDevice).pressKeys(List.of(0x04, 0x04, 0x51, 0x51, 0x51, 0x2B, 0x2B));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testWake() {
        mPreparer.execute(mDevice, "wake");

        verify(mDevice).wakeUp();
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testHome() {
        mPreparer.execute(mDevice, "home");

        verify(mDevice).goHome();
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testBack() {
        mPreparer.execute(mDevice, "back");

        verify(mDevice).goBack();
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test
    public void testSleep() {
        mPreparer.execute(mDevice, "sleep 123");

        verify(mDevice).sleep(eq(Duration.ofMillis(123L)));
        verifyNoMoreInteractions(ignoreStubs(mDevice));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalid_unknownKeyword() {
        mPreparer.execute(mDevice, "jump 12 3");
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalid_missingCoordinates() {
        mPreparer.execute(mDevice, "click");
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalid_tooFewCoordinates() {
        mPreparer.execute(mDevice, "longClick 1");
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalid_tooManyCoordinates() {
        mPreparer.execute(mDevice, "swipe 1 2 3 4 5 6");
    }
}
