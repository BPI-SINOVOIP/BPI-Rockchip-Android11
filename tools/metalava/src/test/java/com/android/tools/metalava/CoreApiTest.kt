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

@file:Suppress("JavaDoc", "DanglingJavadoc")

package com.android.tools.metalava

import com.android.tools.lint.checks.infrastructure.TestFile
import com.android.tools.lint.checks.infrastructure.TestFiles
import org.junit.Test

/** Test to explore hidden versus public APIs via annotations */
class CoreApiTest : DriverTest() {
    @Test
    fun `Hidden with --hide-annotation`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                      /**
                       * Hide everything in this package:
                       */
                      @libcore.api.LibCoreHidden
                      package test.pkg;
                      """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    // Not included: hidden by default from package annotation
                    public class NotExposed {
                    }
                    """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    import libcore.api.IntraCoreApi;

                    /**
                     * Included because it is annotated with a --show-single-annotation
                     */
                    @libcore.api.LibCoreHidden
                    @IntraCoreApi
                    public class Exposed {
                        public void stillHidden() { }
                        public String stillHidden;
                        @IntraCoreApi
                        public void exposed() { }
                        @IntraCoreApi
                        public String exposed;

                        public class StillHidden {
                        }
                    }
                    """
                ).indented(),

                libcoreCoreApi,
                libcoreCoreHidden
            ),
            api =
            """
                package libcore.api {
                  public abstract class IntraCoreApi implements java.lang.annotation.Annotation {
                  }
                }
                package test.pkg {
                  public class Exposed {
                    method public void exposed();
                    field public java.lang.String exposed;
                  }
                }
                """,
            stubs = arrayOf(
                """
                /**
                 * Hide everything in this package:
                 */
                package test.pkg;
                """,
                // Specify target with [] since NotExposed is not included in the stubs, so
                // matching up stub expected files here with source inputs doesn't match
                """
                [test/pkg/Exposed.java]
                package test.pkg;
                /**
                 * Included because it is annotated with a --show-single-annotation
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Exposed {
                Exposed() { throw new RuntimeException("Stub!"); }
                public void exposed() { throw new RuntimeException("Stub!"); }
                public java.lang.String exposed;
                }
                """
            ),
            extraArguments = arrayOf(
                ARG_SHOW_SINGLE_ANNOTATION, "libcore.api.IntraCoreApi",
                ARG_HIDE_ANNOTATION, "libcore.api.LibCoreHidden"
            )
        )
    }

    @Test
    fun `Hidden with package javadoc and hiding default constructor explicitly`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                      /**
                       * Hide everything in this package:
                       * @hide
                       */
                      package test.pkg;
                      """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    // Not included: hidden by default from package annotation
                    public class NotExposed {
                    }
                    """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    import libcore.api.IntraCoreApi;

                    /**
                     * Included because it is annotated with a --show-single-annotation
                     * @hide
                     */
                    @IntraCoreApi
                    public class Exposed {
                        /** @hide */
                        public Exposed() { }
                        public void stillHidden() { }
                        @IntraCoreApi
                        public void exposed() { }

