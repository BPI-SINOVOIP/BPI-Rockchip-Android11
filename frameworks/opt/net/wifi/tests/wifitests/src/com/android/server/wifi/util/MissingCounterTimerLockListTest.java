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

import androidx.test.filters.SmallTest;

import com.android.server.wifi.Clock;
import com.android.server.wifi.WifiBaseTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.HashSet;
import java.util.Set;

@SmallTest
public class MissingCounterTimerLockListTest extends WifiBaseTest {
    private static final int TEST_MISSING_COUNT = 2;
    private static final long BLOCKING_DURATION = 1000;
    private static final String TEST_SSID = "testSSID";
    private static final String TEST_FQDN = "testFQDN";

    private MissingCounterTimerLockList<String> mMissingCounterTimerLockList;

    @Mock Clock mClock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMissingCounterTimerLockList =
                new MissingCounterTimerLockList<>(TEST_MISSING_COUNT, mClock);
    }

    @Test
    public void testAddRemove() {
        mMissingCounterTimerLockList.add(TEST_SSID, BLOCKING_DURATION);
        mMissingCounterTimerLockList.add(TEST_FQDN, BLOCKING_DURATION);
        mMissingCounterTimerLockList.add(null, BLOCKING_DURATION);
        assertEquals(2, mMissingCounterTimerLockList.size());
        assertTrue(mMissingCounterTimerLockList.isLocked(TEST_SSID));
        assertTrue(mMissingCounterTimerLockList.isLocked(TEST_FQDN));
        assertTrue(mMissingCounterTimerLockList.remove(TEST_SSID));
        assertEquals(1, mMissingCounterTimerLockList.size());
        assertFalse(mMissingCounterTimerLockList.isLocked(TEST_SSID));
        assertTrue(mMissingCounterTimerLockList.isLocked(TEST_FQDN));
        mMissingCounterTimerLockList.clear();
        assertEquals(0, mMissingCounterTimerLockList.size());
    }

    @Test
    public void testUpdateAndTimer() {
        mMissingCounterTimerLockList.add(TEST_SSID, BLOCKING_DURATION);
        mMissingCounterTimerLockList.add(TEST_FQDN, BLOCKING_DURATION);
        assertEquals(2, mMissingCounterTimerLockList.size());
        when(mClock.getWallClockMillis()).thenReturn((long) 0);
        Set<String> updateSet = new HashSet<>();
        updateSet.add(TEST_FQDN);
        mMissingCounterTimerLockList.update(updateSet);
        assertEquals(2, mMissingCounterTimerLockList.size());
        mMissingCounterTimerLockList.update(updateSet);
        assertEquals(2, mMissingCounterTimerLockList.size());
        when(mClock.getWallClockMillis()).thenReturn(BLOCKING_DURATION + 50);
        assertEquals(2, mMissingCounterTimerLockList.size());
        assertFalse(mMissingCounterTimerLockList.isLocked(TEST_SSID));
        assertTrue(mMissingCounterTimerLockList.isLocked(TEST_FQDN));
    }
}
