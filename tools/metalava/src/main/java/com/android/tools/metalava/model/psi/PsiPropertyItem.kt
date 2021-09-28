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

import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.PropertyItem
import com.android.tools.metalava.model.TypeItem
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiMethod
import com.intellij.psi.PsiType
import org.jetbrains.uast.UClass

class PsiPropertyItem(
    override val codebase: PsiBasedCodebase,
    private val psiMethod: PsiMethod,
    private val containingClass: PsiClassItem,
    private val name: String,
    modifiers: PsiModifierItem,
    documentation: String,
    private val fieldType: PsiTypeItem
) :
    PsiItem(
        codebase = codebase,
        modifiers = modifiers,
        documentation = documentation,
        element = psiMethod
    ), PropertyItem {

    override fun type(): TypeItem = fieldType
    override fun name(): String = name
    override fun containingClass(): ClassItem = containingClass

    override fun isCloned(): Boolean {
        val psiClass = run {
            val p = containingClass().psi() as? PsiClass ?: return false
            if (p is UClass) {
                p.sourcePsi as? PsiClass ?: return false
            } else {
                p
            }
        }
        return psiMethod.containingClass != psiClass
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) {
            return true
        }
        return other is FieldItem && name == other.name() && containingClass == other.containingClass()
    }

    override fun hashCode(): Int {
        return name.hashCode()
    }

    override fun toString(): String = "field ${containingClass.fullName()}.${name()}"

    companion object {
        fun create(
            codebase: PsiBasedCodebase,
            containingClass: PsiClassItem,
            name: String,
            psiType: PsiType,
            psiMethod: PsiMethod
        ): PsiPropertyItem {
            val commentText = javadoc(psiMethod)
            val modifiers = modifiers(codebase, psiMethod, commentText)
            val typeItem = codebase.getType(psiType)
            val property = PsiPropertyItem(
                codebase = codebase,
                psiMethod = psiMethod,
                containingClass = containingClass,
                name = name,
                documentation = commentText,
                modifiers = modifiers,
                fieldType = typeItem
            )
            property.modifiers.setOwner(property)
            return property
        }
    }
}