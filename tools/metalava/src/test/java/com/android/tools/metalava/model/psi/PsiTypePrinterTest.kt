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

package com.android.tools.metalava.model.psi

import com.android.tools.lint.LintCoreApplicationEnvironment
import com.android.tools.lint.checks.infrastructure.TestFile
import com.android.tools.metalava.DriverTest
import com.android.tools.metalava.libcoreNonNullSource
import com.android.tools.metalava.libcoreNullableSource
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.nonNullSource
import com.android.tools.metalava.nullableSource
import com.android.tools.metalava.parseSources
import com.intellij.openapi.util.Disposer
import com.intellij.psi.JavaRecursiveElementVisitor
import com.intellij.psi.PsiAnnotation
import com.intellij.psi.PsiType
import com.intellij.psi.PsiTypeElement
import org.jetbrains.uast.UAnnotation
import org.jetbrains.uast.UMethod
import org.jetbrains.uast.UTypeReferenceExpression
import org.jetbrains.uast.UVariable
import org.jetbrains.uast.toUElement
import org.jetbrains.uast.visitor.AbstractUastVisitor
import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.util.function.Predicate

class PsiTypePrinterTest : DriverTest() {
    // @Test
    fun `Test class reference types`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.lang.String
            Printed: java.lang.String!

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Merged: [@Nullable]
            Printed: java.util.List<java.lang.String!>?

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@Nullable List<java.lang.@Nullable String>
            Printed: java.util.List<java.lang.String?>?

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@NonNull List<java.lang.@Nullable String>
            Printed: java.util.List<java.lang.String?>

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@Nullable List<java.lang.@NonNull String>
            Printed: java.util.List<java.lang.String>?

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@NonNull List<java.lang.@NonNull String>
            Printed: java.util.List<java.lang.String>

            Type: PsiClassReferenceType
            Canonical: java.util.Map<java.lang.String,java.lang.Number>
            Printed: java.util.Map<java.lang.String!, java.lang.Number!>!

            Type: PsiClassReferenceType
            Canonical: java.lang.Number
            Printed: java.lang.Number!
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                skip = setOf("int", "long"),
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public String myPlatformField1;
                           public List<String> getList(Map<String, Number> keys) { return null; }

                           public @androidx.annotation.Nullable String myNullableField;
                           public @androidx.annotation.Nullable List<String> myNullableFieldWithPlatformElement;

