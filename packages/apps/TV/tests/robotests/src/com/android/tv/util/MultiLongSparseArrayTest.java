/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.tv.util;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link MultiLongSparseArray}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class MultiLongSparseArrayTest {
    @Test
    public void testEmpty() {
        MultiLongSparseArray<String> sparseArray = new MultiLongSparseArray<>();
        assertThat(sparseArray.get(0)).isEmpty();
    }

    @Test
    public void testOneElement() {
        MultiLongSparseArray<String> sparseArray = new MultiLongSparseArray<>();
        sparseArray.put(0, "foo");
        assertThat(sparseArray.get(0)).containsExactly("foo");
    }

    @Test
    public void testTwoElements() {
        MultiLongSparseArray<String> sparseArray = new MultiLongSparseArray<>();
        sparseArray.put(0, "foo");
        sparseArray.put(0, "bar");
        assertThat(sparseArray.get(0)).containsExactly("foo", "bar");
    }

    @Test
    public void testClearEmptyCache() {
        MultiLongSparseArray<String> sparseArray = new MultiLongSparseArray<>();
        sparseArray.clearEmptyCache();
        assertThat(sparseArray.getEmptyCacheSize()).isEqualTo(0);
        sparseArray.put(0, "foo");
        sparseArray.remove(0, "foo");
        assertThat(sparseArray.getEmptyCacheSize()).isEqualTo(1);
        sparseArray.clearEmptyCache();
        assertThat(sparseArray.getEmptyCacheSize()).isEqualTo(0);
    }

    @Test
    public void testMaxEmptyCacheSize() {
        MultiLongSparseArray<String> sparseArray = new MultiLongSparseArray<>();
        sparseArray.clearEmptyCache();
        assertThat(sparseArray.getEmptyCacheSize()).isEqualTo(0);
        for (int i = 0; i <= MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT + 2; i++) {
            sparseArray.put(i, "foo");
        }
        for (int i = 0; i <= MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT + 2; i++) {
            sparseArray.remove(i, "foo");
        }
        assertThat(sparseArray.getEmptyCacheSize())
                .isEqualTo(MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT);
        sparseArray.clearEmptyCache();
        assertThat(sparseArray.getEmptyCacheSize()).isEqualTo(0);
    }

    @Test
    public void testReuseEmptySets() {
        MultiLongSparseArray<String> sparseArray = new MultiLongSparseArray<>();
        sparseArray.clearEmptyCache();
        assertThat(sparseArray.getEmptyCacheSize()).isEqualTo(0);
        // create a bunch of sets
        for (int i = 0; i <= MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT + 2; i++) {
            sparseArray.put(i, "foo");
        }
        // remove them so they are all put in the cache.
        for (int i = 0; i <= MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT + 2; i++) {
            sparseArray.remove(i, "foo");
        }
        assertThat(sparseArray.getEmptyCacheSize())
                .isEqualTo(MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT);

        // now create elements, that use the cached empty sets.
        for (int i = 0; i < MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT; i++) {
            sparseArray.put(10 + i, "bar");
            assertThat(sparseArray.getEmptyCacheSize())
                    .isEqualTo(MultiLongSparseArray.DEFAULT_MAX_EMPTIES_KEPT - i - 1);
        }
    }
}
