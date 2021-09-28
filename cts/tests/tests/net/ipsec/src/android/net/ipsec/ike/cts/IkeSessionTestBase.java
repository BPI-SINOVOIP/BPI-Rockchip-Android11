/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.net.ipsec.ike.cts;

import static android.net.ipsec.ike.IkeSessionConfiguration.EXTENSION_TYPE_FRAGMENTATION;
import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.annotation.NonNull;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.InetAddresses;
import android.net.IpSecManager;
import android.net.IpSecTransform;
import android.net.LinkAddress;
import android.net.Network;
import android.net.TestNetworkInterface;
import android.net.TestNetworkManager;
import android.net.annotations.PolicyDirection;
import android.net.ipsec.ike.ChildSessionCallback;
import android.net.ipsec.ike.ChildSessionConfiguration;
import android.net.ipsec.ike.IkeSessionCallback;
import android.net.ipsec.ike.IkeSessionConfiguration;
import android.net.ipsec.ike.IkeSessionConnectionInfo;
import android.net.ipsec.ike.IkeTrafficSelector;
import android.net.ipsec.ike.TransportModeChildSessionParams;
import android.net.ipsec.ike.TunnelModeChildSessionParams;
import android.net.ipsec.ike.cts.IkeTunUtils.PortPair;
import android.net.ipsec.ike.cts.TestNetworkUtils.TestNetworkCallback;
import android.net.ipsec.ike.exceptions.IkeException;
import android.net.ipsec.ike.exceptions.IkeProtocolException;
import android.os.Binder;
import android.os.ParcelFileDescriptor;
import android.platform.test.annotations.AppModeFull;

import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;
import com.android.testutils.ArrayTrackRecord;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.runner.RunWith;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Package private base class for testing IkeSessionParams and IKE exchanges.
 *
 * <p>Subclasses MUST explicitly call #setUpTestNetwork and #tearDownTestNetwork to be able to use
 * the test network
 *
 * <p>All IKE Sessions running in test mode will generate SPIs deterministically. That is to say
 * each IKE Session will always generate the same IKE INIT SPI and test vectors are generated based
 * on this deterministic IKE SPI. Each test will use different local and remote addresses to avoid
 * the case that the next test try to allocate the same SPI before the previous test has released
 * it, since SPI resources are not released in testing thread. Similarly, each test MUST use
 * different Network instances to avoid sharing the same IkeSocket and hitting IKE SPI collision.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "MANAGE_TEST_NETWORKS permission can't be granted to instant apps")
abstract class IkeSessionTestBase extends IkeTestBase {
    // Package-wide common expected results that will be shared by all IKE/Child SA creation tests
    static final String EXPECTED_REMOTE_APP_VERSION_EMPTY = "";
    static final byte[] EXPECTED_PROTOCOL_ERROR_DATA_NONE = new byte[0];

    static final InetAddress EXPECTED_DNS_SERVERS_ONE =
            InetAddresses.parseNumericAddress("8.8.8.8");
    static final InetAddress EXPECTED_DNS_SERVERS_TWO =
            InetAddresses.parseNumericAddress("8.8.4.4");

    static final InetAddress EXPECTED_INTERNAL_ADDR =
            InetAddresses.parseNumericAddress("198.51.100.10");
    static final LinkAddress EXPECTED_INTERNAL_LINK_ADDR =
            new LinkAddress(EXPECTED_INTERNAL_ADDR, IP4_PREFIX_LEN);
    static final InetAddress EXPECTED_INTERNAL_ADDR_V6 =
            InetAddresses.parseNumericAddress("2001:db8::2");
    static final LinkAddress EXPECTED_INTERNAL_LINK_ADDR_V6 =
            new LinkAddress(EXPECTED_INTERNAL_ADDR_V6, IP6_PREFIX_LEN);

