/*
 * Copyright (C) 2017 The Android Open Source Project
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

@SuppressWarnings("ALL") // Sample code
class ExtractAnnotationsTest : DriverTest() {

    private val sourceFiles1 = arrayOf(
        java(
            """
                    package test.pkg;

                    import android.annotation.IntDef;
                    import android.annotation.IntRange;

                    import java.lang.annotation.Retention;
                    import java.lang.annotation.RetentionPolicy;

                    @SuppressWarnings({"UnusedDeclaration", "WeakerAccess"})
                    public class IntDefTest {
                        @IntDef({STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT})
                        @IntRange(from = 20)
                        private @interface DialogStyle {}

                        public static final int STYLE_NORMAL = 0;
                        public static final int STYLE_NO_TITLE = 1;
                        public static final int STYLE_NO_FRAME = 2;
                        public static final int STYLE_NO_INPUT = 3;
                        public static final int UNRELATED = 3;

                        public void setStyle(@DialogStyle int style, int theme) {
                        }

                        public void testIntDef(int arg) {
                        }
                        @IntDef(value = {STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT, 3, 3 + 1}, flag=true)
                        @Retention(RetentionPolicy.SOURCE)
                        private @interface DialogFlags {}

                        public void setFlags(Object first, @DialogFlags int flags) {
                        }

                        public static final String TYPE_1 = "type1";
                        public static final String TYPE_2 = "type2";
                        public static final String UNRELATED_TYPE = "other";

                        public static class Inner {
                            public void setInner(@DialogFlags int flags) {
                            }
                        }
                    }
                    """
        ).indented(),
        intDefAnnotationSource,
        intRangeAnnotationSource
    )

    @Test
    fun `Check java typedef extraction and warning about non-source retention of typedefs`() {
        check(
            includeSourceRetentionAnnotations = false,
            format = FileFormat.V2,
            sourceFiles = sourceFiles1,
            expectedIssues = "src/test/pkg/IntDefTest.java:11: error: This typedef annotation class should have @Retention(RetentionPolicy.SOURCE) [AnnotationExtraction]",
            extractAnnotations = mapOf(
                "test.pkg" to """
                <?xml version="1.0" encoding="UTF-8"?>
                <root>
                  <item name="test.pkg.IntDefTest void setFlags(java.lang.Object, int) 1">
                    <annotation name="androidx.annotation.IntDef">
                      <val name="value" val="{test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT, 3, 4}" />
                      <val name="flag" val="true" />
                    </annotation>
                  </item>
                  <item name="test.pkg.IntDefTest void setStyle(int, int) 0">
                    <annotation name="androidx.annotation.IntDef">
                      <val name="value" val="{test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT}" />
                    </annotation>
                  </item>
                  <item name="test.pkg.IntDefTest.Inner void setInner(int) 0">
                    <annotation name="androidx.annotation.IntDef">
                      <val name="value" val="{test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT, 3, 4}" />
                      <val name="flag" val="true" />
                    </annotation>
                  </item>
                </root>
                """
            )
        )
    }

    @Test
    fun `Check Kotlin and referencing hidden constants from typedef`() {
        check(
            includeSourceRetentionAnnotations = false,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    @file:Suppress("unused", "UseExpressionBody")

                    package test.pkg

                    import android.annotation.LongDef

                    const val STYLE_NORMAL = 0L
                    const val STYLE_NO_TITLE = 1L
                    const val STYLE_NO_FRAME = 2L
                    const val STYLE_NO_INPUT = 3L
                    const val UNRELATED = 3L
                    private const val HIDDEN = 4

                    const val TYPE_1 = "type1"
                    const val TYPE_2 = "type2"
                    const val UNRELATED_TYPE = "other"

                    class LongDefTest {

                        /** @hide */
                        @LongDef(STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT, HIDDEN)
                        @Retention(AnnotationRetention.SOURCE)
                        private annotation class DialogStyle

                        fun setStyle(@DialogStyle style: Int, theme: Int) {}

                        fun testLongDef(arg: Int) {
                        }

                        @LongDef(STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT, 3L, 3L + 1L, flag = true)
                        @Retention(AnnotationRetention.SOURCE)
                        private annotation class DialogFlags

                        fun setFlags(first: Any, @DialogFlags flags: Int) {}

                        class Inner {
                            fun setInner(@DialogFlags flags: Int) {}
                            fun isNull(value: String?): Boolean
                        }
                    }"""
                ).indented(),
                longDefAnnotationSource
            ),
            expectedIssues = "src/test/pkg/LongDefTest.kt:12: error: Typedef class references hidden field field LongDefTestKt.HIDDEN: removed from typedef metadata [HiddenTypedefConstant]",
            extractAnnotations = mapOf(
                "test.pkg" to """
                    <?xml version="1.0" encoding="UTF-8"?>
                    <root>
                      <item name="test.pkg.LongDefTest void setFlags(java.lang.Object, int) 1">
                        <annotation name="androidx.annotation.LongDef">
                          <val name="flag" val="true" />
                          <val name="value" val="{test.pkg.LongDefTestKt.STYLE_NORMAL, test.pkg.LongDefTestKt.STYLE_NO_TITLE, test.pkg.LongDefTestKt.STYLE_NO_FRAME, test.pkg.LongDefTestKt.STYLE_NO_INPUT, 3, 4L}" />
                        </annotation>
                      </item>
                      <item name="test.pkg.LongDefTest void setStyle(int, int) 0">
                        <annotation name="androidx.annotation.LongDef">
                          <val name="value" val="{test.pkg.LongDefTestKt.STYLE_NORMAL, test.pkg.LongDefTestKt.STYLE_NO_TITLE, test.pkg.LongDefTestKt.STYLE_NO_FRAME, test.pkg.LongDefTestKt.STYLE_NO_INPUT}" />
                        </annotation>
                      </item>
                      <item name="test.pkg.LongDefTest.Inner void setInner(int) 0">
                        <annotation name="androidx.annotation.LongDef">
                          <val name="flag" val="true" />
                          <val name="value" val="{test.pkg.LongDefTestKt.STYLE_NORMAL, test.pkg.LongDefTestKt.STYLE_NO_TITLE, test.pkg.LongDefTestKt.STYLE_NO_FRAME, test.pkg.LongDefTestKt.STYLE_NO_INPUT, 3, 4L}" />
                        </annotation>
                      </item>
                    </root>
                """
            )
        )
    }

    @Test
    fun `Check including only class retention annotations other than typedefs`() {
        check(
            includeSourceRetentionAnnotations = true,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    @file:Suppress("unused", "UseExpressionBody")

                    package test.pkg

                    import android.annotation.LongDef

                    const val STYLE_NORMAL = 0L
                    const val STYLE_NO_TITLE = 1L
                    const val STYLE_NO_FRAME = 2L
                    const val STYLE_NO_INPUT = 3L
                    const val UNRELATED = 3L
                    private const val HIDDEN = 4

                    const val TYPE_1 = "type1"
                    const val TYPE_2 = "type2"
                    const val UNRELATED_TYPE = "other"

                    class LongDefTest {

                        /** @hide */
                        @LongDef(STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT, HIDDEN)
                        @Retention(AnnotationRetention.SOURCE)
                        private annotation class DialogStyle

                        fun setStyle(@DialogStyle style: Int, theme: Int) {}

                        fun testLongDef(arg: Int) {
                        }

                        @LongDef(STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT, 3L, 3L + 1L, flag = true)
                        @Retention(AnnotationRetention.SOURCE)
                        private annotation class DialogFlags

                        fun setFlags(first: Any, @DialogFlags flags: Int) {}

                        class Inner {
                            fun setInner(@DialogFlags flags: Int) {}
                            fun isNull(value: String?): Boolean
                        }
                    }"""
                ).indented(),
                longDefAnnotationSource
            ),
            expectedIssues = "src/test/pkg/LongDefTest.kt:12: error: Typedef class references hidden field field LongDefTestKt.HIDDEN: removed from typedef metadata [HiddenTypedefConstant]",
            extractAnnotations = mapOf(
                "test.pkg" to """
                    <?xml version="1.0" encoding="UTF-8"?>
                    <root>
                      <item name="test.pkg.LongDefTest void setFlags(java.lang.Object, int) 1">
                        <annotation name="androidx.annotation.LongDef">
                          <val name="flag" val="true" />
                          <val name="value" val="{test.pkg.LongDefTestKt.STYLE_NORMAL, test.pkg.LongDefTestKt.STYLE_NO_TITLE, test.pkg.LongDefTestKt.STYLE_NO_FRAME, test.pkg.LongDefTestKt.STYLE_NO_INPUT, 3, 4L}" />
                        </annotation>
                      </item>
                      <item name="test.pkg.LongDefTest void setStyle(int, int) 0">
                        <annotation name="androidx.annotation.LongDef">
                          <val name="value" val="{test.pkg.LongDefTestKt.STYLE_NORMAL, test.pkg.LongDefTestKt.STYLE_NO_TITLE, test.pkg.LongDefTestKt.STYLE_NO_FRAME, test.pkg.LongDefTestKt.STYLE_NO_INPUT}" />
                        </annotation>
                      </item>
                      <item name="test.pkg.LongDefTest.Inner void setInner(int) 0">
                        <annotation name="androidx.annotation.LongDef">
                          <val name="flag" val="true" />
                          <val name="value" val="{test.pkg.LongDefTestKt.STYLE_NORMAL, test.pkg.LongDefTestKt.STYLE_NO_TITLE, test.pkg.LongDefTestKt.STYLE_NO_FRAME, test.pkg.LongDefTestKt.STYLE_NO_INPUT, 3, 4L}" />
                        </annotation>
                      </item>
                    </root>
                """
            )
        )
    }

    @Test
    fun `Extract permission annotations`() {
        check(
            includeSourceRetentionAnnotations = false,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.annotation.RequiresPermission;

                    public class PermissionsTest {
                        @RequiresPermission(Manifest.permission.MY_PERMISSION)
                        public void myMethod() {
                        }
                        @RequiresPermission(anyOf={Manifest.permission.MY_PERMISSION,Manifest.permission.MY_PERMISSION2})
                        public void myMethod2() {
                        }

                        @RequiresPermission.Read(@RequiresPermission(Manifest.permission.MY_READ_PERMISSION))
                        @RequiresPermission.Write(@RequiresPermission(Manifest.permission.MY_WRITE_PERMISSION))
                        public static final String CONTENT_URI = "";
                    }
                    """
                ).indented(),
                java(
                    """
                    package test.pkg;

                    public class Manifest {
                        public static final class permission {
                            public static final String MY_PERMISSION = "android.permission.MY_PERMISSION_STRING";
                            public static final String MY_PERMISSION2 = "android.permission.MY_PERMISSION_STRING2";
                            public static final String MY_READ_PERMISSION = "android.permission.MY_READ_PERMISSION_STRING";
                            public static final String MY_WRITE_PERMISSION = "android.permission.MY_WRITE_PERMISSION_STRING";
                        }
                    }
                    """
                ).indented(),
                requiresPermissionSource
            ),
            extractAnnotations = mapOf(
                "test.pkg" to """
                <?xml version="1.0" encoding="UTF-8"?>
                <root>
                  <item name="test.pkg.PermissionsTest CONTENT_URI">
                    <annotation name="androidx.annotation.RequiresPermission.Read">
                      <val name="value" val="&quot;android.permission.MY_READ_PERMISSION_STRING&quot;" />
                    </annotation>
                    <annotation name="androidx.annotation.RequiresPermission.Write">
                      <val name="value" val="&quot;android.permission.MY_WRITE_PERMISSION_STRING&quot;" />
                    </annotation>
                  </item>
                  <item name="test.pkg.PermissionsTest void myMethod()">
                    <annotation name="androidx.annotation.RequiresPermission">
                      <val name="value" val="&quot;android.permission.MY_PERMISSION_STRING&quot;" />
                    </annotation>
                  </item>
                  <item name="test.pkg.PermissionsTest void myMethod2()">
                    <annotation name="androidx.annotation.RequiresPermission">
                      <val name="anyOf" val="{&quot;android.permission.MY_PERMISSION_STRING&quot;, &quot;android.permission.MY_PERMISSION_STRING2&quot;}" />
                    </annotation>
                  </item>
                </root>
                """
            )
        )
    }

    @Test
    fun `Include merged annotations in exported source annotations`() {
        check(
            includeSourceRetentionAnnotations = true,
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            includeSystemApiAnnotations = false,
            omitCommonPackages = false,
            expectedIssues = "error: Unexpected reference to Nonexistent.Field [InternalError]",
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class MyTest {
                        public void test(int arg) { }
                    }"""
                ),
                java(
                    """
                        package java.util;
                        public class Calendar {
                            public static final int ERA = 1;
                            public static final int YEAR = 2;
                            public static final int MONTH = 3;
                            public static final int WEEK_OF_YEAR = 4;
                        }
                    """
                )
            ),
            mergeXmlAnnotations = """<?xml version="1.0" encoding="UTF-8"?>
                <root>
                  <item name="test.pkg.MyTest void test(int) 0">
                    <annotation name="org.intellij.lang.annotations.MagicConstant">
                      <val name="intValues" val="{java.util.Calendar.ERA, java.util.Calendar.YEAR, java.util.Calendar.MONTH, java.util.Calendar.WEEK_OF_YEAR, Nonexistent.Field}" />
                    </annotation>
                  </item>
                </root>
                """,
            extractAnnotations = mapOf(
                "test.pkg" to """
                <?xml version="1.0" encoding="UTF-8"?>
                <root>
                  <item name="test.pkg.MyTest void test(int) 0">
                    <annotation name="androidx.annotation.IntDef">
                      <val name="value" val="{java.util.Calendar.ERA, java.util.Calendar.YEAR, java.util.Calendar.MONTH, java.util.Calendar.WEEK_OF_YEAR}" />
                    </annotation>
                  </item>
                </root>
                """
            )
        )
    }

    @Test
    fun `Only including class retention annotations in stubs`() {
        check(
            includeSourceRetentionAnnotations = false,
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            includeSystemApiAnnotations = false,
            omitCommonPackages = false,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    import androidx.annotation.RecentlyNullable;
                    public class Test {
                        @RecentlyNullable
                        public static String sayHello(@IntRange(from = 10) int value) { return "hello " + value; }
                    }
                    """
                ),
                intRangeAnnotationSource,
                recentlyNullableSource
            ),
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                public Test() { throw new RuntimeException("Stub!"); }
                @androidx.annotation.RecentlyNullable
                public static java.lang.String sayHello(int value) { throw new RuntimeException("Stub!"); }
                }
                """
            ),
            extractAnnotations = mapOf(
                "test.pkg" to """
                    <?xml version="1.0" encoding="UTF-8"?>
                    <root>
                      <item name="test.pkg.Test java.lang.String sayHello(int) 0">
                        <annotation name="androidx.annotation.IntRange">
                          <val name="from" val="10" />
                        </annotation>
                      </item>
                    </root>
                """
            )
        )
    }

    @Test
    fun `Check warning about unexpected returns from typedef method`() {
        check(
            includeSourceRetentionAnnotations = false,
            expectedIssues = "src/test/pkg/IntDefTest.java:36: warning: Returning unexpected constant UNRELATED; is @DialogStyle missing this constant? Expected one of STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT [ReturningUnexpectedConstant]",
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.annotation.IntDef;
                    import android.annotation.IntRange;
                    import java.lang.annotation.Retention;
                    import java.lang.annotation.RetentionPolicy;

                    @SuppressWarnings({"UnusedDeclaration", "WeakerAccess"})
                    public class IntDefTest {
                        @IntDef({STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT})
                        @IntRange(from = 20)
                        @Retention(RetentionPolicy.SOURCE)
                        private @interface DialogStyle {
                        }

                        public static final int STYLE_NORMAL = 0;
                        public static final int STYLE_NO_TITLE = 1;
                        public static final int STYLE_NO_FRAME = 2;
                        public static final int STYLE_NO_INPUT = 3;
                        public static final int UNRELATED = 3;
                        public static final int[] EMPTY_ARRAY = new int[0];

                        private int mField1 = 4;
                        private int mField2 = 5;

                        @DialogStyle
                        public int getStyle1() {
                            //noinspection ConstantConditions
                            if (mField1 < 1) {
                                return STYLE_NO_TITLE; // OK
                            } else if (mField1 < 2) {
                                return 0; // OK
                            } else if (mField1 < 3) {
                                return mField2; // OK
                            } else {
                                return UNRELATED; // WARN
                            }
                        }

                        @DialogStyle
                        public int[] getStyle2() {
                            return EMPTY_ARRAY; // OK
                        }
                    }
                    """
                ).indented(),
                intDefAnnotationSource
            ),
            extractAnnotations = mapOf(
                "test.pkg" to """
                <?xml version="1.0" encoding="UTF-8"?>
                <root>
                  <item name="test.pkg.IntDefTest int getStyle1()">
                    <annotation name="androidx.annotation.IntDef">
                      <val name="value" val="{test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT}" />
                    </annotation>
                  </item>
                  <item name="test.pkg.IntDefTest int[] getStyle2()">
                    <annotation name="androidx.annotation.IntDef">
                      <val name="value" val="{test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT}" />
                    </annotation>
                  </item>
                </root>
                """
            )
        )
    }

    @Test
    fun `No typedef signatures in api files`() {
        check(
            includeSourceRetentionAnnotations = false,
            extraArguments = arrayOf(
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_TYPEDEFS_IN_SIGNATURES, "none"
            ),
            format = FileFormat.V2,
            sourceFiles = sourceFiles1,
            api = """
                // Signature format: 2.0
                package test.pkg {
                  public class IntDefTest {
                    ctor public IntDefTest();
                    method public void setFlags(Object, int);
                    method public void setStyle(int, int);
                    method public void testIntDef(int);
                    field public static final int STYLE_NORMAL = 0; // 0x0
                    field public static final int STYLE_NO_FRAME = 2; // 0x2
                    field public static final int STYLE_NO_INPUT = 3; // 0x3
                    field public static final int STYLE_NO_TITLE = 1; // 0x1
                    field public static final String TYPE_1 = "type1";
                    field public static final String TYPE_2 = "type2";
                    field public static final int UNRELATED = 3; // 0x3
                    field public static final String UNRELATED_TYPE = "other";
                  }
                  public static class IntDefTest.Inner {
                    ctor public IntDefTest.Inner();
                    method public void setInner(int);
                  }
                }
            """
        )
    }

    @Test
    fun `Inlining typedef signatures in api files`() {
        check(
            includeSourceRetentionAnnotations = false,
            extraArguments = arrayOf(
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_TYPEDEFS_IN_SIGNATURES, "inline"
            ),
            format = FileFormat.V2,
            sourceFiles = sourceFiles1,
            api = """
                // Signature format: 2.0
                package test.pkg {
                  public class IntDefTest {
                    ctor public IntDefTest();
                    method public void setFlags(Object, @IntDef(value={test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT, 3, 3 + 1}, flag=true) int);
                    method public void setStyle(@IntDef({test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT}) int, int);
                    method public void testIntDef(int);
                    field public static final int STYLE_NORMAL = 0; // 0x0
                    field public static final int STYLE_NO_FRAME = 2; // 0x2
                    field public static final int STYLE_NO_INPUT = 3; // 0x3
                    field public static final int STYLE_NO_TITLE = 1; // 0x1
                    field public static final String TYPE_1 = "type1";
                    field public static final String TYPE_2 = "type2";
                    field public static final int UNRELATED = 3; // 0x3
                    field public static final String UNRELATED_TYPE = "other";
                  }
                  public static class IntDefTest.Inner {
                    ctor public IntDefTest.Inner();
                    method public void setInner(@IntDef(value={test.pkg.IntDefTest.STYLE_NORMAL, test.pkg.IntDefTest.STYLE_NO_TITLE, test.pkg.IntDefTest.STYLE_NO_FRAME, test.pkg.IntDefTest.STYLE_NO_INPUT, 3, 3 + 1}, flag=true) int);
                  }
                }
            """
        )
    }

    @Test
    fun `Referencing typedef signatures in api files`() {
        check(
            includeSourceRetentionAnnotations = false,
            extraArguments = arrayOf(
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_TYPEDEFS_IN_SIGNATURES, "ref"
            ),
            format = FileFormat.V2,
            sourceFiles = sourceFiles1,
            api = """
                // Signature format: 2.0
                package test.pkg {
                  public class IntDefTest {
                    ctor public IntDefTest();
                    method public void setFlags(Object, @DialogFlags int);
                    method public void setStyle(@DialogStyle int, int);
                    method public void testIntDef(int);
                    field public static final int STYLE_NORMAL = 0; // 0x0
                    field public static final int STYLE_NO_FRAME = 2; // 0x2
                    field public static final int STYLE_NO_INPUT = 3; // 0x3
                    field public static final int STYLE_NO_TITLE = 1; // 0x1
                    field public static final String TYPE_1 = "type1";
                    field public static final String TYPE_2 = "type2";
                    field public static final int UNRELATED = 3; // 0x3
                    field public static final String UNRELATED_TYPE = "other";
                  }
                  public static class IntDefTest.Inner {
                    ctor public IntDefTest.Inner();
                    method public void setInner(@DialogFlags int);
                  }
                }
            """
        )
    }
}