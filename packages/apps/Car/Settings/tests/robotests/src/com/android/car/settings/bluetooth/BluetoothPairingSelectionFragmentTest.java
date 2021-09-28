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

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.bluetooth.BluetoothDevice;
import android.content.Context;

import com.android.car.settings.testutils.BaseTestActivity;
import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowBluetoothAdapter;
import com.android.car.settings.testutils.ShadowBluetoothPan;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.ToolbarController;
import com.android.settingslib.bluetooth.BluetoothCallback;
import com.android.settingslib.bluetooth.BluetoothEventManager;
import com.android.settingslib.bluetooth.CachedBluetoothDevice;
import com.android.settingslib.bluetooth.LocalBluetoothManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

/** Unit test for {@link BluetoothPairingSelectionFragment}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowBluetoothAdapter.class, ShadowBluetoothPan.class})
public class BluetoothPairingSelectionFragmentTest {

    @Mock
    private BluetoothEventManager mEventManager;
    private BluetoothEventManager mSaveRealEventManager;
    private LocalBluetoothManager mLocalBluetoothManager;
    private FragmentController<BluetoothPairingSelectionFragment> mFragmentController;
    private BluetoothPairingSelectionFragment mFragment;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context context = RuntimeEnvironment.application;
        mLocalBluetoothManager = LocalBluetoothManager.getInstance(context, /* onInitCallback= */
                null);
        mSaveRealEventManager = mLocalBluetoothManager.getEventManager();
        ReflectionHelpers.setField(mLocalBluetoothManager, "mEventManager", mEventManager);

        mFragment = new BluetoothPairingSelectionFragment();
        mFragmentController = FragmentController.of(mFragment);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowBluetoothAdapter.reset();
        ReflectionHelpers.setField(mLocalBluetoothManager, "mEventManager", mSaveRealEventManager);
    }

    @Test
    public void onStart_setsBluetoothManagerForegroundActivity() {
        mFragmentController.create().start();

        assertThat(mLocalBluetoothManager.getForegroundActivity()).isEqualTo(
                mFragment.requireActivity());
    }

    @Test
    public void onStart_registersEventListener() {
        mFragmentController.create().start();

        verify(mEventManager).registerCallback(any(BluetoothCallback.class));
    }

    @Test
    public void onStart_showsProgressBar() {
        mFragmentController.setup();
        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());

        assertThat(toolbar.getProgressBar().isVisible()).isTrue();
    }

    @Test
    public void onStop_clearsBluetoothManagerForegroundActivity() {
        mFragmentController.create().start().resume().pause().stop();

        assertThat(mLocalBluetoothManager.getForegroundActivity()).isNull();
    }

    @Test
    public void onStop_unregistersEventListener() {
        ArgumentCaptor<BluetoothCallback> callbackCaptor = ArgumentCaptor.forClass(
                BluetoothCallback.class);
        mFragmentController.create().start().resume().pause().stop();

        verify(mEventManager).registerCallback(callbackCaptor.capture());
        verify(mEventManager).unregisterCallback(callbackCaptor.getValue());
    }

    @Test
    public void onStop_hidesProgressBar() {
        mFragmentController.setup().onPause();
        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        toolbar.getProgressBar().setVisible(true);

        mFragmentController.stop();

        assertThat(toolbar.getProgressBar().isVisible()).isFalse();
    }

    @Test
    public void onDeviceBondStateChanged_deviceBonded_goesBack() {
        ArgumentCaptor<BluetoothCallback> callbackCaptor = ArgumentCaptor.forClass(
                BluetoothCallback.class);
        mFragmentController.setup();
        verify(mEventManager).registerCallback(callbackCaptor.capture());

        callbackCaptor.getValue().onDeviceBondStateChanged(mock(CachedBluetoothDevice.class),
                BluetoothDevice.BOND_BONDED);

        assertThat(
                ((BaseTestActivity) mFragment.requireActivity()).getOnBackPressedFlag()).isTrue();
    }
}
