package com.android.tools.metalava

import com.android.tools.lint.checks.infrastructure.TestFiles.source
import com.android.tools.metalava.model.psi.trimDocIndent
import com.google.common.io.Files
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.text.Charsets.UTF_8

/** Tests for the [DocAnalyzer] which enhances the docs */
class DocAnalyzerTest : DriverTest() {
    // TODO: Test @StringDef

    @Test
    fun `Basic documentation generation test`() {
        check(
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

                nonNullSource,
                nullableSource
            ),
            checkCompilation = false, // needs androidx.annotations in classpath
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Foo {
                public Foo() { throw new RuntimeException("Stub!"); }
                /**
                 * These are the docs for method1.
                 * @param factor1 This value must never be {@code null}.
                 * @param factor2 This value must never be {@code null}.
                 * @return This value may be {@code null}.
                 */
                @androidx.annotation.Nullable
                public java.lang.Double method1(@androidx.annotation.NonNull java.lang.Double factor1, @androidx.annotation.NonNull java.lang.Double factor2) { throw new RuntimeException("Stub!"); }
                /**
                 * These are the docs for method2. It can sometimes return null.
                 * @param factor1 This value must never be {@code null}.
                 * @param factor2 This value must never be {@code null}.
                 */
                @androidx.annotation.Nullable
                public java.lang.Double method2(@androidx.annotation.NonNull java.lang.Double factor1, @androidx.annotation.NonNull java.lang.Double factor2) { throw new RuntimeException("Stub!"); }
                /**
                 * @param factor1 This value must never be {@code null}.
                 * @param factor2 This value must never be {@code null}.
                 * @return This value may be {@code null}.
                 */
                @androidx.annotation.Nullable
                public java.lang.Double method3(@androidx.annotation.NonNull java.lang.Double factor1, @androidx.annotation.NonNull java.lang.Double factor2) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Fix first sentence handling`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package android.annotation;

                    import static java.lang.annotation.ElementType.*;
                    import static java.lang.annotation.RetentionPolicy.CLASS;
                    import java.lang.annotation.*;

                    /**
                     * Denotes that an integer parameter, field or method return value is expected
                     * to be a String resource reference (e.g. {@code android.R.string.ok}).
                     */
                    @Documented
                    @Retention(CLASS)
                    @Target({METHOD, PARAMETER, FIELD, LOCAL_VARIABLE})
                    public @interface StringRes {
                    }
                    """
                )
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package android.annotation;
                /**
                 * Denotes that an integer parameter, field or method return value is expected
                 * to be a String resource reference (e.g.&nbsp;{@code android.R.string.ok}).
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @java.lang.annotation.Documented
                @java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.CLASS)
                @java.lang.annotation.Target({java.lang.annotation.ElementType.METHOD, java.lang.annotation.ElementType.PARAMETER, java.lang.annotation.ElementType.FIELD, java.lang.annotation.ElementType.LOCAL_VARIABLE})
                public @interface StringRes {
                }
                """
            ),
            extraArguments = arrayOf(ARG_HIDE, "Typo") // "e.g. " correction should still run with Typo fixing is off.
        )
    }

    @Test
    fun `Fix typo replacement`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    /** This is an API for Andriod. Replace all occurrences: Andriod. */
                    public class Foo {
                        /** Ignore matches within words: xAndriodx */
                        public Foo() {
                        }
                    }
                    """
                )
            ),
            checkCompilation = true,
            docStubs = true,
            expectedIssues = "src/test/pkg/Foo.java:2: warning: Replaced Andriod with Android in the documentation for class test.pkg.Foo [Typo]",
            stubs = arrayOf(
                """
                package test.pkg;
                /** This is an API for Android. Replace all occurrences: Android. */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Foo {
                /** Ignore matches within words: xAndriodx */
                public Foo() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Document Permissions`() {
        check(
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.Manifest;
                    import android.annotation.RequiresPermission;

                    public class PermissionTest {
                        @RequiresPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                        public void test1() {
                        }

                        @RequiresPermission(allOf = Manifest.permission.ACCESS_COARSE_LOCATION)
                        public void test2() {
                        }

                        @RequiresPermission(anyOf = {Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION})
                        public void test3() {
                        }

                        @RequiresPermission(allOf = {Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCOUNT_MANAGER})
                        public void test4() {
                        }

                        @RequiresPermission(value=Manifest.permission.WATCH_APPOPS, conditional=true) // b/73559440
                        public void test5() {
                        }

                        @RequiresPermission(anyOf = {Manifest.permission.ACCESS_COARSE_LOCATION, "carrier privileges"})
                        public void test6() {
                        }

                        // Typo in marker
                        @RequiresPermission(anyOf = {Manifest.permission.ACCESS_COARSE_LOCATION, "carier priviliges"})
                        public void test6() {
                        }
                    }
                    """
                ),
                java(
                    """
                    package android;

                    public abstract class Manifest {
                        public static final class permission {
                            public static final String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                            public static final String ACCESS_FINE_LOCATION = "android.permission.ACCESS_FINE_LOCATION";
                            public static final String ACCOUNT_MANAGER = "android.permission.ACCOUNT_MANAGER";
                            public static final String WATCH_APPOPS = "android.permission.WATCH_APPOPS";
                        }
                    }
                    """
                ),
                requiresPermissionSource
            ),
            checkCompilation = false, // needs androidx.annotations in classpath
            expectedIssues = "src/test/pkg/PermissionTest.java:31: lint: Unrecognized permission `carier priviliges`; did you mean `carrier privileges`? [MissingPermission]",
            stubs = arrayOf(
                """
                package test.pkg;
                import android.Manifest;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class PermissionTest {
                public PermissionTest() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link android.Manifest.permission#ACCESS_COARSE_LOCATION}
                 */
                @androidx.annotation.RequiresPermission(android.Manifest.permission.ACCESS_COARSE_LOCATION)
                public void test1() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link android.Manifest.permission#ACCESS_COARSE_LOCATION}
                 */
                @androidx.annotation.RequiresPermission(allOf=android.Manifest.permission.ACCESS_COARSE_LOCATION)
                public void test2() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link android.Manifest.permission#ACCESS_COARSE_LOCATION} or {@link android.Manifest.permission#ACCESS_FINE_LOCATION}
                 */
                @androidx.annotation.RequiresPermission(anyOf={android.Manifest.permission.ACCESS_COARSE_LOCATION, android.Manifest.permission.ACCESS_FINE_LOCATION})
                public void test3() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link android.Manifest.permission#ACCESS_COARSE_LOCATION} and {@link android.Manifest.permission#ACCOUNT_MANAGER}
                 */
                @androidx.annotation.RequiresPermission(allOf={android.Manifest.permission.ACCESS_COARSE_LOCATION, android.Manifest.permission.ACCOUNT_MANAGER})
                public void test4() { throw new RuntimeException("Stub!"); }
                @androidx.annotation.RequiresPermission(value=android.Manifest.permission.WATCH_APPOPS, conditional=true)
                public void test5() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link android.Manifest.permission#ACCESS_COARSE_LOCATION} or {@link android.telephony.TelephonyManager#hasCarrierPrivileges carrier privileges}
                 */
                @androidx.annotation.RequiresPermission(anyOf={android.Manifest.permission.ACCESS_COARSE_LOCATION, "carrier privileges"})
                public void test6() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link android.Manifest.permission#ACCESS_COARSE_LOCATION} or "carier priviliges"
                 */
                @androidx.annotation.RequiresPermission(anyOf={android.Manifest.permission.ACCESS_COARSE_LOCATION, "carier priviliges"})
                public void test6() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Conditional Permission`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.Manifest;
                    import android.annotation.RequiresPermission;

                    // Scenario described in b/73559440
                    public class PermissionTest {
                        @RequiresPermission(value=Manifest.permission.WATCH_APPOPS, conditional=true)
                        public void test1() {
                        }
                    }
                    """
                ),
                java(
                    """
                    package android;

                    public abstract class Manifest {
                        public static final class permission {
                            public static final String WATCH_APPOPS = "android.permission.WATCH_APPOPS";
                        }
                    }
                    """
                ),
                requiresPermissionSource
            ),
            checkCompilation = false, // needs androidx.annotations in classpath
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class PermissionTest {
                public PermissionTest() { throw new RuntimeException("Stub!"); }
                @androidx.annotation.RequiresPermission(value=android.Manifest.permission.WATCH_APPOPS, conditional=true)
                public void test1() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }
    @Test
    fun `Document ranges`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.Manifest;
                    import android.annotation.IntRange;

                    public class RangeTest {
                        @IntRange(from = 10)
                        public int test1(@IntRange(from = 20) int range2) { return 15; }

                        @IntRange(from = 10, to = 20)
                        public int test2() { return 15; }

                        @IntRange(to = 100)
                        public int test3() { return 50; }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            docStubs = true,
            checkCompilation = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * @param range2 Value is 20 or greater
                 * @return Value is 10 or greater
                 */
                @androidx.annotation.IntRange(from=10)
                public int test1(@androidx.annotation.IntRange(from=20) int range2) { throw new RuntimeException("Stub!"); }
                /**
                 * @return Value is between 10 and 20 inclusive
                 */
                @androidx.annotation.IntRange(from=10, to=20)
                public int test2() { throw new RuntimeException("Stub!"); }
                /**
                 * @return Value is 100 or less
                 */
                @androidx.annotation.IntRange(to=100)
                public int test3() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Merging in documentation snippets from annotation memberDoc and classDoc`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import androidx.annotation.UiThread;
                    import androidx.annotation.WorkerThread;
                    @UiThread
                    public class RangeTest {
                        @WorkerThread
                        public int test1() { }
                    }
                    """
                ),
                uiThreadSource,
                workerThreadSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                /**
                 * Methods in this class must be called on the thread that originally created
                 * this UI element, unless otherwise noted. This is typically the
                 * main thread of your app. *
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @androidx.annotation.UiThread
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This method may take several seconds to complete, so it should
                 * only be called from a worker thread.
                 */
                @androidx.annotation.WorkerThread
                public int test1() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Warn about multiple threading annotations`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import androidx.annotation.UiThread;
                    import androidx.annotation.WorkerThread;
                    public class RangeTest {
                        @UiThread @WorkerThread
                        public int test1() { }
                    }
                    """
                ),
                uiThreadSource,
                workerThreadSource
            ),
            checkCompilation = true,
            expectedIssues = "src/test/pkg/RangeTest.java:5: lint: Found more than one threading annotation on method test.pkg.RangeTest.test1(); the auto-doc feature does not handle this correctly [MultipleThreadAnnotations]",
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This method must be called on the thread that originally created
                 * this UI element. This is typically the main thread of your app.
                 * <br>
                 * This method may take several seconds to complete, so it should
                 * only be called from a worker thread.
                 */
                @androidx.annotation.UiThread
                @androidx.annotation.WorkerThread
                public int test1() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Merge Multiple sections`() {
        check(
            expectedIssues = "src/android/widget/Toolbar2.java:14: error: Documentation should not specify @apiSince manually; it's computed and injected at build time by metalava [ForbiddenTag]",
            sourceFiles = arrayOf(
                java(
                    """
                    package android.widget;
                    import androidx.annotation.UiThread;

                    public class Toolbar2 {
                        /**
                        * Existing documentation for {@linkplain #getCurrentContentInsetEnd()} here.
                        * @return blah blah blah
                        */
                        @UiThread
                        public int getCurrentContentInsetEnd() {
                            return 0;
                        }

                        /**
                        * @apiSince 15
                        */
                        @UiThread
                        public int getCurrentContentInsetRight() {
                            return 0;
                        }
                    }
                    """
                ),
                uiThreadSource
            ),
            checkCompilation = true,
            docStubs = true,
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/widget/Toolbar2" since="21">
                            <method name="&lt;init>(Landroid/content/Context;)V"/>
                            <method name="collapseActionView()V"/>
                            <method name="getContentInsetStartWithNavigation()I" since="24"/>
                            <method name="getCurrentContentInsetEnd()I" since="24"/>
                            <method name="getCurrentContentInsetLeft()I" since="24"/>
                            <method name="getCurrentContentInsetRight()I" since="24"/>
                            <method name="getCurrentContentInsetStart()I" since="24"/>
                        </class>
                    </api>
                    """,
            stubs = arrayOf(
                """
                package android.widget;
                /** @apiSince 21 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Toolbar2 {
                public Toolbar2() { throw new RuntimeException("Stub!"); }
                /**
                 * Existing documentation for {@linkplain #getCurrentContentInsetEnd()} here.
                 * <br>
                 * This method must be called on the thread that originally created
                 * this UI element. This is typically the main thread of your app.
                 * @return blah blah blah
                 * @apiSince 24
                 */
                @androidx.annotation.UiThread
                public int getCurrentContentInsetEnd() { throw new RuntimeException("Stub!"); }
                /**
                 * <br>
                 * This method must be called on the thread that originally created
                 * this UI element. This is typically the main thread of your app.
                 * @apiSince 15
                 */
                @androidx.annotation.UiThread
                public int getCurrentContentInsetRight() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun Typedefs() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.annotation.IntDef;
                    import android.annotation.IntRange;

                    import java.lang.annotation.Retention;
                    import java.lang.annotation.RetentionPolicy;

                    @SuppressWarnings({"UnusedDeclaration", "WeakerAccess"})
                    public class TypedefTest {
                        @IntDef({STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT})
                        @Retention(RetentionPolicy.SOURCE)
                        private @interface DialogStyle {}

                        public static final int STYLE_NORMAL = 0;
                        public static final int STYLE_NO_TITLE = 1;
                        public static final int STYLE_NO_FRAME = 2;
                        public static final int STYLE_NO_INPUT = 3;
                        public static final int STYLE_UNRELATED = 3;

                        public void setStyle(@DialogStyle int style, int theme) {
                        }

                        @IntDef(value = {STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT, 2, 3 + 1},
                        flag=true)
                        @Retention(RetentionPolicy.SOURCE)
                        private @interface DialogFlags {}

                        public void setFlags(Object first, @DialogFlags int flags) {
                        }
                    }
                    """
                ),
                intRangeAnnotationSource,
                intDefAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class TypedefTest {
                public TypedefTest() { throw new RuntimeException("Stub!"); }
                /**
                 * @param style Value is {@link test.pkg.TypedefTest#STYLE_NORMAL}, {@link test.pkg.TypedefTest#STYLE_NO_TITLE}, {@link test.pkg.TypedefTest#STYLE_NO_FRAME}, or {@link test.pkg.TypedefTest#STYLE_NO_INPUT}
                 */
                public void setStyle(int style, int theme) { throw new RuntimeException("Stub!"); }
                /**
                 * @param flags Value is either <code>0</code> or a combination of {@link test.pkg.TypedefTest#STYLE_NORMAL}, {@link test.pkg.TypedefTest#STYLE_NO_TITLE}, {@link test.pkg.TypedefTest#STYLE_NO_FRAME}, {@link test.pkg.TypedefTest#STYLE_NO_INPUT}, 2, and 3 + 1
                 */
                public void setFlags(java.lang.Object first, int flags) { throw new RuntimeException("Stub!"); }
                public static final int STYLE_NORMAL = 0; // 0x0
                public static final int STYLE_NO_FRAME = 2; // 0x2
                public static final int STYLE_NO_INPUT = 3; // 0x3
                public static final int STYLE_NO_TITLE = 1; // 0x1
                public static final int STYLE_UNRELATED = 3; // 0x3
                }
                """
            )
        )
    }

    @Test
    fun `Typedefs combined with ranges`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;

                    import android.annotation.IntDef;
                    import android.annotation.IntRange;

                    import java.lang.annotation.Retention;
                    import java.lang.annotation.RetentionPolicy;

                    @SuppressWarnings({"UnusedDeclaration", "WeakerAccess"})
                    public class TypedefTest {
                        @IntDef({STYLE_NORMAL, STYLE_NO_TITLE, STYLE_NO_FRAME, STYLE_NO_INPUT})
                        @IntRange(from = 20)
                        @Retention(RetentionPolicy.SOURCE)
                        private @interface DialogStyle {}

                        public static final int STYLE_NORMAL = 0;
                        public static final int STYLE_NO_TITLE = 1;
                        public static final int STYLE_NO_FRAME = 2;

                        public void setStyle(@DialogStyle int style, int theme) {
                        }
                    }
                    """
                ),
                intRangeAnnotationSource,
                intDefAnnotationSource
            ),
            docStubs = true,
            checkCompilation = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class TypedefTest {
                public TypedefTest() { throw new RuntimeException("Stub!"); }
                /**
                 * @param style Value is {@link test.pkg.TypedefTest#STYLE_NORMAL}, {@link test.pkg.TypedefTest#STYLE_NO_TITLE}, {@link test.pkg.TypedefTest#STYLE_NO_FRAME}, or STYLE_NO_INPUT
                 * Value is 20 or greater
                 */
                public void setStyle(int style, int theme) { throw new RuntimeException("Stub!"); }
                public static final int STYLE_NORMAL = 0; // 0x0
                public static final int STYLE_NO_FRAME = 2; // 0x2
                public static final int STYLE_NO_TITLE = 1; // 0x1
                }
                """
            )
        )
    }

    @Test
    fun `Create method documentation from nothing`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresPermission;
                    @SuppressWarnings("WeakerAccess")
                    public class RangeTest {
                        public static final String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                        @RequiresPermission(ACCESS_COARSE_LOCATION)
                        public void test1() {
                        }
                    }
                    """
                ),
                requiresPermissionSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires {@link test.pkg.RangeTest#ACCESS_COARSE_LOCATION}
                 */
                @androidx.annotation.RequiresPermission(test.pkg.RangeTest.ACCESS_COARSE_LOCATION)
                public void test1() { throw new RuntimeException("Stub!"); }
                public static final java.lang.String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                }
                """
            )
        )
    }

    @Test
    fun `Warn about missing field`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresPermission;
                    public class RangeTest {
                        @RequiresPermission("MyPermission")
                        public void test1() {
                        }
                    }
                    """
                ),
                requiresPermissionSource
            ),
            checkCompilation = true,
            docStubs = true,
            expectedIssues = "src/test/pkg/RangeTest.java:4: lint: Cannot find permission field for \"MyPermission\" required by method test.pkg.RangeTest.test1() (may be hidden or removed) [MissingPermission]",
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * Requires "MyPermission"
                 */
                @androidx.annotation.RequiresPermission("MyPermission")
                public void test1() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Add to existing single-line method documentation`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresPermission;
                    @SuppressWarnings("WeakerAccess")
                    public class RangeTest {
                        public static final String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                        /** This is the existing documentation. */
                        @RequiresPermission(ACCESS_COARSE_LOCATION)
                        public int test1() { }
                    }
                    """
                ),
                requiresPermissionSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * <br>
                 * Requires {@link test.pkg.RangeTest#ACCESS_COARSE_LOCATION}
                 */
                @androidx.annotation.RequiresPermission(test.pkg.RangeTest.ACCESS_COARSE_LOCATION)
                public int test1() { throw new RuntimeException("Stub!"); }
                public static final java.lang.String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                }
                """
            )
        )
    }

