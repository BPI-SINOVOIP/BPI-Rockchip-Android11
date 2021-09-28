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

package android.net.ipsec.ike.cts;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import android.net.ipsec.ike.IkeDerAsn1DnIdentification;
import android.net.ipsec.ike.IkeFqdnIdentification;
import android.net.ipsec.ike.IkeIpv4AddrIdentification;
import android.net.ipsec.ike.IkeIpv6AddrIdentification;
import android.net.ipsec.ike.IkeKeyIdIdentification;
import android.net.ipsec.ike.IkeRfc822AddrIdentification;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import javax.security.auth.x500.X500Principal;

@RunWith(AndroidJUnit4.class)
public final class IkeIdentificationTest extends IkeTestBase {
    @Test
    public void testIkeDerAsn1DnIdentification() throws Exception {
        X500Principal asn1Dn = new X500Principal(LOCAL_ASN1_DN_STRING);

        IkeDerAsn1DnIdentification ikeId = new IkeDerAsn1DnIdentification(asn1Dn);
        assertEquals(asn1Dn, ikeId.derAsn1Dn);
    }

    @Test
    public void testIkeFqdnIdentification() throws Exception {
        IkeFqdnIdentification ikeId = new IkeFqdnIdentification(LOCAL_HOSTNAME);
        assertEquals(LOCAL_HOSTNAME, ikeId.fqdn);
    }

    @Test
    public void testIkeIpv4AddrIdentification() throws Exception {
        IkeIpv4AddrIdentification ikeId = new IkeIpv4AddrIdentification(IPV4_ADDRESS_LOCAL);
        assertEquals(IPV4_ADDRESS_LOCAL, ikeId.ipv4Address);
    }

    @Test
    public void testIkeIpv6AddrIdentification() throws Exception {
        IkeIpv6AddrIdentification ikeId = new IkeIpv6AddrIdentification(IPV6_ADDRESS_LOCAL);
        assertEquals(IPV6_ADDRESS_LOCAL, ikeId.ipv6Address);
    }

    @Test
    public void testIkeKeyIdIdentification() throws Exception {
        IkeKeyIdIdentification ikeId = new IkeKeyIdIdentification(LOCAL_KEY_ID);
        assertArrayEquals(LOCAL_KEY_ID, ikeId.keyId);
    }

    @Test
    public void testIkeRfc822AddrIdentification() throws Exception {
        IkeRfc822AddrIdentification ikeId = new IkeRfc822AddrIdentification(LOCAL_RFC822_NAME);
        assertEquals(LOCAL_RFC822_NAME, ikeId.rfc822Name);
    }
}
