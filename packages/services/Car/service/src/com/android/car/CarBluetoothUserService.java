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
package com.android.car;

import android.bluetooth.BluetoothA2dpSink;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadsetClient;
import android.bluetooth.BluetoothMapClient;
import android.bluetooth.BluetoothPan;
import android.bluetooth.BluetoothPbapClient;
import android.bluetooth.BluetoothProfile;
import android.car.ICarBluetoothUserService;
import android.util.Log;
import android.util.SparseBooleanArray;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class CarBluetoothUserService extends ICarBluetoothUserService.Stub {
    private static final String TAG = "CarBluetoothUserService";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    private final PerUserCarService mService;
    private final BluetoothAdapter mBluetoothAdapter;

    // Profiles we support
    private static final List<Integer> sProfilesToConnect = Arrays.asList(
            BluetoothProfile.HEADSET_CLIENT,
            BluetoothProfile.PBAP_CLIENT,
            BluetoothProfile.A2DP_SINK,
            BluetoothProfile.MAP_CLIENT,
            BluetoothProfile.PAN
    );

    // Profile Proxies Objects to pair with above list. Access to these proxy objects will all be
    // guarded by the below mBluetoothProxyLock
    private BluetoothA2dpSink mBluetoothA2dpSink = null;
    private BluetoothHeadsetClient mBluetoothHeadsetClient = null;
    private BluetoothPbapClient mBluetoothPbapClient = null;
    private BluetoothMapClient mBluetoothMapClient = null;
    private BluetoothPan mBluetoothPan = null;

    // Concurrency variables for waitForProxies. Used so we can best effort block with a timeout
    // while waiting for services to be bound to the proxy objects.
    private final ReentrantLock mBluetoothProxyLock;
    private final Condition mConditionAllProxiesConnected;
    private SparseBooleanArray mBluetoothProfileStatus;
    private int mConnectedProfiles;
    private static final int PROXY_OPERATION_TIMEOUT_MS = 8000;

    /**
     * Create a CarBluetoothUserService instance.
     *
     * @param service - A reference to a PerUserCarService, so we can use its context to receive
     *                 updates as a particular user.
     */
    public CarBluetoothUserService(PerUserCarService service) {
        mService = service;
        mConnectedProfiles = 0;
        mBluetoothProfileStatus = new SparseBooleanArray();
        for (int profile : sProfilesToConnect) {
            mBluetoothProfileStatus.put(profile, false);
        }
        mBluetoothProxyLock = new ReentrantLock();
        mConditionAllProxiesConnected = mBluetoothProxyLock.newCondition();
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        Objects.requireNonNull(mBluetoothAdapter, "Bluetooth adapter cannot be null");
    }

    /**
     * Setup connections to the profile proxy objects that talk to the Bluetooth profile services.
     *
     * Proxy references are held by the Bluetooth Framework on our behalf. We will be notified each
     * time the underlying service connects for each proxy we create. Notifications stop when we
     * close the proxy. As such, each time this is called we clean up any existing proxies before
     * creating new ones.
     */
    @Override
    public void setupBluetoothConnectionProxies() {
        logd("Initiate connections to profile proxies");

        // Clear existing proxy objects
        closeBluetoothConnectionProxies();

        // Create proxy for each supported profile. Objects arrive later in the profile listener.
        // Operations on the proxies expect them to be connected. Functions below should call
        // waitForProxies() to best effort wait for them to be up if Bluetooth is enabled.
        for (int profile : sProfilesToConnect) {
            logd("Creating proxy for " + Utils.getProfileName(profile));
            mBluetoothAdapter.getProfileProxy(mService.getApplicationContext(),
                    mProfileListener, profile);
        }
    }

    /**
     * Close connections to the profile proxy objects
     */
    @Override
    public void closeBluetoothConnectionProxies() {
        logd("Clean up profile proxy objects");
        mBluetoothProxyLock.lock();
        try {
            mBluetoothAdapter.closeProfileProxy(BluetoothProfile.A2DP_SINK, mBluetoothA2dpSink);
            mBluetoothA2dpSink = null;
            mBluetoothProfileStatus.put(BluetoothProfile.A2DP_SINK, false);

            mBluetoothAdapter.closeProfileProxy(BluetoothProfile.HEADSET_CLIENT,
                    mBluetoothHeadsetClient);
            mBluetoothHeadsetClient = null;
            mBluetoothProfileStatus.put(BluetoothProfile.HEADSET_CLIENT, false);

            mBluetoothAdapter.closeProfileProxy(BluetoothProfile.PBAP_CLIENT, mBluetoothPbapClient);
            mBluetoothPbapClient = null;
            mBluetoothProfileStatus.put(BluetoothProfile.PBAP_CLIENT, false);

            mBluetoothAdapter.closeProfileProxy(BluetoothProfile.MAP_CLIENT, mBluetoothMapClient);
            mBluetoothMapClient = null;
            mBluetoothProfileStatus.put(BluetoothProfile.MAP_CLIENT, false);

            mBluetoothAdapter.closeProfileProxy(BluetoothProfile.PAN, mBluetoothPan);
            mBluetoothPan = null;
            mBluetoothProfileStatus.put(BluetoothProfile.PAN, false);

            mConnectedProfiles = 0;
        } finally {
            mBluetoothProxyLock.unlock();
        }
    }

    /**
     * Listen for and collect Bluetooth profile proxy connections and disconnections.
     */
    private BluetoothProfile.ServiceListener mProfileListener =
            new BluetoothProfile.ServiceListener() {
        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            logd("onServiceConnected profile: " + Utils.getProfileName(profile));

            // Grab the profile proxy object and update the status book keeping in one step so the
            // book keeping and proxy objects never disagree
            mBluetoothProxyLock.lock();
            try {
                switch (profile) {
                    case BluetoothProfile.A2DP_SINK:
                        mBluetoothA2dpSink = (BluetoothA2dpSink) proxy;
                        break;
                    case BluetoothProfile.HEADSET_CLIENT:
                        mBluetoothHeadsetClient = (BluetoothHeadsetClient) proxy;
                        break;
                    case BluetoothProfile.PBAP_CLIENT:
                        mBluetoothPbapClient = (BluetoothPbapClient) proxy;
                        break;
                    case BluetoothProfile.MAP_CLIENT:
                        mBluetoothMapClient = (BluetoothMapClient) proxy;
                        break;
                    case BluetoothProfile.PAN:
                        mBluetoothPan = (BluetoothPan) proxy;
                        break;
                    default:
                        logd("Unhandled profile connected: " + Utils.getProfileName(profile));
                        break;
                }

                if (!mBluetoothProfileStatus.get(profile, false)) {
                    mBluetoothProfileStatus.put(profile, true);
                    mConnectedProfiles++;
                    if (mConnectedProfiles == sProfilesToConnect.size()) {
                        logd("All profiles have connected");
                        mConditionAllProxiesConnected.signal();
                    }
                } else {
                    Log.w(TAG, "Received duplicate service connection event for: "
                            + Utils.getProfileName(profile));
                }
            } finally {
                mBluetoothProxyLock.unlock();
            }
        }

        public void onServiceDisconnected(int profile) {
            logd("onServiceDisconnected profile: " + Utils.getProfileName(profile));
            mBluetoothProxyLock.lock();
            try {
                if (mBluetoothProfileStatus.get(profile, false)) {
                    mBluetoothProfileStatus.put(profile, false);
                    mConnectedProfiles--;
                } else {
                    Log.w(TAG, "Received duplicate service disconnection event for: "
                            + Utils.getProfileName(profile));
                }
            } finally {
                mBluetoothProxyLock.unlock();
            }
        }
    };

    /**
     * Check if a proxy is available for the given profile to talk to the Profile's bluetooth
     * service.
     *
     * @param profile - Bluetooth profile to check for
     * @return - true if proxy available, false if not.
     */
    @Override
    public boolean isBluetoothConnectionProxyAvailable(int profile) {
        if (!mBluetoothAdapter.isEnabled()) return false;
        boolean proxyConnected = false;
        mBluetoothProxyLock.lock();
        try {
            proxyConnected = mBluetoothProfileStatus.get(profile, false);
        } finally {
            mBluetoothProxyLock.unlock();
        }
        return proxyConnected;
    }

    /**
     * Wait for the proxy objects to be up for all profiles, with a timeout.
     *
     * @param timeout Amount of time in milliseconds to wait for giving up on the wait operation
     * @return True if the condition was satisfied within the timeout, False otherwise
     */
    private boolean waitForProxies(int timeout /* ms */) {
        logd("waitForProxies()");
        // If bluetooth isn't on then the operation waiting on proxies was never meant to actually
        // work regardless if Bluetooth comes on within the timeout period or not. Return false.
        if (!mBluetoothAdapter.isEnabled()) return false;
        try {
            while (mConnectedProfiles != sProfilesToConnect.size()) {
                if (!mConditionAllProxiesConnected.await(
                        timeout, TimeUnit.MILLISECONDS)) {
                    Log.e(TAG, "Timeout while waiting for proxies, Connected: " + mConnectedProfiles
                            + "/" + sProfilesToConnect.size());
                    return false;
                }
            }
        } catch (InterruptedException e) {
            Log.w(TAG, "waitForProxies: interrupted", e);
            return false;
        }
        return true;
    }

    /**
     * Connect a given remote device on a specific Bluetooth profile
     *
     * @param profile BluetoothProfile.* based profile ID
     * @param device The device you wish to connect
     */
    @Override
    public boolean bluetoothConnectToProfile(int profile, BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "Cannot connect to profile on null device");
            return false;
        }
        logd("Trying to connect to " + device.getName() + " (" + device.getAddress() + ") Profile: "
                + Utils.getProfileName(profile));
        mBluetoothProxyLock.lock();
        try {
            if (!isBluetoothConnectionProxyAvailable(profile)
                    && !waitForProxies(PROXY_OPERATION_TIMEOUT_MS)) {
                Log.e(TAG, "Cannot connect to Profile. Proxy Unavailable");
                return false;
            }
            switch (profile) {
                case BluetoothProfile.A2DP_SINK:
                    return mBluetoothA2dpSink.connect(device);
                case BluetoothProfile.HEADSET_CLIENT:
                    return mBluetoothHeadsetClient.connect(device);
                case BluetoothProfile.MAP_CLIENT:
                    return mBluetoothMapClient.connect(device);
                case BluetoothProfile.PBAP_CLIENT:
                    return mBluetoothPbapClient.connect(device);
                case BluetoothProfile.PAN:
                    return mBluetoothPan.connect(device);
                default:
                    Log.w(TAG, "Unknown Profile: " + Utils.getProfileName(profile));
                    break;
            }
        } finally {
            mBluetoothProxyLock.unlock();
        }
        return false;
    }

    /**
     * Disonnect a given remote device from a specific Bluetooth profile
     *
     * @param profile BluetoothProfile.* based profile ID
     * @param device The device you wish to disconnect
     */
    @Override
    public boolean bluetoothDisconnectFromProfile(int profile, BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "Cannot disconnect from profile on null device");
            return false;
        }
        logd("Trying to disconnect from " + device.getName() + " (" + device.getAddress()
                + ") Profile: " + Utils.getProfileName(profile));
        mBluetoothProxyLock.lock();
        try {
            if (!isBluetoothConnectionProxyAvailable(profile)
                    && !waitForProxies(PROXY_OPERATION_TIMEOUT_MS)) {
                Log.e(TAG, "Cannot disconnect from profile. Proxy Unavailable");
                return false;
            }
            switch (profile) {
                case BluetoothProfile.A2DP_SINK:
                    return mBluetoothA2dpSink.disconnect(device);
                case BluetoothProfile.HEADSET_CLIENT:
                    return mBluetoothHeadsetClient.disconnect(device);
                case BluetoothProfile.MAP_CLIENT:
                    return mBluetoothMapClient.disconnect(device);
                case BluetoothProfile.PBAP_CLIENT:
                    return mBluetoothPbapClient.disconnect(device);
                case BluetoothProfile.PAN:
                    return mBluetoothPan.disconnect(device);
                default:
                    Log.w(TAG, "Unknown Profile: " + Utils.getProfileName(profile));
                    break;
            }
        } finally {
            mBluetoothProxyLock.unlock();
        }
        return false;
    }

    /**
     * Get the priority of the given Bluetooth profile for the given remote device
     *
     * @param profile - Bluetooth profile
     * @param device - remote Bluetooth device
     */
    @Override
    public int getProfilePriority(int profile, BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "Cannot get " + Utils.getProfileName(profile)
                    + " profile priority on null device");
            return BluetoothProfile.PRIORITY_UNDEFINED;
        }
        int priority;
        mBluetoothProxyLock.lock();
        try {
            if (!isBluetoothConnectionProxyAvailable(profile)
                    && !waitForProxies(PROXY_OPERATION_TIMEOUT_MS)) {
                Log.e(TAG, "Cannot get " + Utils.getProfileName(profile)
                        + " profile priority. Proxy Unavailable");
                return BluetoothProfile.PRIORITY_UNDEFINED;
            }
            switch (profile) {
                case BluetoothProfile.A2DP_SINK:
                    priority = mBluetoothA2dpSink.getPriority(device);
                    break;
                case BluetoothProfile.HEADSET_CLIENT:
                    priority = mBluetoothHeadsetClient.getPriority(device);
                    break;
                case BluetoothProfile.MAP_CLIENT:
                    priority = mBluetoothMapClient.getPriority(device);
                    break;
                case BluetoothProfile.PBAP_CLIENT:
                    priority = mBluetoothPbapClient.getPriority(device);
                    break;
                default:
                    Log.w(TAG, "Unknown Profile: " + Utils.getProfileName(profile));
                    priority = BluetoothProfile.PRIORITY_UNDEFINED;
                    break;
            }
        } finally {
            mBluetoothProxyLock.unlock();
        }
        logd(Utils.getProfileName(profile) + " priority for " + device.getName() + " ("
                + device.getAddress() + ") = " + priority);
        return priority;
    }

    /**
     * Set the priority of the given Bluetooth profile for the given remote device
     *
     * @param profile - Bluetooth profile
     * @param device - remote Bluetooth device
     * @param priority - priority to set
     */
    @Override
    public void setProfilePriority(int profile, BluetoothDevice device, int priority) {
        if (device == null) {
            Log.e(TAG, "Cannot set " + Utils.getProfileName(profile)
                    + " profile priority on null device");
            return;
        }
        logd("Setting " + Utils.getProfileName(profile) + " priority for " + device.getName() + " ("
                + device.getAddress() + ") to " + priority);
        mBluetoothProxyLock.lock();
        try {
            if (!isBluetoothConnectionProxyAvailable(profile)
                    && !waitForProxies(PROXY_OPERATION_TIMEOUT_MS)) {
                Log.e(TAG, "Cannot set " + Utils.getProfileName(profile)
                        + " profile priority. Proxy Unavailable");
                return;
            }
            switch (profile) {
                case BluetoothProfile.A2DP_SINK:
                    mBluetoothA2dpSink.setPriority(device, priority);
                    break;
                case BluetoothProfile.HEADSET_CLIENT:
                    mBluetoothHeadsetClient.setPriority(device, priority);
                    break;
                case BluetoothProfile.MAP_CLIENT:
                    mBluetoothMapClient.setPriority(device, priority);
                    break;
                case BluetoothProfile.PBAP_CLIENT:
                    mBluetoothPbapClient.setPriority(device, priority);
                    break;
                default:
                    Log.w(TAG, "Unknown Profile: " + Utils.getProfileName(profile));
                    break;
            }
        } finally {
            mBluetoothProxyLock.unlock();
        }
    }

    /**
     * Log to debug if debug output is enabled
     */
    private void logd(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }
}
