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

package com.android.tools.metalava.model.text

import com.android.tools.metalava.JAVA_LANG_DEPRECATED
import com.android.tools.metalava.model.AnnotationAttribute
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.AnnotationTarget
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.DefaultAnnotationAttribute
import com.android.tools.metalava.model.DefaultAnnotationItem
import com.android.tools.metalava.model.DefaultModifierList
import com.android.tools.metalava.model.ModifierList
import java.io.StringWriter

class TextModifiers(
    override val codebase: Codebase,
    flags: Int = PACKAGE_PRIVATE,
    annotations: MutableList<AnnotationItem>? = null
) : DefaultModifierList(codebase, flags, annotations) {

    fun duplicate(): TextModifiers {
        val annotations = this.annotations
        val newAnnotations =
            if (annotations == null || annotations.isEmpty()) {
                null
            } else {
                annotations.toMutableList()
            }
        return TextModifiers(codebase, flags, newAnnotations)
    }

    fun addAnnotations(annotationSources: List<String>?) {
        annotationSources ?: return
        if (annotationSources.isEmpty()) {
            return
        }

        val annotations = ArrayList<AnnotationItem>(annotationSources.size)
        annotationSources.forEach { source ->
            val index = source.indexOf('(')
            val originalName = if (index == -1) source.substring(1) else source.substring(1, index)
            val qualifiedName = AnnotationItem.mapName(codebase, originalName)

            // @Deprecated is also treated as a "modifier"
            if (qualifiedName == JAVA_LANG_DEPRECATED) {
                setDeprecated(true)
            }

            val attributes =
                if (index == -1) {
                    emptyList()
                } else {
                    DefaultAnnotationAttribute.createList(source.substring(index + 1, source.lastIndexOf(')')))
                }
            val codebase = codebase
            val item = object : DefaultAnnotationItem(codebase) {
                override fun attributes(): List<AnnotationAttribute> = attributes
                override fun originalName(): String? = originalName
                override fun qualifiedName(): String? = qualifiedName
                override fun toSource(target: AnnotationTarget, showDefaultAttrs: Boolean): String = source
            }
            annotations.add(item)
        }
        this.annotations = annotations
    }

    override fun toString(): String {
        val item = owner()
        val writer = StringWriter()
        ModifierList.write(writer, this, item, target = AnnotationTarget.SDK_STUBS_FILE)
        return writer.toString()
    }
}
