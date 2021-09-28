/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.net;

import static android.net.NetworkCapabilities.LINK_BANDWIDTH_UNSPECIFIED;
import static android.net.NetworkCapabilities.MAX_TRANSPORT;
import static android.net.NetworkCapabilities.MIN_TRANSPORT;
import static android.net.NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL;
import static android.net.NetworkCapabilities.NET_CAPABILITY_CBS;
import static android.net.NetworkCapabilities.NET_CAPABILITY_EIMS;
import static android.net.NetworkCapabilities.NET_CAPABILITY_FOREGROUND;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_MMS;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_METERED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_VPN;
import static android.net.NetworkCapabilities.NET_CAPABILITY_OEM_PAID;
import static android.net.NetworkCapabilities.NET_CAPABILITY_PARTIAL_CONNECTIVITY;
import static android.net.NetworkCapabilities.NET_CAPABILITY_VALIDATED;
import static android.net.NetworkCapabilities.NET_CAPABILITY_WIFI_P2P;
import static android.net.NetworkCapabilities.RESTRICTED_CAPABILITIES;
import static android.net.NetworkCapabilities.SIGNAL_STRENGTH_UNSPECIFIED;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.net.NetworkCapabilities.TRANSPORT_TEST;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI_AWARE;
import static android.net.NetworkCapabilities.UNRESTRICTED_CAPABILITIES;

import static com.android.testutils.ParcelUtilsKt.assertParcelSane;
import static com.android.testutils.ParcelUtilsKt.assertParcelingIsLossless;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.wifi.aware.DiscoverySession;
import android.net.wifi.aware.PeerHandle;
import android.net.wifi.aware.WifiAwareNetworkSpecifier;
import android.os.Build;
import android.os.Process;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.ArraySet;

import androidx.core.os.BuildCompat;
import androidx.test.runner.AndroidJUnit4;

