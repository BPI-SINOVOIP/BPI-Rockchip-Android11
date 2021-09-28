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
 * limitations under the License
 */

package com.android.tradefed.util;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.sl4a.Sl4aClient;
import com.android.tradefed.util.sl4a.Sl4aEventDispatcher.EventSl4aObject;

import com.google.common.annotations.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** A utility class provides Bluetooth operations on one or two devices using SL4A */
public class Sl4aBluetoothUtil {

    private static final long BT_STATE_CHANGE_TIMEOUT_MS = 10000;
    private static final long BT_PAIRING_CHECK_INTERVAL_MS = 200;
    private static final long BT_CHECK_CONNECTION_INTERVAL_MS = 100;

    @VisibleForTesting
    static final String BT_SNOOP_LOG_CMD_LEGACY = "setprop persist.bluetooth.btsnoopenable %s";

    @VisibleForTesting
    static final String BT_SNOOP_LOG_CMD = "setprop persist.bluetooth.btsnooplogmode %s";

    /** Holding mappings from device serial number to the {@link Sl4aClient} of the device */
    private Map<String, Sl4aClient> mSl4aClients = new HashMap<>();

    /** Holding mappings from {@link ITestDevice} instance to device MAC address */
    private Map<ITestDevice, String> mAddresses = new HashMap<>();

    @VisibleForTesting long mBtPairTimeoutMs = 25000;

    @VisibleForTesting long mBtConnectionTimeoutMs = 15000;

    /** SL4A RPC commands to be used for Bluetooth operations */
    @VisibleForTesting
    static class Commands {
        static final String BLUETOOTH_CHECK_STATE = "bluetoothCheckState";
        static final String BLUETOOTH_TOGGLE_STATE = "bluetoothToggleState";
        static final String BLUETOOTH_GET_LOCAL_ADDRESS = "bluetoothGetLocalAddress";
        static final String BLUETOOTH_GET_BONDED_DEVICES = "bluetoothGetBondedDevices";
        static final String BLUETOOTH_MAKE_DISCOVERABLE = "bluetoothMakeDiscoverable";
        static final String BLUETOOTH_GET_SCAN_MODE = "bluetoothGetScanMode";
        static final String BLUETOOTH_START_PAIRING_HELPER = "bluetoothStartPairingHelper";
        static final String BLUETOOTH_DISCOVER_AND_BOND = "bluetoothDiscoverAndBond";
        static final String BLUETOOTH_UNBOND = "bluetoothUnbond";
        static final String BLUETOOTH_START_CONNECTION_STATE_CHANGE_MONITOR =
                "bluetoothStartConnectionStateChangeMonitor";
        static final String BLUETOOTH_CONNECT_BONDED = "bluetoothConnectBonded";
        static final String BLUETOOTH_DISCONNECT_CONNECTED_PROFILE =
                "bluetoothDisconnectConnectedProfile";
        static final String BLUETOOTH_HFP_CLIENT_GET_CONNECTED_DEVICES =
                "bluetoothHfpClientGetConnectedDevices";
        static final String BLUETOOTH_A2DP_GET_CONNECTED_DEVICES =
                "bluetoothA2dpGetConnectedDevices";
        static final String BLUETOOTH_A2DP_SINK_GET_CONNECTED_DEVICES =
                "bluetoothA2dpSinkGetConnectedDevices";
        static final String BLUETOOTH_PBAP_CLIENT_GET_CONNECTED_DEVICES =
                "bluetoothPbapClientGetConnectedDevices";
        static final String BLUETOOTH_PAN_GET_CONNECTED_DEVICES = "bluetoothPanGetConnectedDevices";
        static final String BLUETOOTH_MAP_GET_CONNECTED_DEVICES = "bluetoothMapGetConnectedDevices";
        static final String BLUETOOTH_MAP_CLIENT_GET_CONNECTED_DEVICES =
                "bluetoothMapClientGetConnectedDevices";
        static final String BLUETOOTH_CHANGE_PROFILE_ACCESS_PERMISSION =
                "bluetoothChangeProfileAccessPermission";
        static final String BLUETOOTH_A2DP_SINK_SET_PRIORITY = "bluetoothA2dpSinkSetPriority";
        static final String BLUETOOTH_HFP_CLIENT_SET_PRIORITY = "bluetoothHfpClientSetPriority";
        static final String BLUETOOTH_PBAP_CLIENT_SET_PRIORITY = "bluetoothPbapClientSetPriority";
    }

