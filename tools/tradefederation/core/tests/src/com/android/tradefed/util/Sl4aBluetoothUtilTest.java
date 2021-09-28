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

import static com.android.tradefed.util.Sl4aBluetoothUtil.BT_SNOOP_LOG_CMD;
import static com.android.tradefed.util.Sl4aBluetoothUtil.BT_SNOOP_LOG_CMD_LEGACY;

import static junit.framework.TestCase.assertFalse;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothAccessLevel;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothConnectionState;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothProfile;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothPriorityLevel;
import com.android.tradefed.util.Sl4aBluetoothUtil.Commands;
import com.android.tradefed.util.Sl4aBluetoothUtil.Events;
import com.android.tradefed.util.sl4a.Sl4aClient;
import com.android.tradefed.util.sl4a.Sl4aEventDispatcher;
import com.android.tradefed.util.sl4a.Sl4aEventDispatcher.EventSl4aObject;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Unit Test for {@link Sl4aBluetoothUtil} utility */
@RunWith(JUnit4.class)
public class Sl4aBluetoothUtilTest {

    private static final String BT_CONNECTION_EVENT_FORMAT = "{profile: %d, state: %d, addr: %s}";

    private static final Set<BluetoothProfile> PROFILES =
            new HashSet<>(
                    Arrays.asList(
                            BluetoothProfile.A2DP_SINK,
                            BluetoothProfile.MAP_CLIENT,
                            BluetoothProfile.PBAP_CLIENT,
                            BluetoothProfile.HEADSET_CLIENT,
                            BluetoothProfile.PAN));

    @Mock private ITestDevice mPrimary;
    @Mock private ITestDevice mSecondary;
    @Mock private Sl4aClient mPrimaryClient;
    @Mock private Sl4aClient mSecondaryClient;
    @Mock private Sl4aEventDispatcher mEventDispatcher;

    private Sl4aBluetoothUtil mBluetoothUtil = new Sl4aBluetoothUtil();

    @Before
    public void setup() {
        initMocks(this);
        when(mPrimary.getSerialNumber()).thenReturn("serial1");
        when(mSecondary.getSerialNumber()).thenReturn("serial2");
        when(mPrimaryClient.getEventDispatcher()).thenReturn(mEventDispatcher);
        mBluetoothUtil.setSl4a(mPrimary, mPrimaryClient);
        mBluetoothUtil.setSl4a(mSecondary, mSecondaryClient);
    }

