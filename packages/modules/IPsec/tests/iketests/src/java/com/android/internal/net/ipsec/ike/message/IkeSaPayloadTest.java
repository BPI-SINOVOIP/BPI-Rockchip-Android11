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

package com.android.internal.net.ipsec.ike.message;

import static com.android.internal.net.TestUtils.createMockRandomFactory;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.net.InetAddresses;
import android.net.IpSecManager;
import android.net.IpSecSpiResponse;
import android.net.ipsec.ike.ChildSaProposal;
import android.net.ipsec.ike.IkeSaProposal;
import android.net.ipsec.ike.SaProposal;
import android.net.ipsec.ike.exceptions.IkeProtocolException;
import android.util.Pair;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.exceptions.InvalidSyntaxException;
import com.android.internal.net.ipsec.ike.exceptions.NoValidProposalChosenException;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.Attribute;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.AttributeDecoder;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.ChildProposal;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.DhGroupTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EsnTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IkeProposal;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.KeyLengthAttribute;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.PrfTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.Proposal;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.Transform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.TransformDecoder;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.UnrecognizedAttribute;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.UnrecognizedTransform;
import com.android.internal.net.ipsec.ike.testutils.MockIpSecTestUtils;
import com.android.internal.net.ipsec.ike.utils.IkeSpiGenerator;
import com.android.internal.net.ipsec.ike.utils.IpSecSpiGenerator;
import com.android.server.IpSecService;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

public final class IkeSaPayloadTest {
    private static final String OUTBOUND_SA_PAYLOAD_HEADER = "22000030";
    private static final String OUTBOUND_PROPOSAL_RAW_PACKET =
            "0000002C010100040300000C0100000C800E0080030000080300000203000008040"
                    + "000020000000802000002";
    private static final String INBOUND_PROPOSAL_RAW_PACKET =
            "0000002c010100040300000c0100000c800e0080030000080300000203000008040"
                    + "000020000000802000002";
    private static final String INBOUND_TWO_PROPOSAL_RAW_PACKET =
            "020000dc010100190300000c0100000c800e00800300000c0100000c800e00c0030"
                    + "0000c0100000c800e01000300000801000003030000080300000c0300"
                    + "00080300000d030000080300000e03000008030000020300000803000"
                    + "005030000080200000503000008020000060300000802000007030000"
                    + "080200000403000008020000020300000804000013030000080400001"
                    + "40300000804000015030000080400001c030000080400001d03000008"
                    + "0400001e030000080400001f030000080400000f03000008040000100"
                    + "300000804000012000000080400000e000001000201001a0300000c01"
                    + "000014800e00800300000c01000014800e00c00300000c01000014800"
                    + "e01000300000c0100001c800e01000300000c01000013800e00800300"
                    + "000c01000013800e00c00300000c01000013800e01000300000c01000"
                    + "012800e00800300000c01000012800e00c00300000c01000012800e01"
                    + "000300000802000005030000080200000603000008020000070300000"
                    + "802000004030000080200000203000008040000130300000804000014"
                    + "0300000804000015030000080400001c030000080400001d030000080"
                    + "400001e030000080400001f030000080400000f030000080400001003"
                    + "00000804000012000000080400000e";
    private static final String INBOUND_CHILD_PROPOSAL_RAW_PACKET =
            "0000002801030403cae7019f0300000c0100000c800e00800300000803000002000" + "0000805000000";
    private static final String INBOUND_CHILD_TWO_PROPOSAL_RAW_PACKET =
            "0200002801030403cae7019f0300000c0100000c800e00800300000803000002000"
                    + "00008050000000000001802030401cae7019e0000000c01000012800e"
                    + "0080";
    private static final String ENCR_TRANSFORM_RAW_PACKET = "0300000c0100000c800e0080";
    private static final String PRF_TRANSFORM_RAW_PACKET = "0000000802000002";
    private static final String INTEG_TRANSFORM_RAW_PACKET = "0300000803000002";
    private static final String DH_GROUP_TRANSFORM_RAW_PACKET = "0300000804000002";
    private static final String ESN_TRANSFORM_RAW_PACKET = "0000000805000000";

    private static final int TRANSFORM_TYPE_OFFSET = 4;
    private static final int TRANSFORM_ID_OFFSET = 7;

    private static final String ATTRIBUTE_RAW_PACKET = "800e0080";

    private static final int PROPOSAL_NUMBER = 1;

