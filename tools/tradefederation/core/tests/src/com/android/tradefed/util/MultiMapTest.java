/*
 * Copyright (C) 2011 The Android Open Source Project
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
package com.android.tradefed.util;

import junit.framework.TestCase;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@link MultiMap}.
 */
public class MultiMapTest extends TestCase {

    /**
     * Test for {@link MultiMap#getUniqueMap()}.
     */
    public void testGetUniqueMap() {
        MultiMap<String, String> multiMap = new MultiMap<String, String>();
        multiMap.put("key", "value1");
        multiMap.put("key", "value2");
        multiMap.put("uniquekey", "value");
        multiMap.put("key2", "collisionKeyvalue");
        Map<String, String> uniqueMap = multiMap.getUniqueMap();
        assertEquals(4, uniqueMap.size());
        // key's for value1, value2 and collisionKeyvalue might be one of three possible values,
        // depending on order of collision resolvement
        assertTrue(checkKeyForValue(uniqueMap, "value1"));
        assertTrue(checkKeyForValue(uniqueMap, "value2"));
        assertTrue(checkKeyForValue(uniqueMap, "collisionKeyvalue"));
        // uniquekey should be unmodified
        assertEquals("value", uniqueMap.get("uniquekey"));
    }

    /**
     * Helper method testGetUniqueMap() for that will check that the given value's key matches
     * one of the expected values
     *
     * @param uniqueMap
     * @param value
     * @return <code>true</code> if key matched one of the expected values
     */
    private boolean checkKeyForValue(Map<String, String> uniqueMap, String value) {
        for (Map.Entry<String, String> entry : uniqueMap.entrySet()) {
            if (entry.getValue().equals(value)) {
                String key = entry.getKey();
                return key.equals("key") || key.equals("key2") || key.equals("key2X");
            }
        }
        return false;
    }

    /** Test for {@link MultiMap#entries()}. */
    public void testEmptyMapEntries() {
        MultiMap<String, String> multiMap = new MultiMap<>();
        assertEquals(0, multiMap.entries().size());
    }

    public void testEntries() {
        MultiMap<String, String> multiMap = new MultiMap<>();
        multiMap.put("a", "1");
        multiMap.put("b", "2");
        multiMap.put("c", "3");
        multiMap.put("c", "4");
        multiMap.put("c", "4");

        assertContainsExactly(
                Arrays.asList(
                        asEntry("a", "1"),
                        asEntry("b", "2"),
                        asEntry("c", "3"),
                        asEntry("c", "4"),
                        asEntry("c", "4")),
                multiMap.entries());
    }

    private static <K, V> Map.Entry<K, V> asEntry(K key, V value) {
        return new AbstractMap.SimpleImmutableEntry<>(key, value);
    }

    /**
     * Check that actual collection contains exact items as expected collection, without considering
     * order.
     */
    private static <T> void assertContainsExactly(Collection<T> expected, Collection<T> actual) {
        assertEquals(expected.size(), actual.size());
        List<T> copy = new ArrayList<>(actual);
        for (T item : expected) {
            int index = copy.indexOf(item);
            if (index == -1) {
                fail(String.format("Couldn't find %s in %s", item, actual.getClass()));
            }
            copy.remove(index);
        }
    }
}
