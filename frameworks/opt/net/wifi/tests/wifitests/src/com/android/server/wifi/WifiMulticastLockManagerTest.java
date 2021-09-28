/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.os.BatteryStatsManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.WorkSource;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link com.android.server.wifi.WifiConfigStoreData}.
 */
@SmallTest
public class WifiMulticastLockManagerTest extends WifiBaseTest {
    private static final String WL_1_TAG = "Wakelock-1";
    private static final String WL_2_TAG = "Wakelock-2";

    @Mock WifiMulticastLockManager.FilterController mHandler;
    @Mock BatteryStatsManager mBatteryStats;
    WifiMulticastLockManager mManager;

    /**
     * Initialize |WifiMulticastLockManager| instance before each test.
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mManager = new WifiMulticastLockManager(mHandler, mBatteryStats);
    }

    /**
     * Test behavior when no locks are held.
     */
    @Test
    public void noLocks() {
        assertFalse(mManager.isMulticastEnabled());
        mManager.initializeFiltering();
        verify(mHandler, times(1)).startFilteringMulticastPackets();
    }

    /**
     * Test behavior when one lock is aquired then released.
     */
    @Test
    public void oneLock() throws RemoteException {
        IBinder binder = mock(IBinder.class);
        mManager.acquireLock(binder, WL_1_TAG);
        assertTrue(mManager.isMulticastEnabled());
        verify(mHandler).stopFilteringMulticastPackets();
        mManager.initializeFiltering();
        verify(mHandler, times(0)).startFilteringMulticastPackets();
        ArgumentCaptor<WorkSource> wsCaptor = ArgumentCaptor.forClass(WorkSource.class);
        verify(mBatteryStats).reportWifiMulticastEnabled(wsCaptor.capture());
        assertNotNull(wsCaptor.getValue());
        assertEquals(Binder.getCallingUid(), wsCaptor.getValue().getAttributionUid());
        verify(mBatteryStats, times(0)).reportWifiMulticastDisabled(any());

        mManager.releaseLock(WL_1_TAG);
        verify(mBatteryStats).reportWifiMulticastDisabled(wsCaptor.capture());
        assertNotNull(wsCaptor.getValue());
        assertEquals(Binder.getCallingUid(), wsCaptor.getValue().getAttributionUid());
        assertFalse(mManager.isMulticastEnabled());
    }

    /**
     * Test behavior when one lock is aquired then released with the wrong tag.
     */
    @Test
    public void oneLock_wrongName() throws RemoteException {
        IBinder binder = mock(IBinder.class);
        mManager.acquireLock(binder, WL_1_TAG);
        assertTrue(mManager.isMulticastEnabled());
        verify(mHandler).stopFilteringMulticastPackets();
        mManager.initializeFiltering();
        verify(mHandler, never()).startFilteringMulticastPackets();
        verify(mBatteryStats).reportWifiMulticastEnabled(any());
        verify(mBatteryStats, never()).reportWifiMulticastDisabled(any());

        mManager.releaseLock(WL_2_TAG);
        verify(mBatteryStats, never()).reportWifiMulticastDisabled(any());
        assertTrue(mManager.isMulticastEnabled());
    }

    /**
     * Test behavior when multiple locks are aquired then released in nesting order.
     */
    @Test
    public void multipleLocksInOrder() throws RemoteException {
        IBinder binder = mock(IBinder.class);

        InOrder inOrderHandler = inOrder(mHandler);
        InOrder inOrderBatteryStats = inOrder(mBatteryStats);

        mManager.acquireLock(binder, WL_1_TAG);
        inOrderHandler.verify(mHandler).stopFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastEnabled(any());
        assertTrue(mManager.isMulticastEnabled());

        mManager.acquireLock(binder, WL_2_TAG);
        inOrderHandler.verify(mHandler).stopFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastEnabled(any());
        assertTrue(mManager.isMulticastEnabled());

        mManager.initializeFiltering();
        inOrderHandler.verify(mHandler, never()).startFilteringMulticastPackets();

        mManager.releaseLock(WL_2_TAG);
        inOrderHandler.verify(mHandler, never()).startFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastDisabled(any());
        assertTrue(mManager.isMulticastEnabled());

        mManager.releaseLock(WL_1_TAG);
        inOrderHandler.verify(mHandler).startFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastDisabled(any());
        assertFalse(mManager.isMulticastEnabled());
    }

    /**
     * Test behavior when multiple locks are aquired then released out of nesting order.
     */
    @Test
    public void multipleLocksOutOfOrder() throws RemoteException {
        IBinder binder = mock(IBinder.class);

        InOrder inOrderHandler = inOrder(mHandler);
        InOrder inOrderBatteryStats = inOrder(mBatteryStats);

        mManager.acquireLock(binder, WL_1_TAG);
        inOrderHandler.verify(mHandler).stopFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastEnabled(any());
        assertTrue(mManager.isMulticastEnabled());

        mManager.acquireLock(binder, WL_2_TAG);
        inOrderHandler.verify(mHandler).stopFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastEnabled(any());
        assertTrue(mManager.isMulticastEnabled());

        mManager.initializeFiltering();
        inOrderHandler.verify(mHandler, never()).startFilteringMulticastPackets();

        mManager.releaseLock(WL_1_TAG);
        inOrderHandler.verify(mHandler, never()).startFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastDisabled(any());
        assertTrue(mManager.isMulticastEnabled());

        mManager.releaseLock(WL_2_TAG);
        inOrderHandler.verify(mHandler).startFilteringMulticastPackets();
        inOrderBatteryStats.verify(mBatteryStats).reportWifiMulticastDisabled(any());
        assertFalse(mManager.isMulticastEnabled());
    }
}
