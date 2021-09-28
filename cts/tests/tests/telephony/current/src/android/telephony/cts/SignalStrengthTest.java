/*
 * Copyright (C) 2014 The Android Open Source Project
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
package android.telephony.cts;

import static android.telephony.NetworkRegistrationInfo.NR_STATE_CONNECTED;

import static androidx.test.InstrumentationRegistry.getContext;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.SystemClock;
import android.telephony.CellSignalStrength;
import android.telephony.CellSignalStrengthCdma;
import android.telephony.CellSignalStrengthGsm;
import android.telephony.CellSignalStrengthLte;
import android.telephony.CellSignalStrengthNr;
import android.telephony.CellSignalStrengthTdscdma;
import android.telephony.CellSignalStrengthWcdma;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.util.Log;

import org.junit.Before;
import org.junit.Test;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Test SignalStrength to ensure that valid data is being reported and that invalid data is
 * not reported.
 */
public class SignalStrengthTest {
    private static final String TAG = "SignalStrengthTest";

    private TelephonyManager mTm;
    private PackageManager mPm;

    // Check that the device is camped on a cellular cell */
    private boolean isCamped() {
        ServiceState ss = mTm.getServiceState();
        if (ss == null) return false;
        return (ss.getState() == ServiceState.STATE_IN_SERVICE
                || ss.getState() == ServiceState.STATE_EMERGENCY_ONLY);
    }

    @Before
    public void setUp() throws Exception {
        mTm = (TelephonyManager) getContext().getSystemService(Context.TELEPHONY_SERVICE);
        mPm = getContext().getPackageManager();
    }

    @Test
    public void testSignalStrength() throws Throwable {
        if (!mPm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }

        if (!isCamped()) fail("Device is not camped on cellular");

        // Detect accidental connection to Wi-Fi for data, which fails the test,
        // due to IWLAN often being enabled.
        assertDeviceIsOnCellDataOrNoData();

        SignalStrength ss = mTm.getSignalStrength();
        assertNotNull("TelephonyManager.getSignalStrength() returned NULL!", ss);

        long curTime = SystemClock.elapsedRealtime();
        assertTrue("Invalid timestamp in SignalStrength: " + ss.getTimestampMillis(),
                ss.getTimestampMillis() > 0 && ss.getTimestampMillis() <= curTime);
        Log.d(TAG, "Timestamp of SignalStrength: " + Long.toString(ss.getTimestampMillis()));

        List<CellSignalStrength> signalStrengths = ss.getCellSignalStrengths();

        assertTrue("No Signal Strength Information Reported!", !signalStrengths.isEmpty());

        Set<Class<? extends CellSignalStrength>> types =
                new HashSet<Class<? extends CellSignalStrength>>();

        Class<? extends CellSignalStrength> dataType =
                getSignalStrengthTypeForNetworkType(mTm.getDataNetworkType());
        if (dataType != null) types.add(dataType);

        Class<? extends CellSignalStrength> voiceType =
                getSignalStrengthTypeForNetworkType(mTm.getVoiceNetworkType());

        // Check if camped for Voice-Only
        if (dataType == null && voiceType != null) {
            types.add(voiceType);
        }

        // Check for SRLTE
        if (dataType != null && voiceType != null
                && dataType.equals(CellSignalStrengthLte.class)
                && voiceType.equals(CellSignalStrengthCdma.class)) {
            types.add(voiceType);
        }

        // Check for NR
        if (isUsingEnDc()) {
            types.add(CellSignalStrengthNr.class);
        }

        for (CellSignalStrength css : signalStrengths) {
            assertTrue("Invalid SignalStrength type detected:" + css.getClass()
                    + " - Allowed Types: " + types, types.contains(css.getClass()));
        }

        assertTrue("No valid signal strengths reported for the camped/registered network types",
                (dataType != null && !ss.getCellSignalStrengths(dataType).isEmpty())
                        || (voiceType != null && !ss.getCellSignalStrengths(voiceType).isEmpty()));
    }

    /** Check whether the device is LTE + NR dual connected */
    private boolean isUsingEnDc() {
        ServiceState ss = mTm.getServiceState();
        return ss != null && ss.getNrState() == NR_STATE_CONNECTED;
    }

    private void assertDeviceIsOnCellDataOrNoData() {
        ConnectivityManager cm = getContext().getSystemService(ConnectivityManager.class);
        Network activeNetwork = cm.getActiveNetwork();
        if (activeNetwork == null) return;
        assertTrue("Device is connected and using non-cellular data (likely Wi-Fi)",
                cm.getNetworkCapabilities(activeNetwork)
                        .hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR));
    }

    /** Get the CellSignalStrength class type that should be returned when using a network type */
    private static Class<? extends CellSignalStrength>
            getSignalStrengthTypeForNetworkType(int networkType) {
        switch(networkType) {
            case TelephonyManager.NETWORK_TYPE_GPRS: /* fall through */
            case TelephonyManager.NETWORK_TYPE_EDGE: /* fall through */
            case TelephonyManager.NETWORK_TYPE_GSM:
                return CellSignalStrengthGsm.class;
            case TelephonyManager.NETWORK_TYPE_UMTS: /* fall through */
            case TelephonyManager.NETWORK_TYPE_HSDPA: /* fall through */
            case TelephonyManager.NETWORK_TYPE_HSUPA: /* fall through */
            case TelephonyManager.NETWORK_TYPE_HSPA: /* fall through */
            case TelephonyManager.NETWORK_TYPE_HSPAP:
                return CellSignalStrengthWcdma.class;
            case TelephonyManager.NETWORK_TYPE_TD_SCDMA:
                return CellSignalStrengthTdscdma.class;
            case TelephonyManager.NETWORK_TYPE_CDMA: /* fall through */
            case TelephonyManager.NETWORK_TYPE_1xRTT: /* fall through */
            case TelephonyManager.NETWORK_TYPE_EVDO_0: /* fall through */
            case TelephonyManager.NETWORK_TYPE_EVDO_A: /* fall through */
            case TelephonyManager.NETWORK_TYPE_EVDO_B: /* fall through */
            case TelephonyManager.NETWORK_TYPE_EHRPD:
                return CellSignalStrengthCdma.class;
            case TelephonyManager.NETWORK_TYPE_LTE: /* fall through */
            case TelephonyManager.NETWORK_TYPE_LTE_CA:
                return CellSignalStrengthLte.class;
            case TelephonyManager.NETWORK_TYPE_IWLAN: /* fall through */
            case TelephonyManager.NETWORK_TYPE_IDEN: /* fall through */
            case TelephonyManager.NETWORK_TYPE_NR: /* fall through */
            default:
                return null;
        }
    }
}
