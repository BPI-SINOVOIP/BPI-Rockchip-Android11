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

package com.android.internal.net.eap.statemachine;

import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.net.eap.EapSessionConfig.EapAkaPrimeConfig;
import android.telephony.TelephonyManager;

import com.android.internal.net.eap.message.simaka.EapAkaPrimeTypeData;

import org.junit.Before;

public class EapAkaPrimeTest {

    // network name example in RFC 5448#3.1
    protected static final int SUB_ID = 1;
    protected static final String PEER_NETWORK_NAME = "foo:bar";
    protected static final boolean ALLOW_MISMATCHED_NETWORK_NAMES = false;
    protected static final EapAkaPrimeConfig EAP_AKA_PRIME_CONFIG =
            new EapAkaPrimeConfig(
                    SUB_ID, APPTYPE_USIM, PEER_NETWORK_NAME, ALLOW_MISMATCHED_NETWORK_NAMES);
    protected static final byte[] DUMMY_EAP_TYPE_DATA = hexStringToByteArray("112233445566");

    // EAP-Identity = hex("test@android.net")
    protected static final byte[] EAP_IDENTITY_BYTES =
            hexStringToByteArray("7465737440616E64726F69642E6E6574");

    protected Context mMockContext;
    protected TelephonyManager mMockTelephonyManager;
    protected EapAkaPrimeTypeData.EapAkaPrimeTypeDataDecoder mMockTypeDataDecoder;

    protected EapAkaPrimeMethodStateMachine mStateMachine;

    @Before
    public void setUp() {
        mMockContext = mock(Context.class);
        mMockTelephonyManager = mock(TelephonyManager.class);
        mMockTypeDataDecoder = mock(EapAkaPrimeTypeData.EapAkaPrimeTypeDataDecoder.class);

        TelephonyManager mockTelephonyManagerFromContext = mock(TelephonyManager.class);
        doReturn(mockTelephonyManagerFromContext)
                .when(mMockContext)
                .getSystemService(Context.TELEPHONY_SERVICE);
        doReturn(mMockTelephonyManager)
                .when(mockTelephonyManagerFromContext)
                .createForSubscriptionId(SUB_ID);

        mStateMachine =
                new EapAkaPrimeMethodStateMachine(
                        mMockContext,
                        EAP_IDENTITY_BYTES,
                        EAP_AKA_PRIME_CONFIG,
                        mMockTypeDataDecoder);

        verify(mMockContext).getSystemService(eq(Context.TELEPHONY_SERVICE));
        verify(mockTelephonyManagerFromContext).createForSubscriptionId(SUB_ID);
    }
}
