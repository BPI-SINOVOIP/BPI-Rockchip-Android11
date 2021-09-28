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

package com.android.tools.metalava.model

import com.android.tools.metalava.model.visitors.ItemVisitor
import com.android.tools.metalava.model.visitors.TypeVisitor

interface PropertyItem : MemberItem {
    /** The type of this property */
    override fun type(): TypeItem

    override fun accept(visitor: ItemVisitor) {
        if (visitor.skip(this)) {
            return
        }

        visitor.visitItem(this)
        visitor.visitProperty(this)

        visitor.afterVisitProperty(this)
        visitor.afterVisitItem(this)
    }

    override fun acceptTypes(visitor: TypeVisitor) {
        if (visitor.skip(this)) {
            return
        }

        val type = type()
        visitor.visitType(type, this)
        visitor.afterVisitType(type, this)
    }

    override fun hasNullnessInfo(): Boolean {
        if (!requiresNullnessInfo()) {
            return true
        }

        return modifiers.hasNullnessInfo()
    }

    override fun requiresNullnessInfo(): Boolean {
        if (type().primitive) {
            return false
        }

        return true
    }

    companion object {
        val comparator: java.util.Comparator<PropertyItem> = Comparator { a, b -> a.name().compareTo(b.name()) }
    }
}