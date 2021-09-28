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

class ConvertTest : DriverTest() {

    @Test
    fun `Test conversion flag`() {
        check(
            compatibilityMode = true,
            convertToJDiff = listOf(
                ConvertData(
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                      }
                    }
                    """,
                    outputFile =
                    """
                    <api>
                    <package name="test.pkg"
                    >
                    <class name="MyTest1"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <constructor name="MyTest1"
                     type="test.pkg.MyTest1"
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
                ),
                ConvertData(
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest2 {
                      }
                    }
                    """,
                    outputFile =
                    """
                    <api>
                    <package name="test.pkg"
                    >
                    <class name="MyTest2"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </class>
                    </package>
                    </api>
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert new with compat mode and api strip`() {
        check(
            compatibilityMode = true,
            convertToJDiff = listOf(
                ConvertData(
                    strip = true,
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    baseApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        field public deprecated java.lang.Number myNumber;
                      }
                    }
                    """,
                    outputFile =
                    """
                    <api>
                    <package name="test.pkg"
                    >
                    <class name="MyTest1"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
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
                    </class>
                    <class name="MyTest2"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <constructor name="MyTest2"
                     type="test.pkg.MyTest2"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </constructor>
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
                    </class>
                    </package>
                    <package name="test.pkg.new"
                    >
                    <interface name="MyInterface"
                     abstract="true"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </interface>
                    <class name="MyTest3"
                     extends="java.lang.Object"
                     abstract="true"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </class>
                    <class name="MyTest4"
                     extends="java.lang.Object"
                     abstract="true"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <implements name="test.pkg.new.MyInterface">
                    </implements>
                    </class>
                    </package>
                    </api>
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert new without compat mode and no strip`() {
        check(
            compatibilityMode = false,
            convertToJDiff = listOf(
                ConvertData(
                    strip = false,
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    baseApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        field public deprecated java.lang.Number myNumber;
                      }
                    }
                    """,
                    outputFile =
                    """
                    <api name="convert-output1">
                    <package name="test.pkg"
                    >
                    <class name="MyTest1"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
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
                    </class>
                    <class name="MyTest2"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <constructor name="MyTest2"
                     type="test.pkg.MyTest2"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </constructor>
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
                    </class>
                    </package>
                    <package name="test.pkg.new"
                    >
                    <interface name="MyInterface"
                     abstract="true"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </interface>
                    <class name="MyTest3"
                     extends="java.lang.Object"
                     abstract="true"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <implements name="java.util.List">
                    </implements>
                    </class>
                    <class name="MyTest4"
                     extends="java.lang.Object"
                     abstract="true"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <implements name="test.pkg.new.MyInterface">
                    </implements>
                    </class>
                    </package>
                    </api>
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert nothing new`() {
        check(
            expectedOutput = "No API change detected, not generating diff",
            compatibilityMode = true,
            convertToJDiff = listOf(
                ConvertData(
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public class MyTest3 {
                      }
                    }
                    """,
                    baseApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public class MyTest3 {
                      }
                    }
                    """,
                    outputFile =
                    """
                    """
                )
            )
        )
    }

    @Test
    fun `Test doclava compat`() {
        // A few more differences
        check(
            compatibilityMode = true,
            convertToJDiff = listOf(
                ConvertData(
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public void method(java.util.List<String>);
                        field protected static final java.lang.String CRLF = "\r\n";
                        field protected static final byte[] CRLF_BYTES;
                      }
                    }
                    """,
                    outputFile =
                    """
                    <api>
                    <package name="test.pkg"
                    >
                    <class name="MyTest1"
                     extends="java.lang.Object"
                     abstract="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <constructor name="MyTest1"
                     type="test.pkg.MyTest1"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    </constructor>
                    <method name="method"
                     return="void"
                     abstract="false"
                     native="false"
                     synchronized="false"
                     static="false"
                     final="false"
                     deprecated="not deprecated"
                     visibility="public"
                    >
                    <parameter name="null" type="java.util.List&lt;String&gt;">
                    </parameter>
                    </method>
                    <field name="CRLF"
                     type="java.lang.String"
                     transient="false"
                     volatile="false"
                     value="&quot;\r\n&quot;"
                     static="true"
                     final="true"
                     deprecated="not deprecated"
                     visibility="protected"
                    >
                    </field>
                    <field name="CRLF_BYTES"
                     type="byte[]"
                     transient="false"
                     volatile="false"
                     value="null"
                     static="true"
                     final="true"
                     deprecated="not deprecated"
                     visibility="protected"
                    >
                    </field>
                    </class>
                    </package>
                    </api>
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert new to v2 without compat mode and no strip`() {
        check(
            compatibilityMode = false,
            convertToJDiff = listOf(
                ConvertData(
                    strip = false,
                    format = FileFormat.V2,
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    baseApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        field public deprecated java.lang.Number myNumber;
                      }
                    }
                    """,
                    outputFile =
                    """
                    // Signature format: 2.0
                    package test.pkg {
                      public class MyTest1 {
                        method public Double convert(Float);
                        field public static final String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public Double convert(Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert new to v1 signatures with compat mode and no strip`() {
        // Output is similar to the v2 format, but with fully qualified java.lang types
        // and fields not included
        check(
            compatibilityMode = false,
            convertToJDiff = listOf(
                ConvertData(
                    strip = false,
                    format = FileFormat.V1,
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public abstract interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    baseApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        field public deprecated java.lang.Number myNumber;
                      }
                    }
                    """,
                    outputFile =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        method public java.lang.Double convert(java.lang.Float);
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public abstract interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert v2 to v1`() {
        check(
            compatibilityMode = false,
            convertToJDiff = listOf(
                ConvertData(
                    strip = false,
                    format = FileFormat.V1,
                    fromApi =
                    """
                    // Signature format: 2.0
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method @Deprecated public int clamp(int);
                        method public Double convert(Float);
                        field public static final String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field @Deprecated public Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public Double convert(Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    outputFile =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public abstract interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert v1 to v2`() {
        check(
            compatibilityMode = false,
            convertToJDiff = listOf(
                ConvertData(
                    strip = false,
                    format = FileFormat.V2,
                    fromApi =
                    """
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method public deprecated int clamp(int);
                        method public java.lang.Double convert(java.lang.Float);
                        field public static final java.lang.String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field public deprecated java.lang.Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public java.lang.Double convert(java.lang.Float);
                      }
                    }
                    package test.pkg.new {
                      public abstract interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    outputFile =
                    """
                    // Signature format: 2.0
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method @Deprecated public int clamp(int);
                        method public Double convert(Float);
                        field public static final String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field @Deprecated public Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public Double convert(Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """
                )
            )
        )
    }

    @Test
    fun `Test convert v2 to v2`() {
        check(
            compatibilityMode = false,
            convertToJDiff = listOf(
                ConvertData(
                    strip = false,
                    format = FileFormat.V2,
                    fromApi =
                    """
                    // Signature format: 2.0
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method @Deprecated public int clamp(int);
                        method public Double convert(Float);
                        field public static final String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field @Annot @Annot.Nested @NonNull public String annotationLoaded;
                        field @Deprecated public Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public Double convert(Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """,
                    outputFile =
                    """
                    // Signature format: 2.0
                    package test.pkg {
                      public class MyTest1 {
                        ctor public MyTest1();
                        method @Deprecated public int clamp(int);
                        method public Double convert(Float);
                        field public static final String ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*";
                        field @Annot @Annot.Nested @NonNull public String annotationLoaded;
                        field @Deprecated public Number myNumber;
                      }
                      public class MyTest2 {
                        ctor public MyTest2();
                        method public Double convert(Float);
                      }
                    }
                    package test.pkg.new {
                      public interface MyInterface {
                      }
                      public abstract class MyTest3 implements java.util.List {
                      }
                      public abstract class MyTest4 implements test.pkg.new.MyInterface {
                      }
                    }
                    """
                )
            )
        )
    }
}