    static final IkeTrafficSelector TUNNEL_MODE_INBOUND_TS =
            new IkeTrafficSelector(
                    MIN_PORT, MAX_PORT, EXPECTED_INTERNAL_ADDR, EXPECTED_INTERNAL_ADDR);
    static final IkeTrafficSelector TUNNEL_MODE_OUTBOUND_TS = DEFAULT_V4_TS;
    static final IkeTrafficSelector TUNNEL_MODE_INBOUND_TS_V6 =
            new IkeTrafficSelector(
                    MIN_PORT, MAX_PORT, EXPECTED_INTERNAL_ADDR_V6, EXPECTED_INTERNAL_ADDR_V6);
    static final IkeTrafficSelector TUNNEL_MODE_OUTBOUND_TS_V6 = DEFAULT_V6_TS;

    // This value is align with the test vectors hex that are generated in an IPv4 environment
    static final IkeTrafficSelector TRANSPORT_MODE_OUTBOUND_TS =
            new IkeTrafficSelector(
                    MIN_PORT,
                    MAX_PORT,
                    InetAddresses.parseNumericAddress("10.138.0.2"),
                    InetAddresses.parseNumericAddress("10.138.0.2"));

    static final long IKE_DETERMINISTIC_INITIATOR_SPI = Long.parseLong("46B8ECA1E0D72A18", 16);

    // Static state to reduce setup/teardown
    static Context sContext = InstrumentationRegistry.getContext();
    static ConnectivityManager sCM =
            (ConnectivityManager) sContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    static TestNetworkManager sTNM;

    private static final int TIMEOUT_MS = 500;

    // Constants to be used for providing different IP addresses for each tests
    private static final byte IP_ADDR_LAST_BYTE_MAX = (byte) 100;
    private static final byte[] INITIAL_AVAILABLE_IP4_ADDR_LOCAL =
            InetAddresses.parseNumericAddress("192.0.2.1").getAddress();
    private static final byte[] INITIAL_AVAILABLE_IP4_ADDR_REMOTE =
            InetAddresses.parseNumericAddress("198.51.100.1").getAddress();
    private static final byte[] NEXT_AVAILABLE_IP4_ADDR_LOCAL = INITIAL_AVAILABLE_IP4_ADDR_LOCAL;
    private static final byte[] NEXT_AVAILABLE_IP4_ADDR_REMOTE = INITIAL_AVAILABLE_IP4_ADDR_REMOTE;

    ParcelFileDescriptor mTunFd;
    TestNetworkCallback mTunNetworkCallback;
    Network mTunNetwork;
    IkeTunUtils mTunUtils;

    InetAddress mLocalAddress;
    InetAddress mRemoteAddress;

    Executor mUserCbExecutor;
    TestIkeSessionCallback mIkeSessionCallback;
    TestChildSessionCallback mFirstChildSessionCallback;

    // This method is guaranteed to run in subclasses and will run before subclasses' @BeforeClass
    // methods.
    @BeforeClass
    public static void setUpPermissionBeforeClass() throws Exception {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .adoptShellPermissionIdentity();
        sTNM = sContext.getSystemService(TestNetworkManager.class);
    }

