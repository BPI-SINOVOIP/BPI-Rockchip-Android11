/*
 * Copyright 2018 The Android Open Source Project
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

package android.view.inspector.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.inspector.IntFlagMapping;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests for {@link IntFlagMapping}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public final class IntFlagMappingTest {
    @Test
    public void testNonExclusiveFlags() {
        IntFlagMapping mapping = new IntFlagMapping();
        mapping.add(1, 1, "ONE");
        mapping.add(2, 2, "TWO");

        assertEmpty(mapping.get(0));
        assertEquals(setOf("ONE"), mapping.get(1));
        assertEquals(setOf("TWO"), mapping.get(2));
        assertEquals(setOf("ONE", "TWO"), mapping.get(3));
        assertEmpty(mapping.get(4));
    }

    @Test
    public void testMutuallyExclusiveFlags() {
        IntFlagMapping mapping = new IntFlagMapping();
        mapping.add(3, 1, "ONE");
        mapping.add(3, 2, "TWO");

        assertEmpty(mapping.get(0));
        assertEquals(setOf("ONE"), mapping.get(1));
        assertEquals(setOf("TWO"), mapping.get(2));
        assertEmpty(mapping.get(3));
        assertEmpty(mapping.get(4));
    }

    @Test
    public void testMixedFlags() {
        IntFlagMapping mapping = new IntFlagMapping();
        mapping.add(3, 1, "ONE");
        mapping.add(3, 2, "TWO");
        mapping.add(4, 4, "FOUR");

        assertEmpty(mapping.get(0));
        assertEquals(setOf("ONE"), mapping.get(1));
        assertEquals(setOf("TWO"), mapping.get(2));
        assertEmpty(mapping.get(3));
        assertEquals(setOf("FOUR"), mapping.get(4));
        assertEquals(setOf("ONE", "FOUR"), mapping.get(5));
        assertEquals(setOf("TWO", "FOUR"), mapping.get(6));
        assertEquals(setOf("FOUR"), mapping.get(7));
    }

    @Test(expected = NullPointerException.class)
    public void testNullName() {
        new IntFlagMapping().add(0, 0, null);
    }

    private static Set<String> setOf(String... values) {
        final Set<String> set = new HashSet<>(values.length);
        set.addAll(Arrays.asList(values));
        return Collections.unmodifiableSet(set);
    }

    private static void assertEmpty(Collection collection) {
        assertTrue(collection.isEmpty());
    }
}
