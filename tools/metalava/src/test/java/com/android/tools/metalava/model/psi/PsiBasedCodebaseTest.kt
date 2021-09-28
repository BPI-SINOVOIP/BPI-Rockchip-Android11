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

package com.android.tools.metalava.model.psi

import com.android.tools.lint.checks.infrastructure.TestFiles.base64gzip
import com.android.tools.metalava.DriverTest
import org.junit.Test

class PsiBasedCodebaseTest : DriverTest() {
    @Test
    fun `Regression test for issue 112931426`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg2;
                    public class Foo {
                        public test.pkg.ForeignClass getFoo(test.pkg.ForeignClass.ExtraCallback callback) {
                            return null;
                        }
                    }
                    """
                )
            ),
            classpath = arrayOf(
                /* The following source file, compiled, and root folder jar'ed and stored as base64 gzip:
                    package test.pkg;

                    public class ForeignClass {
                        public class ExtraCallback extends Something.Callback {
                            public void foo() {}
                        }

                        public static class Something {
                            public static class Callback {
                                public void foo() {}
                            }
                        }
                    }
                 */
                base64gzip(
                    "test.jar", "" +
                        "H4sIAAAAAAAAAAvwZmYRYeDg4GCYUiXhy4AEOBlYGHxdQxx1Pf3c9P+dYmBg" +
                        "ZgjwZucASTFBlQTg1CwCxHDNvo5+nm6uwSF6vm6ffc+c9vHW1bvI662rde7M" +
                        "+c1BBleMHzwt0vPy1fH0vVi6ioUz4oXkEekoCa2MH+Kqas+XaFk8Fxd9Iq46" +
                        "jeFq9qeij0WMYFdIfLSxdQHa4Qp1BRcDA9BlDWiuYAXiktTiEn3cSjhhSgqy" +
                        "0/UR/kFXpoeszC2/KDUzPc85J7G4WCU4Pze1JCMzL13FOTEnJykxOVsvGSTR" +
                        "G+DrfdhBoHZy3ZLO0NsZV2dz2F7hevYg8GOQBocr/9R75oW9t/PKVT3/xs9j" +
                        "4f/HUD/FonOriEgJf/6zs3vMrc/8Pv5ausFY5scKhYKNj5OmB+wPcHs6vcXr" +
                        "fYXTCr43c1Vy+qdMvN5dqXI5WjBzHYuyNLet4EqtH5sizsqHPEvqf9D7NsbI" +
                        "mHNhZWqPD3tUyswzHL2NF6yEPvZckH9qdPrw8Ughvlyt0CSv0PLUd3XZ5ysX" +
                        "Vmdz/ruhdymCL/RjfPXdUitR/0WdWlkt9+qfC0W16l45pPLQ12Rq4buk+QU/" +
                        "jjM4PQ9n7XvwwbL7eUNXqsTrM059RzZeq2e0nZ47fWOcmK2JxOx4prRomZ8v" +
                        "tD97nvx4+cV6yczMkgtaG8+WKjw4484ufUd3k2/FfFAsnq2qKldgZGD4xYic" +
                        "ltDDXpVw2EOCvBUW5MdqJN+2h/UHvVhYZcDe3zXdwZGjasWXfZ3ed8p2T1R5" +
                        "+/+TSsUH9h+Lj3iKfFUoYZczzr8/e+bd7/3XzRnELfguON4/2tjt3GSvHbap" +
                        "LkRNvcX82Amja4tWpTxN8viQtLBUa5PqwZ1Bblevt5x7UuK34fEjR6FdnUaf" +
                        "yvZ6pVbqB509o6BptPD5ohDfv763bBYq9UyKCiv9suXM4sxAr6mzy278SG/3" +
                        "djLQvpaqtnzVx6//T50TT1J2qvgQKyPot+2L0hcp5yWtJxfvLlHcPMvgZpLU" +
                        "f7uKH+tFlCP+1J9V5ItrM3yglZWd8PwN65czna2xbstVbVt6Hk5nqL6R/363" +
                        "7rcZjGxzInvMzEssih/WM4HCV1RV2JEXGLab8IavJs7wda0oKUpES9aBp/0O" +
                        "BQjYBnMt3XrCWfBoeM8MBZWkR0GH2zKSnO4k3Ij/0DX1UvatpD+64seOtMo/" +
                        "cJdc3toh1aP3zub8HOPJv7/9+8T1gCNaTShi24LFGQtSEnoncOXPWVDkobHV" +
                        "WbKld/fDBYXLal7y7Jmm3fRghqJ63/MWX6edv7jT9ntfiH7lJbQp9/hk8ceB" +
                        "/r+mL3petPRCWO3Dstb3Lj+3rHDf1HLmpfOL7mkFd+e94Of8tvO5p73a1Dv3" +
                        "Zq8P/2l2VFpt3dSk6N5vG6yy3nx/OlP45JP9fbJ3uqoTlTun8DJXMku33K3n" +
                        "kLihEvKc2TpL5adIfsBM2cQ6Mc6XkTstpjdu+MtuXPbjcsk/+1NPFz21vORQ" +
                        "4zhr7cnIycW9e0NX2zdmnb8gl9xz1OyoxIcvTLencnKZbXKQUX1sp9j+Z8pq" +
                        "uZcu79VdjZZtUD+r2drmlDN9w4l9Z7LVIlQO/w39lqp2xTKq/ScjKKr4XzX/" +
                        "CwFGUyMTvqiSxhVVkMjJDdjod9hAoPb6El5ezVzmpcpOBgzMkhKOOoJTJyhJ" +
                        "dO2yPsiVF8uuqvdo+4wJEj8Y7LgKOXlcV+na36/+/ubzz9e/HwsfYD1mJ9Mi" +
                        "V2b7plJCcNflJM23k40sFXeYuk3bKBaqqs3/u+Xe2dW6rjpb/Fy3KZSsmxL4" +
                        "bdaDOXPOceTF2Hg52Qe63Vs7baWOvMuqSM7JSsJVal6hn+0vPjUSTZwTEXnz" +
                        "ReVE98mLpmiZ+x71brLXDUtVU19Vxn998vPij0pMJQ+F3aT7hdm2ql869ORi" +
                        "rdX0maoyOVvk1m+t2XLz4/pcmUVH94fxPzrZn3AnTcxUJmP+3uZ7FhmKCW9M" +
                        "FjXLJX/IPsD3y7fuwoMJtg4nJp5efTa8ak9+Yx24ppi7O7NRABiwK8B5gJFJ" +
                        "hAG1xoLVZaDqDhWgVH7oWpErIBEUbbY4qj6QCVwMuCsqBDiMqLZwa+FE0fIM" +
                        "tRpDuBWkDbmY1UPRBiocSKvW0M1GLmJUUczuZCKy2EY3EjkraKIY+ZKZhJIK" +
                        "3VjkhCCNYuwUVry5KsCblQ2kjAMIzYBuegvmAQBYPJKrOQkAAA=="
                )
            ),
            api = """
                package test.pkg2 {
                  public class Foo {
                    ctor public Foo();
                    method public test.pkg.ForeignClass getFoo(test.pkg.ForeignClass.ExtraCallback);
                  }
                }
                """
        )
    }

    @Test
    fun `Invalid syntax`() {
        check(
            expectedIssues = """
                src/test/pkg/Foo.java:3: error: Syntax error: `'{' or ';' expected` [InvalidSyntax]
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class Foo {
                        public void foo()
                    }
                    """
                )
            )
        )
    }
}