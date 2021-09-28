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

package android.net.cts;

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;
import static android.net.cts.util.CtsNetUtils.TestNetworkCallback;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.Manifest;
import android.annotation.NonNull;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Ikev2VpnProfile;
import android.net.IpSecAlgorithm;
import android.net.LinkAddress;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.ProxyInfo;
import android.net.TestNetworkInterface;
import android.net.TestNetworkManager;
import android.net.VpnManager;
import android.net.cts.util.CtsNetUtils;
import android.os.Build;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;

import androidx.test.InstrumentationRegistry;

import com.android.internal.util.HexDump;
import com.android.org.bouncycastle.x509.X509V1CertificateGenerator;
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo;
import com.android.testutils.DevSdkIgnoreRunner;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.math.BigInteger;
import java.net.InetAddress;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.concurrent.TimeUnit;

import javax.security.auth.x500.X500Principal;

@RunWith(DevSdkIgnoreRunner.class)
@IgnoreUpTo(Build.VERSION_CODES.Q)
@AppModeFull(reason = "Appops state changes disallowed for instant apps (OP_ACTIVATE_PLATFORM_VPN)")
public class Ikev2VpnTest {
    private static final String TAG = Ikev2VpnTest.class.getSimpleName();

    // Test vectors for IKE negotiation in test mode.
    private static final String SUCCESSFUL_IKE_INIT_RESP_V4 =
            "46b8eca1e0d72a18b2b5d9006d47a0022120222000000000000002d0220000300000002c01010004030000"
                    + "0c0100000c800e0100030000080300000c030000080200000400000008040000102800020800"
                    + "100000b8070f159fe5141d8754ca86f72ecc28d66f514927e96cbe9eec0adb42bf2c276a0ab7"
                    + "a97fa93555f4be9218c14e7f286bb28c6b4fb13825a420f2ffc165854f200bab37d69c8963d4"
                    + "0acb831d983163aa50622fd35c182efe882cf54d6106222abcfaa597255d302f1b95ab71c142"
                    + "c279ea5839a180070bff73f9d03fab815f0d5ee2adec7e409d1e35979f8bd92ffd8aab13d1a0"
                    + "0657d816643ae767e9ae84d2ccfa2bcce1a50572be8d3748ae4863c41ae90da16271e014270f"
                    + "77edd5cd2e3299f3ab27d7203f93d770bacf816041cdcecd0f9af249033979da4369cb242dd9"
                    + "6d172e60513ff3db02de63e50eb7d7f596ada55d7946cad0af0669d1f3e2804846ab3f2a930d"
                    + "df56f7f025f25c25ada694e6231abbb87ee8cfd072c8481dc0b0f6b083fdc3bd89b080e49feb"
                    + "0288eef6fdf8a26ee2fc564a11e7385215cf2deaf2a9965638fc279c908ccdf04094988d91a2"
                    + "464b4a8c0326533aff5119ed79ecbd9d99a218b44f506a5eb09351e67da86698b4c58718db25"
                    + "d55f426fb4c76471b27a41fbce00777bc233c7f6e842e39146f466826de94f564cad8b92bfbe"
                    + "87c99c4c7973ec5f1eea8795e7da82819753aa7c4fcfdab77066c56b939330c4b0d354c23f83"
                    + "ea82fa7a64c4b108f1188379ea0eb4918ee009d804100e6bf118771b9058d42141c847d5ec37"
                    + "6e5ec591c71fc9dac01063c2bd31f9c783b28bf1182900002430f3d5de3449462b31dd28bc27"
                    + "297b6ad169bccce4f66c5399c6e0be9120166f2900001c0000400428b8df2e66f69c8584a186"
                    + "c5eac66783551d49b72900001c000040054e7a622e802d5cbfb96d5f30a6e433994370173529"
                    + "0000080000402e290000100000402f00020003000400050000000800004014";
    private static final String SUCCESSFUL_IKE_INIT_RESP_V6 =
            "46b8eca1e0d72a1800d9ea1babce26bf2120222000000000000002d0220000300000002c01010004030000"
                    + "0c0100000c800e0100030000080300000c030000080200000400000008040000102800020800"
                    + "100000ea0e6dd9ca5930a9a45c323a41f64bfd8cdef7730f5fbff37d7c377da427f489a42aa8"
                    + "c89233380e6e925990d49de35c2cdcf63a61302c731a4b3569df1ee1bf2457e55a6751838ede"
                    + "abb75cc63ba5c9e4355e8e784f383a5efe8a44727dc14aeaf8dacc2620fb1c8875416dc07739"
                    + "7fe4decc1bd514a9c7d270cf21fd734c63a25c34b30b68686e54e8a198f37f27cb491fe27235"
                    + "fab5476b036d875ccab9a68d65fbf3006197f9bebbf94de0d3802b4fafe1d48d931ce3a1a346"
                    + "2d65bd639e9bd7fa46299650a9dbaf9b324e40b466942d91a59f41ef8042f8474c4850ed0f63"
                    + "e9238949d41cd8bbaea9aefdb65443a6405792839563aa5dc5c36b5ce8326ccf8a94d9622b85"
                    + "038d390d5fc0299e14e1f022966d4ac66515f6108ca04faec44821fe5bbf2ed4f84ff5671219"
                    + "608cb4c36b44a31ba010c9088f8d5ff943bb9ff857f74be1755f57a5783874adc57f42bb174e"
                    + "4ad3215de628707014dbcb1707bd214658118fdd7a42b3e1638b991ce5b812a667f1145be811"
                    + "685e3cd3baf9b18d062657b64c206a4d19a531c252a6a51a04aeaf42c618620cdbab65baca23"
                    + "82c57ed888422aeaacf7f1bc3fe2247ff7e7eaca218b74d7b31d02f2b0afa123f802529e7e6c"
                    + "3259d418290740ddbf55686e26998d7edcbbf895664972fed666f2f20af40503aa2af436ec6d"
                    + "4ec981ab19b9088755d94ae7a7c2066ea331d4e56e290000243fefe5555fce552d57a84e682c"
                    + "d4a6dfb3f2f94a94464d5bec3d88b88e9559642900001c00004004eb4afff764e7b79bca78b1"
                    + "3a89100d36d678ae982900001c00004005d177216a3c26f782076e12570d40bfaaa148822929"
                    + "0000080000402e290000100000402f00020003000400050000000800004014";
    private static final String SUCCESSFUL_IKE_AUTH_RESP_V4 =
            "46b8eca1e0d72a18b2b5d9006d47a0022e20232000000001000000e0240000c420a2500a3da4c66fa6929e"
                    + "600f36349ba0e38de14f78a3ad0416cba8c058735712a3d3f9a0a6ed36de09b5e9e02697e7c4"
                    + "2d210ac86cfbd709503cfa51e2eab8cfdc6427d136313c072968f6506a546eb5927164200592"
                    + "6e36a16ee994e63f029432a67bc7d37ca619e1bd6e1678df14853067ecf816b48b81e8746069"
                    + "406363e5aa55f13cb2afda9dbebee94256c29d630b17dd7f1ee52351f92b6e1c3d8551c513f1"
                    + "d74ac52a80b2041397e109fe0aeb3c105b0d4be0ae343a943398764281";
    private static final String SUCCESSFUL_IKE_AUTH_RESP_V6 =
            "46b8eca1e0d72a1800d9ea1babce26bf2e20232000000001000000f0240000d4aaf6eaa6c06b50447e6f54"
                    + "827fd8a9d9d6ac8015c1ebb3e8cb03fc6e54b49a107441f50004027cc5021600828026367f03"
                    + "bc425821cd7772ee98637361300c9b76056e874fea2bd4a17212370b291894264d8c023a01d1"
                    + "c3b691fd4b7c0b534e8c95af4c4638e2d125cb21c6267e2507cd745d72e8da109c47b9259c6c"
                    + "57a26f6bc5b337b9b9496d54bdde0333d7a32e6e1335c9ee730c3ecd607a8689aa7b0577b74f"
                    + "3bf437696a9fd5fc0aee3ed346cd9e15d1dda293df89eb388a8719388a60ca7625754de12cdb"
                    + "efe4c886c5c401";
    private static final long IKE_INITIATOR_SPI = Long.parseLong("46B8ECA1E0D72A18", 16);

