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
package com.android.bluetooth.a2dpsink;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAudioConfig;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothA2dpSink;
import android.media.AudioManager;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Provides Bluetooth A2DP Sink profile, as a service in the Bluetooth application.
 * @hide
 */
public class A2dpSinkService extends ProfileService {
    private static final String TAG = "A2dpSinkService";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    static final int MAXIMUM_CONNECTED_DEVICES = 1;

    private final BluetoothAdapter mAdapter;
    protected Map<BluetoothDevice, A2dpSinkStateMachine> mDeviceStateMap =
            new ConcurrentHashMap<>(1);

    private final Object mStreamHandlerLock = new Object();

    private A2dpSinkStreamHandler mA2dpSinkStreamHandler;
    private static A2dpSinkService sService;

    static {
        classInitNative();
    }

    @Override
    protected boolean start() {
        synchronized (mStreamHandlerLock) {
            mA2dpSinkStreamHandler = new A2dpSinkStreamHandler(this, this);
        }
        initNative();
        setA2dpSinkService(this);
        return true;
    }

    @Override
    protected boolean stop() {
        setA2dpSinkService(null);
        cleanupNative();
        for (A2dpSinkStateMachine stateMachine : mDeviceStateMap.values()) {
            stateMachine.quitNow();
        }
        mDeviceStateMap.clear();
        synchronized (mStreamHandlerLock) {
            if (mA2dpSinkStreamHandler != null) {
                mA2dpSinkStreamHandler.cleanup();
                mA2dpSinkStreamHandler = null;
            }
        }
        return true;
    }

    public static synchronized A2dpSinkService getA2dpSinkService() {
        return sService;
    }

    /**
     * Testing API to inject a mockA2dpSinkService.
     * @hide
     */
    @VisibleForTesting
    public static synchronized void setA2dpSinkService(A2dpSinkService service) {
        sService = service;
    }


    public A2dpSinkService() {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    /**
     * Request audio focus such that the designated device can stream audio
     */
    public void requestAudioFocus(BluetoothDevice device, boolean request) {
        synchronized (mStreamHandlerLock) {
            if (mA2dpSinkStreamHandler == null) return;
            mA2dpSinkStreamHandler.requestAudioFocus(request);
        }
    }

    /**
     * Get the current Bluetooth Audio focus state
     *
     * @return AudioManger.AUDIOFOCUS_* states on success, or AudioManager.ERROR on error
     */
    public int getFocusState() {
        synchronized (mStreamHandlerLock) {
            if (mA2dpSinkStreamHandler == null) return AudioManager.ERROR;
            return mA2dpSinkStreamHandler.getFocusState();
        }
    }

    boolean isA2dpPlaying(BluetoothDevice device) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        synchronized (mStreamHandlerLock) {
            if (mA2dpSinkStreamHandler == null) return false;
            return mA2dpSinkStreamHandler.isPlaying();
        }
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new A2dpSinkServiceBinder(this);
    }

    //Binder object: Must be static class or memory leak may occur
    private static class A2dpSinkServiceBinder extends IBluetoothA2dpSink.Stub
            implements IProfileServiceBinder {
        private A2dpSinkService mService;

        private A2dpSinkService getService() {
            if (!Utils.checkCaller()) {
                Log.w(TAG, "A2dp call not allowed for non-active user");
                return null;
            }

            if (mService != null) {
                return mService;
            }
            return null;
        }

        A2dpSinkServiceBinder(A2dpSinkService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public boolean connect(BluetoothDevice device) {
            A2dpSinkService service = getService();
            if (service == null) {
                return false;
            }
            return service.connect(device);
        }

        @Override
        public boolean disconnect(BluetoothDevice device) {
            A2dpSinkService service = getService();
            if (service == null) {
                return false;
            }
            return service.disconnect(device);
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices() {
            A2dpSinkService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getConnectedDevices();
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
            A2dpSinkService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getDevicesMatchingConnectionStates(states);
        }

        @Override
        public int getConnectionState(BluetoothDevice device) {
            A2dpSinkService service = getService();
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
            A2dpSinkService service = getService();
            if (service == null) {
                return false;
            }
            return service.setConnectionPolicy(device, connectionPolicy);
        }

        @Override
        public int getConnectionPolicy(BluetoothDevice device) {
            A2dpSinkService service = getService();
            if (service == null) {
                return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            }
            return service.getConnectionPolicy(device);
        }

        @Override
        public boolean isA2dpPlaying(BluetoothDevice device) {
            A2dpSinkService service = getService();
            if (service == null) {
                return false;
            }
            return service.isA2dpPlaying(device);
        }

        @Override
        public BluetoothAudioConfig getAudioConfig(BluetoothDevice device) {
            A2dpSinkService service = getService();
            if (service == null) {
                return null;
            }
            return service.getAudioConfig(device);
        }
    }

    /* Generic Profile Code */

    /**
     * Connect the given Bluetooth device.
     *
     * @return true if connection is successful, false otherwise.
     */
    public boolean connect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (device == null) {
            throw new IllegalArgumentException("Null device");
        }
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, " connect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.w(TAG, "Connection not allowed: <" + device.getAddress()
                    + "> is CONNECTION_POLICY_FORBIDDEN");
            return false;
        }