    // This method is guaranteed to run in subclasses and will run after subclasses' @AfterClass
    // methods.
    @AfterClass
    public static void tearDownPermissionAfterClass() throws Exception {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Before
    public void setUp() throws Exception {
        mLocalAddress = getNextAvailableIpv4AddressLocal();
        mRemoteAddress = getNextAvailableIpv4AddressRemote();
        setUpTestNetwork(mLocalAddress);

        mUserCbExecutor = Executors.newSingleThreadExecutor();
        mIkeSessionCallback = new TestIkeSessionCallback();
        mFirstChildSessionCallback = new TestChildSessionCallback();
    }

    @After
    public void tearDown() throws Exception {
        tearDownTestNetwork();
    }

    void setUpTestNetwork(InetAddress localAddr) throws Exception {
        int prefixLen = localAddr instanceof Inet4Address ? IP4_PREFIX_LEN : IP6_PREFIX_LEN;

        TestNetworkInterface testIface =
                sTNM.createTunInterface(new LinkAddress[] {new LinkAddress(localAddr, prefixLen)});

        mTunFd = testIface.getFileDescriptor();
        mTunNetworkCallback =
                TestNetworkUtils.setupAndGetTestNetwork(
                        sCM, sTNM, testIface.getInterfaceName(), new Binder());
        mTunNetwork = mTunNetworkCallback.getNetworkBlocking();
        mTunUtils = new IkeTunUtils(mTunFd);
    }

    void tearDownTestNetwork() throws Exception {
        sCM.unregisterNetworkCallback(mTunNetworkCallback);

        sTNM.teardownTestNetwork(mTunNetwork);
        mTunFd.close();
    }

    static void setAppOp(int appop, boolean allow) {
        String opName = AppOpsManager.opToName(appop);
        for (String pkg : new String[] {"com.android.shell", sContext.getPackageName()}) {
            String cmd =
                    String.format(
                            "appops set %s %s %s",
                            pkg, // Package name
                            opName, // Appop
                            (allow ? "allow" : "deny")); // Action

            SystemUtil.runShellCommand(cmd);
        }
    }

    Inet4Address getNextAvailableIpv4AddressLocal() throws Exception {
        return (Inet4Address)
                getNextAvailableAddress(
                        NEXT_AVAILABLE_IP4_ADDR_LOCAL,
                        INITIAL_AVAILABLE_IP4_ADDR_LOCAL,
                        false /* isIp6 */);
    }

    Inet4Address getNextAvailableIpv4AddressRemote() throws Exception {
        return (Inet4Address)
                getNextAvailableAddress(
                        NEXT_AVAILABLE_IP4_ADDR_REMOTE,
                        INITIAL_AVAILABLE_IP4_ADDR_REMOTE,
                        false /* isIp6 */);
    }

    InetAddress getNextAvailableAddress(
            byte[] nextAddressBytes, byte[] initialAddressBytes, boolean isIp6) throws Exception {
        int addressLen = isIp6 ? IP6_ADDRESS_LEN : IP4_ADDRESS_LEN;

        synchronized (nextAddressBytes) {
            if (nextAddressBytes[addressLen - 1] == IP_ADDR_LAST_BYTE_MAX) {
                resetNextAvailableAddress(nextAddressBytes, initialAddressBytes);
            }

            InetAddress address = InetAddress.getByAddress(nextAddressBytes);
            nextAddressBytes[addressLen - 1]++;
            return address;
        }
    }

    private void resetNextAvailableAddress(byte[] nextAddressBytes, byte[] initialAddressBytes) {
        synchronized (nextAddressBytes) {
            System.arraycopy(
                    nextAddressBytes, 0, initialAddressBytes, 0, initialAddressBytes.length);
        }
    }

    TransportModeChildSessionParams buildTransportModeChildParamsWithTs(
            IkeTrafficSelector inboundTs, IkeTrafficSelector outboundTs) {
        return new TransportModeChildSessionParams.Builder()
                .addSaProposal(SaProposalTest.buildChildSaProposalWithCombinedModeCipher())
                .addSaProposal(SaProposalTest.buildChildSaProposalWithNormalModeCipher())
                .addInboundTrafficSelectors(inboundTs)
                .addOutboundTrafficSelectors(outboundTs)
                .build();
    }

    TransportModeChildSessionParams buildTransportModeChildParamsWithDefaultTs() {
        return new TransportModeChildSessionParams.Builder()
                .addSaProposal(SaProposalTest.buildChildSaProposalWithCombinedModeCipher())
                .addSaProposal(SaProposalTest.buildChildSaProposalWithNormalModeCipher())
                .build();
    }

    TunnelModeChildSessionParams buildTunnelModeChildSessionParams() {
        return new TunnelModeChildSessionParams.Builder()
                .addSaProposal(SaProposalTest.buildChildSaProposalWithNormalModeCipher())
                .addSaProposal(SaProposalTest.buildChildSaProposalWithCombinedModeCipher())
                .addInternalAddressRequest(AF_INET)
                .addInternalAddressRequest(AF_INET6)
                .build();
    }

    PortPair performSetupIkeAndFirstChildBlocking(String ikeInitRespHex, String... ikeAuthRespHexes)
            throws Exception {
        return performSetupIkeAndFirstChildBlocking(
                ikeInitRespHex,
                1 /* expectedAuthReqPktCnt */,
                true /*expectedAuthUseEncap*/,
                ikeAuthRespHexes);
    }

    PortPair performSetupIkeAndFirstChildBlocking(
            String ikeInitRespHex, boolean expectedAuthUseEncap, String... ikeAuthRespHexes)
            throws Exception {
        return performSetupIkeAndFirstChildBlocking(
                ikeInitRespHex,
                1 /* expectedAuthReqPktCnt */,
                expectedAuthUseEncap,
                ikeAuthRespHexes);
    }

    PortPair performSetupIkeAndFirstChildBlocking(
            String ikeInitRespHex,
            int expectedAuthReqPktCnt,
            boolean expectedAuthUseEncap,
            String... ikeAuthRespHexes)
            throws Exception {
        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI,
                0 /* expectedMsgId */,
                false /* expectedUseEncap */,
                ikeInitRespHex);

        byte[] ikeAuthReqPkt =
                mTunUtils
                        .awaitReqAndInjectResp(
                                IKE_DETERMINISTIC_INITIATOR_SPI,
                                1 /* expectedMsgId */,
                                expectedAuthUseEncap,
                                expectedAuthReqPktCnt,
                                ikeAuthRespHexes)
                        .get(0);
        return IkeTunUtils.getSrcDestPortPair(ikeAuthReqPkt);
    }

