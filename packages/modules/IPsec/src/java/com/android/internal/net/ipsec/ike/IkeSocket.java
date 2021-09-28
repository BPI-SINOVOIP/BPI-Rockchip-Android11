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

package com.android.internal.net.ipsec.ike;

import static android.net.ipsec.ike.IkeManager.getIkeLog;

import android.net.Network;
import android.net.ipsec.ike.exceptions.IkeProtocolException;
import android.os.Handler;
import android.system.ErrnoException;
import android.system.Os;
import android.util.LongSparseArray;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.ipsec.ike.message.IkeHeader;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * IkeSocket is used for sending and receiving IKE packets for {@link IkeSessionStateMachine}s.
 *
 * <p>To function as a packet receiver, a subclass MUST override #createFd() and #handlePacket() so
 * that it can register a file descriptor with a thread's Looper and handle read events (and
 * errors). Users can expect a call life-cycle like the following:
 *
 * <pre>
 * [1] when user gets a new initiated IkeSocket, start() is called and followed by createFd().
 * [2] yield, waiting for a read event which will invoke handlePacket()
 * [3] when user closes this IkeSocket, its reference count decreases. Then stop() is called when
 *     there is no reference of this instance.
 * </pre>
 *
 * <p>IkeSocket is constructed and called only on a single IKE working thread by {@link
 * IkeSessionStateMachine}. Since all {@link IkeSessionStateMachine}s run on the same working
 * thread, there will not be concurrent modification problems.
 */
public abstract class IkeSocket implements AutoCloseable {
    private static final String TAG = "IkeSocket";

    /** Non-udp-encapsulated IKE packets MUST be sent to 500. */
    public static final int SERVER_PORT_NON_UDP_ENCAPSULATED = 500;
    /** UDP-encapsulated IKE packets MUST be sent to 4500. */
    public static final int SERVER_PORT_UDP_ENCAPSULATED = 4500;

    // Network this socket bound to.
    private final Network mNetwork;
    private final Handler mHandler;

    // Map from locally generated IKE SPI to IkeSessionStateMachine instances.
    @VisibleForTesting
    protected final LongSparseArray<IkeSessionStateMachine> mSpiToIkeSession =
            new LongSparseArray<>();

    // Set to store all running IkeSessionStateMachines that are using this IkeSocket instance.
    @VisibleForTesting
    protected final Set<IkeSessionStateMachine> mAliveIkeSessions = new HashSet<>();

    protected IkeSocket(Network network, Handler handler) {
        mHandler = handler;
        mNetwork = network;
    }

    protected static void parseAndDemuxIkePacket(
            byte[] ikePacketBytes,
            LongSparseArray<IkeSessionStateMachine> spiToIkeSession,
            String tag) {
        try {
            // TODO: Retrieve and log the source address
            getIkeLog().d(tag, "Receive packet of " + ikePacketBytes.length + " bytes)");
            getIkeLog().d(tag, getIkeLog().pii(ikePacketBytes));

            IkeHeader ikeHeader = new IkeHeader(ikePacketBytes);

            long localGeneratedSpi =
                    ikeHeader.fromIkeInitiator
                            ? ikeHeader.ikeResponderSpi
                            : ikeHeader.ikeInitiatorSpi;

            IkeSessionStateMachine ikeStateMachine = spiToIkeSession.get(localGeneratedSpi);
            if (ikeStateMachine == null) {
                getIkeLog().w(tag, "Unrecognized IKE SPI.");
                // TODO(b/148479270): Handle invalid IKE SPI error
            } else {
                ikeStateMachine.receiveIkePacket(ikeHeader, ikePacketBytes);
            }
        } catch (IkeProtocolException e) {
            // Handle invalid IKE header
            getIkeLog().i(tag, "Can't parse malformed IKE packet header.");
        }
    }

