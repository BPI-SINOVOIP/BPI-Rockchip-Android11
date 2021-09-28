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

package android.net.ipsec.ike;

import static android.net.ipsec.ike.IkeSessionParams.IKE_DPD_DELAY_SEC_DEFAULT;
import static android.net.ipsec.ike.IkeSessionParams.IKE_HARD_LIFETIME_SEC_DEFAULT;
import static android.net.ipsec.ike.IkeSessionParams.IKE_HARD_LIFETIME_SEC_MAXIMUM;
import static android.net.ipsec.ike.IkeSessionParams.IKE_HARD_LIFETIME_SEC_MINIMUM;
import static android.net.ipsec.ike.IkeSessionParams.IKE_OPTION_ACCEPT_ANY_REMOTE_ID;
import static android.net.ipsec.ike.IkeSessionParams.IKE_RETRANS_TIMEOUT_MS_LIST_DEFAULT;
import static android.net.ipsec.ike.IkeSessionParams.IKE_OPTION_EAP_ONLY_AUTH;
import static android.net.ipsec.ike.IkeSessionParams.IKE_SOFT_LIFETIME_SEC_DEFAULT;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthDigitalSignLocalConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthDigitalSignRemoteConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthEapConfig;
import static android.net.ipsec.ike.IkeSessionParams.IkeAuthPskConfig;
import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;

import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_IP4_PCSCF;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_IP6_PCSCF;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttribute;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.net.ConnectivityManager;
import android.net.InetAddresses;
import android.net.Network;
import android.net.eap.EapSessionConfig;
import android.telephony.TelephonyManager;
import android.util.SparseArray;

import com.android.internal.net.TestUtils;

import org.junit.Before;
import org.junit.Test;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.security.interfaces.DSAPrivateKey;
import java.security.interfaces.RSAPrivateKey;
import java.util.concurrent.TimeUnit;

public final class IkeSessionParamsTest {
    private static final int IKE_OPTION_INVALID = -1;

    private static final String PSK_HEX_STRING = "6A756E69706572313233";
    private static final byte[] PSK = TestUtils.hexStringToByteArray(PSK_HEX_STRING);

    private static final String LOCAL_IPV4_HOST_ADDRESS = "192.0.2.100";
    private static final String REMOTE_IPV4_HOST_ADDRESS = "192.0.2.100";
    private static final String REMOTE_HOSTNAME = "server.test.android.net";

