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

import com.android.tools.metalava.ANDROIDX_VISIBLE_FOR_TESTING
import com.android.tools.metalava.ANDROID_SUPPORT_VISIBLE_FOR_TESTING
import com.android.tools.metalava.ATTR_OTHERWISE
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.DefaultModifierList
import com.android.tools.metalava.model.ModifierList
import com.android.tools.metalava.model.MutableModifierList
import com.intellij.psi.PsiDocCommentOwner
import com.intellij.psi.PsiMethod
import com.intellij.psi.PsiModifier
import com.intellij.psi.PsiModifierList
import com.intellij.psi.PsiModifierListOwner
import com.intellij.psi.PsiReferenceExpression
import com.intellij.psi.PsiPrimitiveType
import org.jetbrains.kotlin.asJava.elements.KtLightModifierList
import org.jetbrains.kotlin.asJava.elements.KtLightNullabilityAnnotation
import org.jetbrains.kotlin.lexer.KtTokens
import org.jetbrains.kotlin.psi.KtNamedFunction
import org.jetbrains.kotlin.psi.KtProperty
import org.jetbrains.uast.UAnnotated
import org.jetbrains.uast.UMethod
import org.jetbrains.uast.UVariable
import org.jetbrains.uast.kotlin.KotlinNullabilityUAnnotation

