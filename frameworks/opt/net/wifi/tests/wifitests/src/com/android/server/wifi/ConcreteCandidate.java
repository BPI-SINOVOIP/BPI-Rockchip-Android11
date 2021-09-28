/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.server.wifi;

import android.util.ArrayMap;

import com.android.server.wifi.proto.WifiScoreCardProto;

import java.util.Map;

public final class ConcreteCandidate implements WifiCandidates.Candidate {
    private WifiCandidates.Key mKey;
    private int mNetworkConfigId = -1;
    private boolean mIsOpenNetwork;
    private boolean mIsCurrentNetwork;
    private boolean mIsCurrentBssid;
    private boolean mIsPasspoint;
    private boolean mIsEphemeral;
    private boolean mIsTrusted = true;
    private boolean mCarrierOrPrivileged;
    private boolean mIsMetered;
    private boolean mHasNoInternetAccess;
    private boolean mIsNoInternetAccessExpected;
    private int mNominatorId = -1;
    private double mLastSelectionWeight;
    private int mScanRssi = -127;
    private int mFrequency = -1;
    private int mPredictedThroughputMbps = 0;
    private int mEstimatedPercentInternetAvailability = 50;

    private final Map<WifiScoreCardProto.Event, WifiScoreCardProto.Signal>
            mEventStatisticsMap = new ArrayMap<>();

    ConcreteCandidate() {
    }

    public ConcreteCandidate(WifiCandidates.Candidate candidate) {
        mKey = candidate.getKey();
        mNetworkConfigId = candidate.getNetworkConfigId();
        mIsOpenNetwork = candidate.isOpenNetwork();
        mIsCurrentNetwork = candidate.isCurrentNetwork();
        mIsCurrentBssid = candidate.isCurrentBssid();
        mIsPasspoint = candidate.isPasspoint();
        mIsEphemeral = candidate.isEphemeral();
        mIsTrusted = candidate.isTrusted();
        mCarrierOrPrivileged = candidate.isCarrierOrPrivileged();
        mIsMetered = candidate.isMetered();
        mHasNoInternetAccess = candidate.hasNoInternetAccess();
        mIsNoInternetAccessExpected = candidate.isNoInternetAccessExpected();
        mNominatorId = candidate.getNominatorId();
        mLastSelectionWeight = candidate.getLastSelectionWeight();
        mScanRssi = candidate.getScanRssi();
        mFrequency = candidate.getFrequency();
        mPredictedThroughputMbps = candidate.getPredictedThroughputMbps();
        mEstimatedPercentInternetAvailability = candidate
                .getEstimatedPercentInternetAvailability();
        for (WifiScoreCardProto.Event event : WifiScoreCardProto.Event.values()) {
            WifiScoreCardProto.Signal signal = candidate.getEventStatistics(event);
            if (signal != null) {
                mEventStatisticsMap.put(event, signal);
            }
        }
    }

    public ConcreteCandidate setKey(WifiCandidates.Key key) {
        mKey = key;
        return this;
    }

    @Override
    public WifiCandidates.Key getKey() {
        return mKey;
    }

    public ConcreteCandidate setNetworkConfigId(int networkConfigId) {
        mNetworkConfigId = networkConfigId;
        return this;
    }

    @Override
    public int getNetworkConfigId() {
        return mNetworkConfigId;
    }

    public ConcreteCandidate setOpenNetwork(boolean isOpenNetwork) {
        mIsOpenNetwork = isOpenNetwork;
        return this;
    }

    @Override
    public boolean isOpenNetwork() {
        return mIsOpenNetwork;
    }

    public ConcreteCandidate setPasspoint(boolean isPasspoint) {
        mIsPasspoint = isPasspoint;
        return this;
    }

    @Override
    public boolean isPasspoint() {
        return mIsPasspoint;
    }

    public ConcreteCandidate setEphemeral(boolean isEphemeral) {
        mIsEphemeral = isEphemeral;
        return this;
    }

    @Override
    public boolean isEphemeral() {
        return mIsEphemeral;
    }

    public ConcreteCandidate setTrusted(boolean isTrusted) {
        mIsTrusted = isTrusted;
        return this;
    }

    @Override
    public boolean isTrusted() {
        return mIsTrusted;
    }

    public ConcreteCandidate setCarrierOrPrivileged(boolean carrierOrPrivileged) {
        mCarrierOrPrivileged = carrierOrPrivileged;
        return this;
    }

    @Override
    public boolean isCarrierOrPrivileged() {
        return mCarrierOrPrivileged;
    }

    public ConcreteCandidate setMetered(boolean isMetered) {
        mIsMetered = isMetered;
        return this;
    }

    @Override
    public boolean isMetered() {
        return mIsMetered;
    }

    public ConcreteCandidate setNoInternetAccess(boolean hasNoInternetAccess) {
        mHasNoInternetAccess = hasNoInternetAccess;
        return this;
    }

    @Override
    public boolean hasNoInternetAccess() {
        return mHasNoInternetAccess;
    }

    public ConcreteCandidate setNoInternetAccessExpected(boolean isNoInternetAccessExpected) {
        mIsNoInternetAccessExpected = isNoInternetAccessExpected;
        return this;
    }

    @Override
    public boolean isNoInternetAccessExpected() {
        return mIsNoInternetAccessExpected;
    }

    public ConcreteCandidate setNominatorId(int nominatorId) {
        mNominatorId = nominatorId;
        return this;
    }

    @Override
    public int getNominatorId() {
        return mNominatorId;
    }

    public ConcreteCandidate setLastSelectionWeight(double lastSelectionWeight) {
        mLastSelectionWeight = lastSelectionWeight;
        return this;
    }

    @Override
    public double getLastSelectionWeight() {
        return mLastSelectionWeight;
    }

    public ConcreteCandidate setCurrentNetwork(boolean isCurrentNetwork) {
        mIsCurrentNetwork = isCurrentNetwork;
        return this;
    }

    @Override
    public boolean isCurrentNetwork() {
        return mIsCurrentNetwork;
    }

    public ConcreteCandidate setCurrentBssid(boolean isCurrentBssid) {
        mIsCurrentBssid = isCurrentBssid;
        return this;
    }

    @Override
    public boolean isCurrentBssid() {
        return mIsCurrentBssid;
    }

    public ConcreteCandidate setScanRssi(int scanRssi) {
        mScanRssi = scanRssi;
        return this;
    }

    @Override
    public int getScanRssi() {
        return mScanRssi;
    }

    public ConcreteCandidate setFrequency(int frequency) {
        mFrequency = frequency;
        return this;
    }

    @Override
    public int getFrequency() {
        return mFrequency;
    }

    public ConcreteCandidate setPredictedThroughputMbps(int predictedThroughputMbps) {
        mPredictedThroughputMbps = predictedThroughputMbps;
        return this;
    }

    @Override
    public int getPredictedThroughputMbps() {
        return mPredictedThroughputMbps;
    }

    public ConcreteCandidate setEstimatedPercentInternetAvailability(int percent) {
        mEstimatedPercentInternetAvailability = percent;
        return this;
    }

    @Override
    public int getEstimatedPercentInternetAvailability() {
        return mEstimatedPercentInternetAvailability;
    }

    public ConcreteCandidate setEventStatistics(
            WifiScoreCardProto.Event event,
            WifiScoreCardProto.Signal signal) {
        mEventStatisticsMap.put(event, signal);
        return this;
    }

    @Override
    public WifiScoreCardProto.Signal getEventStatistics(WifiScoreCardProto.Event event) {
        return mEventStatisticsMap.get(event);
    }

}
