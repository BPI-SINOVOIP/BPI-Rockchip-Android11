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

package com.googlecode.android_scripting.facade.bluetooth;

import android.app.Service;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.ParcelUuid;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.EventFacade;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcParameter;

import java.util.List;

public class BluetoothA2dpFacade extends RpcReceiver {
    static final ParcelUuid[] SINK_UUIDS = {
        BluetoothUuid.A2DP_SINK, BluetoothUuid.ADV_AUDIO_DIST,
    };
    private BluetoothCodecConfig mBluetoothCodecConfig;

    private final Service mService;
    private final EventFacade mEventFacade;
    private final BroadcastReceiver mBluetoothA2dpReceiver;
    private final BluetoothAdapter mBluetoothAdapter;

    private static volatile boolean sIsA2dpReady = false;
    private static BluetoothA2dp sA2dpProfile = null;

    public BluetoothA2dpFacade(FacadeManager manager) {
        super(manager);
        mService = manager.getService();
        mEventFacade = manager.getReceiver(EventFacade.class);

        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        mBluetoothA2dpReceiver = new BluetoothA2dpReceiver();
        mBluetoothCodecConfig = new BluetoothCodecConfig(
                BluetoothCodecConfig.SOURCE_CODEC_TYPE_INVALID,
                BluetoothCodecConfig.CODEC_PRIORITY_DEFAULT,
                BluetoothCodecConfig.SAMPLE_RATE_NONE,
                BluetoothCodecConfig.BITS_PER_SAMPLE_NONE,
                BluetoothCodecConfig.CHANNEL_MODE_NONE,
                0L, 0L, 0L, 0L);
        mBluetoothAdapter.getProfileProxy(mService, new A2dpServiceListener(),
                BluetoothProfile.A2DP);

        mService.registerReceiver(mBluetoothA2dpReceiver,
                          new IntentFilter(BluetoothA2dp.ACTION_CODEC_CONFIG_CHANGED));
    }

    class A2dpServiceListener implements BluetoothProfile.ServiceListener {
        @Override
        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            sA2dpProfile = (BluetoothA2dp) proxy;
            sIsA2dpReady = true;
        }