    private static final Inet4Address LOCAL_IPV4_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress(LOCAL_IPV4_HOST_ADDRESS));
    private static final Inet4Address REMOTE_IPV4_ADDRESS =
            (Inet4Address) (InetAddresses.parseNumericAddress(REMOTE_IPV4_HOST_ADDRESS));

    private static final Inet4Address PCSCF_IPV4_ADDRESS_1 =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.1"));
    private static final Inet4Address PCSCF_IPV4_ADDRESS_2 =
            (Inet4Address) (InetAddresses.parseNumericAddress("192.0.2.2"));
    private static final Inet6Address PCSCF_IPV6_ADDRESS_1 =
            (Inet6Address) (InetAddresses.parseNumericAddress("2001:DB8::1"));
    private static final Inet6Address PCSCF_IPV6_ADDRESS_2 =
            (Inet6Address) (InetAddresses.parseNumericAddress("2001:DB8::2"));

    private ConnectivityManager mMockConnectManager;
    private Network mMockDefaultNetwork;
    private Network mMockUserConfigNetwork;

    private IkeSaProposal mIkeSaProposal;
    private IkeIdentification mLocalIdentification;
    private IkeIdentification mRemoteIdentification;

    private X509Certificate mMockServerCaCert;
    private X509Certificate mMockClientEndCert;
    private PrivateKey mMockRsaPrivateKey;

    @Before
    public void setUp() throws Exception {
        mMockConnectManager = mock(ConnectivityManager.class);
        mMockDefaultNetwork = mock(Network.class);
        mMockUserConfigNetwork = mock(Network.class);
        when(mMockConnectManager.getActiveNetwork()).thenReturn(mMockDefaultNetwork);

        mIkeSaProposal =
                new IkeSaProposal.Builder()
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8,
                                SaProposal.KEY_LEN_AES_128)
                        .addPseudorandomFunction(SaProposal.PSEUDORANDOM_FUNCTION_AES128_XCBC)
                        .addDhGroup(SaProposal.DH_GROUP_1024_BIT_MODP)
                        .build();
        mLocalIdentification = new IkeIpv4AddrIdentification(LOCAL_IPV4_ADDRESS);
        mRemoteIdentification = new IkeIpv4AddrIdentification(REMOTE_IPV4_ADDRESS);

        mMockServerCaCert = mock(X509Certificate.class);
        mMockClientEndCert = mock(X509Certificate.class);
        mMockRsaPrivateKey = mock(RSAPrivateKey.class);
    }

    private void verifyIkeParamsWithSeverIpAndDefaultValues(IkeSessionParams sessionParams) {
        verifyIkeSessionParamsCommon(sessionParams);

        assertEquals(REMOTE_IPV4_HOST_ADDRESS, sessionParams.getServerHostname());
        assertFalse(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
        assertEquals(IKE_DPD_DELAY_SEC_DEFAULT, sessionParams.getDpdDelaySeconds());
        assertArrayEquals(
                IKE_RETRANS_TIMEOUT_MS_LIST_DEFAULT,
                sessionParams.getRetransmissionTimeoutsMillis());
    }

    private void verifyIkeSessionParamsCommon(IkeSessionParams sessionParams) {
        assertArrayEquals(
                new SaProposal[] {mIkeSaProposal}, sessionParams.getSaProposalsInternal());

        assertEquals(mLocalIdentification, sessionParams.getLocalIdentification());
        assertEquals(mRemoteIdentification, sessionParams.getRemoteIdentification());

        assertFalse(sessionParams.isIkeFragmentationSupported());
    }

    private void verifyAuthPskConfig(IkeSessionParams sessionParams) {
        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthPskConfig);
        assertEquals(IkeSessionParams.IKE_AUTH_METHOD_PSK, localConfig.mAuthMethod);
        assertArrayEquals(PSK, ((IkeAuthPskConfig) localConfig).mPsk);

        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthPskConfig);
        assertEquals(IkeSessionParams.IKE_AUTH_METHOD_PSK, remoteConfig.mAuthMethod);
        assertArrayEquals(PSK, ((IkeAuthPskConfig) remoteConfig).mPsk);
    }

    private IkeSessionParams.Builder buildWithPskCommon(String hostname) {
        return new IkeSessionParams.Builder(mMockConnectManager)
                .setServerHostname(hostname)
                .addSaProposal(mIkeSaProposal)
                .setLocalIdentification(mLocalIdentification)
                .setRemoteIdentification(mRemoteIdentification)
                .setAuthPsk(PSK);
    }

    @Test
    public void testBuildWithPsk() throws Exception {
        IkeSessionParams sessionParams = buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS).build();

        verifyIkeParamsWithSeverIpAndDefaultValues(sessionParams);
        verifyAuthPskConfig(sessionParams);

        assertEquals(mMockDefaultNetwork, sessionParams.getNetwork());

        assertEquals(IKE_HARD_LIFETIME_SEC_DEFAULT, sessionParams.getHardLifetimeSeconds());
        assertEquals(IKE_SOFT_LIFETIME_SEC_DEFAULT, sessionParams.getSoftLifetimeSeconds());
    }

    @Test
    public void testBuildWithPskAndLifetime() throws Exception {
        int hardLifetimeSec = (int) TimeUnit.HOURS.toSeconds(20L);
        int softLifetimeSec = (int) TimeUnit.HOURS.toSeconds(10L);

        IkeSessionParams sessionParams =
                buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                        .setLifetimeSeconds(hardLifetimeSec, softLifetimeSec)
                        .build();

        verifyIkeParamsWithSeverIpAndDefaultValues(sessionParams);
        verifyAuthPskConfig(sessionParams);

        assertEquals(hardLifetimeSec, sessionParams.getHardLifetimeSeconds());
        assertEquals(softLifetimeSec, sessionParams.getSoftLifetimeSeconds());
    }

    @Test
    public void testBuildWithPskAndHostname() throws Exception {
        IkeSessionParams sessionParams = buildWithPskCommon(REMOTE_HOSTNAME).build();

        verifyIkeSessionParamsCommon(sessionParams);
        verifyAuthPskConfig(sessionParams);

        assertEquals(REMOTE_HOSTNAME, sessionParams.getServerHostname());
    }

    @Test
    public void testAddIkeOption() throws Exception {
        IkeSessionParams sessionParams =
                buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                        .addIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .build();

        verifyIkeSessionParamsCommon(sessionParams);
        verifyAuthPskConfig(sessionParams);

        assertTrue(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
    }

    @Test
    public void testAddAndRemoveIkeOption() throws Exception {
        IkeSessionParams sessionParams =
                buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                        .addIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .removeIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .build();

        verifyIkeSessionParamsCommon(sessionParams);
        verifyAuthPskConfig(sessionParams);

        assertFalse(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
    }

    @Test
    public void testAddInvalidIkeOption() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager).addIkeOption(IKE_OPTION_INVALID);
            fail("Expected to fail due to invalid ikeOption");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testRemoveInvalidIkeOption() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager).removeIkeOption(IKE_OPTION_INVALID);
            fail("Expected to fail due to invalid ikeOption");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testCheckInvalidIkeOption() throws Exception {
        IkeSessionParams sessionParams =
                buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                        .addIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .build();

        try {
            sessionParams.hasIkeOption(IKE_OPTION_INVALID);
            fail("Expected to fail due to invalid ikeOption");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithPskAndDpdDelay() throws Exception {
        final int dpdDelaySec = 100;

        IkeSessionParams sessionParams =
                buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                        .setDpdDelaySeconds(dpdDelaySec)
                        .build();

        verifyIkeSessionParamsCommon(sessionParams);
        verifyAuthPskConfig(sessionParams);
        assertEquals(REMOTE_IPV4_HOST_ADDRESS, sessionParams.getServerHostname());

        // Verify default values
        assertFalse(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));
        assertArrayEquals(
                IKE_RETRANS_TIMEOUT_MS_LIST_DEFAULT,
                sessionParams.getRetransmissionTimeoutsMillis());

        // Verify DPD delay
        assertEquals(dpdDelaySec, sessionParams.getDpdDelaySeconds());
    }

    @Test
    public void testBuildWithInvalidDpdDelay() throws Exception {
        final int invalidDpdDelay = 1;
        try {
            new IkeSessionParams.Builder(mMockConnectManager).setDpdDelaySeconds(invalidDpdDelay);
            fail("Expected to fail due to invalid DPD delay");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithPskAndRetransmission() throws Exception {
        final int[] retransmissionTimeoutList = new int[] {1000, 2000, 3000, 4000};

        IkeSessionParams sessionParams =
                buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                        .setRetransmissionTimeoutsMillis(retransmissionTimeoutList)
                        .build();

        verifyIkeSessionParamsCommon(sessionParams);
        verifyAuthPskConfig(sessionParams);
        assertEquals(REMOTE_IPV4_HOST_ADDRESS, sessionParams.getServerHostname());

        // Verify default values
        assertEquals(IKE_DPD_DELAY_SEC_DEFAULT, sessionParams.getDpdDelaySeconds());
        assertFalse(sessionParams.hasIkeOption(IKE_OPTION_ACCEPT_ANY_REMOTE_ID));

        // Verify retransmission configuration
        assertEquals(retransmissionTimeoutList, sessionParams.getRetransmissionTimeoutsMillis());
    }

    @Test
    public void testBuildWithInvalidRetransmissionTimeout() throws Exception {
        final int[] invalidRetransTimeoutMsList = new int[] {1000, 2000, 3000, Integer.MAX_VALUE};
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setRetransmissionTimeoutsMillis(invalidRetransTimeoutMsList);
            fail("Expected to fail due to invalid retransmission timeout");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithInvalidMaxRetransmissionAttempts() throws Exception {
        final int[] invalidRetransTimeoutMsList = new int[20];
        for (int t : invalidRetransTimeoutMsList) t = 500;

        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setRetransmissionTimeoutsMillis(invalidRetransTimeoutMsList);
            fail("Expected to fail due to invalid max retransmission times");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithEap() throws Exception {
        EapSessionConfig eapConfig = mock(EapSessionConfig.class);

        IkeSessionParams sessionParams =
                new IkeSessionParams.Builder(mMockConnectManager)
                        .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                        .addSaProposal(mIkeSaProposal)
                        .setLocalIdentification(mLocalIdentification)
                        .setRemoteIdentification(mRemoteIdentification)
                        .setAuthEap(mMockServerCaCert, eapConfig)
                        .build();

        verifyIkeParamsWithSeverIpAndDefaultValues(sessionParams);
        assertEquals(mMockDefaultNetwork, sessionParams.getNetwork());

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthEapConfig);
        assertEquals(IkeSessionParams.IKE_AUTH_METHOD_EAP, localConfig.mAuthMethod);
        assertEquals(eapConfig, ((IkeAuthEapConfig) localConfig).mEapConfig);

        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthDigitalSignRemoteConfig);
        assertEquals(IkeSessionParams.IKE_AUTH_METHOD_PUB_KEY_SIGNATURE, remoteConfig.mAuthMethod);
        assertEquals(
                mMockServerCaCert,
                ((IkeAuthDigitalSignRemoteConfig) remoteConfig).mTrustAnchor.getTrustedCert());
    }

    @Test
    public void testBuildWithDigitalSignatureAuth() throws Exception {
        IkeSessionParams sessionParams =
                new IkeSessionParams.Builder(mMockConnectManager)
                        .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                        .setNetwork(mMockUserConfigNetwork)
                        .addSaProposal(mIkeSaProposal)
                        .setLocalIdentification(mLocalIdentification)
                        .setRemoteIdentification(mRemoteIdentification)
                        .setAuthDigitalSignature(
                                mMockServerCaCert, mMockClientEndCert, mMockRsaPrivateKey)
                        .build();

        verifyIkeParamsWithSeverIpAndDefaultValues(sessionParams);
        assertEquals(mMockUserConfigNetwork, sessionParams.getNetwork());

        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthDigitalSignLocalConfig);

        IkeAuthDigitalSignLocalConfig localAuthConfig = (IkeAuthDigitalSignLocalConfig) localConfig;
        assertEquals(
                IkeSessionParams.IKE_AUTH_METHOD_PUB_KEY_SIGNATURE, localAuthConfig.mAuthMethod);
        assertEquals(mMockClientEndCert, localAuthConfig.mEndCert);
        assertTrue(localAuthConfig.mIntermediateCerts.isEmpty());
        assertEquals(mMockRsaPrivateKey, localAuthConfig.mPrivateKey);

        IkeAuthConfig remoteConfig = sessionParams.getRemoteAuthConfig();
        assertTrue(remoteConfig instanceof IkeAuthDigitalSignRemoteConfig);
        assertEquals(IkeSessionParams.IKE_AUTH_METHOD_PUB_KEY_SIGNATURE, remoteConfig.mAuthMethod);
        assertEquals(
                mMockServerCaCert,
                ((IkeAuthDigitalSignRemoteConfig) remoteConfig).mTrustAnchor.getTrustedCert());
    }

    @Test
    public void testBuildWithDsaDigitalSignatureAuth() throws Exception {
        try {
            IkeSessionParams sessionParams =
                    new IkeSessionParams.Builder(mMockConnectManager)
                            .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                            .addSaProposal(mIkeSaProposal)
                            .setLocalIdentification(mLocalIdentification)
                            .setRemoteIdentification(mRemoteIdentification)
                            .setAuthDigitalSignature(
                                    mMockServerCaCert,
                                    mMockClientEndCert,
                                    mock(DSAPrivateKey.class))
                            .build();
            fail("Expected to fail because DSA is not supported");
        } catch (IllegalArgumentException expected) {

        }
    }

    private void verifyAttrTypes(SparseArray expectedAttrCntMap, IkeSessionParams ikeParams) {
        ConfigAttribute[] configAttributes = ikeParams.getConfigurationAttributesInternal();

        SparseArray<Integer> atrrCntMap = expectedAttrCntMap.clone();

        for (int i = 0; i < configAttributes.length; i++) {
            int attType = configAttributes[i].attributeType;
            assertNotNull(atrrCntMap.get(attType));

            atrrCntMap.put(attType, atrrCntMap.get(attType) - 1);
            if (atrrCntMap.get(attType) == 0) atrrCntMap.remove(attType);
        }

        assertEquals(0, atrrCntMap.size());
    }

    @Test
    public void testBuildWithPcscfAddress() throws Exception {
        IkeSessionParams sessionParams =
                new IkeSessionParams.Builder(mMockConnectManager)
                        .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                        .addSaProposal(mIkeSaProposal)
                        .setLocalIdentification(mLocalIdentification)
                        .setRemoteIdentification(mRemoteIdentification)
                        .setAuthPsk(PSK)
                        .addPcscfServerRequest(AF_INET)
                        .addPcscfServerRequest(PCSCF_IPV4_ADDRESS_1)
                        .addPcscfServerRequest(PCSCF_IPV6_ADDRESS_2)
                        .addPcscfServerRequest(AF_INET6)
                        .addPcscfServerRequest(PCSCF_IPV4_ADDRESS_1)
                        .addPcscfServerRequest(PCSCF_IPV6_ADDRESS_2)
                        .build();

        SparseArray<Integer> expectedAttrCounts = new SparseArray<>();
        expectedAttrCounts.put(CONFIG_ATTR_IP4_PCSCF, 3);
        expectedAttrCounts.put(CONFIG_ATTR_IP6_PCSCF, 3);

        verifyAttrTypes(expectedAttrCounts, sessionParams);
    }

    @Test
    public void testBuildWithoutPcscfAddress() throws Exception {
        IkeSessionParams sessionParams =
                new IkeSessionParams.Builder(mMockConnectManager)
                        .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                        .addSaProposal(mIkeSaProposal)
                        .setLocalIdentification(mLocalIdentification)
                        .setRemoteIdentification(mRemoteIdentification)
                        .setAuthPsk(PSK)
                        .build();

        SparseArray<Integer> expectedAttrCounts = new SparseArray<>();

        verifyAttrTypes(expectedAttrCounts, sessionParams);
    }

    @Test
    public void testBuildWithoutSaProposal() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                    .build();
            fail("Expected to fail due to absence of SA proposal.");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithoutLocalId() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                    .addSaProposal(mIkeSaProposal)
                    .setRemoteIdentification(mRemoteIdentification)
                    .setAuthPsk(PSK)
                    .build();
            fail("Expected to fail because local identification is not set.");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testBuildWithoutSetAuth() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setServerHostname(REMOTE_IPV4_HOST_ADDRESS)
                    .addSaProposal(mIkeSaProposal)
                    .setLocalIdentification(mLocalIdentification)
                    .setRemoteIdentification(mRemoteIdentification)
                    .build();
            fail("Expected to fail because authentiction method is not set.");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testNonAsciiFqdnAuthentication() throws Exception {
        try {
            new IkeFqdnIdentification("¯\\_(ツ)_/¯");
            fail("Expected failure based on non-ASCII characters.");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testSetHardLifetimeTooLong() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setLifetimeSeconds(
                            IKE_HARD_LIFETIME_SEC_MAXIMUM + 1, IKE_SOFT_LIFETIME_SEC_DEFAULT);
            fail("Expected failure because hard lifetime is too long");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testSetHardLifetimeTooShort() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setLifetimeSeconds(
                            IKE_HARD_LIFETIME_SEC_MINIMUM - 1, IKE_SOFT_LIFETIME_SEC_DEFAULT);
            fail("Expected failure because hard lifetime is too short");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testSetSoftLifetimeTooLong() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setLifetimeSeconds(
                            IKE_HARD_LIFETIME_SEC_DEFAULT, IKE_HARD_LIFETIME_SEC_DEFAULT);
            fail("Expected failure because soft lifetime is too long");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testSetSoftLifetimeTooShort() throws Exception {
        try {
            new IkeSessionParams.Builder(mMockConnectManager)
                    .setLifetimeSeconds(IKE_HARD_LIFETIME_SEC_DEFAULT, 0);
            fail("Expected failure because soft lifetime is too short");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testExpceptionOnEapOnlyOptionWithoutEapAuth() throws Exception {
        try {
            IkeSessionParams sessionParams =
                    buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                            .addIkeOption(IKE_OPTION_EAP_ONLY_AUTH)
                            .build();
            fail("Expected failure because eap only option added without setting auth config");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testExceptionOnEapOnlyOptionWithEapOnlyUnsafeMethod() throws Exception {
        try {
            EapSessionConfig eapSessionConfig = new EapSessionConfig.Builder()
                    .setEapMsChapV2Config(null, null)
                    .setEapAkaConfig(0, 0)
                    .build();

            IkeSessionParams sessionParams =
                    buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                            .addIkeOption(IKE_OPTION_EAP_ONLY_AUTH)
                            .setAuthEap(null, eapSessionConfig)
                            .build();
            fail("Expected failure because eap only option added without eap only safe method");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testBuildWithEapOnlyOption() throws Exception {
        EapSessionConfig eapSessionConfig = new EapSessionConfig.Builder()
                .setEapAkaConfig(0, TelephonyManager.APPTYPE_ISIM)
                .build();

        IkeSessionParams sessionParams = buildWithPskCommon(REMOTE_IPV4_HOST_ADDRESS)
                .addIkeOption(IKE_OPTION_EAP_ONLY_AUTH)
                .setAuthEap(null, eapSessionConfig)
                .build();

        assertNotNull(sessionParams);
        verifyIkeSessionParamsCommon(sessionParams);
        assertTrue(sessionParams.hasIkeOption(IKE_OPTION_EAP_ONLY_AUTH));
        IkeAuthConfig localConfig = sessionParams.getLocalAuthConfig();
        assertTrue(localConfig instanceof IkeAuthEapConfig);
    }
}