    private static final InetAddress LOCAL_OUTER_4 = InetAddress.parseNumericAddress("192.0.2.1");
    private static final InetAddress LOCAL_OUTER_6 =
            InetAddress.parseNumericAddress("2001:db8::1");

    private static final int IP4_PREFIX_LEN = 32;
    private static final int IP6_PREFIX_LEN = 128;

    // TODO: Use IPv6 address when we can generate test vectors (GCE does not allow IPv6 yet).
    private static final String TEST_SERVER_ADDR_V4 = "192.0.2.2";
    private static final String TEST_SERVER_ADDR_V6 = "2001:db8::2";
    private static final String TEST_IDENTITY = "client.cts.android.com";
    private static final List<String> TEST_ALLOWED_ALGORITHMS =
            Arrays.asList(IpSecAlgorithm.AUTH_CRYPT_AES_GCM);

    private static final ProxyInfo TEST_PROXY_INFO =
            ProxyInfo.buildDirectProxy("proxy.cts.android.com", 1234);
    private static final int TEST_MTU = 1300;

    private static final byte[] TEST_PSK = "ikeAndroidPsk".getBytes();
    private static final String TEST_USER = "username";
    private static final String TEST_PASSWORD = "pa55w0rd";

    // Static state to reduce setup/teardown
    private static final Context sContext = InstrumentationRegistry.getContext();
    private static final ConnectivityManager sCM =
            (ConnectivityManager) sContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    private static final VpnManager sVpnMgr =
            (VpnManager) sContext.getSystemService(Context.VPN_MANAGEMENT_SERVICE);
    private static final CtsNetUtils mCtsNetUtils = new CtsNetUtils(sContext);

