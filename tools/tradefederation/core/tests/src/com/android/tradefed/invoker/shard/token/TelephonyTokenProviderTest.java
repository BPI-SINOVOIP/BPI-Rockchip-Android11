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
package com.android.tradefed.invoker.shard.token;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.helper.TelephonyHelper.SimCardInformation;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link TelephonyTokenProvider}. */
@RunWith(JUnit4.class)
public class TelephonyTokenProviderTest {

    private TelephonyTokenProvider provider;
    private ITestDevice mDevice;
    private SimCardInformation mSimInfo;

    @Before
    public void setUp() {
        provider =
                new TelephonyTokenProvider() {
                    @Override
                    SimCardInformation getSimInfo(ITestDevice device)
                            throws DeviceNotAvailableException {
                        return mSimInfo;
                    }
                };
        mDevice = Mockito.mock(ITestDevice.class);
    }

    @Test
    public void testSimCard() {
        mSimInfo = new SimCardInformation();
        mSimInfo.mHasTelephonySupport = true;
        mSimInfo.mSimState = "5"; // Ready
        assertTrue(provider.hasToken(mDevice, TokenProperty.SIM_CARD));
    }

    @Test
    public void testSimCard_noSupport() {
        mSimInfo = new SimCardInformation();
        mSimInfo.mHasTelephonySupport = false;
        assertFalse(provider.hasToken(mDevice, TokenProperty.SIM_CARD));
    }

    @Test
    public void testSimCard_carrier() {
        mSimInfo = new SimCardInformation();
        mSimInfo.mHasTelephonySupport = true;
        mSimInfo.mCarrierPrivileges = true;
        assertTrue(provider.hasToken(mDevice, TokenProperty.UICC_SIM_CARD));
    }

    @Test
    public void testSimCard_securedElementOrange() throws Exception {
        mSimInfo = new SimCardInformation();
        mSimInfo.mHasTelephonySupport = true;
        mSimInfo.mHasSecuredElement = true;
        mSimInfo.mHasSeService = true;
        Mockito.doReturn(TelephonyTokenProvider.ORANGE_SIM_ID)
                .when(mDevice)
                .getProperty(TelephonyTokenProvider.GSM_OPERATOR_PROP);
        assertTrue(provider.hasToken(mDevice, TokenProperty.SECURE_ELEMENT_SIM_CARD));
    }

    @Test
    public void testSimCard_securedElementOrange_dualSim() throws Exception {
        mSimInfo = new SimCardInformation();
        mSimInfo.mHasTelephonySupport = true;
        mSimInfo.mHasSecuredElement = true;
        mSimInfo.mHasSeService = true;
        Mockito.doReturn(TelephonyTokenProvider.ORANGE_SIM_ID + ",9999")
                .when(mDevice)
                .getProperty(TelephonyTokenProvider.GSM_OPERATOR_PROP);
        assertTrue(provider.hasToken(mDevice, TokenProperty.SECURE_ELEMENT_SIM_CARD));
    }

    @Test
    public void testSimCard_securedElement_anySim() throws Exception {
        mSimInfo = new SimCardInformation();
        mSimInfo.mHasTelephonySupport = true;
        mSimInfo.mHasSecuredElement = true;
        mSimInfo.mHasSeService = true;
        Mockito.doReturn("8888")
                .when(mDevice)
                .getProperty(TelephonyTokenProvider.GSM_OPERATOR_PROP);
        assertFalse(provider.hasToken(mDevice, TokenProperty.SECURE_ELEMENT_SIM_CARD));
    }
}
