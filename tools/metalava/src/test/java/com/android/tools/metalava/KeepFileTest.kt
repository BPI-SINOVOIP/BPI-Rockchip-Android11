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

class KeepFileTest : DriverTest() {
    @Test
    fun `Generate Keep file`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    @SuppressWarnings("ALL")
                    public interface MyInterface<T extends Object>
                            extends MyBaseInterface {
                    }
                    """
                ), java(
                    """
                    package a.b.c;
                    @SuppressWarnings("ALL")
                    public interface MyStream<T, S extends MyStream<T, S>> extends java.lang.AutoCloseable {
                    }
                    """
                ), java(
                    """
                    package test.pkg;
                    @SuppressWarnings("ALL")
                    public interface MyInterface2<T extends Number>
                            extends MyBaseInterface {
                        class TtsSpan<C extends MyInterface<?>> { }
                        abstract class Range<T extends Comparable<? super T>> {
                            protected String myString;
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public interface MyBaseInterface {
                        void fun(int a, String b);
                    }
                    """
                )
            ),
            proguard = """
                -keep class a.b.c.MyStream {
                }
                -keep class test.pkg.MyBaseInterface {
                    public abstract void fun(int, java.lang.String);
                }
                -keep class test.pkg.MyInterface {
                }
                -keep class test.pkg.MyInterface2 {
                }
                -keep class test.pkg.MyInterface2${"$"}Range {
                    <init>();
                    protected java.lang.String myString;
                }
                -keep class test.pkg.MyInterface2${"$"}TtsSpan {
                    <init>();
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Primitive types`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class MyClass {
                        public int testMethodA(int a) {}
                        public boolean testMethodB(boolean a) {}
                        public float testMethodC(float a) {}
                        public double testMethodD(double a) {}
                        public byte testMethodE(byte a) {}
                    }
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClass {
                    <init>();
                    public int testMethodA(int);
                    public boolean testMethodB(boolean);
                    public float testMethodC(float);
                    public double testMethodD(double);
                    public byte testMethodE(byte);
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Primitive array types`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class MyClass {
                        public int[] testMethodA(int[] a) {}
                        public float[][] testMethodB(float[][] a) {}
                        public double[][][] testMethodC(double[][][] a) {}
                        public byte testMethodD(byte... a) {}
                    }
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClass {
                    <init>();
                    public int[] testMethodA(int[]);
                    public float[][] testMethodB(float[][]);
                    public double[][][] testMethodC(double[][][]);
                    public byte testMethodD(byte[]);
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Object Array parameters`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class MyClass {
                        public void testMethodA(String a) {}
                        public void testMethodB(Boolean[] a) {}
                        public void testMethodC(Integer... a) {}
                    }
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClass {
                    <init>();
                    public void testMethodA(java.lang.String);
                    public void testMethodB(java.lang.Boolean[]);
                    public void testMethodC(java.lang.Integer[]);
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Arrays with Inner class`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class MyClass {
                        public void testMethodA(InnerClass a) {}
                        public void testMethodB(InnerClass[] a) {}
                        public void testMethodC(InnerClass... a) {}
                        public class InnerClass {}
                    }
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClass {
                    <init>();
                    public void testMethodA(test.pkg.MyClass${"$"}InnerClass);
                    public void testMethodB(test.pkg.MyClass${"$"}InnerClass[]);
                    public void testMethodC(test.pkg.MyClass${"$"}InnerClass[]);
                }
                -keep class test.pkg.MyClass${"$"}InnerClass {
                    <init>();
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Conflicting Class Names in parameters`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class String {}
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class MyClass {
                        public void testMethodA(String a, String b) {}
                        public void testMethodB(String a, test.pkg.String b) {}
                        public void testMethodC(String a, java.lang.String b) {}
                        public void testMethodD(java.lang.String a, test.pkg.String b) {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class MyClassArrays {
                        public void testMethodA(String[] a, String[] b) {}
                        public void testMethodB(String[] a, test.pkg.String[] b) {}
                        public void testMethodC(String[] a, java.lang.String[] b) {}
                        public void testMethodD(java.lang.String... a, test.pkg.String... b) {}
                    }
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClass {
                    <init>();
                    public void testMethodA(test.pkg.String, test.pkg.String);
                    public void testMethodB(test.pkg.String, test.pkg.String);
                    public void testMethodC(test.pkg.String, java.lang.String);
                    public void testMethodD(java.lang.String, test.pkg.String);
                }
                -keep class test.pkg.MyClassArrays {
                    <init>();
                    public void testMethodA(test.pkg.String[], test.pkg.String[]);
                    public void testMethodB(test.pkg.String[], test.pkg.String[]);
                    public void testMethodC(test.pkg.String[], java.lang.String[]);
                    public void testMethodD(java.lang.String[], test.pkg.String[]);
                }
                -keep class test.pkg.String {
                    <init>();
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Multi dimensional arrays`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class String {}
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class MyClassArrays {
                        public void testMethodA(String[][] a, String[][] b) {}
                        public void testMethodB(String[][][] a, test.pkg.String[][][] b) {}
                        public void testMethodC(String[][] a, java.lang.String[][] b) {}
                        public class InnerClass {}
                        public void testMethodD(InnerClass[][] a) {}
                    }
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClassArrays {
                    <init>();
                    public void testMethodA(test.pkg.String[][], test.pkg.String[][]);
                    public void testMethodB(test.pkg.String[][][], test.pkg.String[][][]);
                    public void testMethodC(test.pkg.String[][], java.lang.String[][]);
                    public void testMethodD(test.pkg.MyClassArrays${"$"}InnerClass[][]);
                }
                -keep class test.pkg.MyClassArrays${"$"}InnerClass {
                    <init>();
                }
                -keep class test.pkg.String {
                    <init>();
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }

    @Test
    fun `Methods with arrays as the return type`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class MyClass {
                        public String[] testMethodA() {}
                        public String[][] testMethodB() {}
                        public String[][][] testMethodC() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class MyOtherClass {
                        public java.lang.String[] testMethodA() {}
                        public String[][] testMethodB() {}
                        public test.pkg.String[][][] testMethodC() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    public class String {}
                    """
                )
            ),
            proguard = """
                -keep class test.pkg.MyClass {
                    <init>();
                    public test.pkg.String[] testMethodA();
                    public test.pkg.String[][] testMethodB();
                    public test.pkg.String[][][] testMethodC();
                }
                -keep class test.pkg.MyOtherClass {
                    <init>();
                    public java.lang.String[] testMethodA();
                    public test.pkg.String[][] testMethodB();
                    public test.pkg.String[][][] testMethodC();
                }
                -keep class test.pkg.String {
                    <init>();
                }
                """,
            extraArguments = arrayOf(ARG_HIDE, "KotlinKeyword")
        )
    }
}