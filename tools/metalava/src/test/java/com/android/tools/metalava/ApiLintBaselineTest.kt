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

package com.android.tools.metalava

import org.junit.Test

/**
 * Test for [ApiLint] specifically with baseline arguments.
 */
class ApiLintBaselineTest : DriverTest() {
    @Test
    fun `Test with global baseline`() {
        check(
            apiLint = "", // enabled
            compatibilityMode = false,
            baseline = """
                // Baseline format: 1.0
                Enum: android.pkg.MyEnum:
                    Enums are discouraged in Android APIs
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;

                    public enum MyEnum {
                       FOO, BAR
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test with api-lint specific baseline`() {
        check(
            apiLint = "", // enabled
            compatibilityMode = false,
            baselineApiLint = """
                // Baseline format: 1.0
                Enum: android.pkg.MyEnum:
                    Enums are discouraged in Android APIs
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;

                    public enum MyEnum {
                       FOO, BAR
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test with api-lint specific baseline with update`() {
        check(
            apiLint = "", // enabled
            compatibilityMode = false,
            baselineApiLint = """
                """,
            updateBaselineApiLint = """
                // Baseline format: 1.0
                Enum: android.pkg.MyEnum:
                    Enums are discouraged in Android APIs
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;

                    public enum MyEnum {
                       FOO, BAR
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test with non-api-lint specific baseline`() {
        check(
            apiLint = "", // enabled
            compatibilityMode = false,
            baselineCheckCompatibilityReleased = """
                // Baseline format: 1.0
                Enum: android.pkg.MyEnum:
                    Enums are discouraged in Android APIs
            """,
            expectedIssues = """
                src/android/pkg/MyEnum.java:3: error: Enums are discouraged in Android APIs [Enum] [Rule F5 in go/android-api-guidelines]
                """,
            expectedFail = """
                1 new API lint issues were found.
                See tools/metalava/API-LINT.md for how to handle these.
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;

                    public enum MyEnum {
                       FOO, BAR
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test api-lint error message`() {
        check(
            apiLint = "", // enabled
            compatibilityMode = false,
            baselineApiLint = "",
            errorMessageApiLint = "*** api-lint failed ***",
            expectedIssues = """
                src/android/pkg/MyClassImpl.java:3: error: Don't expose your implementation details: `MyClassImpl` ends with `Impl` [EndsWithImpl]
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;

                    public class MyClassImpl {
                    }
                    """
                )
            ),
            expectedFail = """
                1 new API lint issues were found.
                See tools/metalava/API-LINT.md for how to handle these.
                *** api-lint failed ***
            """,
            expectedOutput = """
                1 new API lint issues were found.
                See tools/metalava/API-LINT.md for how to handle these.
                *** api-lint failed ***
                """
        )
    }

    @Test
    fun `Test no api-lint error message`() {
        check(
            apiLint = "", // enabled
            compatibilityMode = false,
            baselineApiLint = "",
            expectedIssues = """
                src/android/pkg/MyClassImpl.java:3: error: Don't expose your implementation details: `MyClassImpl` ends with `Impl` [EndsWithImpl]
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;

                    public class MyClassImpl {
                    }
                    """
                )
            ),
            expectedFail = """
                1 new API lint issues were found.
                See tools/metalava/API-LINT.md for how to handle these.
            """,
            expectedOutput = """
                1 new API lint issues were found.
                See tools/metalava/API-LINT.md for how to handle these.
                """
        )
    }
}