    private final X509Certificate mServerRootCa;
    private final CertificateAndKey mUserCertKey;

    public Ikev2VpnTest() throws Exception {
        // Build certificates
        mServerRootCa = generateRandomCertAndKeyPair().cert;
        mUserCertKey = generateRandomCertAndKeyPair();
    }

    @After
    public void tearDown() {
        setAppop(AppOpsManager.OP_ACTIVATE_VPN, false);
        setAppop(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, false);
    }

    /**
     * Sets the given appop using shell commands
     *
     * <p>This method must NEVER be called from within a shell permission, as it will attempt to
     * acquire, and then drop the shell permission identity. This results in the caller losing the
     * shell permission identity due to these calls not being reference counted.
     */
    public void setAppop(int appop, boolean allow) {
        // Requires shell permission to update appops.
        runWithShellPermissionIdentity(() -> {
            mCtsNetUtils.setAppopPrivileged(appop, allow);
        }, Manifest.permission.MANAGE_TEST_NETWORKS);
    }

    private Ikev2VpnProfile buildIkev2VpnProfileCommon(
            Ikev2VpnProfile.Builder builder, boolean isRestrictedToTestNetworks) throws Exception {
        if (isRestrictedToTestNetworks) {
            builder.restrictToTestNetworks();
        }

        return builder.setBypassable(true)
                .setAllowedAlgorithms(TEST_ALLOWED_ALGORITHMS)
                .setProxy(TEST_PROXY_INFO)
                .setMaxMtu(TEST_MTU)
                .setMetered(false)
                .build();
    }

    private Ikev2VpnProfile buildIkev2VpnProfilePsk(boolean isRestrictedToTestNetworks)
            throws Exception {
        return buildIkev2VpnProfilePsk(TEST_SERVER_ADDR_V6, isRestrictedToTestNetworks);
    }

    private Ikev2VpnProfile buildIkev2VpnProfilePsk(
            String remote, boolean isRestrictedToTestNetworks) throws Exception {
        final Ikev2VpnProfile.Builder builder =
                new Ikev2VpnProfile.Builder(remote, TEST_IDENTITY).setAuthPsk(TEST_PSK);

        return buildIkev2VpnProfileCommon(builder, isRestrictedToTestNetworks);
    }