        A2dpSinkStateMachine stateMachine = getOrCreateStateMachine(device);
        if (stateMachine != null) {
            stateMachine.connect();
            return true;
        } else {
            // a state machine instance doesn't exist yet, and the max has been reached.
            Log.e(TAG, "Maxed out on the number of allowed A2DP Sink connections. "
                    + "Connect request rejected on " + device);
            return false;
        }
    }

    /**
     * Disconnect the given Bluetooth device.
     *
     * @return true if disconnect is successful, false otherwise.
     */
    public boolean disconnect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_ADMIN_PERM, "Need BLUETOOTH ADMIN permission");
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "A2DP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }

        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        // a state machine instance doesn't exist. maybe it is already gone?
        if (stateMachine == null) {
            return false;
        }
        int connectionState = stateMachine.getState();
        if (connectionState == BluetoothProfile.STATE_DISCONNECTED
                || connectionState == BluetoothProfile.STATE_DISCONNECTING) {
            return false;
        }
        // upon completion of disconnect, the state machine will remove itself from the available
        // devices map
        stateMachine.disconnect();
        return true;
    }

    void removeStateMachine(A2dpSinkStateMachine stateMachine) {
        mDeviceStateMap.remove(stateMachine.getDevice());
    }

    public List<BluetoothDevice> getConnectedDevices() {
        return getDevicesMatchingConnectionStates(new int[]{BluetoothAdapter.STATE_CONNECTED});
    }

    protected A2dpSinkStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        A2dpSinkStateMachine newStateMachine = new A2dpSinkStateMachine(device, this);
        A2dpSinkStateMachine existingStateMachine =
                mDeviceStateMap.putIfAbsent(device, newStateMachine);
        // Given null is not a valid value in our map, ConcurrentHashMap will return null if the
        // key was absent and our new value was added. We should then start and return it.
        if (existingStateMachine == null) {
            newStateMachine.start();
            return newStateMachine;
        }
        return existingStateMachine;
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (DBG) Log.d(TAG, "getDevicesMatchingConnectionStates" + Arrays.toString(states));
        List<BluetoothDevice> deviceList = new ArrayList<>();
        Set<BluetoothDevice> bondedDevices = mAdapter.getBondedDevices();
        int connectionState;
        for (BluetoothDevice device : bondedDevices) {
            connectionState = getConnectionState(device);
            if (DBG) Log.d(TAG, "Device: " + device + "State: " + connectionState);
            for (int i = 0; i < states.length; i++) {
                if (connectionState == states[i]) {
                    deviceList.add(device);
                }
            }
        }
        if (DBG) Log.d(TAG, deviceList.toString());
        Log.d(TAG, "GetDevicesDone");
        return deviceList;
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
        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        return (stateMachine == null) ? BluetoothProfile.STATE_DISCONNECTED
                : stateMachine.getState();
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
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        AdapterService.getAdapterService().getDatabase()
                .setProfileConnectionPolicy(device, BluetoothProfile.A2DP_SINK, connectionPolicy);
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
     * @param device the remote device
     * @return connection policy of the specified device
     */
    public int getConnectionPolicy(BluetoothDevice device) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        return AdapterService.getAdapterService().getDatabase()
                .getProfileConnectionPolicy(device, BluetoothProfile.A2DP_SINK);
    }


    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        ProfileService.println(sb, "Devices Tracked = " + mDeviceStateMap.size());
        for (A2dpSinkStateMachine stateMachine : mDeviceStateMap.values()) {
            ProfileService.println(sb,
                    "==== StateMachine for " + stateMachine.getDevice() + " ====");
            stateMachine.dump(sb);
        }
    }

    BluetoothAudioConfig getAudioConfig(BluetoothDevice device) {
        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        // a state machine instance doesn't exist. maybe it is already gone?
        if (stateMachine == null) {
            return null;
        }
        return stateMachine.getAudioConfig();
    }

    /* JNI interfaces*/

    private static native void classInitNative();

    private native void initNative();

    private native void cleanupNative();

    native boolean connectA2dpNative(byte[] address);

    native boolean disconnectA2dpNative(byte[] address);

    /**
     * set A2DP state machine as the active device
     * the active device is the only one that will receive passthrough commands and the only one
     * that will have its audio decoded
     *
     * @hide
     * @param address
     * @return active device request has been scheduled
     */
    public native boolean setActiveDeviceNative(byte[] address);

    /**
     * inform A2DP decoder of the current audio focus
     *
     * @param focusGranted
     */
    @VisibleForTesting
    public native void informAudioFocusStateNative(int focusGranted);

    /**
     * inform A2DP decoder the desired audio gain
     *
     * @param gain
     */
    @VisibleForTesting
    public native void informAudioTrackGainNative(float gain);

    private void onConnectionStateChanged(byte[] address, int state) {
        StackEvent event = StackEvent.connectionStateChanged(getDevice(address), state);
        A2dpSinkStateMachine stateMachine = getOrCreateStateMachine(event.mDevice);
        stateMachine.sendMessage(A2dpSinkStateMachine.STACK_EVENT, event);
    }

    private void onAudioStateChanged(byte[] address, int state) {
        synchronized (mStreamHandlerLock) {
            if (mA2dpSinkStreamHandler == null) {
                Log.e(TAG, "Received audio state change before we've been started");
                return;
            } else if (state == StackEvent.AUDIO_STATE_STARTED) {
                mA2dpSinkStreamHandler.obtainMessage(
                        A2dpSinkStreamHandler.SRC_STR_START).sendToTarget();
            } else if (state == StackEvent.AUDIO_STATE_STOPPED
                    || state == StackEvent.AUDIO_STATE_REMOTE_SUSPEND) {
                mA2dpSinkStreamHandler.obtainMessage(
                        A2dpSinkStreamHandler.SRC_STR_STOP).sendToTarget();
            }
        }
    }

    private void onAudioConfigChanged(byte[] address, int sampleRate, int channelCount) {
        StackEvent event = StackEvent.audioConfigChanged(getDevice(address), sampleRate,
                channelCount);
        A2dpSinkStateMachine stateMachine = getOrCreateStateMachine(event.mDevice);
        stateMachine.sendMessage(A2dpSinkStateMachine.STACK_EVENT, event);
    }
}
