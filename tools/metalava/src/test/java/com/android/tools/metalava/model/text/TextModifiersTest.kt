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

package com.android.tools.metalava.model.text

import com.android.tools.metalava.doclava1.ApiFile
import com.android.tools.metalava.model.DefaultModifierList
import org.junit.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class TextModifiersTest {

    @Test
    fun `test equivalentTo()`() {
        val codebase = ApiFile.parseApi(
            "test", """
            package androidx.navigation {
              public final class NavDestination {
                ctor public NavDestination();
              }
            }
        """.trimIndent(), false
        )

        assertTrue {
            TextModifiers(codebase, flags = DefaultModifierList.PUBLIC).equivalentTo(
                TextModifiers(codebase, flags = DefaultModifierList.PUBLIC))
        }
        assertFalse {
            TextModifiers(codebase, flags = DefaultModifierList.PRIVATE).equivalentTo(
                TextModifiers(codebase, flags = DefaultModifierList.PUBLIC))
        }
    }
}