    @Test
    fun `Add to existing multi-line method documentation`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresPermission;
                    @SuppressWarnings("WeakerAccess")
                    public class RangeTest {
                        public static final String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                        /**
                         * This is the existing documentation.
                         * Multiple lines of it.
                         */
                        @RequiresPermission(ACCESS_COARSE_LOCATION)
                        public int test1() { }
                    }
                    """
                ),
                requiresPermissionSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * Multiple lines of it.
                 * <br>
                 * Requires {@link test.pkg.RangeTest#ACCESS_COARSE_LOCATION}
                 */
                @androidx.annotation.RequiresPermission(test.pkg.RangeTest.ACCESS_COARSE_LOCATION)
                public int test1() { throw new RuntimeException("Stub!"); }
                public static final java.lang.String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                }
                """
            )
        )
    }

    @Test
    fun `Add new parameter when no doc exists`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    public class RangeTest {
                        public int test1(int parameter1, @IntRange(from = 10) int parameter2, int parameter3) { }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * @param parameter2 Value is 10 or greater
                 */
                public int test1(int parameter1, @androidx.annotation.IntRange(from=10) int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Add to method when there are existing parameter docs and appear before these`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresPermission;
                    @SuppressWarnings("WeakerAccess")
                    public class RangeTest {
                        public static final String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                        /**
                        * This is the existing documentation.
                        * @param parameter1 docs for parameter1
                        * @param parameter2 docs for parameter2
                        * @param parameter3 docs for parameter2
                        * @return return value documented here
                        */
                        @RequiresPermission(ACCESS_COARSE_LOCATION)
                        public int test1(int parameter1, int parameter2, int parameter3) { }
                    }
                        """
                ),
                requiresPermissionSource
            ),
            docStubs = true,
            checkCompilation = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * <br>
                 * Requires {@link test.pkg.RangeTest#ACCESS_COARSE_LOCATION}
                 * @param parameter1 docs for parameter1
                 * @param parameter2 docs for parameter2
                 * @param parameter3 docs for parameter2
                 * @return return value documented here
                 */
                @androidx.annotation.RequiresPermission(test.pkg.RangeTest.ACCESS_COARSE_LOCATION)
                public int test1(int parameter1, int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                public static final java.lang.String ACCESS_COARSE_LOCATION = "android.permission.ACCESS_COARSE_LOCATION";
                }
                """
            )
        )
    }

    @Test
    fun `Add new parameter when doc exists but no param doc`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    public class RangeTest {
                        /**
                        * This is the existing documentation.
                        * @return return value documented here
                        */
                        public int test1(int parameter1, @IntRange(from = 10) int parameter2, int parameter3) { }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * @param parameter2 Value is 10 or greater
                 * @return return value documented here
                 */
                public int test1(int parameter1, @androidx.annotation.IntRange(from=10) int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Add new parameter, sorted correctly between existing ones`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    public class RangeTest {
                        /**
                        * This is the existing documentation.
                        * @param parameter1 docs for parameter1
                        * @param parameter3 docs for parameter2
                        * @return return value documented here
                        */
                        public int test1(int parameter1, @IntRange(from = 10) int parameter2, int parameter3) { }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * @param parameter1 docs for parameter1
                 * @param parameter3 docs for parameter2
                 * @param parameter2 Value is 10 or greater
                 * @return return value documented here
                 */
                public int test1(int parameter1, @androidx.annotation.IntRange(from=10) int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Add to existing parameter`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    public class RangeTest {
                        /**
                        * This is the existing documentation.
                        * @param parameter1 docs for parameter1
                        * @param parameter2 docs for parameter2
                        * @param parameter3 docs for parameter2
                        * @return return value documented here
                        */
                        public int test1(int parameter1, @IntRange(from = 10) int parameter2, int parameter3) { }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * @param parameter1 docs for parameter1
                 * @param parameter2 docs for parameter2
                 * Value is 10 or greater
                 * @param parameter3 docs for parameter2
                 * @return return value documented here
                 */
                public int test1(int parameter1, @androidx.annotation.IntRange(from=10) int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Add new return value`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    public class RangeTest {
                        @IntRange(from = 10)
                        public int test1(int parameter1, int parameter2, int parameter3) { }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * @return Value is 10 or greater
                 */
                @androidx.annotation.IntRange(from=10)
                public int test1(int parameter1, int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Add to existing return value (ensuring it appears last)`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.IntRange;
                    public class RangeTest {
                        /**
                        * This is the existing documentation.
                        * @return return value documented here
                        */
                        @IntRange(from = 10)
                        public int test1(int parameter1, int parameter2, int parameter3) { }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class RangeTest {
                public RangeTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This is the existing documentation.
                 * @return return value documented here
                 * Value is 10 or greater
                 */
                @androidx.annotation.IntRange(from=10)
                public int test1(int parameter1, int parameter2, int parameter3) { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `test documentation trim utility`() {
        assertEquals(
            "/**\n * This is a comment\n * This is a second comment\n */",
            trimDocIndent(
                """/**
         * This is a comment
         * This is a second comment
         */
        """.trimIndent()
            )
        )
    }

    @Test
    fun `Merge API levels`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package android.widget;

                    public class Toolbar {
                        /**
                        * Existing documentation for {@linkplain #getCurrentContentInsetEnd()} here.
                        * @return blah blah blah
                        */
                        public int getCurrentContentInsetEnd() {
                            return 0;
                        }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/widget/Toolbar" since="21">
                            <method name="&lt;init>(Landroid/content/Context;)V"/>
                            <method name="collapseActionView()V"/>
                            <method name="getContentInsetStartWithNavigation()I" since="24"/>
                            <method name="getCurrentContentInsetEnd()I" since="24"/>
                            <method name="getCurrentContentInsetLeft()I" since="24"/>
                            <method name="getCurrentContentInsetRight()I" since="24"/>
                            <method name="getCurrentContentInsetStart()I" since="24"/>
                        </class>
                    </api>
                    """,
            stubs = arrayOf(
                """
                package android.widget;
                /** @apiSince 21 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Toolbar {
                public Toolbar() { throw new RuntimeException("Stub!"); }
                /**
                 * Existing documentation for {@linkplain #getCurrentContentInsetEnd()} here.
                 * @return blah blah blah
                 * @apiSince 24
                 */
                public int getCurrentContentInsetEnd() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Merge deprecation levels`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package android.hardware;
                    /**
                     * The Camera class is used to set image capture settings, start/stop preview.
                     *
                     * @deprecated We recommend using the new {@link android.hardware.camera2} API for new
                     *             applications.*
                    */
                    @Deprecated
                    public class Camera {
                       /** @deprecated Use something else. */
                       public static final String ACTION_NEW_VIDEO = "android.hardware.action.NEW_VIDEO";
                    }
                    """
                )
            ),
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/hardware/Camera" since="1" deprecated="21">
                            <method name="&lt;init>()V"/>
                            <method name="addCallbackBuffer([B)V" since="8"/>
                            <method name="getLogo()Landroid/graphics/drawable/Drawable;"/>
                            <field name="ACTION_NEW_VIDEO" since="14" deprecated="19"/>
                        </class>
                    </api>
                    """,
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package android.hardware;
                /**
                 * The Camera class is used to set image capture settings, start/stop preview.
                 *
                 * @deprecated We recommend using the new {@link android.hardware.camera2} API for new
                 *             applications.*
                 * @apiSince 1
                 * @deprecatedSince 21
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @Deprecated
                public class Camera {
                public Camera() { throw new RuntimeException("Stub!"); }
                /**
                 * @deprecated Use something else.
                 * @apiSince 14
                 * @deprecatedSince 19
                 */
                @Deprecated public static final java.lang.String ACTION_NEW_VIDEO = "android.hardware.action.NEW_VIDEO";
                }
                """
            )
        )
    }

    @Test
    fun `Api levels around current and preview`() {
        check(
            extraArguments = arrayOf(
                ARG_CURRENT_CODENAME,
                "Z",
                ARG_CURRENT_VERSION,
                "35" // not real api level of Z
            ),
            includeSystemApiAnnotations = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;
                    import android.annotation.SystemApi;
                    public class Test {
                       public static final String UNIT_TEST_1 = "unit.test.1";
                       /**
                         * @hide
                         */
                        @SystemApi
                       public static final String UNIT_TEST_2 = "unit.test.2";
                    }
                    """
                ),
                systemApiSource
            ),
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/pkg/Test" since="1">
                            <field name="UNIT_TEST_1" since="35"/>
                            <field name="UNIT_TEST_2" since="36"/>
                        </class>
                    </api>
                    """,
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package android.pkg;
                /** @apiSince 1 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                public Test() { throw new RuntimeException("Stub!"); }
                /** @apiSince 35 */
                public static final java.lang.String UNIT_TEST_1 = "unit.test.1";
                /**
                 * @hide
                 */
                public static final java.lang.String UNIT_TEST_2 = "unit.test.2";
                }
                """
            )
        )
    }

    @Test
    fun `No api levels on SystemApi only elements`() {
        // @SystemApi, @TestApi etc cannot get api versions since we don't have
        // accurate android.jar files (or even reliable api.txt/api.xml files) for them.
        check(
            extraArguments = arrayOf(
                ARG_CURRENT_CODENAME,
                "Z",
                ARG_CURRENT_VERSION,
                "35" // not real api level of Z
            ),
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg;
                    public class Test {
                       public Test(int i) { }
                       public static final String UNIT_TEST_1 = "unit.test.1";
                       public static final String UNIT_TEST_2 = "unit.test.2";
                    }
                    """
                )
            ),
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/pkg/Test" since="1">
                            <method name="&lt;init>(I)V"/>
                            <field name="UNIT_TEST_1" since="35"/>
                            <field name="UNIT_TEST_2" since="36"/>
                        </class>
                    </api>
                    """,
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package android.pkg;
                /** @apiSince 1 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                /** @apiSince 1 */
                public Test(int i) { throw new RuntimeException("Stub!"); }
                /** @apiSince 35 */
                public static final java.lang.String UNIT_TEST_1 = "unit.test.1";
                /** @apiSince Z */
                public static final java.lang.String UNIT_TEST_2 = "unit.test.2";
                }
                """
            )
        )
    }

    @Test
    fun `Generate API level javadocs`() {
        // TODO: Check package-info.java conflict
        // TODO: Test merging
        // TODO: Test non-merging
        check(
            extraArguments = arrayOf(
                ARG_CURRENT_CODENAME,
                "Z",
                ARG_CURRENT_VERSION,
                "35" // not real api level of Z
            ),
            sourceFiles = arrayOf(
                java(
                    """
                    package android.pkg1;
                    public class Test1 {
                    }
                    """
                ),
                java(
                    """
                    package android.pkg1;
                    public class Test2 {
                    }
                    """
                ),
                source(
                    "src/android/pkg2/package.html",
                    """
                    <body bgcolor="white">
                    Some existing doc here.
                    @deprecated
                    <!-- comment -->
                    </body>
                    """
                ).indented(),
                java(
                    """
                    package android.pkg2;
                    public class Test1 {
                    }
                    """
                ),
                java(
                    """
                    package android.pkg2;
                    public class Test2 {
                    }
                    """
                ),
                java(
                    """
                    package android.pkg3;
                    public class Test1 {
                    }
                    """
                )
            ),
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/pkg1/Test1" since="15"/>
                        <class name="android/pkg3/Test1" since="20"/>
                    </api>
                    """,
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package android.pkg1;
                /** @apiSince 15 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test1 {
                public Test1() { throw new RuntimeException("Stub!"); }
                }
                """,
                """
                [android/pkg1/package-info.java]
                /** @apiSince 15 */
                package android.pkg1;
                """,
                """
                [android/pkg2/package-info.java]
                /**
                 * Some existing doc here.
                 * @deprecated
                 * <!-- comment -->
                 */
                package android.pkg2;
                """,
                """
                [android/pkg3/package-info.java]
                /** @apiSince 20 */
                package android.pkg3;
                """
            ),
            docStubsSourceList = """
                TESTROOT/stubs/android/pkg1/package-info.java
                TESTROOT/stubs/android/pkg1/Test1.java
                TESTROOT/stubs/android/pkg1/Test2.java
                TESTROOT/stubs/android/pkg2/package-info.java
                TESTROOT/stubs/android/pkg2/Test1.java
                TESTROOT/stubs/android/pkg2/Test2.java
                TESTROOT/stubs/android/pkg3/package-info.java
                TESTROOT/stubs/android/pkg3/Test1.java
            """
        )
    }

    @Test
    fun `Generate overview html docs`() {
        // If a codebase provides overview.html files in the a public package,
        // make sure that we include this in the exported stubs folder as well!
        check(
            sourceFiles = arrayOf(
                source("src/overview.html", "<html>My overview docs</html>"),
                source(
                    "src/foo/test/visible/package.html",
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
                java(
                    // Note that we're *deliberately* placing the source file in the wrong
                    // source root here. This is to simulate the scenario where the source
                    // root (--source-path) points to a parent of the source folder instead
                    // of the source folder instead. In this case, we need to try a bit harder
                    // to compute the right package name; metalava has some code for that.
                    // This is a regression test for b/144264106.
                    "src/foo/test/visible/MyClass.java",
                    """
                    package test.visible;
                    public class MyClass {
                        public void test() { }
                    }
                    """
                ),
                // Also test hiding classes via javadoc
                source(
                    "src/foo/test/hidden1/package.html",
                    """
                    <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">
                    <html>
                    <body>
                    @hide
                    This is a hidden package
                    </body>
                    </html>
                    """
                ).indented(),
                java(
                    "src/foo/test/hidden1/Hidden.java",
                    """
                    package test.hidden1;
                    public class Hidden {
                        public void test() { }
                    }
                    """
                ),
                // Also test hiding classes via package-info.java
                java(
                    """
                    /**
                     * My package docs<br>
                     * @hide
                     */
                    package test.hidden2;
                    """
                ).indented(),
                java(
                    """
                    package test.hidden2;
                    public class Hidden {
                        public void test() { }
                    }
                    """
                )
            ),
            docStubs = true,
            // Make sure we expose exactly what we intend (so @hide via javadocs and
            // via package-info.java works)
            api = """
                package test.visible {
                  public class MyClass {
                    ctor public MyClass();
                    method public void test();
                  }
                }
            """,
            // Make sure the stubs are generated correctly; in particular, that we've
            // pulled docs from overview.html into javadoc on package-info.java instead
            // (removing all the content surrounding <body>, etc)
            stubs = arrayOf(
                """
                <html>My overview docs</html>
                """,
                """
                [test/visible/package-info.java]
                /**
                 * My package docs<br>
                 * <!-- comment -->
                 * Sample code: /** code here &#42;/
                 * Another line.<br>
                 */
                package test.visible;
                """,
                """
                [test/visible/MyClass.java]
                package test.visible;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class MyClass {
                public MyClass() { throw new RuntimeException("Stub!"); }
                public void test() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Check RequiresFeature handling`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresFeature;
                    import android.content.pm.PackageManager;
                    @SuppressWarnings("WeakerAccess")
                    @RequiresFeature(PackageManager.FEATURE_LOCATION)
                    public class LocationManager {
                    }
                    """
                ),
                java(
                    """
                    package android.content.pm;
                    public abstract class PackageManager {
                        public static final String FEATURE_LOCATION = "android.hardware.location";
                        public boolean hasSystemFeature(String feature) { return false; }
                    }
                    """
                ),

                requiresFeatureSource
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                import android.content.pm.PackageManager;
                /**
                 * Requires the {@link android.content.pm.PackageManager#FEATURE_LOCATION PackageManager#FEATURE_LOCATION} feature which can be detected using {@link android.content.pm.PackageManager#hasSystemFeature(String) PackageManager.hasSystemFeature(String)}.
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class LocationManager {
                public LocationManager() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Check RequiresApi handling`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import androidx.annotation.RequiresApi;
                    @RequiresApi(value = 21)
                    public class MyClass1 {
                    }
                    """
                ),

                requiresApiSource
            ),
            docStubs = true,
            checkCompilation = false, // duplicate class: androidx.annotation.RequiresApi
            stubs = arrayOf(
                """
                package test.pkg;
                /** @apiSince 21 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @androidx.annotation.RequiresApi(21)
                public class MyClass1 {
                public MyClass1() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Include Kotlin deprecation text`() {
        check(
            sourceFiles = arrayOf(
                kotlin(
                    """
                    package test.pkg

                    @Suppress("DeprecatedCallableAddReplaceWith","EqualsOrHashCode")
                    @Deprecated("Use Jetpack preference library", level = DeprecationLevel.ERROR)
                    class Foo {
                        fun foo()

                        @Deprecated("Blah blah blah 1", level = DeprecationLevel.ERROR)
                        override fun toString(): String = "Hello World"

                        /**
                         * My description
                         * @deprecated Existing deprecation message.
                         */
                        @Deprecated("Blah blah blah 2", level = DeprecationLevel.ERROR)
                        override fun hashCode(): Int = 0
                    }

                    """
                )
            ),
            checkCompilation = true,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                /**
                 * @deprecated Use Jetpack preference library
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @Deprecated
                public final class Foo {
                public Foo() { throw new RuntimeException("Stub!"); }
                public void foo() { throw new RuntimeException("Stub!"); }
                /**
                 * {@inheritDoc}
                 * @deprecated Blah blah blah 1
                 */
                @Deprecated
                @androidx.annotation.NonNull
                public java.lang.String toString() { throw new RuntimeException("Stub!"); }
                /**
                 * My description
                 * @deprecated Existing deprecation message.
                 * Blah blah blah 2
                 */
                @Deprecated
                public int hashCode() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Annotation annotating self`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;
                        import java.lang.annotation.Retention;
                        import java.lang.annotation.RetentionPolicy;
                        /**
                         * Documentation here
                         */
                        @SuppressWarnings("WeakerAccess")
                        @MyAnnotation
                        @Retention(RetentionPolicy.SOURCE)
                        public @interface MyAnnotation {
                        }
                    """
                ),
                java(
                    """
                        package test.pkg;

                        /**
                         * Other documentation here
                         */
                        @SuppressWarnings("WeakerAccess")
                        @MyAnnotation
                        public class OtherClass {
                        }
                    """
                )
            ),
            checkCompilation = true,
            stubs = arrayOf(
                """
                package test.pkg;
                /**
                 * Documentation here
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.SOURCE)
                public @interface MyAnnotation {
                }
                """,
                """
                package test.pkg;
                /**
                 * Other documentation here
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class OtherClass {
                public OtherClass() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }

    @Test
    fun `Rewrite external links for 129765390`() {
        // Tests rewriting links that go to {@docRoot}/../platform/ or {@docRoot}/../technotes,
        // which are hosted elsewhere. http://b/129765390
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package javax.security;
                    /**
                     * <a href="{@docRoot}/../technotes/guides/security/StandardNames.html#Cipher">Cipher Section</a>
                     * <p>This class is a member of the
                     * <a href="{@docRoot}/../technotes/guides/collections/index.html">
                     * Java Collections Framework</a>.
                     * <a href="../../../platform/serialization/spec/security.html">
                     *     Security Appendix</a>
                     * <a   href =
                     *  "../../../technotes/Example.html">valid</a>
                     *
                     *
                     * The following examples are not touched.
                     *
                     * <a href="../../../foobar/Example.html">wrong directory<a/>
                     * <a href="../ArrayList.html">wrong directory</a.
                     * <a href="http://example.com/index.html">wrong directory/host</a>
                     */
                    public class Example { }
                    """
                ),
                java(
                    """
                    package not.part.of.ojluni;
                    /**
                     * <p>This class is a member of the
                     * <a href="{@docRoot}/../technotes/guides/collections/index.html">
                     * Java Collections Framework</a>.
                     */
                    public class TestCollection { }
                    """
                )
            ),
            checkCompilation = true,
            expectedIssues = null, // be unopinionated about whether there should be warnings
            docStubs = true,
            stubs = arrayOf(
                    """
                    package javax.security;
                    /**
                     * <a href="https://docs.oracle.com/javase/8/docs/technotes/guides/security/StandardNames.html#Cipher">Cipher Section</a>
                     * <p>This class is a member of the
                     * <a href="https://docs.oracle.com/javase/8/docs/technotes/guides/collections/index.html">
                     * Java Collections Framework</a>.
                     * <a href="https://docs.oracle.com/javase/8/docs/platform/serialization/spec/security.html">
                     *     Security Appendix</a>
                     * <a   href =
                     *  "https://docs.oracle.com/javase/8/docs/technotes/Example.html">valid</a>
                     *
                     *
                     * The following examples are not touched.
                     *
                     * <a href="../../../foobar/Example.html">wrong directory<a/>
                     * <a href="../ArrayList.html">wrong directory</a.
                     * <a href="http://example.com/index.html">wrong directory/host</a>
                     */
                    @SuppressWarnings({"unchecked", "deprecation", "all"})
                    public class Example {
                    public Example() { throw new RuntimeException("Stub!"); }
                    }
                    """,
                    """
                    package not.part.of.ojluni;
                    /**
                     * <p>This class is a member of the
                     * <a href="{@docRoot}/../technotes/guides/collections/index.html">
                     * Java Collections Framework</a>.
                     */
                    @SuppressWarnings({"unchecked", "deprecation", "all"})
                    public class TestCollection {
                    public TestCollection() { throw new RuntimeException("Stub!"); }
                    }
                    """
            ),
            extraArguments = arrayOf(
                ARG_REPLACE_DOCUMENTATION,
                "com.sun:java:javax:jdk.net:sun",
                """(<a\s+href\s?=[\*\s]*")(?:(?:\{@docRoot\}/\.\./)|(?:(?:\.\./)+))((?:platform|technotes).+)">""",
                """$1https://docs.oracle.com/javase/8/docs/$2">"""
            )
        )
    }

    @Test
    fun `Annotation annotating itself indirectly`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        /**
                         * Documentation 1 here
                         */
                        @SuppressWarnings("WeakerAccess")
                        @MyAnnotation2
                        public @interface MyAnnotation1 {
                        }
                    """
                ),
                java(
                    """
                        package test.pkg;

                        /**
                         * Documentation 2 here
                         */
                        @SuppressWarnings("WeakerAccess")
                        @MyAnnotation1
                        public @interface MyAnnotation2 {
                        }
                    """
                )
            ),
            checkCompilation = true,
            stubs = arrayOf(
                """
                package test.pkg;
                /**
                 * Documentation 1 here
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @test.pkg.MyAnnotation2
                public @interface MyAnnotation1 {
                }
                """,
                """
                package test.pkg;
                /**
                 * Documentation 2 here
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @test.pkg.MyAnnotation1
                public @interface MyAnnotation2 {
                }
                """
            )
        )
    }

    @Test
    fun `Invoke external documentation tool`() {
        val jdkPath = getJdkPath()
        if (jdkPath == null) {
            println("Ignoring external doc test: JDK not found")
            return
        }

        val version = System.getProperty("java.version")
        if (!version.startsWith("1.8.")) {
            println("Javadoc invocation test does not work on Java 1.9 and later; bootclasspath not supported")
            return
        }

        val javadoc = File(jdkPath, "bin/javadoc")
        if (!javadoc.isFile) {
            println("Ignoring external doc test: javadoc not found *or* not running on Linux/OSX")
            return
        }
        val androidJar = getAndroidJar(API_LEVEL)?.path
        if (androidJar == null) {
            println("Ignoring external doc test: android.jar not found")
            return
        }

        val dir = Files.createTempDir()
        val html = "$dir/javadoc"
        val sourceList = "$dir/sources.txt"

        check(
            extraArguments = arrayOf(
                ARG_DOC_STUBS_SOURCE_LIST,
                sourceList,
                ARG_GENERATE_DOCUMENTATION,
                javadoc.path,
                "-sourcepath",
                "STUBS_DIR",
                "-d",
                html,
                "-bootclasspath",
                androidJar,
                "STUBS_SOURCE_LIST"
            ),
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.annotation.RequiresFeature;
                    import android.content.pm.PackageManager;
                    @SuppressWarnings("WeakerAccess")
                    @RequiresFeature(PackageManager.FEATURE_LOCATION)
                    public class LocationManager {
                    }
                    """
                ),
                java(
                    """
                    package android.content.pm;
                    public abstract class PackageManager {
                        public static final String FEATURE_LOCATION = "android.hardware.location";
                        public boolean hasSystemFeature(String name) {
                            return false;
                        }
                    }
                    """
                ),

                requiresFeatureSource
            ),
            checkCompilation = true,

            stubs = arrayOf(
                """
                package test.pkg;
                import android.content.pm.PackageManager;
                /**
                 * Requires the {@link android.content.pm.PackageManager#FEATURE_LOCATION PackageManager#FEATURE_LOCATION} feature which can be detected using {@link android.content.pm.PackageManager#hasSystemFeature(String) PackageManager.hasSystemFeature(String)}.
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class LocationManager {
                public LocationManager() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )

        val doc = File(html, "test/pkg/LocationManager.html").readText(UTF_8)
        assertTrue(
            "Did not find matching javadoc fragment in LocationManager.html: actual content is\n$doc",
            doc.contains(
                """
                <hr>
                <br>
                <pre>public class <span class="typeNameLabel">LocationManager</span>
                extends java.lang.Object</pre>
                <div class="block">Requires the <a href="../../android/content/pm/PackageManager.html#FEATURE_LOCATION"><code>PackageManager#FEATURE_LOCATION</code></a> feature which can be detected using <a href="../../android/content/pm/PackageManager.html#hasSystemFeature-java.lang.String-"><code>PackageManager.hasSystemFeature(String)</code></a>.</div>
                </li>
                </ul>
                """.trimIndent()
            )
        )

        dir.deleteRecursively()
    }

    @Test
    fun `Test Column annotation`() {
        // Bug: 120429729
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import android.provider.Column;
                    import android.database.Cursor;
                    @SuppressWarnings("WeakerAccess")
                    public class ColumnTest {
                        @Column(Cursor.FIELD_TYPE_STRING)
                        public static final String DATA = "_data";
                        @Column(value = Cursor.FIELD_TYPE_BLOB, readOnly = true)
                        public static final String HASH = "_hash";
                        @Column(value = Cursor.FIELD_TYPE_STRING, readOnly = true)
                        public static final String TITLE = "title";
                        @Column(value = Cursor.NONEXISTENT, readOnly = true)
                        public static final String BOGUS = "bogus";
                    }
                    """
                ),
                java(
                    """
                        package android.database;
                        public interface Cursor {
                            int FIELD_TYPE_NULL = 0;
                            int FIELD_TYPE_INTEGER = 1;
                            int FIELD_TYPE_FLOAT = 2;
                            int FIELD_TYPE_STRING = 3;
                            int FIELD_TYPE_BLOB = 4;
                        }
                    """
                ),
                columnSource
            ),
            checkCompilation = true,
            expectedIssues = """
                src/test/pkg/ColumnTest.java:12: warning: Cannot find feature field for Cursor.NONEXISTENT required by field ColumnTest.BOGUS (may be hidden or removed) [MissingColumn]
                """,
            docStubs = true,
            stubs = arrayOf(
                """
                package test.pkg;
                import android.database.Cursor;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class ColumnTest {
                public ColumnTest() { throw new RuntimeException("Stub!"); }
                /**
                 * This constant represents a column name that can be used with a {@link android.content.ContentProvider} through a {@link android.content.ContentValues} or {@link android.database.Cursor} object. The values stored in this column are {@link Cursor.NONEXISTENT}, and are read-only and cannot be mutated.
                 */
                public static final java.lang.String BOGUS = "bogus";
                /**
                 * This constant represents a column name that can be used with a {@link android.content.ContentProvider} through a {@link android.content.ContentValues} or {@link android.database.Cursor} object. The values stored in this column are {@link android.database.Cursor#FIELD_TYPE_STRING Cursor#FIELD_TYPE_STRING} .
                 */
                public static final java.lang.String DATA = "_data";
                /**
                 * This constant represents a column name that can be used with a {@link android.content.ContentProvider} through a {@link android.content.ContentValues} or {@link android.database.Cursor} object. The values stored in this column are {@link android.database.Cursor#FIELD_TYPE_BLOB Cursor#FIELD_TYPE_BLOB} , and are read-only and cannot be mutated.
                 */
                public static final java.lang.String HASH = "_hash";
                /**
                 * This constant represents a column name that can be used with a {@link android.content.ContentProvider} through a {@link android.content.ContentValues} or {@link android.database.Cursor} object. The values stored in this column are {@link android.database.Cursor#FIELD_TYPE_STRING Cursor#FIELD_TYPE_STRING} , and are read-only and cannot be mutated.
                 */
                public static final java.lang.String TITLE = "title";
                }
                """
            )
        )
    }

    @Test
    fun `Trailing comment close`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package android.widget;

                    public class Toolbar {
                        /**
                        * Existing documentation for {@linkplain #getCurrentContentInsetEnd()} here. */
                        public int getCurrentContentInsetEnd() {
                            return 0;
                        }
                    }
                    """
                ),
                intRangeAnnotationSource
            ),
            checkCompilation = true,
            docStubs = true,
            applyApiLevelsXml = """
                    <?xml version="1.0" encoding="utf-8"?>
                    <api version="2">
                        <class name="android/widget/Toolbar" since="21">
                            <method name="getCurrentContentInsetEnd()I" since="24"/>
                        </class>
                    </api>
                    """,
            stubs = arrayOf(
                """
                package android.widget;
                /** @apiSince 21 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Toolbar {
                public Toolbar() { throw new RuntimeException("Stub!"); }
                /**
                 * Existing documentation for {@linkplain #getCurrentContentInsetEnd()} here.
                 * @apiSince 24
                 */
                public int getCurrentContentInsetEnd() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }
}
