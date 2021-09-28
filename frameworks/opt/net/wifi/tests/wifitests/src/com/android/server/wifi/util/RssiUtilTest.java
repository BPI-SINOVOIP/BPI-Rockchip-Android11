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
package com.android.server.wifi.util;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.res.Resources;
import android.net.wifi.IWifiManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiBaseTest;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;


/** Unit tests for {@link RssiUtil}. */
@SmallTest
public class RssiUtilTest extends WifiBaseTest {

    public static final int[] RSSI_THRESHOLDS = {-88, -77, -66, -55};

    @Mock private Context mContext;
    @Mock private Resources mResources;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getIntArray(R.array.config_wifiRssiLevelThresholds))
                .thenReturn(RSSI_THRESHOLDS);
    }

    /**
     * Tests that our default RSSI thresholds matches what would be calculated by
     * {@link WifiManager#calculateSignalLevel(int, int)}.
     */
    @Test
    public void testCalculateSignalLevelImplementationsConsistent() {
        for (int rssi = -100; rssi <= 0; rssi++) {
            int expected = WifiManager.calculateSignalLevel(rssi, WifiManager.RSSI_LEVELS);
            int actual = RssiUtil.calculateSignalLevel(mContext, rssi);
            assertWithMessage("RSSI: %s", rssi).that(actual).isEqualTo(expected);
        }
    }

    /** Tests the signal level at RSSIs at around the thresholds. */
    @Test
    public void testCalculateSignalLevel() {
        assertThat(RssiUtil.calculateSignalLevel(mContext, -100)).isEqualTo(0);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -89)).isEqualTo(0);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -88)).isEqualTo(1);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -78)).isEqualTo(1);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -77)).isEqualTo(2);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -67)).isEqualTo(2);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -66)).isEqualTo(3);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -56)).isEqualTo(3);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -55)).isEqualTo(4);
        assertThat(RssiUtil.calculateSignalLevel(mContext, 0)).isEqualTo(4);
        assertThat(RssiUtil.calculateSignalLevel(mContext, Integer.MAX_VALUE)).isEqualTo(4);
    }

    /** Tests that an empty thresholds array always returns level 0. */
    @Test
    public void testCalculateSignalLevelEmptyThresholdsArray() {
        when(mResources.getIntArray(R.array.config_wifiRssiLevelThresholds))
                .thenReturn(new int[0]);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -100)).isEqualTo(0);
        assertThat(RssiUtil.calculateSignalLevel(mContext, -50)).isEqualTo(0);
        assertThat(RssiUtil.calculateSignalLevel(mContext, 0)).isEqualTo(0);

        assertThat(RssiUtil.calculateSignalLevel(mContext, Integer.MAX_VALUE)).isEqualTo(0);
    }

    /** Tests that the expected default max signal level is returned. */
    @Test
    public void testGetMaxSignalLevel() throws Exception {
        IWifiManager iWifiManager = mock(IWifiManager.class);

        doAnswer(invocation -> {
            int rssi = invocation.getArgument(0);
            return RssiUtil.calculateSignalLevel(mContext, rssi);
        }).when(iWifiManager).calculateSignalLevel(anyInt());

        ApplicationInfo applicationInfo = mock(ApplicationInfo.class);
        applicationInfo.targetSdkVersion = Build.VERSION_CODES.Q;
        Context context = mock(Context.class);
        when(context.getApplicationInfo()).thenReturn(applicationInfo);
        when(context.getOpPackageName()).thenReturn("TestPackage");
        WifiManager wifiManager = new WifiManager(
                context, iWifiManager, mock(Looper.class));

        int level = wifiManager.getMaxSignalLevel();
        assertThat(level).isEqualTo(4);
    }
}
