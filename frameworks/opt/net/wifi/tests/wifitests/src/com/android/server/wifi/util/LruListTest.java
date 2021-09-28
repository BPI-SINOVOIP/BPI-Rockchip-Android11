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

import androidx.test.filters.SmallTest;

import com.android.server.wifi.WifiBaseTest;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;


/**
 * Unit tests for IntCounter.
 */
@SmallTest
public class LruListTest extends WifiBaseTest {

    private static final int TEST_MAX_SIZE = 3;
    private LruList<Integer> mLruList;

    @Before
    public void setUp() {
        mLruList = new LruList<Integer>(TEST_MAX_SIZE);
    }

    /**
     * Verify that the least recent entry is removed when the Lru list becomes full.
     * And also an existing element will get moved to the front of the list when added.
     */
    @Test
    public void testLeastRecentEntryIsRemoved() {
        for (int i = 0; i < TEST_MAX_SIZE + 1; i++) {
            mLruList.add(i);
        }
        mLruList.add(2);
        assertEquals(TEST_MAX_SIZE, mLruList.size());
        List<Integer> expected = Arrays.asList(new Integer[] {2, 3, 1});
        assertEquals(expected, mLruList.getEntries());
    }

    /**
     * Verify null input is no-op.
     */
    @Test
    public void testAddNull() {
        mLruList.add(null);
        assertEquals(0, mLruList.size());
    }

    @Test
    public void getIndex() {
        for (int i = 0; i < TEST_MAX_SIZE; i++) {
            mLruList.add(i);
        }
        for (int i = 0; i < TEST_MAX_SIZE; i++) {
            assertEquals(i, mLruList.indexOf(TEST_MAX_SIZE - i - 1));
        }
        // Verify not in the list will return -1.
        assertEquals(-1, mLruList.indexOf(3));
        mLruList.add(3);
        // Verify add more elements more than list size, will remove oldest one.
        assertEquals(0, mLruList.indexOf(3));
        assertEquals(-1, mLruList.indexOf(0));
        // Verify add an element in the list will make it the most recently one.
        mLruList.add(1);
        assertEquals(0, mLruList.indexOf(1));
        assertEquals(1, mLruList.indexOf(3));
        assertEquals(2, mLruList.indexOf(2));
        // Verify remove one element in the list, all element behind it index should decrease 1.
        mLruList.remove(1);
        assertEquals(-1, mLruList.indexOf(1));
        assertEquals(0, mLruList.indexOf(3));
        assertEquals(1, mLruList.indexOf(2));
    }
}
