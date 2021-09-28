/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.bluetooth.hearingaid;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHearingAid;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothHearingAid;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.os.HandlerThread;
import android.os.ParcelUuid;
import android.util.Log;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.ServiceFactory;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.ArrayUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Provides Bluetooth HearingAid profile, as a service in the Bluetooth application.
 * @hide
 */
public class HearingAidService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "HearingAidService";

    // Upper limit of all HearingAid devices: Bonded or Connected
    private static final int MAX_HEARING_AID_STATE_MACHINES = 10;
    private static HearingAidService sHearingAidService;

    private AdapterService mAdapterService;
    private HandlerThread mStateMachinesThread;
    private BluetoothDevice mPreviousAudioDevice;

    @VisibleForTesting
    HearingAidNativeInterface mHearingAidNativeInterface;
    @VisibleForTesting
    AudioManager mAudioManager;

    private final Map<BluetoothDevice, HearingAidStateMachine> mStateMachines =
            new HashMap<>();
    private final Map<BluetoothDevice, Long> mDeviceHiSyncIdMap = new ConcurrentHashMap<>();
    private final Map<BluetoothDevice, Integer> mDeviceCapabilitiesMap = new HashMap<>();
    private final Map<Long, Boolean> mHiSyncIdConnectedMap = new HashMap<>();
    private long mActiveDeviceHiSyncId = BluetoothHearingAid.HI_SYNC_ID_INVALID;

    private BroadcastReceiver mBondStateChangedReceiver;
    private BroadcastReceiver mConnectionStateChangedReceiver;

    private final ServiceFactory mFactory = new ServiceFactory();

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothHearingAidBinder(this);
    }

    @Override
    protected void create() {
        if (DBG) {
            Log.d(TAG, "create()");
        }
    }

    @Override
    protected boolean start() {
        if (DBG) {
            Log.d(TAG, "start()");
        }
        if (sHearingAidService != null) {
            throw new IllegalStateException("start() called twice");
        }

        // Get AdapterService, HearingAidNativeInterface, AudioManager.
        // None of them can be null.
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when HearingAidService starts");
        mHearingAidNativeInterface = Objects.requireNonNull(HearingAidNativeInterface.getInstance(),
                "HearingAidNativeInterface cannot be null when HearingAidService starts");
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        Objects.requireNonNull(mAudioManager,
                "AudioManager cannot be null when HearingAidService starts");

        // Start handler thread for state machines
        mStateMachines.clear();
        mStateMachinesThread = new HandlerThread("HearingAidService.StateMachines");
        mStateMachinesThread.start();

        // Clear HiSyncId map, capabilities map and HiSyncId Connected map
        mDeviceHiSyncIdMap.clear();
        mDeviceCapabilitiesMap.clear();
        mHiSyncIdConnectedMap.clear();

        // Setup broadcast receivers
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        mBondStateChangedReceiver = new BondStateChangedReceiver();
        registerReceiver(mBondStateChangedReceiver, filter);
        filter = new IntentFilter();
        filter.addAction(BluetoothHearingAid.ACTION_CONNECTION_STATE_CHANGED);
        mConnectionStateChangedReceiver = new ConnectionStateChangedReceiver();
        registerReceiver(mConnectionStateChangedReceiver, filter);

        // Mark service as started
        setHearingAidService(this);

        // Initialize native interface
        mHearingAidNativeInterface.init();

        return true;
    }

    @Override
    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }
        if (sHearingAidService == null) {
            Log.w(TAG, "stop() called before start()");
            return true;
        }

        // Cleanup native interface
        mHearingAidNativeInterface.cleanup();
        mHearingAidNativeInterface = null;

        // Mark service as stopped
        setHearingAidService(null);

        // Unregister broadcast receivers
        unregisterReceiver(mBondStateChangedReceiver);
        mBondStateChangedReceiver = null;
        unregisterReceiver(mConnectionStateChangedReceiver);
        mConnectionStateChangedReceiver = null;

        // Destroy state machines and stop handler thread
        synchronized (mStateMachines) {
            for (HearingAidStateMachine sm : mStateMachines.values()) {
                sm.doQuit();
                sm.cleanup();
            }
            mStateMachines.clear();
        }

        // Clear HiSyncId map, capabilities map and HiSyncId Connected map
        mDeviceHiSyncIdMap.clear();
        mDeviceCapabilitiesMap.clear();
        mHiSyncIdConnectedMap.clear();

        if (mStateMachinesThread != null) {
            mStateMachinesThread.quitSafely();
            mStateMachinesThread = null;
        }

        // Clear AdapterService, HearingAidNativeInterface
        mAudioManager = null;
        mHearingAidNativeInterface = null;
        mAdapterService = null;

        return true;
    }

    @Override
    protected void cleanup() {
        if (DBG) {
            Log.d(TAG, "cleanup()");
        }
    }

    /**
     * Get the HearingAidService instance
     * @return HearingAidService instance
     */
    public static synchronized HearingAidService getHearingAidService() {
        if (sHearingAidService == null) {
            Log.w(TAG, "getHearingAidService(): service is NULL");
            return null;
        }

        if (!sHearingAidService.isAvailable()) {
            Log.w(TAG, "getHearingAidService(): service is not available");
            return null;
        }
        return sHearingAidService;
    }

    private static synchronized void setHearingAidService(HearingAidService instance) {
        if (DBG) {
            Log.d(TAG, "setHearingAidService(): set to: " + instance);
        }
        sHearingAidService = instance;
    }

    /**
     * Connects the hearing aid profile to the passed in device
     *
     * @param device is the device with which we will connect the hearing aid profile
     * @return true if hearing aid profile successfully connected, false otherwise
     */
    public boolean connect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "connect(): " + device);
        }
        if (device == null) {
            return false;
        }

        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return false;
        }
        ParcelUuid[] featureUuids = mAdapterService.getRemoteUuids(device);
        if (!ArrayUtils.contains(featureUuids, BluetoothUuid.HEARING_AID)) {
            Log.e(TAG, "Cannot connect to " + device + " : Remote does not have Hearing Aid UUID");
            return false;
        }

        long hiSyncId = mDeviceHiSyncIdMap.getOrDefault(device,
                BluetoothHearingAid.HI_SYNC_ID_INVALID);

        if (hiSyncId != mActiveDeviceHiSyncId
                && hiSyncId != BluetoothHearingAid.HI_SYNC_ID_INVALID
                && mActiveDeviceHiSyncId != BluetoothHearingAid.HI_SYNC_ID_INVALID) {
            for (BluetoothDevice connectedDevice : getConnectedDevices()) {
                disconnect(connectedDevice);
            }
        }

        synchronized (mStateMachines) {
            HearingAidStateMachine smConnect = getOrCreateStateMachine(device);
            if (smConnect == null) {
                Log.e(TAG, "Cannot connect to " + device + " : no state machine");
            }
            smConnect.sendMessage(HearingAidStateMachine.CONNECT);
        }

        for (BluetoothDevice storedDevice : mDeviceHiSyncIdMap.keySet()) {
            if (device.equals(storedDevice)) {
                continue;
            }
            if (mDeviceHiSyncIdMap.getOrDefault(storedDevice,
                    BluetoothHearingAid.HI_SYNC_ID_INVALID) == hiSyncId) {
                synchronized (mStateMachines) {
                    HearingAidStateMachine sm = getOrCreateStateMachine(storedDevice);
                    if (sm == null) {
                        Log.e(TAG, "Ignored connect request for " + device + " : no state machine");
                        continue;
                    }
                    sm.sendMessage(HearingAidStateMachine.CONNECT);
                }
                if (hiSyncId == BluetoothHearingAid.HI_SYNC_ID_INVALID
                        && !device.equals(storedDevice)) {
                    break;
                }
            }
        }
        return true;
    }

    /**
     * Disconnects hearing aid profile for the passed in device
     *
     * @param device is the device with which we want to disconnected the hearing aid profile
     * @return true if hearing aid profile successfully disconnected, false otherwise
     */
    public boolean disconnect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "disconnect(): " + device);
        }
        if (device == null) {
            return false;
        }
        long hiSyncId = mDeviceHiSyncIdMap.getOrDefault(device,
                BluetoothHearingAid.HI_SYNC_ID_INVALID);

        for (BluetoothDevice storedDevice : mDeviceHiSyncIdMap.keySet()) {
            if (mDeviceHiSyncIdMap.getOrDefault(storedDevice,
                    BluetoothHearingAid.HI_SYNC_ID_INVALID) == hiSyncId) {
                synchronized (mStateMachines) {
                    HearingAidStateMachine sm = mStateMachines.get(storedDevice);
                    if (sm == null) {
                        Log.e(TAG, "Ignored disconnect request for " + device
                                + " : no state machine");
                        continue;
                    }
                    sm.sendMessage(HearingAidStateMachine.DISCONNECT);
                }
                if (hiSyncId == BluetoothHearingAid.HI_SYNC_ID_INVALID
                        && !device.equals(storedDevice)) {
                    break;
                }
            }
        }
        return true;
    }

    List<BluetoothDevice> getConnectedDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            List<BluetoothDevice> devices = new ArrayList<>();
            for (HearingAidStateMachine sm : mStateMachines.values()) {
                if (sm.isConnected()) {
                    devices.add(sm.getDevice());
                }
            }
            return devices;
        }
    }

    /**
     * Check any peer device is connected.
     * The check considers any peer device is connected.
     *
     * @param device the peer device to connect to
     * @return true if there are any peer device connected.
     */
    public boolean isConnectedPeerDevices(BluetoothDevice device) {
        long hiSyncId = getHiSyncId(device);
        if (getConnectedPeerDevices(hiSyncId).isEmpty()) {
            return false;
        }
        return true;
    }

    /**
     * Check whether can connect to a peer device.
     * The check considers a number of factors during the evaluation.
     *
     * @param device the peer device to connect to
     * @return true if connection is allowed, otherwise false
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean okToConnect(BluetoothDevice device) {
        // Check if this is an incoming connection in Quiet mode.
        if (mAdapterService.isQuietModeEnabled()) {
            Log.e(TAG, "okToConnect: cannot connect to " + device + " : quiet mode enabled");
            return false;
        }
        // Check connection policy and accept or reject the connection.
        int connectionPolicy = getConnectionPolicy(device);
        int bondState = mAdapterService.getBondState(device);
        // Allow this connection only if the device is bonded. Any attempt to connect while
        // bonding would potentially lead to an unauthorized connection.
        if (bondState != BluetoothDevice.BOND_BONDED) {
            Log.w(TAG, "okToConnect: return false, bondState=" + bondState);
            return false;
        } else if (connectionPolicy != BluetoothProfile.CONNECTION_POLICY_UNKNOWN
                && connectionPolicy != BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            // Otherwise, reject the connection if connectionPolicy is not valid.
            Log.w(TAG, "okToConnect: return false, connectionPolicy=" + connectionPolicy);
            return false;
        }
        return true;
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            return devices;
        }
        final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
        if (bondedDevices == null) {
            return devices;
        }
        synchronized (mStateMachines) {
            for (BluetoothDevice device : bondedDevices) {
                final ParcelUuid[] featureUuids = device.getUuids();
                if (!ArrayUtils.contains(featureUuids, BluetoothUuid.HEARING_AID)) {
                    continue;
                }
                int connectionState = BluetoothProfile.STATE_DISCONNECTED;
                HearingAidStateMachine sm = mStateMachines.get(device);
                if (sm != null) {
                    connectionState = sm.getConnectionState();
                }
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        break;
                    }
                }
            }
            return devices;
        }
    }

    /**
     * Get the list of devices that have state machines.
     *
     * @return the list of devices that have state machines
     */
    @VisibleForTesting
    List<BluetoothDevice> getDevices() {
        List<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            for (HearingAidStateMachine sm : mStateMachines.values()) {
                devices.add(sm.getDevice());
            }
            return devices;
        }
    }

    /**
     * Get the HiSyncIdMap for testing
     *
     * @return mDeviceHiSyncIdMap
     */
    @VisibleForTesting
    Map<BluetoothDevice, Long> getHiSyncIdMap() {
        return mDeviceHiSyncIdMap;
    }

    /**
     * Get the current connection state of the profile
     *
     * @param device is the remote bluetooth device
     * @return {@link BluetoothProfile#STATE_DISCONNECTED} if this profile is disconnected,
     * {@link BluetoothProfile#STATE_CONNECTING} if this profile is being connected,
     * {@link BluetoothProfile#STATE_CONNECTED} if this profile is connected, or
     * {@link BluetoothProfile#STATE_DISCONNECTING} if this profile is being disconnected
     */
    public int getConnectionState(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        synchronized (mStateMachines) {
            HearingAidStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return sm.getConnectionState();
        }
    }

    /**
     * Set connection policy of the profile and connects it if connectionPolicy is
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED} or disconnects if connectionPolicy is
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN}
     *
     * <p> The device should already be paired.
     * Connection policy can be one of:
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED},
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile#CONNECTION_POLICY_UNKNOWN}
     *
     * @param device Paired bluetooth device
     * @param connectionPolicy is the connection policy to set to for this profile
     * @return true if connectionPolicy is set, false on error
     */
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        mAdapterService.getDatabase()
                .setProfileConnectionPolicy(device, BluetoothProfile.HEARING_AID, connectionPolicy);
        if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            connect(device);
        } else if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            disconnect(device);
        }
        return true;
    }

    /**
     * Get the connection policy of the profile.
     *
     * <p> The connection policy can be any of:
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED},
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile#CONNECTION_POLICY_UNKNOWN}
     *
     * @param device Bluetooth device
     * @return connection policy of the device
     * @hide
     */
    public int getConnectionPolicy(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        return mAdapterService.getDatabase()
                .getProfileConnectionPolicy(device, BluetoothProfile.HEARING_AID);
    }

    void setVolume(int volume) {
        mHearingAidNativeInterface.setVolume(volume);
    }

    long getHiSyncId(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (device == null) {
            return BluetoothHearingAid.HI_SYNC_ID_INVALID;
        }
        return mDeviceHiSyncIdMap.getOrDefault(device, BluetoothHearingAid.HI_SYNC_ID_INVALID);
    }

    int getCapabilities(BluetoothDevice device) {
        return mDeviceCapabilitiesMap.getOrDefault(device, -1);
    }

    /**
     * Set the active device.
     * @param device the new active device
     * @return true on success, otherwise false
     */
    public boolean setActiveDevice(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        if (DBG) {
            Log.d(TAG, "setActiveDevice:" + device);
        }
        synchronized (mStateMachines) {
            if (device == null) {
                if (mActiveDeviceHiSyncId != BluetoothHearingAid.HI_SYNC_ID_INVALID) {
                    reportActiveDevice(null);
                    mActiveDeviceHiSyncId = BluetoothHearingAid.HI_SYNC_ID_INVALID;
                }
                return true;
            }
            if (getConnectionState(device) != BluetoothProfile.STATE_CONNECTED) {
                Log.e(TAG, "setActiveDevice(" + device + "): failed because device not connected");
                return false;
            }
            Long deviceHiSyncId = mDeviceHiSyncIdMap.getOrDefault(device,
                    BluetoothHearingAid.HI_SYNC_ID_INVALID);
            if (deviceHiSyncId != mActiveDeviceHiSyncId) {
                mActiveDeviceHiSyncId = deviceHiSyncId;
                reportActiveDevice(device);
            }
        }
        return true;
    }

    /**
     * Get the connected physical Hearing Aid devices that are active
     *
     * @return the list of active devices. The first element is the left active
     * device; the second element is the right active device. If either or both side
     * is not active, it will be null on that position
     */
    public List<BluetoothDevice> getActiveDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_PERM, "Need BLUETOOTH permission");
        if (DBG) {
            Log.d(TAG, "getActiveDevices");
        }
        ArrayList<BluetoothDevice> activeDevices = new ArrayList<>();
        activeDevices.add(null);
        activeDevices.add(null);
        synchronized (mStateMachines) {
            if (mActiveDeviceHiSyncId == BluetoothHearingAid.HI_SYNC_ID_INVALID) {
                return activeDevices;
            }
            for (BluetoothDevice device : mDeviceHiSyncIdMap.keySet()) {
                if (getConnectionState(device) != BluetoothProfile.STATE_CONNECTED) {
                    continue;
                }
                if (mDeviceHiSyncIdMap.get(device) == mActiveDeviceHiSyncId) {
                    int deviceSide = getCapabilities(device) & 1;
                    if (deviceSide == BluetoothHearingAid.SIDE_RIGHT) {
                        activeDevices.set(1, device);
                    } else {
                        activeDevices.set(0, device);
                    }
                }
            }
        }
        return activeDevices;
    }

    void messageFromNative(HearingAidStackEvent stackEvent) {
        Objects.requireNonNull(stackEvent.device,
                "Device should never be null, event: " + stackEvent);

        if (stackEvent.type == HearingAidStackEvent.EVENT_TYPE_DEVICE_AVAILABLE) {
            BluetoothDevice device = stackEvent.device;
            int capabilities = stackEvent.valueInt1;
            long hiSyncId = stackEvent.valueLong2;
            if (DBG) {
                Log.d(TAG, "Device available: device=" + device + " capabilities="
                        + capabilities + " hiSyncId=" + hiSyncId);
            }
            mDeviceCapabilitiesMap.put(device, capabilities);
            mDeviceHiSyncIdMap.put(device, hiSyncId);
            return;
        }

        synchronized (mStateMachines) {
            BluetoothDevice device = stackEvent.device;
            HearingAidStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                if (stackEvent.type == HearingAidStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
                    switch (stackEvent.valueInt1) {
                        case HearingAidStackEvent.CONNECTION_STATE_CONNECTED:
                        case HearingAidStackEvent.CONNECTION_STATE_CONNECTING:
                            sm = getOrCreateStateMachine(device);
                            break;
                        default:
                            break;
                    }
                }
            }
            if (sm == null) {
                Log.e(TAG, "Cannot process stack event: no state machine: " + stackEvent);
                return;
            }
            sm.sendMessage(HearingAidStateMachine.STACK_EVENT, stackEvent);
        }
    }

    private HearingAidStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "getOrCreateStateMachine failed: device cannot be null");
            return null;
        }
        synchronized (mStateMachines) {
            HearingAidStateMachine sm = mStateMachines.get(device);
            if (sm != null) {
                return sm;
            }
            // Limit the maximum number of state machines to avoid DoS attack
            if (mStateMachines.size() >= MAX_HEARING_AID_STATE_MACHINES) {
                Log.e(TAG, "Maximum number of HearingAid state machines reached: "
                        + MAX_HEARING_AID_STATE_MACHINES);
                return null;
            }
            if (DBG) {
                Log.d(TAG, "Creating a new state machine for " + device);
            }
            sm = HearingAidStateMachine.make(device, this,
                    mHearingAidNativeInterface, mStateMachinesThread.getLooper());
            mStateMachines.put(device, sm);
            return sm;
        }
    }

    /**
     * Report the active device change to the active device manager and the media framework.
     * @param device the new active device; or null if no active device
     */
    private void reportActiveDevice(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "reportActiveDevice(" + device + ")");
        }

        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_ACTIVE_DEVICE_CHANGED,
                BluetoothProfile.HEARING_AID, mAdapterService.obfuscateAddress(device),
                mAdapterService.getMetricId(device));

        Intent intent = new Intent(BluetoothHearingAid.ACTION_ACTIVE_DEVICE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        sendBroadcast(intent, ProfileService.BLUETOOTH_PERM);

        if (device == null) {
            if (DBG) {
                Log.d(TAG, "Set Hearing Aid audio to disconnected");
            }
            boolean suppressNoisyIntent =
                    (getConnectionState(mPreviousAudioDevice) == BluetoothProfile.STATE_CONNECTED);
            mAudioManager.setBluetoothHearingAidDeviceConnectionState(
                    mPreviousAudioDevice, BluetoothProfile.STATE_DISCONNECTED,
                    suppressNoisyIntent, 0);
            mPreviousAudioDevice = null;
        } else {
            if (DBG) {
                Log.d(TAG, "Set Hearing Aid audio to connected");
            }
            if (mPreviousAudioDevice != null) {
                mAudioManager.setBluetoothHearingAidDeviceConnectionState(
                        mPreviousAudioDevice, BluetoothProfile.STATE_DISCONNECTED,
                        true, 0);
            }
            mAudioManager.setBluetoothHearingAidDeviceConnectionState(
                    device, BluetoothProfile.STATE_CONNECTED,
                    true, 0);
            mPreviousAudioDevice = device;
        }
    }

    // Remove state machine if the bonding for a device is removed
    private class BondStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) {
                return;
            }
            int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                                           BluetoothDevice.ERROR);
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            Objects.requireNonNull(device, "ACTION_BOND_STATE_CHANGED with no EXTRA_DEVICE");
            bondStateChanged(device, state);
        }
    }

    /**
     * Process a change in the bonding state for a device.
     *
     * @param device the device whose bonding state has changed
     * @param bondState the new bond state for the device. Possible values are:
     * {@link BluetoothDevice#BOND_NONE},
     * {@link BluetoothDevice#BOND_BONDING},
     * {@link BluetoothDevice#BOND_BONDED}.
     */
    @VisibleForTesting
    void bondStateChanged(BluetoothDevice device, int bondState) {
        if (DBG) {
            Log.d(TAG, "Bond state changed for device: " + device + " state: " + bondState);
        }
        // Remove state machine if the bonding for a device is removed
        if (bondState != BluetoothDevice.BOND_NONE) {
            return;
        }
        mDeviceHiSyncIdMap.remove(device);
        synchronized (mStateMachines) {
            HearingAidStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return;
            }
            if (sm.getConnectionState() != BluetoothProfile.STATE_DISCONNECTED) {
                return;
            }
            removeStateMachine(device);
        }
    }

    private void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            HearingAidStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                Log.w(TAG, "removeStateMachine: device " + device
                        + " does not have a state machine");
                return;
            }
            Log.i(TAG, "removeStateMachine: removing state machine for device: " + device);
            sm.doQuit();
            sm.cleanup();
            mStateMachines.remove(device);
        }
    }

    private List<BluetoothDevice> getConnectedPeerDevices(long hiSyncId) {
        List<BluetoothDevice> result = new ArrayList<>();
        for (BluetoothDevice peerDevice : getConnectedDevices()) {
            if (getHiSyncId(peerDevice) == hiSyncId) {
                result.add(peerDevice);
            }
        }
        return result;
    }

    @VisibleForTesting
    synchronized void connectionStateChanged(BluetoothDevice device, int fromState,
                                                     int toState) {
        if ((device == null) || (fromState == toState)) {
            Log.e(TAG, "connectionStateChanged: unexpected invocation. device=" + device
                    + " fromState=" + fromState + " toState=" + toState);
            return;
        }
        if (toState == BluetoothProfile.STATE_CONNECTED) {
            long myHiSyncId = getHiSyncId(device);
            if (myHiSyncId == BluetoothHearingAid.HI_SYNC_ID_INVALID
                    || getConnectedPeerDevices(myHiSyncId).size() == 1) {
                // Log hearing aid connection event if we are the first device in a set
                // Or when the hiSyncId has not been found
                MetricsLogger.logProfileConnectionEvent(
                        BluetoothMetricsProto.ProfileId.HEARING_AID);
            }
            if (!mHiSyncIdConnectedMap.getOrDefault(myHiSyncId, false)) {
                setActiveDevice(device);
                mHiSyncIdConnectedMap.put(myHiSyncId, true);
            }
        }
        if (fromState == BluetoothProfile.STATE_CONNECTED && getConnectedDevices().isEmpty()) {
            setActiveDevice(null);
            long myHiSyncId = getHiSyncId(device);
            mHiSyncIdConnectedMap.put(myHiSyncId, false);
        }
        // Check if the device is disconnected - if unbond, remove the state machine
        if (toState == BluetoothProfile.STATE_DISCONNECTED) {
            int bondState = mAdapterService.getBondState(device);
            if (bondState == BluetoothDevice.BOND_NONE) {
                if (DBG) {
                    Log.d(TAG, device + " is unbond. Remove state machine");
                }
                removeStateMachine(device);
            }
        }
    }

    private class ConnectionStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothHearingAid.ACTION_CONNECTION_STATE_CHANGED.equals(intent.getAction())) {
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            int toState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
            int fromState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
            connectionStateChanged(device, fromState, toState);
        }
    }

    /**
     * Binder object: must be a static class or memory leak may occur
     */
    @VisibleForTesting
    static class BluetoothHearingAidBinder extends IBluetoothHearingAid.Stub
            implements IProfileServiceBinder {
        private HearingAidService mService;

        private HearingAidService getService() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "HearingAid call not allowed for non-active user");
                return null;
            }

            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            return null;
        }

        BluetoothHearingAidBinder(HearingAidService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public boolean connect(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return false;
            }
            return service.connect(device);
        }

        @Override
        public boolean disconnect(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return false;
            }
            return service.disconnect(device);
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices() {
            HearingAidService service = getService();
            if (service == null) {
                return new ArrayList<>();
            }
            return service.getConnectedDevices();
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
            HearingAidService service = getService();
            if (service == null) {
                return new ArrayList<>();
            }
            return service.getDevicesMatchingConnectionStates(states);
        }

        @Override
        public int getConnectionState(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public boolean setActiveDevice(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return false;
            }
            return service.setActiveDevice(device);
        }

        @Override
        public List<BluetoothDevice> getActiveDevices() {
            HearingAidService service = getService();
            if (service == null) {
                return new ArrayList<>();
            }
            return service.getActiveDevices();
        }

        @Override
        public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
            HearingAidService service = getService();
            if (service == null) {
                return false;
            }
            return service.setConnectionPolicy(device, connectionPolicy);
        }

        @Override
        public int getConnectionPolicy(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            }
            return service.getConnectionPolicy(device);
        }

        @Override
        public void setVolume(int volume) {
            HearingAidService service = getService();
            if (service == null) {
                return;
            }
            service.setVolume(volume);
        }

        @Override
        public long getHiSyncId(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return BluetoothHearingAid.HI_SYNC_ID_INVALID;
            }
            return service.getHiSyncId(device);
        }

        @Override
        public int getDeviceSide(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return BluetoothHearingAid.SIDE_RIGHT;
            }
            return service.getCapabilities(device) & 1;
        }

        @Override
        public int getDeviceMode(BluetoothDevice device) {
            HearingAidService service = getService();
            if (service == null) {
                return BluetoothHearingAid.MODE_BINAURAL;
            }
            return service.getCapabilities(device) >> 1 & 1;
        }
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        for (HearingAidStateMachine sm : mStateMachines.values()) {
            sm.dump(sb);
        }
    }
}
