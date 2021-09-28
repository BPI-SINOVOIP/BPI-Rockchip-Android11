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

import android.support.annotation.Nullable;
import android.util.Log;
import android.util.Pair;
import com.android.tv.tuner.hdhomerun.HdHomeRunDiscover.HdHomeRunDiscoverDevice;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

/**
 * A class to send/receive control commands and results to/from HDHomeRun devices via TCP sockets.
 * {@link #close()} method should be called after usage to close the TCP socket.
 */
class HdHomeRunControlSocket implements AutoCloseable {
    private static final String TAG = "HdHomeRunControlSocket";
    private static final boolean DEBUG = false;

    private int mDesiredDeviceId;
    private int mDesiredDeviceIp;
    private int mActualDeviceId;
    private int mActualDeviceIp;
    private Socket mSocket;

    HdHomeRunControlSocket(int deviceId, int deviceIp) {
        mDesiredDeviceId = deviceId;
        mDesiredDeviceIp = deviceIp;
        mActualDeviceId = 0;
        mActualDeviceIp = 0;
    }

    /**
     * Gets control settings from HDHomeRun devices.
     *
     * @param name the name of the field whose value we want to get.
     */
    @Nullable
    String get(String name) {
        byte[] data = new byte[name.length() + 3];
        ByteBuffer buffer = ByteBuffer.wrap(data);
        buffer.put(HdHomeRunUtils.HDHOMERUN_TAG_GETSET_NAME);
        buffer.put((byte) (name.length() + 1));
        buffer.put(name.getBytes());

        // Send & Receive.
        byte[] result =
                sendAndReceive(
                        data,
                        HdHomeRunUtils.HDHOMERUN_TYPE_GETSET_REQUEST,
                        HdHomeRunUtils.HDHOMERUN_CONTROL_RECEIVE_TIMEOUT_MS);
        if (result == null) {
            if (DEBUG) Log.d(TAG, "Cannot get result for " + name);
            return null;
        }

        // Response.
        buffer = ByteBuffer.wrap(result);
        while (true) {
            Pair<Byte, byte[]> tagAndValue = HdHomeRunUtils.readTaggedValue(buffer);
            if (tagAndValue == null) {
                break;
            }
            switch (tagAndValue.first) {
                case HdHomeRunUtils.HDHOMERUN_TAG_GETSET_VALUE:
                    // Removes the 0 tail.
                    return new String(
                            Arrays.copyOfRange(
                                    tagAndValue.second, 0, tagAndValue.second.length - 1));
                case HdHomeRunUtils.HDHOMERUN_TAG_ERROR_MESSAGE:
                    return null;
            }
        }
        return null;
    }

    /** Gets ID of HDHomeRun devices. */
    int getDeviceId() {
        if (!connectAndUpdateDeviceInfo()) {
            return 0;
        }
        return mActualDeviceId;
    }

    private boolean connectAndUpdateDeviceInfo() {
        if (mSocket != null) {
            return true;
        }
        if ((mDesiredDeviceId == 0) && (mDesiredDeviceIp == 0)) {
            if (DEBUG) Log.d(TAG, "Desired ID and IP cannot be both zero.");
            return false;
        }
        if (HdHomeRunUtils.isIpMulticast(mDesiredDeviceIp)) {
            if (DEBUG) Log.d(TAG, "IP cannot be multicast IP.");
            return false;
        }

        // Find device.
        List<HdHomeRunDiscoverDevice> result =
                HdHomeRunUtils.findHdHomeRunDevices(
                        mDesiredDeviceIp,
                        HdHomeRunUtils.HDHOMERUN_DEVICE_TYPE_WILDCARD,
                        mDesiredDeviceId,
                        1);
        if (result.isEmpty()) {
            if (DEBUG) Log.d(TAG, "Cannot find device on: " + mDesiredDeviceIp);
            return false;
        }
        mActualDeviceIp = result.get(0).mIpAddress;
        mActualDeviceId = result.get(0).mDeviceId;

        // Create socket and initiate connection.
        mSocket = new Socket();
        try {
            mSocket.connect(
                    new InetSocketAddress(
                            HdHomeRunUtils.intToAddress(mActualDeviceIp),
                            HdHomeRunUtils.HDHOMERUN_CONTROL_TCP_PORT),
                    HdHomeRunUtils.HDHOMERUN_CONTROL_CONNECT_TIMEOUT_MS);
        } catch (IOException e) {
            if (DEBUG) Log.d(TAG, "Cannot connect to socket: " + mSocket);
            mSocket = null;
            return false;
        }

        // Success.
        Log.i(TAG, "Connected to socket: " + mSocket);
        return true;
    }

    private byte[] sendAndReceive(byte[] data, short type, int timeout) {
        byte[] sealedData = HdHomeRunUtils.sealFrame(data, type);
        for (int i = 0; i < 2; i++) {
            if (mSocket == null && !connectAndUpdateDeviceInfo()) {
                return null;
            }
            if (!send(sealedData)) {
                continue;
            }
            Pair<Short, byte[]> receivedData = receive(timeout);
            if (receivedData == null || receivedData.first == null) {
                continue;
            }
            if (receivedData.first != type + 1) {
                if (DEBUG) Log.d(TAG, "Returned type incorrect: " + receivedData.first);
                close();
                continue;
            }
            return receivedData.second;
        }
        return null;
    }

    private boolean send(byte[] data) {
        try {
            OutputStream out = mSocket.getOutputStream();
            mSocket.setSoTimeout(HdHomeRunUtils.HDHOMERUN_CONTROL_SEND_TIMEOUT_MS);
            out.write(data);
        } catch (IOException e) {
            if (DEBUG) Log.d(TAG, "Cannot send packet to socket: " + mSocket);
            close();
            return false;
        }
        return true;
    }

    private Pair<Short, byte[]> receive(int timeout) {
        byte[] receivedData = new byte[3074];
        try {
            InputStream input = mSocket.getInputStream();
            mSocket.setSoTimeout(timeout);
            int index = 0;
            long startTime = System.currentTimeMillis();
            while (System.currentTimeMillis() - startTime < timeout) {
                int length = receivedData.length - index;
                index += input.read(receivedData, index, length);
                Pair<Short, byte[]> result = HdHomeRunUtils.openFrame(receivedData, index);
                if (result != null) {
                    if (result.first == HdHomeRunUtils.HDHOMERUN_TYPE_INVALID) {
                        if (DEBUG) Log.d(TAG, "Returned type is invalid.");
                        close();
                        return null;
                    }
                    return result;
                }
                if (DEBUG) Log.d(TAG, "Received result is null!");
            }
        } catch (IOException e) {
            if (DEBUG) Log.d(TAG, "Cannot receive from socket: " + mSocket);
            close();
        }
        return null;
    }

    @Override
    public void close() {
        if (mSocket != null) {
            try {
                mSocket.close();
            } catch (IOException e) {
                // Do nothing
            }
            mSocket = null;
        }
    }
}
