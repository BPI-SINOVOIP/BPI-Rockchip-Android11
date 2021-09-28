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

package android.net.netlink;

import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.IPPROTO_TCP;
import static android.system.OsConstants.IPPROTO_UDP;
import static android.system.OsConstants.SOCK_DGRAM;
import static android.system.OsConstants.SOCK_STREAM;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assume.assumeTrue;

import android.app.Instrumentation;
import android.content.Context;
import android.net.ConnectivityManager;
import android.os.Process;
import android.system.Os;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.networkstack.apishim.common.ShimUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileDescriptor;
import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class InetDiagSocketIntegrationTest {
    private ConnectivityManager mCm;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = instrumentation.getTargetContext();
        mCm = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    private class Connection {
        public int socketDomain;
        public int socketType;
        public InetAddress localAddress;
        public InetAddress remoteAddress;
        public InetAddress localhostAddress;
        public InetSocketAddress local;
        public InetSocketAddress remote;
        public int protocol;
        public FileDescriptor localFd;
        public FileDescriptor remoteFd;

        public FileDescriptor createSocket() throws Exception {
            return Os.socket(socketDomain, socketType, protocol);
        }

        Connection(String to, String from) throws Exception {
            remoteAddress = InetAddress.getByName(to);
            if (from != null) {
                localAddress = InetAddress.getByName(from);
            } else {
                localAddress = (remoteAddress instanceof Inet4Address)
                        ? Inet4Address.getByName("localhost") : Inet6Address.getByName("::");
            }
            if ((localAddress instanceof Inet4Address) && (remoteAddress instanceof Inet4Address)) {
                socketDomain = AF_INET;
                localhostAddress = Inet4Address.getByName("localhost");
            } else {
                socketDomain = AF_INET6;
                localhostAddress = Inet6Address.getByName("::");
            }
        }

        public void close() throws Exception {
            Os.close(localFd);
        }
    }

    private class TcpConnection extends Connection {
        TcpConnection(String to, String from) throws Exception {
            super(to, from);
            protocol = IPPROTO_TCP;
            socketType = SOCK_STREAM;

            remoteFd = createSocket();
            Os.bind(remoteFd, remoteAddress, 0);
            Os.listen(remoteFd, 10);
            int remotePort = ((InetSocketAddress) Os.getsockname(remoteFd)).getPort();

            localFd = createSocket();
            Os.bind(localFd, localAddress, 0);
            Os.connect(localFd, remoteAddress, remotePort);

            local = (InetSocketAddress) Os.getsockname(localFd);
            remote = (InetSocketAddress) Os.getpeername(localFd);
        }

        public void close() throws Exception {
            super.close();
            Os.close(remoteFd);
        }
    }
    private class UdpConnection extends Connection {
        UdpConnection(String to, String from) throws Exception {
            super(to, from);
            protocol = IPPROTO_UDP;
            socketType = SOCK_DGRAM;

            remoteFd = null;
            localFd = createSocket();
            Os.bind(localFd, localAddress, 0);

            Os.connect(localFd, remoteAddress, 7);
            local = (InetSocketAddress) Os.getsockname(localFd);
            remote = new InetSocketAddress(remoteAddress, 7);
        }
    }

    private void checkConnectionOwnerUid(int protocol, InetSocketAddress local,
                                         InetSocketAddress remote, boolean expectSuccess) {
        final int uid = mCm.getConnectionOwnerUid(protocol, local, remote);

        if (expectSuccess) {
            assertEquals(Process.myUid(), uid);
        } else {
            assertNotEquals(Process.myUid(), uid);
        }
    }

    private int findLikelyFreeUdpPort(UdpConnection conn) throws Exception {
        UdpConnection udp = new UdpConnection(conn.remoteAddress.getHostAddress(),
                conn.localAddress.getHostAddress());
        final int localPort = udp.local.getPort();
        udp.close();
        return localPort;
    }

    /**
     * Create a test connection for UDP and TCP sockets and verify that this
     * {protocol, local, remote} socket result in receiving a valid UID.
     */
    public void checkGetConnectionOwnerUid(String to, String from) throws Exception {
        TcpConnection tcp = new TcpConnection(to, from);
        checkConnectionOwnerUid(tcp.protocol, tcp.local, tcp.remote, true);
        checkConnectionOwnerUid(IPPROTO_UDP, tcp.local, tcp.remote, false);
        checkConnectionOwnerUid(tcp.protocol, new InetSocketAddress(0), tcp.remote, false);
        checkConnectionOwnerUid(tcp.protocol, tcp.local, new InetSocketAddress(0), false);
        tcp.close();

        UdpConnection udp = new UdpConnection(to, from);
        checkConnectionOwnerUid(udp.protocol, udp.local, udp.remote, true);
        checkConnectionOwnerUid(IPPROTO_TCP, udp.local, udp.remote, false);
        checkConnectionOwnerUid(udp.protocol, new InetSocketAddress(findLikelyFreeUdpPort(udp)),
                udp.remote, false);
        udp.close();
    }

    @Test
    public void testGetConnectionOwnerUid() throws Exception {
        // Skip the test for API <= Q, as b/141603906 this was only fixed in Q-QPR2
        assumeTrue(ShimUtils.isAtLeastR());
        checkGetConnectionOwnerUid("::", null);
        checkGetConnectionOwnerUid("::", "::");
        checkGetConnectionOwnerUid("0.0.0.0", null);
        checkGetConnectionOwnerUid("0.0.0.0", "0.0.0.0");
        checkGetConnectionOwnerUid("127.0.0.1", null);
        checkGetConnectionOwnerUid("127.0.0.1", "127.0.0.2");
        checkGetConnectionOwnerUid("::1", null);
        checkGetConnectionOwnerUid("::1", "::1");
    }

    /* Verify fix for b/141603906 */
    @Test
    public void testB141603906() throws Exception {
        // Skip the test for API <= Q, as b/141603906 this was only fixed in Q-QPR2
        assumeTrue(ShimUtils.isAtLeastR());
        final InetSocketAddress src = new InetSocketAddress(0);
        final InetSocketAddress dst = new InetSocketAddress(0);
        final int numThreads = 8;
        final int numSockets = 5000;
        final Thread[] threads = new Thread[numThreads];

        for (int i = 0; i < numThreads; i++) {
            threads[i] = new Thread(() -> {
                for (int j = 0; j < numSockets; j++) {
                    mCm.getConnectionOwnerUid(IPPROTO_TCP, src, dst);
                }
            });
        }

        for (Thread thread : threads) {
            thread.start();
        }

        for (Thread thread : threads) {
            thread.join();
        }
    }
}