class PsiModifierItem(
    codebase: Codebase,
    flags: Int = PACKAGE_PRIVATE,
    annotations: MutableList<AnnotationItem>? = null
) : DefaultModifierList(codebase, flags, annotations), ModifierList, MutableModifierList {
    companion object {
        fun create(codebase: PsiBasedCodebase, element: PsiModifierListOwner, documentation: String?): PsiModifierItem {
            val modifiers =
                if (element is UAnnotated) {
                    create(codebase, element, element)
                } else {
                    create(codebase, element)
                }
            if (documentation?.contains("@deprecated") == true ||
                // Check for @Deprecated annotation
                ((element as? PsiDocCommentOwner)?.isDeprecated == true)
            ) {
                modifiers.setDeprecated(true)
            }

            return modifiers
        }

        private fun computeFlag(element: PsiModifierListOwner, modifierList: PsiModifierList): Int {
            var visibilityFlags = if (modifierList.hasModifierProperty(PsiModifier.PUBLIC)) {
                PUBLIC
            } else if (modifierList.hasModifierProperty(PsiModifier.PROTECTED)) {
                PROTECTED
            } else if (modifierList.hasModifierProperty(PsiModifier.PRIVATE)) {
                PRIVATE
            } else {
                PACKAGE_PRIVATE
            }
            var flags = 0
            if (modifierList.hasModifierProperty(PsiModifier.STATIC)) {
                flags = flags or STATIC
            }
            if (modifierList.hasModifierProperty(PsiModifier.ABSTRACT)) {
                flags = flags or ABSTRACT
            }
            if (modifierList.hasModifierProperty(PsiModifier.FINAL)) {
                flags = flags or FINAL
            }
            if (modifierList.hasModifierProperty(PsiModifier.NATIVE)) {
                flags = flags or NATIVE
            }
            if (modifierList.hasModifierProperty(PsiModifier.SYNCHRONIZED)) {
                flags = flags or SYNCHRONIZED
            }
            if (modifierList.hasModifierProperty(PsiModifier.STRICTFP)) {
                flags = flags or STRICT_FP
            }
            if (modifierList.hasModifierProperty(PsiModifier.TRANSIENT)) {
                flags = flags or TRANSIENT
            }
            if (modifierList.hasModifierProperty(PsiModifier.VOLATILE)) {
                flags = flags or VOLATILE
            }
            if (modifierList.hasModifierProperty(PsiModifier.DEFAULT)) {
                flags = flags or DEFAULT
            }

            // Look for special Kotlin keywords
            if (modifierList is KtLightModifierList<*>) {
                val ktModifierList = modifierList.kotlinOrigin
                if (ktModifierList != null) {
                    if (ktModifierList.hasModifier(KtTokens.VARARG_KEYWORD)) {
                        flags = flags or VARARG
                    }
                    if (ktModifierList.hasModifier(KtTokens.SEALED_KEYWORD)) {
                        flags = flags or SEALED
                    }
                    if (ktModifierList.hasModifier(KtTokens.INTERNAL_KEYWORD)) {
                        // Also remove public flag which at the UAST levels it promotes these
                        // methods to, e.g. "internal myVar" gets turned into
                        //    public final boolean getMyHiddenVar$lintWithKotlin()
                        visibilityFlags = INTERNAL
                    }
                    if (ktModifierList.hasModifier(KtTokens.INFIX_KEYWORD)) {
                        flags = flags or INFIX
                    }
                    if (ktModifierList.hasModifier(KtTokens.CONST_KEYWORD)) {
                        flags = flags or CONST
                    }
                    if (ktModifierList.hasModifier(KtTokens.OPERATOR_KEYWORD)) {
                        flags = flags or OPERATOR
                    }
                    if (ktModifierList.hasModifier(KtTokens.INLINE_KEYWORD)) {
                        flags = flags or INLINE

                        // Workaround for b/117565118:
                        if (element is PsiMethod) {
                            val t =
                                ((element as? UMethod)?.sourcePsi as? KtNamedFunction)?.typeParameterList?.text ?: ""
                            if (t.contains("reified") &&
                                !ktModifierList.hasModifier(KtTokens.PRIVATE_KEYWORD) &&
                                !ktModifierList.hasModifier(KtTokens.INTERNAL_KEYWORD)
                            ) {
                                // Switch back from private to public
                                visibilityFlags = PUBLIC
                            }
                        }
                    }
                    if (ktModifierList.hasModifier(KtTokens.SUSPEND_KEYWORD)) {
                        flags = flags or SUSPEND

                        // Workaround for b/117565118:
                        if (!ktModifierList.hasModifier(KtTokens.INTERNAL_KEYWORD)) {
                            // Switch back from private to public
                            visibilityFlags = PUBLIC
                        }
                    }
                    if (ktModifierList.hasModifier(KtTokens.COMPANION_KEYWORD)) {
                        flags = flags or COMPANION
                    }
                } else {
                    // UAST returns a null modifierList.kotlinOrigin for get/set methods for
                    // properties
                    if (element is UMethod && element.sourceElement is KtProperty) {
                        // If the name contains the marker of an internal method, mark it internal
                        if (element.name.endsWith("\$lintWithKotlin")) {
                            visibilityFlags = INTERNAL
                        }
                    }
                }
            }

            // Merge in the visibility flags.
            flags = flags or visibilityFlags

            return flags
        }

        private fun create(codebase: PsiBasedCodebase, element: PsiModifierListOwner): PsiModifierItem {
            val modifierList = element.modifierList ?: return PsiModifierItem(codebase)

            var flags = computeFlag(element, modifierList)

            val psiAnnotations = modifierList.annotations
            return if (psiAnnotations.isEmpty()) {
                PsiModifierItem(codebase, flags)
            } else {
                val annotations: MutableList<AnnotationItem> =
                    psiAnnotations.map {
                        val qualifiedName = it.qualifiedName
                        // Consider also supporting com.android.internal.annotations.VisibleForTesting?
                        if (qualifiedName == ANDROIDX_VISIBLE_FOR_TESTING ||
                            qualifiedName == ANDROID_SUPPORT_VISIBLE_FOR_TESTING) {
                            val otherwise = it.findAttributeValue(ATTR_OTHERWISE)
                            val ref = when {
                                otherwise is PsiReferenceExpression -> otherwise.referenceName ?: ""
                                otherwise != null -> otherwise.text
                                else -> ""
                            }
                            flags = getVisibilityFlag(ref, flags)
                        }

                        PsiAnnotationItem.create(codebase, it, qualifiedName)
                    }.toMutableList()
                PsiModifierItem(codebase, flags, annotations)
            }
        }

        private fun create(
            codebase: PsiBasedCodebase,
            element: PsiModifierListOwner,
            annotated: UAnnotated
        ): PsiModifierItem {
            val modifierList = element.modifierList ?: return PsiModifierItem(codebase)
            var flags = computeFlag(element, modifierList)
            val uAnnotations = annotated.annotations

            return if (uAnnotations.isEmpty()) {
                val psiAnnotations = modifierList.annotations
                if (!psiAnnotations.isEmpty()) {
                    val annotations: MutableList<AnnotationItem> =
                        psiAnnotations.map { PsiAnnotationItem.create(codebase, it) }.toMutableList()
                    PsiModifierItem(codebase, flags, annotations)
                } else {
                    PsiModifierItem(codebase, flags)
                }
            } else {
                val isPrimitiveVariable = element is UVariable && element.type is PsiPrimitiveType

                val annotations: MutableList<AnnotationItem> = uAnnotations
                    // Uast sometimes puts nullability annotations on primitives!?
                    .filter { !isPrimitiveVariable || it !is KotlinNullabilityUAnnotation }
                    .map {

                        val qualifiedName = it.qualifiedName
                        if (qualifiedName == ANDROIDX_VISIBLE_FOR_TESTING ||
                            qualifiedName == ANDROID_SUPPORT_VISIBLE_FOR_TESTING) {
                            val otherwise = it.findAttributeValue(ATTR_OTHERWISE)
                            val ref = when {
                                otherwise is PsiReferenceExpression -> otherwise.referenceName ?: ""
                                otherwise != null -> otherwise.asSourceString()
                                else -> ""
                            }
                            flags = getVisibilityFlag(ref, flags)
                        }

                        UAnnotationItem.create(codebase, it, qualifiedName)
                    }.toMutableList()

                if (!isPrimitiveVariable) {
                    val psiAnnotations = modifierList.annotations
                    if (psiAnnotations.isNotEmpty() && annotations.none { it.isNullnessAnnotation() }) {
                        val ktNullAnnotation = psiAnnotations.firstOrNull { it is KtLightNullabilityAnnotation<*> }
                        ktNullAnnotation?.let {
                            annotations.add(PsiAnnotationItem.create(codebase, it))
                        }
                    }
                }

                PsiModifierItem(codebase, flags, annotations)
            }
        }

        /** Modifies the modifier flags based on the VisibleForTesting otherwise constants */
        private fun getVisibilityFlag(ref: String, flags: Int): Int {
            val visibilityFlags = if (ref.endsWith("PROTECTED")) {
                PROTECTED
            } else if (ref.endsWith("PACKAGE_PRIVATE")) {
                PACKAGE_PRIVATE
            } else if (ref.endsWith("PRIVATE") || ref.endsWith("NONE")) {
                PRIVATE
            } else {
                flags and VISIBILITY_MASK
            }

            return (flags and VISIBILITY_MASK.inv()) or visibilityFlags
        }

        fun create(codebase: PsiBasedCodebase, original: PsiModifierItem): PsiModifierItem {
            val originalAnnotations = original.annotations ?: return PsiModifierItem(codebase, original.flags)
            val copy: MutableList<AnnotationItem> = ArrayList(originalAnnotations.size)
            originalAnnotations.mapTo(copy) { PsiAnnotationItem.create(codebase, it as PsiAnnotationItem) }
            return PsiModifierItem(codebase, original.flags, copy)
        }
    }
}