                           // Type use annotations
                           public java.util.@libcore.util.Nullable List<java.lang.@libcore.util.Nullable String> myNullableFieldWithNullableElement;
                           public java.util.@libcore.util.NonNull List<java.lang.@libcore.util.Nullable String> myNonNullFieldWithNullableElement;
                           public java.util.@libcore.util.Nullable List<java.lang.@libcore.util.NonNull String> myNullableFieldWithNonNullElement;
                           public java.util.@libcore.util.NonNull List<java.lang.@libcore.util.NonNull String> myNonNullFieldWithNonNullElement;
                        }
                        """
                    ),
                    nullableSource,
                    nonNullSource,
                    libcoreNonNullSource, // allows TYPE_USE
                    libcoreNullableSource
                )
            ).trimIndent()
        )
    }

    // @Test
    fun `Test class reference types without Kotlin style nulls`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@Nullable List<java.lang.@Nullable String>
            Printed: java.util.@libcore.util.Nullable List<java.lang.@libcore.util.Nullable String>

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@NonNull List<java.lang.@Nullable String>
            Printed: java.util.@libcore.util.NonNull List<java.lang.@libcore.util.Nullable String>

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@Nullable List<java.lang.@NonNull String>
            Printed: java.util.@libcore.util.Nullable List<java.lang.@libcore.util.NonNull String>

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Annotated: java.util.@NonNull List<java.lang.@NonNull String>
            Printed: java.util.@libcore.util.NonNull List<java.lang.@libcore.util.NonNull String>
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = false,
                skip = setOf("int", "long"),
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public java.util.@libcore.util.Nullable List<java.lang.@libcore.util.Nullable String> myNullableFieldWithNullableElement;
                           public java.util.@libcore.util.NonNull List<java.lang.@libcore.util.Nullable String> myNonNullFieldWithNullableElement;
                           public java.util.@libcore.util.Nullable List<java.lang.@libcore.util.NonNull String> myNullableFieldWithNonNullElement;
                           public java.util.@libcore.util.NonNull List<java.lang.@libcore.util.NonNull String> myNonNullFieldWithNonNullElement;
                        }
                        """
                    ),
                    nullableSource,
                    nonNullSource,
                    libcoreNonNullSource, // allows TYPE_USE
                    libcoreNullableSource
                )
            ).trimIndent()
        )
    }

    // @Test
    fun `Test merge annotations`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.lang.String
            Merged: [@Nullable]
            Printed: java.lang.String?

            Type: PsiArrayType
            Canonical: java.lang.String[]
            Merged: [@Nullable]
            Printed: java.lang.String![]?

            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.String>
            Merged: [@Nullable]
            Printed: java.util.List<java.lang.String!>?

            Type: PsiClassReferenceType
            Canonical: java.util.Map<java.lang.String,java.lang.Number>
            Merged: [@Nullable]
            Printed: java.util.Map<java.lang.String!, java.lang.Number!>?

            Type: PsiClassReferenceType
            Canonical: java.lang.Number
            Merged: [@Nullable]
            Printed: java.lang.Number?

            Type: PsiEllipsisType
            Canonical: java.lang.String...
            Merged: [@Nullable]
            Printed: java.lang.String!...
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                skip = setOf("int", "long", "void"),
                extraAnnotations = listOf("@libcore.util.Nullable"),
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public String myPlatformField1;
                           public String[] myPlatformField2;
                           public List<String> getList(Map<String, Number> keys) { return null; }
                           public void method(Number number) { }
                           public void ellipsis(String... args) { }
                        }
                        """
                    ),
                    nullableSource,
                    nonNullSource,
                    libcoreNonNullSource,
                    libcoreNullableSource
                )
            ).trimIndent()
        )
    }

    // @Test
    fun `Check other annotations than nullness annotations`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.Integer>
            Annotated: java.util.List<java.lang.@IntRange(from=5,to=10) Integer>
            Printed: java.util.List<java.lang.@androidx.annotation.IntRange(from=5,to=10) Integer!>!
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public List<java.lang.@androidx.annotation.IntRange(from=5,to=10) Integer> myRangeList;
                        }
                        """
                    ),
                    intRangeAsTypeUse
                ),
                include = setOf(
                    "java.lang.Integer",
                    "java.util.List<java.lang.Integer>"
                )
            ).trimIndent()
        )
    }

    // @Test
    fun `Test negative filtering`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.Integer>
            Annotated: java.util.List<java.lang.@IntRange(from=5,to=10) Integer>
            Printed: java.util.List<java.lang.Integer!>!
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public List<java.lang.@androidx.annotation.IntRange(from=5,to=10) Integer> myRangeList;
                        }
                        """
                    ),
                    intRangeAsTypeUse
                ),
                include = setOf(
                    "java.lang.Integer",
                    "java.util.List<java.lang.Integer>"
                ),
                // Remove the annotations via filtering
                filter = Predicate { false }
            ).trimIndent()
        )
    }

    // @Test
    fun `Test positive filtering`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.util.List<java.lang.Integer>
            Annotated: java.util.List<java.lang.@IntRange(from=5,to=10) Integer>
            Printed: java.util.List<java.lang.@androidx.annotation.IntRange(from=5,to=10) Integer!>!
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public List<java.lang.@androidx.annotation.IntRange(from=5,to=10) Integer> myRangeList;
                        }
                        """
                    ),
                    intRangeAsTypeUse
                ),
                include = setOf(
                    "java.lang.Integer",
                    "java.util.List<java.lang.Integer>"
                ),
                // Include the annotations via filtering
                filter = Predicate { true }
            ).trimIndent()
        )
    }

    // @Test
    fun `Test primitives`() {
        assertEquals(
            """
            Type: PsiPrimitiveType
            Canonical: int
            Printed: int

            Type: PsiPrimitiveType
            Canonical: long
            Printed: long

            Type: PsiPrimitiveType
            Canonical: void
            Printed: void

            Type: PsiPrimitiveType
            Canonical: int
            Annotated: @IntRange(from=5,to=10) int
            Printed: @androidx.annotation.IntRange(from=5,to=10) int
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public void foo() { }
                           public int myPrimitiveField;
                           public long myPrimitiveField2;
                           public void foo(@androidx.annotation.IntRange(from=5,to=10) int foo) { }
                        }
                        """
                    ),
                    nullableSource,
                    nonNullSource,
                    libcoreNonNullSource, // allows TYPE_USE
                    libcoreNullableSource,
                    intRangeAsTypeUse
                )
            ).trimIndent()
        )
    }

    // @Test
    fun `Test primitives with type use turned off`() {
        assertEquals(
            """
            Type: PsiPrimitiveType
            Canonical: int
            Printed: int

            Type: PsiPrimitiveType
            Canonical: long
            Printed: long

            Type: PsiPrimitiveType
            Canonical: void
            Printed: void

            Type: PsiPrimitiveType
            Canonical: int
            Annotated: @IntRange(from=5,to=10) int
            Printed: int
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = false,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public void foo() { }
                           public int myPrimitiveField;
                           public long myPrimitiveField2;
                           public void foo(@androidx.annotation.IntRange(from=5,to=10) int foo) { }
                        }
                        """
                    ),
                    nullableSource,
                    nonNullSource,
                    libcoreNonNullSource, // allows TYPE_USE
                    libcoreNullableSource,
                    intRangeAsTypeUse
                )
            ).trimIndent()
        )
    }

    // @Test
    fun `Test arrays`() {
        assertEquals(
            """
            Type: PsiArrayType
            Canonical: java.lang.String[]
            Printed: java.lang.String![]!

            Type: PsiArrayType
            Canonical: java.lang.String[][]
            Printed: java.lang.String![]![]!

            Type: PsiArrayType
            Canonical: java.lang.String[]
            Annotated: java.lang.@Nullable String @Nullable []
            Printed: java.lang.String?[]?

            Type: PsiArrayType
            Canonical: java.lang.String[]
            Annotated: java.lang.@NonNull String @Nullable []
            Printed: java.lang.String[]?

            Type: PsiArrayType
            Canonical: java.lang.String[]
            Annotated: java.lang.@Nullable String @NonNull []
            Printed: java.lang.String?[]
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public String[] myArray1;
                           public String[][] myArray2;
                           public java.lang.@libcore.util.Nullable String @libcore.util.Nullable [] array1;
                           public java.lang.@libcore.util.NonNull String @libcore.util.Nullable [] array2;
                           public java.lang.@libcore.util.Nullable String @libcore.util.NonNull [] array3;
                        }
                        """
                    ),
                    libcoreNonNullSource,
                    libcoreNullableSource
                ),
                skip = setOf("int", "java.lang.String")
            ).trimIndent()
        )
    }

    // @Test
    fun `Test ellipsis types`() {
        assertEquals(
            """
            Type: PsiEllipsisType
            Canonical: java.lang.String...
            Printed: java.lang.String!...

            Type: PsiEllipsisType
            Canonical: java.lang.String...
            Annotated: java.lang.@Nullable String @NonNull ...
            Merged: [@NonNull]
            Printed: java.lang.String?...
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                            // Ellipsis type
                           public void ellipsis1(String... args) { }
                           public void ellipsis2(java.lang.@libcore.util.Nullable String @libcore.util.NonNull ... args) { }
                        }
                        """
                    ),
                    libcoreNonNullSource,
                    libcoreNullableSource
                ),
                skip = setOf("void", "int", "java.lang.String")
            ).trimIndent()
        )
    }

    // @Test
    fun `Test wildcard type`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: T
            Annotated: @NonNull T
            Merged: [@NonNull]
            Printed: T

            Type: PsiWildcardType
            Canonical: ? super T
            Printed: ? super T

            Type: PsiClassReferenceType
            Canonical: T
            Printed: T!

            Type: PsiClassReferenceType
            Canonical: java.util.Collection<? extends T>
            Annotated: java.util.Collection<? extends @Nullable T>
            Printed: java.util.Collection<? extends T?>!

            Type: PsiWildcardType
            Canonical: ? extends T
            Annotated: ? extends @Nullable T
            Printed: ? extends T?

            Type: PsiClassReferenceType
            Canonical: T
            Annotated: @Nullable T
            Merged: [@Nullable]
            Printed: T?
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                            // Intersection type
                            @libcore.util.NonNull public static <T extends java.lang.String & java.lang.Comparable<? super T>> T foo(@libcore.util.Nullable java.util.Collection<? extends @libcore.util.Nullable T> coll) { return null; }
                        }
                        """
                    ),
                    libcoreNonNullSource,
                    libcoreNullableSource
                ),
                skip = setOf("int")
            ).trimIndent()
        )
    }

    // @Test
    fun `Test primitives in arrays cannot be null`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.util.List<int[]>
            Printed: java.util.List<int[]!>!

            Type: PsiClassReferenceType
            Canonical: java.util.List<boolean[][]>
            Printed: java.util.List<boolean[]![]!>!
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                            public List<int[]> ints;
                            public List<boolean[][]> booleans;
                        }
                        """
                    )
                ),
                skip = setOf("int")
            ).trimIndent()
        )
    }

    @Test
    fun `Test kotlin`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.lang.String
            Merged: [@NonNull]
            Printed: java.lang.String

            Type: PsiClassReferenceType
            Canonical: java.util.Map<java.lang.String,java.lang.String>
            Merged: [@Nullable]
            Printed: java.util.Map<java.lang.String,java.lang.String>?

            Type: PsiPrimitiveType
            Canonical: void
            Printed: void

            Type: PsiPrimitiveType
            Canonical: int
            Merged: [@NonNull]
            Printed: int

            Type: PsiClassReferenceType
            Canonical: java.lang.Integer
            Merged: [@Nullable]
            Printed: java.lang.Integer?

            Type: PsiEllipsisType
            Canonical: java.lang.String...
            Merged: [@NonNull]
            Printed: java.lang.String!...
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    kotlin(
                        """
                        package test.pkg
                        class Foo {
                            val foo1: String = "test1"
                            val foo2: String? = "test1"
                            val foo3: MutableMap<String?, String>? = null
                            fun method1(int: Int = 42,
                                int2: Int? = null,
                                byte: Int = 2 * 21,
                                str: String = "hello " + "world",
                                vararg args: String) { }
                        }
                        """
                    )
                )
            ).trimIndent()
        )
    }

    @Test
    fun `Test inner class references`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: test.pkg.MyClass.MyInner
            Printed: test.pkg.MyClass.MyInner!
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public test.pkg.MyClass.MyInner getObserver() { return null; }

                           public class MyInner {
                           }
                        }
                        """
                    )
                ),
                skip = setOf("void", "int", "java.lang.String")
            ).trimIndent()
        )
    }

    // @Test
    fun `Test type bounds`() {
        assertEquals(
            """
            Type: PsiClassReferenceType
            Canonical: java.util.List<? extends java.lang.Number>
            Printed: java.util.List<? extends java.lang.Number>!

            Type: PsiWildcardType
            Canonical: ? extends java.lang.Number
            Printed: ? extends java.lang.Number

            Type: PsiClassReferenceType
            Canonical: java.lang.Number
            Printed: java.lang.Number!

            Type: PsiClassReferenceType
            Canonical: java.util.Map<? extends java.lang.Number,? super java.lang.Number>
            Printed: java.util.Map<? extends java.lang.Number, ? super java.lang.Number>!

            Type: PsiWildcardType
            Canonical: ? super java.lang.Number
            Printed: ? super java.lang.Number
            """.trimIndent(),
            prettyPrintTypes(
                supportTypeUseAnnotations = true,
                kotlinStyleNulls = true,
                files = listOf(
                    java(
                        """
                        package test.pkg;
                        import java.util.List;
                        import java.util.Map;

                        @SuppressWarnings("ALL")
                        public class MyClass extends Object {
                           public void foo1(List<? extends Number> arg) { }
                           public void foo2(Map<? extends Number, ? super Number> arg) { }
                        }
                        """
                    )
                ),
                skip = setOf("void")
            ).trimIndent()
        )
    }

    data class Entry(
        val type: PsiType,
        val elementAnnotations: List<AnnotationItem>?,
        val canonical: String,
        val annotated: String,
        val printed: String
    )

    private fun prettyPrintTypes(
        files: List<TestFile>,
        filter: Predicate<Item>? = null,
        kotlinStyleNulls: Boolean = true,
        supportTypeUseAnnotations: Boolean = true,
        skip: Set<String> = emptySet(),
        include: Set<String> = emptySet(),
        extraAnnotations: List<String> = emptyList()
    ): String {
        val dir = createProject(*files.toTypedArray())
        val sourcePath = listOf(File(dir, "src"))

        val sourceFiles = mutableListOf<File>()
        fun addFiles(file: File) {
            if (file.isFile) {
                sourceFiles.add(file)
            } else {
                for (child in file.listFiles()) {
                    addFiles(child)
                }
            }
        }
        addFiles(dir)

        val classPath = mutableListOf<File>()
        val classPathProperty: String = System.getProperty("java.class.path")
        for (path in classPathProperty.split(':')) {
            val file = File(path)
            if (file.isFile) {
                classPath.add(file)
            }
        }

        val codebase = parseSources(
            sourceFiles, "test project",
            sourcePath = sourcePath, classpath = classPath
        )

        val results = LinkedHashMap<String, Entry>()
        fun handleType(type: PsiType, annotations: List<AnnotationItem> = emptyList()) {
            val key = type.getCanonicalText(true)
            if (results.contains(key)) {
                return
            }
            val canonical = type.getCanonicalText(false)
            if (skip.contains(key) || skip.contains(canonical)) {
                return
            }
            if (include.isNotEmpty() && !(include.contains(key) || include.contains(canonical))) {
                return
            }

            val mapAnnotations = false
            val printer = PsiTypePrinter(codebase, filter, mapAnnotations, kotlinStyleNulls, supportTypeUseAnnotations)

            var mergeAnnotations: MutableList<AnnotationItem>? = null
            if (extraAnnotations.isNotEmpty()) {
                val list = mutableListOf<AnnotationItem>()
                for (annotation in extraAnnotations) {
                    list.add(codebase.createAnnotation(annotation))
                }
                mergeAnnotations = list
            }
            if (annotations.isNotEmpty()) {
                val list = mutableListOf<AnnotationItem>()
                for (annotation in annotations) {
                    list.add(annotation)
                }
                if (mergeAnnotations == null) {
                    mergeAnnotations = list
                } else {
                    mergeAnnotations.addAll(list)
                }
            }

            val pretty = printer.getAnnotatedCanonicalText(type, mergeAnnotations)
            results[key] = Entry(type, mergeAnnotations, canonical, key, pretty)
        }

        for (unit in codebase.units) {
            unit.toUElement()?.accept(object : AbstractUastVisitor() {
                override fun visitMethod(node: UMethod): Boolean {
                    handle(node.returnType, node.annotations)

                    // Visit all the type elements in the method: this helps us pick up
                    // the type parameter lists for example which contains some interesting
                    // stuff such as type bounds
                    val psi = node.sourcePsi
                    psi?.accept(object : JavaRecursiveElementVisitor() {
                        override fun visitTypeElement(type: PsiTypeElement) {
                            handle(type.type, psiAnnotations = type.annotations)
                            super.visitTypeElement(type)
                        }
                    })
                    return super.visitMethod(node)
                }

                override fun visitVariable(node: UVariable): Boolean {
                    handle(node.type, node.annotations)
                    return super.visitVariable(node)
                }

                private fun handle(
                    type: PsiType?,
                    uastAnnotations: List<UAnnotation> = emptyList(),
                    psiAnnotations: Array<PsiAnnotation> = emptyArray()
                ) {
                    type ?: return

                    val annotations = mutableListOf<AnnotationItem>()
                    for (annotation in uastAnnotations) {
                        annotations.add(UAnnotationItem.create(codebase, annotation))
                    }
                    for (annotation in psiAnnotations) {
                        annotations.add(PsiAnnotationItem.create(codebase, annotation))
                    }

                    handleType(type, annotations)
                }

                override fun visitTypeReferenceExpression(node: UTypeReferenceExpression): Boolean {
                    handleType(node.type)
                    return super.visitTypeReferenceExpression(node)
                }
            })
        }

        val writer = StringWriter()
        val printWriter = PrintWriter(writer)

        results.keys.forEach { key ->
            val cleanKey = key.replace("libcore.util.", "").replace("androidx.annotation.", "")
            val entry = results[key]!!
            val string = entry.printed
            val type = entry.type
            val typeName = type.javaClass.simpleName
            val canonical = entry.canonical
            printWriter.printf("Type: %s\n", typeName)
            printWriter.printf("Canonical: %s\n", canonical)
            if (cleanKey != canonical) {
                printWriter.printf("Annotated: %s\n", cleanKey)
            }
            val elementAnnotations = entry.elementAnnotations
            if (elementAnnotations != null && elementAnnotations.isNotEmpty()) {
                printWriter.printf("Merged: %s\n", elementAnnotations.toString()
                    .replace("androidx.annotation.", "")
                    .replace("libcore.util.", ""))
            }
            printWriter.printf("Printed: %s\n\n", string)
        }

        Disposer.dispose(LintCoreApplicationEnvironment.get().parentDisposable)

        return writer.toString().removeSuffix("\n\n")
    }

    // TYPE_USE version of intRangeAnnotationSource
    private val intRangeAsTypeUse = java(
        """
        package androidx.annotation;
        import java.lang.annotation.*;
        import static java.lang.annotation.ElementType.*;
        import static java.lang.annotation.RetentionPolicy.SOURCE;
        @Retention(SOURCE)
        @Target({METHOD,PARAMETER,FIELD,LOCAL_VARIABLE,ANNOTATION_TYPE,TYPE_USE})
        public @interface IntRange {
            long from() default Long.MIN_VALUE;
            long to() default Long.MAX_VALUE;
        }
        """
    ).indented()
}
