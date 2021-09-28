package com.android.tools.metalava

import org.junit.Test

/**
 * Tests for the ARG_FORCE_CONVERT_TO_WARNING_NULLABILITY_ANNOTATIONS functionality,
 * which replaces @Nullable/@NonNull with @RecentlyNullable/@RecentlyNonNull
 */
class MarkPackagesAsRecentTest : DriverTest() {

    @Test
    fun `Basic MarkPackagesAsRecentTest test`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import androidx.annotation.Nullable;
                    public class Foo {
                        @Nullable
                        public void method() { }
                    }
                    """
                )
            ),

            extraArguments = arrayOf(
                ARG_FORCE_CONVERT_TO_WARNING_NULLABILITY_ANNOTATIONS, "*"
            ),

            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Foo {
                public Foo() { throw new RuntimeException("Stub!"); }
                @androidx.annotation.RecentlyNullable
                public void method() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `MarkPackagesAsRecent test with showAnnotation arguments`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import androidx.annotation.Nullable;
                    public class Foo {
                        @Nullable
                        public void method() { }
                    }
                    """
                )
            ),

            extraArguments = arrayOf(
                ARG_FORCE_CONVERT_TO_WARNING_NULLABILITY_ANNOTATIONS, "*",
                ARG_SHOW_ANNOTATION, "androidx.annotation.RestrictTo"
            ),

            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Foo {
                public Foo() { throw new RuntimeException("Stub!"); }
                @androidx.annotation.RecentlyNullable
                public void method() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }
}
