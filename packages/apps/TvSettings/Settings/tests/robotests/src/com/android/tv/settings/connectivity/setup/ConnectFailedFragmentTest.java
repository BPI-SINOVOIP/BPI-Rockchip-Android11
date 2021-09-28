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

package com.android.tv.settings.connectivity.setup;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.net.wifi.WifiConfiguration;

import androidx.lifecycle.ViewModelProviders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class ConnectFailedFragmentTest {

    WifiConfiguration mWifiConfig;
    @Spy
    private ConnectFailedState.ConnectFailedFragment mConnectFailedFragment;
    private WifiSetupActivity mActivity;
    private UserChoiceInfo mUserChoiceInfo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(WifiSetupActivity.class).create().get();
        doReturn(mActivity).when(mConnectFailedFragment).getContext();
        mUserChoiceInfo = ViewModelProviders.of(mActivity).get(UserChoiceInfo.class);
        mUserChoiceInfo.init();
        mWifiConfig = new WifiConfiguration();
    }

    @Test
    public void testGetConnectionFailTitle_unknown_failure() {
        mWifiConfig.SSID = "testWifi1";
        mUserChoiceInfo.setConnectionFailedStatus(UserChoiceInfo.ConnectionFailedStatus.UNKNOWN);
        mUserChoiceInfo.setWifiConfiguration(mWifiConfig);
        mConnectFailedFragment.mUserChoiceInfo = mUserChoiceInfo;
        assertEquals(mConnectFailedFragment.getConnectionFailTitle(),
                "Couldn\'t connect to testWifi1");
    }

    @Test
    public void testGetConnectionFailTitle_auth_failure() {
        mWifiConfig.SSID = "testWifi1";
        mUserChoiceInfo.setConnectionFailedStatus(
                UserChoiceInfo.ConnectionFailedStatus.AUTHENTICATION);
        mUserChoiceInfo.setWifiConfiguration(mWifiConfig);
        mConnectFailedFragment.mUserChoiceInfo = mUserChoiceInfo;
        assertEquals(mConnectFailedFragment.getConnectionFailTitle(), "Wi-Fi password not valid");
    }

    @Test
    public void testGetConnectionFailTitle_rejected_failure() {
        mWifiConfig.SSID = "testWifi1";
        mUserChoiceInfo.setConnectionFailedStatus(UserChoiceInfo.ConnectionFailedStatus.REJECTED);
        mUserChoiceInfo.setWifiConfiguration(mWifiConfig);
        mConnectFailedFragment.mUserChoiceInfo = mUserChoiceInfo;
        assertEquals(mConnectFailedFragment.getConnectionFailTitle(), "Wi-Fi network didn\'t "
                + "accept connection");
    }

    @Test
    public void testGetConnectionFailTitle_time_out_failure() {
        mWifiConfig.SSID = "testWifi1";
        mUserChoiceInfo.setConnectionFailedStatus(UserChoiceInfo.ConnectionFailedStatus.TIMEOUT);
        mUserChoiceInfo.setWifiConfiguration(mWifiConfig);
        mConnectFailedFragment.mUserChoiceInfo = mUserChoiceInfo;
        assertEquals(mConnectFailedFragment.getConnectionFailTitle(), "Couldn\'t find testWifi1");
    }

}
