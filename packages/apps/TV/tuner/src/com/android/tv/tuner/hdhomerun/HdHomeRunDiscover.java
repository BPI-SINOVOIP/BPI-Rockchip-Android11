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
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.List;

/** A class to discover HDHomeRun devices on the network with UDP broadcasting. */
class HdHomeRunDiscover {
    private static final String TAG = "HdHomeRunDiscover";
    private static final boolean DEBUG = false;

    private static final int HDHOMERUN_DISCOVER_MAX_SOCK_COUNT = 16;
    private static final int HDHOMERUN_DISCOVER_RETRY_LIMIT = 2;
    private static final int HDHOMERUN_DISCOVER_TIMEOUT_MS = 500;
    private static final int HDHOMERUN_DISCOVER_RECEIVE_WAITE_TIME_MS = 10;

    private List<HdHomeRunDiscoverSocket> mSockets = new ArrayList<>();

    /** Creates a discover object. If cannot add a default socket, return {@code null}. */
    static HdHomeRunDiscover create() {
        HdHomeRunDiscover hdHomeRunDiscover = new HdHomeRunDiscover();
        // Create a routable socket (always first entry).
        if (!hdHomeRunDiscover.addSocket(0, 0)) {
            return null;
        }
        return hdHomeRunDiscover;
    }

    /** Closes and releases all sockets required by this discover object. */
    void close() {
        for (HdHomeRunDiscoverSocket discoverSocket : mSockets) {
            discoverSocket.close();
        }
    }

    /** Finds HDHomeRun devices. */
    @NonNull
    List<HdHomeRunDiscoverDevice> findDevices(
            int targetIp, int deviceType, int deviceId, int maxCount) {
        List<HdHomeRunDiscoverDevice> resultList = new ArrayList<>();
        resetLocalIpSockets();
        for (int retry = 0;
                retry < HDHOMERUN_DISCOVER_RETRY_LIMIT && resultList.isEmpty();
                retry++) {
            int localIpSent = send(targetIp, deviceType, deviceId);
            if (localIpSent == 0) {
                if (DEBUG) {
                    Log.d(TAG, "Cannot send to target ip: " + HdHomeRunUtils.getIpString(targetIp));
                }
                continue;
            }
            long timeout = System.currentTimeMillis() + HDHOMERUN_DISCOVER_TIMEOUT_MS * localIpSent;
            while (System.currentTimeMillis() < timeout) {
                HdHomeRunDiscoverDevice result = new HdHomeRunDiscoverDevice();
                if (!receive(result)) {
                    continue;
                }
                // Filter.
                if (deviceType != HdHomeRunUtils.HDHOMERUN_DEVICE_TYPE_WILDCARD
                        && deviceType != result.mDeviceType) {
                    continue;
                }
                if (deviceId != HdHomeRunUtils.HDHOMERUN_DEVICE_ID_WILDCARD
                        && deviceId != result.mDeviceId) {
                    continue;
                }
                if (isObsoleteDevice(deviceId)) {
                    continue;
                }
                // Ensure not already in list.
                if (resultList.contains(result)) {
                    continue;
                }
                // Add to list.
                resultList.add(result);
                if (resultList.size() >= maxCount) {
                    break;
                }
            }
        }
        return resultList;
    }

    private boolean addSocket(int localIp, int subnetMask) {
        for (int i = 1; i < mSockets.size(); i++) {
            HdHomeRunDiscoverSocket discoverSocket = mSockets.get(i);
            if ((discoverSocket.mLocalIp == localIp)
                    && (discoverSocket.mSubnetMask == subnetMask)) {
                discoverSocket.mDetected = true;
                return true;
            }
        }
        if (mSockets.size() >= HDHOMERUN_DISCOVER_MAX_SOCK_COUNT) {
            return false;
        }
        DatagramSocket socket;
        try {
            socket = new DatagramSocket(0, HdHomeRunUtils.intToAddress(localIp));
            socket.setBroadcast(true);
        } catch (IOException e) {
            if (DEBUG) Log.d(TAG, "Cannot create socket: " + HdHomeRunUtils.getIpString(localIp));
            return false;
        }
        // Write socket entry.
        mSockets.add(new HdHomeRunDiscoverSocket(socket, true, localIp, subnetMask));
        return true;
    }

