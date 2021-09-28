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

package com.android.car.settings.wifi;

import static com.android.car.ui.core.CarUi.requireToolbar;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.Intent;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.test.core.app.ApplicationProvider;

import com.android.car.settings.testutils.FragmentController;
import com.android.car.settings.testutils.ShadowLocalBroadcastManager;
import com.android.car.settings.testutils.ShadowWifiManager;
import com.android.car.ui.core.testsupport.CarUiInstallerRobolectric;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;
import com.android.settingslib.wifi.AccessPoint;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowLocalBroadcastManager.class, ShadowWifiManager.class})
public class AddWifiFragmentTest {

    private Context mContext;
    private LocalBroadcastManager mLocalBroadcastManager;
    private AddWifiFragment mFragment;
    private FragmentController<AddWifiFragment> mFragmentController;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mLocalBroadcastManager = LocalBroadcastManager.getInstance(mContext);
        mFragment = new AddWifiFragment();
        mFragmentController = FragmentController.of(mFragment);

        // Needed to install Install CarUiLib BaseLayouts Toolbar for test activity
        CarUiInstallerRobolectric.install();
    }

    @After
    public void tearDown() {
        ShadowLocalBroadcastManager.reset();
        ShadowWifiManager.reset();
    }

    @Test
    public void onCreate_registersNameChangeListener() {
        mFragmentController.create();

        assertThat(isReceiverRegisteredForAction(
                NetworkNamePreferenceController.ACTION_NAME_CHANGE)).isTrue();
    }

    @Test
    public void onCreate_registersSecurityChangeListener() {
        mFragmentController.create();

        assertThat(isReceiverRegisteredForAction(
                NetworkSecurityPreferenceController.ACTION_SECURITY_CHANGE)).isTrue();
    }

    @Test
    public void onDestroy_unregistersNameChangeListener() {
        mFragmentController.create();
        mFragmentController.destroy();

        assertThat(isReceiverRegisteredForAction(
                NetworkNamePreferenceController.ACTION_NAME_CHANGE)).isFalse();
    }

    @Test
    public void onDestroy_unregistersSecurityChangeListener() {
        mFragmentController.create();
        mFragmentController.destroy();

        assertThat(isReceiverRegisteredForAction(
                NetworkSecurityPreferenceController.ACTION_SECURITY_CHANGE)).isFalse();
    }

    @Test
    public void initialState_buttonDisabled() {
        mFragmentController.setup();
        assertThat(getAddWifiButton().isEnabled()).isFalse();
    }

    @Test
    public void receiveNameChangeIntent_emptyName_buttonDisabled() {
        mFragmentController.setup();

        sendNameChangeBroadcastIntent("");

        assertThat(getAddWifiButton().isEnabled()).isFalse();
    }

    @Test
    public void receiveNameChangeIntent_name_buttonEnabled() {
        mFragmentController.setup();

        sendNameChangeBroadcastIntent("test_network_name");

        assertThat(getAddWifiButton().isEnabled()).isTrue();
    }

    @Test
    public void receiveSecurityChangeIntent_nameSet_buttonDisabled() {
        mFragmentController.setup();
        sendNameChangeBroadcastIntent("test_network_name");

        sendSecurityChangeBroadcastIntent(AccessPoint.SECURITY_PSK);

        assertThat(getAddWifiButton().isEnabled()).isFalse();
    }

    private void sendNameChangeBroadcastIntent(String networkName) {
        Intent intent = new Intent(NetworkNamePreferenceController.ACTION_NAME_CHANGE);
        intent.putExtra(NetworkNamePreferenceController.KEY_NETWORK_NAME, networkName);
        mLocalBroadcastManager.sendBroadcastSync(intent);
        Robolectric.flushForegroundThreadScheduler();
    }

    private void sendSecurityChangeBroadcastIntent(int securityType) {
        Intent intent = new Intent(NetworkSecurityPreferenceController.ACTION_SECURITY_CHANGE);
        intent.putExtra(NetworkSecurityPreferenceController.KEY_SECURITY_TYPE, securityType);
        mLocalBroadcastManager.sendBroadcastSync(intent);
        Robolectric.flushForegroundThreadScheduler();
    }

    private MenuItem getAddWifiButton() {
        ToolbarController toolbar = requireToolbar(mFragment.requireActivity());
        return toolbar.getMenuItems().get(0);
    }

    private boolean isReceiverRegisteredForAction(String action) {
        List<ShadowLocalBroadcastManager.Wrapper> receivers =
                ShadowLocalBroadcastManager.getRegisteredBroadcastReceivers();

        boolean found = false;
        for (ShadowLocalBroadcastManager.Wrapper receiver : receivers) {
            if (receiver.getIntentFilter().hasAction(action)) {
                found = true;
            }
        }

        return found;
    }
}
