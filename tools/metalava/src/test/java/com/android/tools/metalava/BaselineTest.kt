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

class BaselineTest : DriverTest() {
    @Test
    fun `Check baseline`() {
        check(
            extraArguments = arrayOf(
                ARG_HIDE,
                "HiddenSuperclass",
                ARG_HIDE,
                "UnavailableSymbol",
                ARG_HIDE,
                "HiddenTypeParameter",
                ARG_ERROR,
                "ReferencesHidden"
            ),
            baseline = """
                // Baseline format: 1.0
                BothPackageInfoAndHtml: test/visible/package-info.java:
                    It is illegal to provide both a package-info.java file and a package.html file for the same package
                IgnoringSymlink: test/pkg/sub1/sub2/sub3:
                    Ignoring symlink during package.html discovery directory traversal
                ReferencesHidden: test.pkg.Foo#get(T):
                    Class test.pkg.Hidden2 is hidden but was referenced (as type parameter) from public method test.pkg.Foo.get(T)
                ReferencesHidden: test.pkg.Foo#getHidden1():
                    Class test.pkg.Hidden1 is not public but was referenced (as return type) from public method test.pkg.Foo.getHidden1()
                //ReferencesHidden: test.pkg.Foo#getHidden2():
                //    Class test.pkg.Hidden2 is hidden but was referenced (as return type) from public method test.pkg.Foo.getHidden2()
                ReferencesHidden: test.pkg.Foo#hidden1:
                    Class test.pkg.Hidden1 is not public but was referenced (as field type) from public field test.pkg.Foo.hidden1
                ReferencesHidden: test.pkg.Foo#hidden2:
                    Class test.pkg.Hidden2 is hidden but was referenced (as field type) from public field test.pkg.Foo.hidden2
                ReferencesHidden: test.pkg.Foo#method(test.pkg.Hidden1, test.pkg.Hidden2):
                    Class test.pkg.Hidden3 is hidden but was referenced (as exception) from public method test.pkg.Foo.method(test.pkg.Hidden1,test.pkg.Hidden2)
                ReferencesHidden: test.pkg.Foo#method(test.pkg.Hidden1, test.pkg.Hidden2) parameter #0:
                    Class test.pkg.Hidden1 is not public but was referenced (as parameter type) from public parameter hidden1 in test.pkg.Foo.method(test.pkg.Hidden1 hidden1, test.pkg.Hidden2 hidden2)
                ReferencesHidden: test.pkg.Foo#method(test.pkg.Hidden1, test.pkg.Hidden2) parameter #1:
                    Class test.pkg.Hidden2 is hidden but was referenced (as parameter type) from public parameter hidden2 in test.pkg.Foo.method(test.pkg.Hidden1 hidden1, test.pkg.Hidden2 hidden2)
            """,
            // Commented out above:
            expectedIssues = """
                src/test/pkg/Foo.java:9: error: Class test.pkg.Hidden2 is hidden but was referenced (as return type) from public method test.pkg.Foo.getHidden2() [ReferencesHidden]
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class Foo extends Hidden2 {
                        public Hidden1 hidden1;
                        public Hidden2 hidden2;
                        public void method(Hidden1 hidden1, Hidden2 hidden2) throws Hidden3 {
                        }
                        public <S extends Hidden1, T extends Hidden2> S get(T t) { return null; }
                        public Hidden1 getHidden1() { return null; }
                        public Hidden2 getHidden2() { return null; }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    // Implicitly not part of the API by being package private
                    class Hidden1 {
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    /** @hide */
                    public class Hidden2 {
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    /** @hide */
                    public class Hidden3 extends IOException {
                    }
                    """
                ),
                // Generate duplicate package-info & package.html warning: tests baseline functionality
                // around PSI elements
                java(
                    """
                    /**
                     * My package docs<br>
                     * <!-- comment -->
                     * Sample code: /** code here &#42;/
                     * Another line.<br>
                     */
                    package test.visible;
                    """
                ),
                source(
                    "src/test/visible/package.html",
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
                ).indented()
            ),
            projectSetup = { dir ->
                // Generate a symlink warning: tests baseline functionality around errors reported as file paths
                val file = File(dir, "src/test/pkg/sub1/sub2")
                file.mkdirs()
                val symlink = File(file, "sub3").toPath()
                java.nio.file.Files.createSymbolicLink(symlink, dir.toPath())
                val git = File(file, ".git").toPath()
                java.nio.file.Files.createSymbolicLink(git, dir.toPath())
            },
            api = """
                package test.pkg {
                  public class Foo {
                    ctor public Foo();
                    method public <S extends test.pkg.Hidden1, T extends test.pkg.Hidden2> S get(T);
                    method public test.pkg.Hidden1 getHidden1();
                    method public test.pkg.Hidden2 getHidden2();
                    method public void method(test.pkg.Hidden1, test.pkg.Hidden2) throws test.pkg.Hidden3;
                    field public test.pkg.Hidden1 hidden1;
                    field public test.pkg.Hidden2 hidden2;
                  }
                }
                """
        )
    }

    @Test
    fun `Check baseline with show annotations`() {
        // When using show annotations we should only reference errors that are present in the delta
        check(
            includeSystemApiAnnotations = true,
            extraArguments = arrayOf(
                ARG_SHOW_ANNOTATION, "android.annotation.TestApi",
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_HIDE_PACKAGE, "android.support.annotation",
                ARG_API_LINT
            ),
            baseline = """
            """,
            updateBaseline = """
                // Baseline format: 1.0
                PairedRegistration: android.pkg.RegistrationMethods#registerUnpaired2Callback(Runnable):
                    Found registerUnpaired2Callback but not unregisterUnpaired2Callback in android.pkg.RegistrationMethods
            """,
            expectedIssues = "",
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;
                    import android.annotation.TestApi;
                    import androidx.annotation.Nullable;

                    public class RegistrationMethods {
                        // Here we have 3 sets of correct registrations: both register and unregister are
                        // present from the full API that is part of the Test API.
                        // There is also 2 incorrect registrations: one in the base API and one in the test
                        // API.
                        //
                        // In the test API, we expect to only see the single missing test API one. The missing
                        // base one should only be in the base baseline.
                        //
                        // Furthermore, this test makes sure that when the set of methods to be consulted spans
                        // the two APIs (one hidden, one not) the code correctly finds both; e.g. iteration of
                        // the test API does see the full set of methods even if they're left out of the
                        // signature files and baselines.

                        public void registerOk1Callback(@Nullable Runnable r) { }
                        public void unregisterOk1Callback(@Nullable Runnable r) { }

                        /** @hide */
                        @TestApi
                        public void registerOk2Callback(@Nullable Runnable r) { }
                        /** @hide */
                        @TestApi
                        public void unregisterOk2Callback(@Nullable Runnable r) { }

                        // In the Test API, both methods are present
                        public void registerOk3Callback(@Nullable Runnable r) { }
                        /** @hide */
                        @TestApi
                        public void unregisterOk3Callback(@Nullable Runnable r) { }

                        public void registerUnpaired1Callback(@Nullable Runnable r) { }

                        /** @hide */
                        @TestApi
                        public void registerUnpaired2Callback(@Nullable Runnable r) { }
                    }
                    """
                ),
                testApiSource
            ),
            api = """
                package android.pkg {
                  public class RegistrationMethods {
                    method public void registerOk2Callback(java.lang.Runnable);
                    method public void registerUnpaired2Callback(java.lang.Runnable);
                    method public void unregisterOk2Callback(java.lang.Runnable);
                    method public void unregisterOk3Callback(java.lang.Runnable);
                  }
                }
                """
        )
    }

    @Test
    fun `Check merging`() {
        // Checks merging existing baseline with new baseline: here we have 2 issues that are no longer
        // in the code base, one issue that is in the code base before and after, and one new issue, and
        // all 4 end up in the merged baseline.
        check(
            extraArguments = arrayOf(
                ARG_HIDE,
                "HiddenSuperclass",
                ARG_HIDE,
                "UnavailableSymbol",
                ARG_HIDE,
                "HiddenTypeParameter",
                ARG_ERROR,
                "ReferencesHidden"
            ),
            baseline = """
                // Baseline format: 1.0
                BothPackageInfoAndHtml: test/visible/package-info.java:
                    It is illegal to provide both a package-info.java file and a package.html file for the same package
                IgnoringSymlink: test/pkg/sub1/sub2/sub3:
                    Ignoring symlink during package.html discovery directory traversal
                ReferencesHidden: test.pkg.Foo#hidden2:
                    Class test.pkg.Hidden2 is hidden but was referenced (as field type) from public field test.pkg.Foo.hidden2
            """,
            mergeBaseline = """
                // Baseline format: 1.0
                BothPackageInfoAndHtml: test/visible/package-info.java:
                    It is illegal to provide both a package-info.java file and a package.html file for the same package
                IgnoringSymlink: test/pkg/sub1/sub2/sub3:
                    Ignoring symlink during package.html discovery directory traversal
                ReferencesHidden: test.pkg.Foo#hidden1:
                    Class test.pkg.Hidden1 is not public but was referenced (as field type) from public field test.pkg.Foo.hidden1
                ReferencesHidden: test.pkg.Foo#hidden2:
                    Class test.pkg.Hidden2 is hidden but was referenced (as field type) from public field test.pkg.Foo.hidden2
            """,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    public class Foo extends Hidden2 {
                        public Hidden1 hidden1;
                        public Hidden2 hidden2;
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    // Implicitly not part of the API by being package private
                    class Hidden1 {
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;
                    /** @hide */
                    public class Hidden2 {
                    }
                    """
                )
            )
        )
    }
}