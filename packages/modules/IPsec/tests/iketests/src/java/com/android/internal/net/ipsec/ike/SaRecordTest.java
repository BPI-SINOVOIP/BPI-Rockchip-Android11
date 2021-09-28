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

package com.android.internal.net.ipsec.ike;

import static com.android.internal.net.TestUtils.createMockRandomFactory;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.InetAddresses;
import android.net.IpSecManager;
import android.net.IpSecManager.SecurityParameterIndex;
import android.net.IpSecManager.UdpEncapsulationSocket;
import android.net.IpSecTransform;
import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.SaRecord.ChildSaRecord;
import com.android.internal.net.ipsec.ike.SaRecord.ChildSaRecordConfig;
import com.android.internal.net.ipsec.ike.SaRecord.IIpSecTransformHelper;
import com.android.internal.net.ipsec.ike.SaRecord.IkeSaRecord;
import com.android.internal.net.ipsec.ike.SaRecord.IkeSaRecordConfig;
import com.android.internal.net.ipsec.ike.SaRecord.IpSecTransformHelper;
import com.android.internal.net.ipsec.ike.SaRecord.SaLifetimeAlarmScheduler;
import com.android.internal.net.ipsec.ike.SaRecord.SaRecordHelper;
import com.android.internal.net.ipsec.ike.crypto.IkeCipher;
import com.android.internal.net.ipsec.ike.crypto.IkeMacIntegrity;
import com.android.internal.net.ipsec.ike.crypto.IkeMacPrf;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.PrfTransform;
import com.android.internal.net.ipsec.ike.testutils.MockIpSecTestUtils;
import com.android.internal.net.ipsec.ike.utils.IkeSecurityParameterIndex;
import com.android.internal.net.ipsec.ike.utils.IkeSpiGenerator;
import com.android.server.IpSecService;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.net.Inet4Address;

@RunWith(JUnit4.class)
public final class SaRecordTest {
    private static final Inet4Address LOCAL_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.200"));
    private static final Inet4Address REMOTE_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.100"));

    private static final IkeSpiGenerator IKE_SPI_GENERATOR =
            new IkeSpiGenerator(createMockRandomFactory());

    private static final String PRF_KEY_HEX_STRING = "094787780EE466E2CB049FA327B43908BC57E485";
    private static final String DATA_TO_SIGN_HEX_STRING = "010000000a50500d";
    private static final String CALCULATED_MAC_HEX_STRING =
            "D83B20CC6A0932B2A7CEF26E4020ABAAB64F0C6A";

    private static final long IKE_INIT_SPI = 0x5F54BF6D8B48E6E1L;
    private static final long IKE_RESP_SPI = 0x909232B3D1EDCB5CL;

    private static final String IKE_NONCE_INIT_HEX_STRING =
            "C39B7F368F4681B89FA9B7BE6465ABD7C5F68B6ED5D3B4C72CB4240EB5C46412";
    private static final String IKE_NONCE_RESP_HEX_STRING =
            "9756112CA539F5C25ABACC7EE92B73091942A9C06950F98848F1AF1694C4DDFF";

    private static final String IKE_SHARED_DH_KEY_HEX_STRING =
            "C14155DEA40056BD9C76FB4819687B7A397582F4CD5AFF4B"
                    + "8F441C56E0C08C84234147A0BA249A555835A048E3CA2980"
                    + "7D057A61DD26EEFAD9AF9C01497005E52858E29FB42EB849"
                    + "6731DF96A11CCE1F51137A9A1B900FA81AEE7898E373D4E4"
                    + "8B899BBECA091314ECD4B6E412EF4B0FEF798F54735F3180"
                    + "7424A318287F20E8";

