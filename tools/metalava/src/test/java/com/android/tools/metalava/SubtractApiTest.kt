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

@file:Suppress("ALL")

package com.android.tools.metalava

import org.junit.Test

class SubtractApiTest : DriverTest() {
    @Test
    fun `Subtract APIs`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class OnlyInNew {
                        private OnlyInNew() { }
                        public void method1() { }
                        public void method5() { }
                        public void method6() { }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class InBoth {
                        private InBoth() { }
                        public void method1() { }
                        public void method5() { }
                        public void method9() { }
                    }
                    """
                )
            ),
            subtractApi = """
                package test.pkg {
                  public class InBoth {
                    method public void method1();
                    method public void method5();
                    method public void method9();
                  }
                  public class OnlyInOld {
                    method public void method1();
                    method public void method2();
                    method public void method3();
                  }
                }
                """,
            api = """
                package test.pkg {
                  public class OnlyInNew {
                    method public void method1();
                    method public void method5();
                    method public void method6();
                  }
                }
                """,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class OnlyInNew {
                private OnlyInNew() { throw new RuntimeException("Stub!"); }
                public void method1() { throw new RuntimeException("Stub!"); }
                public void method5() { throw new RuntimeException("Stub!"); }
                public void method6() { throw new RuntimeException("Stub!"); }
                }
                """
            ),
            stubsSourceList = """
                TESTROOT/stubs/test/pkg/OnlyInNew.java
            """
        )
    }
}