    /** Starts the packet reading poll-loop. */
    public void start() {
        // Start background reader thread
        new Thread(
                () -> {
                    try {
                        // Loop will exit and thread will quit when the retrieved fd is closed.
                        // Receiving either EOF or an exception will exit this reader loop.
                        // FileInputStream in uninterruptable, so there's no good way to ensure that
                        // that this thread shuts down except upon FD closure.
                        while (true) {
                            byte[] intercepted = receiveFromFd();
                            if (intercepted == null) {
                                // Exit once we've hit EOF
                                return;
                            } else if (intercepted.length > 0) {
                                // Only save packet if we've received any bytes.
                                getIkeLog()
                                        .d(
                                                this.getClass().getSimpleName(),
                                                "Received packet");
                                mHandler.post(
                                        () -> {
                                            handlePacket(intercepted, intercepted.length);
                                        });
                            }
                        }
                    } catch (IOException ignored) {
                        // Simply exit this reader thread
                        return;
                    }
                }).start();
    }

    private byte[] receiveFromFd() throws IOException {
        FileInputStream in = new FileInputStream(getFd());
        byte[] inBytes = new byte[4096];
        int bytesRead = in.read(inBytes);

        if (bytesRead < 0) {
            return null; // return null for EOF
        } else if (bytesRead >= 4096) {
            throw new IllegalStateException("Too big packet. Fragmentation unsupported");
        }
        return Arrays.copyOf(inBytes, bytesRead);
    }

    /**
     * Returns the port that this IKE socket is listening on (bound to).
     */
    public final int getLocalPort() throws ErrnoException {
        InetSocketAddress localAddr = (InetSocketAddress) Os.getsockname(getFd());
        return localAddr.getPort();
    }

    protected abstract FileDescriptor getFd();

    protected FileDescriptor createFd() {
        return getFd();
    }

    protected abstract void handlePacket(byte[] recvbuf, int length);

    /**
     * Return Network this socket bound to
     *
     * @return the bound Network
     */
    public final Network getNetwork() {
        return mNetwork;
    }

    /**
     * Register new created IKE SA
     *
     * @param spi the locally generated IKE SPI
     * @param ikeSession the IKE session this IKE SA belongs to
     */
    public final void registerIke(long spi, IkeSessionStateMachine ikeSession) {
        mSpiToIkeSession.put(spi, ikeSession);
    }

    /**
     * Unregister a deleted IKE SA
     *
     * @param spi the locally generated IKE SPI
     */
    public final void unregisterIke(long spi) {
        mSpiToIkeSession.remove(spi);
    }

    /**
     * Release reference of current IkeSocket when the IKE session no longer needs it.
     *
     * @param ikeSession IKE session that is being closed.
     */
    public final void releaseReference(IkeSessionStateMachine ikeSession) {
        mAliveIkeSessions.remove(ikeSession);
        if (mAliveIkeSessions.isEmpty()) close();
    }

    /**
     * Send an encoded IKE packet to destination address
     *
     * @param ikePacket encoded IKE packet
     * @param serverAddress IP address of remote server
     */
    public abstract void sendIkePacket(byte[] ikePacket, InetAddress serverAddress);

    /**
     * Returns port of remote IKE sever (destination port of outbound packet)
     *
     * @return destination port in remote IKE sever.
     */
    public abstract int getIkeServerPort();

    /** Implement {@link AutoCloseable#close()} */
    @Override
    public void close() {
        stop();
    }

    /** Stops the packet reading loop */
    public void stop() {
        // No additional cleanup at this time. Subclasses are in charge of closing their sockets,
        // which will result in the packet reading poll loop exiting.
    }

    /**
     * IPacketReceiver provides a package private interface for handling received packet.
     *
     * <p>IPacketReceiver exists so that the interface is injectable for testing.
     */
    interface IPacketReceiver {
        void handlePacket(byte[] recvbuf, LongSparseArray<IkeSessionStateMachine> spiToIkeSession);
    }
}
