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

package com.android.tools.metalava

import org.junit.Assert.assertSame
import org.junit.Test

class FileFormatTest {
    @Test
    fun `Check format parsing`() {
        assertSame(FileFormat.V1, FileFormat.parseHeader("""
                package test.pkg {
                  public class MyTest {
                    ctor public MyTest();
                  }
                }
        """.trimIndent()))

        assertSame(FileFormat.V2, FileFormat.parseHeader("""
            // Signature format: 2.0
            package libcore.util {
              @java.lang.annotation.Documented @java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.SOURCE) public @interface NonNull {
                method public abstract int from() default java.lang.Integer.MIN_VALUE;
                method public abstract int to() default java.lang.Integer.MAX_VALUE;
              }
            }
        """.trimIndent()))

        assertSame(FileFormat.V3, FileFormat.parseHeader("""
            // Signature format: 3.0
            package androidx.collection {
              public final class LruCacheKt {
                ctor public LruCacheKt();
              }
            }
        """.trimIndent()))

        assertSame(FileFormat.V2, FileFormat.parseHeader(
            "// Signature format: 2.0\r\n" +
                "package libcore.util {\\r\n" +
                "  @java.lang.annotation.Documented @java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.SOURCE) public @interface NonNull {\r\n" +
                "    method public abstract int from() default java.lang.Integer.MIN_VALUE;\r\n" +
                "    method public abstract int to() default java.lang.Integer.MAX_VALUE;\r\n" +
                "  }\r\n" +
                "}\r\n"))

        assertSame(FileFormat.BASELINE, FileFormat.parseHeader("""
                // Baseline format: 1.0
                BothPackageInfoAndHtml: test/visible/package-info.java:
                    It is illegal to provide both a package-info.java file and a package.html file for the same package
                IgnoringSymlink: test/pkg/sub1/sub2/sub3:
                    Ignoring symlink during package.html discovery directory traversal
        """.trimIndent()))

        assertSame(FileFormat.JDIFF, FileFormat.parseHeader("""
            <api>
            <package name="test.pkg"
            >
            </api>
        """.trimIndent()))

        assertSame(FileFormat.JDIFF, FileFormat.parseHeader("""
            <?xml version="1.0" encoding="utf-8"?>
            <api>
            <package name="test.pkg"
            >
            </api>
        """.trimIndent()))

        assertSame(FileFormat.SINCE_XML, FileFormat.parseHeader("""
            <?xml version="1.0" encoding="utf-8"?>
            <api version="2">
                <class name="android/hardware/Camera" since="1" deprecated="21">
                    <method name="&lt;init>()V"/>
                    <method name="addCallbackBuffer([B)V" since="8"/>
                    <method name="getLogo()Landroid/graphics/drawable/Drawable;"/>
                    <field name="ACTION_NEW_VIDEO" since="14" deprecated="25"/>
                </class>
            </api>
        """.trimIndent()))

        assertSame(FileFormat.UNKNOWN, FileFormat.parseHeader("""
            blah blah
        """.trimIndent()))

        assertSame(FileFormat.UNKNOWN, FileFormat.parseHeader("""
            <?xml version="1.0" encoding="utf-8"?>
            <manifest />
        """.trimIndent()))
    }
}