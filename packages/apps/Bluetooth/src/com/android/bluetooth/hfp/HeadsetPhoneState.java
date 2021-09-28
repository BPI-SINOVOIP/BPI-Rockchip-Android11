/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.bluetooth.hfp;

import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.os.Handler;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionManager.OnSubscriptionsChangedListener;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;

import java.util.HashMap;
import java.util.Objects;
import java.util.concurrent.Executor;


/**
 * Class that manages Telephony states
 *
 * Note:
 * The methods in this class are not thread safe, don't call them from
 * multiple threads. Call them from the HeadsetPhoneStateMachine message
 * handler only.
 */
public class HeadsetPhoneState {
    private static final String TAG = "HeadsetPhoneState";

    private final HeadsetService mHeadsetService;
    private final TelephonyManager mTelephonyManager;
    private final SubscriptionManager mSubscriptionManager;
    private final Handler mHandler;

    private ServiceState mServiceState;

    // HFP 1.6 CIND service value
    private int mCindService = HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
    // Number of active (foreground) calls
    private int mNumActive;
    // Current Call Setup State
    private int mCallState = HeadsetHalConstants.CALL_STATE_IDLE;
    // Number of held (background) calls
    private int mNumHeld;
    // HFP 1.6 CIND signal value
    private int mCindSignal;
    // HFP 1.6 CIND roam value
    private int mCindRoam = HeadsetHalConstants.SERVICE_TYPE_HOME;
    // HFP 1.6 CIND battchg value
    private int mCindBatteryCharge;

    private final HashMap<BluetoothDevice, Integer> mDeviceEventMap = new HashMap<>();
    private PhoneStateListener mPhoneStateListener;
    private final OnSubscriptionsChangedListener mOnSubscriptionsChangedListener;

    HeadsetPhoneState(HeadsetService headsetService) {
        Objects.requireNonNull(headsetService, "headsetService is null");
        mHeadsetService = headsetService;
        mTelephonyManager =
                (TelephonyManager) mHeadsetService.getSystemService(Context.TELEPHONY_SERVICE);
        Objects.requireNonNull(mTelephonyManager, "TELEPHONY_SERVICE is null");
        // Register for SubscriptionInfo list changes which is guaranteed to invoke
        // onSubscriptionInfoChanged and which in turns calls loadInBackgroud.
        mSubscriptionManager = SubscriptionManager.from(mHeadsetService);
        Objects.requireNonNull(mSubscriptionManager, "TELEPHONY_SUBSCRIPTION_SERVICE is null");
        // Initialize subscription on the handler thread
        mHandler = new Handler(headsetService.getStateMachinesThreadLooper());
        mOnSubscriptionsChangedListener = new HeadsetPhoneStateOnSubscriptionChangedListener();
        mSubscriptionManager.addOnSubscriptionsChangedListener(command -> mHandler.post(command),
                mOnSubscriptionsChangedListener);
    }

    /**
     * Cleanup this instance. Instance can no longer be used after calling this method.
     */
    public void cleanup() {
        synchronized (mDeviceEventMap) {
            mDeviceEventMap.clear();
            stopListenForPhoneState();
        }
        mSubscriptionManager.removeOnSubscriptionsChangedListener(mOnSubscriptionsChangedListener);
    }

    @Override
    public String toString() {
        return "HeadsetPhoneState [mTelephonyServiceAvailability=" + mCindService + ", mNumActive="
                + mNumActive + ", mCallState=" + mCallState + ", mNumHeld=" + mNumHeld
                + ", mSignal=" + mCindSignal + ", mRoam=" + mCindRoam + ", mBatteryCharge="
                + mCindBatteryCharge + ", TelephonyEvents=" + getTelephonyEventsToListen() + "]";
    }

    private int getTelephonyEventsToListen() {
        synchronized (mDeviceEventMap) {
            return mDeviceEventMap.values()
                    .stream()
                    .reduce(PhoneStateListener.LISTEN_NONE, (a, b) -> a | b);
        }
    }

    /**
     * Start or stop listening for phone state change
     *
     * @param device remote device that subscribes to this phone state update
     * @param events events in {@link PhoneStateListener} to listen to
     */
    @VisibleForTesting
    public void listenForPhoneState(BluetoothDevice device, int events) {
        synchronized (mDeviceEventMap) {
            int prevEvents = getTelephonyEventsToListen();
            if (events == PhoneStateListener.LISTEN_NONE) {
                mDeviceEventMap.remove(device);
            } else {
                mDeviceEventMap.put(device, events);
            }
            int updatedEvents = getTelephonyEventsToListen();
            if (prevEvents != updatedEvents) {
                stopListenForPhoneState();
                startListenForPhoneState();
            }
        }
    }

