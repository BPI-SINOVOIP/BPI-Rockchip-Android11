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

import com.android.tools.lint.checks.infrastructure.TestFiles.source
import org.junit.Test
import java.io.File

/** Tests around symlinks in source trees */
class SymlinkTest : DriverTest() {
    @Test
    fun `Symlink test making sure we don't iterate forever`() {
        // This test checks two things:
        // (1) if there are symlinks in the source directory, we
        //     (a) don't recurse forever and (b) we emit a warning
        //     that symlink targets are ignored, and
        // (2) we ignore empty strings as paths. E.g.
        //      --source-path ""
        //     means to not look anywhere for the sources.

        // Read the pwd setting that the --pwd flag in extraArguments
        // below sets such that we can undo the damage for subsequent
        // tests.
        val before = System.getProperty("user.dir")
        try {
            check(
                expectedIssues = "TESTROOT/src/test/pkg/sub1/sub2/sub3: info: Ignoring symlink during package.html discovery directory traversal [IgnoringSymlink]",
                sourceFiles = arrayOf(
                    java(
                        """
                        package test.pkg;
                        import android.annotation.Nullable;
                        import android.annotation.NonNull;
                        public class Foo {
                            /** These are the docs for method1. */
                            @Nullable public Double method1(@NonNull Double factor1, @NonNull Double factor2) { }
                            /** These are the docs for method2. It can sometimes return null. */
                            @Nullable public Double method2(@NonNull Double factor1, @NonNull Double factor2) { }
                            @Nullable public Double method3(@NonNull Double factor1, @NonNull Double factor2) { }
                        }
                        """
                    ),
                    source(
                        "src/test/pkg/sub1/package.html",
                        """
                        <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
                        <!-- not a body tag: <body> -->
                        <html>
                        <body bgcolor="white">
                        My package docs<br>
                        <!-- comment -->
                        Sample code: /** code here */
                        Another line.<br>
                        </BODY>
                        </html>
                        """
                    ).indented(),
                    nonNullSource,
                    nullableSource
                ),
                projectSetup = { dir ->
                    // Add a symlink from deep in the source tree back out to the
                    // root, which makes a cycle
                    val file = File(dir, "src/test/pkg/sub1/sub2")
                    file.mkdirs()
                    val symlink = File(file, "sub3").toPath()
                    java.nio.file.Files.createSymbolicLink(symlink, dir.toPath())

                    val git = File(file, ".git").toPath()
                    java.nio.file.Files.createSymbolicLink(git, dir.toPath())
                },
                // Empty source path: don't pick up random directory stuff
                extraArguments = arrayOf(
                    "--pwd", temporaryFolder.root.path,
                    ARG_SOURCE_PATH, ""
                ),
                checkCompilation = false, // needs androidx.annotations in classpath
                stubs = arrayOf(
                    """
                    package test.pkg;
                    @SuppressWarnings({"unchecked", "deprecation", "all"})
                    public class Foo {
                    public Foo() { throw new RuntimeException("Stub!"); }
                    /** These are the docs for method1. */
                    @android.annotation.Nullable
                    public java.lang.Double method1(@android.annotation.NonNull java.lang.Double factor1, @android.annotation.NonNull java.lang.Double factor2) { throw new RuntimeException("Stub!"); }
                    /** These are the docs for method2. It can sometimes return null. */
                    @android.annotation.Nullable
                    public java.lang.Double method2(@android.annotation.NonNull java.lang.Double factor1, @android.annotation.NonNull java.lang.Double factor2) { throw new RuntimeException("Stub!"); }
                    @android.annotation.Nullable
                    public java.lang.Double method3(@android.annotation.NonNull java.lang.Double factor1, @android.annotation.NonNull java.lang.Double factor2) { throw new RuntimeException("Stub!"); }
                    }
                    """
                )
            )
        } finally {
            System.setProperty("user.dir", before)
        }
    }
}