        @Override
        public void onServiceDisconnected(int profile) {
            sIsA2dpReady = false;
        }
    }

    class BluetoothA2dpReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            if (BluetoothA2dp.ACTION_CODEC_CONFIG_CHANGED.equals(action)) {
                BluetoothCodecStatus codecStatus = intent.getParcelableExtra(
                        BluetoothCodecStatus.EXTRA_CODEC_STATUS);
                if (codecStatus.getCodecConfig().equals(mBluetoothCodecConfig)) {
                    mEventFacade.postEvent("BluetoothA2dpCodecConfigChanged", new Bundle());
                }
            }
        }
    }



    /**
     * Connect A2DP Profile to input BluetoothDevice
     *
     * @param device the BluetoothDevice object to connect to
     * @return if the connection was successfull or not
    */
    public Boolean a2dpConnect(BluetoothDevice device) {
        List<BluetoothDevice> sinks = sA2dpProfile.getConnectedDevices();
        if (sinks != null) {
            for (BluetoothDevice sink : sinks) {
                sA2dpProfile.disconnect(sink);
            }
        }
        return sA2dpProfile.connect(device);
    }

    /**
     * Disconnect A2DP Profile from input BluetoothDevice
     *
     * @param device the BluetoothDevice object to disconnect from
     * @return if the disconnection was successfull or not
    */
    public Boolean a2dpDisconnect(BluetoothDevice device) {
        if (sA2dpProfile == null) return false;
        if (sA2dpProfile.getPriority(device) > BluetoothProfile.PRIORITY_ON) {
            sA2dpProfile.setPriority(device, BluetoothProfile.PRIORITY_ON);
        }
        return sA2dpProfile.disconnect(device);
    }

    /**
     * Checks to see if the A2DP profile is ready for use.
     *
     * @return Returns true if the A2DP Profile is ready.
     */
    @Rpc(description = "Is A2dp profile ready.")
    public Boolean bluetoothA2dpIsReady() {
        return sIsA2dpReady;
    }

    /**
     * Set Bluetooth A2DP connection priority
     *
     * @param deviceStr the Bluetooth device's mac address to set the connection priority of
     * @param priority the integer priority to be set
     */
    @Rpc(description = "Set priority of the profile")
    public void bluetoothA2dpSetPriority(
            @RpcParameter(name = "device", description = "Mac address of a BT device.")
            String deviceStr,
            @RpcParameter(name = "priority", description = "Priority that needs to be set.")
            Integer priority)
            throws Exception {
        if (sA2dpProfile == null) return;
        BluetoothDevice device =
                BluetoothFacade.getDevice(mBluetoothAdapter.getBondedDevices(), deviceStr);
        Log.d("Changing priority of device " + device.getAlias() + " p: " + priority);
        sA2dpProfile.setPriority(device, priority);
    }


    /**
     * Connect to remote device using the A2DP profile.
     *
     * @param deviceID the name or mac address of the remote Bluetooth device.
     * @return True if connected successfully.
     * @throws Exception
     */
    @Rpc(description = "Connect to an A2DP device.")
    public Boolean bluetoothA2dpConnect(
            @RpcParameter(name = "deviceID",
                description = "Name or MAC address of a bluetooth device.")
            String deviceID)
            throws Exception {
        if (sA2dpProfile == null) {
            return false;
        }
        BluetoothDevice mDevice =
                BluetoothFacade.getDevice(
                        mBluetoothAdapter.getBondedDevices(), deviceID);
        Log.d("Connecting to device " + mDevice.getAlias());
        return a2dpConnect(mDevice);
    }

    /**
     * Disconnect a remote device using the A2DP profile.
     *
     * @param deviceID the name or mac address of the remote Bluetooth device.
     * @return True if connected successfully.
     * @throws Exception
     */
    @Rpc(description = "Disconnect an A2DP device.")
    public Boolean bluetoothA2dpDisconnect(
            @RpcParameter(name = "deviceID", description = "Name or MAC address of a device.")
            String deviceID)
            throws Exception {
        if (sA2dpProfile == null) {
            return false;
        }
        List<BluetoothDevice> connectedA2dpDevices =
                sA2dpProfile.getConnectedDevices();
        Log.d("Connected a2dp devices " + connectedA2dpDevices);
        BluetoothDevice mDevice = BluetoothFacade.getDevice(
                connectedA2dpDevices, deviceID);
        return a2dpDisconnect(mDevice);
    }

    /**
     * Get the list of devices connected through the A2DP profile.
     *
     * @return List of bluetooth devices that are in one of the following states:
     *   connected, connecting, and disconnecting.
     */
    @Rpc(description = "Get all the devices connected through A2DP.")
    public List<BluetoothDevice> bluetoothA2dpGetConnectedDevices() {
        while (!sIsA2dpReady) {
            continue;
        }
        return sA2dpProfile.getDevicesMatchingConnectionStates(
                new int[] { BluetoothProfile.STATE_CONNECTED,
                    BluetoothProfile.STATE_CONNECTING,
                    BluetoothProfile.STATE_DISCONNECTING});
    }

    private boolean isSelectableCodec(BluetoothCodecConfig target,
            BluetoothCodecConfig capability) {
        return target.getCodecType() == capability.getCodecType()
                && (target.getSampleRate() & capability.getSampleRate()) != 0
                && (target.getBitsPerSample() & capability.getBitsPerSample()) != 0
                && (target.getChannelMode() & capability.getChannelMode()) != 0;
    }

    /**
     * Set active devices with giving codec config
     *
     * @param codecType codec type want to set to, list in BluetoothCodecConfig.
     * @param sampleRate sample rate want to set to, list in BluetoothCodecConfig.
     * @param bitsPerSample bits per sample want to set to, list in BluetoothCodecConfig.
     * @param channelMode channel mode want to set to, list in BluetoothCodecConfig.
     * @return True if set codec config successfully.
     */
    @Rpc(description = "Set A2dp codec config.")
    public boolean bluetoothA2dpSetCodecConfigPreference(
            @RpcParameter(name = "codecType") Integer codecType,
            @RpcParameter(name = "sampleRate") Integer sampleRate,
            @RpcParameter(name = "bitsPerSample") Integer bitsPerSample,
            @RpcParameter(name = "channelMode") Integer channelMode,
            @RpcParameter(name = "codecSpecific1") Long codecSpecific1)
            throws Exception {
        while (!sIsA2dpReady) {
            continue;
        }
        BluetoothCodecConfig codecConfig = new BluetoothCodecConfig(
                codecType,
                BluetoothCodecConfig.CODEC_PRIORITY_HIGHEST,
                sampleRate,
                bitsPerSample,
                channelMode,
                codecSpecific1,
                0L, 0L, 0L);
        BluetoothDevice activeDevice = sA2dpProfile.getActiveDevice();
        if (activeDevice == null) {
            Log.e("No active device");
            throw new Exception("No active device");
        }
        BluetoothCodecStatus currentCodecStatus = sA2dpProfile.getCodecStatus(activeDevice);
        BluetoothCodecConfig currentCodecConfig = currentCodecStatus.getCodecConfig();
        if (isSelectableCodec(codecConfig, currentCodecConfig)
                && codecConfig.getCodecSpecific1() == currentCodecConfig.getCodecSpecific1()) {
            Log.e("Same as current codec configuration " + currentCodecConfig);
            return false;
        }
        for (BluetoothCodecConfig selectable :
                currentCodecStatus.getCodecsSelectableCapabilities()) {
            if (isSelectableCodec(codecConfig, selectable)) {
                mBluetoothCodecConfig = codecConfig;
                sA2dpProfile.setCodecConfigPreference(activeDevice, mBluetoothCodecConfig);
                return true;
            }
        }
        return false;
    }

    /**
     * Get current active device codec config
     *
     * @return Current active device codec config,
     */
    @Rpc(description = "Get current codec config.")
    public BluetoothCodecConfig bluetoothA2dpGetCurrentCodecConfig() throws Exception {
        while (!sIsA2dpReady) {
            continue;
        }
        if (sA2dpProfile.getActiveDevice() == null) {
            Log.e("No active device.");
            throw new Exception("No active device");
        }
        return sA2dpProfile.getCodecStatus(sA2dpProfile.getActiveDevice()).getCodecConfig();
    }

    @Override
    public void shutdown() {
        mService.unregisterReceiver(mBluetoothA2dpReceiver);
    }
}