    private static final String IKE_SKEYSEED_HEX_STRING =
            "8C42F3B1F5F81C7BAAC5F33E9A4F01987B2F9657";
    private static final String IKE_SK_D_HEX_STRING = "C86B56EFCF684DCC2877578AEF3137167FE0EBF6";
    private static final String IKE_SK_AUTH_INIT_HEX_STRING =
            "554FBF5A05B7F511E05A30CE23D874DB9EF55E51";
    private static final String IKE_SK_AUTH_RESP_HEX_STRING =
            "36D83420788337CA32ECAA46892C48808DCD58B1";
    private static final String IKE_SK_ENCR_INIT_HEX_STRING = "5CBFD33F75796C0188C4A3A546AEC4A1";
    private static final String IKE_SK_ENCR_RESP_HEX_STRING = "C33B35FCF29514CD9D8B4A695E1A816E";
    private static final String IKE_SK_PRF_INIT_HEX_STRING =
            "094787780EE466E2CB049FA327B43908BC57E485";
    private static final String IKE_SK_PRF_RESP_HEX_STRING =
            "A30E6B08BE56C0E6BFF4744143C75219299E1BEB";
    private static final String IKE_KEY_MAT =
            IKE_SK_D_HEX_STRING
                    + IKE_SK_AUTH_INIT_HEX_STRING
                    + IKE_SK_AUTH_RESP_HEX_STRING
                    + IKE_SK_ENCR_INIT_HEX_STRING
                    + IKE_SK_ENCR_RESP_HEX_STRING
                    + IKE_SK_PRF_INIT_HEX_STRING
                    + IKE_SK_PRF_RESP_HEX_STRING;

    private static final int IKE_AUTH_ALGO_KEY_LEN = 20;
    private static final int IKE_ENCR_ALGO_KEY_LEN = 16;
    private static final int IKE_PRF_KEY_LEN = 20;
    private static final int IKE_SK_D_KEY_LEN = IKE_PRF_KEY_LEN;

    private static final int FIRST_CHILD_INIT_SPI = 0x2ad4c0a2;
    private static final int FIRST_CHILD_RESP_SPI = 0xcae7019f;

    private static final String FIRST_CHILD_ENCR_INIT_HEX_STRING =
            "1B865CEA6E2C23973E8C5452ADC5CD7D";
    private static final String FIRST_CHILD_ENCR_RESP_HEX_STRING =
            "5E82FEDACC6DCB0756DDD7553907EBD1";
    private static final String FIRST_CHILD_AUTH_INIT_HEX_STRING =
            "A7A5A44F7EF4409657206C7DC52B7E692593B51E";
    private static final String FIRST_CHILD_AUTH_RESP_HEX_STRING =
            "CDE612189FD46DE870FAEC04F92B40B0BFDBD9E1";
    private static final String FIRST_CHILD_KEY_MAT =
            FIRST_CHILD_ENCR_INIT_HEX_STRING
                    + FIRST_CHILD_AUTH_INIT_HEX_STRING
                    + FIRST_CHILD_ENCR_RESP_HEX_STRING
                    + FIRST_CHILD_AUTH_RESP_HEX_STRING;

    private static final int FIRST_CHILD_AUTH_ALGO_KEY_LEN = 20;
    private static final int FIRST_CHILD_ENCR_ALGO_KEY_LEN = 16;

    private IkeMacPrf mIkeHmacSha1Prf;
    private IkeMacIntegrity mHmacSha1IntegrityMac;
    private IkeCipher mAesCbcCipher;

    private SaLifetimeAlarmScheduler mMockLifetimeAlarmScheduler;

    private SaRecordHelper mSaRecordHelper = new SaRecordHelper();

