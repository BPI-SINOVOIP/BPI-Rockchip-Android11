/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;

import android.content.Context;
import android.content.ContentResolver;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkUtils;
import android.net.cts.util.CtsNetUtils;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;
import android.system.ErrnoException;
import android.system.OsConstants;
import android.test.AndroidTestCase;

import java.util.ArrayList;

public class MultinetworkApiTest extends AndroidTestCase {

    static {
        System.loadLibrary("nativemultinetwork_jni");
    }

    private static final String TAG = "MultinetworkNativeApiTest";
    static final String GOOGLE_PRIVATE_DNS_SERVER = "dns.google";

    /**
     * @return 0 on success
     */
    private static native int runGetaddrinfoCheck(long networkHandle);
    private static native int runSetprocnetwork(long networkHandle);
    private static native int runSetsocknetwork(long networkHandle);
    private static native int runDatagramCheck(long networkHandle);
    private static native void runResNapiMalformedCheck(long networkHandle);
    private static native void runResNcancelCheck(long networkHandle);
    private static native void runResNqueryCheck(long networkHandle);
    private static native void runResNsendCheck(long networkHandle);
    private static native void runResNnxDomainCheck(long networkHandle);


    private ContentResolver mCR;
    private ConnectivityManager mCM;
    private CtsNetUtils mCtsNetUtils;
    private String mOldMode;
    private String mOldDnsSpecifier;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mCM = (ConnectivityManager) getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        mCR = getContext().getContentResolver();
        mCtsNetUtils = new CtsNetUtils(getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    private Network[] getTestableNetworks() {
        final ArrayList<Network> testableNetworks = new ArrayList<Network>();
        for (Network network : mCM.getAllNetworks()) {
            final NetworkCapabilities nc = mCM.getNetworkCapabilities(network);
            if (nc != null
                    && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)
                    && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) {
                testableNetworks.add(network);
            }
        }

        assertTrue(
                "This test requires that at least one network be connected. " +
                "Please ensure that the device is connected to a network.",
                testableNetworks.size() >= 1);
        return testableNetworks.toArray(new Network[0]);
    }

    public void testGetaddrinfo() throws ErrnoException {
        for (Network network : getTestableNetworks()) {
            int errno = runGetaddrinfoCheck(network.getNetworkHandle());
            if (errno != 0) {
                throw new ErrnoException(
                        "getaddrinfo on " + mCM.getNetworkInfo(network), -errno);
            }
        }
    }

    public void testSetprocnetwork() throws ErrnoException {
        // Hopefully no prior test in this process space has set a default network.
        assertNull(mCM.getProcessDefaultNetwork());
        assertEquals(0, NetworkUtils.getBoundNetworkForProcess());

        for (Network network : getTestableNetworks()) {
            mCM.setProcessDefaultNetwork(null);
            assertNull(mCM.getProcessDefaultNetwork());

            int errno = runSetprocnetwork(network.getNetworkHandle());
            if (errno != 0) {
                throw new ErrnoException(
                        "setprocnetwork on " + mCM.getNetworkInfo(network), -errno);
            }
            Network processDefault = mCM.getProcessDefaultNetwork();
            assertNotNull(processDefault);
            assertEquals(network, processDefault);
            // TODO: open DatagramSockets, connect them to 192.0.2.1 and 2001:db8::,
            // and ensure that the source address is in fact on this network as
            // determined by mCM.getLinkProperties(network).

            mCM.setProcessDefaultNetwork(null);
        }

        for (Network network : getTestableNetworks()) {
            NetworkUtils.bindProcessToNetwork(0);
            assertNull(mCM.getBoundNetworkForProcess());

            int errno = runSetprocnetwork(network.getNetworkHandle());
            if (errno != 0) {
                throw new ErrnoException(
                        "setprocnetwork on " + mCM.getNetworkInfo(network), -errno);
            }
            assertEquals(network, new Network(mCM.getBoundNetworkForProcess()));
            // TODO: open DatagramSockets, connect them to 192.0.2.1 and 2001:db8::,
            // and ensure that the source address is in fact on this network as
            // determined by mCM.getLinkProperties(network).

            NetworkUtils.bindProcessToNetwork(0);
        }
    }

