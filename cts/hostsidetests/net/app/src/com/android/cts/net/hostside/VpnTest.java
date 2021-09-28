/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.cts.net.hostside;

import static android.os.Process.INVALID_UID;
import static android.system.OsConstants.*;

import android.annotation.Nullable;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.Proxy;
import android.net.ProxyInfo;
import android.net.VpnService;
import android.net.wifi.WifiManager;
import android.provider.Settings;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.SystemProperties;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;
import android.text.TextUtils;
import android.util.Log;

import com.android.compatibility.common.util.BlockingBroadcastReceiver;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Objects;
import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the VpnService API.
 *
 * These tests establish a VPN via the VpnService API, and have the service reflect the packets back
 * to the device without causing any network traffic. This allows testing the local VPN data path
 * without a network connection or a VPN server.
 *
 * Note: in Lollipop, VPN functionality relies on kernel support for UID-based routing. If these
 * tests fail, it may be due to the lack of kernel support. The necessary patches can be
 * cherry-picked from the Android common kernel trees:
 *
 * android-3.10:
 *   https://android-review.googlesource.com/#/c/99220/
 *   https://android-review.googlesource.com/#/c/100545/
 *
 * android-3.4:
 *   https://android-review.googlesource.com/#/c/99225/
 *   https://android-review.googlesource.com/#/c/100557/
 *
 * To ensure that the kernel has the required commits, run the kernel unit
 * tests described at:
 *
 *   https://source.android.com/devices/tech/config/kernel_network_tests.html
 *
 */
public class VpnTest extends InstrumentationTestCase {

    // These are neither public nor @TestApi.
    // TODO: add them to @TestApi.
    private static final String PRIVATE_DNS_MODE_SETTING = "private_dns_mode";
    private static final String PRIVATE_DNS_MODE_PROVIDER_HOSTNAME = "hostname";
    private static final String PRIVATE_DNS_MODE_OPPORTUNISTIC = "opportunistic";
    private static final String PRIVATE_DNS_SPECIFIER_SETTING = "private_dns_specifier";

    public static String TAG = "VpnTest";
    public static int TIMEOUT_MS = 3 * 1000;
    public static int SOCKET_TIMEOUT_MS = 100;
    public static String TEST_HOST = "connectivitycheck.gstatic.com";

    private UiDevice mDevice;
    private MyActivity mActivity;
    private String mPackageName;
    private ConnectivityManager mCM;
    private WifiManager mWifiManager;
    private RemoteSocketFactoryClient mRemoteSocketFactoryClient;

    Network mNetwork;
    NetworkCallback mCallback;
    final Object mLock = new Object();
    final Object mLockShutdown = new Object();

    private String mOldPrivateDnsMode;
    private String mOldPrivateDnsSpecifier;

    private boolean supportedHardware() {
        final PackageManager pm = getInstrumentation().getContext().getPackageManager();
        return !pm.hasSystemFeature("android.hardware.type.watch");
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mNetwork = null;
        mCallback = null;
        storePrivateDnsSetting();

        mDevice = UiDevice.getInstance(getInstrumentation());
        mActivity = launchActivity(getInstrumentation().getTargetContext().getPackageName(),
                MyActivity.class, null);
        mPackageName = mActivity.getPackageName();
        mCM = (ConnectivityManager) mActivity.getSystemService(Context.CONNECTIVITY_SERVICE);
        mWifiManager = (WifiManager) mActivity.getSystemService(Context.WIFI_SERVICE);
        mRemoteSocketFactoryClient = new RemoteSocketFactoryClient(mActivity);
        mRemoteSocketFactoryClient.bind();
        mDevice.waitForIdle();
    }

    @Override
    public void tearDown() throws Exception {
        restorePrivateDnsSetting();
        mRemoteSocketFactoryClient.unbind();
        if (mCallback != null) {
            mCM.unregisterNetworkCallback(mCallback);
        }
        Log.i(TAG, "Stopping VPN");
        stopVpn();
        mActivity.finish();
        super.tearDown();
    }

    private void prepareVpn() throws Exception {
        final int REQUEST_ID = 42;

        // Attempt to prepare.
        Log.i(TAG, "Preparing VPN");
        Intent intent = VpnService.prepare(mActivity);

        if (intent != null) {
            // Start the confirmation dialog and click OK.
            mActivity.startActivityForResult(intent, REQUEST_ID);
            mDevice.waitForIdle();

            String packageName = intent.getComponent().getPackageName();
            String resourceIdRegex = "android:id/button1$|button_start_vpn";
            final UiObject okButton = new UiObject(new UiSelector()
                    .className("android.widget.Button")
                    .packageName(packageName)
                    .resourceIdMatches(resourceIdRegex));
            if (okButton.waitForExists(TIMEOUT_MS) == false) {
                mActivity.finishActivity(REQUEST_ID);
                fail("VpnService.prepare returned an Intent for '" + intent.getComponent() + "' " +
                     "to display the VPN confirmation dialog, but this test could not find the " +
                     "button to allow the VPN application to connect. Please ensure that the "  +
                     "component displays a button with a resource ID matching the regexp: '" +
                     resourceIdRegex + "'.");
            }

            // Click the button and wait for RESULT_OK.
            okButton.click();
            try {
                int result = mActivity.getResult(TIMEOUT_MS);
                if (result != MyActivity.RESULT_OK) {
                    fail("The VPN confirmation dialog did not return RESULT_OK when clicking on " +
                         "the button matching the regular expression '" + resourceIdRegex +
                         "' of " + intent.getComponent() + "'. Please ensure that clicking on " +
                         "that button allows the VPN application to connect. " +
                         "Return value: " + result);
                }
            } catch (InterruptedException e) {
                fail("VPN confirmation dialog did not return after " + TIMEOUT_MS + "ms");
            }

            // Now we should be prepared.
            intent = VpnService.prepare(mActivity);
            if (intent != null) {
                fail("VpnService.prepare returned non-null even after the VPN dialog " +
                     intent.getComponent() + "returned RESULT_OK.");
            }
        }
    }

