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

package com.android.internal.net.ipsec.ike;

import static android.system.OsConstants.AF_INET6;
import static android.system.OsConstants.F_SETFL;
import static android.system.OsConstants.IPPROTO_UDP;
import static android.system.OsConstants.SOCK_DGRAM;
import static android.system.OsConstants.SOCK_NONBLOCK;

import android.net.InetAddresses;
import android.net.Network;
import android.os.Handler;
import android.system.ErrnoException;
import android.system.Os;

import com.android.internal.annotations.VisibleForTesting;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.InetAddress;
import java.util.HashMap;
import java.util.Map;

/**
 * IkeUdp6Socket uses an IPv6-bound {@link FileDescriptor} to send and receive IKE packets.
 *
 * <p>Caller MUST provide one {@link Network} when trying to get an instance of IkeUdp6Socket. Each
 * {@link Network} will only be bound to by one IkeUdp6Socket instance. When caller requests an
 * IkeUdp6Socket with an already bound {@link Network}, the existing instance will be returned.
 */
public final class IkeUdp6Socket extends IkeUdpSocket {
    private static final String TAG = IkeUdp6Socket.class.getSimpleName();

    private static final InetAddress INADDR_ANY = InetAddresses.parseNumericAddress("::");

    // Map from Network to IkeUdp6Socket instances.
    private static Map<Network, IkeUdp6Socket> sNetworkToUdp6SocketMap = new HashMap<>();

    private IkeUdp6Socket(FileDescriptor socket, Network network, Handler handler) {
        super(socket, network, handler == null ? new Handler() : handler);
    }

    /**
     * Get an IkeUdp6Socket instance.
     *
     * <p>Return the existing IkeUdp6Socket instance if it has been created for the input Network.
     * Otherwise, create and return a new IkeUdp6Socket instance.
     *
     * @param network the Network this socket will be bound to
     * @param ikeSession the IkeSessionStateMachine that is requesting an IkeUdp6Socket.
     * @return an IkeUdp6Socket instance
     */
    public static IkeUdp6Socket getInstance(Network network, IkeSessionStateMachine ikeSession)
            throws ErrnoException, IOException {
        return getInstance(network, ikeSession, null);
    }

    // package protected; for testing purposes.
    @VisibleForTesting
    static IkeUdp6Socket getInstance(
            Network network, IkeSessionStateMachine ikeSession, Handler handler)
            throws ErrnoException, IOException {
        IkeUdp6Socket ikeSocket = sNetworkToUdp6SocketMap.get(network);
        if (ikeSocket == null) {
            FileDescriptor sock = Os.socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            Os.bind(sock, INADDR_ANY, 0);

            // {@link PacketReader} requires non-blocking I/O access. Set SOCK_NONBLOCK here.
            Os.fcntlInt(sock, F_SETFL, SOCK_DGRAM | SOCK_NONBLOCK);
            network.bindSocket(sock);

            ikeSocket = new IkeUdp6Socket(sock, network, handler);

            // Create and register FileDescriptor for receiving IKE packet on current thread.
            ikeSocket.start();

            sNetworkToUdp6SocketMap.put(network, ikeSocket);
        }
        ikeSocket.mAliveIkeSessions.add(ikeSession);
        return ikeSocket;
    }

    /** Implement {@link AutoCloseable#close()} */
    @Override
    public void close() {
        sNetworkToUdp6SocketMap.remove(getNetwork());

        super.close();
    }
}