    /** SL4A events to be used for Bluetooth */
    @VisibleForTesting
    static class Events {
        static final String BLUETOOTH_STATE_CHANGED_ON = "BluetoothStateChangedOn";
        static final String BLUETOOTH_STATE_CHANGED_OFF = "BluetoothStateChangedOff";
        static final String BLUETOOTH_PROFILE_CONNECTION_STATE_CHANGED =
                "BluetoothProfileConnectionStateChanged";
    }

    /** Enums for Bluetooth profiles which are based on {@code BluetoothProfile.java} */
    public enum BluetoothProfile {
        HEADSET(1),
        A2DP(2),
        HID_HOST(4),
        PAN(5),
        PBAP(6),
        GATT(7),
        GATT_SERVER(8),
        MAP(9),
        SAP(10),
        A2DP_SINK(11),
        AVRCP_CONTROLLER(12),
        HEADSET_CLIENT(16),
        PBAP_CLIENT(17),
        MAP_CLIENT(18);

        private static final Map<Integer, BluetoothProfile> sProfileToValue =
                Stream.of(values())
                        .collect(Collectors.toMap(BluetoothProfile::getProfile, value -> value));

        private final int mProfile;

        public static BluetoothProfile valueOfProfile(int profile) {
            return sProfileToValue.get(profile);
        }

        BluetoothProfile(int profile) {
            mProfile = profile;
        }

        public int getProfile() {
            return mProfile;
        }
    }

    /** Enums for Bluetooth connection states which are based on {@code BluetoothProfile.java} */
    public enum BluetoothConnectionState {
        DISCONNECTED(0),
        CONNECTING(1),
        CONNECTED(2),
        DISCONNECTING(3);

        private final int mState;

        BluetoothConnectionState(int state) {
            mState = state;
        }

        public int getState() {
            return mState;
        }
    }

    /** Enums for Bluetooth device access level which are based on {@code BluetoothDevice.java} */
    public enum BluetoothAccessLevel {
        ACCESS_UNKNOWN(0),
        ACCESS_ALLOWED(1),
        ACCESS_REJECTED(2);

        private final int mAccess;

        BluetoothAccessLevel(int access) {
            mAccess = access;
        }

        public int getAccess() {
            return mAccess;
        }
    }

    /**
     * Enums for Bluetooth profile priority level which are based on {@code BluetoothProfile.java}
     */
    public enum BluetoothPriorityLevel {
        PRIORITY_AUTO_CONNECT(1000),
        PRIORITY_ON(100),
        PRIORITY_OFF(0),
        PRIORITY_UNDEFINED(-1);

        private final int mPriority;

        BluetoothPriorityLevel(int priority) {
            mPriority = priority;
        }

        public int getPriority() {
            return mPriority;
        }
    }

    /**
     * Explicitly start SL4A client with the given device and SL4A apk file. Normally this method is
     * not required, because SL4A connection will always be established before actual operations.
     *
     * @param device the device to be connected using SL4A
     * @param sl4aApkFile the optional SL4A apk to install and use.
     * @throws DeviceNotAvailableException
     */
    public void startSl4a(ITestDevice device, File sl4aApkFile) throws DeviceNotAvailableException {
        Sl4aClient sl4aClient = Sl4aClient.startSL4A(device, sl4aApkFile);
        mSl4aClients.put(device.getSerialNumber(), sl4aClient);
    }