    private void resetLocalIpSockets() {
        for (int i = 1; i < mSockets.size(); i++) {
            mSockets.get(i).mDetected = false;
            mSockets.get(i).mDiscoverPacketSent = false;
        }
        List<LocalIpInfo> ipInfoList = getLocalIpInfo(HDHOMERUN_DISCOVER_MAX_SOCK_COUNT);
        for (LocalIpInfo ipInfo : ipInfoList) {
            if (DEBUG) {
                Log.d(
                        TAG,
                        "Add local IP: "
                                + HdHomeRunUtils.getIpString(ipInfo.mIpAddress)
                                + ", "
                                + HdHomeRunUtils.getIpString(ipInfo.mSubnetMask));
            }
            addSocket(ipInfo.mIpAddress, ipInfo.mSubnetMask);
        }
        Iterator<HdHomeRunDiscoverSocket> iterator = mSockets.iterator();
        while (iterator.hasNext()) {
            HdHomeRunDiscoverSocket discoverSocket = iterator.next();
            if (!discoverSocket.mDetected) {
                discoverSocket.close();
                iterator.remove();
            }
        }
    }

    private List<LocalIpInfo> getLocalIpInfo(int maxCount) {
        Enumeration<NetworkInterface> interfaces;
        try {
            interfaces = NetworkInterface.getNetworkInterfaces();
        } catch (SocketException e) {
            return Collections.emptyList();
        }
        List<LocalIpInfo> result = new ArrayList<>();
        while (interfaces.hasMoreElements()) {
            NetworkInterface networkInterface = interfaces.nextElement();
            for (InterfaceAddress interfaceAddress : networkInterface.getInterfaceAddresses()) {
                InetAddress inetAddress = interfaceAddress.getAddress();
                if (!inetAddress.isAnyLocalAddress()
                        && !inetAddress.isLinkLocalAddress()
                        && !inetAddress.isLoopbackAddress()
                        && !inetAddress.isMulticastAddress()) {
                    LocalIpInfo localIpInfo = new LocalIpInfo();
                    localIpInfo.mIpAddress = HdHomeRunUtils.addressToInt(inetAddress.getAddress());
                    localIpInfo.mSubnetMask =
                            (0x7fffffff >> (31 - interfaceAddress.getNetworkPrefixLength()));
                    result.add(localIpInfo);
                    if (result.size() >= maxCount) {
                        return result;
                    }
                }
            }
        }
        return result;
    }

    private int send(int targetIp, int deviceType, int deviceId) {
        return targetIp == 0
                ? sendWildcardIp(deviceType, deviceId)
                : sendTargetIp(targetIp, deviceType, deviceId);
    }

    private int sendWildcardIp(int deviceType, int deviceId) {
        int localIpSent = 0;

        // Send subnet broadcast using each local ip socket.
        // This will work with multiple separate 169.254.x.x interfaces.
        for (int i = 1; i < mSockets.size(); i++) {
            HdHomeRunDiscoverSocket discoverSocket = mSockets.get(i);
            int targetIp = discoverSocket.mLocalIp | ~discoverSocket.mSubnetMask;
            if (DEBUG) Log.d(TAG, "Send: " + HdHomeRunUtils.getIpString(targetIp));
            localIpSent += discoverSocket.send(targetIp, deviceType, deviceId) ? 1 : 0;
        }
        // If no local ip sockets then fall back to sending a global broadcast letting
        // the OS choose the interface.
        if (localIpSent == 0) {
            if (DEBUG) Log.d(TAG, "Send: " + HdHomeRunUtils.getIpString(0xFFFFFFFF));
            localIpSent = mSockets.get(0).send(0xFFFFFFFF, deviceType, deviceId) ? 1 : 0;
        }
        return localIpSent;
    }

