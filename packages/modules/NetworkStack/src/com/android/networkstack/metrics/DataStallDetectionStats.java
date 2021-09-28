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

package com.android.networkstack.metrics;

import android.net.util.NetworkStackUtils;
import android.net.wifi.WifiInfo;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.internal.util.HexDump;
import com.android.server.connectivity.nano.CellularData;
import com.android.server.connectivity.nano.DataStallEventProto;
import com.android.server.connectivity.nano.DnsEvent;
import com.android.server.connectivity.nano.WifiData;

import com.google.protobuf.nano.MessageNano;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/**
 * Class to record the stats of detection level information for data stall.
 *
 * @hide
 */
public final class DataStallDetectionStats {
    public static final int UNKNOWN_SIGNAL_STRENGTH = -1;
    // Default value of TCP signals.
    @VisibleForTesting
    public static final int UNSPECIFIED_TCP_FAIL_RATE = -1;
    @VisibleForTesting
    public static final int UNSPECIFIED_TCP_PACKETS_COUNT = -1;
    @VisibleForTesting
    @NonNull
    public final byte[] mCellularInfo;
    @VisibleForTesting
    @NonNull
    public final byte[] mWifiInfo;
    @NonNull
    public final byte[] mDns;
    @VisibleForTesting
    public final int mEvaluationType;
    @VisibleForTesting
    public final int mNetworkType;
    // The TCP packets fail rate percentage from the latest tcp polling. -1 means the TCP signal is
    // not known or not supported on the SDK version of this device.
    @VisibleForTesting @IntRange(from = -1, to = 100)
    public final int mTcpFailRate;
    // Number of packets sent since the last received packet.
    @VisibleForTesting
    public final int mTcpSentSinceLastRecv;

    public DataStallDetectionStats(@Nullable byte[] cell, @Nullable byte[] wifi,
                @NonNull int[] returnCode, @NonNull long[] dnsTime, int evalType, int netType,
                int failRate, int sentSinceLastRecv) {
        mCellularInfo = emptyCellDataIfNull(cell);
        mWifiInfo = emptyWifiInfoIfNull(wifi);

        DnsEvent dns = new DnsEvent();
        dns.dnsReturnCode = returnCode;
        dns.dnsTime = dnsTime;
        mDns = MessageNano.toByteArray(dns);
        mEvaluationType = evalType;
        mNetworkType = netType;
        mTcpFailRate = failRate;
        mTcpSentSinceLastRecv = sentSinceLastRecv;
    }

    /**
     * Because metrics data must contain data for each field even if it's not supported or not
     * available, generate a byte array representing an empty {@link CellularData} if the
     * {@link CellularData} is unavailable.
     *
     * @param cell a byte array representing current {@link CellularData} of {@code this}
     * @return a byte array of a {@link CellularData}.
     */
    @VisibleForTesting
    public static byte[] emptyCellDataIfNull(@Nullable byte[] cell) {
        if (cell != null) return cell;

        CellularData data  = new CellularData();
        data.ratType = DataStallEventProto.RADIO_TECHNOLOGY_UNKNOWN;
        data.networkMccmnc = "";
        data.simMccmnc = "";
        data.signalStrength = UNKNOWN_SIGNAL_STRENGTH;
        return MessageNano.toByteArray(data);
    }

    /**
     * Because metrics data must contain data for each field even if it's not supported or not
     * available, generate a byte array representing an empty {@link WifiData} if the
     * {@link WiFiData} is unavailable.
     *
     * @param wifi a byte array representing current {@link WiFiData} of {@code this}.
     * @return a byte array of a {@link WiFiData}.
     */
    @VisibleForTesting
    public static byte[] emptyWifiInfoIfNull(@Nullable byte[] wifi) {
        if (wifi != null) return wifi;

        WifiData data = new WifiData();
        data.wifiBand = DataStallEventProto.AP_BAND_UNKNOWN;
        data.signalStrength = UNKNOWN_SIGNAL_STRENGTH;
        return MessageNano.toByteArray(data);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("type: ").append(mNetworkType)
          .append(", evaluation type: ")
          .append(mEvaluationType)
          .append(", wifi info: ")
          .append(HexDump.toHexString(mWifiInfo))
          .append(", cell info: ")
          .append(HexDump.toHexString(mCellularInfo))
          .append(", dns: ")
          .append(HexDump.toHexString(mDns))
          .append(", tcp fail rate: ")
          .append(mTcpFailRate)
          .append(", tcp received: ")
          .append(mTcpSentSinceLastRecv);
        return sb.toString();
    }

