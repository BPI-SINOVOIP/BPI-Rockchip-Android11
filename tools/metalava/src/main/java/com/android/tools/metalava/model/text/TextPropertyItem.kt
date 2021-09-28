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

package com.android.tools.metalava.model.text

import com.android.tools.metalava.doclava1.SourcePositionInfo
import com.android.tools.metalava.doclava1.TextCodebase
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.PropertyItem
import com.android.tools.metalava.model.TypeItem

class TextPropertyItem(
    codebase: TextCodebase,
    name: String,
    containingClass: TextClassItem,
    modifiers: TextModifiers,
    private val type: TextTypeItem,
    position: SourcePositionInfo
) : TextMemberItem(codebase, name, containingClass, position, modifiers), PropertyItem {

    init {
        modifiers.setOwner(this)
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is FieldItem) return false

        if (name() != other.name()) {
            return false
        }

        return containingClass() == other.containingClass()
    }

    override fun hashCode(): Int = name().hashCode()

    override fun type(): TypeItem = type

    override fun toString(): String = "Field ${containingClass().fullName()}.${name()}"
}