    public void testSetsocknetwork() throws ErrnoException {
        for (Network network : getTestableNetworks()) {
            int errno = runSetsocknetwork(network.getNetworkHandle());
            if (errno != 0) {
                throw new ErrnoException(
                        "setsocknetwork on " + mCM.getNetworkInfo(network), -errno);
            }
        }
    }

    public void testNativeDatagramTransmission() throws ErrnoException {
        for (Network network : getTestableNetworks()) {
            int errno = runDatagramCheck(network.getNetworkHandle());
            if (errno != 0) {
                throw new ErrnoException(
                        "DatagramCheck on " + mCM.getNetworkInfo(network), -errno);
            }
        }
    }

    public void testNoSuchNetwork() {
        final Network eNoNet = new Network(54321);
        assertNull(mCM.getNetworkInfo(eNoNet));

        final long eNoNetHandle = eNoNet.getNetworkHandle();
        assertEquals(-OsConstants.ENONET, runSetsocknetwork(eNoNetHandle));
        assertEquals(-OsConstants.ENONET, runSetprocnetwork(eNoNetHandle));
        // TODO: correct test permissions so this call is not silently re-mapped
        // to query on the default network.
        // assertEquals(-OsConstants.ENONET, runGetaddrinfoCheck(eNoNetHandle));
    }

    public void testNetworkHandle() {
        // Test Network -> NetworkHandle -> Network results in the same Network.
        for (Network network : getTestableNetworks()) {
            long networkHandle = network.getNetworkHandle();
            Network newNetwork = Network.fromNetworkHandle(networkHandle);
            assertEquals(newNetwork, network);
        }

        // Test that only obfuscated handles are allowed.
        try {
            Network.fromNetworkHandle(100);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            Network.fromNetworkHandle(-1);
            fail();
        } catch (IllegalArgumentException e) {}
        try {
            Network.fromNetworkHandle(0);
            fail();
        } catch (IllegalArgumentException e) {}
    }

    public void testResNApi() throws Exception {
        final Network[] testNetworks = getTestableNetworks();

        for (Network network : testNetworks) {
            // Throws AssertionError directly in jni function if test fail.
            runResNqueryCheck(network.getNetworkHandle());
            runResNsendCheck(network.getNetworkHandle());
            runResNcancelCheck(network.getNetworkHandle());
            runResNapiMalformedCheck(network.getNetworkHandle());

            final NetworkCapabilities nc = mCM.getNetworkCapabilities(network);
            // Some cellular networks configure their DNS servers never to return NXDOMAIN, so don't
            // test NXDOMAIN on these DNS servers.
            // b/144521720
            if (nc != null && !nc.hasTransport(TRANSPORT_CELLULAR)) {
                runResNnxDomainCheck(network.getNetworkHandle());
            }
        }
    }

    @AppModeFull(reason = "WRITE_SECURE_SETTINGS permission can't be granted to instant apps")
    public void testResNApiNXDomainPrivateDns() throws InterruptedException {
        mCtsNetUtils.storePrivateDnsSetting();
        // Enable private DNS strict mode and set server to dns.google before doing NxDomain test.
        // b/144521720
        try {
            mCtsNetUtils.setPrivateDnsStrictMode(GOOGLE_PRIVATE_DNS_SERVER);
            for (Network network : getTestableNetworks()) {
              // Wait for private DNS setting to propagate.
              mCtsNetUtils.awaitPrivateDnsSetting("NxDomain test wait private DNS setting timeout",
                        network, GOOGLE_PRIVATE_DNS_SERVER, true);
              runResNnxDomainCheck(network.getNetworkHandle());
            }
        } finally {
            mCtsNetUtils.restorePrivateDnsSetting();
        }
    }
}
