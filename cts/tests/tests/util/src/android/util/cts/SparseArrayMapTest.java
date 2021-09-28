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

package android.util.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.util.SparseArrayMap;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class SparseArrayMapTest {
    private static final String[] KEYS_1 = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k"};
    private static final String[] KEYS_2 = {"z", "y", "x", "w", "v", "u", "t", "s", "r", "q"};

    @Test
    public void testStoreSingleInt() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(0, KEYS_1[i], i);
        }

        assertEquals(KEYS_1.length, sam.numElementsForKey(0));
        assertEquals(0, sam.numElementsForKey(1));

        for (int i = 0; i < KEYS_1.length; i++) {
            assertEquals(i, sam.get(0, KEYS_1[i]).intValue());
        }
    }

    @Test
    public void testStoreMultipleInt() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();

        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(0, KEYS_1[i], i);
        }
        for (int i = 0; i < KEYS_2.length; i++) {
            sam.add(1, KEYS_2[i], i);
        }

        assertEquals(KEYS_1.length, sam.numElementsForKey(0));
        assertEquals(KEYS_2.length, sam.numElementsForKey(1));

        for (int i = 0; i < KEYS_1.length; i++) {
            assertEquals(i, sam.get(0, KEYS_1[i]).intValue());
            assertNull(sam.get(1, KEYS_1[i]));
        }
        for (int i = 0; i < KEYS_2.length; i++) {
            assertNull(sam.get(0, KEYS_2[i]));
            assertEquals(i, sam.get(1, KEYS_2[i]).intValue());
        }
    }

    @Test
    public void testClear() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(0, KEYS_1[i], i);
        }

        assertEquals(KEYS_1.length, sam.numElementsForKey(0));

        sam.clear();
        assertEquals(0, sam.numElementsForKey(0));
    }

    @Test
    public void testContains() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(0, KEYS_1[i], i);
        }
        for (int i = 0; i < KEYS_1.length; i++) {
            assertTrue(sam.contains(0, KEYS_1[i]));
            assertFalse(sam.contains(1, KEYS_1[i]));
        }
        for (int i = 0; i < KEYS_2.length; i++) {
            assertFalse(sam.contains(0, KEYS_2[i]));
            assertFalse(sam.contains(1, KEYS_2[i]));
        }
    }

    @Test
    public void testDelete() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(0, KEYS_1[i], i);
            sam.add(1, KEYS_1[i], i);
            sam.add(2, KEYS_1[i], i);
        }

        for (int i = 0; i < KEYS_1.length; i++) {
            assertTrue(sam.contains(0, KEYS_1[i]));
            assertTrue(sam.contains(1, KEYS_1[i]));
            assertTrue(sam.contains(2, KEYS_1[i]));

            assertEquals(i, sam.delete(0, KEYS_1[i]).intValue());
            assertFalse(sam.contains(0, KEYS_1[i]));
            assertTrue(sam.contains(1, KEYS_1[i]));
            assertTrue(sam.contains(2, KEYS_1[i]));

            assertNull(sam.delete(0, KEYS_1[i]));
            assertFalse(sam.contains(0, KEYS_1[i]));
            assertTrue(sam.contains(1, KEYS_1[i]));
            assertTrue(sam.contains(2, KEYS_1[i]));

            assertEquals(i, sam.delete(1, KEYS_1[i]).intValue());
            assertFalse(sam.contains(0, KEYS_1[i]));
            assertFalse(sam.contains(1, KEYS_1[i]));
            assertTrue(sam.contains(2, KEYS_1[i]));
        }

        assertEquals(0, sam.numElementsForKey(0));
        assertEquals(0, sam.numElementsForKey(1));
        assertEquals(KEYS_1.length, sam.numElementsForKey(2));
        sam.delete(2);
        assertEquals(0, sam.numElementsForKey(2));
    }

    @Test
    public void testGetOrDefault() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            if (i % 2 == 0) {
                sam.add(0, KEYS_1[i], i);
            }
        }

        Integer def = Integer.MAX_VALUE;
        for (int i = 0; i < KEYS_1.length; i++) {
            assertEquals(i % 2 == 0 ? i : def, sam.getOrDefault(0, KEYS_1[i], def).intValue());
        }
    }

    @Test
    public void testIntKeyIndexing() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(i * 2, KEYS_1[i], i * 2 + 1);
        }
        for (int i = 0; i < KEYS_1.length; i++) {
            int index = sam.indexOfKey(2 * i);
            assertEquals(2 * i, sam.keyAt(index));
        }
    }

    @Test
    public void testIntStringKeyIndexing() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < KEYS_1.length; i++) {
            sam.add(i * 2, KEYS_1[i], i * 2 + 1);
        }
        for (int i = 0; i < KEYS_1.length; i++) {
            int intIndex = sam.indexOfKey(2 * i);
            int stringIndex = sam.indexOfKey(2 * i, KEYS_1[i]);
            assertEquals(KEYS_1[i], sam.keyAt(intIndex, stringIndex));
        }
    }

    @Test
    public void testNumMaps() {
        SparseArrayMap<Integer> sam = new SparseArrayMap<>();
        for (int i = 0; i < 10; i++) {
            assertEquals(i, sam.numMaps());
            sam.add(i, "blue", i);
            assertEquals(i + 1, sam.numMaps());
        }

        // Delete a key that doesn't exist.
        sam.delete(100);
        assertEquals(10, sam.numMaps());

        for (int i = 10; i > 0; i--) {
            assertEquals(i, sam.numMaps());
            sam.delete(i - 1);
            assertEquals(i - 1, sam.numMaps());
        }
    }
}