                        public class StillHidden {
                        }
                    }
                    """
                ).indented(),

                libcoreCoreApi,
                libcoreCoreHidden
            ),
            api =
            """
                package libcore.api {
                  public abstract class IntraCoreApi implements java.lang.annotation.Annotation {
                  }
                }
                package test.pkg {
                  public class Exposed {
                    method public void exposed();
                  }
                }
                """,
            stubs = arrayOf(
                """
                /**
                 * Hide everything in this package:
                 * @hide
                 */
                package test.pkg;
                """,
                // Specify target with [] since NotExposed is not included in the stubs, so
                // matching up stub expected files here with source inputs doesn't match
                """
                [test/pkg/Exposed.java]
                package test.pkg;
                /**
                 * Included because it is annotated with a --show-single-annotation
                 * @hide
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Exposed {
                Exposed() { throw new RuntimeException("Stub!"); }
                public void exposed() { throw new RuntimeException("Stub!"); }
                }
                """
            ),
            extraArguments = arrayOf(
                ARG_SHOW_SINGLE_ANNOTATION, "libcore.api.IntraCoreApi",
                ARG_HIDE_ANNOTATION, "libcore.api.LibCoreHidden"
            )
        )
    }

    @Test
    fun `Complain if annotating a member and the surrounding class is not included`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                      /**
                       * Hide everything in this package:
                       * @hide
                       */
                      package test.pkg;
                      """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    import libcore.api.IntraCoreApi;

                    /**
                    * Included because it is annotated with a --show-single-annotation
                    * @hide
                    */
                    public class Exposed {
                        public void stillHidden() { }
                        public String stillHidden;
                        @IntraCoreApi // error: can only expose methods in class also exposed
                        public void exposed() { }

                        @IntraCoreApi
                        public String exposed;

                        @IntraCoreApi // error: can only expose inner classes in exported outer class
                        public class StillHidden {
                        }
                    }
                    """
                ).indented(),

                libcoreCoreApi,
                libcoreCoreHidden
            ),
            api =
            """
                package libcore.api {
                  public abstract class IntraCoreApi implements java.lang.annotation.Annotation {
                  }
                }
                """,
            extraArguments = arrayOf(
                ARG_SHOW_SINGLE_ANNOTATION, "libcore.api.IntraCoreApi",
                ARG_HIDE_ANNOTATION, "libcore.api.LibCoreHidden"
            ),
            expectedIssues = """
            src/test/pkg/Exposed.java:11: error: Attempting to unhide method test.pkg.Exposed.exposed(), but surrounding class test.pkg.Exposed is hidden and should also be annotated with @libcore.api.IntraCoreApi [ShowingMemberInHiddenClass]
            src/test/pkg/Exposed.java:14: error: Attempting to unhide field test.pkg.Exposed.exposed, but surrounding class test.pkg.Exposed is hidden and should also be annotated with @libcore.api.IntraCoreApi [ShowingMemberInHiddenClass]
            src/test/pkg/Exposed.java:17: error: Attempting to unhide class test.pkg.Exposed.StillHidden, but surrounding class test.pkg.Exposed is hidden and should also be annotated with @libcore.api.IntraCoreApi [ShowingMemberInHiddenClass]
            """
        )
    }

    @Test
    fun `Hidden with --hide-meta-annotation`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    @libcore.api.LibCoreHiddenFeature
                    package test.pkg.hidden;
                    """
                ).indented(),

                java(
                    """
                    package test.pkg.hidden;
                    public class HiddenClass {
                    }
                    """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    public class ExposedClass {
                        @libcore.api.LibCoreHiddenFeature
                        public void hiddenMethod() { }
                        @libcore.api.LibCoreHiddenFeature
                        public String hiddenField;

                        public void exposedMethod() { }
                        public String exposedField;

                        @libcore.api.LibCoreHiddenFeature
                        public class HiddenInnerClass {
                            public void hiddenMethod() { }
                            public String hiddenField;
                        }
                    }
                    """
                ).indented(),

                java(
                    """
                    package test.pkg;
                    @libcore.api.LibCoreHiddenFeature
                    public class HiddenClass {
                        public void hiddenMethod() { }
                        public String hiddenField;
                    }
                    """
                ).indented(),

                libcoreCoreHiddenFeature,
                libcoreCoreMetaHidden
            ),
            api =
                """
                package libcore.api {
                  public abstract class LibCoreMetaHidden implements java.lang.annotation.Annotation {
                  }
                }
                package test.pkg {
                  public class ExposedClass {
                    ctor public ExposedClass();
                    method public void exposedMethod();
                    field public java.lang.String exposedField;
                  }
                }
                """,
            stubs = arrayOf(
                NO_STUB,
                NO_STUB,
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class ExposedClass {
                public ExposedClass() { throw new RuntimeException("Stub!"); }
                public void exposedMethod() { throw new RuntimeException("Stub!"); }
                public java.lang.String exposedField;
                }
                """,
                NO_STUB
            ),
            extraArguments = arrayOf(
                ARG_HIDE_META_ANNOTATION, "libcore.api.LibCoreMetaHidden"
            )
        )
    }
}

val libcoreCoreApi: TestFile = TestFiles.java(
    """
    package libcore.api;

    import static java.lang.annotation.ElementType.ANNOTATION_TYPE;
    import static java.lang.annotation.ElementType.CONSTRUCTOR;
    import static java.lang.annotation.ElementType.FIELD;
    import static java.lang.annotation.ElementType.METHOD;
    import static java.lang.annotation.ElementType.PACKAGE;
    import static java.lang.annotation.ElementType.TYPE;

    import java.lang.annotation.Retention;
    import java.lang.annotation.RetentionPolicy;
    import java.lang.annotation.Target;

    /**
     * @hide
     */
    @SuppressWarnings("ALL")
    @IntraCoreApi // @IntraCoreApi is itself part of the intra-core API
    @Target({TYPE, FIELD, METHOD, CONSTRUCTOR, ANNOTATION_TYPE, PACKAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IntraCoreApi {
    }
    """
).indented()

val libcoreCoreHidden: TestFile = TestFiles.java(
    """
    package libcore.api;

    import static java.lang.annotation.ElementType.ANNOTATION_TYPE;
    import static java.lang.annotation.ElementType.CONSTRUCTOR;
    import static java.lang.annotation.ElementType.FIELD;
    import static java.lang.annotation.ElementType.METHOD;
    import static java.lang.annotation.ElementType.PACKAGE;
    import static java.lang.annotation.ElementType.TYPE;

    import java.lang.annotation.Retention;
    import java.lang.annotation.RetentionPolicy;
    import java.lang.annotation.Target;

    /**
     * @hide
     */
    @Target({TYPE, FIELD, METHOD, CONSTRUCTOR, ANNOTATION_TYPE, PACKAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LibCoreHidden {
    }
    """
).indented()

/**
 * Annotation whose annotated elements should be hidden.
 */
val libcoreCoreHiddenFeature: TestFile = TestFiles.java(
    """
    package libcore.api;

    import static java.lang.annotation.ElementType.ANNOTATION_TYPE;
    import static java.lang.annotation.ElementType.CONSTRUCTOR;
    import static java.lang.annotation.ElementType.FIELD;
    import static java.lang.annotation.ElementType.METHOD;
    import static java.lang.annotation.ElementType.PACKAGE;
    import static java.lang.annotation.ElementType.TYPE;
    import static java.lang.annotation.RetentionPolicy.CLASS;

    import java.lang.annotation.Retention;

    @Retention(CLASS)
    @LibCoreHiddenFeature
    @LibCoreMetaHidden
    public @interface LibCoreHiddenFeature {
    }
    """
).indented()

/**
 * Meta-annotation used to denote an annotation whose annotated elements should
 * be hidden.
 */
val libcoreCoreMetaHidden: TestFile = TestFiles.java(
    """
    package libcore.api;

    import static java.lang.annotation.ElementType.ANNOTATION_TYPE;
    import static java.lang.annotation.RetentionPolicy.CLASS;

    import java.lang.annotation.Retention;
    import java.lang.annotation.Target;

    @Retention(CLASS)
    @Target({ANNOTATION_TYPE})
    public @interface LibCoreMetaHidden {
    }
    """
).indented()
