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

class ReporterTest : DriverTest() {
    @Test
    fun `Errors are sent to stderr`() {
        check(
            apiLint = "",
            expectedIssues = """
                src/test/pkg/foo.java:2: error: Class must start with uppercase char: foo [StartWithUpper] [Rule S1 in go/android-api-guidelines]
                src/test/pkg/foo.java:4: warning: If min/max could change in future, make them dynamic methods: test.pkg.foo#MAX_BAR [MinMaxConstant] [Rule C8 in go/android-api-guidelines]
            """,
            errorSeverityExpectedIssues = """
                src/test/pkg/foo.java:2: error: Class must start with uppercase char: foo [StartWithUpper] [Rule S1 in go/android-api-guidelines]
            """,
            expectedFail = """
                2 new API lint issues were found.
                See tools/metalava/API-LINT.md for how to handle these.
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class foo {
                        private foo() {}
                        public static final int MAX_BAR = 0;
                    }
                    """
                )
            )
        )
    }
}