    void performCloseIkeBlocking(int expectedMsgId, String deleteIkeRespHex) throws Exception {
        performCloseIkeBlocking(expectedMsgId, true /* expectedUseEncap*/, deleteIkeRespHex);
    }

    void performCloseIkeBlocking(
            int expectedMsgId, boolean expectedUseEncap, String deleteIkeRespHex) throws Exception {
        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI, expectedMsgId, expectedUseEncap, deleteIkeRespHex);
    }

    /** Testing callback that allows caller to block current thread until a method get called */
    static class TestIkeSessionCallback implements IkeSessionCallback {
        private CompletableFuture<IkeSessionConfiguration> mFutureIkeConfig =
                new CompletableFuture<>();
        private CompletableFuture<Boolean> mFutureOnClosedCall = new CompletableFuture<>();
        private CompletableFuture<IkeException> mFutureOnClosedException =
                new CompletableFuture<>();

        private int mOnErrorExceptionsCount = 0;
        private ArrayTrackRecord<IkeProtocolException> mOnErrorExceptionsTrackRecord =
                new ArrayTrackRecord<>();

        @Override
        public void onOpened(@NonNull IkeSessionConfiguration sessionConfiguration) {
            mFutureIkeConfig.complete(sessionConfiguration);
        }

        @Override
        public void onClosed() {
            mFutureOnClosedCall.complete(true /* unused */);
        }

        @Override
        public void onClosedExceptionally(@NonNull IkeException exception) {
            mFutureOnClosedException.complete(exception);
        }

        @Override
        public void onError(@NonNull IkeProtocolException exception) {
            mOnErrorExceptionsTrackRecord.add(exception);
        }

        public IkeSessionConfiguration awaitIkeConfig() throws Exception {
            return mFutureIkeConfig.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public IkeException awaitOnClosedException() throws Exception {
            return mFutureOnClosedException.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public IkeProtocolException awaitNextOnErrorException() {
            return mOnErrorExceptionsTrackRecord.poll(
                    (long) TIMEOUT_MS,
                    mOnErrorExceptionsCount++,
                    (transform) -> {
                        return true;
                    });
        }

        public void awaitOnClosed() throws Exception {
            mFutureOnClosedCall.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }
    }

    /** Testing callback that allows caller to block current thread until a method get called */
    static class TestChildSessionCallback implements ChildSessionCallback {
        private CompletableFuture<ChildSessionConfiguration> mFutureChildConfig =
                new CompletableFuture<>();
        private CompletableFuture<Boolean> mFutureOnClosedCall = new CompletableFuture<>();
        private CompletableFuture<IkeException> mFutureOnClosedException =
                new CompletableFuture<>();

        private int mCreatedIpSecTransformCount = 0;
        private int mDeletedIpSecTransformCount = 0;
        private ArrayTrackRecord<IpSecTransformCallRecord> mCreatedIpSecTransformsTrackRecord =
                new ArrayTrackRecord<>();
        private ArrayTrackRecord<IpSecTransformCallRecord> mDeletedIpSecTransformsTrackRecord =
                new ArrayTrackRecord<>();

        @Override
        public void onOpened(@NonNull ChildSessionConfiguration sessionConfiguration) {
            mFutureChildConfig.complete(sessionConfiguration);
        }

        @Override
        public void onClosed() {
            mFutureOnClosedCall.complete(true /* unused */);
        }

        @Override
        public void onClosedExceptionally(@NonNull IkeException exception) {
            mFutureOnClosedException.complete(exception);
        }

        @Override
        public void onIpSecTransformCreated(@NonNull IpSecTransform ipSecTransform, int direction) {
            mCreatedIpSecTransformsTrackRecord.add(
                    new IpSecTransformCallRecord(ipSecTransform, direction));
        }

        @Override
        public void onIpSecTransformDeleted(@NonNull IpSecTransform ipSecTransform, int direction) {
            mDeletedIpSecTransformsTrackRecord.add(
                    new IpSecTransformCallRecord(ipSecTransform, direction));
        }

        public ChildSessionConfiguration awaitChildConfig() throws Exception {
            return mFutureChildConfig.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public IkeException awaitOnClosedException() throws Exception {
            return mFutureOnClosedException.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public IpSecTransformCallRecord awaitNextCreatedIpSecTransform() {
            return mCreatedIpSecTransformsTrackRecord.poll(
                    (long) TIMEOUT_MS,
                    mCreatedIpSecTransformCount++,
                    (transform) -> {
                        return true;
                    });
        }

        public IpSecTransformCallRecord awaitNextDeletedIpSecTransform() {
            return mDeletedIpSecTransformsTrackRecord.poll(
                    (long) TIMEOUT_MS,
                    mDeletedIpSecTransformCount++,
                    (transform) -> {
                        return true;
                    });
        }

        public void awaitOnClosed() throws Exception {
            mFutureOnClosedCall.get(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }
    }

    /**
     * This class represents a created or deleted IpSecTransfrom that is provided by
     * ChildSessionCallback
     */
    static class IpSecTransformCallRecord {
        public final IpSecTransform ipSecTransform;
        public final int direction;

        IpSecTransformCallRecord(IpSecTransform ipSecTransform, @PolicyDirection int direction) {
            this.ipSecTransform = ipSecTransform;
            this.direction = direction;
        }

        @Override
        public int hashCode() {
            return Objects.hash(ipSecTransform, direction);
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof IpSecTransformCallRecord)) return false;

            IpSecTransformCallRecord record = (IpSecTransformCallRecord) o;
            return ipSecTransform.equals(record.ipSecTransform) && direction == record.direction;
        }
    }

    void verifyIkeSessionSetupBlocking() throws Exception {
        IkeSessionConfiguration ikeConfig = mIkeSessionCallback.awaitIkeConfig();
        assertNotNull(ikeConfig);
        assertEquals(EXPECTED_REMOTE_APP_VERSION_EMPTY, ikeConfig.getRemoteApplicationVersion());
        assertTrue(ikeConfig.getRemoteVendorIds().isEmpty());
        assertTrue(ikeConfig.getPcscfServers().isEmpty());
        assertTrue(ikeConfig.isIkeExtensionEnabled(EXTENSION_TYPE_FRAGMENTATION));

        IkeSessionConnectionInfo ikeConnectInfo = ikeConfig.getIkeSessionConnectionInfo();
        assertNotNull(ikeConnectInfo);
        assertEquals(mLocalAddress, ikeConnectInfo.getLocalAddress());
        assertEquals(mRemoteAddress, ikeConnectInfo.getRemoteAddress());
        assertEquals(mTunNetwork, ikeConnectInfo.getNetwork());
    }

    void verifyChildSessionSetupBlocking(
            TestChildSessionCallback childCallback,
            List<IkeTrafficSelector> expectedInboundTs,
            List<IkeTrafficSelector> expectedOutboundTs,
            List<LinkAddress> expectedInternalAddresses)
            throws Exception {
        verifyChildSessionSetupBlocking(
                childCallback,
                expectedInboundTs,
                expectedOutboundTs,
                expectedInternalAddresses,
                new ArrayList<InetAddress>() /* expectedDnsServers */);
    }

    void verifyChildSessionSetupBlocking(
            TestChildSessionCallback childCallback,
            List<IkeTrafficSelector> expectedInboundTs,
            List<IkeTrafficSelector> expectedOutboundTs,
            List<LinkAddress> expectedInternalAddresses,
            List<InetAddress> expectedDnsServers)
            throws Exception {
        ChildSessionConfiguration childConfig = childCallback.awaitChildConfig();
        assertNotNull(childConfig);
        assertEquals(expectedInboundTs, childConfig.getInboundTrafficSelectors());
        assertEquals(expectedOutboundTs, childConfig.getOutboundTrafficSelectors());
        assertEquals(expectedInternalAddresses, childConfig.getInternalAddresses());
        assertEquals(expectedDnsServers, childConfig.getInternalDnsServers());
        assertTrue(childConfig.getInternalSubnets().isEmpty());
        assertTrue(childConfig.getInternalDhcpServers().isEmpty());
    }

    void verifyCloseIkeAndChildBlocking(
            IpSecTransformCallRecord expectedTransformRecordA,
            IpSecTransformCallRecord expectedTransformRecordB)
            throws Exception {
        verifyDeleteIpSecTransformPair(
                mFirstChildSessionCallback, expectedTransformRecordA, expectedTransformRecordB);
        mFirstChildSessionCallback.awaitOnClosed();
        mIkeSessionCallback.awaitOnClosed();
    }

    static void verifyCreateIpSecTransformPair(
            IpSecTransformCallRecord transformRecordA, IpSecTransformCallRecord transformRecordB) {
        IpSecTransform transformA = transformRecordA.ipSecTransform;
        IpSecTransform transformB = transformRecordB.ipSecTransform;

        assertNotNull(transformA);
        assertNotNull(transformB);

        Set<Integer> expectedDirections = new HashSet<>();
        expectedDirections.add(IpSecManager.DIRECTION_IN);
        expectedDirections.add(IpSecManager.DIRECTION_OUT);

        Set<Integer> resultDirections = new HashSet<>();
        resultDirections.add(transformRecordA.direction);
        resultDirections.add(transformRecordB.direction);

        assertEquals(expectedDirections, resultDirections);
    }

    static void verifyDeleteIpSecTransformPair(
            TestChildSessionCallback childCb,
            IpSecTransformCallRecord expectedTransformRecordA,
            IpSecTransformCallRecord expectedTransformRecordB) {
        Set<IpSecTransformCallRecord> expectedTransforms = new HashSet<>();
        expectedTransforms.add(expectedTransformRecordA);
        expectedTransforms.add(expectedTransformRecordB);

        Set<IpSecTransformCallRecord> resultTransforms = new HashSet<>();
        resultTransforms.add(childCb.awaitNextDeletedIpSecTransform());
        resultTransforms.add(childCb.awaitNextDeletedIpSecTransform());

        assertEquals(expectedTransforms, resultTransforms);
    }

    /** Package private method to check if device has IPsec tunnels feature */
    static boolean hasTunnelsFeature() {
        return sContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_IPSEC_TUNNELS);
    }

    // TODO(b/148689509): Verify hostname based creation
}