    // TODO: Consider replacing arguments with a Builder.
    private void startVpn(
        String[] addresses, String[] routes, String allowedApplications,
        String disallowedApplications, @Nullable ProxyInfo proxyInfo,
        @Nullable ArrayList<Network> underlyingNetworks, boolean isAlwaysMetered) throws Exception {
        prepareVpn();

        // Register a callback so we will be notified when our VPN comes up.
        final NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_VPN)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build();
        mCallback = new NetworkCallback() {
            public void onAvailable(Network network) {
                synchronized (mLock) {
                    Log.i(TAG, "Got available callback for network=" + network);
                    mNetwork = network;
                    mLock.notify();
                }
            }
        };
        mCM.registerNetworkCallback(request, mCallback);  // Unregistered in tearDown.

        // Start the service and wait up for TIMEOUT_MS ms for the VPN to come up.
        Intent intent = new Intent(mActivity, MyVpnService.class)
                .putExtra(mPackageName + ".cmd", "connect")
                .putExtra(mPackageName + ".addresses", TextUtils.join(",", addresses))
                .putExtra(mPackageName + ".routes", TextUtils.join(",", routes))
                .putExtra(mPackageName + ".allowedapplications", allowedApplications)
                .putExtra(mPackageName + ".disallowedapplications", disallowedApplications)
                .putExtra(mPackageName + ".httpProxy", proxyInfo)
                .putParcelableArrayListExtra(
                    mPackageName + ".underlyingNetworks", underlyingNetworks)
                .putExtra(mPackageName + ".isAlwaysMetered", isAlwaysMetered);

        mActivity.startService(intent);
        synchronized (mLock) {
            if (mNetwork == null) {
                 Log.i(TAG, "bf mLock");
                 mLock.wait(TIMEOUT_MS);
                 Log.i(TAG, "af mLock");
            }
        }

        if (mNetwork == null) {
            fail("VPN did not become available after " + TIMEOUT_MS + "ms");
        }

