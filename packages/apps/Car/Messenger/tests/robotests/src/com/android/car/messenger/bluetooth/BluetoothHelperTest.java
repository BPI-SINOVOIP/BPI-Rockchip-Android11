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

package com.android.car.messenger.bluetooth;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothMapClient;

import com.android.car.messenger.testutils.ShadowBluetoothAdapter;

import com.google.common.collect.Sets;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.Collections;
import java.util.Set;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowBluetoothAdapter.class})
public class BluetoothHelperTest {

    private static final String BLUETOOTH_ADDRESS_ONE = "FA:F8:14:CA:32:39";
    private static final String BLUETOOTH_ADDRESS_TWO = "FA:F8:33:44:32:39";

    @Mock
    private BluetoothMapClient mMockMapClient;
    @Mock
    private BluetoothDevice mMockDeviceOne;
    @Mock
    private BluetoothDevice mMockDeviceTwo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockDeviceOne.getAddress()).thenReturn(BLUETOOTH_ADDRESS_ONE);
        when(mMockDeviceTwo.getAddress()).thenReturn(BLUETOOTH_ADDRESS_TWO);
        when(mMockMapClient.sendMessage(any(), any(), any(), any(), any())).thenReturn(true);

        BluetoothAdapter.getDefaultAdapter().enable();
    }

    @After
    public void tearDown() {
        ShadowBluetoothAdapter.reset();
    }

    @Test
    public void testGetPairedDevices_nullAdapter() {
        ShadowBluetoothAdapter.setBluetoothEnabled(false);

        assertThat(BluetoothHelper.getBondedDevices()).isEmpty();
    }

    @Test
    public void testGetPairedDevices_noBondedDevices() {
        getShadowBluetoothAdapter().setBondedDevices(Collections.emptySet());

        assertThat(BluetoothHelper.getBondedDevices()).isEmpty();
    }

    @Test
    public void testGetPairedDevices_bondedDevices() {
        getShadowBluetoothAdapter().setBondedDevices(
                Sets.newHashSet(mMockDeviceOne, mMockDeviceTwo));

        Set<BluetoothDevice> bondedDevices = BluetoothHelper.getBondedDevices();
        assertThat(bondedDevices).hasSize(2);
        assertThat(bondedDevices).contains(mMockDeviceOne);
        assertThat(bondedDevices).contains(mMockDeviceTwo);
    }

    @Test
    public void testSendMessage_nullAdapter() {
        ShadowBluetoothAdapter.setBluetoothEnabled(false);

        assertThat(BluetoothHelper.sendMessage(mMockMapClient, BLUETOOTH_ADDRESS_ONE, null,
                "test message", null, null)).isFalse();
    }

    @Test
    public void testSendMessage_existingDevice() {
        getShadowBluetoothAdapter().addRemoteDevice(mMockDeviceOne);

        assertThat(BluetoothHelper.sendMessage(mMockMapClient, BLUETOOTH_ADDRESS_ONE, null,
                "test message", null, null)).isTrue();
    }

    @Test
    public void testSendMessage_invalidDeviceAddress() {
        getShadowBluetoothAdapter().addRemoteDevice(mMockDeviceTwo);

        assertThrows(IllegalArgumentException.class,
                () -> BluetoothHelper.sendMessage(mMockMapClient, BLUETOOTH_ADDRESS_ONE, null,
                        "test message", null, null));
    }

    private ShadowBluetoothAdapter getShadowBluetoothAdapter() {
        return (ShadowBluetoothAdapter)
                Shadow.extract(
                        BluetoothAdapter.getDefaultAdapter());
    }
}
