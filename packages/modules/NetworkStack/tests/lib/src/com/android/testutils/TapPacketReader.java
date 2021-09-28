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

package com.android.testutils;

import android.net.util.PacketReader;
import android.os.Handler;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.function.Predicate;

import kotlin.Lazy;
import kotlin.LazyKt;

public class TapPacketReader extends PacketReader {
    private final FileDescriptor mTapFd;
    private final ArrayTrackRecord<byte[]> mReceivedPackets = new ArrayTrackRecord<>();
    private final Lazy<ArrayTrackRecord<byte[]>.ReadHead> mReadHead =
            LazyKt.lazy(mReceivedPackets::newReadHead);

    public TapPacketReader(Handler h, FileDescriptor tapFd, int maxPacketSize) {
        super(h, maxPacketSize);
        mTapFd = tapFd;
    }

    @Override
    protected FileDescriptor createFd() {
        return mTapFd;
    }

    @Override
    protected void handlePacket(byte[] recvbuf, int length) {
        final byte[] newPacket = Arrays.copyOf(recvbuf, length);
        if (!mReceivedPackets.add(newPacket)) {
            throw new AssertionError("More than " + Integer.MAX_VALUE + " packets outstanding!");
        }
    }

    /**
     * Get the next packet that was received on the interface.
     */
    @Nullable
    public byte[] popPacket(long timeoutMs) {
        return mReadHead.getValue().poll(timeoutMs, packet -> true);
    }

    /**
     * Get the next packet that was received on the interface and matches the specified filter.
     */
    @Nullable
    public byte[] popPacket(long timeoutMs, @NonNull Predicate<byte[]> filter) {
        return mReadHead.getValue().poll(timeoutMs, filter::test);
    }

    public void sendResponse(final ByteBuffer packet) throws IOException {
        try (FileOutputStream out = new FileOutputStream(mTapFd)) {
            byte[] packetBytes = new byte[packet.limit()];
            packet.get(packetBytes);
            packet.flip();  // So we can reuse it in the future.
            out.write(packetBytes);
        }
    }
}