        // Unfortunately, when the available callback fires, the VPN UID ranges are not yet
        // configured. Give the system some time to do so. http://b/18436087 .
        try { Thread.sleep(3000); } catch(InterruptedException e) {}
    }

    private void stopVpn() {
        // Register a callback so we will be notified when our VPN comes up.
        final NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_VPN)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build();
        mCallback = new NetworkCallback() {
            public void onLost(Network network) {
                synchronized (mLockShutdown) {
                    Log.i(TAG, "Got lost callback for network=" + network
                            + ",mNetwork = " + mNetwork);
                    if( mNetwork == network){
                        mLockShutdown.notify();
                    }
                }
            }
       };
        mCM.registerNetworkCallback(request, mCallback);  // Unregistered in tearDown.
        // Simply calling mActivity.stopService() won't stop the service, because the system binds
        // to the service for the purpose of sending it a revoke command if another VPN comes up,
        // and stopping a bound service has no effect. Instead, "start" the service again with an
        // Intent that tells it to disconnect.
        Intent intent = new Intent(mActivity, MyVpnService.class)
                .putExtra(mPackageName + ".cmd", "disconnect");
        mActivity.startService(intent);
        synchronized (mLockShutdown) {
            try {
                 Log.i(TAG, "bf mLockShutdown");
                 mLockShutdown.wait(TIMEOUT_MS);
                 Log.i(TAG, "af mLockShutdown");
            } catch(InterruptedException e) {}
        }
    }

    private static void closeQuietly(Closeable c) {
        if (c != null) {
            try {
                c.close();
            } catch (IOException e) {
            }
        }
    }

    private static void checkPing(String to) throws IOException, ErrnoException {
        InetAddress address = InetAddress.getByName(to);
        FileDescriptor s;
        final int LENGTH = 64;
        byte[] packet = new byte[LENGTH];
        byte[] header;

        // Construct a ping packet.
        Random random = new Random();
        random.nextBytes(packet);
        if (address instanceof Inet6Address) {
            s = Os.socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
            header = new byte[] { (byte) 0x80, (byte) 0x00, (byte) 0x00, (byte) 0x00 };
        } else {
            // Note that this doesn't actually work due to http://b/18558481 .
            s = Os.socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
            header = new byte[] { (byte) 0x08, (byte) 0x00, (byte) 0x00, (byte) 0x00 };
        }
        System.arraycopy(header, 0, packet, 0, header.length);

        // Send the packet.
        int port = random.nextInt(65534) + 1;
        Os.connect(s, address, port);
        Os.write(s, packet, 0, packet.length);

        // Expect a reply.
        StructPollfd pollfd = new StructPollfd();
        pollfd.events = (short) POLLIN;  // "error: possible loss of precision"
        pollfd.fd = s;
        int ret = Os.poll(new StructPollfd[] { pollfd }, SOCKET_TIMEOUT_MS);
        assertEquals("Expected reply after sending ping", 1, ret);

        byte[] reply = new byte[LENGTH];
        int read = Os.read(s, reply, 0, LENGTH);
        assertEquals(LENGTH, read);

        // Find out what the kernel set the ICMP ID to.
        InetSocketAddress local = (InetSocketAddress) Os.getsockname(s);
        port = local.getPort();
        packet[4] = (byte) ((port >> 8) & 0xff);
        packet[5] = (byte) (port & 0xff);

        // Check the contents.
        if (packet[0] == (byte) 0x80) {
            packet[0] = (byte) 0x81;
        } else {
            packet[0] = 0;
        }
        // Zero out the checksum in the reply so it matches the uninitialized checksum in packet.
        reply[2] = reply[3] = 0;
        MoreAsserts.assertEquals(packet, reply);
    }

    // Writes data to out and checks that it appears identically on in.
    private static void writeAndCheckData(
            OutputStream out, InputStream in, byte[] data) throws IOException {
        out.write(data, 0, data.length);
        out.flush();

        byte[] read = new byte[data.length];
        int bytesRead = 0, totalRead = 0;
        do {
            bytesRead = in.read(read, totalRead, read.length - totalRead);
            totalRead += bytesRead;
        } while (bytesRead >= 0 && totalRead < data.length);
        assertEquals(totalRead, data.length);
        MoreAsserts.assertEquals(data, read);
    }

    private void checkTcpReflection(String to, String expectedFrom) throws IOException {
        // Exercise TCP over the VPN by "connecting to ourselves". We open a server socket and a
        // client socket, and connect the client socket to a remote host, with the port of the
        // server socket. The PacketReflector reflects the packets, changing the source addresses
        // but not the ports, so our client socket is connected to our server socket, though both
        // sockets think their peers are on the "remote" IP address.

        // Open a listening socket.
        ServerSocket listen = new ServerSocket(0, 10, InetAddress.getByName("::"));

        // Connect the client socket to it.
        InetAddress toAddr = InetAddress.getByName(to);
        Socket client = new Socket();
        try {
            client.connect(new InetSocketAddress(toAddr, listen.getLocalPort()), SOCKET_TIMEOUT_MS);
            if (expectedFrom == null) {
                closeQuietly(listen);
                closeQuietly(client);
                fail("Expected connection to fail, but it succeeded.");
            }
        } catch (IOException e) {
            if (expectedFrom != null) {
                closeQuietly(listen);
                fail("Expected connection to succeed, but it failed.");
            } else {
                // We expected the connection to fail, and it did, so there's nothing more to test.
                return;
            }
        }

        // The connection succeeded, and we expected it to succeed. Send some data; if things are
        // working, the data will be sent to the VPN, reflected by the PacketReflector, and arrive
        // at our server socket. For good measure, send some data in the other direction.
        Socket server = null;
        try {
            // Accept the connection on the server side.
            listen.setSoTimeout(SOCKET_TIMEOUT_MS);
            server = listen.accept();
            checkConnectionOwnerUidTcp(client);
            checkConnectionOwnerUidTcp(server);
            // Check that the source and peer addresses are as expected.
            assertEquals(expectedFrom, client.getLocalAddress().getHostAddress());
            assertEquals(expectedFrom, server.getLocalAddress().getHostAddress());
            assertEquals(
                    new InetSocketAddress(toAddr, client.getLocalPort()),
                    server.getRemoteSocketAddress());
            assertEquals(
                    new InetSocketAddress(toAddr, server.getLocalPort()),
                    client.getRemoteSocketAddress());

            // Now write some data.
            final int LENGTH = 32768;
            byte[] data = new byte[LENGTH];
            new Random().nextBytes(data);

            // Make sure our writes don't block or time out, because we're single-threaded and can't
            // read and write at the same time.
            server.setReceiveBufferSize(LENGTH * 2);
            client.setSendBufferSize(LENGTH * 2);
            client.setSoTimeout(SOCKET_TIMEOUT_MS);
            server.setSoTimeout(SOCKET_TIMEOUT_MS);

            // Send some data from client to server, then from server to client.
            writeAndCheckData(client.getOutputStream(), server.getInputStream(), data);
            writeAndCheckData(server.getOutputStream(), client.getInputStream(), data);
        } finally {
            closeQuietly(listen);
            closeQuietly(client);
            closeQuietly(server);
        }
    }

    private void checkConnectionOwnerUidUdp(DatagramSocket s, boolean expectSuccess) {
        final int expectedUid = expectSuccess ? Process.myUid() : INVALID_UID;
        InetSocketAddress loc = new InetSocketAddress(s.getLocalAddress(), s.getLocalPort());
        InetSocketAddress rem = new InetSocketAddress(s.getInetAddress(), s.getPort());
        int uid = mCM.getConnectionOwnerUid(OsConstants.IPPROTO_UDP, loc, rem);
        assertEquals(expectedUid, uid);
    }

    private void checkConnectionOwnerUidTcp(Socket s) {
        final int expectedUid = Process.myUid();
        InetSocketAddress loc = new InetSocketAddress(s.getLocalAddress(), s.getLocalPort());
        InetSocketAddress rem = new InetSocketAddress(s.getInetAddress(), s.getPort());
        int uid = mCM.getConnectionOwnerUid(OsConstants.IPPROTO_TCP, loc, rem);
        assertEquals(expectedUid, uid);
    }

    private void checkUdpEcho(String to, String expectedFrom) throws IOException {
        DatagramSocket s;
        InetAddress address = InetAddress.getByName(to);
        if (address instanceof Inet6Address) {  // http://b/18094870
            s = new DatagramSocket(0, InetAddress.getByName("::"));
        } else {
            s = new DatagramSocket();
        }
        s.setSoTimeout(SOCKET_TIMEOUT_MS);

        Random random = new Random();
        byte[] data = new byte[random.nextInt(1650)];
        random.nextBytes(data);
        DatagramPacket p = new DatagramPacket(data, data.length);
        s.connect(address, 7);

        if (expectedFrom != null) {
            assertEquals("Unexpected source address: ",
                         expectedFrom, s.getLocalAddress().getHostAddress());
        }

        try {
            if (expectedFrom != null) {
                s.send(p);
                checkConnectionOwnerUidUdp(s, true);
                s.receive(p);
                MoreAsserts.assertEquals(data, p.getData());
            } else {
                try {
                    s.send(p);
                    s.receive(p);
                    fail("Received unexpected reply");
                } catch (IOException expected) {
                    checkConnectionOwnerUidUdp(s, false);
                }
            }
        } finally {
            s.close();
        }
    }

    private void checkTrafficOnVpn() throws Exception {
        checkUdpEcho("192.0.2.251", "192.0.2.2");
        checkUdpEcho("2001:db8:dead:beef::f00", "2001:db8:1:2::ffe");
        checkPing("2001:db8:dead:beef::f00");
        checkTcpReflection("192.0.2.252", "192.0.2.2");
        checkTcpReflection("2001:db8:dead:beef::f00", "2001:db8:1:2::ffe");
    }

    private void checkNoTrafficOnVpn() throws Exception {
        checkUdpEcho("192.0.2.251", null);
        checkUdpEcho("2001:db8:dead:beef::f00", null);
        checkTcpReflection("192.0.2.252", null);
        checkTcpReflection("2001:db8:dead:beef::f00", null);
    }

    private FileDescriptor openSocketFd(String host, int port, int timeoutMs) throws Exception {
        Socket s = new Socket(host, port);
        s.setSoTimeout(timeoutMs);
        // Dup the filedescriptor so ParcelFileDescriptor's finalizer doesn't garbage collect it
        // and cause our fd to become invalid. http://b/35927643 .
        FileDescriptor fd = Os.dup(ParcelFileDescriptor.fromSocket(s).getFileDescriptor());
        s.close();
        return fd;
    }

    private FileDescriptor openSocketFdInOtherApp(
            String host, int port, int timeoutMs) throws Exception {
        Log.d(TAG, String.format("Creating test socket in UID=%d, my UID=%d",
                mRemoteSocketFactoryClient.getUid(), Os.getuid()));
        FileDescriptor fd = mRemoteSocketFactoryClient.openSocketFd(host, port, TIMEOUT_MS);
        return fd;
    }

    private void sendRequest(FileDescriptor fd, String host) throws Exception {
        String request = "GET /generate_204 HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: keep-alive\r\n\r\n";
        byte[] requestBytes = request.getBytes(StandardCharsets.UTF_8);
        int ret = Os.write(fd, requestBytes, 0, requestBytes.length);
        Log.d(TAG, "Wrote " + ret + "bytes");

        String expected = "HTTP/1.1 204 No Content\r\n";
        byte[] response = new byte[expected.length()];
        Os.read(fd, response, 0, response.length);

        String actual = new String(response, StandardCharsets.UTF_8);
        assertEquals(expected, actual);
        Log.d(TAG, "Got response: " + actual);
    }

    private void assertSocketStillOpen(FileDescriptor fd, String host) throws Exception {
        try {
            assertTrue(fd.valid());
            sendRequest(fd, host);
            assertTrue(fd.valid());
        } finally {
            Os.close(fd);
        }
    }

    private void assertSocketClosed(FileDescriptor fd, String host) throws Exception {
        try {
            assertTrue(fd.valid());
            sendRequest(fd, host);
            fail("Socket opened before VPN connects should be closed when VPN connects");
        } catch (ErrnoException expected) {
            assertEquals(ECONNABORTED, expected.errno);
            assertTrue(fd.valid());
        } finally {
            Os.close(fd);
        }
    }

    private ContentResolver getContentResolver() {
        return getInstrumentation().getContext().getContentResolver();
    }

    private boolean isPrivateDnsInStrictMode() {
        return PRIVATE_DNS_MODE_PROVIDER_HOSTNAME.equals(
                Settings.Global.getString(getContentResolver(), PRIVATE_DNS_MODE_SETTING));
    }

    private void storePrivateDnsSetting() {
        mOldPrivateDnsMode = Settings.Global.getString(getContentResolver(),
                PRIVATE_DNS_MODE_SETTING);
        mOldPrivateDnsSpecifier = Settings.Global.getString(getContentResolver(),
                PRIVATE_DNS_SPECIFIER_SETTING);
    }

    private void restorePrivateDnsSetting() {
        Settings.Global.putString(getContentResolver(), PRIVATE_DNS_MODE_SETTING,
                mOldPrivateDnsMode);
        Settings.Global.putString(getContentResolver(), PRIVATE_DNS_SPECIFIER_SETTING,
                mOldPrivateDnsSpecifier);
    }

    // TODO: replace with CtsNetUtils.awaitPrivateDnsSetting in Q or above.
    private void expectPrivateDnsHostname(final String hostname) throws Exception {
        final NetworkRequest request = new NetworkRequest.Builder()
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
                .build();
        final CountDownLatch latch = new CountDownLatch(1);
        final NetworkCallback callback = new NetworkCallback() {
            @Override
            public void onLinkPropertiesChanged(Network network, LinkProperties lp) {
                if (network.equals(mNetwork) &&
                        Objects.equals(lp.getPrivateDnsServerName(), hostname)) {
                    latch.countDown();
                }
            }
        };

        mCM.registerNetworkCallback(request, callback);

        try {
            assertTrue("Private DNS hostname was not " + hostname + " after " + TIMEOUT_MS + "ms",
                    latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            mCM.unregisterNetworkCallback(callback);
        }
    }

    private void setAndVerifyPrivateDns(boolean strictMode) throws Exception {
        final ContentResolver cr = getInstrumentation().getContext().getContentResolver();
        String privateDnsHostname;

        if (strictMode) {
            privateDnsHostname = "vpncts-nx.metric.gstatic.com";
            Settings.Global.putString(cr, PRIVATE_DNS_SPECIFIER_SETTING, privateDnsHostname);
            Settings.Global.putString(cr, PRIVATE_DNS_MODE_SETTING,
                    PRIVATE_DNS_MODE_PROVIDER_HOSTNAME);
        } else {
            Settings.Global.putString(cr, PRIVATE_DNS_MODE_SETTING, PRIVATE_DNS_MODE_OPPORTUNISTIC);
            privateDnsHostname = null;
        }

        expectPrivateDnsHostname(privateDnsHostname);

        String randomName = "vpncts-" + new Random().nextInt(1000000000) + "-ds.metric.gstatic.com";
        if (strictMode) {
            // Strict mode private DNS is enabled. DNS lookups should fail, because the private DNS
            // server name is invalid.
            try {
                InetAddress.getByName(randomName);
                fail("VPN DNS lookup should fail with private DNS enabled");
            } catch (UnknownHostException expected) {
            }
        } else {
            // Strict mode private DNS is disabled. DNS lookup should succeed, because the VPN
            // provides no DNS servers, and thus DNS falls through to the default network.
            assertNotNull("VPN DNS lookup should succeed with private DNS disabled",
                    InetAddress.getByName(randomName));
        }
    }

    // Tests that strict mode private DNS is used on VPNs.
    private void checkStrictModePrivateDns() throws Exception {
        final boolean initialMode = isPrivateDnsInStrictMode();
        setAndVerifyPrivateDns(!initialMode);
        setAndVerifyPrivateDns(initialMode);
    }

    public void testDefault() throws Exception {
        if (!supportedHardware()) return;
        // If adb TCP port opened, this test may running by adb over network.
        // All of socket would be destroyed in this test. So this test don't
        // support adb over network, see b/119382723.
        if (SystemProperties.getInt("persist.adb.tcp.port", -1) > -1
                || SystemProperties.getInt("service.adb.tcp.port", -1) > -1) {
            Log.i(TAG, "adb is running over the network, so skip this test");
            return;
        }

        final BlockingBroadcastReceiver receiver = new BlockingBroadcastReceiver(
                getInstrumentation().getTargetContext(), MyVpnService.ACTION_ESTABLISHED);
        receiver.register();

        FileDescriptor fd = openSocketFdInOtherApp(TEST_HOST, 80, TIMEOUT_MS);

        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                 new String[] {"0.0.0.0/0", "::/0"},
                 "", "", null, null /* underlyingNetworks */, false /* isAlwaysMetered */);

        final Intent intent = receiver.awaitForBroadcast(TimeUnit.MINUTES.toMillis(1));
        assertNotNull("Failed to receive broadcast from VPN service", intent);
        assertFalse("Wrong VpnService#isAlwaysOn",
                intent.getBooleanExtra(MyVpnService.EXTRA_ALWAYS_ON, true));
        assertFalse("Wrong VpnService#isLockdownEnabled",
                intent.getBooleanExtra(MyVpnService.EXTRA_LOCKDOWN_ENABLED, true));

        assertSocketClosed(fd, TEST_HOST);

        checkTrafficOnVpn();

        checkStrictModePrivateDns();

        receiver.unregisterQuietly();
    }

    public void testAppAllowed() throws Exception {
        if (!supportedHardware()) return;

        FileDescriptor fd = openSocketFdInOtherApp(TEST_HOST, 80, TIMEOUT_MS);

        // Shell app must not be put in here or it would kill the ADB-over-network use case
        String allowedApps = mRemoteSocketFactoryClient.getPackageName() + "," + mPackageName;
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                 new String[] {"192.0.2.0/24", "2001:db8::/32"},
                 allowedApps, "", null, null /* underlyingNetworks */, false /* isAlwaysMetered */);

        assertSocketClosed(fd, TEST_HOST);

        checkTrafficOnVpn();

        checkStrictModePrivateDns();
    }

    public void testAppDisallowed() throws Exception {
        if (!supportedHardware()) return;

        FileDescriptor localFd = openSocketFd(TEST_HOST, 80, TIMEOUT_MS);
        FileDescriptor remoteFd = openSocketFdInOtherApp(TEST_HOST, 80, TIMEOUT_MS);

        String disallowedApps = mRemoteSocketFactoryClient.getPackageName() + "," + mPackageName;
        // If adb TCP port opened, this test may running by adb over TCP.
        // Add com.android.shell appllication into blacklist to exclude adb socket for VPN test,
        // see b/119382723.
        // Note: The test don't support running adb over network for root device
        disallowedApps = disallowedApps + ",com.android.shell";
        Log.i(TAG, "Append shell app to disallowedApps: " + disallowedApps);
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                 new String[] {"192.0.2.0/24", "2001:db8::/32"},
                 "", disallowedApps, null, null /* underlyingNetworks */,
                 false /* isAlwaysMetered */);

        assertSocketStillOpen(localFd, TEST_HOST);
        assertSocketStillOpen(remoteFd, TEST_HOST);

        checkNoTrafficOnVpn();
    }

    public void testGetConnectionOwnerUidSecurity() throws Exception {
        if (!supportedHardware()) return;

        DatagramSocket s;
        InetAddress address = InetAddress.getByName("localhost");
        s = new DatagramSocket();
        s.setSoTimeout(SOCKET_TIMEOUT_MS);
        s.connect(address, 7);
        InetSocketAddress loc = new InetSocketAddress(s.getLocalAddress(), s.getLocalPort());
        InetSocketAddress rem = new InetSocketAddress(s.getInetAddress(), s.getPort());
        try {
            int uid = mCM.getConnectionOwnerUid(OsConstants.IPPROTO_TCP, loc, rem);
            fail("Only an active VPN app may call this API.");
        } catch (SecurityException expected) {
            return;
        }
    }

    public void testSetProxy() throws  Exception {
        if (!supportedHardware()) return;
        ProxyInfo initialProxy = mCM.getDefaultProxy();
        // Receiver for the proxy change broadcast.
        BlockingBroadcastReceiver proxyBroadcastReceiver = new ProxyChangeBroadcastReceiver();
        proxyBroadcastReceiver.register();

        String allowedApps = mPackageName;
        ProxyInfo testProxyInfo = ProxyInfo.buildDirectProxy("10.0.0.1", 8888);
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "",
                testProxyInfo, null /* underlyingNetworks */, false /* isAlwaysMetered */);

        // Check that the proxy change broadcast is received
        try {
            assertNotNull("No proxy change was broadcast when VPN is connected.",
                    proxyBroadcastReceiver.awaitForBroadcast());
        } finally {
            proxyBroadcastReceiver.unregisterQuietly();
        }

        // Proxy is set correctly in network and in link properties.
        assertNetworkHasExpectedProxy(testProxyInfo, mNetwork);
        assertDefaultProxy(testProxyInfo);

        proxyBroadcastReceiver = new ProxyChangeBroadcastReceiver();
        proxyBroadcastReceiver.register();
        stopVpn();
        try {
            assertNotNull("No proxy change was broadcast when VPN was disconnected.",
                    proxyBroadcastReceiver.awaitForBroadcast());
        } finally {
            proxyBroadcastReceiver.unregisterQuietly();
        }

        // After disconnecting from VPN, the proxy settings are the ones of the initial network.
        assertDefaultProxy(initialProxy);
    }

    public void testSetProxyDisallowedApps() throws Exception {
        if (!supportedHardware()) return;
        ProxyInfo initialProxy = mCM.getDefaultProxy();

        // If adb TCP port opened, this test may running by adb over TCP.
        // Add com.android.shell appllication into blacklist to exclude adb socket for VPN test,
        // see b/119382723.
        // Note: The test don't support running adb over network for root device
        String disallowedApps = mPackageName + ",com.android.shell";
        ProxyInfo testProxyInfo = ProxyInfo.buildDirectProxy("10.0.0.1", 8888);
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, "", disallowedApps,
                testProxyInfo, null /* underlyingNetworks */, false /* isAlwaysMetered */);

        // The disallowed app does has the proxy configs of the default network.
        assertNetworkHasExpectedProxy(initialProxy, mCM.getActiveNetwork());
        assertDefaultProxy(initialProxy);
    }

    public void testNoProxy() throws Exception {
        if (!supportedHardware()) return;
        ProxyInfo initialProxy = mCM.getDefaultProxy();
        BlockingBroadcastReceiver proxyBroadcastReceiver = new ProxyChangeBroadcastReceiver();
        proxyBroadcastReceiver.register();
        String allowedApps = mPackageName;
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "", null,
                null /* underlyingNetworks */, false /* isAlwaysMetered */);

        try {
            assertNotNull("No proxy change was broadcast.",
                    proxyBroadcastReceiver.awaitForBroadcast());
        } finally {
            proxyBroadcastReceiver.unregisterQuietly();
        }

        // The VPN network has no proxy set.
        assertNetworkHasExpectedProxy(null, mNetwork);

        proxyBroadcastReceiver = new ProxyChangeBroadcastReceiver();
        proxyBroadcastReceiver.register();
        stopVpn();
        try {
            assertNotNull("No proxy change was broadcast.",
                    proxyBroadcastReceiver.awaitForBroadcast());
        } finally {
            proxyBroadcastReceiver.unregisterQuietly();
        }
        // After disconnecting from VPN, the proxy settings are the ones of the initial network.
        assertDefaultProxy(initialProxy);
        assertNetworkHasExpectedProxy(initialProxy, mCM.getActiveNetwork());
    }

    public void testBindToNetworkWithProxy() throws Exception {
        if (!supportedHardware()) return;
        String allowedApps = mPackageName;
        Network initialNetwork = mCM.getActiveNetwork();
        ProxyInfo initialProxy = mCM.getDefaultProxy();
        ProxyInfo testProxyInfo = ProxyInfo.buildDirectProxy("10.0.0.1", 8888);
        // Receiver for the proxy change broadcast.
        BlockingBroadcastReceiver proxyBroadcastReceiver = new ProxyChangeBroadcastReceiver();
        proxyBroadcastReceiver.register();
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "",
                testProxyInfo, null /* underlyingNetworks */, false /* isAlwaysMetered */);

        assertDefaultProxy(testProxyInfo);
        mCM.bindProcessToNetwork(initialNetwork);
        try {
            assertNotNull("No proxy change was broadcast.",
                proxyBroadcastReceiver.awaitForBroadcast());
        } finally {
            proxyBroadcastReceiver.unregisterQuietly();
        }
        assertDefaultProxy(initialProxy);
    }

    public void testVpnMeterednessWithNoUnderlyingNetwork() throws Exception {
        if (!supportedHardware()) {
            return;
        }
        // VPN is not routing any traffic i.e. its underlying networks is an empty array.
        ArrayList<Network> underlyingNetworks = new ArrayList<>();
        String allowedApps = mPackageName;

        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "", null,
                underlyingNetworks, false /* isAlwaysMetered */);

        // VPN should now be the active network.
        assertEquals(mNetwork, mCM.getActiveNetwork());
        assertVpnTransportContains(NetworkCapabilities.TRANSPORT_VPN);
        // VPN with no underlying networks should be metered by default.
        assertTrue(isNetworkMetered(mNetwork));
        assertTrue(mCM.isActiveNetworkMetered());
    }

    public void testVpnMeterednessWithNullUnderlyingNetwork() throws Exception {
        if (!supportedHardware()) {
            return;
        }
        Network underlyingNetwork = mCM.getActiveNetwork();
        if (underlyingNetwork == null) {
            Log.i(TAG, "testVpnMeterednessWithNullUnderlyingNetwork cannot execute"
                    + " unless there is an active network");
            return;
        }
        // VPN tracks platform default.
        ArrayList<Network> underlyingNetworks = null;
        String allowedApps = mPackageName;

        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "", null,
                underlyingNetworks, false /*isAlwaysMetered */);

        // Ensure VPN transports contains underlying network's transports.
        assertVpnTransportContains(underlyingNetwork);
        // Its meteredness should be same as that of underlying network.
        assertEquals(isNetworkMetered(underlyingNetwork), isNetworkMetered(mNetwork));
        // Meteredness based on VPN capabilities and CM#isActiveNetworkMetered should be in sync.
        assertEquals(isNetworkMetered(mNetwork), mCM.isActiveNetworkMetered());
    }

    public void testVpnMeterednessWithNonNullUnderlyingNetwork() throws Exception {
        if (!supportedHardware()) {
            return;
        }
        Network underlyingNetwork = mCM.getActiveNetwork();
        if (underlyingNetwork == null) {
            Log.i(TAG, "testVpnMeterednessWithNonNullUnderlyingNetwork cannot execute"
                    + " unless there is an active network");
            return;
        }
        // VPN explicitly declares WiFi to be its underlying network.
        ArrayList<Network> underlyingNetworks = new ArrayList<>(1);
        underlyingNetworks.add(underlyingNetwork);
        String allowedApps = mPackageName;

        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "", null,
                underlyingNetworks, false /* isAlwaysMetered */);

        // Ensure VPN transports contains underlying network's transports.
        assertVpnTransportContains(underlyingNetwork);
        // Its meteredness should be same as that of underlying network.
        assertEquals(isNetworkMetered(underlyingNetwork), isNetworkMetered(mNetwork));
        // Meteredness based on VPN capabilities and CM#isActiveNetworkMetered should be in sync.
        assertEquals(isNetworkMetered(mNetwork), mCM.isActiveNetworkMetered());
    }

    public void testAlwaysMeteredVpnWithNullUnderlyingNetwork() throws Exception {
        if (!supportedHardware()) {
            return;
        }
        Network underlyingNetwork = mCM.getActiveNetwork();
        if (underlyingNetwork == null) {
            Log.i(TAG, "testAlwaysMeteredVpnWithNullUnderlyingNetwork cannot execute"
                    + " unless there is an active network");
            return;
        }
        // VPN tracks platform default.
        ArrayList<Network> underlyingNetworks = null;
        String allowedApps = mPackageName;
        boolean isAlwaysMetered = true;

        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "", null,
                underlyingNetworks, isAlwaysMetered);

        // VPN's meteredness does not depend on underlying network since it is always metered.
        assertTrue(isNetworkMetered(mNetwork));
        assertTrue(mCM.isActiveNetworkMetered());
    }

    public void testAlwaysMeteredVpnWithNonNullUnderlyingNetwork() throws Exception {
        if (!supportedHardware()) {
            return;
        }
        Network underlyingNetwork = mCM.getActiveNetwork();
        if (underlyingNetwork == null) {
            Log.i(TAG, "testAlwaysMeteredVpnWithNonNullUnderlyingNetwork cannot execute"
                    + " unless there is an active network");
            return;
        }
        // VPN explicitly declares its underlying network.
        ArrayList<Network> underlyingNetworks = new ArrayList<>(1);
        underlyingNetworks.add(underlyingNetwork);
        String allowedApps = mPackageName;
        boolean isAlwaysMetered = true;

        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                new String[] {"0.0.0.0/0", "::/0"}, allowedApps, "", null,
                underlyingNetworks, isAlwaysMetered);

        // VPN's meteredness does not depend on underlying network since it is always metered.
        assertTrue(isNetworkMetered(mNetwork));
        assertTrue(mCM.isActiveNetworkMetered());
    }

    public void testB141603906() throws Exception {
        final InetSocketAddress src = new InetSocketAddress(0);
        final InetSocketAddress dst = new InetSocketAddress(0);
        final int NUM_THREADS = 8;
        final int NUM_SOCKETS = 5000;
        final Thread[] threads = new Thread[NUM_THREADS];
        startVpn(new String[] {"192.0.2.2/32", "2001:db8:1:2::ffe/128"},
                 new String[] {"0.0.0.0/0", "::/0"},
                 "" /* allowedApplications */, "com.android.shell" /* disallowedApplications */,
                null /* proxyInfo */, null /* underlyingNetworks */, false /* isAlwaysMetered */);

        for (int i = 0; i < NUM_THREADS; i++) {
            threads[i] = new Thread(() -> {
                for (int j = 0; j < NUM_SOCKETS; j++) {
                    mCM.getConnectionOwnerUid(IPPROTO_TCP, src, dst);
                }
            });
        }
        for (Thread thread : threads) {
            thread.start();
        }
        for (Thread thread : threads) {
            thread.join();
        }
        stopVpn();
    }

    private boolean isNetworkMetered(Network network) {
        NetworkCapabilities nc = mCM.getNetworkCapabilities(network);
        return !nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);
    }

    private void assertVpnTransportContains(Network underlyingNetwork) {
        int[] transports = mCM.getNetworkCapabilities(underlyingNetwork).getTransportTypes();
        assertVpnTransportContains(transports);
    }

    private void assertVpnTransportContains(int... transports) {
        NetworkCapabilities vpnCaps = mCM.getNetworkCapabilities(mNetwork);
        for (int transport : transports) {
            assertTrue(vpnCaps.hasTransport(transport));
        }
    }

    private void assertDefaultProxy(ProxyInfo expected) {
        assertEquals("Incorrect proxy config.", expected, mCM.getDefaultProxy());
        String expectedHost = expected == null ? null : expected.getHost();
        String expectedPort = expected == null ? null : String.valueOf(expected.getPort());
        assertEquals("Incorrect proxy host system property.", expectedHost,
            System.getProperty("http.proxyHost"));
        assertEquals("Incorrect proxy port system property.", expectedPort,
            System.getProperty("http.proxyPort"));
    }

    private void assertNetworkHasExpectedProxy(ProxyInfo expected, Network network) {
        LinkProperties lp = mCM.getLinkProperties(network);
        assertNotNull("The network link properties object is null.", lp);
        assertEquals("Incorrect proxy config.", expected, lp.getHttpProxy());

        assertEquals(expected, mCM.getProxyForNetwork(network));
    }

    class ProxyChangeBroadcastReceiver extends BlockingBroadcastReceiver {
        private boolean received;

        public ProxyChangeBroadcastReceiver() {
            super(VpnTest.this.getInstrumentation().getContext(), Proxy.PROXY_CHANGE_ACTION);
            received = false;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (!received) {
                // Do not call onReceive() more than once.
                super.onReceive(context, intent);
            }
            received = true;
        }
    }
}
