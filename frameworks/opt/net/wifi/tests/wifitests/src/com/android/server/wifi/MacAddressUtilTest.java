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

package com.android.server.wifi;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.net.MacAddress;
import android.net.wifi.WifiConfiguration;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.security.ProviderException;
import java.util.Random;

import javax.crypto.Mac;

/**
 * Unit tests for {@link com.android.server.wifi.MacAddressUtil}.
 */
@SmallTest
public class MacAddressUtilTest extends WifiBaseTest {
    private MacAddressUtil mMacAddressUtil;

    @Mock private Mac mMac;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mMacAddressUtil = new MacAddressUtil();
    }

    /**
     * Verifies that calculatePersistentMac generate valid randomized MACs.
     */
    @Test
    public void testCalculatePersistentMac() {
        // verify null inputs
        assertNull(mMacAddressUtil.calculatePersistentMac(null, null));

        Random rand = new Random();
        // Verify that a the MAC address calculated is valid
        for (int i = 0; i < 10; i++) {
            byte[] bytes = new byte[32];
            rand.nextBytes(bytes);
            when(mMac.doFinal(any())).thenReturn(bytes);
            MacAddress macAddress = mMacAddressUtil.calculatePersistentMac(
                    "TEST_SSID_AND_SECURITY_TYPE_" + i, mMac);
            assertTrue(WifiConfiguration.isValidMacAddressForRandomization(macAddress));
        }
    }

    /**
     * Verify the java.security.ProviderException is caught.
     */
    @Test
    public void testCalculatePersistentMacCatchesException() {
        when(mMac.doFinal(any())).thenThrow(new ProviderException("error occurred"));
        try {
            assertNull(mMacAddressUtil.calculatePersistentMac("TEST_SSID_AND_SECURITY_TYPE",
                    mMac));
        } catch (Exception e) {
            fail("Exception not caught.");
        }
    }
}