    private Ikev2VpnProfile buildIkev2VpnProfileUsernamePassword(boolean isRestrictedToTestNetworks)
            throws Exception {
        final Ikev2VpnProfile.Builder builder =
                new Ikev2VpnProfile.Builder(TEST_SERVER_ADDR_V6, TEST_IDENTITY)
                        .setAuthUsernamePassword(TEST_USER, TEST_PASSWORD, mServerRootCa);

        return buildIkev2VpnProfileCommon(builder, isRestrictedToTestNetworks);
    }

    private Ikev2VpnProfile buildIkev2VpnProfileDigitalSignature(boolean isRestrictedToTestNetworks)
            throws Exception {
        final Ikev2VpnProfile.Builder builder =
                new Ikev2VpnProfile.Builder(TEST_SERVER_ADDR_V6, TEST_IDENTITY)
                        .setAuthDigitalSignature(
                                mUserCertKey.cert, mUserCertKey.key, mServerRootCa);

        return buildIkev2VpnProfileCommon(builder, isRestrictedToTestNetworks);
    }

    private void checkBasicIkev2VpnProfile(@NonNull Ikev2VpnProfile profile) throws Exception {
        assertEquals(TEST_SERVER_ADDR_V6, profile.getServerAddr());
        assertEquals(TEST_IDENTITY, profile.getUserIdentity());
        assertEquals(TEST_PROXY_INFO, profile.getProxyInfo());
        assertEquals(TEST_ALLOWED_ALGORITHMS, profile.getAllowedAlgorithms());
        assertTrue(profile.isBypassable());
        assertFalse(profile.isMetered());
        assertEquals(TEST_MTU, profile.getMaxMtu());
        assertFalse(profile.isRestrictedToTestNetworks());
    }

    @Test
    public void testBuildIkev2VpnProfilePsk() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        final Ikev2VpnProfile profile =
                buildIkev2VpnProfilePsk(false /* isRestrictedToTestNetworks */);

        checkBasicIkev2VpnProfile(profile);
        assertArrayEquals(TEST_PSK, profile.getPresharedKey());