    /**
     * Stop SL4A clients that already being opened. It basically provide a way to cleanup clients
     * immediately after they are no longer used
     */
    public void stopSl4a() {
        for (Map.Entry<String, Sl4aClient> entry : mSl4aClients.entrySet()) {
            entry.getValue().close();
        }
        mSl4aClients.clear();
    }

    @VisibleForTesting
    void setSl4a(ITestDevice device, Sl4aClient client) {
        mSl4aClients.put(device.getSerialNumber(), client);
    }

    /** Clean up all SL4A connections */
    @Override
    protected void finalize() {
        stopSl4a();
    }

    /**
     * Enable Bluetooth on target device
     *
     * @param device target device
     * @return true if Bluetooth successfully enabled
     * @throws DeviceNotAvailableException
     */
    public boolean enable(ITestDevice device) throws DeviceNotAvailableException {
        return toggleState(device, true);
    }

    /**
     * Disable Bluetooth on target device
     *
     * @param device target device
     * @return true if Bluetooth successfully disabled
     * @throws DeviceNotAvailableException
     */
    public boolean disable(ITestDevice device) throws DeviceNotAvailableException {
        return toggleState(device, false);
    }

    /**
     * Get Bluetooth MAC Address of target device
     *
     * @param device target device
     * @return MAC Address string
     * @throws DeviceNotAvailableException
     */
    public String getAddress(ITestDevice device) throws DeviceNotAvailableException {
        if (mAddresses.containsKey(device)) {
            return mAddresses.get(device);
        }
        Sl4aClient client = getSl4aClient(device);
        String address = null;
        try {
            address = (String) client.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS);
            mAddresses.put(device, address);
        } catch (IOException e) {
            CLog.e(
                    "Failed to get Bluetooth MAC address on device: %s, %s",
                    device.getSerialNumber(), e);
        }
        return address;
    }

    /**
     * Get set of Bluetooth MAC addresses of the bonded (paired) devices on the target device
     *
     * @param device target device
     * @return Set of Bluetooth MAC addresses
     * @throws DeviceNotAvailableException
     */
    public Set<String> getBondedDevices(ITestDevice device) throws DeviceNotAvailableException {
        Set<String> addresses = new HashSet<>();
        Sl4aClient client = getSl4aClient(device);
        try {
            Object response = client.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES);
            if (response != null) {
                JSONArray bondedDevices = (JSONArray) response;
                for (int i = 0; i < bondedDevices.length(); i++) {
                    JSONObject bondedDevice = bondedDevices.getJSONObject(i);
                    if (bondedDevice.has("address")) {
                        addresses.add(bondedDevice.getString("address"));
                    }
                }
            }
        } catch (IOException | JSONException e) {
            CLog.e("Failed to get bonded devices for device: %s, %s", device.getSerialNumber(), e);
        }
        return addresses;
    }

    /**
     * Pair primary device to secondary device
     *
     * @param primary device to pair from
     * @param secondary device to pair to
     * @return true if pairing is successful
     * @throws DeviceNotAvailableException
     */
    public boolean pair(ITestDevice primary, ITestDevice secondary)
            throws DeviceNotAvailableException {
        Sl4aClient primaryClient = getSl4aClient(primary);
        Sl4aClient secondaryClient = getSl4aClient(secondary);
        try {
            if (isPaired(primary, secondary)) {
                CLog.i("The two devices are already paired.");
                return true;
            }
            CLog.d("Make secondary device discoverable");
            secondaryClient.rpcCall(Commands.BLUETOOTH_MAKE_DISCOVERABLE);
            Integer response = (Integer) secondaryClient.rpcCall(Commands.BLUETOOTH_GET_SCAN_MODE);
            if (response != 3) {
                CLog.e("Scan mode is not CONNECTABLE_DISCOVERABLE");
                return false;
            }
            CLog.d("Secondary device is made discoverable");

            CLog.d("Start pairing helper on both devices");
            primaryClient.rpcCall(Commands.BLUETOOTH_START_PAIRING_HELPER);
            secondaryClient.rpcCall(Commands.BLUETOOTH_START_PAIRING_HELPER);

            // Discover and bond (pair) companion device
            CLog.d("Start discover and bond to secondary device: %s", secondary.getSerialNumber());
            primaryClient.getEventDispatcher().clearAllEvents();
            primaryClient.rpcCall(Commands.BLUETOOTH_DISCOVER_AND_BOND, getAddress(secondary));

            if (!waitUntilPaired(primary, secondary)) {
                CLog.e("Bluetooth pairing timeout");
                return false;
            }
        } catch (IOException | InterruptedException e) {
            CLog.e("Error when pair two devices, %s", e);
            return false;
        }
        CLog.i("Secondary device successfully paired");
        return true;
    }

    /**
     * Un-pair all paired devices for current device
     *
     * @param device Current device to perform the action
     * @return true if un-pair successfully
     * @throws DeviceNotAvailableException
     */
    public boolean unpairAll(ITestDevice device) throws DeviceNotAvailableException {
        Set<String> bondedDevices = getBondedDevices(device);
        Sl4aClient client = getSl4aClient(device);
        for (String address : bondedDevices) {
            try {
                Boolean res = (Boolean) client.rpcCall(Commands.BLUETOOTH_UNBOND, address);
                if (!res) {
                    CLog.w(
                            "Failed to unpair device %s. It may not be an actual failure, instead "
                                    + "it may be due to trying to unpair an already unpaired device. This "
                                    + "usually happens when device was first connected using LE transport "
                                    + "where both LE address and classic address are paired and unpaired "
                                    + "at the same time.",
                            address);
                }
            } catch (IOException e) {
                CLog.e("Failed to unpair all Bluetooth devices, %s", e);
            }
        }
        return getBondedDevices(device).isEmpty();
    }

    /**
     * Connect primary device to secondary device on given Bluetooth profiles
     *
     * @param primary device to connect from
     * @param secondary device to connect to
     * @param profiles A set of Bluetooth profiles are required to be connected
     * @return true if connection are successful
     * @throws DeviceNotAvailableException
     */
    public boolean connect(
            ITestDevice primary, ITestDevice secondary, Set<BluetoothProfile> profiles)
            throws DeviceNotAvailableException {
        if (!isPaired(primary, secondary)) {
            CLog.e("Primary device have not yet paired to secondary device");
            return false;
        }
        CLog.d("Connecting to profiles: %s", profiles);
        Sl4aClient primaryClient = getSl4aClient(primary);
        String address = getAddress(secondary);
        try {
            primaryClient.rpcCall(
                    Commands.BLUETOOTH_START_CONNECTION_STATE_CHANGE_MONITOR, address);
            primaryClient.rpcCall(Commands.BLUETOOTH_CONNECT_BONDED, address);
            Set<BluetoothProfile> connectedProfiles =
                    waitForConnectedOrDisconnectedProfiles(
                            primary, address, BluetoothConnectionState.CONNECTED, profiles);
            return waitForRemainingProfilesConnected(primary, address, connectedProfiles, profiles);
        } catch (IOException | InterruptedException | JSONException e) {
            CLog.e("Failed to connect to secondary device, %s", e);
        }
        return false;
    }

    /**
     * Disconnect primary device from secondary device
     *
     * @param primary device to perform disconnect operation
     * @param secondary device to be disconnected
     * @param profiles Given set of Bluetooth profiles required to be disconnected
     * @return true if disconnected successfully
     * @throws DeviceNotAvailableException
     */
    public boolean disconnect(
            ITestDevice primary, ITestDevice secondary, Set<BluetoothProfile> profiles)
            throws DeviceNotAvailableException {
        CLog.d("Disconnecting to profiles: %s", profiles);
        Sl4aClient primaryClient = getSl4aClient(primary);
        String address = getAddress(secondary);
        try {
            primaryClient.rpcCall(
                    Commands.BLUETOOTH_START_CONNECTION_STATE_CHANGE_MONITOR, address);
            primaryClient.rpcCall(
                    Commands.BLUETOOTH_DISCONNECT_CONNECTED_PROFILE,
                    address,
                    new JSONArray(
                            profiles.stream()
                                    .map(profile -> profile.getProfile())
                                    .collect(Collectors.toList())));
            Set<BluetoothProfile> disconnectedProfiles =
                    waitForConnectedOrDisconnectedProfiles(
                            primary, address, BluetoothConnectionState.DISCONNECTED, profiles);
            return waitForRemainingProfilesDisconnected(
                    primary, address, disconnectedProfiles, profiles);
        } catch (IOException | JSONException | InterruptedException e) {
            CLog.e("Failed to disconnect from secondary device, %s", e);
        }
        return false;
    }

    /**
     * Enable Bluetooth snoop log
     *
     * @param device to enable snoop log
     * @return true if enabled successfully
     * @throws DeviceNotAvailableException
     */
    public boolean enableBluetoothSnoopLog(ITestDevice device) throws DeviceNotAvailableException {
        if (isQAndAbove(device)) {
            device.executeShellCommand(String.format(BT_SNOOP_LOG_CMD, "full"));
        } else {
            device.executeShellCommand(String.format(BT_SNOOP_LOG_CMD_LEGACY, "true"));
        }
        return disable(device) && enable(device);
    }

    /**
     * Disable Bluetooth snoop log
     *
     * @param device to disable snoop log
     * @return true if disabled successfully
     * @throws DeviceNotAvailableException
     */
    public boolean disableBluetoothSnoopLog(ITestDevice device) throws DeviceNotAvailableException {
        if (isQAndAbove(device)) {
            device.executeShellCommand(String.format(BT_SNOOP_LOG_CMD, "disabled"));
        } else {
            device.executeShellCommand(String.format(BT_SNOOP_LOG_CMD_LEGACY, "false"));
        }
        return disable(device) && enable(device);
    }

    /**
     * Change Bluetooth profile access permission of secondary device on primary device in order for
     * secondary device to access primary device on the given profile
     *
     * @param primary device to change permission
     * @param secondary device that accesses primary device on the given profile
     * @param profile Bluetooth profile to access
     * @param access level of access, see {@code BluetoothAccessLevel}
     * @return true if permission changed successfully
     * @throws DeviceNotAvailableException
     */
    public boolean changeProfileAccessPermission(
            ITestDevice primary,
            ITestDevice secondary,
            BluetoothProfile profile,
            BluetoothAccessLevel access)
            throws DeviceNotAvailableException {
        Sl4aClient primaryClient = getSl4aClient(primary);
        String secondaryAddress = getAddress(secondary);
        try {
            primaryClient.rpcCall(
                    Commands.BLUETOOTH_CHANGE_PROFILE_ACCESS_PERMISSION,
                    secondaryAddress,
                    profile.mProfile,
                    access.getAccess());
        } catch (IOException e) {
            CLog.e("Failed to set profile access level %s for profile %s, %s", access, profile, e);
            return false;
        }
        return true;
    }

    /**
     * Change priority setting of given profiles on primary device towards secondary device
     *
     * @param primary device to set priority on
     * @param secondary device to set priority for
     * @param profiles Bluetooth profiles to change priority setting
     * @param priority level of priority
     * @return true if set priority successfully
     * @throws DeviceNotAvailableException
     */
    public boolean setProfilePriority(
            ITestDevice primary,
            ITestDevice secondary,
            Set<BluetoothProfile> profiles,
            BluetoothPriorityLevel priority)
            throws DeviceNotAvailableException {
        Sl4aClient primaryClient = getSl4aClient(primary);
        String secondaryAddress = getAddress(secondary);

        for (BluetoothProfile profile : profiles) {
            try {
                switch (profile) {
                    case A2DP_SINK:
                        primaryClient.rpcCall(
                                Commands.BLUETOOTH_A2DP_SINK_SET_PRIORITY,
                                secondaryAddress,
                                priority.getPriority());
                        break;
                    case HEADSET_CLIENT:
                        primaryClient.rpcCall(
                                Commands.BLUETOOTH_HFP_CLIENT_SET_PRIORITY,
                                secondaryAddress,
                                priority.getPriority());
                        break;
                    case PBAP_CLIENT:
                        primaryClient.rpcCall(
                                Commands.BLUETOOTH_PBAP_CLIENT_SET_PRIORITY,
                                secondaryAddress,
                                priority.getPriority());
                        break;
                    default:
                        CLog.e("Profile %s is not yet supported for priority settings", profile);
                        return false;
                }
            } catch (IOException e) {
                CLog.e("Failed to set profile %s with priority %s, %s", profile, priority, e);
                return false;
            }
        }
        return true;
    }

    private boolean toggleState(ITestDevice device, boolean targetState)
            throws DeviceNotAvailableException {
        Sl4aClient client = getSl4aClient(device);
        try {
            boolean currentState = (Boolean) client.rpcCall(Commands.BLUETOOTH_CHECK_STATE);
            if (currentState == targetState) {
                return true;
            }
            client.getEventDispatcher().clearAllEvents();
            Boolean result = (Boolean) client.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, targetState);
            if (!result) {
                CLog.e(
                        "Error in sl4a when toggling %s Bluetooth state.",
                        targetState ? "ON" : "OFF");
                return false;
            }
            String event =
                    targetState
                            ? Events.BLUETOOTH_STATE_CHANGED_ON
                            : Events.BLUETOOTH_STATE_CHANGED_OFF;
            EventSl4aObject response =
                    client.getEventDispatcher().popEvent(event, BT_STATE_CHANGE_TIMEOUT_MS);
            if (response == null) {
                CLog.e(
                        "Get null response after toggling %s Bluetooth state.",
                        targetState ? "ON" : "OFF");
                return false;
            }
        } catch (IOException e) {
            CLog.e("Error when toggling %s Bluetooth state, %s", targetState ? "ON" : "OFF", e);
            return false;
        }
        return true;
    }

    private Sl4aClient getSl4aClient(ITestDevice device) throws DeviceNotAvailableException {
        String serial = device.getSerialNumber();
        if (!mSl4aClients.containsKey(serial)) {
            Sl4aClient client = Sl4aClient.startSL4A(device, null);
            mSl4aClients.put(serial, client);
        }
        return mSl4aClients.get(serial);
    }

    private boolean waitUntilPaired(ITestDevice primary, ITestDevice secondary)
            throws InterruptedException, DeviceNotAvailableException {
        long endTime = System.currentTimeMillis() + mBtPairTimeoutMs;
        while (System.currentTimeMillis() < endTime) {
            if (isPaired(primary, secondary)) {
                return true;
            }
            Thread.sleep(BT_PAIRING_CHECK_INTERVAL_MS);
        }
        return false;
    }

    private boolean isPaired(ITestDevice primary, ITestDevice secondary)
            throws DeviceNotAvailableException {
        return getBondedDevices(primary).contains(getAddress(secondary));
    }

    private Set<BluetoothProfile> waitForConnectedOrDisconnectedProfiles(
            ITestDevice primary,
            String address,
            BluetoothConnectionState targetState,
            Set<BluetoothProfile> allProfiles)
            throws DeviceNotAvailableException, JSONException {

        Set<BluetoothProfile> targetProfiles = new HashSet<>();
        Sl4aClient primaryClient = getSl4aClient(primary);
        // Check connection broadcast receiver
        while (!targetProfiles.containsAll(allProfiles)) {
            EventSl4aObject event =
                    primaryClient
                            .getEventDispatcher()
                            .popEvent(
                                    Events.BLUETOOTH_PROFILE_CONNECTION_STATE_CHANGED,
                                    mBtConnectionTimeoutMs);
            if (event == null) {
                CLog.w("Timeout while waiting for connection state changes for all profiles");
                return targetProfiles;
            }
            JSONObject profileData = new JSONObject(event.getData());
            int profile = profileData.getInt("profile");
            int state = profileData.getInt("state");
            String actualAddress = profileData.getString("addr");
            if (state == targetState.getState() && address.equals(actualAddress)) {
                targetProfiles.add(BluetoothProfile.valueOfProfile(profile));
            }
        }
        return targetProfiles;
    }

    private boolean waitForRemainingProfilesConnected(
            ITestDevice primary,
            String address,
            Set<BluetoothProfile> connectedProfiles,
            Set<BluetoothProfile> allProfiles)
            throws InterruptedException, DeviceNotAvailableException, JSONException, IOException {
        long endTime = System.currentTimeMillis() + mBtConnectionTimeoutMs;
        while (System.currentTimeMillis() < endTime
                && !connectedProfiles.containsAll(allProfiles)) {
            for (BluetoothProfile profile : allProfiles) {
                if (connectedProfiles.contains(profile)) {
                    continue;
                }
                if (isProfileConnected(primary, address, profile)) {
                    connectedProfiles.add(profile);
                }
                CLog.d("Connected profiles for now: %s", connectedProfiles);
            }
            Thread.sleep(BT_CHECK_CONNECTION_INTERVAL_MS);
        }
        return connectedProfiles.containsAll(allProfiles);
    }

    private boolean waitForRemainingProfilesDisconnected(
            ITestDevice primary,
            String address,
            Set<BluetoothProfile> disConnectedProfiles,
            Set<BluetoothProfile> allProfiles)
            throws InterruptedException, DeviceNotAvailableException, JSONException, IOException {
        long endTime = System.currentTimeMillis() + mBtConnectionTimeoutMs;
        while (System.currentTimeMillis() < endTime
                && !disConnectedProfiles.containsAll(allProfiles)) {
            for (BluetoothProfile profile : allProfiles) {
                if (disConnectedProfiles.contains(profile)) {
                    continue;
                }
                if (!isProfileConnected(primary, address, profile)) {
                    disConnectedProfiles.add(profile);
                }
                CLog.d("Disconnected profiles for now: %s", disConnectedProfiles);
            }
            Thread.sleep(BT_CHECK_CONNECTION_INTERVAL_MS);
        }
        return disConnectedProfiles.containsAll(allProfiles);
    }

    private boolean isProfileConnected(
            ITestDevice primary, String address, BluetoothProfile profile)
            throws DeviceNotAvailableException, IOException, JSONException {
        switch (profile) {
            case HEADSET_CLIENT:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_HFP_CLIENT_GET_CONNECTED_DEVICES);
            case A2DP:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_A2DP_GET_CONNECTED_DEVICES);
            case A2DP_SINK:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_A2DP_SINK_GET_CONNECTED_DEVICES);
            case PAN:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_PAN_GET_CONNECTED_DEVICES);
            case PBAP_CLIENT:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_PBAP_CLIENT_GET_CONNECTED_DEVICES);
            case MAP:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_MAP_GET_CONNECTED_DEVICES);
            case MAP_CLIENT:
                return checkConnectedDevice(
                        primary, address, Commands.BLUETOOTH_MAP_CLIENT_GET_CONNECTED_DEVICES);
            default:
                CLog.e("Unsupported profile %s to check connection state", profile);
        }
        return false;
    }

    private boolean checkConnectedDevice(ITestDevice primary, String address, String sl4aCommand)
            throws DeviceNotAvailableException, IOException, JSONException {
        Sl4aClient primaryClient = getSl4aClient(primary);
        JSONArray devices = (JSONArray) primaryClient.rpcCall(sl4aCommand);
        if (devices == null) {
            CLog.e("Empty response");
            return false;
        }
        for (int i = 0; i < devices.length(); i++) {
            JSONObject device = devices.getJSONObject(i);
            if (device.has("address") && device.getString("address").equals(address)) {
                return true;
            }
        }
        return false;
    }

    private boolean isQAndAbove(ITestDevice device) throws DeviceNotAvailableException {
        return device.getApiLevel() > 28;
    }
}
