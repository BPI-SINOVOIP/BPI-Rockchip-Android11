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

package com.android.tools.metalava.model.psi

import com.android.tools.lint.checks.infrastructure.TestFile
import com.android.tools.metalava.DriverTest
import org.intellij.lang.annotations.Language
import org.junit.Assert.assertEquals
import org.junit.Test

class JavadocTest : DriverTest() {
    private fun checkStubs(
        @Language("JAVA") source: String,
        compatibilityMode: Boolean = true,
        warnings: String? = "",
        api: String? = null,
        extraArguments: Array<String> = emptyArray(),
        docStubs: Boolean = false,
        showAnnotations: Array<String> = emptyArray(),
        includeSourceRetentionAnnotations: Boolean = true,
        skipEmitPackages: List<String> = listOf("java.lang", "java.util", "java.io"),
        sourceFiles: Array<TestFile>
    ) {
        check(
            sourceFiles = sourceFiles,
            showAnnotations = showAnnotations,
            stubs = arrayOf(source),
            compatibilityMode = compatibilityMode,
            expectedIssues = warnings,
            checkCompilation = true,
            api = api,
            extraArguments = extraArguments,
            docStubs = docStubs,
            includeSourceRetentionAnnotations = includeSourceRetentionAnnotations,
            skipEmitPackages = skipEmitPackages
        )
    }

