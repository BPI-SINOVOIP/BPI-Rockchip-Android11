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

package com.android.tools.metalava.model.text

import com.android.tools.metalava.doclava1.ApiFile
import com.android.tools.metalava.model.text.TextTypeParameterItem.Companion.bounds

import com.google.common.truth.Truth.assertThat
import org.junit.Test

class TextTypeParameterItemTest {
    @Test
    fun testTypeParameterNames() {
        assertThat(bounds(null).toString()).isEqualTo("[]")
        assertThat(bounds("").toString()).isEqualTo("[]")
        assertThat(bounds("X").toString()).isEqualTo("[]")
        assertThat(bounds("DEF extends T").toString()).isEqualTo("[T]")
        assertThat(bounds("T extends java.lang.Comparable<? super T>").toString())
            .isEqualTo("[java.lang.Comparable]")
        assertThat(bounds("T extends java.util.List<Number> & java.util.RandomAccess").toString())
            .isEqualTo("[java.util.List, java.util.RandomAccess]")

        // When a type variable is on a member and the type variable is defined on the surrounding
        // class, look up the bound on the class type parameter:
        val codebase = ApiFile.parseApi(
            "test", """
            package androidx.navigation {
              public final class NavDestination {
                ctor public NavDestination();
              }
              public class NavDestinationBuilder<D extends androidx.navigation.NavDestination> {
                ctor public NavDestinationBuilder(int id);
                method public D build();
              }
            }
        """.trimIndent(), false
        )
        val cls = codebase.findClass("androidx.navigation.NavDestinationBuilder")
        val method = cls?.findMethod("build", "") as TextMethodItem
        assertThat(method).isNotNull()
        assertThat(bounds("D", method).toString()).isEqualTo("[androidx.navigation.NavDestination]")
    }
}