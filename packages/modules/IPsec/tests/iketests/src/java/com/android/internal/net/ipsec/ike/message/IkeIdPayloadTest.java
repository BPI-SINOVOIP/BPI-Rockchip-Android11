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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.net.InetAddresses;
import android.net.ipsec.ike.IkeDerAsn1DnIdentification;
import android.net.ipsec.ike.IkeFqdnIdentification;
import android.net.ipsec.ike.IkeIdentification;
import android.net.ipsec.ike.IkeIpv4AddrIdentification;
import android.net.ipsec.ike.IkeIpv6AddrIdentification;
import android.net.ipsec.ike.IkeKeyIdIdentification;
import android.net.ipsec.ike.IkeRfc822AddrIdentification;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.exceptions.AuthenticationFailedException;
import com.android.internal.net.ipsec.ike.testutils.CertUtils;

import org.junit.BeforeClass;
import org.junit.Test;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.nio.ByteBuffer;
import java.security.cert.X509Certificate;

import javax.security.auth.x500.X500Principal;

public final class IkeIdPayloadTest {

    private static final String IPV4_ADDR_ID_PAYLOAD_RESPONDER_HEX_STRING =
            "2700000c01000000c0000264";
    private static final String IPV4_ADDR_ID_PAYLOAD_RESPONDER_BODY_HEX_STRING = "01000000c0000264";
    private static final String IPV4_ADDR_STRING = "192.0.2.100";

    private static final String IPV6_ADDR_ID_PAYLOAD_RESPONDER_HEX_STRING =
            "27000018050000000000200100000db80000000000000001";
    private static final String IPV6_ADDR_ID_PAYLOAD_RESPONDER_BODY_HEX_STRING =
            "050000000000200100000db80000000000000001";
    private static final String IPV6_ADDR_STRING = "0:2001:0:db8::1";

    private static final String FQDN_ID_PAYLOAD_HEX_STRING =
            "2500001702000000696B652E616E64726F69642E6E6574";
    private static final String FQDN_ID_PAYLOAD_BODY_HEX_STRING =
            "02000000696B652E616E64726F69642E6E6574";
    private static final String FQDN = "ike.android.net";

    private static final String RFC822_ADDR_ID_PAYLOAD_HEX_STRING =
            "2500001e03000000616e64726f6964696b65406578616d706c652e636f6d";
    private static final String RFC822_ADDR_ID_PAYLOAD_BODY_HEX_STRING =
            "03000000616e64726f6964696b65406578616d706c652e636f6d";
    private static final String RFC822_NAME = "androidike@example.com";

    private static final String KEY_ID_PAYLOAD_HEX_STRING =
            "250000170b000000616E64726F6964496B654B65794964";
    private static final String KEY_ID_PAYLOAD_BODY_HEX_STRING =
            "0b000000616E64726F6964496B654B65794964";
    private static final byte[] KEY_ID = "androidIkeKeyId".getBytes();

    private static final String ASN1_DN_PAYLOAD_HEX_STRING =
            "25000051090000003047310b30090603550406130255533110300e060355"
                    + "040a1307416e64726f6964312630240603550403131d736d616c6c2e7365"
                    + "727665722e746573742e616e64726f69642e6e6574";
    private static final String ASN1_DN_PAYLOAD_BODY_HEX_STRING =
            "090000003047310b30090603550406130255533110300e060355040a1307"
                    + "416e64726f6964312630240603550403131d736d616c6c2e736572766572"
                    + "2e746573742e616e64726f69642e6e6574";
    private static final String ASN1_DN_INVALID_PAYLOAD_BODY_HEX_STRING = "090000001010101010";
    private static final String ASN1_DN_STRING =
            "CN=small.server.test.android.net, O=Android, C=US";

    private static final int ID_TYPE_OFFSET = 0;

    private static X509Certificate sEndCertWithSanDns;
    private static X509Certificate sEndCertWithSanIp;

    private static final String CERT_SAN_DNS = "server.test.android.net";
    private static final Inet4Address CERT_SAN_IP =
            (Inet4Address) InetAddresses.parseNumericAddress("192.168.43.138");

    @BeforeClass
    public static void setUpBeforeClass() throws Exception {
        sEndCertWithSanDns = CertUtils.createCertFromPemFile("end-cert-a.pem");
        sEndCertWithSanIp = CertUtils.createCertFromPemFile("end-cert-small.pem");
    }

    @Test
    public void testDecodeIpv4AddrIdPayload() throws Exception {
        byte[] inputPacket =
                TestUtils.hexStringToByteArray(IPV4_ADDR_ID_PAYLOAD_RESPONDER_BODY_HEX_STRING);
        IkeIdPayload payload = new IkeIdPayload(false, inputPacket, false);

        assertEquals(IkePayload.PAYLOAD_TYPE_ID_RESPONDER, payload.payloadType);
        assertEquals(IkeIdentification.ID_TYPE_IPV4_ADDR, payload.ikeId.idType);
        IkeIpv4AddrIdentification ikeId = (IkeIpv4AddrIdentification) payload.ikeId;
        Inet4Address expectedAddr = (Inet4Address) Inet4Address.getByName(IPV4_ADDR_STRING);
        assertEquals(expectedAddr, ikeId.ipv4Address);
    }

