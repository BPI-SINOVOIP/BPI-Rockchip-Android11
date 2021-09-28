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

import org.junit.Ignore
import org.junit.Test
import java.io.File
import kotlin.text.Charsets.UTF_8

class
CompatibilityCheckTest : DriverTest() {
    @Test
    fun `Change between class and interface`() {
        check(
            expectedIssues = """
                TESTROOT/load-api.txt:2: error: Class test.pkg.MyTest1 changed class/interface declaration [ChangedClass]
                TESTROOT/load-api.txt:4: error: Class test.pkg.MyTest2 changed class/interface declaration [ChangedClass]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class MyTest1 {
                  }
                  public interface MyTest2 {
                  }
                  public class MyTest3 {
                  }
                  public interface MyTest4 {
                  }
                }
                """,
            // MyTest1 and MyTest2 reversed from class to interface or vice versa, MyTest3 and MyTest4 unchanged
            signatureSource = """
                package test.pkg {
                  public interface MyTest1 {
                  }
                  public class MyTest2 {
                  }
                  public class MyTest3 {
                  }
                  public interface MyTest4 {
                  }
                }
                """
        )
    }

    @Test
    fun `Interfaces should not be dropped`() {
        check(
            expectedIssues = """
                TESTROOT/load-api.txt:2: error: Class test.pkg.MyTest1 changed class/interface declaration [ChangedClass]
                TESTROOT/load-api.txt:4: error: Class test.pkg.MyTest2 changed class/interface declaration [ChangedClass]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class MyTest1 {
                  }
                  public interface MyTest2 {
                  }
                  public class MyTest3 {
                  }
                  public interface MyTest4 {
                  }
                }
                """,
            // MyTest1 and MyTest2 reversed from class to interface or vice versa, MyTest3 and MyTest4 unchanged
            signatureSource = """
                package test.pkg {
                  public interface MyTest1 {
                  }
                  public class MyTest2 {
                  }
                  public class MyTest3 {
                  }
                  public interface MyTest4 {
                  }
                }
                """
        )
    }

    @Test
    fun `Ensure warnings for removed APIs`() {
        check(
            expectedIssues = """
                TESTROOT/current-api.txt:3: error: Removed method test.pkg.MyTest1.method(Float) [RemovedMethod]
                TESTROOT/current-api.txt:4: error: Removed field test.pkg.MyTest1.field [RemovedField]
                TESTROOT/current-api.txt:6: error: Removed class test.pkg.MyTest2 [RemovedClass]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class MyTest1 {
                    method public Double method(Float);
                    field public Double field;
                  }
                  public class MyTest2 {
                    method public Double method(Float);
                    field public Double field;
                  }
                }
                package test.pkg.other {
                }
                """,
            signatureSource = """
                package test.pkg {
                  public class MyTest1 {
                  }
                }
                """
        )
    }

    @Test
    fun `Flag invalid nullness changes`() {
        check(
            expectedIssues = """
                TESTROOT/load-api.txt:5: error: Attempted to remove @Nullable annotation from method test.pkg.MyTest.convert3(Float) [InvalidNullConversion]
                TESTROOT/load-api.txt:5: error: Attempted to remove @Nullable annotation from parameter arg1 in test.pkg.MyTest.convert3(Float arg1) [InvalidNullConversion]
                TESTROOT/load-api.txt:6: error: Attempted to remove @NonNull annotation from method test.pkg.MyTest.convert4(Float) [InvalidNullConversion]
                TESTROOT/load-api.txt:6: error: Attempted to remove @NonNull annotation from parameter arg1 in test.pkg.MyTest.convert4(Float arg1) [InvalidNullConversion]
                TESTROOT/load-api.txt:7: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter arg1 in test.pkg.MyTest.convert5(Float arg1) [InvalidNullConversion]
                TESTROOT/load-api.txt:8: error: Attempted to change method return from @NonNull to @Nullable: incompatible change for method test.pkg.MyTest.convert6(Float) [InvalidNullConversion]
                """,
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class MyTest {
                    method public Double convert1(Float);
                    method public Double convert2(Float);
                    method @Nullable public Double convert3(@Nullable Float);
                    method @NonNull public Double convert4(@NonNull Float);
                    method @Nullable public Double convert5(@Nullable Float);
                    method @NonNull public Double convert6(@NonNull Float);
                    // booleans cannot reasonably be annotated with @Nullable/@NonNull but
                    // the compiler accepts it and we had a few of these accidentally annotated
                    // that way in API 28, such as Boolean.getBoolean. Make sure we don't flag
                    // these as incompatible changes when they're dropped.
                    method public void convert7(@NonNull boolean);
                  }
                }
                """,
            // Changes: +nullness, -nullness, nullable->nonnull, nonnull->nullable
            signatureSource = """
                package test.pkg {
                  public class MyTest {
                    method @Nullable public Double convert1(@Nullable Float);
                    method @NonNull public Double convert2(@NonNull Float);
                    method public Double convert3(Float);
                    method public Double convert4(Float);
                    method @NonNull public Double convert5(@NonNull Float);
                    method @Nullable public Double convert6(@Nullable Float);
                    method public void convert7(boolean);
                  }
                }
                """
        )
    }

    @Test
    fun `Kotlin Nullness`() {
        check(
            expectedIssues = """
                src/test/pkg/Outer.kt:5: error: Attempted to change method return from @NonNull to @Nullable: incompatible change for method test.pkg.Outer.method2(String,String) [InvalidNullConversion]
                src/test/pkg/Outer.kt:5: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter string in test.pkg.Outer.method2(String string, String maybeString) [InvalidNullConversion]
                src/test/pkg/Outer.kt:6: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter string in test.pkg.Outer.method3(String maybeString, String string) [InvalidNullConversion]
                src/test/pkg/Outer.kt:8: error: Attempted to change method return from @NonNull to @Nullable: incompatible change for method test.pkg.Outer.Inner.method2(String,String) [InvalidNullConversion]
                src/test/pkg/Outer.kt:8: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter string in test.pkg.Outer.Inner.method2(String string, String maybeString) [InvalidNullConversion]
                src/test/pkg/Outer.kt:9: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter string in test.pkg.Outer.Inner.method3(String maybeString, String string) [InvalidNullConversion]
                """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                    package test.pkg {
                      public final class Outer {
                        ctor public Outer();
                        method public final String? method1(String, String?);
                        method public final String method2(String?, String);
                        method public final String? method3(String, String?);
                      }
                      public static final class Outer.Inner {
                        ctor public Outer.Inner();
                        method public final String method2(String?, String);
                        method public final String? method3(String, String?);
                      }
                    }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    class Outer {
                        fun method1(string: String, maybeString: String?): String? = null
                        fun method2(string: String, maybeString: String?): String? = null
                        fun method3(maybeString: String?, string : String): String = ""
                        class Inner {
                            fun method2(string: String, maybeString: String?): String? = null
                            fun method3(maybeString: String?, string : String): String = ""
                        }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Java Parameter Name Change`() {
        check(
            expectedIssues = """
                src/test/pkg/JavaClass.java:6: error: Attempted to remove parameter name from parameter newName in test.pkg.JavaClass.method1 in method test.pkg.JavaClass.method1 [ParameterNameChange]
                src/test/pkg/JavaClass.java:7: error: Attempted to change parameter name from secondParameter to newName in method test.pkg.JavaClass.method2 [ParameterNameChange]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class JavaClass {
                    ctor public JavaClass();
                    method public String method1(String parameterName);
                    method public String method2(String firstParameter, String secondParameter);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    @Suppress("all")
                    package test.pkg;
                    import androidx.annotation.ParameterName;

                    public class JavaClass {
                        public String method1(String newName) { return null; }
                        public String method2(@ParameterName("firstParameter") String s, @ParameterName("newName") String prevName) { return null; }
                    }
                    """
                ),
                supportParameterName
            ),
            extraArguments = arrayOf(ARG_HIDE_PACKAGE, "androidx.annotation")
        )
    }

    @Test
    fun `Kotlin Parameter Name Change`() {
        check(
            expectedIssues = """
                src/test/pkg/KotlinClass.kt:4: error: Attempted to change parameter name from prevName to newName in method test.pkg.KotlinClass.method1 [ParameterNameChange]
                """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class KotlinClass {
                    ctor public KotlinClass();
                    method public final String? method1(String prevName);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    class KotlinClass {
                        fun method1(newName: String): String? = null
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Kotlin Coroutines`() {
        check(
            expectedIssues = "",
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class TestKt {
                    ctor public TestKt();
                    method public static suspend inline java.lang.Object hello(kotlin.coroutines.experimental.Continuation<? super kotlin.Unit>);
                  }
                }
                """,
            signatureSource = """
                package test.pkg {
                  public final class TestKt {
                    ctor public TestKt();
                    method public static suspend inline Object hello(@NonNull kotlin.coroutines.Continuation<? super kotlin.Unit> p);
                  }
                }
                """
        )
    }

    @Test
    fun `Add flag new methods but not overrides from platform`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:6: error: Added method test.pkg.MyClass.method2(String) [AddedMethod]
                src/test/pkg/MyClass.java:7: error: Added field test.pkg.MyClass.newField [AddedField]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class MyClass {
                    method public String method1(String);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class MyClass  {
                        private MyClass() { }
                        public String method1(String newName) { return null; }
                        public String method2(String newName) { return null; }
                        public int newField = 5;
                        public String toString() { return "Hello World"; }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Remove operator`() {
        check(
            expectedIssues = """
                src/test/pkg/Foo.kt:4: error: Cannot remove `operator` modifier from method test.pkg.Foo.plus(String): Incompatible change [OperatorRemoval]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class Foo {
                    ctor public Foo();
                    method public final operator void plus(String s);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    class Foo {
                        fun plus(s: String) { }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Remove vararg`() {
        check(
            expectedIssues = """
                src/test/pkg/test.kt:3: error: Changing from varargs to array is an incompatible change: parameter x in test.pkg.TestKt.method2(int[] x) [VarargRemoval]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class TestKt {
                    method public static final void method1(int[] x);
                    method public static final void method2(int... x);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg
                    fun method1(vararg x: Int) { }
                    fun method2(x: IntArray) { }
                    """
                )
            )
        )
    }

    @Test
    fun `Add final`() {
        // Adding final on class or method is incompatible; adding it on a parameter is fine.
        // Field is iffy.
        check(
            expectedIssues = """
                src/test/pkg/Java.java:4: error: Method test.pkg.Java.method has added 'final' qualifier [AddedFinal]
                src/test/pkg/Kotlin.kt:4: error: Method test.pkg.Kotlin.method has added 'final' qualifier [AddedFinal]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Java {
                    method public void method(int);
                  }
                  public class Kotlin {
                    ctor public Kotlin();
                    method public void method(String s);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    open class Kotlin {
                        fun method(s: String) { }
                    }
                    """
                ),
                java(
                    """
                        package test.pkg;
                        public class Java {
                            private Java() { }
                            public final void method(final int parameter) { }
                        }
                        """
                )
            )
        )
    }

    @Test
    fun `Inherited final`() {
        // Make sure that we correctly compare effectively final (inherited from surrounding class)
        // between the signature file codebase and the real codebase
        check(
            expectedIssues = """
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class Cls extends test.pkg.Parent {
                  }
                  public class Parent {
                    method public void method(int);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;
                        public final class Cls extends Parent {
                            private Cls() { }
                            @Override public void method(final int parameter) { }
                        }
                        """
                ),
                java(
                    """
                        package test.pkg;
                        public class Parent {
                            private Parent() { }
                            public void method(final int parameter) { }
                        }
                        """
                )
            )
        )
    }

    @Test
    fun `Implicit concrete`() {
        // Doclava signature files sometimes leave out overridden methods of
        // abstract methods. We don't want to list these as having changed
        // their abstractness.
        check(
            expectedIssues = """
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class Cls extends test.pkg.Parent {
                  }
                  public class Parent {
                    method public abstract void method(int);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;
                        public final class Cls extends Parent {
                            private Cls() { }
                            @Override public void method(final int parameter) { }
                        }
                        """
                ),
                java(
                    """
                        package test.pkg;
                        public class Parent {
                            private Parent() { }
                            public abstract void method(final int parameter);
                        }
                        """
                )
            )
        )
    }

    @Test
    fun `Implicit modifiers from inherited super classes`() {
        check(
            expectedIssues = """
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class Cls implements test.pkg.Interface {
                    method public void method(int);
                    method public final void method2(int);
                  }
                  public interface Interface {
                    method public void method2(int);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;
                        public final class Cls extends HiddenParent implements Interface {
                            private Cls() { }
                            @Override public void method(final int parameter) { }
                        }
                        """
                ),
                java(
                    """
                        package test.pkg;
                        class HiddenParent {
                            private HiddenParent() { }
                            public abstract void method(final int parameter) { }
                            public final void method2(final int parameter) { }
                        }
                        """
                ),
                java(
                    """
                        package test.pkg;
                        public interface Interface {
                            void method2(final int parameter) { }
                        }
                        """
                )
            )
        )
    }

    @Test
    fun `Wildcard comparisons`() {
        // Doclava signature files sometimes leave out overridden methods of
        // abstract methods. We don't want to list these as having changed
        // their abstractness.
        check(
            expectedIssues = """
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class AbstractMap<K, V> implements java.util.Map {
                    method public java.util.Set<K> keySet();
                    method public V put(K, V);
                    method public void putAll(java.util.Map<? extends K, ? extends V>);
                  }
                  public abstract class EnumMap<K extends java.lang.Enum<K>, V> extends test.pkg.AbstractMap  {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;
                        @SuppressWarnings({"ConstantConditions", "NullableProblems"})
                        public abstract class AbstractMap<K, V> implements java.util.Map {
                            private AbstractMap() { }
                            public V put(K k, V v) { return null; }
                            public java.util.Set<K> keySet() { return null; }
                            public V put(K k, V v) { return null; }
                            public void putAll(java.util.Map<? extends K, ? extends V> x) { }
                        }
                        """
                ),
                java(
                    """
                        package test.pkg;
                        public abstract class EnumMap<K extends java.lang.Enum<K>, V> extends test.pkg.AbstractMap  {
                            private EnumMap() { }
                            public V put(K k, V v) { return null; }
                        }
                        """
                )
            )
        )
    }

    @Test
    fun `Added constructor`() {
        // Regression test for issue 116619591
        check(
            expectedIssues = "src/test/pkg/AbstractMap.java:2: error: Added constructor test.pkg.AbstractMap() [AddedMethod]",
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class AbstractMap<K, V> implements java.util.Map {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;
                        @SuppressWarnings({"ConstantConditions", "NullableProblems"})
                        public abstract class AbstractMap<K, V> implements java.util.Map {
                        }
                        """
                )
            )
        )
    }

    @Test
    fun `Remove infix`() {
        check(
            expectedIssues = """
                src/test/pkg/Foo.kt:5: error: Cannot remove `infix` modifier from method test.pkg.Foo.add2(String): Incompatible change [InfixRemoval]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class Foo {
                    ctor public Foo();
                    method public final void add1(String s);
                    method public final infix void add2(String s);
                    method public final infix void add3(String s);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    class Foo {
                        infix fun add1(s: String) { }
                        fun add2(s: String) { }
                        infix fun add3(s: String) { }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Add seal`() {
        check(
            expectedIssues = """
                src/test/pkg/Foo.kt: error: Cannot add 'sealed' modifier to class test.pkg.Foo: Incompatible change [AddSealed]
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Foo {
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg
                    sealed class Foo
                    """
                )
            )
        )
    }

    @Test
    fun `Remove default parameter`() {
        check(
            expectedIssues = """
                src/test/pkg/Foo.kt:7: error: Attempted to remove default value from parameter s1 in test.pkg.Foo.method4 in method test.pkg.Foo.method4 [DefaultValueChange]
                """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class Foo {
                    ctor public Foo();
                    method public final void method1(boolean b, String? s1);
                    method public final void method2(boolean b, String? s1);
                    method public final void method3(boolean b, String? s1 = "null");
                    method public final void method4(boolean b, String? s1 = "null");
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    class Foo {
                        fun method1(b: Boolean, s1: String?) { }         // No change
                        fun method2(b: Boolean, s1: String? = null) { }  // Adding: OK
                        fun method3(b: Boolean, s1: String? = null) { }  // No change
                        fun method4(b: Boolean, s1: String?) { }         // Removed
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Removing method or field when still available via inheritance is OK`() {
        check(
            expectedIssues = """
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Child extends test.pkg.Parent {
                    ctor public Child();
                    field public int field1;
                    method public void method1();
                  }
                  public class Parent {
                    ctor public Parent();
                    field public int field1;
                    field public int field2;
                    method public void method1();
                    method public void method2();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class Parent {
                        public int field1 = 0;
                        public int field2 = 0;
                        public void method1() { }
                        public void method2() { }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public class Child extends Parent {
                        public int field1 = 0;
                        @Override public void method1() { } // NO CHANGE
                        //@Override public void method2() { } // REMOVED OK: Still inherited
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Change field constant value, change field type`() {
        check(
            expectedIssues = """
                src/test/pkg/Parent.java:5: error: Field test.pkg.Parent.field2 has changed value from 2 to 42 [ChangedValue]
                src/test/pkg/Parent.java:6: error: Field test.pkg.Parent.field3 has changed type from int to char [ChangedType]
                src/test/pkg/Parent.java:7: error: Field test.pkg.Parent.field4 has added 'final' qualifier [AddedFinal]
                src/test/pkg/Parent.java:8: error: Field test.pkg.Parent.field5 has changed 'static' qualifier [ChangedStatic]
                src/test/pkg/Parent.java:9: error: Field test.pkg.Parent.field6 has changed 'transient' qualifier [ChangedTransient]
                src/test/pkg/Parent.java:10: error: Field test.pkg.Parent.field7 has changed 'volatile' qualifier [ChangedVolatile]
                src/test/pkg/Parent.java:11: error: Field test.pkg.Parent.field8 has changed deprecation state true --> false [ChangedDeprecated]
                src/test/pkg/Parent.java:12: error: Field test.pkg.Parent.field9 has changed deprecation state false --> true [ChangedDeprecated]
                src/test/pkg/Parent.java:19: error: Field test.pkg.Parent.field94 has changed value from 1 to 42 [ChangedValue]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Parent {
                    ctor public Parent();
                    field public static final int field1 = 1; // 0x1
                    field public static final int field2 = 2; // 0x2
                    field public int field3;
                    field public int field4 = 4; // 0x4
                    field public int field5;
                    field public int field6;
                    field public int field7;
                    field public deprecated int field8;
                    field public int field9;
                    field public static final int field91 = 1; // 0x1
                    field public static final int field92 = 1; // 0x1
                    field public static final int field93 = 1; // 0x1
                    field public static final int field94 = 1; // 0x1
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.SuppressLint;
                    public class Parent {
                        public static final int field1 = 1;  // UNCHANGED
                        public static final int field2 = 42; // CHANGED VALUE
                        public char field3 = 3;              // CHANGED TYPE
                        public final int field4 = 4;         // ADDED FINAL
                        public static int field5 = 5;        // ADDED STATIC
                        public transient int field6 = 6;     // ADDED TRANSIENT
                        public volatile int field7 = 7;      // ADDED VOLATILE
                        public int field8 = 8;               // REMOVED DEPRECATED
                        /** @deprecated */ @Deprecated public int field9 = 8;  // ADDED DEPRECATED
                        @SuppressLint("ChangedValue")
                        public static final int field91 = 42;// CHANGED VALUE: Suppressed
                        @SuppressLint("ChangedValue:Field test.pkg.Parent.field92 has changed value from 1 to 42")
                        public static final int field92 = 42;// CHANGED VALUE: Suppressed with same message
                        @SuppressLint("ChangedValue: Field test.pkg.Parent.field93 has changed value from 1 to 42")
                        public static final int field93 = 42;// CHANGED VALUE: Suppressed with same message
                        @SuppressLint("ChangedValue:Field test.pkg.Parent.field94 has changed value from 10 to 1")
                        public static final int field94 = 42;// CHANGED VALUE: Suppressed but with different message
                    }
                    """
                ),
                suppressLintSource
            ),
            extraArguments = arrayOf(ARG_HIDE_PACKAGE, "android.annotation")
        )
    }

    @Test
    fun `Change annotation default method value change`() {
        check(
            inputKotlinStyleNulls = true,
            expectedIssues = """
                src/test/pkg/ExportedProperty.java:15: error: Method test.pkg.ExportedProperty.category has changed value from "" to nothing [ChangedValue]
                src/test/pkg/ExportedProperty.java:14: error: Method test.pkg.ExportedProperty.floating has changed value from 1.0f to 1.1f [ChangedValue]
                src/test/pkg/ExportedProperty.java:16: error: Method test.pkg.ExportedProperty.formatToHexString has changed value from nothing to false [ChangedValue]
                src/test/pkg/ExportedProperty.java:13: error: Method test.pkg.ExportedProperty.prefix has changed value from "" to "hello" [ChangedValue]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public @interface ExportedProperty {
                    method public abstract boolean resolveId() default false;
                    method public abstract float floating() default 1.0f;
                    method public abstract String! prefix() default "";
                    method public abstract String! category() default "";
                    method public abstract boolean formatToHexString();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import java.lang.annotation.ElementType;
                    import java.lang.annotation.Retention;
                    import java.lang.annotation.RetentionPolicy;
                    import java.lang.annotation.Target;
                    import static java.lang.annotation.RetentionPolicy.SOURCE;

                    @Target({ElementType.FIELD, ElementType.METHOD})
                    @Retention(RetentionPolicy.RUNTIME)
                    public @interface ExportedProperty {
                        boolean resolveId() default false;            // UNCHANGED
                        String prefix() default "hello";              // CHANGED VALUE
                        float floating() default 1.1f;                // CHANGED VALUE
                        String category();                            // REMOVED VALUE
                        boolean formatToHexString() default false;    // ADDED VALUE
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- class to interface`() {
        check(
            expectedIssues = """
                src/test/pkg/Parent.java:3: error: Class test.pkg.Parent changed class/interface declaration [ChangedClass]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Parent {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public interface Parent {
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- change implemented interfaces`() {
        check(
            expectedIssues = """
                src/test/pkg/Parent.java:3: error: Class test.pkg.Parent no longer implements java.io.Closeable [RemovedInterface]
                src/test/pkg/Parent.java:3: error: Added interface java.util.List to class class test.pkg.Parent [AddedInterface]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class Parent implements java.io.Closeable, java.util.Map {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class Parent implements java.util.Map, java.util.List {
                        private Parent() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- change qualifiers`() {
        check(
            expectedIssues = """
                src/test/pkg/Parent.java:3: error: Class test.pkg.Parent changed 'abstract' qualifier [ChangedAbstract]
                src/test/pkg/Parent.java:3: error: Class test.pkg.Parent changed 'static' qualifier [ChangedStatic]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Parent {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract static class Parent {
                        private Parent() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- final`() {
        check(
            expectedIssues = """
                src/test/pkg/Class1.java:3: error: Class test.pkg.Class1 added 'final' qualifier [AddedFinal]
                TESTROOT/current-api.txt:3: error: Removed constructor test.pkg.Class1() [RemovedMethod]
                src/test/pkg/Class2.java:3: error: Class test.pkg.Class2 added 'final' qualifier but was previously uninstantiable and therefore could not be subclassed [AddedFinalUninstantiable]
                src/test/pkg/Class3.java:3: error: Class test.pkg.Class3 removed 'final' qualifier [RemovedFinal]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Class1 {
                      ctor public Class1();
                  }
                  public class Class2 {
                  }
                  public final class Class3 {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public final class Class1 {
                        private Class1() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public final class Class2 {
                        private Class2() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public class Class3 {
                        private Class3() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- visibility`() {
        check(
            expectedIssues = """
                src/test/pkg/Class1.java:3: error: Class test.pkg.Class1 changed visibility from protected to public [ChangedScope]
                src/test/pkg/Class2.java:3: error: Class test.pkg.Class2 changed visibility from public to protected [ChangedScope]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  protected class Class1 {
                  }
                  public class Class2 {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class Class1 {
                        private Class1() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    protected class Class2 {
                        private Class2() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- deprecation`() {
        check(
            expectedIssues = """
                src/test/pkg/Class1.java:3: error: Class test.pkg.Class1 has changed deprecation state false --> true [ChangedDeprecated]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Class1 {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    /** @deprecated */
                    @Deprecated public class Class1 {
                        private Class1() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- superclass`() {
        check(
            expectedIssues = """
                src/test/pkg/Class3.java:3: error: Class test.pkg.Class3 superclass changed from java.lang.Char to java.lang.Number [ChangedSuperclass]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class Class1 {
                  }
                  public abstract class Class2 extends java.lang.Number {
                  }
                  public abstract class Class3 extends java.lang.Char {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class Class1 extends java.lang.Short {
                        private Class1() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public abstract class Class2 extends java.lang.Float {
                        private Class2() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public abstract class Class3 extends java.lang.Number {
                        private Class3() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible class change -- type variables`() {
        check(
            expectedIssues = """
                src/test/pkg/Class1.java:3: error: Class test.pkg.Class1 changed number of type parameters from 1 to 2 [ChangedType]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Class1<X> {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class Class1<X,Y> {
                        private Class1() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible method change -- modifiers`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:5: error: Method test.pkg.MyClass.myMethod2 has changed 'abstract' qualifier [ChangedAbstract]
                src/test/pkg/MyClass.java:6: error: Method test.pkg.MyClass.myMethod3 has changed 'static' qualifier [ChangedStatic]
                src/test/pkg/MyClass.java:7: error: Method test.pkg.MyClass.myMethod4 has changed deprecation state true --> false [ChangedDeprecated]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass {
                      method public void myMethod2();
                      method public void myMethod3();
                      method deprecated public void myMethod4();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass {
                        private MyClass() {}
                        public native abstract void myMethod2(); // Note that Errors.CHANGE_NATIVE is hidden by default
                        public static void myMethod3() {}
                        public void myMethod4() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible method change -- final`() {
        check(
            expectedIssues = """
                src/test/pkg/Outer.java:7: error: Method test.pkg.Outer.Class1.method1 has added 'final' qualifier [AddedFinal]
                src/test/pkg/Outer.java:19: error: Method test.pkg.Outer.Class4.method4 has removed 'final' qualifier [RemovedFinal]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class Outer {
                  }
                  public class Outer.Class1 {
                    method public void method1();
                  }
                  public final class Outer.Class2 {
                    method public void method2();
                  }
                  public final class Outer.Class3 {
                    method public void method3();
                  }
                  public class Outer.Class4 {
                    method public final void method4();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class Outer {
                        private Outer() {}
                        public class Class1 {
                            private Class1() {}
                            public final void method1() { } // Added final
                        }
                        public final class Class2 {
                            private Class2() {}
                            public final void method2() { } // Added final but class is effectively final so no change
                        }
                        public final class Class3 {
                            private Class3() {}
                            public void method3() { } // Removed final but is still effectively final
                        }
                        public class Class4 {
                            private Class4() {}
                            public void method4() { } // Removed final
                        }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible method change -- visibility`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:5: error: Method test.pkg.MyClass.myMethod1 changed visibility from protected to public [ChangedScope]
                src/test/pkg/MyClass.java:6: error: Method test.pkg.MyClass.myMethod2 changed visibility from public to protected [ChangedScope]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass {
                      method protected void myMethod1();
                      method public void myMethod2();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass {
                        private MyClass() {}
                        public void myMethod1() {}
                        protected void myMethod2() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible method change -- throws list`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:7: error: Method test.pkg.MyClass.method1 added thrown exception java.io.IOException [ChangedThrows]
                src/test/pkg/MyClass.java:8: error: Method test.pkg.MyClass.method2 no longer throws exception java.io.IOException [ChangedThrows]
                src/test/pkg/MyClass.java:9: error: Method test.pkg.MyClass.method3 no longer throws exception java.io.IOException [ChangedThrows]
                src/test/pkg/MyClass.java:9: error: Method test.pkg.MyClass.method3 no longer throws exception java.lang.NumberFormatException [ChangedThrows]
                src/test/pkg/MyClass.java:9: error: Method test.pkg.MyClass.method3 added thrown exception java.lang.UnsupportedOperationException [ChangedThrows]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass {
                      method public void finalize() throws java.lang.Throwable;
                      method public void method1();
                      method public void method2() throws java.io.IOException;
                      method public void method3() throws java.io.IOException, java.lang.NumberFormatException;
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    @SuppressWarnings("RedundantThrows")
                    public abstract class MyClass {
                        private MyClass() {}
                        public void finalize() {}
                        public void method1() throws java.io.IOException {}
                        public void method2() {}
                        public void method3() throws java.lang.UnsupportedOperationException {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible method change -- return types`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:5: error: Method test.pkg.MyClass.method1 has changed return type from float to int [ChangedType]
                src/test/pkg/MyClass.java:6: error: Method test.pkg.MyClass.method2 has changed return type from java.util.List<Number> to java.util.List<java.lang.Integer> [ChangedType]
                src/test/pkg/MyClass.java:7: error: Method test.pkg.MyClass.method3 has changed return type from java.util.List<Integer> to java.util.List<java.lang.Number> [ChangedType]
                src/test/pkg/MyClass.java:8: error: Method test.pkg.MyClass.method4 has changed return type from String to String[] [ChangedType]
                src/test/pkg/MyClass.java:9: error: Method test.pkg.MyClass.method5 has changed return type from String[] to String[][] [ChangedType]
                src/test/pkg/MyClass.java:10: error: Method test.pkg.MyClass.method6 has changed return type from T (extends java.lang.Object) to U (extends java.lang.Number) [ChangedType]
                src/test/pkg/MyClass.java:11: error: Method test.pkg.MyClass.method7 has changed return type from T to Number [ChangedType]
                src/test/pkg/MyClass.java:13: error: Method test.pkg.MyClass.method9 has changed return type from X (extends java.lang.Throwable) to U (extends java.lang.Number) [ChangedType]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass<T extends Number> {
                      method public float method1();
                      method public java.util.List<Number> method2();
                      method public java.util.List<Integer> method3();
                      method public String method4();
                      method public String[] method5();
                      method public <X extends java.lang.Throwable> T method6(java.util.function.Supplier<? extends X>);
                      method public <X extends java.lang.Throwable> T method7(java.util.function.Supplier<? extends X>);
                      method public <X extends java.lang.Throwable> Number method8(java.util.function.Supplier<? extends X>);
                      method public <X extends java.lang.Throwable> X method9(java.util.function.Supplier<? extends X>);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass<U extends Number> { // Changing type variable name is fine/compatible
                        private MyClass() {}
                        public int method1() { return 0; }
                        public java.util.List<Integer> method2() { return null; }
                        public java.util.List<Number> method3() { return null; }
                        public String[] method4() { return null; }
                        public String[][] method5() { return null; }
                        public <X extends java.lang.Throwable> U method6(java.util.function.Supplier<? extends X> arg) { return null; }
                        public <X extends java.lang.Throwable> Number method7(java.util.function.Supplier<? extends X> arg) { return null; }
                        public <X extends java.lang.Throwable> U method8(java.util.function.Supplier<? extends X> arg) { return null; }
                        public <X extends java.lang.Throwable> U method9(java.util.function.Supplier<? extends X> arg) { return null; }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Incompatible field change -- visibility and removing final`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:5: error: Field test.pkg.MyClass.myField1 changed visibility from protected to public [ChangedScope]
                src/test/pkg/MyClass.java:6: error: Field test.pkg.MyClass.myField2 changed visibility from public to protected [ChangedScope]
                src/test/pkg/MyClass.java:7: error: Field test.pkg.MyClass.myField3 has removed 'final' qualifier [RemovedFinal]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass {
                      field protected int myField1;
                      field public int myField2;
                      field public final int myField3;
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass {
                        private MyClass() {}
                        public int myField1 = 1;
                        protected int myField2 = 1;
                        public int myField3 = 1;
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Adding classes, interfaces and packages, and removing these`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:3: error: Added class test.pkg.MyClass [AddedClass]
                src/test/pkg/MyInterface.java:3: error: Added class test.pkg.MyInterface [AddedInterface]
                TESTROOT/current-api.txt:2: error: Removed class test.pkg.MyOldClass [RemovedClass]
                error: Added package test.pkg2 [AddedPackage]
                TESTROOT/current-api.txt:5: error: Removed package test.pkg3 [RemovedPackage]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyOldClass {
                  }
                }
                package test.pkg3 {
                  public abstract class MyOldClass {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass {
                        private MyClass() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public interface MyInterface {
                    }
                    """
                ),
                java(
                    """
                    package test.pkg2;

                    public abstract class MyClass2 {
                        private MyClass2() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test removing public constructor`() {
        check(
            expectedIssues = """
                TESTROOT/current-api.txt:3: error: Removed constructor test.pkg.MyClass() [RemovedMethod]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass {
                    ctor public MyClass();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass {
                        private MyClass() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test type variables from text signature files`() {
        check(
            expectedIssues = """
                src/test/pkg/MyClass.java:8: error: Method test.pkg.MyClass.myMethod4 has changed return type from S (extends java.lang.Object) to S (extends java.lang.Float) [ChangedType]
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public abstract class MyClass<T extends test.pkg.Number,T_SPLITR> {
                    method public T myMethod1();
                    method public <S extends test.pkg.Number> S myMethod2();
                    method public <S> S myMethod3();
                    method public <S> S myMethod4();
                    method public java.util.List<byte[]> myMethod5();
                    method public T_SPLITR[] myMethod6();
                  }
                  public class Number {
                    ctor public Number();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass<T extends Number,T_SPLITR> {
                        private MyClass() {}
                        public T myMethod1() { return null; }
                        public <S extends Number> S myMethod2() { return null; }
                        public <S> S myMethod3() { return null; }
                        public <S extends Float> S myMethod4() { return null; }
                        public java.util.List<byte[]> myMethod5() { return null; }
                        public T_SPLITR[] myMethod6() { return null; }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class Number {
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test Kotlin extensions`() {
        check(
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            omitCommonPackages = true,
            compatibilityMode = false,
            expectedIssues = "",
            checkCompatibilityApi = """
                package androidx.content {
                  public final class ContentValuesKt {
                    method public static android.content.ContentValues contentValuesOf(kotlin.Pair<String,?>... pairs);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    "src/androidx/content/ContentValues.kt",
                    """
                    package androidx.content

                    import android.content.ContentValues

                    fun contentValuesOf(vararg pairs: Pair<String, Any?>) = ContentValues(pairs.size).apply {
                        for ((key, value) in pairs) {
                            when (value) {
                                null -> putNull(key)
                                is String -> put(key, value)
                                is Int -> put(key, value)
                                is Long -> put(key, value)
                                is Boolean -> put(key, value)
                                is Float -> put(key, value)
                                is Double -> put(key, value)
                                is ByteArray -> put(key, value)
                                is Byte -> put(key, value)
                                is Short -> put(key, value)
                                else -> {
                                    val valueType = value.javaClass.canonicalName
                                    throw IllegalArgumentException("Illegal value type")
                                }
                            }
                        }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test Kotlin type bounds`() {
        check(
            inputKotlinStyleNulls = false,
            outputKotlinStyleNulls = true,
            omitCommonPackages = true,
            compatibilityMode = false,
            expectedIssues = "",
            checkCompatibilityApi = """
                package androidx.navigation {
                  public final class NavDestination {
                    ctor public NavDestination();
                  }
                  public class NavDestinationBuilder<D extends androidx.navigation.NavDestination> {
                    ctor public NavDestinationBuilder(int id);
                    method public D build();
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package androidx.navigation

                    open class NavDestinationBuilder<out D : NavDestination>(
                            id: Int
                    ) {
                        open fun build(): D {
                            TODO()
                        }
                    }

                    class NavDestination
                    """
                )
            )
        )
    }

    @Test
    fun `Test inherited methods`() {
        check(
            expectedIssues = """
                """,
            checkCompatibilityApi = """
                package test.pkg {
                  public class Child1 extends test.pkg.Parent {
                  }
                  public class Child2 extends test.pkg.Parent {
                    method public void method0(java.lang.String, int);
                    method public void method4(java.lang.String, int);
                  }
                  public class Child3 extends test.pkg.Parent {
                    method public void method1(java.lang.String, int);
                    method public void method2(java.lang.String, int);
                  }
                  public class Parent {
                    method public void method1(java.lang.String, int);
                    method public void method2(java.lang.String, int);
                    method public void method3(java.lang.String, int);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class Child1 extends Parent {
                        private Child1() {}
                        @Override
                        public void method1(String first, int second) {
                        }
                        @Override
                        public void method2(String first, int second) {
                        }
                        @Override
                        public void method3(String first, int second) {
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public class Child2 extends Parent {
                        private Child2() {}
                        @Override
                        public void method0(String first, int second) {
                        }
                        @Override
                        public void method1(String first, int second) {
                        }
                        @Override
                        public void method2(String first, int second) {
                        }
                        @Override
                        public void method3(String first, int second) {
                        }
                        @Override
                        public void method4(String first, int second) {
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public class Child3 extends Parent {
                        private Child3() {}
                        @Override
                        public void method1(String first, int second) {
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class Parent {
                        private Parent() { }
                        public void method1(String first, int second) {
                        }
                        public void method2(String first, int second) {
                        }
                        public void method3(String first, int second) {
                        }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Partial text file which references inner classes not listed elsewhere`() {
        // This happens in system and test files where we only include APIs that differ
        // from the base IDE. When parsing these code bases we need to gracefully handle
        // references to inner classes.
        check(
            includeSystemApiAnnotations = true,
            expectedIssues = """
                src/test/pkg/Bar.java:17: error: Added method test.pkg.Bar.Inner1.Inner2.addedMethod() to the system API [AddedMethod]
                TESTROOT/current-api.txt:4: error: Removed method test.pkg.Bar.Inner1.Inner2.removedMethod() [RemovedMethod]
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package other.pkg;

                    public class MyClass {
                        public class MyInterface {
                            public void test() { }
                        }
                    }
                    """
                ).indented(),
                java(
                    """
                    package test.pkg;
                    import android.annotation.SystemApi;

                    public class Bar {
                        public class Inner1 {
                            private Inner1() { }
                            @SuppressWarnings("JavaDoc")
                            public class Inner2 {
                                private Inner2() { }

                                /**
                                 * @hide
                                 */
                                @SystemApi
                                public void method() { }

                                /**
                                 * @hide
                                 */
                                @SystemApi
                                public void addedMethod() { }
                            }
                        }
                    }
                    """
                ),
                systemApiSource
            ),

            extraArguments = arrayOf(
                ARG_SHOW_ANNOTATION, "android.annotation.TestApi",
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_HIDE_PACKAGE, "android.support.annotation"
            ),

            checkCompatibilityApi =
            """
                package test.pkg {
                  public class Bar.Inner1.Inner2 {
                    method public void method();
                    method public void removedMethod();
                  }
                }
                """
        )
    }

    @Test
    fun `Partial text file which adds methods to show-annotation API`() {
        // This happens in system and test files where we only include APIs that differ
        // from the base IDE. When parsing these code bases we need to gracefully handle
        // references to inner classes.
        check(
            includeSystemApiAnnotations = true,
            expectedIssues = """
                TESTROOT/current-api.txt:4: error: Removed method android.rolecontrollerservice.RoleControllerService.onClearRoleHolders() [RemovedMethod]
                src/android/rolecontrollerservice/RoleControllerService.java:7: error: Added method android.rolecontrollerservice.RoleControllerService.onGrantDefaultRoles() to the system API [AddedAbstractMethod]
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.rolecontrollerservice;

                    public class Service {
                    }
                    """
                ).indented(),
                java(
                    """
                    package android.rolecontrollerservice;
                    import android.annotation.SystemApi;

                    /** @hide */
                    @SystemApi
                    public abstract class RoleControllerService extends Service {
                        public abstract void onGrantDefaultRoles();
                    }
                    """
                ),
                systemApiSource
            ),

            extraArguments = arrayOf(
                ARG_SHOW_ANNOTATION, "android.annotation.TestApi",
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_HIDE_PACKAGE, "android.support.annotation"
            ),

            checkCompatibilityApi =
                """
                package android.rolecontrollerservice {
                  public abstract class RoleControllerService extends android.rolecontrollerservice.Service {
                    ctor public RoleControllerService();
                    method public abstract void onClearRoleHolders();
                  }
                }
                """
        )
    }

    @Test
    fun `Partial text file where type previously did not exist`() {
        check(
            expectedIssues = """
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.SystemApi;

                    /**
                     * @hide
                     */
                    @SystemApi
                    public class SampleException1 extends java.lang.Exception {
                    }
                    """
                ).indented(),
                java(
                    """
                    package test.pkg;
                    import android.annotation.SystemApi;

                    /**
                     * @hide
                     */
                    @SystemApi
                    public class SampleException2 extends java.lang.Throwable {
                    }
                    """
                ).indented(),
                java(
                    """
                    package test.pkg;
                    import android.annotation.SystemApi;

                    /**
                     * @hide
                     */
                    @SystemApi
                    public class Utils {
                        public void method1() throws SampleException1 { }
                        public void method2() throws SampleException2 { }
                    }
                    """
                ),
                systemApiSource
            ),

            extraArguments = arrayOf(
                ARG_SHOW_ANNOTATION, "android.annotation.SystemApi",
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_HIDE_PACKAGE, "android.support.annotation"
            ),

            checkCompatibilityApiReleased =
            """
                package test.pkg {
                  public class Utils {
                    ctor public Utils();
                    // We don't define SampleException1 or SampleException in this file,
                    // in this partial signature, so we don't need to validate that they
                    // have not been changed
                    method public void method1() throws test.pkg.SampleException1;
                    method public void method2() throws test.pkg.SampleException2;
                  }
                }
                """
        )
    }

    @Test
    fun `Test verifying simple removed API`() {
        check(
            expectedIssues = """
                src/test/pkg/Bar.java:8: error: Added method test.pkg.Bar.newlyRemoved() to the removed API [AddedMethod]
                """,
            checkCompatibilityRemovedApiCurrent = """
                package test.pkg {
                  public class Bar {
                    ctor public Bar();
                    method public void removedMethod();
                  }
                  public class Bar.Inner {
                    ctor public Bar.Inner();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    @SuppressWarnings("JavaDoc")
                    public class Bar {
                        /** @removed */
                        public Bar() { }
                        // No longer removed: /** @removed */
                        public void removedMethod() { }
                        /** @removed */
                        public void newlyRemoved() { }

                        public void newlyAdded() { }

                        /** @removed */
                        public class Inner { }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test verifying removed API`() {
        check(
            expectedIssues = """
                """,
            checkCompatibilityRemovedApiCurrent = """
                package test.pkg {
                  public class Bar {
                    ctor public Bar();
                    method public void removedMethod();
                    field public int removedField;
                  }
                  public class Bar.Inner {
                    ctor public Bar.Inner();
                  }
                  public class Bar.Inner2.Inner3.Inner4 {
                    ctor public Bar.Inner2.Inner3.Inner4();
                  }
                  public class Bar.Inner5.Inner6.Inner7 {
                    field public int removed;
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    @SuppressWarnings("JavaDoc")
                    public class Bar {
                        /** @removed */
                        public Bar() { }
                        public int field;
                        public void test() { }
                        /** @removed */
                        public int removedField;
                        /** @removed */
                        public void removedMethod() { }
                        /** @removed and @hide - should not be listed */
                        public int hiddenField;

                        /** @removed */
                        public class Inner { }

                        public class Inner2 {
                            public class Inner3 {
                                /** @removed */
                                public class Inner4 { }
                            }
                        }

                        public class Inner5 {
                            public class Inner6 {
                                public class Inner7 {
                                    /** @removed */
                                    public int removed;
                                }
                            }
                        }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Regression test for bug 120847535`() {
        // Regression test for
        // 120847535: check-api doesn't fail on method that is in current.txt, but marked @hide @TestApi
        check(
            expectedIssues = """
                TESTROOT/current-api.txt:6: error: Removed method test.view.ViewTreeObserver.registerFrameCommitCallback(Runnable) [RemovedMethod]
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.view;
                    import android.annotation.TestApi;
                    public final class ViewTreeObserver {
                         /**
                         * @hide
                         */
                        @TestApi
                        public void registerFrameCommitCallback(Runnable callback) {
                        }
                    }
                    """
                ).indented(),
                java(
                    """
                    package test.view;
                    public final class View {
                        private View() { }
                    }
                    """
                ).indented(),
                testApiSource
            ),

            api = """
                package test.view {
                  public final class View {
                  }
                  public final class ViewTreeObserver {
                    ctor public ViewTreeObserver();
                  }
                }
            """,
            extraArguments = arrayOf(
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_HIDE_PACKAGE, "android.support.annotation"
            ),

            checkCompatibilityApi = """
                package test.view {
                  public final class View {
                  }
                  public final class ViewTreeObserver {
                    ctor public ViewTreeObserver();
                    method public void registerFrameCommitCallback(java.lang.Runnable);
                  }
                }
                """
        )
    }

    @Test
    fun `Test release compatibility checking`() {
        // Different checks are enforced for current vs release API comparisons:
        // we don't flag AddedClasses etc. Removed classes *are* enforced.
        check(
            expectedIssues = """
                src/test/pkg/Class1.java:3: error: Class test.pkg.Class1 added 'final' qualifier [AddedFinal]
                TESTROOT/released-api.txt:3: error: Removed constructor test.pkg.Class1() [RemovedMethod]
                src/test/pkg/MyClass.java:5: warning: Method test.pkg.MyClass.myMethod2 has changed 'abstract' qualifier [ChangedAbstract]
                src/test/pkg/MyClass.java:6: error: Method test.pkg.MyClass.myMethod3 has changed 'static' qualifier [ChangedStatic]
                TESTROOT/released-api.txt:14: error: Removed class test.pkg.MyOldClass [RemovedClass]
                TESTROOT/released-api.txt:17: error: Removed package test.pkg3 [RemovedPackage]
                """,
            checkCompatibilityApiReleased = """
                package test.pkg {
                  public class Class1 {
                      ctor public Class1();
                  }
                  public class Class2 {
                  }
                  public final class Class3 {
                  }
                  public abstract class MyClass {
                      method public void myMethod2();
                      method public void myMethod3();
                      method deprecated public void myMethod4();
                  }
                  public abstract class MyOldClass {
                  }
                }
                package test.pkg3 {
                  public abstract class MyOldClass {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public final class Class1 {
                        private Class1() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public final class Class2 {
                        private Class2() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public class Class3 {
                        private Class3() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public abstract class MyNewClass {
                        private MyNewClass() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public abstract class MyClass {
                        private MyClass() {}
                        public native abstract void myMethod2(); // Note that Errors.CHANGE_NATIVE is hidden by default
                        public static void myMethod3() {}
                        public void myMethod4() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test remove deprecated API is an error`() {
        // Regression test for b/145745855
        check(
            expectedIssues = """
                TESTROOT/released-api.txt:6: error: Removed deprecated class test.pkg.DeprecatedClass [RemovedDeprecatedClass]
                TESTROOT/released-api.txt:3: error: Removed deprecated constructor test.pkg.SomeClass() [RemovedDeprecatedMethod]
                TESTROOT/released-api.txt:4: error: Removed deprecated method test.pkg.SomeClass.deprecatedMethod() [RemovedDeprecatedMethod]
                """,
            checkCompatibilityApiReleased = """
                package test.pkg {
                  public class SomeClass {
                      ctor deprecated public SomeClass();
                      method deprecated public void deprecatedMethod();
                  }
                  deprecated public class DeprecatedClass {
                      ctor deprecated public DeprecatedClass();
                      method deprecated public void deprecatedMethod();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class SomeClass {
                        private SomeClass() {}
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Implicit nullness`() {
        check(
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 2.0
                package androidx.annotation {
                  @java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.CLASS) @java.lang.annotation.Target({java.lang.annotation.ElementType.ANNOTATION_TYPE, java.lang.annotation.ElementType.TYPE, java.lang.annotation.ElementType.METHOD, java.lang.annotation.ElementType.CONSTRUCTOR, java.lang.annotation.ElementType.FIELD, java.lang.annotation.ElementType.PACKAGE}) public @interface RestrictTo {
                    method public abstract androidx.annotation.RestrictTo.Scope[] value();
                  }

                  public enum RestrictTo.Scope {
                    enum_constant @Deprecated public static final androidx.annotation.RestrictTo.Scope GROUP_ID;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope LIBRARY;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope LIBRARY_GROUP;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope SUBCLASSES;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope TESTS;
                  }
                }
                """,

            sourceFiles = arrayOf(
                restrictToSource
            )
        )
    }

    @Test
    fun `Implicit nullness in compat format`() {
        // Make sure we put "static" in enum modifier lists when in v1/compat mode
        check(
            compatibilityMode = true,
            inputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                package androidx.annotation {
                  public abstract class RestrictTo implements java.lang.annotation.Annotation {
                    method public abstract androidx.annotation.RestrictTo.Scope[] value();
                  }

                  public static final class RestrictTo.Scope extends java.lang.Enum {
                    enum_constant deprecated public static final androidx.annotation.RestrictTo.Scope GROUP_ID;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope LIBRARY;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope LIBRARY_GROUP;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope SUBCLASSES;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope TESTS;
                  }
                }
                """,

            sourceFiles = arrayOf(
                restrictToSource
            )
        )
    }

    @Test
    fun `Java String constants`() {
        check(
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                package androidx.browser.browseractions {
                  public class BrowserActionsIntent {
                    field public static final String EXTRA_APP_ID = "androidx.browser.browseractions.APP_ID";
                  }
                }
                """,

            sourceFiles = arrayOf(
                java(
                    """
                     package androidx.browser.browseractions;
                     public class BrowserActionsIntent {
                        private BrowserActionsIntent() { }
                        public static final String EXTRA_APP_ID = "androidx.browser.browseractions.APP_ID";

                     }
                    """
                ).indented()
            )
        )
    }

    @Test
    fun `Classes with maps`() {
        check(
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 2.0
                package androidx.collection {
                  public class SimpleArrayMap<K, V> {
                  }
                }
                """,

            sourceFiles = arrayOf(
                java(
                    """
                    package androidx.collection;

                    public class SimpleArrayMap<K, V> {
                        private SimpleArrayMap() { }
                    }
                    """
                ).indented()
            )
        )
    }

    @Test
    fun `Referencing type parameters in types`() {
        check(
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 2.0
                package androidx.collection {
                  public class MyMap<Key, Value> {
                    ctor public MyMap();
                    field public Key! myField;
                    method public Key! getReplacement(Key!);
                  }
                }
                """,

            sourceFiles = arrayOf(
                java(
                    """
                    package androidx.collection;

                    public class MyMap<Key, Value> {
                        public Key getReplacement(Key key) { return null; }
                        public Key myField = null;
                    }
                    """
                ).indented()
            )
        )
    }

    @Test
    fun `Comparing annotations with methods with v1 signature files`() {
        check(
            compatibilityMode = true,
            checkCompatibilityApi = """
                package androidx.annotation {
                  public abstract class RestrictTo implements java.lang.annotation.Annotation {
                  }
                  public static final class RestrictTo.Scope extends java.lang.Enum {
                    method public static androidx.annotation.RestrictTo.Scope valueOf(java.lang.String);
                    method public static final androidx.annotation.RestrictTo.Scope[] values();
                    enum_constant public static final deprecated androidx.annotation.RestrictTo.Scope GROUP_ID;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope LIBRARY;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope LIBRARY_GROUP;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope SUBCLASSES;
                    enum_constant public static final androidx.annotation.RestrictTo.Scope TESTS;
                  }
                }
                """,

            sourceFiles = arrayOf(
                restrictToSource
            )
        )
    }

    @Test
    fun `Insignificant type formatting differences`() {
        check(
            checkCompatibilityApi = """
                package test.pkg {
                  public final class UsageStatsManager {
                    method public java.util.Map<java.lang.String, java.lang.Integer> getAppStandbyBuckets();
                    method public void setAppStandbyBuckets(java.util.Map<java.lang.String, java.lang.Integer>);
                    field public java.util.Map<java.lang.String, java.lang.Integer> map;
                  }
                }
                """,
            signatureSource = """
                package test.pkg {
                  public final class UsageStatsManager {
                    method public java.util.Map<java.lang.String,java.lang.Integer> getAppStandbyBuckets();
                    method public void setAppStandbyBuckets(java.util.Map<java.lang.String,java.lang.Integer>);
                    field public java.util.Map<java.lang.String,java.lang.Integer> map;
                  }
                }
                """
        )
    }

    @Test
    fun `Compare signatures with Kotlin nullability from signature`() {
        check(
            expectedIssues = """
            TESTROOT/load-api.txt:5: error: Attempted to remove @NonNull annotation from parameter str in test.pkg.Foo.method1(int p, Integer int2, int p1, String str, java.lang.String... args) [InvalidNullConversion]
            TESTROOT/load-api.txt:7: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter str in test.pkg.Foo.method3(String str, int p, int int2) [InvalidNullConversion]
            """.trimIndent(),
            format = FileFormat.V3,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package test.pkg {
                  public final class Foo {
                    ctor public Foo();
                    method public void method1(int p = 42, Integer? int2 = null, int p1 = 42, String str = "hello world", java.lang.String... args);
                    method public void method2(int p, int int2 = (2 * int) * some.other.pkg.Constants.Misc.SIZE);
                    method public void method3(String? str, int p, int int2 = double(int) + str.length);
                    field public static final test.pkg.Foo.Companion! Companion;
                  }
                }
                """,
            signatureSource = """
                // Signature format: 3.0
                package test.pkg {
                  public final class Foo {
                    ctor public Foo();
                    method public void method1(int p = 42, Integer? int2 = null, int p1 = 42, String! str = "hello world", java.lang.String... args);
                    method public void method2(int p, int int2 = (2 * int) * some.other.pkg.Constants.Misc.SIZE);
                    method public void method3(String str, int p, int int2 = double(int) + str.length);
                    field public static final test.pkg.Foo.Companion! Companion;
                  }
                }
                """
        )
    }

    @Test
    fun `Compare signatures with Kotlin nullability from source`() {
        check(
            expectedIssues = """
            src/test/pkg/test.kt:4: error: Attempted to change parameter from @Nullable to @NonNull: incompatible change for parameter str1 in test.pkg.TestKt.fun1(String str1, String str2, java.util.List<java.lang.String> list) [InvalidNullConversion]
            """.trimIndent(),
            format = FileFormat.V3,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package test.pkg {
                  public final class TestKt {
                    method public static void fun1(String? str1, String str2, java.util.List<java.lang.String!> list);
                  }
                }
                """,
            sourceFiles = arrayOf(
                kotlin(
                    """
                        package test.pkg
                        import java.util.List

                        fun fun1(str1: String, str2: String?, list: List<String?>) { }

                    """.trimIndent()
                )
            )
        )
    }

    @Test
    fun `Adding and removing reified`() {
        check(
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            expectedIssues = """
            src/test/pkg/test.kt:5: error: Method test.pkg.TestKt.add made type variable T reified: incompatible change [ChangedThrows]
            src/test/pkg/test.kt:8: error: Method test.pkg.TestKt.two made type variable S reified: incompatible change [ChangedThrows]
            """,
            checkCompatibilityApi = """
                package test.pkg {
                  public final class TestKt {
                    method public static inline <T> void add(T! t);
                    method public static inline <reified T> void remove(T! t);
                    method public static inline <reified T> void unchanged(T! t);
                    method public static inline <S, reified T> void two(S! s, T! t);
                  }
                }
                """,

            sourceFiles = arrayOf(
                kotlin(
                    """
                    @file:Suppress("NOTHING_TO_INLINE", "RedundantVisibilityModifier", "unused")

                    package test.pkg

                    inline fun <reified T> add(t: T) { }
                    inline fun <T> remove(t: T) { }
                    inline fun <reified T> unchanged(t: T) { }
                    inline fun <reified S, T> two(s: S, t: T) { }
                    """
                ).indented()
            )
        )
    }

    @Ignore("Not currently working: we're getting the wrong PSI results; I suspect caching across the two codebases")
    @Test
    fun `Test All Android API levels`() {
        // Checks API across Android SDK versions and makes sure the results are
        // intentional (to help shake out bugs in the API compatibility checker)

        // Expected migration warnings (the map value) when migrating to the target key level from the previous level
        val expected = mapOf(
            5 to "warning: Method android.view.Surface.lockCanvas added thrown exception java.lang.IllegalArgumentException [ChangedThrows]",
            6 to """
                warning: Method android.accounts.AbstractAccountAuthenticator.confirmCredentials added thrown exception android.accounts.NetworkErrorException [ChangedThrows]
                warning: Method android.accounts.AbstractAccountAuthenticator.updateCredentials added thrown exception android.accounts.NetworkErrorException [ChangedThrows]
                warning: Field android.view.WindowManager.LayoutParams.TYPE_STATUS_BAR_PANEL has changed value from 2008 to 2014 [ChangedValue]
                """,
            7 to """
                error: Removed field android.view.ViewGroup.FLAG_USE_CHILD_DRAWING_ORDER [RemovedField]
                """,

            // setOption getting removed here is wrong! Seems to be a PSI loading bug.
            8 to """
                warning: Constructor android.net.SSLCertificateSocketFactory no longer throws exception java.security.KeyManagementException [ChangedThrows]
                warning: Constructor android.net.SSLCertificateSocketFactory no longer throws exception java.security.NoSuchAlgorithmException [ChangedThrows]
                error: Removed method java.net.DatagramSocketImpl.getOption(int) [RemovedMethod]
                error: Removed method java.net.DatagramSocketImpl.setOption(int,Object) [RemovedMethod]
                warning: Constructor java.nio.charset.Charset no longer throws exception java.nio.charset.IllegalCharsetNameException [ChangedThrows]
                warning: Method java.nio.charset.Charset.forName no longer throws exception java.nio.charset.IllegalCharsetNameException [ChangedThrows]
                warning: Method java.nio.charset.Charset.forName no longer throws exception java.nio.charset.UnsupportedCharsetException [ChangedThrows]
                warning: Method java.nio.charset.Charset.isSupported no longer throws exception java.nio.charset.IllegalCharsetNameException [ChangedThrows]
                warning: Method java.util.regex.Matcher.appendReplacement no longer throws exception java.lang.IllegalStateException [ChangedThrows]
                warning: Method java.util.regex.Matcher.start no longer throws exception java.lang.IllegalStateException [ChangedThrows]
                warning: Method java.util.regex.Pattern.compile no longer throws exception java.util.regex.PatternSyntaxException [ChangedThrows]
                warning: Class javax.xml.XMLConstants added final qualifier [AddedFinal]
                error: Removed constructor javax.xml.XMLConstants() [RemovedMethod]
                warning: Method javax.xml.parsers.DocumentBuilder.isXIncludeAware no longer throws exception java.lang.UnsupportedOperationException [ChangedThrows]
                warning: Method javax.xml.parsers.DocumentBuilderFactory.newInstance no longer throws exception javax.xml.parsers.FactoryConfigurationError [ChangedThrows]
                warning: Method javax.xml.parsers.SAXParser.isXIncludeAware no longer throws exception java.lang.UnsupportedOperationException [ChangedThrows]
                warning: Method javax.xml.parsers.SAXParserFactory.newInstance no longer throws exception javax.xml.parsers.FactoryConfigurationError [ChangedThrows]
                warning: Method org.w3c.dom.Element.getAttributeNS added thrown exception org.w3c.dom.DOMException [ChangedThrows]
                warning: Method org.w3c.dom.Element.getAttributeNodeNS added thrown exception org.w3c.dom.DOMException [ChangedThrows]
                warning: Method org.w3c.dom.Element.getElementsByTagNameNS added thrown exception org.w3c.dom.DOMException [ChangedThrows]
                warning: Method org.w3c.dom.Element.hasAttributeNS added thrown exception org.w3c.dom.DOMException [ChangedThrows]
                warning: Method org.w3c.dom.NamedNodeMap.getNamedItemNS added thrown exception org.w3c.dom.DOMException [ChangedThrows]
                """,

            18 to """
                warning: Class android.os.Looper added final qualifier but was previously uninstantiable and therefore could not be subclassed [AddedFinalUninstantiable]
                warning: Class android.os.MessageQueue added final qualifier but was previously uninstantiable and therefore could not be subclassed [AddedFinalUninstantiable]
                error: Removed field android.os.Process.BLUETOOTH_GID [RemovedField]
                error: Removed class android.renderscript.Program [RemovedClass]
                error: Removed class android.renderscript.ProgramStore [RemovedClass]
                """,
            19 to """
                warning: Method android.app.Notification.Style.build has changed 'abstract' qualifier [ChangedAbstract]
                error: Removed method android.os.Debug.MemoryInfo.getOtherLabel(int) [RemovedMethod]
                error: Removed method android.os.Debug.MemoryInfo.getOtherPrivateDirty(int) [RemovedMethod]
                error: Removed method android.os.Debug.MemoryInfo.getOtherPss(int) [RemovedMethod]
                error: Removed method android.os.Debug.MemoryInfo.getOtherSharedDirty(int) [RemovedMethod]
                warning: Field android.view.animation.Transformation.TYPE_ALPHA has changed value from nothing/not constant to 1 [ChangedValue]
                warning: Field android.view.animation.Transformation.TYPE_ALPHA has added 'final' qualifier [AddedFinal]
                warning: Field android.view.animation.Transformation.TYPE_BOTH has changed value from nothing/not constant to 3 [ChangedValue]
                warning: Field android.view.animation.Transformation.TYPE_BOTH has added 'final' qualifier [AddedFinal]
                warning: Field android.view.animation.Transformation.TYPE_IDENTITY has changed value from nothing/not constant to 0 [ChangedValue]
                warning: Field android.view.animation.Transformation.TYPE_IDENTITY has added 'final' qualifier [AddedFinal]
                warning: Field android.view.animation.Transformation.TYPE_MATRIX has changed value from nothing/not constant to 2 [ChangedValue]
                warning: Field android.view.animation.Transformation.TYPE_MATRIX has added 'final' qualifier [AddedFinal]
                warning: Method java.nio.CharBuffer.subSequence has changed return type from CharSequence to java.nio.CharBuffer [ChangedType]
                """, // The last warning above is not right; seems to be a PSI jar loading bug. It returns the wrong return type!

            20 to """
                error: Removed method android.util.TypedValue.complexToDimensionNoisy(int,android.util.DisplayMetrics) [RemovedMethod]
                warning: Method org.json.JSONObject.keys has changed return type from java.util.Iterator to java.util.Iterator<java.lang.String> [ChangedType]
                warning: Field org.xmlpull.v1.XmlPullParserFactory.features has changed type from java.util.HashMap to java.util.HashMap<java.lang.String, java.lang.Boolean> [ChangedType]
                """,
            26 to """
                warning: Field android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_PERCEPTIBLE has changed value from 130 to 230 [ChangedValue]
                warning: Field android.content.pm.PermissionInfo.PROTECTION_MASK_FLAGS has changed value from 4080 to 65520 [ChangedValue]
                """,
            27 to ""
        )

        val suppressLevels = mapOf(
            1 to "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,ChangedDeprecated",
            7 to "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,ChangedDeprecated",
            18 to "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,RemovedMethod,ChangedDeprecated,ChangedThrows,AddedFinal,ChangedType,RemovedDeprecatedClass",
            26 to "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,RemovedMethod,ChangedDeprecated,ChangedThrows,AddedFinal,RemovedClass,RemovedDeprecatedClass",
            27 to "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,RemovedMethod,ChangedDeprecated,ChangedThrows,AddedFinal"
        )

        val loadPrevAsSignature = false

        for (apiLevel in 5..27) {
            if (!expected.containsKey(apiLevel)) {
                continue
            }
            println("Checking compatibility from API level ${apiLevel - 1} to $apiLevel...")
            val current = getAndroidJar(apiLevel)
            if (current == null) {
                println("Couldn't find $current: Check that pwd for test is correct. Skipping this test.")
                return
            }

            val previous = getAndroidJar(apiLevel - 1)
            if (previous == null) {
                println("Couldn't find $previous: Check that pwd for test is correct. Skipping this test.")
                return
            }
            val previousApi = previous.path

            // PSI based check

            check(
                extraArguments = arrayOf(
                    "--omit-locations",
                    ARG_HIDE,
                    suppressLevels[apiLevel]
                        ?: "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,ChangedDeprecated,RemovedField,RemovedClass,RemovedDeprecatedClass" +
                        (if ((apiLevel == 19 || apiLevel == 20) && loadPrevAsSignature) ",ChangedType" else "")

                ),
                expectedIssues = expected[apiLevel]?.trimIndent() ?: "",
                checkCompatibilityApi = previousApi,
                apiJar = current
            )

            // Signature based check
            if (apiLevel >= 21) {
                // Check signature file checks. We have .txt files for API level 14 and up, but there are a
                // BUNCH of problems in older signature files that make the comparisons not work --
                // missing type variables in class declarations, missing generics in method signatures, etc.
                val signatureFile = File("../../prebuilts/sdk/${apiLevel - 1}/public/api/android.txt")
                if (!(signatureFile.isFile)) {
                    println("Couldn't find $signatureFile: Check that pwd for test is correct. Skipping this test.")
                    return
                }
                val previousSignatureApi = signatureFile.readText(UTF_8)

                check(
                    extraArguments = arrayOf(
                        "--omit-locations",
                        ARG_HIDE,
                        suppressLevels[apiLevel]
                            ?: "AddedPackage,AddedClass,AddedMethod,AddedInterface,AddedField,ChangedDeprecated,RemovedField,RemovedClass,RemovedDeprecatedClass"
                    ),
                    expectedIssues = expected[apiLevel]?.trimIndent() ?: "",
                    checkCompatibilityApi = previousSignatureApi,
                    apiJar = current
                )
            }
        }
    }

    @Test
    fun `Ignore hidden references`() {
        check(
            expectedIssues = """
                """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class MyClass {
                    ctor public MyClass();
                    method public void method1(test.pkg.Hidden);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class MyClass {
                        public void method1(Hidden hidden) { }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    /** @hide */
                    public class Hidden {
                    }
                    """
                )
            ),
            extraArguments = arrayOf(
                ARG_HIDE, "ReferencesHidden",
                ARG_HIDE, "UnavailableSymbol",
                ARG_HIDE, "HiddenTypeParameter"
            )
        )
    }

    @Test
    fun `Fail on compatible changes that affect signature file contents`() {
        // Regression test for 122916999
        check(
            extraArguments = arrayOf(ARG_NO_NATIVE_DIFF),
            allowCompatibleDifferences = false,
            expectedFail = """
                Aborting: Your changes have resulted in differences in the signature file
                for the public API.

                The changes may be compatible, but the signature file needs to be updated.

                Diffs:
                @@ -5 +5
                      ctor public MyClass();
                -     method public void method2();
                      method public void method1();
                @@ -7 +6
                      method public void method1();
                +     method public void method2();
                      method public void method3();
            """.trimIndent(),
            compatibilityMode = false,
            // Methods in order
            checkCompatibilityApi = """
                package test.pkg {

                  public class MyClass {
                    ctor public MyClass();
                    method public void method2();
                    method public void method1();
                    method public void method3();
                    method public void method4();
                  }

                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class MyClass {
                        public void method1() { }
                        public void method2() { }
                        public void method3() { }
                        public native void method4();
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Empty bundle files`() {
        // Regression test for 124333557
        // Makes sure we properly handle conflicting definitions of a java file in separate source roots
        check(
            expectedIssues = "",
            compatibilityMode = false,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package com.android.location.provider {
                  public class LocationProviderBase1 {
                    ctor public LocationProviderBase1();
                    method public void onGetStatus(android.os.Bundle!);
                  }
                  public class LocationProviderBase2 {
                    ctor public LocationProviderBase2();
                    method public void onGetStatus(android.os.Bundle!);
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    "src2/com/android/location/provider/LocationProviderBase1.java",
                    """
                    /** Something */
                    package com.android.location.provider;
                    """
                ),
                java(
                    "src/com/android/location/provider/LocationProviderBase1.java",
                    """
                    package com.android.location.provider;
                    import android.os.Bundle;

                    public class LocationProviderBase1 {
                        public void onGetStatus(Bundle bundle) { }
                    }
                    """
                ),
                // Try both combinations (empty java file both first on the source path
                // and second on the source path)
                java(
                    "src/com/android/location/provider/LocationProviderBase2.java",
                    """
                    /** Something */
                    package com.android.location.provider;
                    """
                ),
                java(
                    "src/com/android/location/provider/LocationProviderBase2.java",
                    """
                    package com.android.location.provider;
                    import android.os.Bundle;

                    public class LocationProviderBase2 {
                        public void onGetStatus(Bundle bundle) { }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Check parameterized return type nullability`() {
        // Regression test for 130567941
        check(
            expectedIssues = "",
            compatibilityMode = false,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package androidx.coordinatorlayout.widget {
                  public class CoordinatorLayout {
                    ctor public CoordinatorLayout();
                    method public java.util.List<android.view.View!> getDependencies();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package androidx.coordinatorlayout.widget;

                    import java.util.List;
                    import androidx.annotation.NonNull;
                    import android.view.View;

                    public class CoordinatorLayout {
                        @NonNull
                        public List<View> getDependencies() {
                            throw Exception("Not implemented");
                        }
                    }
                    """
                ),
                androidxNonNullSource
            ),
            extraArguments = arrayOf(ARG_HIDE_PACKAGE, "androidx.annotation")
        )
    }

    @Test
    fun `Check return type changing package`() {
        // Regression test for 130567941
        check(
            expectedIssues = """
            TESTROOT/load-api.txt:7: error: Method test.pkg.sample.SampleClass.convert has changed return type from Number to java.lang.Number [ChangedType]
            """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package test.pkg.sample {
                  public abstract class SampleClass {
                    method public <Number> Number! convert(Number);
                    method public <Number> Number! convert(Number);
                  }
                }
                """,
            signatureSource = """
                // Signature format: 3.0
                package test.pkg.sample {
                  public abstract class SampleClass {
                    // Here the generic type parameter applies to both the function argument and the function return type
                    method public <Number> Number! convert(Number);
                    // Here the generic type parameter applies to the function argument but not the function return type
                    method public <Number> java.lang.Number! convert(Number);
                  }
                }
            """
        )
    }

    @Test
    fun `Check generic type argument when showUnannotated is explicitly enabled`() {
        // Regression test for 130567941
        check(
            expectedIssues = """
            """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package androidx.versionedparcelable {
                  public abstract class VersionedParcel {
                    method public <T> T![]! readArray();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package androidx.versionedparcelable;

                    public abstract class VersionedParcel {
                        private VersionedParcel() { }

                        public <T> T[] readArray() { return null; }
                    }
                    """
                )
            ),
            extraArguments = arrayOf(ARG_SHOW_UNANNOTATED, ARG_SHOW_ANNOTATION, "androidx.annotation.RestrictTo")
        )
    }

    @Test
    fun `Check using parameterized arrays as type parameters`() {
        check(
            format = FileFormat.V3,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import java.util.ArrayList;
                    import java.lang.Exception;

                    public class SampleArray<D extends ArrayList> extends ArrayList<D[]> {
                        public D[] get(int index) {
                            throw Exception("Not implemented");
                        }
                    }
                    """
                )
            ),

            checkCompatibilityApi = """
                // Signature format: 3.0
                package test.pkg {
                  public class SampleArray<D extends java.util.ArrayList> extends java.util.ArrayList<D[]> {
                    ctor public SampleArray();
                    method public D![]! get(int);
                  }
                }
                """
        )
    }

    @Test
    fun `Check implicit containing class`() {
        // Regression test for 131633221
        check(
            expectedIssues = """
            src/androidx/core/app/NotificationCompat.java:5: error: Added class androidx.core.app.NotificationCompat [AddedClass]
            """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package androidx.core.app {
                  public static class NotificationCompat.Builder {
                    ctor public NotificationCompat.Builder();
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package androidx.core.app;

                    import android.content.Context;

                    public class NotificationCompat {
                      private NotificationCompat() {
                      }
                      public static class Builder {
                      }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `New default method on annotation`() {
        // Regression test for 134754815
        check(
            expectedIssues = """
            src/androidx/room/Relation.java:5: error: Added method androidx.room.Relation.IHaveNoDefault() [AddedAbstractMethod]
            """,
            compatibilityMode = false,
            inputKotlinStyleNulls = true,
            outputKotlinStyleNulls = true,
            checkCompatibilityApi = """
                // Signature format: 3.0
                package androidx.room {
                  public @interface Relation {
                  }
                }
                """,
            sourceFiles = arrayOf(
                java(
                    """
                    package androidx.room;

                    public @interface Relation {
                        String IHaveADefault() default "";
                        String IHaveNoDefault();
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Changing static qualifier on inner classes with no public constructors`() {
        check(
            expectedIssues = """
                TESTROOT/load-api.txt:11: error: Class test.pkg.ParentClass.AnotherBadInnerClass changed 'static' qualifier [ChangedStatic]
                TESTROOT/load-api.txt:8: error: Class test.pkg.ParentClass.BadInnerClass changed 'static' qualifier [ChangedStatic]
            """,
            compatibilityMode = false,
            checkCompatibilityApi = """
                package test.pkg {
                  public class ParentClass {
                  }
                  public static class ParentClass.OkInnerClass {
                  }
                  public class ParentClass.AnotherOkInnerClass {
                  }
                  public static class ParentClass.BadInnerClass {
                    ctor public BadInnerClass();
                  }
                  public class ParentClass.AnotherBadInnerClass {
                    ctor public AnotherBadInnerClass();
                  }
                }
                """,
            signatureSource = """
                package test.pkg {
                  public class ParentClass {
                  }
                  public class ParentClass.OkInnerClass {
                  }
                  public static class ParentClass.AnotherOkInnerClass {
                  }
                  public class ParentClass.BadInnerClass {
                    ctor public BadInnerClass();
                  }
                  public static class ParentClass.AnotherBadInnerClass {
                    ctor public AnotherBadInnerClass();
                  }
                }
                """
        )
    }

    // TODO: Check method signatures changing incompatibly (look especially out for adding new overloaded
    // methods and comparator getting confused!)
    //   ..equals on the method items should actually be very useful!
}
