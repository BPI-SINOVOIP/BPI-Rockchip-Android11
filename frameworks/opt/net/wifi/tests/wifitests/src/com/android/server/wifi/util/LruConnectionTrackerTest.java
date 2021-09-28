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

package com.android.server.wifi.util;

import static org.junit.Assert.*;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.wifi.WifiConfiguration;

import androidx.test.filters.SmallTest;

import com.android.server.wifi.MockResources;
import com.android.server.wifi.WifiConfigurationTestUtil;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
public class LruConnectionTrackerTest {
    private static final int TEST_MAX_SIZE = 4;
    private LruConnectionTracker mList;
    private MockResources mResources;

    @Mock private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mResources = new MockResources();
        mResources.setInteger(R.integer.config_wifiMaxPnoSsidCount, TEST_MAX_SIZE - 1);
        when(mContext.getResources()).thenReturn(mResources);
        mList = new LruConnectionTracker(TEST_MAX_SIZE, mContext);
    }

    @Test
    public void testAddRemoveNetwork() {
        WifiConfiguration network = WifiConfigurationTestUtil.createOpenNetwork();
        assertEquals(Integer.MAX_VALUE, mList.getAgeIndexOfNetwork(network));
        mList.addNetwork(network);
        assertEquals(0, mList.getAgeIndexOfNetwork(network));
        mList.removeNetwork(network);
        assertEquals(Integer.MAX_VALUE, mList.getAgeIndexOfNetwork(network));
    }

    @Test
    public void testConnectionOrderStore() {
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network2 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network3 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network4 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network5 = WifiConfigurationTestUtil.createOpenNetwork();
        mList.addNetwork(network1);
        mList.addNetwork(network2);
        mList.addNetwork(network3);
        mList.addNetwork(network4);
        // Expect reverse order of the added order
        assertEquals(0, mList.getAgeIndexOfNetwork(network4));
        assertEquals(1, mList.getAgeIndexOfNetwork(network3));
        assertEquals(2, mList.getAgeIndexOfNetwork(network2));
        assertEquals(3, mList.getAgeIndexOfNetwork(network1));
        // Exceed the size of the list, should remove the oldest one.
        mList.addNetwork(network5);
        assertEquals(Integer.MAX_VALUE, mList.getAgeIndexOfNetwork(network1));
        assertEquals(0, mList.getAgeIndexOfNetwork(network5));
        // Add one already in the list, move it to the head of the list.
        mList.addNetwork(network3);
        assertEquals(0, mList.getAgeIndexOfNetwork(network3));
        assertEquals(1, mList.getAgeIndexOfNetwork(network5));
        assertEquals(2, mList.getAgeIndexOfNetwork(network4));
        assertEquals(3, mList.getAgeIndexOfNetwork(network2));
    }

    @Test
    public void testIsMostRecentlyNetwork() {
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network2 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network3 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network4 = WifiConfigurationTestUtil.createOpenNetwork();
        mList.addNetwork(network1);
        mList.addNetwork(network2);
        mList.addNetwork(network3);
        assertTrue(mList.isMostRecentlyConnected(network1));
        assertTrue(mList.isMostRecentlyConnected(network2));
        assertTrue(mList.isMostRecentlyConnected(network3));
        assertFalse(mList.isMostRecentlyConnected(network4));
        // network in the list is more than the threshold, will mark the oldest false.
        mList.addNetwork(network4);
        assertFalse(mList.isMostRecentlyConnected(network1));
        assertTrue(mList.isMostRecentlyConnected(network4));
        // Add network in the list and which is true. Should not change other network flag.
        mList.addNetwork(network2);
        assertFalse(mList.isMostRecentlyConnected(network1));
        assertTrue(mList.isMostRecentlyConnected(network2));
        assertTrue(mList.isMostRecentlyConnected(network3));
        assertTrue(mList.isMostRecentlyConnected(network4));
    }
}
