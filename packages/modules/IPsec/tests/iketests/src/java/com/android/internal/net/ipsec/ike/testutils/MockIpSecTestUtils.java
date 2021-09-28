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

package com.android.internal.net.ipsec.ike.testutils;

import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.IPPROTO_UDP;
import static android.system.OsConstants.SOCK_DGRAM;

import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.IpSecManager;
import android.net.IpSecSpiResponse;
import android.net.IpSecUdpEncapResponse;
import android.system.Os;

import androidx.test.InstrumentationRegistry;

import com.android.server.IpSecService;

/** This class provides utility methods for mocking IPsec surface. */
public final class MockIpSecTestUtils {
    private static final int DUMMY_CHILD_SPI = 0x2ad4c0a2;
    private static final int DUMMY_CHILD_SPI_RESOURCE_ID = 0x1234;

    private static final int DUMMY_UDP_ENCAP_PORT = 34567;
    private static final int DUMMY_UDP_ENCAP_RESOURCE_ID = 0x3234;

    private Context mContext;
    private IpSecService mMockIpSecService;
    private IpSecManager mIpSecManager;

    private MockIpSecTestUtils() throws Exception {
        mMockIpSecService = mock(IpSecService.class);
        mContext = InstrumentationRegistry.getContext();
        mIpSecManager = new IpSecManager(mContext, mMockIpSecService);

        when(mMockIpSecService.allocateSecurityParameterIndex(anyString(), anyInt(), anyObject()))
                .thenReturn(
                        new IpSecSpiResponse(
                                IpSecManager.Status.OK,
                                DUMMY_CHILD_SPI_RESOURCE_ID,
                                DUMMY_CHILD_SPI));

        when(mMockIpSecService.openUdpEncapsulationSocket(anyInt(), anyObject()))
                .thenReturn(
                        new IpSecUdpEncapResponse(
                                IpSecManager.Status.OK,
                                DUMMY_UDP_ENCAP_RESOURCE_ID,
                                DUMMY_UDP_ENCAP_PORT,
                                Os.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)));
    }

    public static MockIpSecTestUtils setUpMockIpSec() throws Exception {
        return new MockIpSecTestUtils();
    }

    public static IpSecSpiResponse buildDummyIpSecSpiResponse(int spi) throws Exception {
        return new IpSecSpiResponse(IpSecManager.Status.OK, DUMMY_CHILD_SPI_RESOURCE_ID, spi);
    }

    public static IpSecUdpEncapResponse buildDummyIpSecUdpEncapResponse(int port) throws Exception {
        return new IpSecUdpEncapResponse(
                IpSecManager.Status.OK,
                DUMMY_UDP_ENCAP_RESOURCE_ID,
                port,
                Os.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    }

    public Context getContext() {
        return mContext;
    }

    public IpSecManager getIpSecManager() {
        return mIpSecManager;
    }

    public IpSecService getIpSecService() {
        return mMockIpSecService;
    }
}
