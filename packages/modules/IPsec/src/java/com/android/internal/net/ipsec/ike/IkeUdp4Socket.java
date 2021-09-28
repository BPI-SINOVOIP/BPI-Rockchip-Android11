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

import static android.system.OsConstants.AF_INET;
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
 * IkeUdp4Socket uses an IPv4-bound {@link FileDescriptor} to send and receive IKE packets.
 *
 * <p>Caller MUST provide one {@link Network} when trying to get an instance of IkeUdp4Socket. Each
 * {@link Network} will only be bound to by one IkeUdp4Socket instance. When caller requests an
 * IkeUdp4Socket with an already bound {@link Network}, the existing instance will be returned.
 */
public final class IkeUdp4Socket extends IkeUdpSocket {
    private static final String TAG = IkeUdp4Socket.class.getSimpleName();

    private static final InetAddress INADDR_ANY = InetAddresses.parseNumericAddress("0.0.0.0");

    // Map from Network to IkeUdp4Socket instances.
    private static Map<Network, IkeUdp4Socket> sNetworkToUdp4SocketMap = new HashMap<>();

    private IkeUdp4Socket(FileDescriptor socket, Network network, Handler handler) {
        super(socket, network, handler == null ? new Handler() : handler);
    }

    /**
     * Get an IkeUdp4Socket instance.
     *
     * <p>Return the existing IkeUdp4Socket instance if it has been created for the input Network.
     * Otherwise, create and return a new IkeUdp4Socket instance.
     *
     * @param network the Network this socket will be bound to
     * @param ikeSession the IkeSessionStateMachine that is requesting an IkeUdp4Socket.
     * @return an IkeUdp4Socket instance
     */
    public static IkeUdp4Socket getInstance(Network network, IkeSessionStateMachine ikeSession)
            throws ErrnoException, IOException {
        return getInstance(network, ikeSession, null);
    }

    // package protected; for testing purposes.
    @VisibleForTesting
    static IkeUdp4Socket getInstance(
            Network network, IkeSessionStateMachine ikeSession, Handler handler)
            throws ErrnoException, IOException {
        IkeUdp4Socket ikeSocket = sNetworkToUdp4SocketMap.get(network);
        if (ikeSocket == null) {
            FileDescriptor sock = Os.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            Os.bind(sock, INADDR_ANY, 0);

            // {@link PacketReader} requires non-blocking I/O access. Set SOCK_NONBLOCK here.
            Os.fcntlInt(sock, F_SETFL, SOCK_DGRAM | SOCK_NONBLOCK);
            network.bindSocket(sock);

            ikeSocket = new IkeUdp4Socket(sock, network, handler);

            // Create and register FileDescriptor for receiving IKE packet on current thread.
            ikeSocket.start();

            sNetworkToUdp4SocketMap.put(network, ikeSocket);
        }
        ikeSocket.mAliveIkeSessions.add(ikeSession);
        return ikeSocket;
    }

    /** Implement {@link AutoCloseable#close()} */
    @Override
    public void close() {
        sNetworkToUdp4SocketMap.remove(getNetwork());

        super.close();
    }
}
