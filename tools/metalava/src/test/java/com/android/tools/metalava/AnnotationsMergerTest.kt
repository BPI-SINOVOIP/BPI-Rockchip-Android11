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

class AnnotationsMergerTest : DriverTest() {

    // TODO: Test what happens when we have conflicting data
    //   - NULLABLE_SOURCE on one non null on the other
    //   - annotation specified with different parameters (e.g @Size(4) vs @Size(6))
    // Test with jar file

    @Test
    fun `Signature files contain annotations`() {
        check(
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            includeSystemApiAnnotations = false,
            omitCommonPackages = false,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import androidx.annotation.NonNull;
                    import androidx.annotation.Nullable;
                    import android.annotation.IntRange;
                    import androidx.annotation.UiThread;

                    @UiThread
                    public class MyTest {
                        public @Nullable Number myNumber;
                        public @Nullable Double convert(@NonNull Float f) { return null; }
                        public @IntRange(from=10,to=20) int clamp(int i) { return 10; }
                    }"""
                ),
                uiThreadSource,
                intRangeAnnotationSource,
                androidxNonNullSource,
                androidxNullableSource
            ),
            // Skip the annotations themselves from the output
            extraArguments = arrayOf(
                ARG_HIDE_PACKAGE, "android.annotation",
                ARG_HIDE_PACKAGE, "androidx.annotation",
                ARG_HIDE_PACKAGE, "android.support.annotation"
            ),
            api = """
                package test.pkg {
                  @androidx.annotation.UiThread public class MyTest {
                    ctor public MyTest();
                    method @androidx.annotation.IntRange(from=10, to=20) public int clamp(int);
                    method @androidx.annotation.Nullable public java.lang.Double convert(@androidx.annotation.NonNull java.lang.Float);
                    field @androidx.annotation.Nullable public java.lang.Number myNumber;
                  }
                }
                """
        )
    }

    @Test
    fun `Merged class and method annotations with no arguments`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public class MyTest {
                        public Number myNumber;
                        public Double convert(Float f) { return null; }
                        public int clamp(int i) { return 10; }
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeXmlAnnotations = """<?xml version="1.0" encoding="UTF-8"?>
                <root>
                  <item name="test.pkg.MyTest">
                    <annotation name="android.support.annotation.UiThread" />
                  </item>
                  <item name="test.pkg.MyTest java.lang.Double convert(java.lang.Float)">
                    <annotation name="android.support.annotation.Nullable" />
                  </item>
                  <item name="test.pkg.MyTest java.lang.Double convert(java.lang.Float) 0">
                    <annotation name="android.support.annotation.NonNull" />
                  </item>
                  <item name="test.pkg.MyTest myNumber">
                    <annotation name="android.support.annotation.Nullable" />
                  </item>
                  <item name="test.pkg.MyTest int clamp(int)">
                    <annotation name="android.support.annotation.IntRange">
                      <val name="from" val="10" />
                      <val name="to" val="20" />
                    </annotation>
                  </item>
                  <item name="test.pkg.MyTest int clamp(int) 0">
                    <annotation name='org.jetbrains.annotations.Range'>
                      <val name="from" val="-1"/>
                      <val name="to" val="java.lang.Integer.MAX_VALUE"/>
                    </annotation>
                  </item>
                  </root>
                """,
            api = """
                package test.pkg {
                  @androidx.annotation.UiThread public class MyTest {
                    ctor public MyTest();
                    method @androidx.annotation.IntRange(from=10, to=20) public int clamp(@androidx.annotation.IntRange(from=-1L, to=java.lang.Integer.MAX_VALUE) int);
                    method @androidx.annotation.Nullable public java.lang.Double convert(@androidx.annotation.NonNull java.lang.Float);
                    field @androidx.annotation.Nullable public java.lang.Number myNumber;
                  }
                }
                """
        )
    }

    @Test
    fun `Merge signature files`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public interface Appendable {
                        Appendable append(CharSequence csq) throws IOException;
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            inputKotlinStyleNulls = true,
            omitCommonPackages = false,
            mergeSignatureAnnotations = """
                package test.pkg {
                  public interface Appendable {
                    method public test.pkg.Appendable append(java.lang.CharSequence?);
                    method public test.pkg.Appendable append2(java.lang.CharSequence?);
                    method public java.lang.String! reverse(java.lang.String!);
                  }
                  public interface RandomClass {
                    method public test.pkg.Appendable append(java.lang.CharSequence);
                  }
                }
                """,
            api = """
                package test.pkg {
                  public interface Appendable {
                    method @androidx.annotation.NonNull public test.pkg.Appendable append(@androidx.annotation.Nullable java.lang.CharSequence);
                  }
                }
                """
        )
    }

    @Test
    fun `Merge qualifier annotations from Java stub files`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public interface Appendable {
                        Appendable append(CharSequence csq) throws IOException;
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.NonNull;
                import libcore.util.Nullable;

                public interface Appendable {
                    @NonNull Appendable append(@Nullable java.lang.CharSequence csq);
                }
                """,
            api = """
                package test.pkg {
                  public interface Appendable {
                    method @androidx.annotation.NonNull public test.pkg.Appendable append(@androidx.annotation.Nullable java.lang.CharSequence);
                  }
                }
                """
        )
    }