    @Test
    public void testDecodeIpv6AddrIdPayload() throws Exception {
        byte[] inputPacket =
                TestUtils.hexStringToByteArray(IPV6_ADDR_ID_PAYLOAD_RESPONDER_BODY_HEX_STRING);
        IkeIdPayload payload = new IkeIdPayload(false, inputPacket, false);

        assertEquals(IkePayload.PAYLOAD_TYPE_ID_RESPONDER, payload.payloadType);
        assertEquals(IkeIdentification.ID_TYPE_IPV6_ADDR, payload.ikeId.idType);
        IkeIpv6AddrIdentification ikeId = (IkeIpv6AddrIdentification) payload.ikeId;
        Inet6Address expectedAddr = (Inet6Address) Inet6Address.getByName(IPV6_ADDR_STRING);
        assertEquals(expectedAddr, ikeId.ipv6Address);
    }

    @Test
    public void testDecodeFqdnIdPayload() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(FQDN_ID_PAYLOAD_BODY_HEX_STRING);
        IkeIdPayload payload =
                new IkeIdPayload(false /*critical*/, inputPacket, false /*isInitiator*/);

        assertEquals(IkePayload.PAYLOAD_TYPE_ID_RESPONDER, payload.payloadType);
        assertArrayEquals(inputPacket, payload.getEncodedPayloadBody());
        assertEquals(IkeIdentification.ID_TYPE_FQDN, payload.ikeId.idType);
        IkeFqdnIdentification ikeId = (IkeFqdnIdentification) payload.ikeId;
        assertEquals(FQDN, ikeId.fqdn);
    }

    @Test
    public void testDecodeRfc822AddrIdPayload() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(RFC822_ADDR_ID_PAYLOAD_BODY_HEX_STRING);
        IkeIdPayload payload =
                new IkeIdPayload(false /*critical*/, inputPacket, true /*isInitiator*/);

        assertEquals(IkePayload.PAYLOAD_TYPE_ID_INITIATOR, payload.payloadType);
        assertEquals(IkeIdentification.ID_TYPE_RFC822_ADDR, payload.ikeId.idType);
        IkeRfc822AddrIdentification ikeId = (IkeRfc822AddrIdentification) payload.ikeId;
        assertEquals(RFC822_NAME, ikeId.rfc822Name);
    }

    @Test
    public void testDecodeKeyIdPayload() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(KEY_ID_PAYLOAD_BODY_HEX_STRING);
        IkeIdPayload payload =
                new IkeIdPayload(false /*critical*/, inputPacket, true /*isInitiator*/);

        assertEquals(IkePayload.PAYLOAD_TYPE_ID_INITIATOR, payload.payloadType);
        assertEquals(IkeIdentification.ID_TYPE_KEY_ID, payload.ikeId.idType);
        IkeKeyIdIdentification ikeId = (IkeKeyIdIdentification) payload.ikeId;
        assertArrayEquals(KEY_ID, ikeId.keyId);
    }

    @Test
    public void testDecodeDerAsn1DnIdPayload() throws Exception {
        byte[] inputPacket = TestUtils.hexStringToByteArray(ASN1_DN_PAYLOAD_BODY_HEX_STRING);
        IkeIdPayload payload =
                new IkeIdPayload(false /*critical*/, inputPacket, true /*isInitiator*/);

        assertEquals(IkePayload.PAYLOAD_TYPE_ID_INITIATOR, payload.payloadType);
        assertEquals(IkeIdentification.ID_TYPE_DER_ASN1_DN, payload.ikeId.idType);

        IkeDerAsn1DnIdentification ikeId = (IkeDerAsn1DnIdentification) payload.ikeId;
        X500Principal expectedAsn1Dn = new X500Principal(ASN1_DN_STRING);
        assertEquals(expectedAsn1Dn, ikeId.derAsn1Dn);
    }

    @Test
    public void testDecodeInvalidDerAsn1DnIdPayload() throws Exception {
        try {
            byte[] inputPacket =
                    TestUtils.hexStringToByteArray(ASN1_DN_INVALID_PAYLOAD_BODY_HEX_STRING);
            new IkeIdPayload(false /*critical*/, inputPacket, true /*isInitiator*/);
            fail("Expected to fail due to incorrect formed DN");
        } catch (AuthenticationFailedException expected) {

        }
    }

    @Test
    public void testDecodeUnsupportedIdType() throws Exception {
        byte[] inputPacket =
                TestUtils.hexStringToByteArray(IPV4_ADDR_ID_PAYLOAD_RESPONDER_BODY_HEX_STRING);
        inputPacket[ID_TYPE_OFFSET] = 0;

        try {
            new IkeIdPayload(false, inputPacket, true);
            fail("Expected AuthenticationFailedException: ID Type is unsupported.");
        } catch (AuthenticationFailedException expected) {
        }
    }

    @Test
    public void testConstructAndEncodeIpv4AddrIdPayload() throws Exception {
        Inet4Address ipv4Address = (Inet4Address) Inet4Address.getByName(IPV4_ADDR_STRING);
        IkeIdPayload payload = new IkeIdPayload(false, new IkeIpv4AddrIdentification(ipv4Address));

        ByteBuffer inputBuffer = ByteBuffer.allocate(payload.getPayloadLength());
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_AUTH, inputBuffer);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(IPV4_ADDR_ID_PAYLOAD_RESPONDER_HEX_STRING);
        assertArrayEquals(expectedBytes, inputBuffer.array());
    }

    @Test
    public void testConstructAndEncodeIpv6AddrIdPayload() throws Exception {
        Inet6Address ipv6Address = (Inet6Address) Inet6Address.getByName(IPV6_ADDR_STRING);
        IkeIdPayload payload = new IkeIdPayload(false, new IkeIpv6AddrIdentification(ipv6Address));

        ByteBuffer inputBuffer = ByteBuffer.allocate(payload.getPayloadLength());
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_AUTH, inputBuffer);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(IPV6_ADDR_ID_PAYLOAD_RESPONDER_HEX_STRING);
        assertArrayEquals(expectedBytes, inputBuffer.array());
    }

    @Test
    public void testConstructAndEncodeFqdnIdPayload() throws Exception {
        IkeIdPayload payload =
                new IkeIdPayload(false /*isInitiator*/, new IkeFqdnIdentification(FQDN));

        ByteBuffer inputBuffer = ByteBuffer.allocate(payload.getPayloadLength());
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_CERT, inputBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(FQDN_ID_PAYLOAD_HEX_STRING);
        assertArrayEquals(expectedBytes, inputBuffer.array());
    }

    @Test
    public void testConstructAndEncodeRfc822AddrIdPayload() throws Exception {
        IkeIdPayload payload =
                new IkeIdPayload(
                        true /*isInitiator*/, new IkeRfc822AddrIdentification(RFC822_NAME));

        ByteBuffer inputBuffer = ByteBuffer.allocate(payload.getPayloadLength());
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_CERT, inputBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(RFC822_ADDR_ID_PAYLOAD_HEX_STRING);
        assertArrayEquals(expectedBytes, inputBuffer.array());
    }

    @Test
    public void testConstructAndEncodeKeyIdPayload() throws Exception {
        IkeIdPayload payload =
                new IkeIdPayload(true /*isInitiator*/, new IkeKeyIdIdentification(KEY_ID));

        ByteBuffer inputBuffer = ByteBuffer.allocate(payload.getPayloadLength());
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_CERT, inputBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(KEY_ID_PAYLOAD_HEX_STRING);
        assertArrayEquals(expectedBytes, inputBuffer.array());
    }

    @Test
    public void testConstructAndEncodeDerAsn1DnIdPayload() throws Exception {
        X500Principal asnDn = new X500Principal(ASN1_DN_STRING);
        IkeIdPayload payload =
                new IkeIdPayload(true /*isInitiator*/, new IkeDerAsn1DnIdentification(asnDn));

        ByteBuffer inputBuffer = ByteBuffer.allocate(payload.getPayloadLength());
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_CERT, inputBuffer);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(ASN1_DN_PAYLOAD_HEX_STRING);
        assertArrayEquals(expectedBytes, inputBuffer.array());
    }

    @Test
    public void validatCertSanDns() throws Exception {
        IkeIdPayload payload =
                new IkeIdPayload(false /*isInitiator*/, new IkeFqdnIdentification(CERT_SAN_DNS));
        payload.validateEndCertIdOrThrow(sEndCertWithSanDns);
    }

    @Test
    public void validatCertSanIp() throws Exception {
        IkeIdPayload payload =
                new IkeIdPayload(false /*isInitiator*/, new IkeIpv4AddrIdentification(CERT_SAN_IP));

        payload.validateEndCertIdOrThrow(sEndCertWithSanIp);
    }

    @Test
    public void testThrowWhenCertAndIdMismtached() throws Exception {
        IkeIdPayload payload =
                new IkeIdPayload(false /*isInitiator*/, new IkeIpv4AddrIdentification(CERT_SAN_IP));
        try {
            payload.validateEndCertIdOrThrow(sEndCertWithSanDns);
            fail("Expected to fail because ID and certificate are mismatched");
        } catch (Exception expected) {

        }
    }
}
