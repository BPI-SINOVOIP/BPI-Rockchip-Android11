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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.net.Network;
import android.os.Handler;
import android.os.test.TestLooper;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.message.IkeHeader;
import com.android.internal.util.HexDump;

import org.junit.Test;
import org.mockito.ArgumentCaptor;

import java.io.FileDescriptor;

// TODO: Combine this and IkeUdp4SocketTest, and take a Factory method as an input.
public final class IkeUdp6SocketTest extends IkeSocketTestBase {
    private final TestLooper mLooper = new TestLooper();
    private final Handler mHandler = new Handler(mLooper.getLooper());

    @Override
    protected IkeSocket.IPacketReceiver getPacketReceiver() {
        return new IkeUdpSocket.PacketReceiver();
    }

    @Test
    public void testGetAndCloseIkeUdp6SocketSameNetwork() throws Exception {
        IkeSessionStateMachine mockIkeSessionOne = mock(IkeSessionStateMachine.class);
        IkeSessionStateMachine mockIkeSessionTwo = mock(IkeSessionStateMachine.class);

        IkeUdp6Socket ikeSocketOne =
                IkeUdp6Socket.getInstance(mMockNetwork, mockIkeSessionOne, mHandler);
        assertEquals(1, ikeSocketOne.mAliveIkeSessions.size());

        IkeUdp6Socket ikeSocketTwo =
                IkeUdp6Socket.getInstance(mMockNetwork, mockIkeSessionTwo, mHandler);
        assertEquals(2, ikeSocketTwo.mAliveIkeSessions.size());
        assertEquals(ikeSocketOne, ikeSocketTwo);

        verify(mMockNetwork).bindSocket(eq(ikeSocketOne.getFd()));

        ikeSocketOne.releaseReference(mockIkeSessionOne);
        assertEquals(1, ikeSocketOne.mAliveIkeSessions.size());
        assertTrue(isFdOpen(ikeSocketOne.getFd()));

        ikeSocketTwo.releaseReference(mockIkeSessionTwo);
        assertEquals(0, ikeSocketTwo.mAliveIkeSessions.size());
        verifyCloseFd(ikeSocketTwo.getFd());
    }

    @Test
    public void testGetAndCloseIkeUdp6SocketDifferentNetwork() throws Exception {
        IkeSessionStateMachine mockIkeSessionOne = mock(IkeSessionStateMachine.class);
        IkeSessionStateMachine mockIkeSessionTwo = mock(IkeSessionStateMachine.class);

        Network mockNetworkOne = mock(Network.class);
        Network mockNetworkTwo = mock(Network.class);

        IkeUdp6Socket ikeSocketOne =
                IkeUdp6Socket.getInstance(mockNetworkOne, mockIkeSessionOne, mHandler);
        assertEquals(1, ikeSocketOne.mAliveIkeSessions.size());

        IkeUdp6Socket ikeSocketTwo =
                IkeUdp6Socket.getInstance(mockNetworkTwo, mockIkeSessionTwo, mHandler);
        assertEquals(1, ikeSocketTwo.mAliveIkeSessions.size());

        assertNotEquals(ikeSocketOne, ikeSocketTwo);

        ArgumentCaptor<FileDescriptor> fdCaptorOne = ArgumentCaptor.forClass(FileDescriptor.class);
        ArgumentCaptor<FileDescriptor> fdCaptorTwo = ArgumentCaptor.forClass(FileDescriptor.class);
        verify(mockNetworkOne).bindSocket(fdCaptorOne.capture());
        verify(mockNetworkTwo).bindSocket(fdCaptorTwo.capture());

        FileDescriptor fdOne = fdCaptorOne.getValue();
        FileDescriptor fdTwo = fdCaptorTwo.getValue();
        assertNotNull(fdOne);
        assertNotNull(fdTwo);
        assertNotEquals(fdOne, fdTwo);

        ikeSocketOne.releaseReference(mockIkeSessionOne);
        assertEquals(0, ikeSocketOne.mAliveIkeSessions.size());
        verifyCloseFd(ikeSocketOne.getFd());

        ikeSocketTwo.releaseReference(mockIkeSessionTwo);
        assertEquals(0, ikeSocketTwo.mAliveIkeSessions.size());
        verifyCloseFd(ikeSocketTwo.getFd());
    }

    @Test
    public void testReceiveIkePacket() throws Exception {
        IkeSessionStateMachine mockIkeSession = mock(IkeSessionStateMachine.class);
        IkeUdp6Socket ikeSocket = IkeUdp6Socket.getInstance(mMockNetwork, mockIkeSession, mHandler);
        assertNotNull(ikeSocket);

        // Set up state
        ikeSocket.registerIke(LOCAL_SPI, mockIkeSession);
        IkeSocket.IPacketReceiver packetReceiver = mock(IkeSocket.IPacketReceiver.class);
        IkeUdpSocket.setPacketReceiver(packetReceiver);
        try {
            // Send a packet
            byte[] pktBytes = HexDump.hexStringToByteArray(IKE_REQ_MESSAGE_HEX_STRING);
            ikeSocket.handlePacket(pktBytes, pktBytes.length);

            verify(packetReceiver).handlePacket(eq(pktBytes), any());

        } finally {
            ikeSocket.releaseReference(mockIkeSession);
            IkeUdpSocket.setPacketReceiver(getPacketReceiver());
        }
    }

    @Test
    public void testHandlePacket() throws Exception {
        byte[] recvBuf = TestUtils.hexStringToByteArray(IKE_REQ_MESSAGE_HEX_STRING);

        getPacketReceiver().handlePacket(recvBuf, mSpiToIkeStateMachineMap);

        byte[] expectedIkePacketBytes = TestUtils.hexStringToByteArray(IKE_REQ_MESSAGE_HEX_STRING);
        ArgumentCaptor<IkeHeader> ikeHeaderCaptor = ArgumentCaptor.forClass(IkeHeader.class);
        verify(mMockIkeSessionStateMachine)
                .receiveIkePacket(ikeHeaderCaptor.capture(), eq(expectedIkePacketBytes));

        IkeHeader capturedIkeHeader = ikeHeaderCaptor.getValue();
        assertEquals(REMOTE_SPI, capturedIkeHeader.ikeInitiatorSpi);
        assertEquals(LOCAL_SPI, capturedIkeHeader.ikeResponderSpi);
    }
}
