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

package com.android.ndkports

import org.junit.jupiter.api.assertThrows
import kotlin.test.Test
import kotlin.test.assertEquals

class CMakeCompatibleVersionTest {
    @Test
    fun `can parse version number`() {
        assertEquals(
            CMakeCompatibleVersion(1, null, null, null),
            CMakeCompatibleVersion.parse("1")
        )
        assertEquals(
            CMakeCompatibleVersion(2, 1, null, null),
            CMakeCompatibleVersion.parse("2.1")
        )
        assertEquals(
            CMakeCompatibleVersion(3, 2, 1, null),
            CMakeCompatibleVersion.parse("3.2.1")
        )
        assertEquals(
            CMakeCompatibleVersion(4, 3, 2, 1),
            CMakeCompatibleVersion.parse("4.3.2.1")
        )
    }

    @Test
    fun `reject invalid versions`() {
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(" ")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("1.")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(".1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(".1.")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(" 1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("1 ")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(" 1 ")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("2.1.")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(".2.1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(".2.1.")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("1a")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("2b.1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("5.4.3.2.1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("4.3.2.1a")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("2.a.1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("3. .1")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("1..2")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse(".")
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion.parse("...")
        }
    }

    @Test
    fun `constructor requires that nulls come last`() {
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion(1, 2, null, 3)
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion(1, null, 2, 3)
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion(1, null, 2, null)
        }
        assertThrows<IllegalArgumentException> {
            CMakeCompatibleVersion(1, null, null, 2)
        }
    }
}