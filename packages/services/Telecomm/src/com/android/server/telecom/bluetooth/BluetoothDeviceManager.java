/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.server.telecom.bluetooth;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothHearingAid;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.telecom.Log;
import android.util.LocalLog;

import com.android.internal.util.IndentingPrintWriter;
import com.android.server.telecom.BluetoothAdapterProxy;
import com.android.server.telecom.BluetoothHeadsetProxy;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

public class BluetoothDeviceManager {
    private final BluetoothProfile.ServiceListener mBluetoothProfileServiceListener =
            new BluetoothProfile.ServiceListener() {
                @Override
                public void onServiceConnected(int profile, BluetoothProfile proxy) {
                    Log.startSession("BMSL.oSC");
                    try {
                        synchronized (mLock) {
                            String logString;
                            if (profile == BluetoothProfile.HEADSET) {
                                mBluetoothHeadsetService =
                                        new BluetoothHeadsetProxy((BluetoothHeadset) proxy);
                                logString = "Got BluetoothHeadset: " + mBluetoothHeadsetService;
                            } else if (profile == BluetoothProfile.HEARING_AID) {
                                mBluetoothHearingAidService = (BluetoothHearingAid) proxy;
                                logString = "Got BluetoothHearingAid: "
                                        + mBluetoothHearingAidService;
                            } else {
                                logString = "Connected to non-requested bluetooth service." +
                                        " Not changing bluetooth headset.";
                            }
                            Log.i(BluetoothDeviceManager.this, logString);
                            mLocalLog.log(logString);
                        }
                    } finally {
                        Log.endSession();
                    }
                }

                @Override
                public void onServiceDisconnected(int profile) {
                    Log.startSession("BMSL.oSD");
                    try {
                        synchronized (mLock) {
                            LinkedHashMap<String, BluetoothDevice> lostServiceDevices;
                            String logString;
                            if (profile == BluetoothProfile.HEADSET) {
                                mBluetoothHeadsetService = null;
                                lostServiceDevices = mHfpDevicesByAddress;
                                mBluetoothRouteManager.onActiveDeviceChanged(null, false);
                                logString = "Lost BluetoothHeadset service. " +
                                        "Removing all tracked devices";
                            } else if (profile == BluetoothProfile.HEARING_AID) {
                                mBluetoothHearingAidService = null;
                                logString = "Lost BluetoothHearingAid service. " +
                                        "Removing all tracked devices.";
                                lostServiceDevices = mHearingAidDevicesByAddress;
                                mBluetoothRouteManager.onActiveDeviceChanged(null, true);
                            } else {
                                return;
                            }
                            Log.i(BluetoothDeviceManager.this, logString);
                            mLocalLog.log(logString);

                            List<BluetoothDevice> devicesToRemove = new LinkedList<>(
                                    lostServiceDevices.values());
                            lostServiceDevices.clear();
                            for (BluetoothDevice device : devicesToRemove) {
                                mBluetoothRouteManager.onDeviceLost(device.getAddress());
                            }
                        }
                    } finally {
                        Log.endSession();
                    }
                }
           };

    private final LinkedHashMap<String, BluetoothDevice> mHfpDevicesByAddress =
            new LinkedHashMap<>();
    private final LinkedHashMap<String, BluetoothDevice> mHearingAidDevicesByAddress =
            new LinkedHashMap<>();
    private final LinkedHashMap<BluetoothDevice, Long> mHearingAidDeviceSyncIds =
            new LinkedHashMap<>();
    private final LocalLog mLocalLog = new LocalLog(20);

    // This lock only protects internal state -- it doesn't lock on anything going into Telecom.
    private final Object mLock = new Object();

    private BluetoothRouteManager mBluetoothRouteManager;
    private BluetoothHeadsetProxy mBluetoothHeadsetService;
    private BluetoothHearingAid mBluetoothHearingAidService;
    private BluetoothDevice mBluetoothHearingAidActiveDeviceCache;
    private BluetoothAdapterProxy mBluetoothAdapterProxy;