    @Test
    fun `Merge qualifier annotations from Java stub files onto stubs that are not in the API signature file`() {
        check(
            includeSystemApiAnnotations = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    public interface Appendable {
                        Appendable append(CharSequence csq) throws IOException;
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    /** @hide */
                    @android.annotation.TestApi
                    public interface ForTesting {
                        void foo();
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.NonNull;
                import libcore.util.Nullable;

                public interface Appendable {
                    @NonNull Appendable append(@Nullable java.lang.CharSequence csq);
                }
                """,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public interface Appendable {
                @android.annotation.NonNull
                public test.pkg.Appendable append(@android.annotation.Nullable java.lang.CharSequence csq);
                }
                """,
                """
                package test.pkg;
                /** @hide */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public interface ForTesting {
                public void foo();
                }
                """
            ),
            api = """
                package test.pkg {
                  public interface ForTesting {
                    method public void foo();
                  }
                }
                """
        )
    }

    @Test
    fun `Merge type use qualifier annotations from Java stub files`() {
        // See b/123223339
        check(
            sourceFiles = arrayOf(
                java(
                    """
                package test.pkg;

                public class Test {
                    private Test() { }
                    public void foo(Object... args) { }
                }
                """
                ),
                libcoreNonNullSource,
                libcoreNullableSource
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                public class Test {
                    public void foo(java.lang.@libcore.util.Nullable Object @libcore.util.NonNull ... args) { throw new RuntimeException("Stub!"); }
                }
                """,
            api = """
                package test.pkg {
                  public class Test {
                    method public void foo(@androidx.annotation.NonNull java.lang.Object...);
                  }
                }
                """,
            extraArguments = arrayOf(ARG_HIDE_PACKAGE, "libcore.util")
        )
    }

    @Test
    fun `Merge qualifier annotations from Java stub files making sure they apply to public members of hidden superclasses`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    class HiddenSuperClass {
                        @Override public String publicMethod(Object object) {return "";}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg;

                    public class PublicClass extends HiddenSuperClass {
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.NonNull;
                import libcore.util.Nullable;

                public class PublicClass {
                    @NonNull public @NonNull String publicMethod(@Nullable Object object) {return "";}
                }
                """,
            api = """
                package test.pkg {
                  public class PublicClass {
                    ctor public PublicClass();
                    method @androidx.annotation.NonNull public java.lang.String publicMethod(@androidx.annotation.Nullable java.lang.Object);
                  }
                }
                """
        )
    }

    @Test
    fun `Merge inclusion annotations from Java stub files`() {
        check(
            expectedIssues = "src/test/pkg/Example.annotated.java:6: error: @test.annotation.Show APIs must also be marked @hide: method test.pkg.Example.cShown() [UnhiddenSystemApi]",
            sourceFiles = arrayOf(
                java(
                    "src/test/pkg/Example.annotated.java",
                    """
                    package test.pkg;

                    public interface Example {
                        void aNotAnnotated();
                        void bHidden();
                        void cShown();
                    }
                    """
                ),
                java(
                    "src/test/pkg/HiddenExample.annotated.java",
                    """
                    package test.pkg;

                    public interface HiddenExample {
                        void method();
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            hideAnnotations = arrayOf("test.annotation.Hide"),
            showAnnotations = arrayOf("test.annotation.Show"),
            showUnannotated = true,
            mergeInclusionAnnotations = """
                package test.pkg;

                public interface Example {
                    void aNotAnnotated();
                    @test.annotation.Hide void bHidden();
                    @test.annotation.Hide @test.annotation.Show void cShown();
                }

                @test.annotation.Hide
                public interface HiddenExample {
                    void method();
                }
                """,
            api = """
                package test.pkg {
                  public interface Example {
                    method public void aNotAnnotated();
                    method public void cShown();
                  }
                }
                """
        )
    }

    @Test
    fun `Merge inclusion annotations from Java stub files using --show-single-annotation`() {
        check(
            sourceFiles = arrayOf(
                java(
                    "src/test/pkg/Example.annotated.java",
                    """
                    package test.pkg;

                    public interface Example {
                        void aNotAnnotated();
                        void bShown();
                    }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            extraArguments = arrayOf(
                ARG_HIDE_ANNOTATION, "test.annotation.Hide",
                ARG_SHOW_SINGLE_ANNOTATION, "test.annotation.Show"
            ),
            showUnannotated = true,
            mergeInclusionAnnotations = """
                package test.pkg;

                @test.annotation.Hide
                @test.annotation.Show
                public interface Example {
                    void aNotAnnotated();
                    @test.annotation.Show void bShown();
                }
                """,
            api = """
                package test.pkg {
                  public interface Example {
                    method public void bShown();
                  }
                }
                """
        )
    }
}