    @Test
    public void testEnable_alreadyEnabled() throws IOException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(eq(Commands.BLUETOOTH_CHECK_STATE))).thenReturn(Boolean.TRUE);
        assertTrue(mBluetoothUtil.enable(mPrimary));
    }

    @Test
    public void testEnable_notEnabled()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.FALSE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.TRUE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_ON), anyLong()))
                .thenReturn(createEvent("fake"));
        assertTrue(mBluetoothUtil.enable(mPrimary));
    }

    @Test
    public void testDisable_alreadyDisabled() throws IOException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(eq(Commands.BLUETOOTH_CHECK_STATE))).thenReturn(Boolean.FALSE);
        assertTrue(mBluetoothUtil.disable(mPrimary));
    }

    @Test
    public void testDisable_notDisabled()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.TRUE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.FALSE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_OFF), anyLong()))
                .thenReturn(createEvent("fake"));
        assertTrue(mBluetoothUtil.disable(mPrimary));
    }

    @Test
    public void testGetAddress() throws IOException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS)).thenReturn("address");
        assertEquals("address", mBluetoothUtil.getAddress(mPrimary));
    }

    @Test
    public void testGetAddress_calledTwice() throws IOException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS)).thenReturn("address");
        assertEquals("address", mBluetoothUtil.getAddress(mPrimary));
        assertEquals("address", mBluetoothUtil.getAddress(mPrimary));
        verify(mPrimaryClient, times(1)).rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS);
    }

    @Test
    public void testGetBondedDevices()
            throws JSONException, IOException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_1}, {address: address_2}]"));
        Set<String> addresses = mBluetoothUtil.getBondedDevices(mPrimary);
        assertEquals(2, addresses.size());
        assertTrue(addresses.contains("address_1"));
        assertTrue(addresses.contains("address_2"));
    }

    @Test
    public void testPair_alreadyPaired()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_2}]"));

        assertTrue(mBluetoothUtil.pair(mPrimary, mSecondary));
        verify(mSecondaryClient, never()).rpcCall(Commands.BLUETOOTH_MAKE_DISCOVERABLE);
        verify(mPrimaryClient, never()).rpcCall(Commands.BLUETOOTH_START_PAIRING_HELPER);
        verify(mSecondaryClient, never()).rpcCall(Commands.BLUETOOTH_START_PAIRING_HELPER);
        verify(mPrimaryClient, never()).rpcCall(Commands.BLUETOOTH_DISCOVER_AND_BOND, "address_2");
    }

    @Test
    public void testPair_success() throws IOException, JSONException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_SCAN_MODE)).thenReturn(3);
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[]"))
                .thenReturn(new JSONArray("[]"))
                .thenReturn(new JSONArray("[]"))
                .thenReturn(new JSONArray("[{address: address_2}]"));

        assertTrue(mBluetoothUtil.pair(mPrimary, mSecondary));
        verify(mSecondaryClient).rpcCall(Commands.BLUETOOTH_MAKE_DISCOVERABLE);
        verify(mPrimaryClient).rpcCall(Commands.BLUETOOTH_START_PAIRING_HELPER);
        verify(mSecondaryClient).rpcCall(Commands.BLUETOOTH_START_PAIRING_HELPER);
        verify(mPrimaryClient).rpcCall(Commands.BLUETOOTH_DISCOVER_AND_BOND, "address_2");
    }

    @Test
    public void testPair_timeout() throws IOException, JSONException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_SCAN_MODE)).thenReturn(3);
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[]"));
        mBluetoothUtil.mBtPairTimeoutMs = 1000;
        assertFalse(mBluetoothUtil.pair(mPrimary, mSecondary));
    }

    @Test
    public void testUnpair_success()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_1}, {address: address_2}]"))
                .thenReturn(new JSONArray("[]"));
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_UNBOND, "address_1")).thenReturn(true);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_UNBOND, "address_2")).thenReturn(true);
        assertTrue(mBluetoothUtil.unpairAll(mPrimary));
    }

    @Test
    public void testUnpair_successWithWarning()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_1}, {address: address_2}]"))
                .thenReturn(new JSONArray("[]"));
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_UNBOND, "address_1")).thenReturn(true);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_UNBOND, "address_2")).thenReturn(false);
        assertTrue(mBluetoothUtil.unpairAll(mPrimary));
    }

    @Test
    public void testUnpair_fail() throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_1}, {address: address_2}]"))
                .thenReturn(new JSONArray("[{address: address_1}]"));
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_UNBOND, "address_1")).thenReturn(false);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_UNBOND, "address_2")).thenReturn(true);
        assertFalse(mBluetoothUtil.unpairAll(mPrimary));
    }

    @Test
    public void testConnect_notPaired()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[]"));
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        assertFalse(mBluetoothUtil.connect(mPrimary, mSecondary, PROFILES));
    }

    @Test
    public void testConnect_success()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_GET_BONDED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_2}]"));
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mEventDispatcher.popEvent(
                        eq(Events.BLUETOOTH_PROFILE_CONNECTION_STATE_CHANGED), anyLong()))
                .thenReturn(
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.A2DP_SINK.getProfile(),
                                        BluetoothConnectionState.CONNECTED.getState(),
                                        "address_2")),
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.HEADSET_CLIENT.getProfile(),
                                        BluetoothConnectionState.CONNECTED.getState(),
                                        "address_2")),
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.PBAP_CLIENT.getProfile(),
                                        BluetoothConnectionState.CONNECTED.getState(),
                                        "address_2")),
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.MAP_CLIENT.getProfile(),
                                        BluetoothConnectionState.CONNECTED.getState(),
                                        "address_2")),
                        null);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_PAN_GET_CONNECTED_DEVICES))
                .thenReturn(new JSONArray("[]"))
                .thenReturn(new JSONArray("[]"))
                .thenReturn(new JSONArray("[{address: address_2}]"));

        mBluetoothUtil.mBtConnectionTimeoutMs = 1000;
        assertTrue(mBluetoothUtil.connect(mPrimary, mSecondary, PROFILES));
        verify(mPrimaryClient)
                .rpcCall(Commands.BLUETOOTH_START_CONNECTION_STATE_CHANGE_MONITOR, "address_2");
        verify(mPrimaryClient).rpcCall(Commands.BLUETOOTH_CONNECT_BONDED, "address_2");
    }

    @Test
    public void testDisconnect_success()
            throws IOException, JSONException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mEventDispatcher.popEvent(
                        eq(Events.BLUETOOTH_PROFILE_CONNECTION_STATE_CHANGED), anyLong()))
                .thenReturn(
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.A2DP_SINK.getProfile(),
                                        BluetoothConnectionState.DISCONNECTED.getState(),
                                        "address_2")),
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.HEADSET_CLIENT.getProfile(),
                                        BluetoothConnectionState.DISCONNECTED.getState(),
                                        "address_2")),
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.PBAP_CLIENT.getProfile(),
                                        BluetoothConnectionState.DISCONNECTED.getState(),
                                        "address_2")),
                        createEvent(
                                String.format(
                                        BT_CONNECTION_EVENT_FORMAT,
                                        BluetoothProfile.MAP_CLIENT.getProfile(),
                                        BluetoothConnectionState.DISCONNECTED.getState(),
                                        "address_2")),
                        null);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_PAN_GET_CONNECTED_DEVICES))
                .thenReturn(new JSONArray("[{address: address_2}]"))
                .thenReturn(new JSONArray("[{address: address_2}]"))
                .thenReturn(new JSONArray("[]"));

        mBluetoothUtil.mBtConnectionTimeoutMs = 1000;
        assertTrue(mBluetoothUtil.disconnect(mPrimary, mSecondary, PROFILES));

        verify(mPrimaryClient)
                .rpcCall(Commands.BLUETOOTH_START_CONNECTION_STATE_CHANGE_MONITOR, "address_2");
        verify(mPrimaryClient)
                .rpcCall(
                        eq(Commands.BLUETOOTH_DISCONNECT_CONNECTED_PROFILE),
                        eq("address_2"),
                        any(JSONArray.class));
    }

    @Test
    public void testEnableBluetoothSnoopLog_AndroidQAndAbove()
            throws DeviceNotAvailableException, JSONException, IOException {
        when(mPrimary.getApiLevel()).thenReturn(29);
        // Mock for disable
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.TRUE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.FALSE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_OFF), anyLong()))
                .thenReturn(createEvent("fake"));
        // Mock for enable
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.FALSE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.TRUE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_ON), anyLong()))
                .thenReturn(createEvent("fake"));

        mBluetoothUtil.enableBluetoothSnoopLog(mPrimary);
        verify(mPrimary).executeShellCommand(String.format(BT_SNOOP_LOG_CMD, "full"));
    }

    @Test
    public void testEnableBluetoothSnoopLog_AndroidPAndBelow()
            throws DeviceNotAvailableException, JSONException, IOException {
        when(mPrimary.getApiLevel()).thenReturn(28);
        // Mock for disable
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.TRUE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.FALSE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_OFF), anyLong()))
                .thenReturn(createEvent("fake"));
        // Mock for enable
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.FALSE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.TRUE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_ON), anyLong()))
                .thenReturn(createEvent("fake"));

        mBluetoothUtil.enableBluetoothSnoopLog(mPrimary);
        verify(mPrimary).executeShellCommand(String.format(BT_SNOOP_LOG_CMD_LEGACY, "true"));
    }

    @Test
    public void testDisableBluetoothSnoopLog_AndroidQAndAbove()
            throws DeviceNotAvailableException, IOException, JSONException {
        when(mPrimary.getApiLevel()).thenReturn(29);
        // Mock for disable
        when(mPrimaryClient.rpcCall(eq(Commands.BLUETOOTH_CHECK_STATE))).thenReturn(Boolean.FALSE);
        // Mock for enable
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.FALSE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.TRUE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_ON), anyLong()))
                .thenReturn(createEvent("fake"));

        mBluetoothUtil.disableBluetoothSnoopLog(mPrimary);
        verify(mPrimary).executeShellCommand(String.format(BT_SNOOP_LOG_CMD, "disabled"));
    }

    @Test
    public void testDisableBluetoothSnoopLog_AndroidPAndBelow()
            throws DeviceNotAvailableException, IOException, JSONException {
        when(mPrimary.getApiLevel()).thenReturn(28);
        // Mock for disable
        when(mPrimaryClient.rpcCall(eq(Commands.BLUETOOTH_CHECK_STATE))).thenReturn(Boolean.FALSE);
        // Mock for enable
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_CHECK_STATE)).thenReturn(Boolean.FALSE);
        when(mPrimaryClient.rpcCall(Commands.BLUETOOTH_TOGGLE_STATE, Boolean.TRUE))
                .thenReturn(Boolean.TRUE);
        when(mEventDispatcher.popEvent(eq(Events.BLUETOOTH_STATE_CHANGED_ON), anyLong()))
                .thenReturn(createEvent("fake"));

        mBluetoothUtil.disableBluetoothSnoopLog(mPrimary);
        verify(mPrimary).executeShellCommand(String.format(BT_SNOOP_LOG_CMD_LEGACY, "false"));
    }

    @Test
    public void testChangeProfileAccessPermission()
            throws IOException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");

        assertTrue(
                mBluetoothUtil.changeProfileAccessPermission(
                        mPrimary,
                        mSecondary,
                        BluetoothProfile.PBAP,
                        BluetoothAccessLevel.ACCESS_ALLOWED));
        // Verify that the sl4a command is called correctly
        verify(mPrimaryClient)
                .rpcCall(
                        Commands.BLUETOOTH_CHANGE_PROFILE_ACCESS_PERMISSION,
                        "address_2",
                        BluetoothProfile.PBAP.getProfile(),
                        BluetoothAccessLevel.ACCESS_ALLOWED.getAccess());
    }

    @Test
    public void testChangeProfileAccessPermission_exception()
            throws IOException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mPrimaryClient.rpcCall(anyString(), ArgumentMatchers.<Object>any()))
                .thenThrow(new IOException());

        assertFalse(
                mBluetoothUtil.changeProfileAccessPermission(
                        mPrimary,
                        mSecondary,
                        BluetoothProfile.PBAP,
                        BluetoothAccessLevel.ACCESS_ALLOWED));
    }

    @Test
    public void testSetProfilePriority_emptyProfile()
            throws IOException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");

        assertTrue(
                mBluetoothUtil.setProfilePriority(
                        mPrimary,
                        mSecondary,
                        Collections.emptySet(),
                        BluetoothPriorityLevel.PRIORITY_ON));

        verify(mPrimaryClient, never()).rpcCall(anyString(), ArgumentMatchers.<Object>any());
    }

    @Test
    public void testSetProfilePriority_multipleProfiles()
            throws IOException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");

        Set<BluetoothProfile> supportedProfiles =
                new HashSet<>(
                        Arrays.asList(
                                BluetoothProfile.A2DP_SINK,
                                BluetoothProfile.HEADSET_CLIENT,
                                BluetoothProfile.PBAP_CLIENT));

        assertTrue(
                mBluetoothUtil.setProfilePriority(
                        mPrimary,
                        mSecondary,
                        supportedProfiles,
                        BluetoothPriorityLevel.PRIORITY_ON));

        verify(mPrimaryClient)
                .rpcCall(
                        Commands.BLUETOOTH_A2DP_SINK_SET_PRIORITY,
                        "address_2",
                        BluetoothPriorityLevel.PRIORITY_ON.getPriority());

        verify(mPrimaryClient)
                .rpcCall(
                        Commands.BLUETOOTH_HFP_CLIENT_SET_PRIORITY,
                        "address_2",
                        BluetoothPriorityLevel.PRIORITY_ON.getPriority());

        verify(mPrimaryClient)
                .rpcCall(
                        Commands.BLUETOOTH_PBAP_CLIENT_SET_PRIORITY,
                        "address_2",
                        BluetoothPriorityLevel.PRIORITY_ON.getPriority());
    }

    @Test
    public void testSetProfilePriority_profileNotSupported()
            throws IOException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");

        assertFalse(
                mBluetoothUtil.setProfilePriority(
                        mPrimary,
                        mSecondary,
                        Collections.singleton(BluetoothProfile.PAN),
                        BluetoothPriorityLevel.PRIORITY_ON));

        verify(mPrimaryClient, never()).rpcCall(anyString(), ArgumentMatchers.<Object>any());
    }

    @Test
    public void testSetProfilePriority_exception() throws IOException, DeviceNotAvailableException {
        when(mSecondaryClient.rpcCall(Commands.BLUETOOTH_GET_LOCAL_ADDRESS))
                .thenReturn("address_2");
        when(mPrimaryClient.rpcCall(anyString(), ArgumentMatchers.<Object>any()))
                .thenThrow(new IOException());

        assertFalse(
                mBluetoothUtil.setProfilePriority(
                        mPrimary,
                        mSecondary,
                        Collections.singleton(BluetoothProfile.PBAP_CLIENT),
                        BluetoothPriorityLevel.PRIORITY_ON));
    }

    private EventSl4aObject createEvent(String jsonData) throws JSONException {
        return new EventSl4aObject(
                new JSONObject(String.format("{name: my_event, data: %s, time: 1111}", jsonData)));
    }
}