    public BluetoothDeviceManager(Context context, BluetoothAdapterProxy bluetoothAdapter) {
        if (bluetoothAdapter != null) {
            mBluetoothAdapterProxy = bluetoothAdapter;
            bluetoothAdapter.getProfileProxy(context, mBluetoothProfileServiceListener,
                    BluetoothProfile.HEADSET);
            bluetoothAdapter.getProfileProxy(context, mBluetoothProfileServiceListener,
                    BluetoothProfile.HEARING_AID);
        }
    }

    public void setBluetoothRouteManager(BluetoothRouteManager brm) {
        mBluetoothRouteManager = brm;
    }

    public int getNumConnectedDevices() {
        synchronized (mLock) {
            return mHfpDevicesByAddress.size() + mHearingAidDevicesByAddress.size();
        }
    }

    public Collection<BluetoothDevice> getConnectedDevices() {
        synchronized (mLock) {
            ArrayList<BluetoothDevice> result = new ArrayList<>(mHfpDevicesByAddress.values());
            result.addAll(mHearingAidDevicesByAddress.values());
            return Collections.unmodifiableCollection(result);
        }
    }

    // Same as getConnectedDevices except it filters out the hearing aid devices that are linked
    // together by their hiSyncId.
    public Collection<BluetoothDevice> getUniqueConnectedDevices() {
        ArrayList<BluetoothDevice> result;
        synchronized (mLock) {
            result = new ArrayList<>(mHfpDevicesByAddress.values());
        }
        Set<Long> seenHiSyncIds = new LinkedHashSet<>();
        // Add the left-most active device to the seen list so that we match up with the list
        // generated in BluetoothRouteManager.
        if (mBluetoothHearingAidService != null) {
            for (BluetoothDevice device : mBluetoothHearingAidService.getActiveDevices()) {
                if (device != null) {
                    result.add(device);
                    seenHiSyncIds.add(mHearingAidDeviceSyncIds.getOrDefault(device, -1L));
                    break;
                }
            }
        }
        synchronized (mLock) {
            for (BluetoothDevice d : mHearingAidDevicesByAddress.values()) {
                long hiSyncId = mHearingAidDeviceSyncIds.getOrDefault(d, -1L);
                if (seenHiSyncIds.contains(hiSyncId)) {
                    continue;
                }
                result.add(d);
                seenHiSyncIds.add(hiSyncId);
            }
        }
        return Collections.unmodifiableCollection(result);
    }

    public BluetoothHeadsetProxy getHeadsetService() {
        return mBluetoothHeadsetService;
    }

    public BluetoothHearingAid getHearingAidService() {
        return mBluetoothHearingAidService;
    }

    public void setHeadsetServiceForTesting(BluetoothHeadsetProxy bluetoothHeadset) {
        mBluetoothHeadsetService = bluetoothHeadset;
    }

    public void setHearingAidServiceForTesting(BluetoothHearingAid bluetoothHearingAid) {
        mBluetoothHearingAidService = bluetoothHearingAid;
    }

    void onDeviceConnected(BluetoothDevice device, boolean isHearingAid) {
        mLocalLog.log("Device connected -- address: " + device.getAddress() + " isHeadingAid: "
                + isHearingAid);
        synchronized (mLock) {
            LinkedHashMap<String, BluetoothDevice> targetDeviceMap;
            if (isHearingAid) {
                if (mBluetoothHearingAidService == null) {
                    Log.w(this, "Hearing aid service null when receiving device added broadcast");
                    return;
                }
                long hiSyncId = mBluetoothHearingAidService.getHiSyncId(device);
                mHearingAidDeviceSyncIds.put(device, hiSyncId);
                targetDeviceMap = mHearingAidDevicesByAddress;
            } else {
                if (mBluetoothHeadsetService == null) {
                    Log.w(this, "Headset service null when receiving device added broadcast");
                    return;
                }
                targetDeviceMap = mHfpDevicesByAddress;
            }
            if (!targetDeviceMap.containsKey(device.getAddress())) {
                targetDeviceMap.put(device.getAddress(), device);
                mBluetoothRouteManager.onDeviceAdded(device.getAddress());
            }
        }
    }

