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

package com.android.tools.metalava

import org.junit.Test

class JDiffXmlTest : DriverTest() {

    @Test
    fun `Loading a signature file and writing the API back out`() {
        check(
            compatibilityMode = true,
            signatureSource =
            """
            package test.pkg {
              public deprecated class MyTest {
                ctor public MyTest();
                method public deprecated int clamp(int);
                method public java.lang.Double convert(java.lang.Float);
                field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                field public deprecated java.lang.Number myNumber;
              }
            }
            """,
            apiXml =
            """
            <api>
            <package name="test.pkg"
            >
            <class name="MyTest"
             extends="java.lang.Object"
             abstract="false"
             static="false"
             final="false"
             deprecated="deprecated"
             visibility="public"
            >
            <constructor name="MyTest"
             type="test.pkg.MyTest"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </constructor>
            <method name="clamp"
             return="int"
             abstract="false"
             native="false"
             synchronized="false"
             static="false"
             final="false"
             deprecated="deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            </method>
            <method name="convert"
             return="java.lang.Double"
             abstract="false"
             native="false"
             synchronized="false"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="java.lang.Float">
            </parameter>
            </method>
            <field name="ANY_CURSOR_ITEM_TYPE"
             type="java.lang.String"
             transient="false"
             volatile="false"
             value="&quot;vnd.android.cursor.item/*&quot;"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </field>
            <field name="myNumber"
             type="java.lang.Number"
             transient="false"
             volatile="false"
             static="false"
             final="false"
             deprecated="deprecated"
             visibility="public"
            >
            </field>
            </class>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Abstract interfaces`() {
        check(
            compatibilityMode = true,
            format = FileFormat.V2,
            signatureSource =
            """
            // Signature format: 2.0
            package test.pkg {
              public interface MyBaseInterface {
                method public abstract void fun(int, String);
              }
            }
            """,
            apiXml =
            """
            <api>
            <package name="test.pkg"
            >
            <interface name="MyBaseInterface"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <method name="fun"
             return="void"
             abstract="true"
             native="false"
             synchronized="false"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            <parameter name="null" type="java.lang.String">
            </parameter>
            </method>
            </interface>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Test generics, superclasses and interfaces`() {
        val source = """
            package a.b.c {
              public abstract interface MyStream<T, S extends a.b.c.MyStream<T, S>> {
              }
            }
            package test.pkg {
              public final class Foo extends java.lang.Enum {
                ctor public Foo(int);
                ctor public Foo(int, int);
                method public static test.pkg.Foo valueOf(java.lang.String);
                method public static final test.pkg.Foo[] values();
              }
              public abstract interface MyBaseInterface {
              }
              public abstract interface MyInterface<T> implements test.pkg.MyBaseInterface {
              }
              public abstract interface MyInterface2<T extends java.lang.Number> implements test.pkg.MyBaseInterface {
              }
              public static abstract class MyInterface2.Range<T extends java.lang.Comparable<? super T>> {
                ctor public MyInterface2.Range();
              }
              public static class MyInterface2.TtsSpan<C extends test.pkg.MyInterface<?>> {
                ctor public MyInterface2.TtsSpan();
              }
              public final class Test<T> {
                ctor public Test();
              }
            }
            """
        check(
            compatibilityMode = true,
            signatureSource = source,
            apiXml =
            """
            <api>
            <package name="a.b.c"
            >
            <interface name="MyStream"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </interface>
            </package>
            <package name="test.pkg"
            >
            <class name="Foo"
             extends="java.lang.Enum"
             abstract="false"
             static="false"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            <method name="valueOf"
             return="test.pkg.Foo"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="java.lang.String">
            </parameter>
            </method>
            <method name="values"
             return="test.pkg.Foo[]"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </method>
            <constructor name="Foo"
             type="test.pkg.Foo"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            </constructor>
            <constructor name="Foo"
             type="test.pkg.Foo"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            <parameter name="null" type="int">
            </parameter>
            </constructor>
            <method name="valueOf"
             return="test.pkg.Foo"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="java.lang.String">
            </parameter>
            </method>
            <method name="values"
             return="test.pkg.Foo[]"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </method>
            </class>
            <interface name="MyBaseInterface"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </interface>
            <interface name="MyInterface"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <implements name="test.pkg.MyBaseInterface">
            </implements>
            </interface>
            <interface name="MyInterface2"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <implements name="test.pkg.MyBaseInterface">
            </implements>
            </interface>
            <class name="MyInterface2.Range"
             extends="java.lang.Object"
             abstract="true"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <constructor name="MyInterface2.Range"
             type="test.pkg.MyInterface2.Range"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </constructor>
            </class>
            <class name="MyInterface2.TtsSpan"
             extends="java.lang.Object"
             abstract="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <constructor name="MyInterface2.TtsSpan"
             type="test.pkg.MyInterface2.TtsSpan"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </constructor>
            </class>
            <class name="Test"
             extends="java.lang.Object"
             abstract="false"
             static="false"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            <constructor name="Test"
             type="test.pkg.Test"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </constructor>
            </class>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Test enums`() {
        val source = """
            package test.pkg {
              public final class Foo extends java.lang.Enum {
                ctor public Foo(int);
                ctor public Foo(int, int);
                method public static test.pkg.Foo valueOf(java.lang.String);
                method public static final test.pkg.Foo[] values();
                enum_constant public static final test.pkg.Foo A;
                enum_constant public static final test.pkg.Foo B;
              }
            }
            """
        check(
            compatibilityMode = false,
            signatureSource = source,
            apiXml =
            """
            <api>
            <package name="test.pkg"
            >
            <class name="Foo"
             extends="java.lang.Enum"
             abstract="false"
             static="false"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            <constructor name="Foo"
             type="test.pkg.Foo"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            </constructor>
            <constructor name="Foo"
             type="test.pkg.Foo"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            <parameter name="null" type="int">
            </parameter>
            </constructor>
            <method name="valueOf"
             return="test.pkg.Foo"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="java.lang.String">
            </parameter>
            </method>
            <method name="values"
             return="test.pkg.Foo[]"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </method>
            <field name="A"
             type="test.pkg.Foo"
             transient="false"
             volatile="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
             metalava:enumConstant="true"
            >
            </field>
            <field name="B"
             type="test.pkg.Foo"
             transient="false"
             volatile="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
             metalava:enumConstant="true"
            >
            </field>
            </class>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Test enums compat mode`() {
        val source = """
            package test.pkg {
              public final class Foo extends java.lang.Enum {
                ctor public Foo(int);
                ctor public Foo(int, int);
                method public static test.pkg.Foo valueOf(java.lang.String);
                method public static final test.pkg.Foo[] values();
                enum_constant public static final test.pkg.Foo A;
                enum_constant public static final test.pkg.Foo B;
              }
            }
            """
        check(
            compatibilityMode = true,
            signatureSource = source,
            apiXml =
            """
            <api>
            <package name="test.pkg"
            >
            <class name="Foo"
             extends="java.lang.Enum"
             abstract="false"
             static="false"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            <method name="valueOf"
             return="test.pkg.Foo"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="java.lang.String">
            </parameter>
            </method>
            <method name="values"
             return="test.pkg.Foo[]"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </method>
            <constructor name="Foo"
             type="test.pkg.Foo"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            </constructor>
            <constructor name="Foo"
             type="test.pkg.Foo"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            <parameter name="null" type="int">
            </parameter>
            </constructor>
            <method name="valueOf"
             return="test.pkg.Foo"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="java.lang.String">
            </parameter>
            </method>
            <method name="values"
             return="test.pkg.Foo[]"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </method>
            </class>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Throws Lists`() {
        check(
            compatibilityMode = true,
            signatureSource = """
                    package android.accounts {
                      public abstract interface AccountManagerFuture<V> {
                        method public abstract V getResult() throws android.accounts.OperationCanceledException, java.io.IOException, android.accounts.AuthenticatorException;
                        method public abstract V getResult(long, java.util.concurrent.TimeUnit) throws android.accounts.OperationCanceledException, java.io.IOException, android.accounts.AuthenticatorException;
                      }
                    }
                    """,
            apiXml =
            """
            <api>
            <package name="android.accounts"
            >
            <interface name="AccountManagerFuture"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <method name="getResult"
             return="V"
             abstract="true"
             native="false"
             synchronized="false"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <exception name="AuthenticatorException" type="android.accounts.AuthenticatorException">
            </exception>
            <exception name="IOException" type="java.io.IOException">
            </exception>
            <exception name="OperationCanceledException" type="android.accounts.OperationCanceledException">
            </exception>
            </method>
            <method name="getResult"
             return="V"
             abstract="true"
             native="false"
             synchronized="false"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="long">
            </parameter>
            <parameter name="null" type="java.util.concurrent.TimeUnit">
            </parameter>
            <exception name="AuthenticatorException" type="android.accounts.AuthenticatorException">
            </exception>
            <exception name="IOException" type="java.io.IOException">
            </exception>
            <exception name="OperationCanceledException" type="android.accounts.OperationCanceledException">
            </exception>
            </method>
            </interface>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Generics in interfaces`() {
        check(
            compatibilityMode = false,
            signatureSource = """
                    package android.accounts {
                      public class ArgbEvaluator implements android.animation.DefaultEvaluator<D> implements android.animation.TypeEvaluator<V> {
                      }
                    }
                    """,
            apiXml =
            """
            <api>
            <package name="android.accounts"
            >
            <class name="ArgbEvaluator"
             extends="java.lang.Object"
             abstract="false"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <implements name="android.animation.DefaultEvaluator&lt;D>">
            </implements>
            <implements name="android.animation.TypeEvaluator&lt;V>">
            </implements>
            <implements name="java.lang.implements">
            </implements>
            </class>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Type Parameter Mapping`() {
        check(
            compatibilityMode = false,
            signatureSource = """
                package test.pkg {
                  public interface AbstractList<D,E,F> extends test.pkg.List<A,B,C> {
                  }
                  public interface ConcreteList<G,H,I> extends test.pkg.AbstractList<D,E,F> {
                  }
                  public interface List<A,B,C> {
                  }
                }
                """,
            apiXml =
            """
            <api>
            <package name="test.pkg"
            >
            <interface name="AbstractList"
             extends="test.pkg.List&lt;A, B, C>"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </interface>
            <interface name="ConcreteList"
             extends="test.pkg.AbstractList&lt;D, E, F>"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </interface>
            <interface name="List"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </interface>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Half float short from signature file`() {
        check(
            compatibilityMode = false,
            signatureSource = """
                package test.pkg {
                  public class Test {
                    ctor public Test();
                    field public static final short LOWEST_VALUE = -1025; // 0xfffffbff
                  }
                }
            """,
            apiXml =
            """
                <api>
                <package name="test.pkg"
                >
                <class name="Test"
                 extends="java.lang.Object"
                 abstract="false"
                 static="false"
                 final="false"
                 deprecated="not deprecated"
                 visibility="public"
                >
                <constructor name="Test"
                 type="test.pkg.Test"
                 static="false"
                 final="false"
                 deprecated="not deprecated"
                 visibility="public"
                >
                </constructor>
                <field name="LOWEST_VALUE"
                 type="short"
                 transient="false"
                 volatile="false"
                 value="-1025"
                 static="true"
                 final="true"
                 deprecated="not deprecated"
                 visibility="public"
                >
                </field>
                </class>
                </package>
                </api>
            """
        )
    }

    @Test
    fun `Half float short from source`() {
        check(
            compatibilityMode = false,
            sourceFiles = arrayOf(
                java(
                    """
                      package test.pkg;
                      public class Test {
                        public static final short LOWEST_VALUE = (short) 0xfbff;
                      }
                      """
                )
            ),
            apiXml =
            """
                <api>
                <package name="test.pkg"
                >
                <class name="Test"
                 extends="java.lang.Object"
                 abstract="false"
                 static="false"
                 final="false"
                 deprecated="not deprecated"
                 visibility="public"
                >
                <constructor name="Test"
                 type="test.pkg.Test"
                 static="false"
                 final="false"
                 deprecated="not deprecated"
                 visibility="public"
                >
                </constructor>
                <field name="LOWEST_VALUE"
                 type="short"
                 transient="false"
                 volatile="false"
                 value="-1025"
                 static="true"
                 final="true"
                 deprecated="not deprecated"
                 visibility="public"
                >
                </field>
                </class>
                </package>
                </api>
            """
        )
    }

    @Test
    fun `Interface extends, compat mode`() {
        check(
            compatibilityMode = true,
            format = FileFormat.V1,
            signatureSource = """
            // Signature format: 2.0
            package android.companion {
              public interface DeviceFilter<D extends android.os.Parcelable> extends android.os.Parcelable {
              }
            }
            """,
            apiXml =
            """
            <api>
            <package name="android.companion"
            >
            <interface name="DeviceFilter"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <implements name="android.os.Parcelable">
            </implements>
            </interface>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Interface extends, non-compat mode`() {
        check(
            compatibilityMode = false,
            format = FileFormat.V2,
            signatureSource = """
            // Signature format: 2.0
            package android.companion {
              public interface DeviceFilter<D extends android.os.Parcelable> extends android.os.Parcelable {
              }
            }
            """,
            apiXml =
            """
            <api>
            <package name="android.companion"
            >
            <interface name="DeviceFilter"
             extends="android.os.Parcelable"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </interface>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Test default methods from signature files`() {
        // Ensure that we treat not just static but default methods in interfaces as non-abstract
        check(
            compatibilityMode = true,
            format = FileFormat.V1,
            signatureSource = """
                package test.pkg {
                  public abstract interface MethodHandleInfo {
                    method public static boolean refKindIsField(int);
                  }
                }
            """,
            apiXml =
            """
            <api>
            <package name="test.pkg"
            >
            <interface name="MethodHandleInfo"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <method name="refKindIsField"
             return="boolean"
             abstract="false"
             native="false"
             synchronized="false"
             static="true"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <parameter name="null" type="int">
            </parameter>
            </method>
            </interface>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Test partial signature files`() {
        // Partial signature files, such as the system and test files which contain only the
        // *diffs* relative to the base API, are tricky: They may for example list just an
        // inner class. See 122926140 for a scenario where this happens.
        check(
            compatibilityMode = true,
            format = FileFormat.V1,
            signatureSource = """
            // Signature format: 2.0
            package android {

              public static final class Manifest.permission {
                field public static final String ACCESS_AMBIENT_LIGHT_STATS = "android.permission.ACCESS_AMBIENT_LIGHT_STATS";
                field public static final String ACCESS_BROADCAST_RADIO = "android.permission.ACCESS_BROADCAST_RADIO";
                field public static final String ACCESS_CACHE_FILESYSTEM = "android.permission.ACCESS_CACHE_FILESYSTEM";
                field public static final String ACCESS_DRM_CERTIFICATES = "android.permission.ACCESS_DRM_CERTIFICATES";
              }
            }
            """,
            apiXml =
            """
            <api>
            <package name="android"
            >
            <class name="Manifest.permission"
             extends="java.lang.Object"
             abstract="false"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            <field name="ACCESS_AMBIENT_LIGHT_STATS"
             type="java.lang.String"
             transient="false"
             volatile="false"
             value="&quot;android.permission.ACCESS_AMBIENT_LIGHT_STATS&quot;"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </field>
            <field name="ACCESS_BROADCAST_RADIO"
             type="java.lang.String"
             transient="false"
             volatile="false"
             value="&quot;android.permission.ACCESS_BROADCAST_RADIO&quot;"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </field>
            <field name="ACCESS_CACHE_FILESYSTEM"
             type="java.lang.String"
             transient="false"
             volatile="false"
             value="&quot;android.permission.ACCESS_CACHE_FILESYSTEM&quot;"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </field>
            <field name="ACCESS_DRM_CERTIFICATES"
             type="java.lang.String"
             transient="false"
             volatile="false"
             value="&quot;android.permission.ACCESS_DRM_CERTIFICATES&quot;"
             static="true"
             final="true"
             deprecated="not deprecated"
             visibility="public"
            >
            </field>
            </class>
            </package>
            </api>
            """
        )
    }

    @Test
    fun `Spaces in type argument lists`() {
        // JDiff expects spaces in type argument lists
        // Regression test for 123140708
        check(
            compatibilityMode = false,
            format = FileFormat.V2,
            signatureSource = """
            // Signature format: 2.0
            package org.apache.http.impl.conn.tsccm {
              @Deprecated public class ConnPoolByRoute extends org.apache.http.impl.conn.tsccm.AbstractConnPool {
                field @Deprecated protected final java.util.Map<org.apache.http.conn.routing.HttpRoute,org.apache.http.impl.conn.tsccm.RouteSpecificPool> routeToPool;
                field @Deprecated protected java.util.Queue<org.apache.http.impl.conn.tsccm.WaitingThread> waitingThreads;
              }
            }
            package test.pkg {
              public abstract class MyClass extends HashMap<String,String> implements Map<String,String>  {
                field public Map<String,String> map;
              }
            }
            """,
            apiXml =
            """
            <api>
            <package name="org.apache.http.impl.conn.tsccm"
            >
            <class name="ConnPoolByRoute"
             extends="org.apache.http.impl.conn.tsccm.AbstractConnPool"
             abstract="false"
             static="false"
             final="false"
             deprecated="deprecated"
             visibility="public"
            >
            <field name="routeToPool"
             type="java.util.Map&lt;org.apache.http.conn.routing.HttpRoute, org.apache.http.impl.conn.tsccm.RouteSpecificPool>"
             transient="false"
             volatile="false"
             static="false"
             final="true"
             deprecated="deprecated"
             visibility="protected"
            >
            </field>
            <field name="waitingThreads"
             type="java.util.Queue&lt;org.apache.http.impl.conn.tsccm.WaitingThread>"
             transient="false"
             volatile="false"
             static="false"
             final="false"
             deprecated="deprecated"
             visibility="protected"
            >
            </field>
            </class>
            </package>
            <package name="test.pkg"
            >
            <class name="MyClass"
             extends="java.lang.HashMap&lt;String, String>"
             abstract="true"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            <implements name="java.lang.Map&lt;String, String>">
            </implements>
            <field name="map"
             type="java.lang.Map&lt;String, String>"
             transient="false"
             volatile="false"
             static="false"
             final="false"
             deprecated="not deprecated"
             visibility="public"
            >
            </field>
            </class>
            </package>
            </api>
            """
        )
    }
}