    private static final int PROPOSAL_NUMBER_OFFSET = 4;
    private static final int PROTOCOL_ID_OFFSET = 5;

    // Constants for multiple proposals test
    private static final byte[] PROPOSAL_NUMBER_LIST = {1, 2};

    private static final Inet4Address LOCAL_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("8.8.4.4"));
    private static final Inet4Address REMOTE_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress("8.8.8.8"));

    private static final int DUMMY_CHILD_SPI_RESOURCE_ID_LOCAL_ONE = 0x1234;
    private static final int DUMMY_CHILD_SPI_RESOURCE_ID_LOCAL_TWO = 0x1235;
    private static final int DUMMY_CHILD_SPI_RESOURCE_ID_REMOTE = 0x2234;

    private static final int CHILD_SPI_LOCAL_ONE = 0x2ad4c0a2;
    private static final int CHILD_SPI_LOCAL_TWO = 0x2ad4c0a3;
    private static final int CHILD_SPI_REMOTE = 0xcae70197;

    private AttributeDecoder mMockedAttributeDecoder;
    private KeyLengthAttribute mAttributeKeyLength128;
    private List<Attribute> mAttributeListWithKeyLength128;

    private EncryptionTransform mEncrAesCbc128Transform;
    private EncryptionTransform mEncrAesGcm8Key128Transform;
    private IntegrityTransform mIntegHmacSha1Transform;
    private PrfTransform mPrfHmacSha1Transform;
    private DhGroupTransform mDhGroup1024Transform;

    private Transform[] mValidNegotiatedTransformSet;

    private IkeSaProposal mIkeSaProposalOne;
    private IkeSaProposal mIkeSaProposalTwo;
    private IkeSaProposal[] mTwoIkeSaProposalsArray;

    private ChildSaProposal mChildSaProposalOne;
    private ChildSaProposal mChildSaProposalTwo;
    private ChildSaProposal[] mTwoChildSaProposalsArray;

    private MockIpSecTestUtils mMockIpSecTestUtils;
    private IpSecService mMockIpSecService;
    private IpSecManager mIpSecManager;

    private IkeSpiGenerator mIkeSpiGenerator;
    private IpSecSpiGenerator mIpSecSpiGenerator;

    private IpSecSpiResponse mDummyIpSecSpiResponseLocalOne;
    private IpSecSpiResponse mDummyIpSecSpiResponseLocalTwo;
    private IpSecSpiResponse mDummyIpSecSpiResponseRemote;

    @Before
    public void setUp() throws Exception {
        mMockedAttributeDecoder = mock(AttributeDecoder.class);
        Transform.setAttributeDecoder(mMockedAttributeDecoder);

        mAttributeKeyLength128 = new KeyLengthAttribute(SaProposal.KEY_LEN_AES_128);
        mAttributeListWithKeyLength128 = new LinkedList<>();
        mAttributeListWithKeyLength128.add(mAttributeKeyLength128);

        mEncrAesCbc128Transform =
                new EncryptionTransform(
                        SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, SaProposal.KEY_LEN_AES_128);
        mEncrAesGcm8Key128Transform =
                new EncryptionTransform(
                        SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8, SaProposal.KEY_LEN_AES_128);
        mIntegHmacSha1Transform =
                new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96);
        mPrfHmacSha1Transform = new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1);
        mDhGroup1024Transform = new DhGroupTransform(SaProposal.DH_GROUP_1024_BIT_MODP);

        mValidNegotiatedTransformSet =
                new Transform[] {
                    mEncrAesCbc128Transform,
                    mIntegHmacSha1Transform,
                    mPrfHmacSha1Transform,
                    mDhGroup1024Transform
                };

        mIkeSaProposalOne =
                new IkeSaProposal.Builder()
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, SaProposal.KEY_LEN_AES_128)
                        .addIntegrityAlgorithm(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96)
                        .addDhGroup(SaProposal.DH_GROUP_1024_BIT_MODP)
                        .addPseudorandomFunction(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1)
                        .build();

        mIkeSaProposalTwo =
                new IkeSaProposal.Builder()
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8,
                                SaProposal.KEY_LEN_AES_128)
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_12,
                                SaProposal.KEY_LEN_AES_128)
                        .addPseudorandomFunction(SaProposal.PSEUDORANDOM_FUNCTION_AES128_XCBC)
                        .addDhGroup(SaProposal.DH_GROUP_1024_BIT_MODP)
                        .addDhGroup(SaProposal.DH_GROUP_2048_BIT_MODP)
                        .build();
        mTwoIkeSaProposalsArray = new IkeSaProposal[] {mIkeSaProposalOne, mIkeSaProposalTwo};

        mChildSaProposalOne =
                new ChildSaProposal.Builder()
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, SaProposal.KEY_LEN_AES_128)
                        .addIntegrityAlgorithm(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96)
                        .build();
        mChildSaProposalTwo =
                new ChildSaProposal.Builder()
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8,
                                SaProposal.KEY_LEN_AES_128)
                        .build();
        mTwoChildSaProposalsArray =
                new ChildSaProposal[] {mChildSaProposalOne, mChildSaProposalTwo};

        mMockIpSecTestUtils = MockIpSecTestUtils.setUpMockIpSec();
        mIpSecManager = mMockIpSecTestUtils.getIpSecManager();

        IpSecService mMockIpSecService = mMockIpSecTestUtils.getIpSecService();
        when(mMockIpSecService.allocateSecurityParameterIndex(
                        eq(LOCAL_ADDRESS.getHostAddress()), anyInt(), anyObject()))
                .thenReturn(MockIpSecTestUtils.buildDummyIpSecSpiResponse(CHILD_SPI_LOCAL_ONE))
                .thenReturn(MockIpSecTestUtils.buildDummyIpSecSpiResponse(CHILD_SPI_LOCAL_TWO));

        when(mMockIpSecService.allocateSecurityParameterIndex(
                        eq(REMOTE_ADDRESS.getHostAddress()), anyInt(), anyObject()))
                .thenReturn(MockIpSecTestUtils.buildDummyIpSecSpiResponse(CHILD_SPI_REMOTE));

        mIkeSpiGenerator = new IkeSpiGenerator(createMockRandomFactory());
        mIpSecSpiGenerator = new IpSecSpiGenerator(mIpSecManager, createMockRandomFactory());
    }

    @After
    public void tearDown() throws Exception {
        Proposal.resetTransformDecoder();
        Transform.resetAttributeDecoder();
    }

    @Test
    public void testDecodeAttribute() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ATTRIBUTE_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        Pair<Attribute, Integer> pair = Attribute.readFrom(inputBuffer);
        Attribute attribute = pair.first;

        assertTrue(attribute instanceof KeyLengthAttribute);
        assertEquals(Attribute.ATTRIBUTE_TYPE_KEY_LENGTH, attribute.type);
        assertEquals(SaProposal.KEY_LEN_AES_128, ((KeyLengthAttribute) attribute).keyLength);
    }

    @Test
    public void testEncodeAttribute() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(mAttributeKeyLength128.getAttributeLength());
        mAttributeKeyLength128.encodeToByteBuffer(byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(ATTRIBUTE_RAW_PACKET);

        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testDecodeEncryptionTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(mAttributeListWithKeyLength128)
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof EncryptionTransform);
        assertEquals(Transform.TRANSFORM_TYPE_ENCR, transform.type);
        assertEquals(SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, transform.id);
        assertTrue(transform.isSupported);
    }

    @Test
    public void testDecodeEncryptionTransformWithInvalidKeyLength() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        List<Attribute> attributeList = new LinkedList<>();
        Attribute keyLengAttr = new KeyLengthAttribute(SaProposal.KEY_LEN_AES_128 + 1);
        attributeList.add(keyLengAttr);

        doReturn(attributeList).when(mMockedAttributeDecoder).decodeAttributes(anyInt(), any());

        try {
            Transform.readFrom(inputBuffer);
            fail("Expected InvalidSyntaxException for invalid key length.");
        } catch (InvalidSyntaxException expected) {
        }
    }

    @Test
    public void testEncodeEncryptionTransform() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(mEncrAesCbc128Transform.getTransformLength());
        mEncrAesCbc128Transform.encodeToByteBuffer(false, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);

        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testConstructEncryptionTransformWithUnsupportedId() throws Exception {
        try {
            new EncryptionTransform(-1);
            fail("Expected IllegalArgumentException for unsupported Transform ID");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testConstructEncryptionTransformWithInvalidKeyLength() throws Exception {
        try {
            new EncryptionTransform(SaProposal.ENCRYPTION_ALGORITHM_3DES, 129);
            fail("Expected IllegalArgumentException for invalid key length.");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testDecodePrfTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(PRF_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(new LinkedList<Attribute>())
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof PrfTransform);
        assertEquals(Transform.TRANSFORM_TYPE_PRF, transform.type);
        assertEquals(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1, transform.id);
        assertTrue(transform.isSupported);
    }

    @Test
    public void testEncodePrfTransform() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(mPrfHmacSha1Transform.getTransformLength());
        mPrfHmacSha1Transform.encodeToByteBuffer(true, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(PRF_TRANSFORM_RAW_PACKET);

        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testConstructPrfTransformWithUnsupportedId() throws Exception {
        try {
            new PrfTransform(-1);
            fail("Expected IllegalArgumentException for unsupported Transform ID");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testDecodeIntegrityTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INTEG_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(new LinkedList<Attribute>())
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof IntegrityTransform);
        assertEquals(Transform.TRANSFORM_TYPE_INTEG, transform.type);
        assertEquals(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96, transform.id);
        assertTrue(transform.isSupported);
    }

    @Test
    public void testDecodeIntegrityTransformWithUnrecognizedAttribute() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INTEG_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(mAttributeListWithKeyLength128)
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof IntegrityTransform);
        assertEquals(Transform.TRANSFORM_TYPE_INTEG, transform.type);
        assertEquals(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96, transform.id);
        assertFalse(transform.isSupported);
    }

    @Test
    public void testEncodeIntegrityTransform() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(mIntegHmacSha1Transform.getTransformLength());
        mIntegHmacSha1Transform.encodeToByteBuffer(false, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(INTEG_TRANSFORM_RAW_PACKET);

        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testConstructIntegrityTransformWithUnsupportedId() throws Exception {
        try {
            new IntegrityTransform(-1);
            fail("Expected IllegalArgumentException for unsupported Transform ID");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testDecodeDhGroupTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(DH_GROUP_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(new LinkedList<Attribute>())
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof DhGroupTransform);
        assertEquals(Transform.TRANSFORM_TYPE_DH, transform.type);
        assertEquals(SaProposal.DH_GROUP_1024_BIT_MODP, transform.id);
        assertTrue(transform.isSupported);
    }

    @Test
    public void testEncodeDhGroupTransform() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.allocate(mDhGroup1024Transform.getTransformLength());
        mDhGroup1024Transform.encodeToByteBuffer(false, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(DH_GROUP_TRANSFORM_RAW_PACKET);

        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testConstructDhGroupTransformWithUnsupportedId() throws Exception {
        try {
            new DhGroupTransform(-1);
            fail("Expected IllegalArgumentException for unsupported Transform ID");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testDecodeEsnTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ESN_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(new LinkedList<Attribute>())
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof EsnTransform);
        assertEquals(Transform.TRANSFORM_TYPE_ESN, transform.type);
        assertEquals(EsnTransform.ESN_POLICY_NO_EXTENDED, transform.id);
        assertTrue(transform.isSupported);
    }

    @Test
    public void testDecodeEsnTransformWithUnsupportedId() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ESN_TRANSFORM_RAW_PACKET);
        inputPacket[TRANSFORM_ID_OFFSET] = -1;
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(new LinkedList<Attribute>())
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof EsnTransform);
        assertEquals(Transform.TRANSFORM_TYPE_ESN, transform.type);
        assertFalse(transform.isSupported);
    }

    @Test
    public void testDecodeEsnTransformWithUnrecognizedAttribute() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ESN_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(mAttributeListWithKeyLength128)
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);
        assertTrue(transform instanceof EsnTransform);
        assertEquals(Transform.TRANSFORM_TYPE_ESN, transform.type);
        assertEquals(EsnTransform.ESN_POLICY_NO_EXTENDED, transform.id);
        assertFalse(transform.isSupported);
    }

    @Test
    public void testEncodeEsnTransform() throws Exception {
        EsnTransform mEsnTransform = new EsnTransform();
        ByteBuffer byteBuffer = ByteBuffer.allocate(mEsnTransform.getTransformLength());
        mEsnTransform.encodeToByteBuffer(true, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(ESN_TRANSFORM_RAW_PACKET);

        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testDecodeUnrecognizedTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);
        inputPacket[TRANSFORM_TYPE_OFFSET] = 6;
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(mAttributeListWithKeyLength128)
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);

        assertTrue(transform instanceof UnrecognizedTransform);
    }

    @Test
    public void testDecodeTransformWithRepeatedAttribute() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        List<Attribute> attributeList = new LinkedList<>();
        attributeList.add(mAttributeKeyLength128);
        attributeList.add(mAttributeKeyLength128);

        doReturn(attributeList).when(mMockedAttributeDecoder).decodeAttributes(anyInt(), any());

        try {
            Transform.readFrom(inputBuffer);
            fail("Expected InvalidSyntaxException for repeated Attribute Type Key Length.");
        } catch (InvalidSyntaxException expected) {
        }
    }

    @Test
    public void testDecodeTransformWithUnrecognizedTransformId() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);
        inputPacket[TRANSFORM_ID_OFFSET] = 1;
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        doReturn(mAttributeListWithKeyLength128)
                .when(mMockedAttributeDecoder)
                .decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);

        assertFalse(transform.isSupported);
    }

    @Test
    public void testDecodeTransformWithUnrecogniedAttributeType() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ENCR_TRANSFORM_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);

        List<Attribute> attributeList = new LinkedList<>();
        attributeList.add(mAttributeKeyLength128);
        Attribute attributeUnrecognized = new UnrecognizedAttribute(1, new byte[0]);
        attributeList.add(attributeUnrecognized);

        doReturn(attributeList).when(mMockedAttributeDecoder).decodeAttributes(anyInt(), any());

        Transform transform = Transform.readFrom(inputBuffer);

        assertFalse(transform.isSupported);
    }

    @Test
    public void testTransformEquals() throws Exception {
        EncryptionTransform mEncrAesGcm8Key128TransformOther =
                new EncryptionTransform(
                        SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8, SaProposal.KEY_LEN_AES_128);

        assertEquals(mEncrAesGcm8Key128Transform, mEncrAesGcm8Key128TransformOther);

        EncryptionTransform mEncrAesGcm8Key192Transform =
                new EncryptionTransform(
                        SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8, SaProposal.KEY_LEN_AES_192);

        assertNotEquals(mEncrAesGcm8Key128Transform, mEncrAesGcm8Key192Transform);

        IntegrityTransform mIntegHmacSha1TransformOther =
                new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96);

        assertNotEquals(mEncrAesGcm8Key128Transform, mIntegHmacSha1Transform);
        assertEquals(mIntegHmacSha1Transform, mIntegHmacSha1TransformOther);
    }

    private TransformDecoder createTestTransformDecoder(Transform[] decodedTransforms) {
        return new TransformDecoder() {
            @Override
            public Transform[] decodeTransforms(int count, ByteBuffer inputBuffer)
                    throws IkeProtocolException {
                for (int i = 0; i < count; i++) {
                    // Read length field and move position
                    inputBuffer.getShort();
                    int length = Short.toUnsignedInt(inputBuffer.getShort());
                    byte[] temp = new byte[length - 4];
                    inputBuffer.get(temp);
                }
                return decodedTransforms;
            }
        };
    }

    @Test
    public void testDecodeSingleProposal() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);
        ByteBuffer inputBuffer = ByteBuffer.wrap(inputPacket);
        Proposal.setTransformDecoder(createTestTransformDecoder(new Transform[0]));

        Proposal proposal = Proposal.readFrom(inputBuffer);

        assertEquals(PROPOSAL_NUMBER, proposal.number);
        assertEquals(IkePayload.PROTOCOL_ID_IKE, proposal.protocolId);
        assertEquals(IkePayload.SPI_LEN_NOT_INCLUDED, proposal.spiSize);
        assertEquals(IkePayload.SPI_NOT_INCLUDED, proposal.spi);
        assertFalse(proposal.hasUnrecognizedTransform);
        assertNotNull(proposal.getSaProposal());
    }

    @Test
    public void testDecodeSaRequestWithMultipleProposal() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_TWO_PROPOSAL_RAW_PACKET);
        Proposal.setTransformDecoder(createTestTransformDecoder(new Transform[0]));

        IkeSaPayload payload = new IkeSaPayload(false, false, inputPacket);

        assertEquals(PROPOSAL_NUMBER_LIST.length, payload.proposalList.size());
        for (int i = 0; i < payload.proposalList.size(); i++) {
            Proposal proposal = payload.proposalList.get(i);
            assertEquals(PROPOSAL_NUMBER_LIST[i], proposal.number);
            assertEquals(IkePayload.PROTOCOL_ID_IKE, proposal.protocolId);
            assertEquals(0, proposal.spiSize);
        }
    }

    @Test
    public void testEncodeProposal() throws Exception {
        // Construct Proposal for IKE INIT exchange.
        IkeProposal proposal =
                IkeProposal.createIkeProposal(
                        (byte) PROPOSAL_NUMBER,
                        IkePayload.SPI_LEN_NOT_INCLUDED,
                        mIkeSaProposalOne,
                        mIkeSpiGenerator,
                        LOCAL_ADDRESS);

        ByteBuffer byteBuffer = ByteBuffer.allocate(proposal.getProposalLength());
        proposal.encodeToByteBuffer(true /*is the last*/, byteBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(OUTBOUND_PROPOSAL_RAW_PACKET);
        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    @Test
    public void testDecodeSaResponseWithMultipleProposal() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_TWO_PROPOSAL_RAW_PACKET);
        Proposal.setTransformDecoder(createTestTransformDecoder(new Transform[0]));

        try {
            new IkeSaPayload(false, true, inputPacket);
            fail("Expected to fail due to more than one proposal in response SA payload.");
        } catch (InvalidSyntaxException expected) {

        }
    }

    @Test
    public void testBuildOutboundIkeRekeySaResponsePayload() throws Exception {
        IkeSaPayload saPayload =
                IkeSaPayload.createRekeyIkeSaResponsePayload(
                        (byte) 1, mIkeSaProposalOne, mIkeSpiGenerator, LOCAL_ADDRESS);

        assertTrue(saPayload.isSaResponse);
        assertEquals(1, saPayload.proposalList.size());

        IkeProposal proposal = (IkeProposal) saPayload.proposalList.get(0);
        assertEquals(IkePayload.PROTOCOL_ID_IKE, proposal.protocolId);
        assertEquals(IkePayload.SPI_LEN_IKE, proposal.spiSize);
        assertEquals(mIkeSaProposalOne, proposal.saProposal);

        assertNotNull(proposal.getIkeSpiResource());
    }

    @Test
    public void testBuildOutboundInitialIkeSaRequestPayload() throws Exception {
        IkeSaPayload saPayload = IkeSaPayload.createInitialIkeSaPayload(mTwoIkeSaProposalsArray);

        assertFalse(saPayload.isSaResponse);
        assertEquals(PROPOSAL_NUMBER_LIST.length, saPayload.proposalList.size());

        for (int i = 0; i < saPayload.proposalList.size(); i++) {
            IkeProposal proposal = (IkeProposal) saPayload.proposalList.get(i);
            assertEquals(PROPOSAL_NUMBER_LIST[i], proposal.number);
            assertEquals(IkePayload.PROTOCOL_ID_IKE, proposal.protocolId);
            assertEquals(IkePayload.SPI_LEN_NOT_INCLUDED, proposal.spiSize);
            assertEquals(mTwoIkeSaProposalsArray[i], proposal.saProposal);

            // SA Payload for IKE INIT exchange does not include IKE SPIs.
            assertNull(proposal.getIkeSpiResource());
        }
    }

    @Test
    public void testBuildOutboundChildSaRequest() throws Exception {
        IkeSaPayload saPayload =
                IkeSaPayload.createChildSaRequestPayload(
                        mTwoChildSaProposalsArray, mIpSecSpiGenerator, LOCAL_ADDRESS);

        assertFalse(saPayload.isSaResponse);
        assertEquals(PROPOSAL_NUMBER_LIST.length, saPayload.proposalList.size());

        int[] expectedSpis = new int[] {CHILD_SPI_LOCAL_ONE, CHILD_SPI_LOCAL_TWO};
        for (int i = 0; i < saPayload.proposalList.size(); i++) {
            ChildProposal proposal = (ChildProposal) saPayload.proposalList.get(i);
            assertEquals(PROPOSAL_NUMBER_LIST[i], proposal.number);
            assertEquals(IkePayload.PROTOCOL_ID_ESP, proposal.protocolId);
            assertEquals(IkePayload.SPI_LEN_IPSEC, proposal.spiSize);
            assertEquals(mTwoChildSaProposalsArray[i], proposal.saProposal);

            assertEquals(expectedSpis[i], proposal.getChildSpiResource().getSpi());
        }
    }

    @Test
    public void testEncodeIkeSaPayload() throws Exception {
        IkeSaPayload saPayload =
                IkeSaPayload.createInitialIkeSaPayload(new IkeSaProposal[] {mIkeSaProposalOne});

        ByteBuffer byteBuffer = ByteBuffer.allocate(saPayload.getPayloadLength());
        saPayload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_KE, byteBuffer);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(
                        OUTBOUND_SA_PAYLOAD_HEADER + OUTBOUND_PROPOSAL_RAW_PACKET);
        assertArrayEquals(expectedBytes, byteBuffer.array());
    }

    private void buildAndVerifyIkeSaRespProposal(
            byte[] saResponseBytes, Transform[] decodedTransforms) throws Exception {
        // Build response SA payload from decoding bytes.
        Proposal.setTransformDecoder(createTestTransformDecoder(decodedTransforms));
        IkeSaPayload respPayload = new IkeSaPayload(false, true, saResponseBytes);

        // Build request SA payload for IKE INIT exchange from SaProposal.
        IkeSaPayload reqPayload = IkeSaPayload.createInitialIkeSaPayload(mTwoIkeSaProposalsArray);

        Pair<IkeProposal, IkeProposal> negotiatedProposalPair =
                IkeSaPayload.getVerifiedNegotiatedIkeProposalPair(
                        reqPayload, respPayload, mIkeSpiGenerator, REMOTE_ADDRESS);
        IkeProposal reqProposal = negotiatedProposalPair.first;
        IkeProposal respProposal = negotiatedProposalPair.second;

        assertEquals(respPayload.proposalList.get(0).getSaProposal(), respProposal.getSaProposal());

        // SA Payload for IKE INIT exchange does not include IKE SPIs.
        assertNull(reqProposal.getIkeSpiResource());
        assertNull(respProposal.getIkeSpiResource());
    }

    @Test
    public void testGetVerifiedNegotiatedIkeProposal() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);

        buildAndVerifyIkeSaRespProposal(inputPacket, mValidNegotiatedTransformSet);
    }

    private void verifyChildSaNegotiation(
            IkeSaPayload reqPayload,
            IkeSaPayload respPayload,
            IpSecSpiGenerator ipSecSpiGenerator,
            InetAddress remoteAddress,
            boolean isLocalInit)
            throws Exception {
        // SA negotiation
        Pair<ChildProposal, ChildProposal> negotiatedProposalPair =
                IkeSaPayload.getVerifiedNegotiatedChildProposalPair(
                        reqPayload, respPayload, ipSecSpiGenerator, remoteAddress);
        ChildProposal reqProposal = negotiatedProposalPair.first;
        ChildProposal respProposal = negotiatedProposalPair.second;

        // Verify results
        assertEquals(respPayload.proposalList.get(0).getSaProposal(), respProposal.getSaProposal());

        int initSpi = isLocalInit ? CHILD_SPI_LOCAL_ONE : CHILD_SPI_REMOTE;
        int respSpi = isLocalInit ? CHILD_SPI_REMOTE : CHILD_SPI_LOCAL_ONE;
        assertEquals(initSpi, reqProposal.getChildSpiResource().getSpi());
        assertEquals(respSpi, respProposal.getChildSpiResource().getSpi());

        // Verify SPIs in unselected Proposals have been released.
        for (Proposal proposal : reqPayload.proposalList) {
            if (proposal != reqProposal) {
                assertNull(((ChildProposal) proposal).getChildSpiResource());
            }
        }
    }

    @Test
    public void testGetVerifiedNegotiatedChildProposalForLocalCreate() throws Exception {
        // Build local request
        IkeSaPayload reqPayload =
                IkeSaPayload.createChildSaRequestPayload(
                        mTwoChildSaProposalsArray, mIpSecSpiGenerator, LOCAL_ADDRESS);

        // Build remote response
        Proposal.setTransformDecoder(
                createTestTransformDecoder(mChildSaProposalOne.getAllTransforms()));
        IkeSaPayload respPayload =
                new IkeSaPayload(
                        false /*critical*/,
                        true /*isResp*/,
                        TestUtils.hexStringToByteArray(INBOUND_CHILD_PROPOSAL_RAW_PACKET));

        verifyChildSaNegotiation(
                reqPayload, respPayload, mIpSecSpiGenerator, REMOTE_ADDRESS, true /*isLocalInit*/);
    }

    @Test
    public void testGetVerifiedNegotiatedChildProposalForRemoteCreate() throws Exception {
        Transform[] transformsOne = mChildSaProposalOne.getAllTransforms();
        Transform[] transformsTwo = mChildSaProposalTwo.getAllTransforms();
        Transform[] decodedTransforms = new Transform[transformsOne.length + transformsTwo.length];
        System.arraycopy(transformsOne, 0, decodedTransforms, 0, transformsOne.length);
        System.arraycopy(
                transformsTwo, 0, decodedTransforms, transformsOne.length, transformsTwo.length);

        // Build remote request
        Proposal.setTransformDecoder(createTestTransformDecoder(decodedTransforms));
        IkeSaPayload reqPayload =
                new IkeSaPayload(
                        false /*critical*/,
                        false /*isResp*/,
                        TestUtils.hexStringToByteArray(INBOUND_CHILD_TWO_PROPOSAL_RAW_PACKET));

        // Build local response
        IkeSaPayload respPayload =
                IkeSaPayload.createChildSaResponsePayload(
                        (byte) 1, mChildSaProposalOne, mIpSecSpiGenerator, LOCAL_ADDRESS);

        verifyChildSaNegotiation(
                reqPayload, respPayload, mIpSecSpiGenerator, REMOTE_ADDRESS, false /*isLocalInit*/);
    }

    // Test throwing when negotiated proposal in SA response payload has unrecognized Transform.
    @Test
    public void testGetVerifiedNegotiatedProposalWithUnrecogTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);

        Transform[] negotiatedTransformSet =
                Arrays.copyOfRange(
                        mValidNegotiatedTransformSet, 0, mValidNegotiatedTransformSet.length);
        negotiatedTransformSet[0] = new UnrecognizedTransform(-1, 1, new LinkedList<>());

        try {
            buildAndVerifyIkeSaRespProposal(inputPacket, negotiatedTransformSet);
            fail("Expected to fail because negotiated proposal has unrecognized Transform.");
        } catch (NoValidProposalChosenException expected) {
        }
    }

    // Test throwing when negotiated proposal has invalid proposal number.
    @Test
    public void testGetVerifiedNegotiatedProposalWithInvalidNumber() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);
        inputPacket[PROPOSAL_NUMBER_OFFSET] = (byte) 10;

        try {
            buildAndVerifyIkeSaRespProposal(inputPacket, mValidNegotiatedTransformSet);
            fail("Expected to fail due to invalid proposal number.");
        } catch (NoValidProposalChosenException expected) {
        }
    }

    // Test throwing when negotiated proposal has mismatched protocol ID.
    @Test
    public void testGetVerifiedNegotiatedProposalWithMisMatchedProtocol() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);
        inputPacket[PROTOCOL_ID_OFFSET] = IkePayload.PROTOCOL_ID_ESP;

        try {
            buildAndVerifyIkeSaRespProposal(inputPacket, mValidNegotiatedTransformSet);
            fail("Expected to fail due to mismatched protocol ID.");
        } catch (NoValidProposalChosenException expected) {
        }
    }

    // Test throwing when negotiated proposal has Transform that was not proposed in request.
    @Test
    public void testGetVerifiedNegotiatedProposalWithMismatchedTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);

        Transform[] negotiatedTransformSet =
                Arrays.copyOfRange(
                        mValidNegotiatedTransformSet, 0, mValidNegotiatedTransformSet.length);
        negotiatedTransformSet[0] = mEncrAesGcm8Key128Transform;

        try {
            buildAndVerifyIkeSaRespProposal(inputPacket, negotiatedTransformSet);
            fail("Expected to fail due to mismatched Transform.");
        } catch (NoValidProposalChosenException expected) {
        }
    }

    // Test throwing when negotiated proposal is lack of a certain type Transform.
    @Test
    public void testGetVerifiedNegotiatedProposalWithoutTransform() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(INBOUND_PROPOSAL_RAW_PACKET);

        try {
            buildAndVerifyIkeSaRespProposal(inputPacket, new Transform[0]);
            fail("Expected to fail due to absence of Transform.");
        } catch (NoValidProposalChosenException expected) {
        }
    }

    @Test
    public void testDecodeSaPayloadWithUnsupportedTransformId() throws Exception {
        Proposal.resetTransformDecoder();
        Transform.resetAttributeDecoder();

        final String saPayloadBodyHex =
                "0000002c010100040300000c0100000c800e0080030000080300000c"
                        + "0300000802000005000000080400001f";
        IkeSaPayload saPayload =
                new IkeSaPayload(
                        false /* isCritical*/,
                        true /* isResp */,
                        TestUtils.hexStringToByteArray(saPayloadBodyHex));
        IkeProposal proposal = (IkeProposal) saPayload.proposalList.get(0);
        DhGroupTransform unsupportedDh = proposal.saProposal.getDhGroupTransforms()[0];
        assertFalse(unsupportedDh.isSupported);
    }
}