import com.android.testutils.DevSdkIgnoreRule;
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import java.util.Arrays;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class NetworkCapabilitiesTest {
    private static final String TEST_SSID = "TEST_SSID";
    private static final String DIFFERENT_TEST_SSID = "DIFFERENT_TEST_SSID";

    @Rule
    public DevSdkIgnoreRule mDevSdkIgnoreRule = new DevSdkIgnoreRule();

    private DiscoverySession mDiscoverySession = Mockito.mock(DiscoverySession.class);
    private PeerHandle mPeerHandle = Mockito.mock(PeerHandle.class);

    private boolean isAtLeastR() {
        // BuildCompat.isAtLeastR() is used to check the Android version before releasing Android R.
        // Build.VERSION.SDK_INT > Build.VERSION_CODES.Q is used to check the Android version after
        // releasing Android R.
        return BuildCompat.isAtLeastR() || Build.VERSION.SDK_INT > Build.VERSION_CODES.Q;
    }

    @Test
    public void testMaybeMarkCapabilitiesRestricted() {
        // verify EIMS is restricted
        assertEquals((1 << NET_CAPABILITY_EIMS) & RESTRICTED_CAPABILITIES,
                (1 << NET_CAPABILITY_EIMS));

        // verify CBS is also restricted
        assertEquals((1 << NET_CAPABILITY_CBS) & RESTRICTED_CAPABILITIES,
                (1 << NET_CAPABILITY_CBS));

        // verify default is not restricted
        assertEquals((1 << NET_CAPABILITY_INTERNET) & RESTRICTED_CAPABILITIES, 0);

        // just to see
        assertEquals(RESTRICTED_CAPABILITIES & UNRESTRICTED_CAPABILITIES, 0);

        // check that internet does not get restricted
        NetworkCapabilities netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_INTERNET);
        netCap.maybeMarkCapabilitiesRestricted();
        assertTrue(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // metered-ness shouldn't matter
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_INTERNET);
        netCap.addCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertTrue(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_INTERNET);
        netCap.removeCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertTrue(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // add EIMS - bundled with unrestricted means it's unrestricted
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_INTERNET);
        netCap.addCapability(NET_CAPABILITY_EIMS);
        netCap.addCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertTrue(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_INTERNET);
        netCap.addCapability(NET_CAPABILITY_EIMS);
        netCap.removeCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertTrue(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // just a restricted cap should be restricted regardless of meteredness
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_EIMS);
        netCap.addCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertFalse(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_EIMS);
        netCap.removeCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertFalse(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // try 2 restricted caps
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_CBS);
        netCap.addCapability(NET_CAPABILITY_EIMS);
        netCap.addCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertFalse(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));
        netCap = new NetworkCapabilities();
        netCap.addCapability(NET_CAPABILITY_CBS);
        netCap.addCapability(NET_CAPABILITY_EIMS);
        netCap.removeCapability(NET_CAPABILITY_NOT_METERED);
        netCap.maybeMarkCapabilitiesRestricted();
        assertFalse(netCap.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));
    }

    @Test
    public void testDescribeImmutableDifferences() {
        NetworkCapabilities nc1;
        NetworkCapabilities nc2;

        // Transports changing
        nc1 = new NetworkCapabilities().addTransportType(TRANSPORT_CELLULAR);
        nc2 = new NetworkCapabilities().addTransportType(TRANSPORT_WIFI);
        assertNotEquals("", nc1.describeImmutableDifferences(nc2));
        assertEquals("", nc1.describeImmutableDifferences(nc1));

        // Mutable capability changing
        nc1 = new NetworkCapabilities().addCapability(NET_CAPABILITY_VALIDATED);
        nc2 = new NetworkCapabilities();
        assertEquals("", nc1.describeImmutableDifferences(nc2));
        assertEquals("", nc1.describeImmutableDifferences(nc1));

        // NOT_METERED changing (http://b/63326103)
        nc1 = new NetworkCapabilities()
                .addCapability(NET_CAPABILITY_NOT_METERED)
                .addCapability(NET_CAPABILITY_INTERNET);
        nc2 = new NetworkCapabilities().addCapability(NET_CAPABILITY_INTERNET);
        assertEquals("", nc1.describeImmutableDifferences(nc2));
        assertEquals("", nc1.describeImmutableDifferences(nc1));

        // Immutable capability changing
        nc1 = new NetworkCapabilities()
                .addCapability(NET_CAPABILITY_INTERNET)
                .removeCapability(NET_CAPABILITY_NOT_RESTRICTED);
        nc2 = new NetworkCapabilities().addCapability(NET_CAPABILITY_INTERNET);
        assertNotEquals("", nc1.describeImmutableDifferences(nc2));
        assertEquals("", nc1.describeImmutableDifferences(nc1));

        // Specifier changing
        nc1 = new NetworkCapabilities().addTransportType(TRANSPORT_WIFI);
        nc2 = new NetworkCapabilities()
                .addTransportType(TRANSPORT_WIFI)
                .setNetworkSpecifier(new StringNetworkSpecifier("specs"));
        assertNotEquals("", nc1.describeImmutableDifferences(nc2));
        assertEquals("", nc1.describeImmutableDifferences(nc1));
    }

    @Test
    public void testLinkBandwidthUtils() {
        assertEquals(LINK_BANDWIDTH_UNSPECIFIED, NetworkCapabilities
                .minBandwidth(LINK_BANDWIDTH_UNSPECIFIED, LINK_BANDWIDTH_UNSPECIFIED));
        assertEquals(10, NetworkCapabilities
                .minBandwidth(LINK_BANDWIDTH_UNSPECIFIED, 10));
        assertEquals(10, NetworkCapabilities
                .minBandwidth(10, LINK_BANDWIDTH_UNSPECIFIED));
        assertEquals(10, NetworkCapabilities
                .minBandwidth(10, 20));

        assertEquals(LINK_BANDWIDTH_UNSPECIFIED, NetworkCapabilities
                .maxBandwidth(LINK_BANDWIDTH_UNSPECIFIED, LINK_BANDWIDTH_UNSPECIFIED));
        assertEquals(10, NetworkCapabilities
                .maxBandwidth(LINK_BANDWIDTH_UNSPECIFIED, 10));
        assertEquals(10, NetworkCapabilities
                .maxBandwidth(10, LINK_BANDWIDTH_UNSPECIFIED));
        assertEquals(20, NetworkCapabilities
                .maxBandwidth(10, 20));
    }

    @Test
    public void testSetUids() {
        final NetworkCapabilities netCap = new NetworkCapabilities();
        final Set<UidRange> uids = new ArraySet<>();
        uids.add(new UidRange(50, 100));
        uids.add(new UidRange(3000, 4000));
        netCap.setUids(uids);
        assertTrue(netCap.appliesToUid(50));
        assertTrue(netCap.appliesToUid(80));
        assertTrue(netCap.appliesToUid(100));
        assertTrue(netCap.appliesToUid(3000));
        assertTrue(netCap.appliesToUid(3001));
        assertFalse(netCap.appliesToUid(10));
        assertFalse(netCap.appliesToUid(25));
        assertFalse(netCap.appliesToUid(49));
        assertFalse(netCap.appliesToUid(101));
        assertFalse(netCap.appliesToUid(2000));
        assertFalse(netCap.appliesToUid(100000));

        assertTrue(netCap.appliesToUidRange(new UidRange(50, 100)));
        assertTrue(netCap.appliesToUidRange(new UidRange(70, 72)));
        assertTrue(netCap.appliesToUidRange(new UidRange(3500, 3912)));
        assertFalse(netCap.appliesToUidRange(new UidRange(1, 100)));
        assertFalse(netCap.appliesToUidRange(new UidRange(49, 100)));
        assertFalse(netCap.appliesToUidRange(new UidRange(1, 10)));
        assertFalse(netCap.appliesToUidRange(new UidRange(60, 101)));
        assertFalse(netCap.appliesToUidRange(new UidRange(60, 3400)));

        NetworkCapabilities netCap2 = new NetworkCapabilities();
        // A new netcap object has null UIDs, so anything will satisfy it.
        assertTrue(netCap2.satisfiedByUids(netCap));
        // Still not equal though.
        assertFalse(netCap2.equalsUids(netCap));
        netCap2.setUids(uids);
        assertTrue(netCap2.satisfiedByUids(netCap));
        assertTrue(netCap.equalsUids(netCap2));
        assertTrue(netCap2.equalsUids(netCap));

        uids.add(new UidRange(600, 700));
        netCap2.setUids(uids);
        assertFalse(netCap2.satisfiedByUids(netCap));
        assertFalse(netCap.appliesToUid(650));
        assertTrue(netCap2.appliesToUid(650));
        netCap.combineCapabilities(netCap2);
        assertTrue(netCap2.satisfiedByUids(netCap));
        assertTrue(netCap.appliesToUid(650));
        assertFalse(netCap.appliesToUid(500));

        assertTrue(new NetworkCapabilities().satisfiedByUids(netCap));
        netCap.combineCapabilities(new NetworkCapabilities());
        assertTrue(netCap.appliesToUid(500));
        assertTrue(netCap.appliesToUidRange(new UidRange(1, 100000)));
        assertFalse(netCap2.appliesToUid(500));
        assertFalse(netCap2.appliesToUidRange(new UidRange(1, 100000)));
        assertTrue(new NetworkCapabilities().satisfiedByUids(netCap));
    }

    @Test
    public void testParcelNetworkCapabilities() {
        final Set<UidRange> uids = new ArraySet<>();
        uids.add(new UidRange(50, 100));
        uids.add(new UidRange(3000, 4000));
        final NetworkCapabilities netCap = new NetworkCapabilities()
            .addCapability(NET_CAPABILITY_INTERNET)
            .setUids(uids)
            .addCapability(NET_CAPABILITY_EIMS)
            .addCapability(NET_CAPABILITY_NOT_METERED);
        if (isAtLeastR()) {
            netCap.setOwnerUid(123);
            netCap.setAdministratorUids(new int[] {5, 11});
        }
        assertParcelingIsLossless(netCap);
        netCap.setSSID(TEST_SSID);
        testParcelSane(netCap);
    }

    @Test
    public void testParcelNetworkCapabilitiesWithRequestorUidAndPackageName() {
        final NetworkCapabilities netCap = new NetworkCapabilities()
                .addCapability(NET_CAPABILITY_INTERNET)
                .addCapability(NET_CAPABILITY_EIMS)
                .addCapability(NET_CAPABILITY_NOT_METERED);
        if (isAtLeastR()) {
            netCap.setRequestorPackageName("com.android.test");
            netCap.setRequestorUid(9304);
        }
        assertParcelingIsLossless(netCap);
        netCap.setSSID(TEST_SSID);
        testParcelSane(netCap);
    }

    private void testParcelSane(NetworkCapabilities cap) {
        if (isAtLeastR()) {
            assertParcelSane(cap, 15);
        } else {
            assertParcelSane(cap, 11);
        }
    }

    @Test
    public void testOemPaid() {
        NetworkCapabilities nc = new NetworkCapabilities();
        // By default OEM_PAID is neither in the unwanted or required lists and the network is not
        // restricted.
        assertFalse(nc.hasUnwantedCapability(NET_CAPABILITY_OEM_PAID));
        assertFalse(nc.hasCapability(NET_CAPABILITY_OEM_PAID));
        nc.maybeMarkCapabilitiesRestricted();
        assertTrue(nc.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // Adding OEM_PAID to capability list should make network restricted.
        nc.addCapability(NET_CAPABILITY_OEM_PAID);
        nc.addCapability(NET_CAPABILITY_INTERNET);  // Combine with unrestricted capability.
        nc.maybeMarkCapabilitiesRestricted();
        assertTrue(nc.hasCapability(NET_CAPABILITY_OEM_PAID));
        assertFalse(nc.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // Now let's make request for OEM_PAID network.
        NetworkCapabilities nr = new NetworkCapabilities();
        nr.addCapability(NET_CAPABILITY_OEM_PAID);
        nr.maybeMarkCapabilitiesRestricted();
        assertTrue(nr.satisfiedByNetworkCapabilities(nc));

        // Request fails for network with the default capabilities.
        assertFalse(nr.satisfiedByNetworkCapabilities(new NetworkCapabilities()));
    }

    @Test
    public void testUnwantedCapabilities() {
        NetworkCapabilities network = new NetworkCapabilities();

        NetworkCapabilities request = new NetworkCapabilities();
        assertTrue("Request: " + request + ", Network:" + network,
                request.satisfiedByNetworkCapabilities(network));

        // Requesting absence of capabilities that network doesn't have. Request should satisfy.
        request.addUnwantedCapability(NET_CAPABILITY_WIFI_P2P);
        request.addUnwantedCapability(NET_CAPABILITY_NOT_METERED);
        assertTrue(request.satisfiedByNetworkCapabilities(network));
        assertArrayEquals(new int[] {NET_CAPABILITY_WIFI_P2P,
                        NET_CAPABILITY_NOT_METERED},
                request.getUnwantedCapabilities());

        // This is a default capability, just want to make sure its there because we use it below.
        assertTrue(network.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // Verify that adding unwanted capability will effectively remove it from capability list.
        request.addUnwantedCapability(NET_CAPABILITY_NOT_RESTRICTED);
        assertTrue(request.hasUnwantedCapability(NET_CAPABILITY_NOT_RESTRICTED));
        assertFalse(request.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));

        // Now this request won't be satisfied because network contains NOT_RESTRICTED.
        assertFalse(request.satisfiedByNetworkCapabilities(network));
        network.removeCapability(NET_CAPABILITY_NOT_RESTRICTED);
        assertTrue(request.satisfiedByNetworkCapabilities(network));

        // Verify that adding capability will effectively remove it from unwanted list
        request.addCapability(NET_CAPABILITY_NOT_RESTRICTED);
        assertTrue(request.hasCapability(NET_CAPABILITY_NOT_RESTRICTED));
        assertFalse(request.hasUnwantedCapability(NET_CAPABILITY_NOT_RESTRICTED));

        assertFalse(request.satisfiedByNetworkCapabilities(network));
        network.addCapability(NET_CAPABILITY_NOT_RESTRICTED);
        assertTrue(request.satisfiedByNetworkCapabilities(network));
    }

    @Test
    public void testConnectivityManagedCapabilities() {
        NetworkCapabilities nc = new NetworkCapabilities();
        assertFalse(nc.hasConnectivityManagedCapability());
        // Check every single system managed capability.
        nc.addCapability(NET_CAPABILITY_CAPTIVE_PORTAL);
        assertTrue(nc.hasConnectivityManagedCapability());
        nc.removeCapability(NET_CAPABILITY_CAPTIVE_PORTAL);
        nc.addCapability(NET_CAPABILITY_FOREGROUND);
        assertTrue(nc.hasConnectivityManagedCapability());
        nc.removeCapability(NET_CAPABILITY_FOREGROUND);
        nc.addCapability(NET_CAPABILITY_PARTIAL_CONNECTIVITY);
        assertTrue(nc.hasConnectivityManagedCapability());
        nc.removeCapability(NET_CAPABILITY_PARTIAL_CONNECTIVITY);
        nc.addCapability(NET_CAPABILITY_VALIDATED);
        assertTrue(nc.hasConnectivityManagedCapability());
    }

    @Test
    public void testEqualsNetCapabilities() {
        NetworkCapabilities nc1 = new NetworkCapabilities();
        NetworkCapabilities nc2 = new NetworkCapabilities();
        assertTrue(nc1.equalsNetCapabilities(nc2));
        assertEquals(nc1, nc2);

        nc1.addCapability(NET_CAPABILITY_MMS);
        assertFalse(nc1.equalsNetCapabilities(nc2));
        assertNotEquals(nc1, nc2);
        nc2.addCapability(NET_CAPABILITY_MMS);
        assertTrue(nc1.equalsNetCapabilities(nc2));
        assertEquals(nc1, nc2);

        nc1.addUnwantedCapability(NET_CAPABILITY_INTERNET);
        assertFalse(nc1.equalsNetCapabilities(nc2));
        nc2.addUnwantedCapability(NET_CAPABILITY_INTERNET);
        assertTrue(nc1.equalsNetCapabilities(nc2));

        nc1.removeCapability(NET_CAPABILITY_INTERNET);
        assertFalse(nc1.equalsNetCapabilities(nc2));
        nc2.removeCapability(NET_CAPABILITY_INTERNET);
        assertTrue(nc1.equalsNetCapabilities(nc2));
    }

    @Test
    public void testSSID() {
        NetworkCapabilities nc1 = new NetworkCapabilities();
        NetworkCapabilities nc2 = new NetworkCapabilities();
        assertTrue(nc2.satisfiedBySSID(nc1));

        nc1.setSSID(TEST_SSID);
        assertTrue(nc2.satisfiedBySSID(nc1));
        nc2.setSSID("different " + TEST_SSID);
        assertFalse(nc2.satisfiedBySSID(nc1));

        assertTrue(nc1.satisfiedByImmutableNetworkCapabilities(nc2));
        assertFalse(nc1.satisfiedByNetworkCapabilities(nc2));
    }

    private ArraySet<UidRange> uidRange(int from, int to) {
        final ArraySet<UidRange> range = new ArraySet<>(1);
        range.add(new UidRange(from, to));
        return range;
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testSetAdministratorUids() {
        NetworkCapabilities nc =
                new NetworkCapabilities().setAdministratorUids(new int[] {2, 1, 3});

        assertArrayEquals(new int[] {1, 2, 3}, nc.getAdministratorUids());
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testSetAdministratorUidsWithDuplicates() {
        try {
            new NetworkCapabilities().setAdministratorUids(new int[] {1, 1});
            fail("Expected IllegalArgumentException for duplicate uids");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testCombineCapabilities() {
        NetworkCapabilities nc1 = new NetworkCapabilities();
        NetworkCapabilities nc2 = new NetworkCapabilities();

        nc1.addUnwantedCapability(NET_CAPABILITY_CAPTIVE_PORTAL);
        nc1.addCapability(NET_CAPABILITY_NOT_ROAMING);
        assertNotEquals(nc1, nc2);
        nc2.combineCapabilities(nc1);
        assertEquals(nc1, nc2);
        assertTrue(nc2.hasCapability(NET_CAPABILITY_NOT_ROAMING));
        assertTrue(nc2.hasUnwantedCapability(NET_CAPABILITY_CAPTIVE_PORTAL));

        // This will effectively move NOT_ROAMING capability from required to unwanted for nc1.
        nc1.addUnwantedCapability(NET_CAPABILITY_NOT_ROAMING);

        nc2.combineCapabilities(nc1);
        // We will get this capability in both requested and unwanted lists thus this request
        // will never be satisfied.
        assertTrue(nc2.hasCapability(NET_CAPABILITY_NOT_ROAMING));
        assertTrue(nc2.hasUnwantedCapability(NET_CAPABILITY_NOT_ROAMING));

        nc1.setSSID(TEST_SSID);
        nc2.combineCapabilities(nc1);
        if (isAtLeastR()) {
            assertTrue(TEST_SSID.equals(nc2.getSsid()));
        }

        // Because they now have the same SSID, the following call should not throw
        nc2.combineCapabilities(nc1);

        nc1.setSSID(DIFFERENT_TEST_SSID);
        try {
            nc2.combineCapabilities(nc1);
            fail("Expected IllegalStateException: can't combine different SSIDs");
        } catch (IllegalStateException expected) {}
        nc1.setSSID(TEST_SSID);

        nc1.setUids(uidRange(10, 13));
        assertNotEquals(nc1, nc2);
        nc2.combineCapabilities(nc1);  // Everything + 10~13 is still everything.
        assertNotEquals(nc1, nc2);
        nc1.combineCapabilities(nc2);  // 10~13 + everything is everything.
        assertEquals(nc1, nc2);
        nc1.setUids(uidRange(10, 13));
        nc2.setUids(uidRange(20, 23));
        assertNotEquals(nc1, nc2);
        nc1.combineCapabilities(nc2);
        assertTrue(nc1.appliesToUid(12));
        assertFalse(nc2.appliesToUid(12));
        assertTrue(nc1.appliesToUid(22));
        assertTrue(nc2.appliesToUid(22));
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testCombineCapabilities_AdministratorUids() {
        final NetworkCapabilities nc1 = new NetworkCapabilities();
        final NetworkCapabilities nc2 = new NetworkCapabilities();

        final int[] adminUids = {3, 6, 12};
        nc1.setAdministratorUids(adminUids);
        nc2.combineCapabilities(nc1);
        assertTrue(nc2.equalsAdministratorUids(nc1));
        assertArrayEquals(nc2.getAdministratorUids(), adminUids);

        final int[] adminUidsOtherOrder = {3, 12, 6};
        nc1.setAdministratorUids(adminUidsOtherOrder);
        assertTrue(nc2.equalsAdministratorUids(nc1));

        final int[] adminUids2 = {11, 1, 12, 3, 6};
        nc1.setAdministratorUids(adminUids2);
        assertFalse(nc2.equalsAdministratorUids(nc1));
        assertFalse(Arrays.equals(nc2.getAdministratorUids(), adminUids2));
        try {
            nc2.combineCapabilities(nc1);
            fail("Shouldn't be able to combine different lists of admin UIDs");
        } catch (IllegalStateException expected) { }
    }

    @Test
    public void testSetCapabilities() {
        final int[] REQUIRED_CAPABILITIES = new int[] {
                NET_CAPABILITY_INTERNET, NET_CAPABILITY_NOT_VPN };
        final int[] UNWANTED_CAPABILITIES = new int[] {
                NET_CAPABILITY_NOT_RESTRICTED, NET_CAPABILITY_NOT_METERED
        };

        NetworkCapabilities nc1 = new NetworkCapabilities();
        NetworkCapabilities nc2 = new NetworkCapabilities();

        nc1.setCapabilities(REQUIRED_CAPABILITIES, UNWANTED_CAPABILITIES);
        assertArrayEquals(REQUIRED_CAPABILITIES, nc1.getCapabilities());

        // Verify that setting and adding capabilities leads to the same object state.
        nc2.clearAll();
        for (int cap : REQUIRED_CAPABILITIES) {
            nc2.addCapability(cap);
        }
        for (int cap : UNWANTED_CAPABILITIES) {
            nc2.addUnwantedCapability(cap);
        }
        assertEquals(nc1, nc2);
    }

    @Test
    public void testSetNetworkSpecifierOnMultiTransportNc() {
        // Sequence 1: Transport + Transport + NetworkSpecifier
        NetworkCapabilities nc1 = new NetworkCapabilities();
        nc1.addTransportType(TRANSPORT_CELLULAR).addTransportType(TRANSPORT_WIFI);
        try {
            nc1.setNetworkSpecifier(new StringNetworkSpecifier("specs"));
            fail("Cannot set NetworkSpecifier on a NetworkCapability with multiple transports!");
        } catch (IllegalStateException expected) {
            // empty
        }

        // Sequence 2: Transport + NetworkSpecifier + Transport
        NetworkCapabilities nc2 = new NetworkCapabilities();
        nc2.addTransportType(TRANSPORT_CELLULAR).setNetworkSpecifier(
                new StringNetworkSpecifier("specs"));
        try {
            nc2.addTransportType(TRANSPORT_WIFI);
            fail("Cannot set a second TransportType of a network which has a NetworkSpecifier!");
        } catch (IllegalStateException expected) {
            // empty
        }
    }

    @Test
    public void testSetTransportInfoOnMultiTransportNc() {
        // Sequence 1: Transport + Transport + TransportInfo
        NetworkCapabilities nc1 = new NetworkCapabilities();
        nc1.addTransportType(TRANSPORT_CELLULAR).addTransportType(TRANSPORT_WIFI)
                .setTransportInfo(new TransportInfo() {});

        // Sequence 2: Transport + NetworkSpecifier + Transport
        NetworkCapabilities nc2 = new NetworkCapabilities();
        nc2.addTransportType(TRANSPORT_CELLULAR).setTransportInfo(new TransportInfo() {})
                .addTransportType(TRANSPORT_WIFI);
    }

    @Test
    public void testCombineTransportInfo() {
        NetworkCapabilities nc1 = new NetworkCapabilities();
        nc1.setTransportInfo(new TransportInfo() {
            // empty
        });
        NetworkCapabilities nc2 = new NetworkCapabilities();
        // new TransportInfo so that object is not #equals to nc1's TransportInfo (that's where
        // combine fails)
        nc2.setTransportInfo(new TransportInfo() {
            // empty
        });

        try {
            nc1.combineCapabilities(nc2);
            fail("Should not be able to combine NetworkCabilities which contain TransportInfos");
        } catch (IllegalStateException expected) {
            // empty
        }

        // verify that can combine with identical TransportInfo objects
        NetworkCapabilities nc3 = new NetworkCapabilities();
        nc3.setTransportInfo(nc1.getTransportInfo());
        nc1.combineCapabilities(nc3);
    }

    @Test
    public void testSet() {
        NetworkCapabilities nc1 = new NetworkCapabilities();
        NetworkCapabilities nc2 = new NetworkCapabilities();

        nc1.addUnwantedCapability(NET_CAPABILITY_CAPTIVE_PORTAL);
        nc1.addCapability(NET_CAPABILITY_NOT_ROAMING);
        assertNotEquals(nc1, nc2);
        nc2.set(nc1);
        assertEquals(nc1, nc2);
        assertTrue(nc2.hasCapability(NET_CAPABILITY_NOT_ROAMING));
        assertTrue(nc2.hasUnwantedCapability(NET_CAPABILITY_CAPTIVE_PORTAL));

        // This will effectively move NOT_ROAMING capability from required to unwanted for nc1.
        nc1.addUnwantedCapability(NET_CAPABILITY_NOT_ROAMING);
        nc1.setSSID(TEST_SSID);
        nc2.set(nc1);
        assertEquals(nc1, nc2);
        // Contrary to combineCapabilities, set() will have removed the NOT_ROAMING capability
        // from nc2.
        assertFalse(nc2.hasCapability(NET_CAPABILITY_NOT_ROAMING));
        assertTrue(nc2.hasUnwantedCapability(NET_CAPABILITY_NOT_ROAMING));
        if (isAtLeastR()) {
            assertTrue(TEST_SSID.equals(nc2.getSsid()));
        }

        nc1.setSSID(DIFFERENT_TEST_SSID);
        nc2.set(nc1);
        assertEquals(nc1, nc2);
        if (isAtLeastR()) {
            assertTrue(DIFFERENT_TEST_SSID.equals(nc2.getSsid()));
        }

        nc1.setUids(uidRange(10, 13));
        nc2.set(nc1);  // Overwrites, as opposed to combineCapabilities
        assertEquals(nc1, nc2);
    }

    @Test
    public void testGetTransportTypes() {
        final NetworkCapabilities nc = new NetworkCapabilities();
        nc.addTransportType(TRANSPORT_CELLULAR);
        nc.addTransportType(TRANSPORT_WIFI);
        nc.addTransportType(TRANSPORT_VPN);
        nc.addTransportType(TRANSPORT_TEST);

        final int[] transportTypes = nc.getTransportTypes();
        assertEquals(4, transportTypes.length);
        assertEquals(TRANSPORT_CELLULAR, transportTypes[0]);
        assertEquals(TRANSPORT_WIFI, transportTypes[1]);
        assertEquals(TRANSPORT_VPN, transportTypes[2]);
        assertEquals(TRANSPORT_TEST, transportTypes[3]);
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testTelephonyNetworkSpecifier() {
        final TelephonyNetworkSpecifier specifier = new TelephonyNetworkSpecifier(1);
        final NetworkCapabilities nc1 = new NetworkCapabilities.Builder()
                .addTransportType(TRANSPORT_WIFI)
                .setNetworkSpecifier(specifier)
                .build();
        assertEquals(specifier, nc1.getNetworkSpecifier());
        try {
            final NetworkCapabilities nc2 = new NetworkCapabilities.Builder()
                    .setNetworkSpecifier(specifier)
                    .build();
            fail("Must have a single transport type. Without transport type or multiple transport"
                    + " types is invalid.");
        } catch (IllegalStateException expected) { }
    }

    @Test
    public void testWifiAwareNetworkSpecifier() {
        final NetworkCapabilities nc = new NetworkCapabilities()
                .addTransportType(TRANSPORT_WIFI_AWARE);
        // If NetworkSpecifier is not set, the default value is null.
        assertNull(nc.getNetworkSpecifier());
        final WifiAwareNetworkSpecifier specifier = new WifiAwareNetworkSpecifier.Builder(
                mDiscoverySession, mPeerHandle).build();
        nc.setNetworkSpecifier(specifier);
        assertEquals(specifier, nc.getNetworkSpecifier());
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testAdministratorUidsAndOwnerUid() {
        // Test default owner uid.
        // If the owner uid is not set, the default value should be Process.INVALID_UID.
        final NetworkCapabilities nc1 = new NetworkCapabilities.Builder().build();
        assertEquals(Process.INVALID_UID, nc1.getOwnerUid());
        // Test setAdministratorUids and getAdministratorUids.
        final int[] administratorUids = {1001, 10001};
        final NetworkCapabilities nc2 = new NetworkCapabilities.Builder()
                .setAdministratorUids(administratorUids)
                .build();
        assertTrue(Arrays.equals(administratorUids, nc2.getAdministratorUids()));
        // Test setOwnerUid and getOwnerUid.
        // The owner UID must be included in administrator UIDs, or throw IllegalStateException.
        try {
            final NetworkCapabilities nc3 = new NetworkCapabilities.Builder()
                    .setOwnerUid(1001)
                    .build();
            fail("The owner UID must be included in administrator UIDs.");
        } catch (IllegalStateException expected) { }
        final NetworkCapabilities nc4 = new NetworkCapabilities.Builder()
                .setAdministratorUids(administratorUids)
                .setOwnerUid(1001)
                .build();
        assertEquals(1001, nc4.getOwnerUid());
        try {
            final NetworkCapabilities nc5 = new NetworkCapabilities.Builder()
                    .setAdministratorUids(null)
                    .build();
            fail("Should not set null into setAdministratorUids");
        } catch (NullPointerException expected) { }
    }

    @Test
    public void testLinkBandwidthKbps() {
        final NetworkCapabilities nc = new NetworkCapabilities();
        // The default value of LinkDown/UpstreamBandwidthKbps should be LINK_BANDWIDTH_UNSPECIFIED.
        assertEquals(LINK_BANDWIDTH_UNSPECIFIED, nc.getLinkDownstreamBandwidthKbps());
        assertEquals(LINK_BANDWIDTH_UNSPECIFIED, nc.getLinkUpstreamBandwidthKbps());
        nc.setLinkDownstreamBandwidthKbps(512);
        nc.setLinkUpstreamBandwidthKbps(128);
        assertEquals(512, nc.getLinkDownstreamBandwidthKbps());
        assertNotEquals(128, nc.getLinkDownstreamBandwidthKbps());
        assertEquals(128, nc.getLinkUpstreamBandwidthKbps());
        assertNotEquals(512, nc.getLinkUpstreamBandwidthKbps());
    }

    @Test
    public void testSignalStrength() {
        final NetworkCapabilities nc = new NetworkCapabilities();
        // The default value of signal strength should be SIGNAL_STRENGTH_UNSPECIFIED.
        assertEquals(SIGNAL_STRENGTH_UNSPECIFIED, nc.getSignalStrength());
        nc.setSignalStrength(-80);
        assertEquals(-80, nc.getSignalStrength());
        assertNotEquals(-50, nc.getSignalStrength());
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testDeduceRestrictedCapability() {
        final NetworkCapabilities nc = new NetworkCapabilities();
        // Default capabilities don't have restricted capability.
        assertFalse(nc.deduceRestrictedCapability());
        // If there is a force restricted capability, then the network capabilities is restricted.
        nc.addCapability(NET_CAPABILITY_OEM_PAID);
        nc.addCapability(NET_CAPABILITY_INTERNET);
        assertTrue(nc.deduceRestrictedCapability());
        // Except for the force restricted capability, if there is any unrestricted capability in
        // capabilities, then the network capabilities is not restricted.
        nc.removeCapability(NET_CAPABILITY_OEM_PAID);
        nc.addCapability(NET_CAPABILITY_CBS);
        assertFalse(nc.deduceRestrictedCapability());
        // Except for the force restricted capability, the network capabilities will only be treated
        // as restricted when there is no any unrestricted capability.
        nc.removeCapability(NET_CAPABILITY_INTERNET);
        assertTrue(nc.deduceRestrictedCapability());
    }

    private void assertNoTransport(NetworkCapabilities nc) {
        for (int i = MIN_TRANSPORT; i <= MAX_TRANSPORT; i++) {
            assertFalse(nc.hasTransport(i));
        }
    }

    // Checks that all transport types from MIN_TRANSPORT to maxTransportType are set and all
    // transport types from maxTransportType + 1 to MAX_TRANSPORT are not set when positiveSequence
    // is true. If positiveSequence is false, then the check sequence is opposite.
    private void checkCurrentTransportTypes(NetworkCapabilities nc, int maxTransportType,
            boolean positiveSequence) {
        for (int i = MIN_TRANSPORT; i <= maxTransportType; i++) {
            if (positiveSequence) {
                assertTrue(nc.hasTransport(i));
            } else {
                assertFalse(nc.hasTransport(i));
            }
        }
        for (int i = MAX_TRANSPORT; i > maxTransportType; i--) {
            if (positiveSequence) {
                assertFalse(nc.hasTransport(i));
            } else {
                assertTrue(nc.hasTransport(i));
            }
        }
    }

    @Test
    public void testMultipleTransportTypes() {
        final NetworkCapabilities nc = new NetworkCapabilities();
        assertNoTransport(nc);
        // Test adding multiple transport types.
        for (int i = MIN_TRANSPORT; i <= MAX_TRANSPORT; i++) {
            nc.addTransportType(i);
            checkCurrentTransportTypes(nc, i, true /* positiveSequence */);
        }
        // Test removing multiple transport types.
        for (int i = MIN_TRANSPORT; i <= MAX_TRANSPORT; i++) {
            nc.removeTransportType(i);
            checkCurrentTransportTypes(nc, i, false /* positiveSequence */);
        }
        assertNoTransport(nc);
        nc.addTransportType(TRANSPORT_WIFI);
        assertTrue(nc.hasTransport(TRANSPORT_WIFI));
        assertFalse(nc.hasTransport(TRANSPORT_VPN));
        nc.addTransportType(TRANSPORT_VPN);
        assertTrue(nc.hasTransport(TRANSPORT_WIFI));
        assertTrue(nc.hasTransport(TRANSPORT_VPN));
        nc.removeTransportType(TRANSPORT_WIFI);
        assertFalse(nc.hasTransport(TRANSPORT_WIFI));
        assertTrue(nc.hasTransport(TRANSPORT_VPN));
        nc.removeTransportType(TRANSPORT_VPN);
        assertFalse(nc.hasTransport(TRANSPORT_WIFI));
        assertFalse(nc.hasTransport(TRANSPORT_VPN));
        assertNoTransport(nc);
    }

    @Test
    public void testAddAndRemoveTransportType() {
        final NetworkCapabilities nc = new NetworkCapabilities();
        try {
            nc.addTransportType(-1);
            fail("Should not set invalid transport type into addTransportType");
        } catch (IllegalArgumentException expected) { }
        try {
            nc.removeTransportType(-1);
            fail("Should not set invalid transport type into removeTransportType");
        } catch (IllegalArgumentException e) { }
    }

    private class TestTransportInfo implements TransportInfo {
        TestTransportInfo() {
        }
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testBuilder() {
        final int ownerUid = 1001;
        final int signalStrength = -80;
        final int requestUid = 10100;
        final int[] administratorUids = {ownerUid, 10001};
        final TelephonyNetworkSpecifier specifier = new TelephonyNetworkSpecifier(1);
        final TestTransportInfo transportInfo = new TestTransportInfo();
        final String ssid = "TEST_SSID";
        final String packageName = "com.google.test.networkcapabilities";
        final NetworkCapabilities nc = new NetworkCapabilities.Builder()
                .addTransportType(TRANSPORT_WIFI)
                .addTransportType(TRANSPORT_CELLULAR)
                .removeTransportType(TRANSPORT_CELLULAR)
                .addCapability(NET_CAPABILITY_EIMS)
                .addCapability(NET_CAPABILITY_CBS)
                .removeCapability(NET_CAPABILITY_CBS)
                .setAdministratorUids(administratorUids)
                .setOwnerUid(ownerUid)
                .setLinkDownstreamBandwidthKbps(512)
                .setLinkUpstreamBandwidthKbps(128)
                .setNetworkSpecifier(specifier)
                .setTransportInfo(transportInfo)
                .setSignalStrength(signalStrength)
                .setSsid(ssid)
                .setRequestorUid(requestUid)
                .setRequestorPackageName(packageName)
                .build();
        assertEquals(1, nc.getTransportTypes().length);
        assertEquals(TRANSPORT_WIFI, nc.getTransportTypes()[0]);
        assertTrue(nc.hasCapability(NET_CAPABILITY_EIMS));
        assertFalse(nc.hasCapability(NET_CAPABILITY_CBS));
        assertTrue(Arrays.equals(administratorUids, nc.getAdministratorUids()));
        assertEquals(ownerUid, nc.getOwnerUid());
        assertEquals(512, nc.getLinkDownstreamBandwidthKbps());
        assertNotEquals(128, nc.getLinkDownstreamBandwidthKbps());
        assertEquals(128, nc.getLinkUpstreamBandwidthKbps());
        assertNotEquals(512, nc.getLinkUpstreamBandwidthKbps());
        assertEquals(specifier, nc.getNetworkSpecifier());
        assertEquals(transportInfo, nc.getTransportInfo());
        assertEquals(signalStrength, nc.getSignalStrength());
        assertNotEquals(-50, nc.getSignalStrength());
        assertEquals(ssid, nc.getSsid());
        assertEquals(requestUid, nc.getRequestorUid());
        assertEquals(packageName, nc.getRequestorPackageName());
        // Cannot assign null into NetworkCapabilities.Builder
        try {
            final NetworkCapabilities.Builder builder = new NetworkCapabilities.Builder(null);
            fail("Should not set null into NetworkCapabilities.Builder");
        } catch (NullPointerException expected) { }
        assertEquals(nc, new NetworkCapabilities.Builder(nc).build());
    }
}