    private int sendTargetIp(int targetIp, int deviceType, int deviceId) {
        int localIpSent = 0;

        // Send targeted packet from any local ip that is in the same subnet.
        // This will work with multiple separate 169.254.x.x interfaces.
        for (int i = 1; i < mSockets.size(); i++) {
            HdHomeRunDiscoverSocket discoverSocket = mSockets.get(i);
            if (discoverSocket.mSubnetMask == 0) {
                continue;
            }
            if ((targetIp & discoverSocket.mSubnetMask)
                    != (discoverSocket.mLocalIp & discoverSocket.mSubnetMask)) {
                continue;
            }
            localIpSent += discoverSocket.send(targetIp, deviceType, deviceId) ? 1 : 0;
        }
        // If target IP does not match a local subnet then fall back to letting the OS choose
        // the gateway interface.
        if (localIpSent == 0) {
            localIpSent = mSockets.get(0).send(targetIp, deviceType, deviceId) ? 1 : 0;
        }
        return localIpSent;
    }

    private boolean receive(HdHomeRunDiscoverDevice result) {
        for (HdHomeRunDiscoverSocket discoverSocket : mSockets) {
            if (discoverSocket.mDiscoverPacketSent && discoverSocket.receive(result)) {
                return true;
            }
        }
        return false;
    }

    private boolean isObsoleteDevice(int deviceId) {
        switch (deviceId >> 20) {
            case 0x100: /* TECH-US/TECH3-US */
                return (deviceId < 0x10040000);
            case 0x120: /* TECH3-EU */
                return (deviceId < 0x12030000);
            case 0x101: /* HDHR-US */
            case 0x102: /* HDHR-T1-US */
            case 0x103: /* HDHR3-US */
            case 0x111: /* HDHR3-DT */
            case 0x121: /* HDHR-EU */
            case 0x122: /* HDHR3-EU */
                return true;
            default:
                return false;
        }
    }

    static class HdHomeRunDiscoverDevice {
        int mIpAddress;
        int mDeviceType;
        int mDeviceId;
        int mTunerCount;
        String mBaseUrl;

        @Override
        public boolean equals(Object other) {
            if (this == other) {
                return true;
            } else if (other instanceof HdHomeRunDiscoverDevice) {
                HdHomeRunDiscoverDevice o = (HdHomeRunDiscoverDevice) other;
                return mIpAddress == o.mIpAddress
                        && mDeviceType == o.mDeviceType
                        && mDeviceId == o.mDeviceId;
            }
            return false;
        }

        @Override
        public int hashCode() {
            int result = mIpAddress;
            result = 31 * result + mDeviceType;
            result = 31 * result + mDeviceId;
            return result;
        }
    }

    private static class HdHomeRunDiscoverSocket {
        DatagramSocket mSocket;
        boolean mDetected;
        boolean mDiscoverPacketSent;
        int mLocalIp;
        int mSubnetMask;

        private HdHomeRunDiscoverSocket(
                DatagramSocket socket, boolean detected, int localIp, int subnetMask) {
            mSocket = socket;
            mDetected = detected;
            mLocalIp = localIp;
            mSubnetMask = subnetMask;
        }

        private boolean send(int targetIp, int deviceType, int deviceId) {
            byte[] data = new byte[12];
            ByteBuffer buffer = ByteBuffer.wrap(data);
            buffer.put(HdHomeRunUtils.HDHOMERUN_TAG_DEVICE_TYPE);
            buffer.put((byte) 4);
            buffer.putInt(deviceType);
            buffer.put(HdHomeRunUtils.HDHOMERUN_TAG_DEVICE_ID);
            buffer.put((byte) 4);
            buffer.putInt(deviceId);
            data = HdHomeRunUtils.sealFrame(data, HdHomeRunUtils.HDHOMERUN_TYPE_DISCOVER_REQUEST);
            try {
                DatagramPacket packet =
                        new DatagramPacket(
                                data,
                                data.length,
                                HdHomeRunUtils.intToAddress(targetIp),
                                HdHomeRunUtils.HDHOMERUN_DISCOVER_UDP_PORT);
                mSocket.send(packet);
                if (DEBUG) {
                    Log.d(TAG, "Discover packet sent to: " + HdHomeRunUtils.getIpString(targetIp));
                }
                mDiscoverPacketSent = true;
            } catch (IOException e) {
                if (DEBUG) {
                    Log.d(
                            TAG,
                            "Cannot send discover packet to socket("
                                    + HdHomeRunUtils.getIpString(mLocalIp)
                                    + ")");
                }
                mDiscoverPacketSent = false;
            }
            return mDiscoverPacketSent;
        }

