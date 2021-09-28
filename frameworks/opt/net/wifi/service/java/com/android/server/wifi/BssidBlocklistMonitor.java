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

package com.android.server.wifi;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.LocalLog;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.wifi.resources.R;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * This class manages the addition and removal of BSSIDs to the BSSID blocklist, which is used
 * for firmware roaming and network selection.
 */
public class BssidBlocklistMonitor {
    // A special type association rejection
    public static final int REASON_AP_UNABLE_TO_HANDLE_NEW_STA = 0;
    // No internet
    public static final int REASON_NETWORK_VALIDATION_FAILURE = 1;
    // Wrong password error
    public static final int REASON_WRONG_PASSWORD = 2;
    // Incorrect EAP credentials
    public static final int REASON_EAP_FAILURE = 3;
    // Other association rejection failures
    public static final int REASON_ASSOCIATION_REJECTION = 4;
    // Association timeout failures.
    public static final int REASON_ASSOCIATION_TIMEOUT = 5;
    // Other authentication failures
    public static final int REASON_AUTHENTICATION_FAILURE = 6;
    // DHCP failures
    public static final int REASON_DHCP_FAILURE = 7;
    // Abnormal disconnect error
    public static final int REASON_ABNORMAL_DISCONNECT = 8;
    // AP initiated disconnect for a given duration.
    public static final int REASON_FRAMEWORK_DISCONNECT_MBO_OCE = 9;
    // Avoid connecting to the failed AP when trying to reconnect on other available candidates.
    public static final int REASON_FRAMEWORK_DISCONNECT_FAST_RECONNECT = 10;
    // The connected scorer has disconnected this network.
    public static final int REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE = 11;
    // Constant being used to keep track of how many failure reasons there are.
    public static final int NUMBER_REASON_CODES = 12;
    public static final int INVALID_REASON = -1;

