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

package com.android.car.settings.bluetooth;

import static android.content.pm.PackageManager.FEATURE_BLUETOOTH;
import static android.os.UserManager.DISALLOW_CONFIG_BLUETOOTH;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.lifecycle.Lifecycle;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowBluetoothAdapter;
import com.android.car.settings.testutils.ShadowBluetoothPan;
import com.android.settingslib.bluetooth.CachedBluetoothDevice;
import com.android.settingslib.bluetooth.CachedBluetoothDeviceManager;
import com.android.settingslib.bluetooth.LocalBluetoothManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowUserManager;
import org.robolectric.util.ReflectionHelpers;

import java.util.Arrays;

/** Unit test for {@link BluetoothBondedDevicesPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowBluetoothAdapter.class, ShadowBluetoothPan.class})
public class BluetoothBondedDevicesPreferenceControllerTest {

    @Mock
    private CachedBluetoothDevice mBondedCachedDevice;
    @Mock
    private BluetoothDevice mBondedDevice;
    @Mock
    private CachedBluetoothDeviceManager mCachedDeviceManager;
    private CachedBluetoothDeviceManager mSaveRealCachedDeviceManager;
    private LocalBluetoothManager mLocalBluetoothManager;
    private PreferenceGroup mPreferenceGroup;
    private PreferenceControllerTestHelper<BluetoothBondedDevicesPreferenceController>
            mControllerHelper;
    private BluetoothBondedDevicesPreferenceController mController;
    private Context mContext;
    private UserHandle mMyUserHandle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mMyUserHandle = UserHandle.of(UserHandle.myUserId());

        mLocalBluetoothManager = LocalBluetoothManager.getInstance(mContext, /* onInitCallback= */
                null);
        mSaveRealCachedDeviceManager = mLocalBluetoothManager.getCachedDeviceManager();
        ReflectionHelpers.setField(mLocalBluetoothManager, "mCachedDeviceManager",
                mCachedDeviceManager);

        when(mBondedDevice.getBondState()).thenReturn(BluetoothDevice.BOND_BONDED);
        when(mBondedCachedDevice.getDevice()).thenReturn(mBondedDevice);
        BluetoothDevice unbondedDevice = mock(BluetoothDevice.class);
        when(unbondedDevice.getBondState()).thenReturn(BluetoothDevice.BOND_NONE);
        CachedBluetoothDevice unbondedCachedDevice = mock(CachedBluetoothDevice.class);
        when(unbondedCachedDevice.getDevice()).thenReturn(unbondedDevice);

        when(mCachedDeviceManager.getCachedDevicesCopy()).thenReturn(
                Arrays.asList(mBondedCachedDevice, unbondedCachedDevice));

        // Make sure controller is available.
        Shadows.shadowOf(mContext.getPackageManager()).setSystemFeature(
                FEATURE_BLUETOOTH, /* supported= */ true);
        BluetoothAdapter.getDefaultAdapter().enable();
        getShadowBluetoothAdapter().setState(BluetoothAdapter.STATE_ON);

        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                BluetoothBondedDevicesPreferenceController.class, mPreferenceGroup);
        mController = mControllerHelper.getController();
    }

    @After
    public void tearDown() {
        ShadowBluetoothAdapter.reset();
        ReflectionHelpers.setField(mLocalBluetoothManager, "mCachedDeviceManager",
                mSaveRealCachedDeviceManager);
    }

    @Test
    public void showsOnlyBondedDevices() {
        mControllerHelper.markState(Lifecycle.State.STARTED);
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(1);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);
        assertThat(devicePreference.getCachedDevice()).isEqualTo(mBondedCachedDevice);
    }

    @Test
    public void onDeviceBondStateChanged_refreshesUi() {
        mControllerHelper.markState(Lifecycle.State.STARTED);
        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(1);

        // Unbond the only bonded device.
        when(mBondedCachedDevice.getBondState()).thenReturn(BluetoothDevice.BOND_NONE);
        when(mBondedDevice.getBondState()).thenReturn(BluetoothDevice.BOND_NONE);
        mController.onDeviceBondStateChanged(mBondedCachedDevice, BluetoothDevice.BOND_NONE);

        assertThat(mPreferenceGroup.getPreferenceCount()).isEqualTo(0);
    }

    @Test
    public void onDeviceClicked_notConnected_connectsToDevice() {
        when(mBondedCachedDevice.isConnected()).thenReturn(false);
        mControllerHelper.markState(Lifecycle.State.STARTED);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);

        devicePreference.performClick();

        verify(mBondedCachedDevice).connect(/* connectAllProfiles= */ true);
    }

    @Test
    public void onDeviceClicked_connected_showsDisconnectConfirmDialog() {
        when(mBondedCachedDevice.isConnected()).thenReturn(true);
        mControllerHelper.markState(Lifecycle.State.STARTED);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);

        devicePreference.performClick();

        verify(mControllerHelper.getMockFragmentController()).showDialog(
                any(BluetoothDisconnectConfirmDialogFragment.class), isNull());
    }

    @Test
    public void devicePreferenceButtonClicked_noUserRestrictions_launchesDetailsFragment() {
        getShadowUserManager().setUserRestriction(mMyUserHandle, DISALLOW_CONFIG_BLUETOOTH, false);
        mControllerHelper.markState(Lifecycle.State.STARTED);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);

        assertThat(devicePreference.isSecondaryActionVisible()).isTrue();
        devicePreference.performSecondaryActionClick();

        verify(mControllerHelper.getMockFragmentController()).launchFragment(
                any(BluetoothDeviceDetailsFragment.class));
    }

    @Test
    public void devicePreferenceButton_disallowConfigBluetooth_actionStaysHidden() {
        getShadowUserManager().setUserRestriction(mMyUserHandle, DISALLOW_CONFIG_BLUETOOTH, true);
        mControllerHelper.markState(Lifecycle.State.STARTED);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);

        assertThat(devicePreference.isSecondaryActionVisible()).isFalse();
    }

    @Test
    public void onUxRestrictionsChanged_hasRestrictions_buttonHidden() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);

        CarUxRestrictions restrictions = new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP, 0).build();
        mController.onUxRestrictionsChanged(restrictions);

        assertThat(devicePreference.isSecondaryActionVisible()).isFalse();
    }

    @Test
    public void onUxRestrictionsChanged_restrictionToggled_buttonShown() {
        mControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_START);
        BluetoothDevicePreference devicePreference =
                (BluetoothDevicePreference) mPreferenceGroup.getPreference(0);

        CarUxRestrictions restrictions = new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP, 0).build();
        mController.onUxRestrictionsChanged(restrictions);

        CarUxRestrictions noRestrictions = new CarUxRestrictions.Builder(
                true, CarUxRestrictions.UX_RESTRICTIONS_BASELINE, 0).build();
        mController.onUxRestrictionsChanged(noRestrictions);

        assertThat(devicePreference.isSecondaryActionVisible()).isTrue();
    }

    private ShadowBluetoothAdapter getShadowBluetoothAdapter() {
        return (ShadowBluetoothAdapter) Shadow.extract(BluetoothAdapter.getDefaultAdapter());
    }

    private ShadowUserManager getShadowUserManager() {
        return Shadow.extract(UserManager.get(mContext));
    }
}