    @Test
    fun `Test package to package info`() {
        @Language("HTML")
        val html = """
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

        @Suppress("DanglingJavadoc")
        @Language("JAVA")
        val java = """
            /**
             * My package docs<br>
             * <!-- comment -->
             * Sample code: /** code here &#42;/
             * Another line.<br>
             */
            """

        assertEquals(java.trimIndent() + "\n", packageHtmlToJavadoc(html.trimIndent()))
    }

    @Test
    fun `Relative documentation links in stubs`() {
        checkStubs(
            docStubs = false,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import java.io.IOException;
                    import test.pkg2.OtherClass;

                    /**
                     *  Blah blah {@link OtherClass} blah blah.
                     *  Referencing <b>field</b> {@link OtherClass#foo},
                     *  and referencing method {@link OtherClass#bar(int,
                     *   boolean)}.
                     *  And relative method reference {@link #baz()}.
                     *  And relative field reference {@link #importance}.
                     *  Here's an already fully qualified reference: {@link test.pkg2.OtherClass}.
                     *  And here's one in the same package: {@link LocalClass}.
                     *
                     *  @deprecated For some reason
                     *  @see OtherClass
                     *  @see OtherClass#bar(int, boolean)
                     */
                    @SuppressWarnings("all")
                    public class SomeClass {
                       /**
                       * My method.
                       * @param focus The focus to find. One of {@link OtherClass#FOCUS_INPUT} or
                       *         {@link OtherClass#FOCUS_ACCESSIBILITY}.
                       * @throws IOException when blah blah blah
                       * @throws {@link RuntimeException} when blah blah blah
                       */
                       public void baz(int focus) throws IOException;
                       public boolean importance;
                    }
                    """
                ),
                java(
                    """
                    package test.pkg2;

                    @SuppressWarnings("all")
                    public class OtherClass {
                        public static final int FOCUS_INPUT = 1;
                        public static final int FOCUS_ACCESSIBILITY = 2;
                        public int foo;
                        public void bar(int baz, boolean bar);
                    }
                    """
                ),
                java(
                    """
                    package test.pkg1;

                    @SuppressWarnings("all")
                    public class LocalClass {
                    }
                    """
                )
            ),
            warnings = "",
            source = """
                    package test.pkg1;
                    import test.pkg2.OtherClass;
                    import java.io.IOException;
                    /**
                     *  Blah blah {@link OtherClass} blah blah.
                     *  Referencing <b>field</b> {@link OtherClass#foo},
                     *  and referencing method {@link OtherClass#bar(int,
                     *   boolean)}.
                     *  And relative method reference {@link #baz()}.
                     *  And relative field reference {@link #importance}.
                     *  Here's an already fully qualified reference: {@link test.pkg2.OtherClass}.
                     *  And here's one in the same package: {@link LocalClass}.
                     *
                     *  @deprecated For some reason
                     *  @see OtherClass
                     *  @see OtherClass#bar(int, boolean)
                     */
                    @SuppressWarnings({"unchecked", "deprecation", "all"})
                    @Deprecated
                    public class SomeClass {
                    public SomeClass() { throw new RuntimeException("Stub!"); }
                    /**
                     * My method.
                     * @param focus The focus to find. One of {@link OtherClass#FOCUS_INPUT} or
                     *         {@link OtherClass#FOCUS_ACCESSIBILITY}.
                     * @throws IOException when blah blah blah
                     * @throws {@link RuntimeException} when blah blah blah
                     */
                    public void baz(int focus) throws java.io.IOException { throw new RuntimeException("Stub!"); }
                    public boolean importance;
                    }
                    """
        )
    }

    @Test
    fun `Rewrite relative documentation links in doc-stubs`() {
        checkStubs(
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import java.io.IOException;
                    import test.pkg2.OtherClass;

                    /**
                     *  Blah blah {@link OtherClass} blah blah.
                     *  Referencing <b>field</b> {@link OtherClass#foo},
                     *  and referencing method {@link OtherClass#bar(int,
                     *   boolean)}.
                     *  And relative method reference {@link #baz()}.
                     *  And relative field reference {@link #importance}.
                     *  Here's an already fully qualified reference: {@link test.pkg2.OtherClass}.
                     *  And here's one in the same package: {@link LocalClass}.
                     *
                     *  @deprecated For some reason
                     *  @see OtherClass
                     *  @see OtherClass#bar(int, boolean)
                     */
                    @SuppressWarnings("all")
                    public class SomeClass {
                       /**
                       * My method.
                       * @param focus The focus to find. One of {@link OtherClass#FOCUS_INPUT} or
                       *         {@link OtherClass#FOCUS_ACCESSIBILITY}.
                       * @throws IOException when blah blah blah
                       * @throws {@link RuntimeException} when blah blah blah
                       */
                       public void baz(int focus) throws IOException;
                       public boolean importance;
                    }
                    """
                ),
                java(
                    """
                    package test.pkg2;

                    @SuppressWarnings("all")
                    public class OtherClass {
                        public static final int FOCUS_INPUT = 1;
                        public static final int FOCUS_ACCESSIBILITY = 2;
                        public int foo;
                        public void bar(int baz, boolean bar);
                    }
                    """
                ),
                java(
                    """
                    package test.pkg1;

                    @SuppressWarnings("all")
                    public class LocalClass {
                    }
                    """
                )
            ),
            warnings = "",
            source = """
                package test.pkg1;
                import test.pkg2.OtherClass;
                import java.io.IOException;
                /**
                 *  Blah blah {@link test.pkg2.OtherClass OtherClass} blah blah.
                 *  Referencing <b>field</b> {@link test.pkg2.OtherClass#foo OtherClass#foo},
                 *  and referencing method {@link test.pkg2.OtherClass#bar(int,boolean) OtherClass#bar(int,
                 *   boolean)}.
                 *  And relative method reference {@link #baz()}.
                 *  And relative field reference {@link #importance}.
                 *  Here's an already fully qualified reference: {@link test.pkg2.OtherClass}.
                 *  And here's one in the same package: {@link test.pkg1.LocalClass LocalClass}.
                 *
                 *  @deprecated For some reason
                 *  @see test.pkg2.OtherClass
                 *  @see test.pkg2.OtherClass#bar(int, boolean)
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                @Deprecated
                public class SomeClass {
                public SomeClass() { throw new RuntimeException("Stub!"); }
                /**
                 * My method.
                 * @param focus The focus to find. One of {@link test.pkg2.OtherClass#FOCUS_INPUT OtherClass#FOCUS_INPUT} or
                 *         {@link test.pkg2.OtherClass#FOCUS_ACCESSIBILITY OtherClass#FOCUS_ACCESSIBILITY}.
                 * @throws java.io.IOException when blah blah blah
                 * @throws {@link java.lang.RuntimeException RuntimeException} when blah blah blah
                 */
                public void baz(int focus) throws java.io.IOException { throw new RuntimeException("Stub!"); }
                public boolean importance;
                }
                """
        )
    }

    @Test
    fun `Rewrite relative documentation links in doc-stubs 2`() {
        // Properly handle links to inherited methods
        checkStubs(
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import java.io.IOException;
                    import test.pkg2.OtherClass;

                    @SuppressWarnings("all")
                    public class R {
                        public static class attr {
                            /**
                             * Resource identifier to assign to this piece of named meta-data.
                             * The resource identifier can later be retrieved from the meta data
                             * Bundle through {@link android.os.Bundle#getInt Bundle.getInt}.
                             * <p>May be a reference to another resource, in the form
                             * "<code>@[+][<i>package</i>:]<i>type</i>/<i>name</i></code>" or a theme
                             * attribute in the form
                             * "<code>?[<i>package</i>:]<i>type</i>/<i>name</i></code>".
                             */
                            public static final int resource=0x01010025;
                        }
                    }
                    """
                ),
                java(
                    """
                    package android.os;

                    @SuppressWarnings("all")
                    public class Bundle extends BaseBundle {
                    }
                    """
                ),
                java(
                    """
                    package android.os;

                    @SuppressWarnings("all")
                    public class BaseBundle {
                        public int getInt(String key) {
                            return getInt(key, 0);
                        }

                        public int getInt(String key, int defaultValue) {
                            return defaultValue;
                        }
                    }
                    """
                )
            ),
            warnings = "",
            source = """
                package test.pkg1;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class R {
                public R() { throw new RuntimeException("Stub!"); }
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public static class attr {
                public attr() { throw new RuntimeException("Stub!"); }
                /**
                 * Resource identifier to assign to this piece of named meta-data.
                 * The resource identifier can later be retrieved from the meta data
                 * Bundle through {@link android.os.Bundle#getInt Bundle.getInt}.
                 * <p>May be a reference to another resource, in the form
                 * "<code>@[+][<i>package</i>:]<i>type</i>/<i>name</i></code>" or a theme
                 * attribute in the form
                 * "<code>?[<i>package</i>:]<i>type</i>/<i>name</i></code>".
                 */
                public static final int resource = 16842789; // 0x1010025
                }
                }
                """
        )
    }

    @Test
    fun `Rewrite relative documentation links in doc-stubs 3`() {
        checkStubs(
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.accessibilityservice;

                    import android.view.accessibility.AccessibilityEvent;
                    import android.view.accessibility.AccessibilityRecord;

                    /**
                     * <p>
                     * Window content may be retrieved with
                     * {@link AccessibilityEvent#getSource() AccessibilityEvent.getSource()}.
                     * Mention AccessibilityRecords here.
                     * </p>
                     */
                    @SuppressWarnings("all")
                    public abstract class AccessibilityService {
                    }
                    """
                ),
                java(
                    """
                    package android.view.accessibility;

                    @SuppressWarnings("all")
                    public final class AccessibilityEvent extends AccessibilityRecord {
                    }
                    """
                ),
                java(
                    """
                    package android.view.accessibility;

                    @SuppressWarnings("all")
                    public class AccessibilityRecord {
                        public AccessibilityNodeInfo getSource() {
                            return null;
                        }
                    }
                    """
                )
            ),
            warnings = "",
            source = """
                package android.accessibilityservice;
                import android.view.accessibility.AccessibilityEvent;
                /**
                 * <p>
                 * Window content may be retrieved with
                 * {@link android.view.accessibility.AccessibilityEvent#getSource() AccessibilityEvent#getSource()}.
                 * Mention AccessibilityRecords here.
                 * </p>
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public abstract class AccessibilityService {
                public AccessibilityService() { throw new RuntimeException("Stub!"); }
                }
                """
        )
    }

    @Test
    fun `Rewrite relative documentation links in doc-stubs 4`() {
        checkStubs(
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package android.content;

                    import android.os.OperationCanceledException;

                    @SuppressWarnings("all")
                    public abstract class AsyncTaskLoader {
                        /**
                         * Called if the task was canceled before it was completed.  Gives the class a chance
                         * to clean up post-cancellation and to properly dispose of the result.
                         *
                         * @param data The value that was returned by {@link #loadInBackground}, or null
                         * if the task threw {@link OperationCanceledException}.
                         */
                        public void onCanceled(D data) {
                        }

                        /**
                         * Called on a worker thread to perform the actual load and to return
                         * the result of the load operation.
                         *
                         * Implementations should not deliver the result directly, but should return them
                         * from this method, which will eventually end up calling {@link #deliverResult} on
                         * the UI thread.  If implementations need to process the results on the UI thread
                         * they may override {@link #deliverResult} and do so there.
                         *
                         * When the load is canceled, this method may either return normally or throw
                         * {@link OperationCanceledException}.  In either case, the Loader will
                         * call {@link #onCanceled} to perform post-cancellation cleanup and to dispose of the
                         * result object, if any.
                         *
                         * @return The result of the load operation.
                         *
                         * @throws OperationCanceledException if the load is canceled during execution.
                         *
                         * @see #onCanceled
                         */
                        public abstract Object loadInBackground();

                        /**
                         * Sends the result of the load to the registered listener. Should only be called by subclasses.
                         *
                         * Must be called from the process's main thread.
                         *
                         * @param data the result of the load
                         */
                        public void deliverResult(Object data) {
                        }
                    }
                    """
                ),
                java(
                    """
                    package android.os;


                    /**
                     * An exception type that is thrown when an operation in progress is canceled.
                     */
                    @SuppressWarnings("all")
                    public class OperationCanceledException extends RuntimeException {
                        public OperationCanceledException() {
                            this(null);
                        }

                        public OperationCanceledException(String message) {
                            super(message != null ? message : "The operation has been canceled.");
                        }
                    }
                    """
                )
            ),
            warnings = "",
            source = """
                package android.content;
                import android.os.OperationCanceledException;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public abstract class AsyncTaskLoader {
                public AsyncTaskLoader() { throw new RuntimeException("Stub!"); }
                /**
                 * Called if the task was canceled before it was completed.  Gives the class a chance
                 * to clean up post-cancellation and to properly dispose of the result.
                 *
                 * @param data The value that was returned by {@link #loadInBackground}, or null
                 * if the task threw {@link android.os.OperationCanceledException OperationCanceledException}.
                 */
                public void onCanceled(D data) { throw new RuntimeException("Stub!"); }
                /**
                 * Called on a worker thread to perform the actual load and to return
                 * the result of the load operation.
                 *
                 * Implementations should not deliver the result directly, but should return them
                 * from this method, which will eventually end up calling {@link #deliverResult} on
                 * the UI thread.  If implementations need to process the results on the UI thread
                 * they may override {@link #deliverResult} and do so there.
                 *
                 * When the load is canceled, this method may either return normally or throw
                 * {@link android.os.OperationCanceledException OperationCanceledException}.  In either case, the Loader will
                 * call {@link #onCanceled} to perform post-cancellation cleanup and to dispose of the
                 * result object, if any.
                 *
                 * @return The result of the load operation.
                 *
                 * @throws android.os.OperationCanceledException if the load is canceled during execution.
                 *
                 * @see #onCanceled
                 */
                public abstract java.lang.Object loadInBackground();
                /**
                 * Sends the result of the load to the registered listener. Should only be called by subclasses.
                 *
                 * Must be called from the process's main thread.
                 *
                 * @param data the result of the load
                 */
                public void deliverResult(java.lang.Object data) { throw new RuntimeException("Stub!"); }
                }
                """
        )
    }

    @Test
    fun `Rewrite relative documentation links in doc-stubs 5`() {
        // Properly handle links to inherited methods
        checkStubs(
            docStubs = true,
            sourceFiles = arrayOf(
                java(
                    """
                    package org.xmlpull.v1;

                    /**
                     * Example docs.
                     * <pre>
                     * import org.xmlpull.v1.<a href="XmlPullParserException.html">XmlPullParserException</a>;
                     *         xpp.<a href="#setInput">setInput</a>( new StringReader ( "&lt;foo>Hello World!&lt;/foo>" ) );
                     * </pre>
                     * see #setInput
                     */
                    @SuppressWarnings("all")
                    public interface XmlPullParser {
                        void setInput();
                    }
                    """
                )
            ),
            warnings = "",
            source = """
                package org.xmlpull.v1;
                /**
                 * Example docs.
                 * <pre>
                 * import org.xmlpull.v1.<a href="XmlPullParserException.html">XmlPullParserException</a>;
                 *         xpp.<a href="#setInput">setInput</a>( new StringReader ( "&lt;foo>Hello World!&lt;/foo>" ) );
                 * </pre>
                 * see #setInput
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public interface XmlPullParser {
                public void setInput();
                }
                """
        )
    }

    @Test
    fun `Check references to inherited field constants`() {
        checkStubs(
            docStubs = true,
            compatibilityMode = false,
            warnings = "",
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import test.pkg2.MyChild;

                    /**
                     * Reference to {@link MyChild#CONSTANT1},
                     * {@link MyChild#CONSTANT2}, and
                     * {@link MyChild#myMethod}.
                     * <p>
                     * Absolute reference:
                     * {@link test.pkg2.MyChild#CONSTANT1 MyChild.CONSTANT1}
                     * <p>
                     * Inner class reference:
                     * {@link Test.TestInner#CONSTANT3}, again
                     * {@link TestInner#CONSTANT3}
                     *
                     * @see test.pkg2.MyChild#myMethod
                     */
                    @SuppressWarnings("all")
                    public class Test {
                        public static class TestInner {
                            public static final String CONSTANT3 = "Hello";
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg1;
                    @SuppressWarnings("all")
                    interface MyConstants {
                        long CONSTANT1 = 12345;
                    }
                    """
                ),
                java(
                    """
                    package test.pkg1;
                    import java.io.Closeable;
                    @SuppressWarnings("all")
                    class MyParent implements MyConstants, Closeable {
                        public static final long CONSTANT2 = 67890;
                        public void myMethod() {
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg2;

                    import test.pkg1.MyParent;
                    @SuppressWarnings("all")
                    public class MyChild extends MyParent implements MyConstants {
                    }
                    """
                )
            ),
            source = """
                package test.pkg1;
                import test.pkg2.MyChild;
                /**
                 * Reference to {@link test.pkg2.MyChild#CONSTANT1 MyChild#CONSTANT1},
                 * {@link test.pkg2.MyChild#CONSTANT2 MyChild#CONSTANT2}, and
                 * {@link test.pkg2.MyChild#myMethod MyChild#myMethod}.
                 * <p>
                 * Absolute reference:
                 * {@link test.pkg2.MyChild#CONSTANT1 MyChild.CONSTANT1}
                 * <p>
                 * Inner class reference:
                 * {@link test.pkg1.Test.TestInner#CONSTANT3 Test.TestInner#CONSTANT3}, again
                 * {@link test.pkg1.Test.TestInner#CONSTANT3 TestInner#CONSTANT3}
                 *
                 * @see test.pkg2.MyChild#myMethod
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                public Test() { throw new RuntimeException("Stub!"); }
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public static class TestInner {
                public TestInner() { throw new RuntimeException("Stub!"); }
                public static final java.lang.String CONSTANT3 = "Hello";
                }
                }
                """
        )
    }

    @Test
    fun `Handle @attr references`() {
        checkStubs(
            docStubs = true,
            compatibilityMode = false,
            warnings = "",
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;

                    @SuppressWarnings("all")
                    public class Test {
                        /**
                         * Returns the drawable that will be drawn between each item in the list.
                         *
                         * @return the current drawable drawn between list elements
                         * This value may be {@code null}.
                         * @attr ref R.styleable#ListView_divider
                         */
                        public Object getFoo() {
                            return null;
                        }
                    }
                    """
                )
            ),
            source = """
                package test.pkg1;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                public Test() { throw new RuntimeException("Stub!"); }
                /**
                 * Returns the drawable that will be drawn between each item in the list.
                 *
                 * @return the current drawable drawn between list elements
                 * This value may be {@code null}.
                 * @attr ref android.R.styleable#ListView_divider
                 */
                public java.lang.Object getFoo() { throw new RuntimeException("Stub!"); }
                }
                """
        )
    }

    @Test
    fun `Rewrite parameter list`() {
        checkStubs(
            docStubs = true,
            compatibilityMode = false,
            warnings = "",
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import test.pkg2.OtherClass1;
                    import test.pkg2.OtherClass2;

                    /**
                     * Reference to {@link OtherClass1#myMethod(OtherClass2, int name, OtherClass2[])},
                     */
                    @SuppressWarnings("all")
                    public class Test<E extends OtherClass2> {
                        /**
                         * Reference to {@link OtherClass1#myMethod(E, int, OtherClass2 [])},
                         */
                        public void test() { }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg2;

                    @SuppressWarnings("all")
                    class OtherClass1 {
                        public void myMethod(OtherClass2 parameter1, int parameter2, OtherClass2[] parameter3) {
                        }
                    }
                    """
                ),
                java(
                    """
                    package test.pkg2;

                    @SuppressWarnings("all")
                    public class OtherClass2 {
                    }
                    """
                )
            ),
            source = """
                package test.pkg1;
                import test.pkg2.OtherClass2;
                /**
                 * Reference to {@link test.pkg2.OtherClass1#myMethod(test.pkg2.OtherClass2,int name,test.pkg2.OtherClass2[]) OtherClass1#myMethod(OtherClass2, int name, OtherClass2[])},
                 */
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test<E extends test.pkg2.OtherClass2> {
                public Test() { throw new RuntimeException("Stub!"); }
                /**
                 * Reference to {@link test.pkg2.OtherClass1#myMethod(E,int,test.pkg2.OtherClass2[]) OtherClass1#myMethod(E, int, OtherClass2 [])},
                 */
                public void test() { throw new RuntimeException("Stub!"); }
                }
                """
        )
    }

    @Test
    fun `Rewrite parameter list 2`() {
        checkStubs(
            docStubs = true,
            compatibilityMode = false,
            warnings = "",
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import java.nio.ByteBuffer;

                    @SuppressWarnings("all")
                    public class Test {
                        /**
                         * Blah blah
                         * <blockquote><pre>
                         * {@link #wrap(ByteBuffer [], int, int, ByteBuffer)
                         *     engine.wrap(new ByteBuffer [] { src }, 0, 1, dst);}
                         * </pre></blockquote>
                         */
                        public void test() { }

                        public abstract void wrap(ByteBuffer [] srcs, int offset,
                            int length, ByteBuffer dst);
                    }
                    """
                )
            ),
            source = """
                package test.pkg1;
                import java.nio.ByteBuffer;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                public Test() { throw new RuntimeException("Stub!"); }
                /**
                 * Blah blah
                 * <blockquote><pre>
                 * {@link #wrap(java.nio.ByteBuffer[],int,int,java.nio.ByteBuffer)
                 *     engine.wrap(new ByteBuffer [] { src }, 0, 1, dst);}
                 * </pre></blockquote>
                 */
                public void test() { throw new RuntimeException("Stub!"); }
                public abstract void wrap(java.nio.ByteBuffer[] srcs, int offset, int length, java.nio.ByteBuffer dst);
                }
                """
        )
    }

    @Test
    fun `Warn about unresolved`() {
        @Suppress("ConstantConditionIf")
        checkStubs(
            docStubs = true,
            compatibilityMode = false,
            warnings =
            if (REPORT_UNRESOLVED_SYMBOLS) {
                """
                src/test/pkg1/Test.java:6: lint: Unresolved documentation reference: SomethingMissing [UnresolvedLink]
                src/test/pkg1/Test.java:6: lint: Unresolved documentation reference: OtherMissing [UnresolvedLink]
            """
            } else {
                ""
            },
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg1;
                    import java.nio.ByteBuffer;

                    @SuppressWarnings("all")
                    public class Test {
                        /**
                         * Reference to {@link SomethingMissing} and
                         * {@link String#randomMethod}.
                         *
                         * @see OtherMissing
                         */
                        public void test() { }
                    }
                    """
                )
            ),
            source = """
                package test.pkg1;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Test {
                public Test() { throw new RuntimeException("Stub!"); }
                /**
                 * Reference to {@link SomethingMissing} and
                 * {@link java.lang.String#randomMethod String#randomMethod}.
                 *
                 * @see OtherMissing
                 */
                public void test() { throw new RuntimeException("Stub!"); }
                }
                """
        )
    }

    @Test
    fun `Javadoc link to innerclass constructor`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                    package android.view;
                    import android.graphics.Insets;


                    public final class WindowInsets {
                        /**
                         * Returns a copy of this WindowInsets with selected system window insets replaced
                         * with new values.
                         *
                         * @param left New left inset in pixels
                         * @param top New top inset in pixels
                         * @param right New right inset in pixels
                         * @param bottom New bottom inset in pixels
                         * @return A modified copy of this WindowInsets
                         * @deprecated use {@link Builder#Builder(WindowInsets)} with
                         *             {@link Builder#setSystemWindowInsets(Insets)} instead.
                         */
                        @Deprecated
                        public WindowInsets replaceSystemWindowInsets(int left, int top, int right, int bottom) {

                        }

                        public static class Builder {
                            public Builder() {
                            }

                            public Builder(WindowInsets insets) {
                            }

                            public Builder setSystemWindowInsets(Insets systemWindowInsets) {
                                return this;
                            }
                        }
                    }
                    """
                )
            ),
            docStubs = true,
            stubs = arrayOf(
                """
                package android.view;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public final class WindowInsets {
                public WindowInsets() { throw new RuntimeException("Stub!"); }
                /**
                 * Returns a copy of this WindowInsets with selected system window insets replaced
                 * with new values.
                 *
                 * @param left New left inset in pixels
                 * @param top New top inset in pixels
                 * @param right New right inset in pixels
                 * @param bottom New bottom inset in pixels
                 * @return A modified copy of this WindowInsets
                 * @deprecated use {@link android.view.WindowInsets.Builder#Builder(android.view.WindowInsets) Builder#Builder(WindowInsets)} with
                 *             {@link android.view.WindowInsets.Builder#setSystemWindowInsets(Insets) Builder#setSystemWindowInsets(Insets)} instead.
                 */
                @Deprecated
                public android.view.WindowInsets replaceSystemWindowInsets(int left, int top, int right, int bottom) { throw new RuntimeException("Stub!"); }
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public static class Builder {
                public Builder() { throw new RuntimeException("Stub!"); }
                public Builder(android.view.WindowInsets insets) { throw new RuntimeException("Stub!"); }
                public android.view.WindowInsets.Builder setSystemWindowInsets(Insets systemWindowInsets) { throw new RuntimeException("Stub!"); }
                }
                }
                """
            )
        )
    }

    @Test
    fun `Ensure references to classes in JavaDoc of hidden members do not affect imports`() {
        check(
            compatibilityMode = false,
            sourceFiles = arrayOf(
                java(
                    """
                    package test.pkg;
                    import test.pkg.bar.Bar;
                    import test.pkg.baz.Baz;
                    public class Foo {
                        /**
                         * This method is hidden so the reference to {@link Baz} in this comment
                         * should not cause test.pkg.baz.Baz import to be added even though Baz is
                         * part of the API.
                         * @hide
                         */
                        public void baz() {}

                        /**
                         * @see Bar
                         */
                        public void bar() {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg.bar;
                    import test.pkg.Foo;
                    import test.pkg.baz.Baz;
                    public class Bar {
                        /** @see Baz */
                        public void baz(Baz baz) {}
                        /** @see Foo */
                        public void foo(Foo foo) {}
                    }
                    """
                ),
                java(
                    """
                    package test.pkg.baz;
                    public class Baz {
                    }
                    """
                )
            ),
            stubs = arrayOf(
                """
                package test.pkg;
                import test.pkg.bar.Bar;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Foo {
                public Foo() { throw new RuntimeException("Stub!"); }
                /**
                 * @see Bar
                 */
                public void bar() { throw new RuntimeException("Stub!"); }
                }
                """,
                """
                package test.pkg.bar;
                import test.pkg.baz.Baz;
                import test.pkg.Foo;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Bar {
                public Bar() { throw new RuntimeException("Stub!"); }
                /** @see Baz */
                public void baz(test.pkg.baz.Baz baz) { throw new RuntimeException("Stub!"); }
                /** @see Foo */
                public void foo(test.pkg.Foo foo) { throw new RuntimeException("Stub!"); }
                }
                """,
                """
                package test.pkg.baz;
                @SuppressWarnings({"unchecked", "deprecation", "all"})
                public class Baz {
                public Baz() { throw new RuntimeException("Stub!"); }
                }
                """
            )
        )
    }
}