    @IntDef(prefix = { "REASON_" }, value = {
            REASON_AP_UNABLE_TO_HANDLE_NEW_STA,
            REASON_NETWORK_VALIDATION_FAILURE,
            REASON_WRONG_PASSWORD,
            REASON_EAP_FAILURE,
            REASON_ASSOCIATION_REJECTION,
            REASON_ASSOCIATION_TIMEOUT,
            REASON_AUTHENTICATION_FAILURE,
            REASON_DHCP_FAILURE,
            REASON_ABNORMAL_DISCONNECT,
            REASON_FRAMEWORK_DISCONNECT_MBO_OCE,
            REASON_FRAMEWORK_DISCONNECT_FAST_RECONNECT,
            REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FailureReason {}

    // To be filled with values from the overlay.
    private static final int[] FAILURE_COUNT_DISABLE_THRESHOLD = new int[NUMBER_REASON_CODES];
    private boolean mFailureCountDisableThresholdArrayInitialized = false;
    private static final String[] FAILURE_REASON_STRINGS = {
            "REASON_AP_UNABLE_TO_HANDLE_NEW_STA",
            "REASON_NETWORK_VALIDATION_FAILURE",
            "REASON_WRONG_PASSWORD",
            "REASON_EAP_FAILURE",
            "REASON_ASSOCIATION_REJECTION",
            "REASON_ASSOCIATION_TIMEOUT",
            "REASON_AUTHENTICATION_FAILURE",
            "REASON_DHCP_FAILURE",
            "REASON_ABNORMAL_DISCONNECT",
            "REASON_FRAMEWORK_DISCONNECT_MBO_OCE",
            "REASON_FRAMEWORK_DISCONNECT_FAST_RECONNECT",
            "REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE"
    };
    private static final Set<Integer> LOW_RSSI_SENSITIVE_FAILURES = new ArraySet<>(Arrays.asList(
            REASON_NETWORK_VALIDATION_FAILURE,
            REASON_EAP_FAILURE,
            REASON_ASSOCIATION_REJECTION,
            REASON_ASSOCIATION_TIMEOUT,
            REASON_AUTHENTICATION_FAILURE,
            REASON_DHCP_FAILURE,
            REASON_ABNORMAL_DISCONNECT,
            REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE
    ));
    private static final long ABNORMAL_DISCONNECT_RESET_TIME_MS = TimeUnit.HOURS.toMillis(3);
    private static final int MIN_RSSI_DIFF_TO_UNBLOCK_BSSID = 5;
    private static final String TAG = "BssidBlocklistMonitor";

    private final Context mContext;
    private final WifiLastResortWatchdog mWifiLastResortWatchdog;
    private final WifiConnectivityHelper mConnectivityHelper;
    private final Clock mClock;
    private final LocalLog mLocalLog;
    private final Calendar mCalendar;
    private final WifiScoreCard mWifiScoreCard;
    private final ScoringParams mScoringParams;

    // Map of bssid to BssidStatus
    private Map<String, BssidStatus> mBssidStatusMap = new ArrayMap<>();

    // Keeps history of 30 blocked BSSIDs that were most recently removed.
    private BssidStatusHistoryLogger mBssidStatusHistoryLogger = new BssidStatusHistoryLogger(30);

    /**
     * Create a new instance of BssidBlocklistMonitor
     */
    BssidBlocklistMonitor(Context context, WifiConnectivityHelper connectivityHelper,
            WifiLastResortWatchdog wifiLastResortWatchdog, Clock clock, LocalLog localLog,
            WifiScoreCard wifiScoreCard, ScoringParams scoringParams) {
        mContext = context;
        mConnectivityHelper = connectivityHelper;
        mWifiLastResortWatchdog = wifiLastResortWatchdog;
        mClock = clock;
        mLocalLog = localLog;
        mCalendar = Calendar.getInstance();
        mWifiScoreCard = wifiScoreCard;
        mScoringParams = scoringParams;
    }

    // A helper to log debugging information in the local log buffer, which can
    // be retrieved in bugreport.
    private void localLog(String log) {
        mLocalLog.log(log);
    }

    /**
     * calculates the blocklist duration based on the current failure streak with exponential
     * backoff.
     * @param failureStreak should be greater or equal to 0.
     * @return duration to block the BSSID in milliseconds
     */
    private long getBlocklistDurationWithExponentialBackoff(int failureStreak,
            int baseBlocklistDurationMs) {
        failureStreak = Math.min(failureStreak, mContext.getResources().getInteger(
                R.integer.config_wifiBssidBlocklistMonitorFailureStreakCap));
        if (failureStreak < 1) {
            return baseBlocklistDurationMs;
        }
        return (long) (Math.pow(2.0, (double) failureStreak) * baseBlocklistDurationMs);
    }

    /**
     * Dump the local log buffer and other internal state of BssidBlocklistMonitor.
     */
    public void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
        pw.println("Dump of BssidBlocklistMonitor");
        pw.println("BssidBlocklistMonitor - Bssid blocklist begin ----");
        mBssidStatusMap.values().stream().forEach(entry -> pw.println(entry));
        pw.println("BssidBlocklistMonitor - Bssid blocklist end ----");
        mBssidStatusHistoryLogger.dump(pw);
    }

    private void addToBlocklist(@NonNull BssidStatus entry, long durationMs,
            @FailureReason int reason, int rssi) {
        entry.setAsBlocked(durationMs, reason, rssi);
        localLog(TAG + " addToBlocklist: bssid=" + entry.bssid + ", ssid=" + entry.ssid
                + ", durationMs=" + durationMs + ", reason=" + getFailureReasonString(reason)
                + ", rssi=" + rssi);
    }

    /**
     * increments the number of failures for the given bssid and returns the number of failures so
     * far.
     * @return the BssidStatus for the BSSID
     */
    private @NonNull BssidStatus incrementFailureCountForBssid(
            @NonNull String bssid, @NonNull String ssid, int reasonCode) {
        BssidStatus status = getOrCreateBssidStatus(bssid, ssid);
        status.incrementFailureCount(reasonCode);
        return status;
    }

    /**
     * Get the BssidStatus representing the BSSID or create a new one if it doesn't exist.
     */
    private @NonNull BssidStatus getOrCreateBssidStatus(@NonNull String bssid,
            @NonNull String ssid) {
        BssidStatus status = mBssidStatusMap.get(bssid);
        if (status == null || !ssid.equals(status.ssid)) {
            if (status != null) {
                localLog("getOrCreateBssidStatus: BSSID=" + bssid + ", SSID changed from "
                        + status.ssid + " to " + ssid);
            }
            status = new BssidStatus(bssid, ssid);
            mBssidStatusMap.put(bssid, status);
        }
        return status;
    }

    private boolean isValidNetworkAndFailureReason(String bssid, String ssid,
            @FailureReason int reasonCode) {
        if (bssid == null || ssid == null || WifiManager.UNKNOWN_SSID.equals(ssid)
                || bssid.equals(ClientModeImpl.SUPPLICANT_BSSID_ANY)
                || reasonCode < 0 || reasonCode >= NUMBER_REASON_CODES) {
            Log.e(TAG, "Invalid input: BSSID=" + bssid + ", SSID=" + ssid
                    + ", reasonCode=" + reasonCode);
            return false;
        }
        return true;
    }

    private boolean shouldWaitForWatchdogToTriggerFirst(String bssid,
            @FailureReason int reasonCode) {
        boolean isWatchdogRelatedFailure = reasonCode == REASON_ASSOCIATION_REJECTION
                || reasonCode == REASON_AUTHENTICATION_FAILURE
                || reasonCode == REASON_DHCP_FAILURE;
        return isWatchdogRelatedFailure && mWifiLastResortWatchdog.shouldIgnoreBssidUpdate(bssid);
    }

    /**
     * Block any attempts to auto-connect to the BSSID for the specified duration.
     * This is meant to be used by features that need wifi to avoid a BSSID for a certain duration,
     * and thus will not increase the failure streak counters.
     * @param bssid identifies the AP to block.
     * @param ssid identifies the SSID the AP belongs to.
     * @param durationMs duration in millis to block.
     * @param blockReason reason for blocking the BSSID.
     * @param rssi the latest RSSI observed.
     */
    public void blockBssidForDurationMs(@NonNull String bssid, @NonNull String ssid,
            long durationMs, @FailureReason int blockReason, int rssi) {
        if (durationMs <= 0 || !isValidNetworkAndFailureReason(bssid, ssid, blockReason)) {
            Log.e(TAG, "Invalid input: BSSID=" + bssid + ", SSID=" + ssid
                    + ", durationMs=" + durationMs + ", blockReason=" + blockReason
                    + ", rssi=" + rssi);
            return;
        }
        BssidStatus status = getOrCreateBssidStatus(bssid, ssid);
        if (status.isInBlocklist
                && status.blocklistEndTimeMs - mClock.getWallClockMillis() > durationMs) {
            // Return because this BSSID is already being blocked for a longer time.
            return;
        }
        addToBlocklist(status, durationMs, blockReason, rssi);
    }

    private String getFailureReasonString(@FailureReason int reasonCode) {
        if (reasonCode == INVALID_REASON) {
            return "INVALID_REASON";
        } else if (reasonCode < 0 || reasonCode >= FAILURE_REASON_STRINGS.length) {
            return "REASON_UNKNOWN";
        }
        return FAILURE_REASON_STRINGS[reasonCode];
    }

    private int getFailureThresholdForReason(@FailureReason int reasonCode) {
        if (mFailureCountDisableThresholdArrayInitialized) {
            return FAILURE_COUNT_DISABLE_THRESHOLD[reasonCode];
        }
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_AP_UNABLE_TO_HANDLE_NEW_STA] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorApUnableToHandleNewStaThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_NETWORK_VALIDATION_FAILURE] =
                mContext.getResources().getInteger(R.integer
                        .config_wifiBssidBlocklistMonitorNetworkValidationFailureThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_WRONG_PASSWORD] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorWrongPasswordThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_EAP_FAILURE] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorEapFailureThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_ASSOCIATION_REJECTION] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorAssociationRejectionThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_ASSOCIATION_TIMEOUT] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorAssociationTimeoutThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_AUTHENTICATION_FAILURE] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorAuthenticationFailureThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_DHCP_FAILURE] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorDhcpFailureThreshold);
        FAILURE_COUNT_DISABLE_THRESHOLD[REASON_ABNORMAL_DISCONNECT] =
                mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorAbnormalDisconnectThreshold);
        mFailureCountDisableThresholdArrayInitialized = true;
        return FAILURE_COUNT_DISABLE_THRESHOLD[reasonCode];
    }

    private boolean handleBssidConnectionFailureInternal(String bssid, String ssid,
            @FailureReason int reasonCode, int rssi) {
        BssidStatus entry = incrementFailureCountForBssid(bssid, ssid, reasonCode);
        int failureThreshold = getFailureThresholdForReason(reasonCode);
        int currentStreak = mWifiScoreCard.getBssidBlocklistStreak(ssid, bssid, reasonCode);
        if (currentStreak > 0 || entry.failureCount[reasonCode] >= failureThreshold) {
            // To rule out potential device side issues, don't add to blocklist if
            // WifiLastResortWatchdog is still not triggered
            if (shouldWaitForWatchdogToTriggerFirst(bssid, reasonCode)) {
                return false;
            }
            int baseBlockDurationMs = getBaseBlockDurationForReason(reasonCode);
            addToBlocklist(entry,
                    getBlocklistDurationWithExponentialBackoff(currentStreak, baseBlockDurationMs),
                    reasonCode, rssi);
            mWifiScoreCard.incrementBssidBlocklistStreak(ssid, bssid, reasonCode);
            return true;
        }
        return false;
    }

    private int getBaseBlockDurationForReason(int blockReason) {
        switch (blockReason) {
            case REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE:
                return mContext.getResources().getInteger(R.integer
                        .config_wifiBssidBlocklistMonitorConnectedScoreBaseBlockDurationMs);
            default:
                return mContext.getResources().getInteger(
                        R.integer.config_wifiBssidBlocklistMonitorBaseBlockDurationMs);
        }
    }

    /**
     * Note a failure event on a bssid and perform appropriate actions.
     * @return True if the blocklist has been modified.
     */
    public boolean handleBssidConnectionFailure(String bssid, String ssid,
            @FailureReason int reasonCode, int rssi) {
        if (!isValidNetworkAndFailureReason(bssid, ssid, reasonCode)) {
            return false;
        }
        if (reasonCode == REASON_ABNORMAL_DISCONNECT) {
            long connectionTime = mWifiScoreCard.getBssidConnectionTimestampMs(ssid, bssid);
            // only count disconnects that happen shortly after a connection.
            if (mClock.getWallClockMillis() - connectionTime
                    > mContext.getResources().getInteger(
                            R.integer.config_wifiBssidBlocklistAbnormalDisconnectTimeWindowMs)) {
                return false;
            }
        }
        return handleBssidConnectionFailureInternal(bssid, ssid, reasonCode, rssi);
    }

    /**
     * Note a connection success event on a bssid and clear appropriate failure counters.
     */
    public void handleBssidConnectionSuccess(@NonNull String bssid, @NonNull String ssid) {
        /**
         * First reset the blocklist streak.
         * This needs to be done even if a BssidStatus is not found, since the BssidStatus may
         * have been removed due to blocklist timeout.
         */
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_AP_UNABLE_TO_HANDLE_NEW_STA);
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_WRONG_PASSWORD);
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_EAP_FAILURE);
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_ASSOCIATION_REJECTION);
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_ASSOCIATION_TIMEOUT);
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_AUTHENTICATION_FAILURE);

        long connectionTime = mClock.getWallClockMillis();
        long prevConnectionTime = mWifiScoreCard.setBssidConnectionTimestampMs(
                ssid, bssid, connectionTime);
        if (connectionTime - prevConnectionTime > ABNORMAL_DISCONNECT_RESET_TIME_MS) {
            mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_ABNORMAL_DISCONNECT);
            mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid,
                    REASON_FRAMEWORK_DISCONNECT_CONNECTED_SCORE);
        }

        BssidStatus status = mBssidStatusMap.get(bssid);
        if (status == null) {
            return;
        }
        // Clear the L2 failure counters
        status.failureCount[REASON_AP_UNABLE_TO_HANDLE_NEW_STA] = 0;
        status.failureCount[REASON_WRONG_PASSWORD] = 0;
        status.failureCount[REASON_EAP_FAILURE] = 0;
        status.failureCount[REASON_ASSOCIATION_REJECTION] = 0;
        status.failureCount[REASON_ASSOCIATION_TIMEOUT] = 0;
        status.failureCount[REASON_AUTHENTICATION_FAILURE] = 0;
        if (connectionTime - prevConnectionTime > ABNORMAL_DISCONNECT_RESET_TIME_MS) {
            status.failureCount[REASON_ABNORMAL_DISCONNECT] = 0;
        }
    }

    /**
     * Note a successful network validation on a BSSID and clear appropriate failure counters.
     * And then remove the BSSID from blocklist.
     */
    public void handleNetworkValidationSuccess(@NonNull String bssid, @NonNull String ssid) {
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_NETWORK_VALIDATION_FAILURE);
        BssidStatus status = mBssidStatusMap.get(bssid);
        if (status == null) {
            return;
        }
        status.failureCount[REASON_NETWORK_VALIDATION_FAILURE] = 0;
        /**
         * Network validation may take more than 1 tries to succeed.
         * remove the BSSID from blocklist to make sure we are not accidentally blocking good
         * BSSIDs.
         **/
        if (status.isInBlocklist) {
            mBssidStatusHistoryLogger.add(status, "Network validation success");
            mBssidStatusMap.remove(bssid);
        }
    }

    /**
     * Note a successful DHCP provisioning and clear appropriate faliure counters.
     */
    public void handleDhcpProvisioningSuccess(@NonNull String bssid, @NonNull String ssid) {
        mWifiScoreCard.resetBssidBlocklistStreak(ssid, bssid, REASON_DHCP_FAILURE);
        BssidStatus status = mBssidStatusMap.get(bssid);
        if (status == null) {
            return;
        }
        status.failureCount[REASON_DHCP_FAILURE] = 0;
    }

    /**
     * Note the removal of a network from the Wifi stack's internal database and reset
     * appropriate failure counters.
     * @param ssid
     */
    public void handleNetworkRemoved(@NonNull String ssid) {
        clearBssidBlocklistForSsid(ssid);
        mWifiScoreCard.resetBssidBlocklistStreakForSsid(ssid);
    }

    /**
     * Clears the blocklist for BSSIDs associated with the input SSID only.
     * @param ssid
     */
    public void clearBssidBlocklistForSsid(@NonNull String ssid) {
        int prevSize = mBssidStatusMap.size();
        mBssidStatusMap.entrySet().removeIf(e -> {
            BssidStatus status = e.getValue();
            if (status.ssid == null) {
                return false;
            }
            if (status.ssid.equals(ssid)) {
                mBssidStatusHistoryLogger.add(status, "clearBssidBlocklistForSsid");
                return true;
            }
            return false;
        });
        int diff = prevSize - mBssidStatusMap.size();
        if (diff > 0) {
            localLog(TAG + " clearBssidBlocklistForSsid: SSID=" + ssid
                    + ", num BSSIDs cleared=" + diff);
        }
    }

    /**
     * Clears the BSSID blocklist and failure counters.
     */
    public void clearBssidBlocklist() {
        if (mBssidStatusMap.size() > 0) {
            int prevSize = mBssidStatusMap.size();
            for (BssidStatus status : mBssidStatusMap.values()) {
                mBssidStatusHistoryLogger.add(status, "clearBssidBlocklist");
            }
            mBssidStatusMap.clear();
            localLog(TAG + " clearBssidBlocklist: num BSSIDs cleared="
                    + (prevSize - mBssidStatusMap.size()));
        }
    }

    /**
     * @param ssid
     * @return the number of BSSIDs currently in the blocklist for the |ssid|.
     */
    public int updateAndGetNumBlockedBssidsForSsid(@NonNull String ssid) {
        return (int) updateAndGetBssidBlocklistInternal()
                .filter(entry -> ssid.equals(entry.ssid)).count();
    }

    private int getNumBlockedBssidsForSsid(@Nullable String ssid) {
        if (ssid == null) {
            return 0;
        }
        return (int) mBssidStatusMap.values().stream()
                .filter(entry -> entry.isInBlocklist && ssid.equals(entry.ssid))
                .count();
    }

    /**
     * Overloaded version of updateAndGetBssidBlocklist.
     * Accepts a @Nullable String ssid as input, and updates the firmware roaming
     * configuration if the blocklist for the input ssid has been changed.
     * @param ssid to update firmware roaming configuration for.
     * @return Set of BSSIDs currently in the blocklist
     */
    public Set<String> updateAndGetBssidBlocklistForSsid(@Nullable String ssid) {
        int numBefore = getNumBlockedBssidsForSsid(ssid);
        Set<String> bssidBlocklist = updateAndGetBssidBlocklist();
        if (getNumBlockedBssidsForSsid(ssid) != numBefore) {
            updateFirmwareRoamingConfiguration(ssid);
        }
        return bssidBlocklist;
    }

    /**
     * Gets the BSSIDs that are currently in the blocklist.
     * @return Set of BSSIDs currently in the blocklist
     */
    public Set<String> updateAndGetBssidBlocklist() {
        return updateAndGetBssidBlocklistInternal()
                .map(entry -> entry.bssid)
                .collect(Collectors.toSet());
    }

    /**
     * Gets the list of block reasons for BSSIDs currently in the blocklist.
     * @return The set of unique reasons for blocking BSSIDs with this SSID.
     */
    public Set<Integer> getFailureReasonsForSsid(@NonNull String ssid) {
        if (ssid == null) {
            return Collections.emptySet();
        }
        return mBssidStatusMap.values().stream()
                .filter(entry -> entry.isInBlocklist && ssid.equals(entry.ssid))
                .map(entry -> entry.blockReason)
                .collect(Collectors.toSet());
    }

    /**
     * Attempts to re-enable BSSIDs that likely experienced failures due to low RSSI.
     * @param scanDetails
     */
    public void tryEnablingBlockedBssids(List<ScanDetail> scanDetails) {
        if (scanDetails == null) {
            return;
        }
        for (ScanDetail scanDetail : scanDetails) {
            ScanResult scanResult = scanDetail.getScanResult();
            if (scanResult == null) {
                continue;
            }
            BssidStatus status = mBssidStatusMap.get(scanResult.BSSID);
            if (status == null || !status.isInBlocklist
                    || !LOW_RSSI_SENSITIVE_FAILURES.contains(status.blockReason)) {
                continue;
            }
            int sufficientRssi = mScoringParams.getSufficientRssi(scanResult.frequency);
            if (status.lastRssi < sufficientRssi && scanResult.level >= sufficientRssi
                    && scanResult.level - status.lastRssi >= MIN_RSSI_DIFF_TO_UNBLOCK_BSSID) {
                mBssidStatusHistoryLogger.add(status, "rssi significantly improved");
                mBssidStatusMap.remove(status.bssid);
            }
        }
    }

    /**
     * Removes expired BssidStatus entries and then return remaining entries in the blocklist.
     * @return Stream of BssidStatus for BSSIDs that are in the blocklist.
     */
    private Stream<BssidStatus> updateAndGetBssidBlocklistInternal() {
        Stream.Builder<BssidStatus> builder = Stream.builder();
        long curTime = mClock.getWallClockMillis();
        mBssidStatusMap.entrySet().removeIf(e -> {
            BssidStatus status = e.getValue();
            if (status.isInBlocklist) {
                if (status.blocklistEndTimeMs < curTime) {
                    mBssidStatusHistoryLogger.add(status, "updateAndGetBssidBlocklistInternal");
                    return true;
                }
                builder.accept(status);
            }
            return false;
        });
        return builder.build();
    }

    /**
     * Sends the BSSIDs belonging to the input SSID down to the firmware to prevent auto-roaming
     * to those BSSIDs.
     * @param ssid
     */
    public void updateFirmwareRoamingConfiguration(@NonNull String ssid) {
        if (!mConnectivityHelper.isFirmwareRoamingSupported()) {
            return;
        }
        ArrayList<String> bssidBlocklist = updateAndGetBssidBlocklistInternal()
                .filter(entry -> ssid.equals(entry.ssid))
                .sorted((o1, o2) -> (int) (o2.blocklistEndTimeMs - o1.blocklistEndTimeMs))
                .map(entry -> entry.bssid)
                .collect(Collectors.toCollection(ArrayList::new));
        int fwMaxBlocklistSize = mConnectivityHelper.getMaxNumBlacklistBssid();
        if (fwMaxBlocklistSize <= 0) {
            Log.e(TAG, "Invalid max BSSID blocklist size:  " + fwMaxBlocklistSize);
            return;
        }
        // Having the blocklist size exceeding firmware max limit is unlikely because we have
        // already flitered based on SSID. But just in case this happens, we are prioritizing
        // sending down BSSIDs blocked for the longest time.
        if (bssidBlocklist.size() > fwMaxBlocklistSize) {
            bssidBlocklist = new ArrayList<String>(bssidBlocklist.subList(0,
                    fwMaxBlocklistSize));
        }
        // plumb down to HAL
        if (!mConnectivityHelper.setFirmwareRoamingConfiguration(bssidBlocklist,
                new ArrayList<String>())) {  // TODO(b/36488259): SSID whitelist management.
        }
    }

    @VisibleForTesting
    public int getBssidStatusHistoryLoggerSize() {
        return mBssidStatusHistoryLogger.size();
    }

    private class BssidStatusHistoryLogger {
        private LinkedList<String> mLogHistory = new LinkedList<>();
        private int mBufferSize;

        BssidStatusHistoryLogger(int bufferSize) {
            mBufferSize = bufferSize;
        }

        public void add(BssidStatus bssidStatus, String trigger) {
            // only log history for Bssids that had been blocked.
            if (bssidStatus == null || !bssidStatus.isInBlocklist) {
                return;
            }
            StringBuilder sb = new StringBuilder();
            mCalendar.setTimeInMillis(mClock.getWallClockMillis());
            sb.append(", logTimeMs="
                    + String.format("%tm-%td %tH:%tM:%tS.%tL", mCalendar, mCalendar,
                    mCalendar, mCalendar, mCalendar, mCalendar));
            sb.append(", trigger=" + trigger);
            mLogHistory.add(bssidStatus.toString() + sb.toString());
            if (mLogHistory.size() > mBufferSize) {
                mLogHistory.removeFirst();
            }
        }

        @VisibleForTesting
        public int size() {
            return mLogHistory.size();
        }

        public void dump(PrintWriter pw) {
            pw.println("BssidBlocklistMonitor - Bssid blocklist history begin ----");
            for (String line : mLogHistory) {
                pw.println(line);
            }
            pw.println("BssidBlocklistMonitor - Bssid blocklist history end ----");
        }
    }

    /**
     * Helper class that counts the number of failures per BSSID.
     */
    private class BssidStatus {
        public final String bssid;
        public final String ssid;
        public final int[] failureCount = new int[NUMBER_REASON_CODES];
        public int blockReason = INVALID_REASON; // reason of blocking this BSSID
        // The latest RSSI that's seen before this BSSID is added to blocklist.
        public int lastRssi = 0;

        // The following are used to flag how long this BSSID stays in the blocklist.
        public boolean isInBlocklist;
        public long blocklistEndTimeMs;
        public long blocklistStartTimeMs;

        BssidStatus(String bssid, String ssid) {
            this.bssid = bssid;
            this.ssid = ssid;
        }

        /**
         * increments the failure count for the reasonCode by 1.
         * @return the incremented failure count
         */
        public int incrementFailureCount(int reasonCode) {
            return ++failureCount[reasonCode];
        }

        /**
         * Set this BSSID as blocked for the specified duration.
         * @param durationMs
         * @param blockReason
         * @param rssi
         */
        public void setAsBlocked(long durationMs, @FailureReason int blockReason, int rssi) {
            isInBlocklist = true;
            blocklistStartTimeMs = mClock.getWallClockMillis();
            blocklistEndTimeMs = blocklistStartTimeMs + durationMs;
            this.blockReason = blockReason;
            lastRssi = rssi;
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("BSSID=" + bssid);
            sb.append(", SSID=" + ssid);
            sb.append(", isInBlocklist=" + isInBlocklist);
            if (isInBlocklist) {
                sb.append(", blockReason=" + getFailureReasonString(blockReason));
                sb.append(", lastRssi=" + lastRssi);
                mCalendar.setTimeInMillis(blocklistStartTimeMs);
                sb.append(", blocklistStartTimeMs="
                        + String.format("%tm-%td %tH:%tM:%tS.%tL", mCalendar, mCalendar,
                        mCalendar, mCalendar, mCalendar, mCalendar));
                mCalendar.setTimeInMillis(blocklistEndTimeMs);
                sb.append(", blocklistEndTimeMs="
                        + String.format("%tm-%td %tH:%tM:%tS.%tL", mCalendar, mCalendar,
                        mCalendar, mCalendar, mCalendar, mCalendar));
            }
            return sb.toString();
        }
    }
}
