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

package com.android.tools.metalava.model.psi

import com.android.tools.metalava.model.ConstructorItem
import com.android.tools.metalava.model.DefaultModifierList.Companion.PACKAGE_PRIVATE
import com.android.tools.metalava.model.MethodItem
import com.intellij.psi.JavaPsiFacade
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiExpressionStatement
import com.intellij.psi.PsiKeyword
import com.intellij.psi.PsiMethod
import com.intellij.psi.PsiMethodCallExpression
import com.intellij.psi.PsiWhiteSpace

class PsiConstructorItem(
    codebase: PsiBasedCodebase,
    psiMethod: PsiMethod,
    containingClass: PsiClassItem,
    name: String,
    modifiers: PsiModifierItem,
    documentation: String,
    parameters: List<PsiParameterItem>,
    returnType: PsiTypeItem,
    val implicitConstructor: Boolean = false
) :
    PsiMethodItem(
        codebase = codebase,
        modifiers = modifiers,
        documentation = documentation,
        psiMethod = psiMethod,
        containingClass = containingClass,
        name = name,
        returnType = returnType,
        parameters = parameters
    ), ConstructorItem {

    init {
        if (implicitConstructor) {
            setThrowsTypes(emptyList())
        }
    }

    override fun isImplicitConstructor(): Boolean = implicitConstructor
    override fun isConstructor(): Boolean = true
    override var superConstructor: ConstructorItem? = null
    override fun isCloned(): Boolean = false

    private var _superMethods: List<MethodItem>? = null
    override fun superMethods(): List<MethodItem> {
        if (_superMethods == null) {
            val result = mutableListOf<MethodItem>()
            psiMethod.findSuperMethods().mapTo(result) { codebase.findMethod(it) }

            if (result.isEmpty() && isConstructor() && containingClass().superClass() != null) {
                // Try a little harder; psi findSuperMethod doesn't seem to find super constructors in
                // some cases, but maybe we can find it by resolving actual super() calls!
                // TODO: Port to UAST
                var curr: PsiElement? = psiMethod.body?.firstBodyElement
                while (curr != null && curr is PsiWhiteSpace) {
                    curr = curr.nextSibling
                }
                if (curr is PsiExpressionStatement && curr.expression is PsiMethodCallExpression &&
                    curr.expression.firstChild?.lastChild is PsiKeyword &&
                    curr.expression.firstChild?.lastChild?.text == "super"
                ) {
                    val resolved = (curr.expression as PsiMethodCallExpression).resolveMethod()
                    if (resolved is PsiMethod) {
                        val superConstructor = codebase.findMethod(resolved)
                        result.add(superConstructor)
                    }
                }
            }
            _superMethods = result
        }

        return _superMethods!!
    }

    override fun psi(): PsiElement? {
        // If no PSI element, is this a synthetic/implicit constructor? If so
        // grab the parent class' PSI element instead for file/location purposes
        if (implicitConstructor && element.containingFile?.virtualFile == null) {
            return containingClass().psi()
        }

        return element
    }

    companion object {
        fun create(
            codebase: PsiBasedCodebase,
            containingClass: PsiClassItem,
            psiMethod: PsiMethod
        ): PsiConstructorItem {
            assert(psiMethod.isConstructor)
            val name = psiMethod.name
            val commentText = javadoc(psiMethod)
            val modifiers = modifiers(codebase, psiMethod, commentText)
            val parameters = psiMethod.parameterList.parameters.mapIndexed { index, parameter ->
                PsiParameterItem.create(codebase, parameter, index)
            }

            val constructor = PsiConstructorItem(
                codebase = codebase,
                psiMethod = psiMethod,
                containingClass = containingClass,
                name = name,
                documentation = commentText,
                modifiers = modifiers,
                parameters = parameters,
                returnType = codebase.getType(containingClass.psiClass),
                implicitConstructor = false
            )
            constructor.modifiers.setOwner(constructor)
            return constructor
        }

        fun createDefaultConstructor(
            codebase: PsiBasedCodebase,
            containingClass: PsiClassItem,
            psiClass: PsiClass
        ): PsiConstructorItem {
            val name = psiClass.name!!

            val factory = JavaPsiFacade.getInstance(psiClass.project).elementFactory
            val psiMethod = factory.createConstructor(name, psiClass)
            val modifiers = PsiModifierItem(codebase, PACKAGE_PRIVATE, null)
            modifiers.setVisibilityLevel(containingClass.modifiers.getVisibilityLevel())

            val item = PsiConstructorItem(
                codebase = codebase,
                psiMethod = psiMethod,
                containingClass = containingClass,
                name = name,
                documentation = "",
                modifiers = modifiers,
                parameters = emptyList(),
                returnType = codebase.getType(psiClass),
                implicitConstructor = true
            )
            modifiers.setOwner(item)
            return item
        }
    }
}