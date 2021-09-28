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

package com.android.tv.tuner.hdhomerun;

import android.support.annotation.NonNull;
import android.util.Log;
import android.util.Pair;

import com.android.tv.tuner.hdhomerun.HdHomeRunDiscover.HdHomeRunDiscoverDevice;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.zip.CRC32;

class HdHomeRunUtils {
    private static final String TAG = "HdHomeRunUtils";
    private static final boolean DEBUG = false;

    static final int HDHOMERUN_DEVICE_TYPE_WILDCARD = 0xFFFFFFFF;
    static final int HDHOMERUN_DEVICE_TYPE_TUNER = 0x00000001;
    static final int HDHOMERUN_DEVICE_ID_WILDCARD = 0xFFFFFFFF;

    static final int HDHOMERUN_DISCOVER_UDP_PORT = 65001;
    static final int HDHOMERUN_CONTROL_TCP_PORT = 65001;

    static final short HDHOMERUN_TYPE_INVALID = -1;
    static final short HDHOMERUN_TYPE_DISCOVER_REQUEST = 0x0002;
    static final short HDHOMERUN_TYPE_DISCOVER_REPLY = 0x0003;
    static final short HDHOMERUN_TYPE_GETSET_REQUEST = 0x0004;
    static final short HDHOMERUN_TYPE_GETSET_REPLY = 0x0005;

    static final byte HDHOMERUN_TAG_DEVICE_TYPE = 0x01;
    static final byte HDHOMERUN_TAG_DEVICE_ID = 0x02;
    static final byte HDHOMERUN_TAG_GETSET_NAME = 0x03;
    static final int HDHOMERUN_TAG_GETSET_VALUE = 0x04;
    static final int HDHOMERUN_TAG_ERROR_MESSAGE = 0x05;
    static final int HDHOMERUN_TAG_TUNER_COUNT = 0x10;
    static final int HDHOMERUN_TAG_BASE_URL = 0x2A;

    static final int HDHOMERUN_CONTROL_CONNECT_TIMEOUT_MS = 2500;
    static final int HDHOMERUN_CONTROL_SEND_TIMEOUT_MS = 2500;
    static final int HDHOMERUN_CONTROL_RECEIVE_TIMEOUT_MS = 2500;

    /**
     * Finds HDHomeRun devices with given IP, type, and ID.
     *
     * @param targetIp {@code 0} to find target devices with broadcasting.
     * @param deviceType The type of target devices.
     * @param deviceId The ID of target devices.
     * @param maxCount Maximum number of devices should be returned.
     */
    @NonNull
    static List<HdHomeRunDiscoverDevice> findHdHomeRunDevices(
            int targetIp, int deviceType, int deviceId, int maxCount) {
        if (isIpMulticast(targetIp)) {
            if (DEBUG) Log.d(TAG, "Target IP cannot be multicast IP.");
            return Collections.emptyList();
        }
        try {
            HdHomeRunDiscover ds = HdHomeRunDiscover.create();
            if (ds == null) {
                if (DEBUG) Log.d(TAG, "Cannot create discover object.");
                return Collections.emptyList();
            }
            List<HdHomeRunDiscoverDevice> result =
                    ds.findDevices(targetIp, deviceType, deviceId, maxCount);
            ds.close();
            return result;
        } catch (Exception e) {
            Log.w(TAG, "Failed to find HdHomeRun Devices", e);
            return Collections.emptyList();
        }
    }

    /** Returns {@code true} if the given IP is a multi-cast IP. */
    static boolean isIpMulticast(long ip) {
        return (ip >= 0xE0000000) && (ip < 0xF0000000);
    }

    /** Translates a {@code byte[]} address to its integer representation. */
    static int addressToInt(byte[] address) {
        return ByteBuffer.wrap(address).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }

    /** Translates an {@code int} address to a corresponding {@link InetAddress}. */
    static InetAddress intToAddress(int address) throws UnknownHostException {
        return InetAddress.getByAddress(
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(address).array());
    }

    /** Gets {@link String} representation of an {@code int} address. */
    static String getIpString(int ip) {
        return String.format(
                "%d.%d.%d.%d", (ip & 0xff), (ip >> 8 & 0xff), (ip >> 16 & 0xff), (ip >> 24 & 0xff));
    }

    /**
     * Opens the packet returned from HDHomeRun devices to acquire the real content and verify it.
     */
    static Pair<Short, byte[]> openFrame(byte[] data, int length) {
        if (length < 4) {
            return null;
        }
        ByteBuffer buffer = ByteBuffer.wrap(data);
        short resultType = buffer.getShort();
        int dataLength = buffer.getShort() & 0xffff;

        if (dataLength + 8 > length) {
            // Not finished yet.
            return null;
        }
        byte[] result = new byte[dataLength];
        buffer.get(result);
        byte[] calculatedCrc = getCrcFromBytes(Arrays.copyOfRange(data, 0, dataLength + 4));
        byte[] packetCrc = new byte[4];
        buffer.get(packetCrc);

        if (!Arrays.equals(calculatedCrc, packetCrc)) {
            return Pair.create(HDHOMERUN_TYPE_INVALID, null);
        }

        return Pair.create(resultType, result);
    }

    /** Seals the contents in a packet to send to HDHomeRun devices. */
    static byte[] sealFrame(byte[] data, short frameType) {
        byte[] result = new byte[data.length + 8];
        ByteBuffer buffer = ByteBuffer.wrap(result);
        buffer.putShort(frameType);
        buffer.putShort((short) data.length);
        buffer.put(data);
        buffer.put(getCrcFromBytes(Arrays.copyOfRange(result, 0, data.length + 4)));
        return result;
    }

    /** Reads a (tag, value) pair from packets returned from HDHomeRun devices. */
    static Pair<Byte, byte[]> readTaggedValue(ByteBuffer buffer) {
        try {
            Byte tag = buffer.get();
            byte[] value = readVarLength(buffer);
            return Pair.create(tag, value);
        } catch (BufferUnderflowException e) {
            return null;
        }
    }

    private static byte[] readVarLength(ByteBuffer buffer) {
        short length;
        Byte lengthByte1 = buffer.get();
        if ((lengthByte1 & 0x80) != 0) {
            length = buffer.get();
            length = (short) ((length << 7) + (lengthByte1 & 0x7F));
        } else {
            length = lengthByte1;
        }
        byte[] result = new byte[length];
        buffer.get(result);
        return result;
    }

    private static byte[] getCrcFromBytes(byte[] data) {
        CRC32 crc32 = new CRC32();
        crc32.update(data);
        long crc = crc32.getValue();
        byte[] result = new byte[4];
        for (int offset = 0; offset < 4; offset++) {
            result[offset] = (byte) (crc & 0xFF);
            crc >>= 8;
        }
        return result;
    }

    private HdHomeRunUtils() {}
}
