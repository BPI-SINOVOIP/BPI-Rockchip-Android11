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

package android.net.ip;

import static android.net.netlink.NetlinkConstants.hexify;
import static android.net.util.SocketUtils.makeNetlinkSocketAddress;
import static android.system.OsConstants.AF_NETLINK;
import static android.system.OsConstants.SOCK_DGRAM;
import static android.system.OsConstants.SOCK_NONBLOCK;

import android.annotation.NonNull;
import android.net.netlink.NetlinkErrorMessage;
import android.net.netlink.NetlinkMessage;
import android.net.netlink.NetlinkSocket;
import android.net.util.PacketReader;
import android.net.util.SharedLog;
import android.net.util.SocketUtils;
import android.os.Handler;
import android.os.SystemClock;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.SocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * A simple base class to listen for netlink broadcasts.
 *
 * Opens a netlink socket of the given family and binds to the specified groups. Polls the socket
 * from the event loop of the passed-in {@link Handler}, and calls the subclass-defined
 * {@link #processNetlinkMessage} method on the handler thread for each netlink message that
 * arrives. Currently ignores all netlink errors.
 */
public class NetlinkMonitor extends PacketReader {
    protected final SharedLog mLog;
    protected final String mTag;
    private final int mFamily;
    private final int mBindGroups;

    private static final boolean DBG = false;

    /**
     * Constructs a new {@code NetlinkMonitor} instance.
     *
     * @param h The Handler on which to poll for messages and on which to call
     *          {@link #processNetlinkMessage}.
     * @param log A SharedLog to log to.
     * @param tag The log tag to use for log messages.
     * @param family the Netlink socket family to, e.g., {@code NETLINK_ROUTE}.
     * @param bindGroups the netlink groups to bind to.
     */
    public NetlinkMonitor(@NonNull Handler h, @NonNull SharedLog log, @NonNull String tag,
            int family, int bindGroups) {
        super(h, NetlinkSocket.DEFAULT_RECV_BUFSIZE);
        mLog = log.forSubComponent(tag);
        mTag = tag;
        mFamily = family;
        mBindGroups = bindGroups;
    }

    @Override
    protected FileDescriptor createFd() {
        FileDescriptor fd = null;

        try {
            fd = Os.socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK, mFamily);
            Os.bind(fd, makeNetlinkSocketAddress(0, mBindGroups));
            NetlinkSocket.connectToKernel(fd);

            if (DBG) {
                final SocketAddress nlAddr = Os.getsockname(fd);
                Log.d(mTag, "bound to sockaddr_nl{" + nlAddr.toString() + "}");
            }
        } catch (ErrnoException | SocketException e) {
            logError("Failed to create rtnetlink socket", e);
            closeSocketQuietly(fd);
            return null;
        }

        return fd;
    }

    @Override
    protected void handlePacket(byte[] recvbuf, int length) {
        final long whenMs = SystemClock.elapsedRealtime();
        final ByteBuffer byteBuffer = ByteBuffer.wrap(recvbuf, 0, length);
        byteBuffer.order(ByteOrder.nativeOrder());

        while (byteBuffer.remaining() > 0) {
            try {
                final int position = byteBuffer.position();
                final NetlinkMessage nlMsg = NetlinkMessage.parse(byteBuffer);
                if (nlMsg == null || nlMsg.getHeader() == null) {
                    byteBuffer.position(position);
                    mLog.e("unparsable netlink msg: " + hexify(byteBuffer));
                    break;
                }

                if (nlMsg instanceof NetlinkErrorMessage) {
                    mLog.e("netlink error: " + nlMsg);
                    continue;
                }

                processNetlinkMessage(nlMsg, whenMs);
            } catch (Exception e) {
                mLog.e("Error handling netlink message", e);
            }
        }
    }

    // TODO: move NetworkStackUtils to frameworks/libs/net for NetworkStackUtils#closeSocketQuietly.
    private void closeSocketQuietly(FileDescriptor fd) {
        try {
            SocketUtils.closeSocket(fd);
        } catch (IOException ignored) {
        }
    }

    /**
     * Processes one netlink message. Must be overridden by subclasses.
     * @param nlMsg the message to process.
     * @param whenMs the timestamp, as measured by {@link SystemClock#elapsedRealtime}, when the
     *               message was received.
     */
    protected void processNetlinkMessage(NetlinkMessage nlMsg, long whenMs) { }
}