    private void startListenForPhoneState() {
        if (mPhoneStateListener != null) {
            Log.w(TAG, "startListenForPhoneState, already listening");
            return;
        }
        int events = getTelephonyEventsToListen();
        if (events == PhoneStateListener.LISTEN_NONE) {
            Log.w(TAG, "startListenForPhoneState, no event to listen");
            return;
        }
        int subId = SubscriptionManager.getDefaultSubscriptionId();
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            // Will retry listening for phone state in onSubscriptionsChanged() callback
            Log.w(TAG, "startListenForPhoneState, invalid subscription ID " + subId);
            return;
        }
        Log.i(TAG, "startListenForPhoneState(), subId=" + subId + ", enabled_events=" + events);
        mPhoneStateListener = new HeadsetPhoneStateListener(command -> mHandler.post(command));
        mTelephonyManager.listen(mPhoneStateListener, events);
    }

    private void stopListenForPhoneState() {
        if (mPhoneStateListener == null) {
            Log.i(TAG, "stopListenForPhoneState(), no listener indicates nothing is listening");
            return;
        }
        Log.i(TAG, "stopListenForPhoneState(), stopping listener, enabled_events="
                + getTelephonyEventsToListen());
        mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
        mPhoneStateListener = null;
    }

    int getCindService() {
        return mCindService;
    }

    int getNumActiveCall() {
        return mNumActive;
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setNumActiveCall(int numActive) {
        mNumActive = numActive;
    }

    int getCallState() {
        return mCallState;
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setCallState(int callState) {
        mCallState = callState;
    }

    int getNumHeldCall() {
        return mNumHeld;
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setNumHeldCall(int numHeldCall) {
        mNumHeld = numHeldCall;
    }

    ServiceState getServiceState() {
        return mServiceState;
    }

    int getCindSignal() {
        return mCindSignal;
    }

    int getCindRoam() {
        return mCindRoam;
    }

    /**
     * Set battery level value used for +CIND result
     *
     * @param batteryLevel battery level value
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setCindBatteryCharge(int batteryLevel) {
        if (mCindBatteryCharge != batteryLevel) {
            mCindBatteryCharge = batteryLevel;
            sendDeviceStateChanged();
        }
    }

    int getCindBatteryCharge() {
        return mCindBatteryCharge;
    }

    boolean isInCall() {
        return (mNumActive >= 1);
    }

    private synchronized void sendDeviceStateChanged() {
        // When out of service, send signal strength as 0. Some devices don't
        // use the service indicator, but only the signal indicator
        int signal = mCindService == HeadsetHalConstants.NETWORK_STATE_AVAILABLE ? mCindSignal : 0;

        Log.d(TAG, "sendDeviceStateChanged. mService=" + mCindService
                + " mSignal=" + mCindSignal + " mRoam=" + mCindRoam
                + " mBatteryCharge=" + mCindBatteryCharge);
        mHeadsetService.onDeviceStateChanged(
                new HeadsetDeviceState(mCindService, mCindRoam, signal, mCindBatteryCharge));
    }

    private class HeadsetPhoneStateOnSubscriptionChangedListener
            extends OnSubscriptionsChangedListener {
        HeadsetPhoneStateOnSubscriptionChangedListener() {
            super();
        }

        @Override
        public void onSubscriptionsChanged() {
            synchronized (mDeviceEventMap) {
                int simState = mTelephonyManager.getSimState();
                if (simState != TelephonyManager.SIM_STATE_READY) {
                    mServiceState = null;
                    mCindSignal = 0;
                    mCindService = HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
                    sendDeviceStateChanged();
                }
                stopListenForPhoneState();
                startListenForPhoneState();
            }
        }
    }

    private class HeadsetPhoneStateListener extends PhoneStateListener {
        HeadsetPhoneStateListener(Executor executor) {
            super(executor);
        }

        @Override
        public synchronized void onServiceStateChanged(ServiceState serviceState) {
            mServiceState = serviceState;
            int cindService = (serviceState.getState() == ServiceState.STATE_IN_SERVICE)
                    ? HeadsetHalConstants.NETWORK_STATE_AVAILABLE
                    : HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
            int newRoam = serviceState.getRoaming() ? HeadsetHalConstants.SERVICE_TYPE_ROAMING
                    : HeadsetHalConstants.SERVICE_TYPE_HOME;

            if (cindService == mCindService && newRoam == mCindRoam) {
                // De-bounce the state change
                return;
            }
            mCindService = cindService;
            mCindRoam = newRoam;
            sendDeviceStateChanged();
        }

        @Override
        public void onSignalStrengthsChanged(SignalStrength signalStrength) {
            int prevSignal = mCindSignal;
            if (mCindService == HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE) {
                mCindSignal = 0;
            } else {
                mCindSignal = signalStrength.getLevel() + 1;
            }
            // +CIND "signal" indicator is always between 0 to 5
            mCindSignal = Integer.max(Integer.min(mCindSignal, 5), 0);
            // This results in a lot of duplicate messages, hence this check
            if (prevSignal != mCindSignal) {
                sendDeviceStateChanged();
            }
        }
    }
}