    @Override
    public boolean equals(@Nullable final Object o) {
        if (!(o instanceof DataStallDetectionStats)) return false;
        final DataStallDetectionStats other = (DataStallDetectionStats) o;
        return (mNetworkType == other.mNetworkType)
            && (mEvaluationType == other.mEvaluationType)
            && Arrays.equals(mWifiInfo, other.mWifiInfo)
            && Arrays.equals(mCellularInfo, other.mCellularInfo)
            && Arrays.equals(mDns, other.mDns)
            && (mTcpFailRate == other.mTcpFailRate)
            && (mTcpSentSinceLastRecv == other.mTcpSentSinceLastRecv);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mNetworkType, mEvaluationType, mWifiInfo, mCellularInfo, mDns,
                mTcpFailRate, mTcpSentSinceLastRecv);
    }

    /**
     * Utility to create an instance of {@Link DataStallDetectionStats}
     *
     * @hide
     */
    public static class Builder {
        @Nullable
        private byte[] mCellularInfo;
        @Nullable
        private byte[] mWifiInfo;
        @NonNull
        private final List<Integer> mDnsReturnCode = new ArrayList<Integer>();
        @NonNull
        private final List<Long> mDnsTimeStamp = new ArrayList<Long>();
        private int mEvaluationType;
        private int mNetworkType;
        private int mTcpFailRate = UNSPECIFIED_TCP_FAIL_RATE;
        private int mTcpSentSinceLastRecv = UNSPECIFIED_TCP_PACKETS_COUNT;

        /**
         * Add a dns event into Builder.
         *
         * @param code the return code of the dns event.
         * @param timeMs the elapsedRealtime in ms that the the dns event was received from netd.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder addDnsEvent(int code, long timeMs) {
            mDnsReturnCode.add(code);
            mDnsTimeStamp.add(timeMs);
            return this;
        }

        /**
         * Set the dns evaluation type into Builder.
         *
         * @param type the return code of the dns event.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder setEvaluationType(int type) {
            mEvaluationType = type;
            return this;
        }

        /**
         * Set the network type into Builder.
         *
         * @param type the network type of the logged network.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder setNetworkType(int type) {
            mNetworkType = type;
            return this;
        }

        /**
         * Set the TCP packet fail rate into Builder. The data is included since android R.
         *
         * @param rate the TCP packet fail rate of the logged network. The default value is
         *               {@code UNSPECIFIED_TCP_FAIL_RATE}, which means the TCP signal is not known
         *               or not supported on the SDK version of this device.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder setTcpFailRate(@IntRange(from = -1, to = 100) int rate) {
            mTcpFailRate = rate;
            return this;
        }

        /**
         * Set the number of TCP packets sent since the last received packet into Builder. The data
         * starts to be included since android R.
         *
         * @param count the number of packets sent since the last received packet of the logged
         *              network. Keep it unset as default value or set to
         *              {@code UNSPECIFIED_TCP_PACKETS_COUNT} if the tcp signal is unsupported with
         *              current device android sdk version or the packets count is unknown.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder setTcpSentSinceLastRecv(int count) {
            mTcpSentSinceLastRecv = count;
            return this;
        }

        /**
         * Set the wifi data into Builder.
         *
         * @param info a {@link WifiInfo} of the connected wifi network.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder setWiFiData(@Nullable final WifiInfo info) {
            WifiData data = new WifiData();
            data.wifiBand = getWifiBand(info);
            data.signalStrength = (info != null) ? info.getRssi() : UNKNOWN_SIGNAL_STRENGTH;
            mWifiInfo = MessageNano.toByteArray(data);
            return this;
        }

        private static int getWifiBand(@Nullable final WifiInfo info) {
            if (info == null) return DataStallEventProto.AP_BAND_UNKNOWN;

            int freq = info.getFrequency();
            // Refer to ScanResult.is5GHz(), ScanResult.is24GHz() and ScanResult.is6GHz().
            if (freq >= 5160 && freq <= 5865) {
                return DataStallEventProto.AP_BAND_5GHZ;
            } else if (freq >= 2412 && freq <= 2484) {
                return DataStallEventProto.AP_BAND_2GHZ;
            } else if (freq >= 5945 && freq <= 7105) {
                return DataStallEventProto.AP_BAND_6GHZ;
            } else {
                return DataStallEventProto.AP_BAND_UNKNOWN;
            }
        }

        /**
         * Set the cellular data into Builder.
         *
         * @param radioType the radio technology of the logged cellular network.
         * @param roaming a boolean indicates if logged cellular network is roaming or not.
         * @param networkMccmnc the mccmnc of the camped network.
         * @param simMccmnc the mccmnc of the sim.
         * @return {@code this} {@link Builder} instance.
         */
        public Builder setCellData(int radioType, boolean roaming,
                @NonNull String networkMccmnc, @NonNull String simMccmnc, int ss) {
            CellularData data  = new CellularData();
            data.ratType = radioType;
            data.isRoaming = roaming;
            data.networkMccmnc = networkMccmnc;
            data.simMccmnc = simMccmnc;
            data.signalStrength = ss;
            mCellularInfo = MessageNano.toByteArray(data);
            return this;
        }

        /**
         * Create a new {@Link DataStallDetectionStats}.
         */
        public DataStallDetectionStats build() {
            return new DataStallDetectionStats(mCellularInfo, mWifiInfo,
                    NetworkStackUtils.convertToIntArray(mDnsReturnCode),
                    NetworkStackUtils.convertToLongArray(mDnsTimeStamp),
                    mEvaluationType, mNetworkType, mTcpFailRate, mTcpSentSinceLastRecv);
        }
    }
}
