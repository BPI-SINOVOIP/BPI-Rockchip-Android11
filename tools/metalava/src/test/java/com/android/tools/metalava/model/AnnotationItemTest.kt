/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.tools.metalava.model

import org.junit.Assert
import org.junit.Test

class AnnotationItemTest {

    fun checkShortenAnnotation(expected: String, source: String) {
        Assert.assertEquals(expected, AnnotationItem.shortenAnnotation(source))
        Assert.assertEquals(source, AnnotationItem.unshortenAnnotation(expected))
    }

    @Test
    fun `Test shortenAnnotation and unshortenAnnotation`() {
        checkShortenAnnotation("@Nullable", "@androidx.annotation.Nullable")
        checkShortenAnnotation("@Deprecated", "@java.lang.Deprecated")
        checkShortenAnnotation("@SystemService", "@android.annotation.SystemService")
        checkShortenAnnotation("@TargetApi", "@android.annotation.TargetApi")
        checkShortenAnnotation("@SuppressLint", "@android.annotation.SuppressLint")
        checkShortenAnnotation("@Unknown", "@androidx.annotation.Unknown")
        checkShortenAnnotation("@Unknown.Nested", "@androidx.annotation.Unknown.Nested")
        checkShortenAnnotation("@my.Annotation", "@my.Annotation")
        checkShortenAnnotation("@m.Annotation", "@m.Annotation")
    }
}
