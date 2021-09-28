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
import java.lang.RuntimeException
import kotlin.test.Test
import kotlin.test.assertEquals

class NdkVersionTest {
    @Test
    fun `can parse source properties`() {
        assertEquals(
            NdkVersion(20, 0, 5594570, null),
            NdkVersion.fromSourcePropertiesText(
                """
                Pkg.Desc = Android NDK
                Pkg.Revision = 20.0.5594570
                """.trimIndent()
            )
        )

        assertEquals(
            NdkVersion(20, 0, 5594570, "canary"),
            NdkVersion.fromSourcePropertiesText(
                """
                Pkg.Revision = 20.0.5594570-canary
                Pkg.Desc = Android NDK
                """.trimIndent()
            )
        )

        assertEquals(
            NdkVersion(20, 0, 5594570, "beta2"),
            NdkVersion.fromSourcePropertiesText(
                """

                    Pkg.Revision     =     20.0.5594570-beta2    
                Pkg.Desc = Android NDK

                """.trimIndent()
            )
        )
        assertEquals(
            NdkVersion(20, 0, 5594570, "rc1"),
            NdkVersion.fromSourcePropertiesText(
                """
                Pkg.Desc = Android NDK



                Pkg.Revision = 20.0.5594570-rc1
                """.trimIndent()
            )
        )
    }

    @Test
    fun `fails if not found`() {
        assertThrows<RuntimeException> {
            NdkVersion.fromSourcePropertiesText("Pkg.Desc = Android NDK")
        }
    }
}