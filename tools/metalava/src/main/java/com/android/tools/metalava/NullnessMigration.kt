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

import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.ParameterItem
import com.android.tools.metalava.model.SUPPORT_TYPE_USE_ANNOTATIONS
import com.android.tools.metalava.model.TypeItem

/**
 * Performs null migration analysis, looking at previous API signature
 * files and new signature files, and replacing new @Nullable and @NonNull
 * annotations with @RecentlyNullable and @RecentlyNonNull.
 *
 * TODO: Enforce compatibility across type use annotations, e.g.
 * changing parameter value from
 *    {@code @NonNull List<@Nullable String>}
 * to
 *    {@code @NonNull List<@NonNull String>}
 * is forbidden.
 */
class NullnessMigration : ComparisonVisitor(visitAddedItemsRecursively = true) {
    override fun compare(old: Item, new: Item) {
        if (hasNullnessInformation(new) && !hasNullnessInformation(old)) {
            new.markRecent()
        }
    }

    // Note: We don't override added(new: Item) to mark newly added methods as newly
    // having nullness annotations: those APIs are themselves new, so there's no reason
    // to mark the nullness contract as migration (warning- rather than error-severity)

    override fun compare(old: MethodItem, new: MethodItem) {
        @Suppress("ConstantConditionIf")
        if (SUPPORT_TYPE_USE_ANNOTATIONS) {
            val newType = new.returnType() ?: return
            val oldType = old.returnType() ?: return
            checkType(oldType, newType)
        }
    }

    override fun compare(old: FieldItem, new: FieldItem) {
        @Suppress("ConstantConditionIf")
        if (SUPPORT_TYPE_USE_ANNOTATIONS) {
            val newType = new.type()
            val oldType = old.type()
            checkType(oldType, newType)
        }
    }

    override fun compare(old: ParameterItem, new: ParameterItem) {
        @Suppress("ConstantConditionIf")
        if (SUPPORT_TYPE_USE_ANNOTATIONS) {
            val newType = new.type()
            val oldType = old.type()
            checkType(oldType, newType)
        }
    }

    private fun hasNullnessInformation(type: TypeItem): Boolean {
        return if (SUPPORT_TYPE_USE_ANNOTATIONS) {
            val typeString = type.toTypeString(outerAnnotations = false, innerAnnotations = true)
            typeString.contains(".Nullable") || typeString.contains(".NonNull")
        } else {
            false
        }
    }

    private fun checkType(old: TypeItem, new: TypeItem) {
        if (hasNullnessInformation(new)) {
            assert(SUPPORT_TYPE_USE_ANNOTATIONS)
            if (old.toTypeString(outerAnnotations = false, innerAnnotations = true) !=
                new.toTypeString(outerAnnotations = false, innerAnnotations = true)
            ) {
                new.markRecent()
            }
        }
    }

    companion object {
        fun hasNullnessInformation(item: Item): Boolean {
            return isNullable(item) || isNonNull(item)
        }

        fun findNullnessAnnotation(item: Item): AnnotationItem? {
            return item.modifiers.annotations().firstOrNull { it.isNullnessAnnotation() }
        }

        fun isNullable(item: Item): Boolean {
            return item.modifiers.annotations().any { it.isNullable() }
        }

        private fun isNonNull(item: Item): Boolean {
            return item.modifiers.annotations().any { it.isNonNull() }
        }
    }
}