    void onDeviceDisconnected(BluetoothDevice device, boolean isHearingAid) {
        mLocalLog.log("Device disconnected -- address: " + device.getAddress() + " isHeadingAid: "
                + isHearingAid);
        synchronized (mLock) {
            LinkedHashMap<String, BluetoothDevice> targetDeviceMap;
            if (isHearingAid) {
                mHearingAidDeviceSyncIds.remove(device);
                targetDeviceMap = mHearingAidDevicesByAddress;
            } else {
                targetDeviceMap = mHfpDevicesByAddress;
            }
            if (targetDeviceMap.containsKey(device.getAddress())) {
                targetDeviceMap.remove(device.getAddress());
                mBluetoothRouteManager.onDeviceLost(device.getAddress());
            }
        }
    }

    public void disconnectAudio() {
        if (mBluetoothHearingAidService == null) {
            Log.w(this, "Trying to disconnect audio but no hearing aid service exists");
        } else {
            for (BluetoothDevice device : mBluetoothHearingAidService.getActiveDevices()) {
                if (device != null) {
                    mBluetoothAdapterProxy.setActiveDevice(null,
                        BluetoothAdapter.ACTIVE_DEVICE_ALL);
                }
            }
        }
        disconnectSco();
    }

    public void disconnectSco() {
        if (mBluetoothHeadsetService == null) {
            Log.w(this, "Trying to disconnect audio but no headset service exists.");
        } else {
            mBluetoothHeadsetService.disconnectAudio();
        }
    }

    // Connect audio to the bluetooth device at address, checking to see whether it's a hearing aid
    // or a HFP device, and using the proper BT API.
    public boolean connectAudio(String address) {
        if (mHearingAidDevicesByAddress.containsKey(address)) {
            if (mBluetoothHearingAidService == null) {
                Log.w(this, "Attempting to turn on audio when the hearing aid service is null");
                return false;
            }
            return mBluetoothAdapterProxy.setActiveDevice(
                    mHearingAidDevicesByAddress.get(address),
                    BluetoothAdapter.ACTIVE_DEVICE_ALL);
        } else if (mHfpDevicesByAddress.containsKey(address)) {
            BluetoothDevice device = mHfpDevicesByAddress.get(address);
            if (mBluetoothHeadsetService == null) {
                Log.w(this, "Attempting to turn on audio when the headset service is null");
                return false;
            }
            boolean success = mBluetoothAdapterProxy.setActiveDevice(device,
                BluetoothAdapter.ACTIVE_DEVICE_PHONE_CALL);
            if (!success) {
                Log.w(this, "Couldn't set active device to %s", address);
                return false;
            }
            if (!mBluetoothHeadsetService.isAudioOn()) {
                return mBluetoothHeadsetService.connectAudio();
            }
            return true;
        } else {
            Log.w(this, "Attempting to turn on audio for a disconnected device");
            return false;
        }
    }

    public void cacheHearingAidDevice() {
        if (mBluetoothHearingAidService != null) {
             for (BluetoothDevice device : mBluetoothHearingAidService.getActiveDevices()) {
                 if (device != null) {
                     mBluetoothHearingAidActiveDeviceCache = device;
                 }
             }
        }
    }

    public void restoreHearingAidDevice() {
        if (mBluetoothHearingAidActiveDeviceCache != null && mBluetoothHearingAidService != null) {
            mBluetoothAdapterProxy.setActiveDevice(
                mBluetoothHearingAidActiveDeviceCache,
                BluetoothAdapter.ACTIVE_DEVICE_ALL);
            mBluetoothHearingAidActiveDeviceCache = null;
        }
    }

    public void dump(IndentingPrintWriter pw) {
        mLocalLog.dump(pw);
    }
}