    @Before
    public void setUp() throws Exception {
        mIkeHmacSha1Prf =
                IkeMacPrf.create(new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1));
        mHmacSha1IntegrityMac =
                IkeMacIntegrity.create(
                        new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96));
        mAesCbcCipher =
                IkeCipher.create(
                        new EncryptionTransform(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_CBC,
                                SaProposal.KEY_LEN_AES_128));

        mMockLifetimeAlarmScheduler = mock(SaLifetimeAlarmScheduler.class);
    }

    // Test generating keying material for making IKE SA.
    @Test
    public void testMakeIkeSaRecord() throws Exception {
        byte[] sKeySeed = TestUtils.hexStringToByteArray(IKE_SKEYSEED_HEX_STRING);
        byte[] nonceInit = TestUtils.hexStringToByteArray(IKE_NONCE_INIT_HEX_STRING);
        byte[] nonceResp = TestUtils.hexStringToByteArray(IKE_NONCE_RESP_HEX_STRING);

        IkeSecurityParameterIndex ikeInitSpi =
                IKE_SPI_GENERATOR.allocateSpi(LOCAL_ADDRESS, IKE_INIT_SPI);
        IkeSecurityParameterIndex ikeRespSpi =
                IKE_SPI_GENERATOR.allocateSpi(REMOTE_ADDRESS, IKE_RESP_SPI);
        IkeSaRecordConfig ikeSaRecordConfig =
                new IkeSaRecordConfig(
                        ikeInitSpi,
                        ikeRespSpi,
                        mIkeHmacSha1Prf,
                        IKE_AUTH_ALGO_KEY_LEN,
                        IKE_ENCR_ALGO_KEY_LEN,
                        true /*isLocalInit*/,
                        mMockLifetimeAlarmScheduler);

        int keyMaterialLen =
                IKE_SK_D_KEY_LEN
                        + IKE_AUTH_ALGO_KEY_LEN * 2
                        + IKE_ENCR_ALGO_KEY_LEN * 2
                        + IKE_PRF_KEY_LEN * 2;

        IkeSaRecord ikeSaRecord =
                mSaRecordHelper.makeIkeSaRecord(sKeySeed, nonceInit, nonceResp, ikeSaRecordConfig);

        assertTrue(ikeSaRecord.isLocalInit);
        assertEquals(IKE_INIT_SPI, ikeSaRecord.getInitiatorSpi());
        assertEquals(IKE_RESP_SPI, ikeSaRecord.getResponderSpi());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_D_HEX_STRING), ikeSaRecord.getSkD());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_AUTH_INIT_HEX_STRING),
                ikeSaRecord.getOutboundIntegrityKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_AUTH_RESP_HEX_STRING),
                ikeSaRecord.getInboundIntegrityKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_ENCR_INIT_HEX_STRING),
                ikeSaRecord.getOutboundEncryptionKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_ENCR_RESP_HEX_STRING),
                ikeSaRecord.getInboundDecryptionKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_PRF_INIT_HEX_STRING), ikeSaRecord.getSkPi());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(IKE_SK_PRF_RESP_HEX_STRING), ikeSaRecord.getSkPr());
        verify(mMockLifetimeAlarmScheduler).scheduleLifetimeExpiryAlarm(anyString());

        ikeSaRecord.close();
        verify(mMockLifetimeAlarmScheduler).cancelLifetimeExpiryAlarm(anyString());
    }

    // Test generating keying material and building IpSecTransform for making Child SA.
    @Test
    public void testMakeChildSaRecord() throws Exception {
        byte[] sharedKey = new byte[0];
        byte[] nonceInit = TestUtils.hexStringToByteArray(IKE_NONCE_INIT_HEX_STRING);
        byte[] nonceResp = TestUtils.hexStringToByteArray(IKE_NONCE_RESP_HEX_STRING);

        MockIpSecTestUtils mockIpSecTestUtils = MockIpSecTestUtils.setUpMockIpSec();
        IpSecManager ipSecManager = mockIpSecTestUtils.getIpSecManager();
        IpSecService mockIpSecService = mockIpSecTestUtils.getIpSecService();
        Context context = mockIpSecTestUtils.getContext();

        when(mockIpSecService.allocateSecurityParameterIndex(
                        eq(LOCAL_ADDRESS.getHostAddress()), anyInt(), anyObject()))
                .thenReturn(MockIpSecTestUtils.buildDummyIpSecSpiResponse(FIRST_CHILD_INIT_SPI));
        when(mockIpSecService.allocateSecurityParameterIndex(
                        eq(REMOTE_ADDRESS.getHostAddress()), anyInt(), anyObject()))
                .thenReturn(MockIpSecTestUtils.buildDummyIpSecSpiResponse(FIRST_CHILD_RESP_SPI));

        SecurityParameterIndex childInitSpi =
                ipSecManager.allocateSecurityParameterIndex(LOCAL_ADDRESS);
        SecurityParameterIndex childRespSpi =
                ipSecManager.allocateSecurityParameterIndex(REMOTE_ADDRESS);

        byte[] initAuthKey = TestUtils.hexStringToByteArray(FIRST_CHILD_AUTH_INIT_HEX_STRING);
        byte[] respAuthKey = TestUtils.hexStringToByteArray(FIRST_CHILD_AUTH_RESP_HEX_STRING);
        byte[] initEncryptionKey = TestUtils.hexStringToByteArray(FIRST_CHILD_ENCR_INIT_HEX_STRING);
        byte[] respEncryptionKey = TestUtils.hexStringToByteArray(FIRST_CHILD_ENCR_RESP_HEX_STRING);

        IIpSecTransformHelper mockIpSecHelper;
        mockIpSecHelper = mock(IIpSecTransformHelper.class);
        SaRecord.setIpSecTransformHelper(mockIpSecHelper);

        IpSecTransform mockInTransform = mock(IpSecTransform.class);
        IpSecTransform mockOutTransform = mock(IpSecTransform.class);
        UdpEncapsulationSocket mockUdpEncapSocket = mock(UdpEncapsulationSocket.class);

        when(mockIpSecHelper.makeIpSecTransform(
                        eq(context),
                        eq(LOCAL_ADDRESS),
                        eq(mockUdpEncapSocket),
                        eq(childRespSpi),
                        eq(mHmacSha1IntegrityMac),
                        eq(mAesCbcCipher),
                        aryEq(initAuthKey),
                        aryEq(initEncryptionKey),
                        eq(false)))
                .thenReturn(mockOutTransform);

        when(mockIpSecHelper.makeIpSecTransform(
                        eq(context),
                        eq(REMOTE_ADDRESS),
                        eq(mockUdpEncapSocket),
                        eq(childInitSpi),
                        eq(mHmacSha1IntegrityMac),
                        eq(mAesCbcCipher),
                        aryEq(respAuthKey),
                        aryEq(respEncryptionKey),
                        eq(false)))
                .thenReturn(mockInTransform);

        ChildSaRecordConfig childSaRecordConfig =
                new ChildSaRecordConfig(
                        mockIpSecTestUtils.getContext(),
                        childInitSpi,
                        childRespSpi,
                        LOCAL_ADDRESS,
                        REMOTE_ADDRESS,
                        mockUdpEncapSocket,
                        mIkeHmacSha1Prf,
                        mHmacSha1IntegrityMac,
                        mAesCbcCipher,
                        TestUtils.hexStringToByteArray(IKE_SK_D_HEX_STRING),
                        false /*isTransport*/,
                        true /*isLocalInit*/,
                        mMockLifetimeAlarmScheduler);

        ChildSaRecord childSaRecord =
                mSaRecordHelper.makeChildSaRecord(
                        sharedKey, nonceInit, nonceResp, childSaRecordConfig);

        assertTrue(childSaRecord.isLocalInit);
        assertEquals(FIRST_CHILD_INIT_SPI, childSaRecord.getLocalSpi());
        assertEquals(FIRST_CHILD_RESP_SPI, childSaRecord.getRemoteSpi());
        assertEquals(mockInTransform, childSaRecord.getInboundIpSecTransform());
        assertEquals(mockOutTransform, childSaRecord.getOutboundIpSecTransform());

        assertArrayEquals(
                TestUtils.hexStringToByteArray(FIRST_CHILD_AUTH_INIT_HEX_STRING),
                childSaRecord.getOutboundIntegrityKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(FIRST_CHILD_AUTH_RESP_HEX_STRING),
                childSaRecord.getInboundIntegrityKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(FIRST_CHILD_ENCR_INIT_HEX_STRING),
                childSaRecord.getOutboundEncryptionKey());
        assertArrayEquals(
                TestUtils.hexStringToByteArray(FIRST_CHILD_ENCR_RESP_HEX_STRING),
                childSaRecord.getInboundDecryptionKey());

        verify(mMockLifetimeAlarmScheduler).scheduleLifetimeExpiryAlarm(anyString());

        childSaRecord.close();
        verify(mMockLifetimeAlarmScheduler).cancelLifetimeExpiryAlarm(anyString());

        SaRecord.setIpSecTransformHelper(new IpSecTransformHelper());
    }
}
