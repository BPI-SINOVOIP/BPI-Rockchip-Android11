/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tools.metalava

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class PackageFilterTest {
    @Test
    fun testExact() {
        val filter = PackageFilter.parse("foo.bar:bar.baz")
        assertThat(filter.matches("foo.bar")).isTrue()
        assertThat(filter.matches("bar.baz")).isTrue()
        assertThat(filter.matches("foo.bar.baz")).isFalse()
        assertThat(filter.matches("foo")).isFalse()
        assertThat(filter.matches("foo.barf")).isFalse()
    }

    @Test
    fun testWildcard() {
        val filter = PackageFilter.parse("foo.bar*:bar.baz.*")
        assertThat(filter.matches("foo.bar")).isTrue()
        assertThat(filter.matches("foo.bars")).isTrue()
        assertThat(filter.matches("foo.bar.baz")).isTrue()
        assertThat(filter.matches("bar.baz")).isTrue() // different from doclava behavior
        assertThat(filter.matches("bar.bazz")).isFalse()
        assertThat(filter.matches("foo.bar.baz")).isTrue()
        assertThat(filter.matches("foo")).isFalse()
    }

    @Test
    fun testBoth() {
        val filter = PackageFilter.parse("foo.bar:foo.bar.*")
        assertThat(filter.matches("foo.bar")).isTrue()
        assertThat(filter.matches("foo.bar.sub")).isTrue()
        assertThat(filter.matches("foo.bar.sub.sub")).isTrue()
        assertThat(filter.matches("foo.bars")).isFalse()
        assertThat(filter.matches("foo")).isFalse()
    }

    @Test
    fun testImplicit() {
        val filter = PackageFilter.parse("foo.bar.*")
        assertThat(filter.matches("foo.bar")).isTrue()
        assertThat(filter.matches("foo.bar.sub")).isTrue()
        assertThat(filter.matches("foo.bar.sub.sub")).isTrue()
        assertThat(filter.matches("foo.bars")).isFalse()
        assertThat(filter.matches("foo")).isFalse()
    }

    @Test
    fun testExplicitInclusion() {
        val filter = PackageFilter.parse("+sample")
        assertThat(filter.matches("sample")).isTrue()
        assertThat(filter.matches("samples")).isFalse()
        assertThat(filter.matches("sample.example")).isFalse()
        assertThat(filter.matches("other")).isFalse()
    }

    @Test
    fun testOnlyExclusion() {
        val filter = PackageFilter.parse("-com")
        assertThat(filter.matches("com")).isFalse()
        assertThat(filter.matches("org")).isFalse() // not implicitly included
        assertThat(filter.matches("company")).isFalse() // not implicitly included
    }

    @Test
    fun testExclusionOverride() {
        val filter = PackageFilter.parse("sample.*:-sample.example.*")
        assertThat(filter.matches("sample")).isTrue()
        assertThat(filter.matches("sample.other")).isTrue()
        assertThat(filter.matches("sample.example")).isFalse()
        assertThat(filter.matches("sample.example.one")).isFalse()
        assertThat(filter.matches("org")).isFalse()
    }

    @Test
    fun testMatchAll() {
        val star = PackageFilter.parse("*")
        assertThat(star.matches("sample")).isTrue()
        val plusStar = PackageFilter.parse("+*")
        assertThat(plusStar.matches("example")).isTrue()
    }

    @Test
    fun testMultipleOverlappingOverrides() {
        val filter = PackageFilter.parse("+*:-android.*:+android.icu.*:-dalvik.*")
        assertThat(filter.matches("android")).isFalse()
        assertThat(filter.matches("android.something")).isFalse()
        assertThat(filter.matches("android.icu")).isTrue()
        assertThat(filter.matches("android.icu.other")).isTrue()
        assertThat(filter.matches("dalvik")).isFalse()
        assertThat(filter.matches("dalvik.sub")).isFalse()
        assertThat(filter.matches("org")).isTrue()
    }
}