        private boolean receive(HdHomeRunDiscoverDevice result) {
            DatagramPacket packet = new DatagramPacket(new byte[3074], 3074);
            try {
                mSocket.setSoTimeout(HDHOMERUN_DISCOVER_RECEIVE_WAITE_TIME_MS);
                mSocket.receive(packet);
                if (DEBUG) Log.d(TAG, "Received packet, size: " + packet.getLength());
            } catch (IOException e) {
                if (DEBUG) {
                    Log.d(
                            TAG,
                            "Cannot receive from socket("
                                    + HdHomeRunUtils.getIpString(mLocalIp)
                                    + ")");
                }
                return false;
            }

            Pair<Short, byte[]> data =
                    HdHomeRunUtils.openFrame(packet.getData(), packet.getLength());
            if (data == null
                    || data.first == null
                    || data.first != HdHomeRunUtils.HDHOMERUN_TYPE_DISCOVER_REPLY) {
                if (DEBUG) Log.d(TAG, "Ill-formed packet: " + Arrays.toString(packet.getData()));
                return false;
            }
            result.mIpAddress = HdHomeRunUtils.addressToInt(packet.getAddress().getAddress());
            if (DEBUG) {
                Log.d(TAG, "Get Device IP: " + HdHomeRunUtils.getIpString(result.mIpAddress));
            }
            ByteBuffer buffer = ByteBuffer.wrap(data.second);
            while (true) {
                Pair<Byte, byte[]> tagAndValue = HdHomeRunUtils.readTaggedValue(buffer);
                if (tagAndValue == null) {
                    break;
                }
                switch (tagAndValue.first) {
                    case HdHomeRunUtils.HDHOMERUN_TAG_DEVICE_TYPE:
                        if (tagAndValue.second.length != 4) {
                            break;
                        }
                        result.mDeviceType = ByteBuffer.wrap(tagAndValue.second).getInt();
                        if (DEBUG) Log.d(TAG, "Get Device Type: " + result.mDeviceType);
                        break;
                    case HdHomeRunUtils.HDHOMERUN_TAG_DEVICE_ID:
                        if (tagAndValue.second.length != 4) {
                            break;
                        }
                        result.mDeviceId = ByteBuffer.wrap(tagAndValue.second).getInt();
                        if (DEBUG) Log.d(TAG, "Get Device ID: " + result.mDeviceId);
                        break;
                    case HdHomeRunUtils.HDHOMERUN_TAG_TUNER_COUNT:
                        if (tagAndValue.second.length != 1) {
                            break;
                        }
                        result.mTunerCount = tagAndValue.second[0];
                        if (DEBUG) Log.d(TAG, "Get Tuner Count: " + result.mTunerCount);
                        break;
                    case HdHomeRunUtils.HDHOMERUN_TAG_BASE_URL:
                        result.mBaseUrl = new String(tagAndValue.second);
                        if (DEBUG) Log.d(TAG, "Get Base URL: " + result.mBaseUrl);
                        break;
                    default:
                        break;
                }
            }
            // Fixup for old firmware.
            if (result.mTunerCount == 0) {
                switch (result.mDeviceId >> 20) {
                    case 0x102:
                        result.mTunerCount = 1;
                        break;
                    case 0x100:
                    case 0x101:
                    case 0x121:
                        result.mTunerCount = 2;
                        break;
                    default:
                        break;
                }
            }
            return true;
        }

        private void close() {
            if (mSocket != null) {
                mSocket.close();
            }
        }
    }

    private static class LocalIpInfo {
        int mIpAddress;
        int mSubnetMask;
    }
}
