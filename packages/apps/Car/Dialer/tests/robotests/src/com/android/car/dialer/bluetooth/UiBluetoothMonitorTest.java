/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.dialer.bluetooth;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertNotNull;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;

import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.livedata.BluetoothStateLiveData;
import com.android.car.dialer.testutils.ShadowBluetoothAdapterForDialer;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.Collections;

@RunWith(CarDialerRobolectricTestRunner.class)
@Config(shadows = ShadowBluetoothAdapterForDialer.class)
public class UiBluetoothMonitorTest {

    private Context mContext;
    private UiBluetoothMonitor mUiBluetoothMonitor;
    @Mock
    BluetoothDevice mMockbluetoothDevice;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mContext = RuntimeEnvironment.application;

        ShadowBluetoothAdapterForDialer shadowBluetoothAdapter = Shadow.extract(
                BluetoothAdapter.getDefaultAdapter());
        // Sets up Bluetooth pair list
        shadowBluetoothAdapter.setBondedDevices(Collections.singleton(mMockbluetoothDevice));
        // Sets up Bluetooth state
        shadowBluetoothAdapter.setEnabled(true);
        // Sets up Bluetooth Hfp connected devices
        shadowBluetoothAdapter.setHfpDevices(Collections.singletonList(mMockbluetoothDevice));

        mUiBluetoothMonitor = UiBluetoothMonitor.init(mContext);
    }

    @Test
    public void testInit() {
        assertNotNull(mUiBluetoothMonitor);
    }

    @Test
    public void testGet() {
        assertThat(UiBluetoothMonitor.get()).isEqualTo(mUiBluetoothMonitor);
    }

    @Test
    public void testPairListLiveData() {
        assertNotNull(mUiBluetoothMonitor.getPairListLiveData());
        assertThat(mUiBluetoothMonitor.getPairListLiveData().hasActiveObservers()).isTrue();
        assertThat(mUiBluetoothMonitor.getPairListLiveData().getValue()).containsExactly(
                mMockbluetoothDevice);
    }

    @Test
    public void testBluetoothStateLiveData() {
        assertNotNull(mUiBluetoothMonitor.getBluetoothStateLiveData());
        assertThat(mUiBluetoothMonitor.getBluetoothStateLiveData().hasActiveObservers()).isTrue();
        assertThat(mUiBluetoothMonitor.getBluetoothStateLiveData().getValue()).isEqualTo(
                BluetoothStateLiveData.BluetoothState.ENABLED);
    }

    @Test
    public void testHfpDeviceListLiveData() {
        assertNotNull(mUiBluetoothMonitor.getHfpDeviceListLiveData());
        assertThat(mUiBluetoothMonitor.getHfpDeviceListLiveData().hasActiveObservers()).isTrue();
        assertThat(mUiBluetoothMonitor.getHfpDeviceListLiveData().getValue()).isNotEmpty();
    }

    @After
    public void tearDown() {
        mUiBluetoothMonitor.tearDown();
    }
}