        // Verify nothing else is set.
        assertNull(profile.getUsername());
        assertNull(profile.getPassword());
        assertNull(profile.getServerRootCaCert());
        assertNull(profile.getRsaPrivateKey());
        assertNull(profile.getUserCert());
    }

    @Test
    public void testBuildIkev2VpnProfileUsernamePassword() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        final Ikev2VpnProfile profile =
                buildIkev2VpnProfileUsernamePassword(false /* isRestrictedToTestNetworks */);

        checkBasicIkev2VpnProfile(profile);
        assertEquals(TEST_USER, profile.getUsername());
        assertEquals(TEST_PASSWORD, profile.getPassword());
        assertEquals(mServerRootCa, profile.getServerRootCaCert());

        // Verify nothing else is set.
        assertNull(profile.getPresharedKey());
        assertNull(profile.getRsaPrivateKey());
        assertNull(profile.getUserCert());
    }

    @Test
    public void testBuildIkev2VpnProfileDigitalSignature() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        final Ikev2VpnProfile profile =
                buildIkev2VpnProfileDigitalSignature(false /* isRestrictedToTestNetworks */);

        checkBasicIkev2VpnProfile(profile);
        assertEquals(mUserCertKey.cert, profile.getUserCert());
        assertEquals(mUserCertKey.key, profile.getRsaPrivateKey());
        assertEquals(mServerRootCa, profile.getServerRootCaCert());

        // Verify nothing else is set.
        assertNull(profile.getUsername());
        assertNull(profile.getPassword());
        assertNull(profile.getPresharedKey());
    }

    private void verifyProvisionVpnProfile(
            boolean hasActivateVpn, boolean hasActivatePlatformVpn, boolean expectIntent)
            throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        setAppop(AppOpsManager.OP_ACTIVATE_VPN, hasActivateVpn);
        setAppop(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, hasActivatePlatformVpn);

        final Ikev2VpnProfile profile =
                buildIkev2VpnProfilePsk(false /* isRestrictedToTestNetworks */);
        final Intent intent = sVpnMgr.provisionVpnProfile(profile);
        assertEquals(expectIntent, intent != null);
    }

    @Test
    public void testProvisionVpnProfileNoPreviousConsent() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        verifyProvisionVpnProfile(false /* hasActivateVpn */,
                false /* hasActivatePlatformVpn */, true /* expectIntent */);
    }

    @Test
    public void testProvisionVpnProfilePlatformVpnConsented() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        verifyProvisionVpnProfile(false /* hasActivateVpn */,
                true /* hasActivatePlatformVpn */, false /* expectIntent */);
    }

    @Test
    public void testProvisionVpnProfileVpnServiceConsented() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        verifyProvisionVpnProfile(true /* hasActivateVpn */,
                false /* hasActivatePlatformVpn */, false /* expectIntent */);
    }

    @Test
    public void testProvisionVpnProfileAllPreConsented() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        verifyProvisionVpnProfile(true /* hasActivateVpn */,
                true /* hasActivatePlatformVpn */, false /* expectIntent */);
    }

    @Test
    public void testDeleteVpnProfile() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        setAppop(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, true);

        final Ikev2VpnProfile profile =
                buildIkev2VpnProfilePsk(false /* isRestrictedToTestNetworks */);
        assertNull(sVpnMgr.provisionVpnProfile(profile));

        // Verify that deleting the profile works (even without the appop)
        setAppop(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, false);
        sVpnMgr.deleteProvisionedVpnProfile();

        // Test that the profile was deleted - starting it should throw an IAE.
        try {
            setAppop(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, true);
            sVpnMgr.startProvisionedVpnProfile();
            fail("Expected IllegalArgumentException due to missing profile");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testStartVpnProfileNoPreviousConsent() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        setAppop(AppOpsManager.OP_ACTIVATE_VPN, false);
        setAppop(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, false);

        // Make sure the VpnProfile is not provisioned already.
        sVpnMgr.stopProvisionedVpnProfile();

        try {
            sVpnMgr.startProvisionedVpnProfile();
            fail("Expected SecurityException for missing consent");
        } catch (SecurityException expected) {
        }
    }

    private void checkStartStopVpnProfileBuildsNetworks(IkeTunUtils tunUtils, boolean testIpv6)
            throws Exception {
        String serverAddr = testIpv6 ? TEST_SERVER_ADDR_V6 : TEST_SERVER_ADDR_V4;
        String initResp = testIpv6 ? SUCCESSFUL_IKE_INIT_RESP_V6 : SUCCESSFUL_IKE_INIT_RESP_V4;
        String authResp = testIpv6 ? SUCCESSFUL_IKE_AUTH_RESP_V6 : SUCCESSFUL_IKE_AUTH_RESP_V4;
        boolean hasNat = !testIpv6;

        // Requires MANAGE_TEST_NETWORKS to provision a test-mode profile.
        mCtsNetUtils.setAppopPrivileged(AppOpsManager.OP_ACTIVATE_PLATFORM_VPN, true);

        final Ikev2VpnProfile profile =
                buildIkev2VpnProfilePsk(serverAddr, true /* isRestrictedToTestNetworks */);
        assertNull(sVpnMgr.provisionVpnProfile(profile));

        sVpnMgr.startProvisionedVpnProfile();

        // Inject IKE negotiation
        int expectedMsgId = 0;
        tunUtils.awaitReqAndInjectResp(IKE_INITIATOR_SPI, expectedMsgId++, false /* isEncap */,
                HexDump.hexStringToByteArray(initResp));
        tunUtils.awaitReqAndInjectResp(IKE_INITIATOR_SPI, expectedMsgId++, hasNat /* isEncap */,
                HexDump.hexStringToByteArray(authResp));

        // Verify the VPN network came up
        final NetworkRequest nr = new NetworkRequest.Builder()
                .clearCapabilities().addTransportType(TRANSPORT_VPN).build();

        final TestNetworkCallback cb = new TestNetworkCallback();
        sCM.requestNetwork(nr, cb);
        cb.waitForAvailable();
        final Network vpnNetwork = cb.currentNetwork;
        assertNotNull(vpnNetwork);

        final NetworkCapabilities caps = sCM.getNetworkCapabilities(vpnNetwork);
        assertTrue(caps.hasTransport(TRANSPORT_VPN));
        assertTrue(caps.hasCapability(NET_CAPABILITY_INTERNET));
        assertEquals(Process.myUid(), caps.getOwnerUid());

        sVpnMgr.stopProvisionedVpnProfile();
        cb.waitForLost();
        assertEquals(vpnNetwork, cb.lastLostNetwork);
    }

    private void doTestStartStopVpnProfile(boolean testIpv6) throws Exception {
        // Non-final; these variables ensure we clean up properly after our test if we have
        // allocated test network resources
        final TestNetworkManager tnm = sContext.getSystemService(TestNetworkManager.class);
        TestNetworkInterface testIface = null;
        TestNetworkCallback tunNetworkCallback = null;

        try {
            // Build underlying test network
            testIface = tnm.createTunInterface(
                    new LinkAddress[] {
                            new LinkAddress(LOCAL_OUTER_4, IP4_PREFIX_LEN),
                            new LinkAddress(LOCAL_OUTER_6, IP6_PREFIX_LEN)});

            // Hold on to this callback to ensure network does not get reaped.
            tunNetworkCallback = mCtsNetUtils.setupAndGetTestNetwork(
                    testIface.getInterfaceName());
            final IkeTunUtils tunUtils = new IkeTunUtils(testIface.getFileDescriptor());

            checkStartStopVpnProfileBuildsNetworks(tunUtils, testIpv6);
        } finally {
            // Make sure to stop the VPN profile. This is safe to call multiple times.
            sVpnMgr.stopProvisionedVpnProfile();

            if (testIface != null) {
                testIface.getFileDescriptor().close();
            }

            if (tunNetworkCallback != null) {
                sCM.unregisterNetworkCallback(tunNetworkCallback);
            }

            final Network testNetwork = tunNetworkCallback.currentNetwork;
            if (testNetwork != null) {
                tnm.teardownTestNetwork(testNetwork);
            }
        }
    }

    @Test
    public void testStartStopVpnProfileV4() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        // Requires shell permission to update appops.
        runWithShellPermissionIdentity(() -> {
            doTestStartStopVpnProfile(false);
        });
    }

    @Test
    public void testStartStopVpnProfileV6() throws Exception {
        assumeTrue(mCtsNetUtils.hasIpsecTunnelsFeature());

        // Requires shell permission to update appops.
        runWithShellPermissionIdentity(() -> {
            doTestStartStopVpnProfile(true);
        });
    }

    private static class CertificateAndKey {
        public final X509Certificate cert;
        public final PrivateKey key;

        CertificateAndKey(X509Certificate cert, PrivateKey key) {
            this.cert = cert;
            this.key = key;
        }
    }

    private static CertificateAndKey generateRandomCertAndKeyPair() throws Exception {
        final Date validityBeginDate =
                new Date(System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1L));
        final Date validityEndDate =
                new Date(System.currentTimeMillis() + TimeUnit.DAYS.toMillis(1L));

        // Generate a keypair
        final KeyPairGenerator keyPairGenerator = KeyPairGenerator.getInstance("RSA");
        keyPairGenerator.initialize(512);
        final KeyPair keyPair = keyPairGenerator.generateKeyPair();

        final X500Principal dnName = new X500Principal("CN=test.android.com");
        final X509V1CertificateGenerator certGen = new X509V1CertificateGenerator();
        certGen.setSerialNumber(BigInteger.valueOf(System.currentTimeMillis()));
        certGen.setSubjectDN(dnName);
        certGen.setIssuerDN(dnName);
        certGen.setNotBefore(validityBeginDate);
        certGen.setNotAfter(validityEndDate);
        certGen.setPublicKey(keyPair.getPublic());
        certGen.setSignatureAlgorithm("SHA256WithRSAEncryption");

        final X509Certificate cert = certGen.generate(keyPair.getPrivate(), "AndroidOpenSSL");
        return new CertificateAndKey(cert, keyPair.getPrivate());
    }
}
