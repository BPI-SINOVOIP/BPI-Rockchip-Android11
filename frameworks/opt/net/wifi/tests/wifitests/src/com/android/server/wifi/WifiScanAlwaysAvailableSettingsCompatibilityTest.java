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

package com.android.server.wifi;

import static com.android.server.wifi.WifiScanAlwaysAvailableSettingsCompatibility.SETTINGS_GLOBAL_WIFI_SCAN_ALWAYS_AVAILABLE;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.validateMockitoUsage;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.os.Handler;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link com.android.server.wifi.WifiScanAlwaysAvailableSettingsCompatibility}.
 */
@SmallTest
public class WifiScanAlwaysAvailableSettingsCompatibilityTest extends WifiBaseTest {
    @Mock
    private Context mContext;
    @Mock
    private Handler mHandler;
    @Mock
    private WifiSettingsStore mWifiSettingsStore;
    @Mock
    private ActiveModeWarden mActiveModeWarden;
    @Mock
    private FrameworkFacade mFrameworkFacade;
    @Mock
    private ContentResolver mContentResolver;

    private ArgumentCaptor<ContentObserver> mContentObserverArgumentCaptor =
            ArgumentCaptor.forClass(ContentObserver.class);

    private WifiScanAlwaysAvailableSettingsCompatibility mScanAlwaysCompatibility;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        when(mContext.getContentResolver()).thenReturn(mContentResolver);
        mScanAlwaysCompatibility =
                new WifiScanAlwaysAvailableSettingsCompatibility(mContext, mHandler,
                        mWifiSettingsStore, mActiveModeWarden, mFrameworkFacade);
    }

    /**
     * Called after each test
     */
    @After
    public void cleanup() {
        validateMockitoUsage();
    }

    @Test
    public void reactToContentObserverChanges() {
        mScanAlwaysCompatibility.initialize();
        verify(mContentResolver).registerContentObserver(
                any(), anyBoolean(), mContentObserverArgumentCaptor.capture());

        ContentObserver contentObserver = mContentObserverArgumentCaptor.getValue();
        assertNotNull(contentObserver);

        when(mWifiSettingsStore.isScanAlwaysAvailable()).thenReturn(false);
        when(mFrameworkFacade.getIntegerSetting(
                any(ContentResolver.class),
                eq(SETTINGS_GLOBAL_WIFI_SCAN_ALWAYS_AVAILABLE),
                anyInt()))
                .thenReturn(1);
        contentObserver.onChange(false);

        verify(mWifiSettingsStore).handleWifiScanAlwaysAvailableToggled(true);
        verify(mActiveModeWarden).scanAlwaysModeChanged();

        when(mWifiSettingsStore.isScanAlwaysAvailable()).thenReturn(true);
        when(mFrameworkFacade.getIntegerSetting(
                any(ContentResolver.class),
                eq(SETTINGS_GLOBAL_WIFI_SCAN_ALWAYS_AVAILABLE),
                anyInt()))
                .thenReturn(0);
        contentObserver.onChange(false);

        verify(mWifiSettingsStore).handleWifiScanAlwaysAvailableToggled(false);
        verify(mActiveModeWarden, times(2)).scanAlwaysModeChanged();
    }


    @Test
    public void changeSetting() {
        mScanAlwaysCompatibility.initialize();

        mScanAlwaysCompatibility.handleWifiScanAlwaysAvailableToggled(true);
        verify(mFrameworkFacade).setIntegerSetting(
                any(ContentResolver.class),
                eq(SETTINGS_GLOBAL_WIFI_SCAN_ALWAYS_AVAILABLE),
                eq(1));

        mScanAlwaysCompatibility.handleWifiScanAlwaysAvailableToggled(false);
        verify(mFrameworkFacade).setIntegerSetting(
                any(ContentResolver.class),
                eq(SETTINGS_GLOBAL_WIFI_SCAN_ALWAYS_AVAILABLE),
                eq(0));
    }
}
