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

package com.android.car.dialer.ui.warning;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;

import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.R;
import com.android.car.dialer.TestDialerApplication;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.livedata.BluetoothPairListLiveData;
import com.android.car.dialer.livedata.BluetoothStateLiveData;
import com.android.car.dialer.livedata.HfpDeviceListLiveData;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.testutils.ShadowBluetoothAdapterForDialer;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import java.util.Collections;

@RunWith(CarDialerRobolectricTestRunner.class)
@Config(shadows = ShadowBluetoothAdapterForDialer.class)
public class NoHfpViewModelTest {

    private NoHfpViewModel mNoHfpViewModel;
    private Context mContext;
    private HfpDeviceListLiveData mHfpDeviceListLiveData;
    private BluetoothPairListLiveData mPairedListLiveData;
    private BluetoothStateLiveData mBluetoothStateLiveData;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        ((TestDialerApplication) RuntimeEnvironment.application).initUiCallManager();
    }

    @After
    public void tearDown() {
        UiBluetoothMonitor.get().tearDown();
        UiCallManager.get().tearDown();
    }

    @Test
    public void testDialerAppState_defaultBluetoothAdapterIsNull_bluetoothError() {
        ShadowBluetoothAdapterForDialer.setBluetoothAvailable(false);
        initializeViewModel();

        assertThat(mNoHfpViewModel.getBluetoothErrorStringLiveData().getValue()).isEqualTo(
                mContext.getString(R.string.bluetooth_unavailable));
    }

    @Test
    public void testDialerAppState_bluetoothNotEnabled_bluetoothError() {
        ShadowBluetoothAdapterForDialer.setBluetoothAvailable(true);
        ShadowBluetoothAdapterForDialer shadowBluetoothAdapter =
                (ShadowBluetoothAdapterForDialer) shadowOf(BluetoothAdapter.getDefaultAdapter());
        shadowBluetoothAdapter.setEnabled(false);
        initializeViewModel();

        assertThat(mBluetoothStateLiveData.getValue()).isEqualTo(
                BluetoothStateLiveData.BluetoothState.DISABLED);
        assertThat(mNoHfpViewModel.getBluetoothErrorStringLiveData().getValue()).isEqualTo(
                mContext.getString(R.string.bluetooth_disabled));
    }

    @Test
    public void testDialerAppState_noPairedDevices_bluetoothError() {
        ShadowBluetoothAdapterForDialer.setBluetoothAvailable(true);
        ShadowBluetoothAdapterForDialer shadowBluetoothAdapter = Shadow.extract(
                BluetoothAdapter.getDefaultAdapter());
        shadowBluetoothAdapter.setEnabled(true);
        shadowBluetoothAdapter.setBondedDevices(Collections.emptySet());
        initializeViewModel();

        assertThat(mBluetoothStateLiveData.getValue()).isEqualTo(
                BluetoothStateLiveData.BluetoothState.ENABLED);

        assertThat(mPairedListLiveData.getValue().isEmpty()).isTrue();
        assertThat(mNoHfpViewModel.getBluetoothErrorStringLiveData().getValue()).isEqualTo(
                mContext.getString(R.string.bluetooth_unpaired));
    }

    @Test
    public void testDialerAppState_hfpNoConnected_bluetoothError() {
        ShadowBluetoothAdapterForDialer.setBluetoothAvailable(true);
        BluetoothDevice mockBluetoothDevice = mock(BluetoothDevice.class);
        ShadowBluetoothAdapterForDialer shadowBluetoothAdapter = Shadow.extract(
                BluetoothAdapter.getDefaultAdapter());
        shadowBluetoothAdapter.setEnabled(true);
        shadowBluetoothAdapter.setBondedDevices(Collections.singleton(mockBluetoothDevice));
        shadowBluetoothAdapter.setHfpDevices(Collections.emptyList());
        initializeViewModel();

        assertThat(mBluetoothStateLiveData.getValue()).isEqualTo(
                BluetoothStateLiveData.BluetoothState.ENABLED);
        assertThat(mPairedListLiveData.getValue().isEmpty()).isFalse();

        assertThat(mHfpDeviceListLiveData.getValue().isEmpty()).isTrue();
        assertThat(mNoHfpViewModel.getBluetoothErrorStringLiveData().getValue()).isEqualTo(
                mContext.getString(R.string.no_hfp));
    }

    @Test
    public void testDialerAppState_bluetoothAllSet_dialerAppNoError() {
        ShadowBluetoothAdapterForDialer.setBluetoothAvailable(true);
        BluetoothDevice mockBluetoothDevice = mock(BluetoothDevice.class);
        ShadowBluetoothAdapterForDialer shadowBluetoothAdapter = Shadow.extract(
                BluetoothAdapter.getDefaultAdapter());
        // Sets up Bluetooth pair list
        shadowBluetoothAdapter.setBondedDevices(Collections.singleton(mockBluetoothDevice));
        // Sets up Bluetooth state
        shadowBluetoothAdapter.setEnabled(true);
        // Sets up Bluetooth Hfp connected devices
        shadowBluetoothAdapter.setHfpDevices(Collections.singletonList(mockBluetoothDevice));

        initializeViewModel();

        assertThat(mNoHfpViewModel.getBluetoothErrorStringLiveData().getValue()).isEqualTo(
                BluetoothErrorStringLiveData.NO_BT_ERROR);
    }

    private void initializeViewModel() {
        UiBluetoothMonitor.init(mContext);
        mHfpDeviceListLiveData = UiBluetoothMonitor.get().getHfpDeviceListLiveData();
        mPairedListLiveData = UiBluetoothMonitor.get().getPairListLiveData();
        mBluetoothStateLiveData = UiBluetoothMonitor.get().getBluetoothStateLiveData();
        mNoHfpViewModel = new NoHfpViewModel((Application) mContext);
        // Observers needed so that the liveData's internal initialization is triggered
        mNoHfpViewModel.getBluetoothErrorStringLiveData().observeForever(o -> {
        });
    }
}
