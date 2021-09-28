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

import com.android.tools.lint.checks.infrastructure.TestFiles.base64gzip
import com.android.tools.lint.checks.infrastructure.TestFiles.jar
import com.android.tools.lint.checks.infrastructure.TestFiles.xml
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.lang.reflect.Modifier
import java.net.URLClassLoader
import kotlin.text.Charsets.UTF_8

class RewriteAnnotationsTest : DriverTest() {
    @Test
    fun `Test copying private annotations from one of the stubs`() {
        val source = File("stub-annotations".replace('/', File.separatorChar))
        assertTrue(source.path, source.isDirectory)
        val target = temporaryFolder.newFolder()
        runDriver(
            ARG_NO_COLOR,
            ARG_NO_BANNER,

            ARG_COPY_ANNOTATIONS,
            source.path,
            target.path
        )

        // Source retention: Shouldn't exist
        val nonNull = File(target, "androidx/annotation/NonNull.java")
        assertFalse("${nonNull.path} exists", nonNull.isFile)

        // Class retention: Should be converted

        val recentlyNull = File(target, "androidx/annotation/RecentlyNullable.java")
        assertTrue("${recentlyNull.path} doesn't exist", recentlyNull.isFile)
        assertEquals(
            """
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
            package androidx.annotation;

            import static java.lang.annotation.ElementType.FIELD;
            import static java.lang.annotation.ElementType.METHOD;
            import static java.lang.annotation.ElementType.PARAMETER;
            import static java.lang.annotation.ElementType.TYPE_USE;
            import static java.lang.annotation.RetentionPolicy.CLASS;

            import java.lang.annotation.Retention;
            import java.lang.annotation.Target;

            /** Stub only annotation. Do not use directly. */
            @Retention(CLASS)
            @Target({METHOD, PARAMETER, FIELD})
            @interface RecentlyNullable {}
        """.trimIndent().trim(), recentlyNull.readText(UTF_8).trim().replace("\r\n", "\n")
        )
    }

    @Test
    fun `Test rewriting the bytecode for one of the public annotations`() {
        val bytecode = base64gzip(
            "androidx/annotation/CallSuper.class", "" +
                "H4sIAAAAAAAAAIWPsU4CQRRF70NhEQWxJMZoLCjdxs6KIMYCA2E3NlbD8kKG" +
                "DDNkmSXwaxZ+gB9FfGMBFps4yczc5J53kve9//wC8IirCK0IlxHahEbiijzj" +
                "F22Y0OorY5JixfnDQm0UoTMprNdLftdrPTXcs9Z55bWza8LdMDCxUXYeq0MR" +
                "T9izDemJUN0oU4i3+w86dkZnuzDQH/aShHBTPpCqfM5euPvyfmB4KcZ0t2KB" +
                "am+D9HX0LDZlZ7nTs+1f9rAqoX2UjaYLzjzhttR/3L9LIFTkniCcCk5/3ypq" +
                "8l9LiqSrM87QwHmIHyDGBZo/ObYRQoUBAAA="
        )

        val compiledStubs = temporaryFolder.newFolder("compiled-stubs")
        bytecode.createFile(compiledStubs)

        runDriver(
            ARG_NO_COLOR,
            ARG_NO_BANNER,

            ARG_REWRITE_ANNOTATIONS,
            compiledStubs.path
        )

        // Load the class to make sure it's legit
        val url = compiledStubs.toURI().toURL()
        val loader = URLClassLoader(arrayOf(url), null)
        val annotationClass = loader.loadClass("androidx.annotation.CallSuper")
        val modifiers = annotationClass.modifiers
        assertEquals(0, modifiers and Modifier.PUBLIC)
        assertTrue(annotationClass.isAnnotation)
    }

    @Test
    fun `Test rewriting the bytecode for one of the public annotations in a jar file`() {
        val bytecode = base64gzip(
            "androidx/annotation/CallSuper.class", "" +
                "H4sIAAAAAAAAAIWPsU4CQRRF70NhEQWxJMZoLCjdxs6KIMYCA2E3NlbD8kKG" +
                "DDNkmSXwaxZ+gB9FfGMBFps4yczc5J53kve9//wC8IirCK0IlxHahEbiijzj" +
                "F22Y0OorY5JixfnDQm0UoTMprNdLftdrPTXcs9Z55bWza8LdMDCxUXYeq0MR" +
                "T9izDemJUN0oU4i3+w86dkZnuzDQH/aShHBTPpCqfM5euPvyfmB4KcZ0t2KB" +
                "am+D9HX0LDZlZ7nTs+1f9rAqoX2UjaYLzjzhttR/3L9LIFTkniCcCk5/3ypq" +
                "8l9LiqSrM87QwHmIHyDGBZo/ObYRQoUBAAA="
        )

        val jarDesc = jar(
            "myjar.jar",
            bytecode,
            xml("foo/bar/baz.xml", "<hello-world/>")
        )

        val jarFile = jarDesc.createFile(temporaryFolder.root)

        runDriver(
            ARG_NO_COLOR,
            ARG_NO_BANNER,

            ARG_REWRITE_ANNOTATIONS,
            jarFile.path
        )

        // Load the class to make sure it's legit
        val url = jarFile.toURI().toURL()
        val loader = URLClassLoader(arrayOf(url), null)
        val annotationClass = loader.loadClass("androidx.annotation.CallSuper")
        val modifiers = annotationClass.modifiers
        assertEquals(0, modifiers and Modifier.PUBLIC)
        assertTrue(annotationClass.isAnnotation)
    }
}