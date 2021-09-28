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

import static android.net.ipsec.ike.IkeSessionParams.IKE_OPTION_ACCEPT_ANY_REMOTE_ID;
import static android.net.ipsec.ike.IkeSessionParams.IKE_OPTION_EAP_ONLY_AUTH;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthDigitalSignLocalConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthDigitalSignRemoteConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthEapConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthPskConfig;
import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;
import static android.telephony.TelephonyManager.APPTYPE_USIM;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.eap.EapSessionConfig;
import android.net.ipsec.ike.IkeFqdnIdentification;
import android.net.ipsec.ike.IkeIdentification;
import android.net.ipsec.ike.IkeSaProposal;
import android.net.ipsec.ike.IkeSessionParams;
import android.net.ipsec.ike.IkeSessionParams.ConfigRequestIpv4PcscfServer;
import android.net.ipsec.ike.IkeSessionParams.ConfigRequestIpv6PcscfServer;
import android.net.ipsec.ike.IkeSessionParams.IkeConfigRequest;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.internal.net.ipsec.ike.testutils.CertUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.security.cert.X509Certificate;
import java.security.interfaces.RSAPrivateKey;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public final class IkeSessionParamsTest extends IkeSessionTestBase {
    private static final int HARD_LIFETIME_SECONDS = (int) TimeUnit.HOURS.toSeconds(20L);
    private static final int SOFT_LIFETIME_SECONDS = (int) TimeUnit.HOURS.toSeconds(10L);
    private static final int DPD_DELAY_SECONDS = (int) TimeUnit.MINUTES.toSeconds(10L);
    private static final int[] RETRANS_TIMEOUT_MS_LIST = new int[] {500, 500, 500, 500, 500, 500};

    private static final Map<Class<? extends IkeConfigRequest>, Integer> EXPECTED_REQ_COUNT =
            new HashMap<>();
    private static final HashSet<InetAddress> EXPECTED_PCSCF_SERVERS = new HashSet<>();

    static {
        EXPECTED_REQ_COUNT.put(ConfigRequestIpv4PcscfServer.class, 3);
        EXPECTED_REQ_COUNT.put(ConfigRequestIpv6PcscfServer.class, 3);

        EXPECTED_PCSCF_SERVERS.add(PCSCF_IPV4_ADDRESS_1);
        EXPECTED_PCSCF_SERVERS.add(PCSCF_IPV4_ADDRESS_2);
        EXPECTED_PCSCF_SERVERS.add(PCSCF_IPV6_ADDRESS_1);
        EXPECTED_PCSCF_SERVERS.add(PCSCF_IPV6_ADDRESS_2);
    }

    // Arbitrary proposal and remote ID. Local ID is chosen to match the client end cert in the
    // following CL
    private static final IkeSaProposal SA_PROPOSAL =
            SaProposalTest.buildIkeSaProposalWithNormalModeCipher();
    private static final IkeIdentification LOCAL_ID = new IkeFqdnIdentification(LOCAL_HOSTNAME);
    private static final IkeIdentification REMOTE_ID = new IkeFqdnIdentification(REMOTE_HOSTNAME);

    private static final EapSessionConfig EAP_ALL_METHODS_CONFIG =
            createEapOnlySafeMethodsBuilder()
                    .setEapMsChapV2Config(EAP_MSCHAPV2_USERNAME, EAP_MSCHAPV2_PASSWORD)
                    .build();
    private static final EapSessionConfig EAP_ONLY_SAFE_METHODS_CONFIG =
            createEapOnlySafeMethodsBuilder().build();

    private X509Certificate mServerCaCert;
    private X509Certificate mClientEndCert;
    private X509Certificate mClientIntermediateCaCertOne;
    private X509Certificate mClientIntermediateCaCertTwo;
    private RSAPrivateKey mClientPrivateKey;

    @Before
    public void setUp() throws Exception {
        // This address is never used except for setting up the test network
        setUpTestNetwork(IPV4_ADDRESS_LOCAL);

        mServerCaCert = CertUtils.createCertFromPemFile("server-a-self-signed-ca.pem");
        mClientEndCert = CertUtils.createCertFromPemFile("client-a-end-cert.pem");
        mClientIntermediateCaCertOne =
                CertUtils.createCertFromPemFile("client-a-intermediate-ca-one.pem");
        mClientIntermediateCaCertTwo =
                CertUtils.createCertFromPemFile("client-a-intermediate-ca-two.pem");
        mClientPrivateKey = CertUtils.createRsaPrivateKeyFromKeyFile("client-a-private-key.key");
    }

    @After
    public void tearDown() throws Exception {
        tearDownTestNetwork();
    }

    private static EapSessionConfig.Builder createEapOnlySafeMethodsBuilder() {
        return new EapSessionConfig.Builder()
                .setEapIdentity(EAP_IDENTITY)
                .setEapSimConfig(SUB_ID, APPTYPE_USIM)
                .setEapAkaConfig(SUB_ID, APPTYPE_USIM)
                .setEapAkaPrimeConfig(
                        SUB_ID, APPTYPE_USIM, NETWORK_NAME, true /* allowMismatchedNetworkNames */);
    }

    /**
     * Create a Builder that has minimum configurations to build an IkeSessionParams.
     *
     * <p>Authentication method is arbitrarily selected. Using other method (e.g. setAuthEap) also
     * works.
     */
    private IkeSessionParams.Builder createIkeParamsBuilderMinimum() {
        return new IkeSessionParams.Builder(sContext)
                .setNetwork(mTunNetwork)
                .setServerHostname(IPV4_ADDRESS_REMOTE.getHostAddress())
                .addSaProposal(SA_PROPOSAL)
                .setLocalIdentification(LOCAL_ID)
                .setRemoteIdentification(REMOTE_ID)
                .setAuthPsk(IKE_PSK);
    }

    /**
     * Verify the minimum configurations to build an IkeSessionParams.
     *
     * @see #createIkeParamsBuilderMinimum
     */
    private void verifyIkeParamsMinimum(IkeSessionParams sessionParams) {
        assertEquals(mTunNetwork, sessionParams.getNetwork());
        assertEquals(IPV4_ADDRESS_REMOTE.getHostAddress(), sessionParams.getServerHostname());
        assertEquals(Arrays.asList(SA_PROPOSAL), sessionParams.getSaProposals());
        assertEquals(LOCAL_ID, sessionParams.getLocalIdentification());
        assertEquals(REMOTE_ID, sessionParams.getRemoteIdentification());

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthPskConfig);
        assertArrayEquals(IKE_PSK, ((IkeAuthPskConfig) localConfig).getPsk());
        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthPskConfig);
        assertArrayEquals(IKE_PSK, ((IkeAuthPskConfig) remoteConfig).getPsk());
    }

    @Test
    public void testBuildWithMinimumSet() throws Exception {
        IkeSessionParams sessionParams = createIkeParamsBuilderMinimum().build();

        verifyIkeParamsMinimum(sessionParams);

        // Verify default values that do not need explicit configuration. Do not do assertEquals
        // to be avoid being a change-detector test
        assertTrue(sessionParams.getHardLifetimeSeconds() > sessionParams.getSoftLifetimeSeconds());
        assertTrue(sessionParams.getSoftLifetimeSeconds() > 0);
        assertTrue(sessionParams.getDpdDelaySeconds() > 0);
        assertTrue(sessionParams.getRetransmissionTimeoutsMillis().length > 0);
        for (int timeout : sessionParams.getRetransmissionTimeoutsMillis()) {
            assertTrue(timeout > 0);
        }
        assertTrue(sessionParams.getConfigurationRequests().isEmpty());
        assertFalse(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
    }

    @Test
    public void testSetLifetimes() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimum()
                        .setLifetimeSeconds(HARD_LIFETIME_SECONDS, SOFT_LIFETIME_SECONDS)
                        .build();

        verifyIkeParamsMinimum(sessionParams);
        assertEquals(HARD_LIFETIME_SECONDS, sessionParams.getHardLifetimeSeconds());
        assertEquals(SOFT_LIFETIME_SECONDS, sessionParams.getSoftLifetimeSeconds());
    }

    @Test
    public void testSetDpdDelay() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimum().setDpdDelaySeconds(DPD_DELAY_SECONDS).build();

        verifyIkeParamsMinimum(sessionParams);
        assertEquals(DPD_DELAY_SECONDS, sessionParams.getDpdDelaySeconds());
    }

    @Test
    public void testSetRetransmissionTimeouts() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimum()
                        .setRetransmissionTimeoutsMillis(RETRANS_TIMEOUT_MS_LIST)
                        .build();

        verifyIkeParamsMinimum(sessionParams);
        assertArrayEquals(RETRANS_TIMEOUT_MS_LIST, sessionParams.getRetransmissionTimeoutsMillis());
    }

    @Test
    public void testSetPcscfConfigRequests() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimum()
                        .setRetransmissionTimeoutsMillis(RETRANS_TIMEOUT_MS_LIST)
                        .addPcscfServerRequest(AF_INET)
                        .addPcscfServerRequest(PCSCF_IPV4_ADDRESS_1)
                        .addPcscfServerRequest(PCSCF_IPV6_ADDRESS_1)
                        .addPcscfServerRequest(AF_INET6)
                        .addPcscfServerRequest(PCSCF_IPV4_ADDRESS_2)
                        .addPcscfServerRequest(PCSCF_IPV6_ADDRESS_2)
                        .build();

        verifyIkeParamsMinimum(sessionParams);
        verifyConfigRequestTypes(EXPECTED_REQ_COUNT, sessionParams.getConfigurationRequests());

        Set<InetAddress> resultAddresses = new HashSet<>();
        for (IkeConfigRequest req : sessionParams.getConfigurationRequests()) {
            if (req instanceof ConfigRequestIpv4PcscfServer
                    && ((ConfigRequestIpv4PcscfServer) req).getAddress() != null) {
                resultAddresses.add(((ConfigRequestIpv4PcscfServer) req).getAddress());
            } else if (req instanceof ConfigRequestIpv6PcscfServer
                    && ((ConfigRequestIpv6PcscfServer) req).getAddress() != null) {
                resultAddresses.add(((ConfigRequestIpv6PcscfServer) req).getAddress());
            }
        }
        assertEquals(EXPECTED_PCSCF_SERVERS, resultAddresses);
    }

    @Test
    public void testAddIkeOption() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimum()
                        .addIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .build();

        verifyIkeParamsMinimum(sessionParams);
        assertTrue(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
    }

    @Test
    public void testRemoveIkeOption() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimum()
                        .addIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .removeIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .build();

        verifyIkeParamsMinimum(sessionParams);
        assertFalse(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
    }

    /**
     * Create a Builder that has minimum configurations to build an IkeSessionParams, except for
     * authentication method.
     */
    private IkeSessionParams.Builder createIkeParamsBuilderMinimumWithoutAuth() {
        return new IkeSessionParams.Builder(sContext)
                .setNetwork(mTunNetwork)
                .setServerHostname(IPV4_ADDRESS_REMOTE.getHostAddress())
                .addSaProposal(SA_PROPOSAL)
                .setLocalIdentification(LOCAL_ID)
                .setRemoteIdentification(REMOTE_ID);
    }

    /**
     * Verify the minimum configurations to build an IkeSessionParams, except for authentication
     * method.
     *
     * @see #createIkeParamsBuilderMinimumWithoutAuth
     */
    private void verifyIkeParamsMinimumWithoutAuth(IkeSessionParams sessionParams) {
        assertEquals(mTunNetwork, sessionParams.getNetwork());
        assertEquals(IPV4_ADDRESS_REMOTE.getHostAddress(), sessionParams.getServerHostname());
        assertEquals(Arrays.asList(SA_PROPOSAL), sessionParams.getSaProposals());
        assertEquals(LOCAL_ID, sessionParams.getLocalIdentification());
        assertEquals(REMOTE_ID, sessionParams.getRemoteIdentification());
    }

    @Test
    public void testBuildWithPsk() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimumWithoutAuth().setAuthPsk(IKE_PSK).build();

        verifyIkeParamsMinimumWithoutAuth(sessionParams);

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthPskConfig);
        assertArrayEquals(IKE_PSK, ((IkeAuthPskConfig) localConfig).getPsk());
        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthPskConfig);
        assertArrayEquals(IKE_PSK, ((IkeAuthPskConfig) remoteConfig).getPsk());
    }

    @Test
    public void testBuildWithEap() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimumWithoutAuth()
                        .setAuthEap(mServerCaCert, EAP_ALL_METHODS_CONFIG)
                        .build();

        verifyIkeParamsMinimumWithoutAuth(sessionParams);

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthEapConfig);
        assertEquals(EAP_ALL_METHODS_CONFIG, ((IkeAuthEapConfig) localConfig).getEapConfig());
        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthDigitalSignRemoteConfig);
        assertEquals(
                mServerCaCert, ((IkeAuthDigitalSignRemoteConfig) remoteConfig).getRemoteCaCert());
    }

    @Test
    public void testBuildWithEapOnlyAuth() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimumWithoutAuth()
                        .setAuthEap(mServerCaCert, EAP_ONLY_SAFE_METHODS_CONFIG)
                        .addIkeOption(IKE_OPTION_EAP_ONLY_AUTH)
                        .build();

        assertTrue(sessionParams.hasIkeOption(IKE_OPTION_EAP_ONLY_AUTH));
        verifyIkeParamsMinimumWithoutAuth(sessionParams);

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthEapConfig);
        assertEquals(EAP_ONLY_SAFE_METHODS_CONFIG, ((IkeAuthEapConfig) localConfig).getEapConfig());
        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthDigitalSignRemoteConfig);
        assertEquals(
                mServerCaCert, ((IkeAuthDigitalSignRemoteConfig) remoteConfig).getRemoteCaCert());
    }

    @Test
    public void testThrowBuildEapOnlyAuthWithUnsafeMethod() throws Exception {
        try {
            IkeSessionParams sessionParams =
                    createIkeParamsBuilderMinimumWithoutAuth()
                            .setAuthEap(mServerCaCert, EAP_ALL_METHODS_CONFIG)
                            .addIkeOption(IKE_OPTION_EAP_ONLY_AUTH)
                            .build();
            fail("Expected to fail because EAP only unsafe method is proposed");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithDigitalSignature() throws Exception {
        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimumWithoutAuth()
                        .setAuthDigitalSignature(mServerCaCert, mClientEndCert, mClientPrivateKey)
                        .build();

        verifyIkeParamsMinimumWithoutAuth(sessionParams);

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthDigitalSignLocalConfig);
        IkeAuthDigitalSignLocalConfig localSignConfig = (IkeAuthDigitalSignLocalConfig) localConfig;
        assertEquals(mClientEndCert, localSignConfig.getClientEndCertificate());
        assertEquals(Collections.EMPTY_LIST, localSignConfig.getIntermediateCertificates());
        assertEquals(mClientPrivateKey, localSignConfig.getPrivateKey());

        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthDigitalSignRemoteConfig);
        assertEquals(
                mServerCaCert, ((IkeAuthDigitalSignRemoteConfig) remoteConfig).getRemoteCaCert());
    }

    @Test
    public void testBuildWithDigitalSignatureAndIntermediateCerts() throws Exception {
        List<X509Certificate> intermediateCerts =
                Arrays.asList(mClientIntermediateCaCertOne, mClientIntermediateCaCertTwo);

        IkeSessionParams sessionParams =
                createIkeParamsBuilderMinimumWithoutAuth()
                        .setAuthDigitalSignature(
                                mServerCaCert, mClientEndCert, intermediateCerts, mClientPrivateKey)
                        .build();

        verifyIkeParamsMinimumWithoutAuth(sessionParams);

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthDigitalSignLocalConfig);
        IkeAuthDigitalSignLocalConfig localSignConfig = (IkeAuthDigitalSignLocalConfig) localConfig;
        assertEquals(mClientEndCert, localSignConfig.getClientEndCertificate());
        assertEquals(intermediateCerts, localSignConfig.getIntermediateCertificates());
        assertEquals(mClientPrivateKey, localSignConfig.getPrivateKey());

        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthDigitalSignRemoteConfig);
        assertEquals(
                mServerCaCert, ((IkeAuthDigitalSignRemoteConfig) remoteConfig).getRemoteCaCert());
    }
}
