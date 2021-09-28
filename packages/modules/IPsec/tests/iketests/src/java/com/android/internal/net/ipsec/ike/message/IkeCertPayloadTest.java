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

package com.android.internal.net.ipsec.ike.message;

import static org.junit.Assert.fail;

import com.android.internal.net.ipsec.ike.exceptions.AuthenticationFailedException;
import com.android.internal.net.ipsec.ike.testutils.CertUtils;

import org.junit.Before;
import org.junit.Test;

import java.security.cert.TrustAnchor;
import java.security.cert.X509Certificate;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

public final class IkeCertPayloadTest {
    private X509Certificate mEndCertA;
    private X509Certificate mEndCertB;
    private X509Certificate mEndCertSmall;

    private X509Certificate mIntermediateCertBOne;
    private X509Certificate mIntermediateCertBTwo;

    private TrustAnchor mTrustAnchorA;
    private TrustAnchor mTrustAnchorB;
    private TrustAnchor mTrustAnchorSmall;

    @Before
    public void setUp() throws Exception {
        mEndCertA = CertUtils.createCertFromPemFile("end-cert-a.pem");
        mTrustAnchorA =
                new TrustAnchor(
                        CertUtils.createCertFromPemFile("self-signed-ca-a.pem"),
                        null /*nameConstraints*/);

        mEndCertB = CertUtils.createCertFromPemFile("end-cert-b.pem");
        mIntermediateCertBOne = CertUtils.createCertFromPemFile("intermediate-ca-b-one.pem");
        mIntermediateCertBTwo = CertUtils.createCertFromPemFile("intermediate-ca-b-two.pem");
        mTrustAnchorB =
                new TrustAnchor(
                        CertUtils.createCertFromPemFile("self-signed-ca-b.pem"),
                        null /*nameConstraints*/);

        mEndCertSmall = CertUtils.createCertFromPemFile("end-cert-small.pem");
        mTrustAnchorSmall =
                new TrustAnchor(
                        CertUtils.createCertFromPemFile("self-signed-ca-small.pem"),
                        null /*nameConstraints*/);
    }

    @Test
    public void testValidateCertsNoIntermediateCerts() throws Exception {
        List<X509Certificate> certList = new LinkedList<>();
        certList.add(mEndCertA);

        Set<TrustAnchor> trustAnchors = new HashSet<>();
        trustAnchors.add(mTrustAnchorA);

        IkeCertPayload.validateCertificates(mEndCertA, certList, null /*crlList*/, trustAnchors);
    }

    @Test
    public void testValidateCertsWithIntermediateCerts() throws Exception {
        List<X509Certificate> certList = new LinkedList<>();

        certList.add(mEndCertB);
        certList.add(mIntermediateCertBTwo);
        certList.add(mIntermediateCertBOne);

        Set<TrustAnchor> trustAnchors = new HashSet<>();
        trustAnchors.add(mTrustAnchorB);

        IkeCertPayload.validateCertificates(mEndCertB, certList, null /*crlList*/, trustAnchors);
    }

    @Test
    public void testValidateCertsWithMultiTrustAnchors() throws Exception {
        List<X509Certificate> certList = new LinkedList<>();
        certList.add(mEndCertA);

        Set<TrustAnchor> trustAnchors = new HashSet<>();
        trustAnchors.add(mTrustAnchorA);
        trustAnchors.add(mTrustAnchorB);

        IkeCertPayload.validateCertificates(mEndCertA, certList, null /*crlList*/, trustAnchors);
    }

    @Test
    public void testValidateCertsWithWrongTrustAnchor() throws Exception {
        List<X509Certificate> certList = new LinkedList<>();
        certList.add(mEndCertA);

        Set<TrustAnchor> trustAnchors = new HashSet<>();
        trustAnchors.add(mTrustAnchorB);

        try {
            IkeCertPayload.validateCertificates(
                    mEndCertA, certList, null /*crlList*/, trustAnchors);
            fail("Expected to fail due to absence of valid trust anchor.");
        } catch (AuthenticationFailedException expected) {
        }
    }

    @Test
    public void testValidateCertsWithMissingIntermediateCerts() throws Exception {
        List<X509Certificate> certList = new LinkedList<>();
        certList.add(mEndCertB);
        certList.add(mIntermediateCertBOne);

        Set<TrustAnchor> trustAnchors = new HashSet<>();
        trustAnchors.add(mTrustAnchorB);

        try {
            IkeCertPayload.validateCertificates(
                    mEndCertA, certList, null /*crlList*/, trustAnchors);
            fail("Expected to fail due to absence of intermediate certificate.");
        } catch (AuthenticationFailedException expected) {
        }
    }

    @Test
    public void testValidateCertsWithSmallSizeKey() throws Exception {
        List<X509Certificate> certList = new LinkedList<>();
        certList.add(mEndCertSmall);

        Set<TrustAnchor> trustAnchors = new HashSet<>();
        trustAnchors.add(mTrustAnchorSmall);

        try {
            IkeCertPayload.validateCertificates(
                    mEndCertSmall, certList, null /*crlList*/, trustAnchors);
            fail("Expected to fail because certificates use small size key");
        } catch (AuthenticationFailedException expected) {
        }
    }
}
