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

import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.ParameterItem
import com.android.tools.metalava.model.TypeItem
import com.android.tools.metalava.model.psi.CodePrinter.Companion.constantToSource
import com.intellij.openapi.components.ServiceManager
import com.intellij.psi.PsiParameter
import org.jetbrains.kotlin.psi.KtConstantExpression
import org.jetbrains.kotlin.psi.KtNamedFunction
import org.jetbrains.kotlin.psi.KtParameter
import org.jetbrains.kotlin.psi.psiUtil.parameterIndex
import org.jetbrains.uast.UExpression
import org.jetbrains.uast.UastContext
import org.jetbrains.uast.kotlin.declarations.KotlinUMethod

class PsiParameterItem(
    override val codebase: PsiBasedCodebase,
    private val psiParameter: PsiParameter,
    private val name: String,
    override val parameterIndex: Int,
    modifiers: PsiModifierItem,
    documentation: String,
    private val type: PsiTypeItem
) : PsiItem(
    codebase = codebase,
    modifiers = modifiers,
    documentation = documentation,
    element = psiParameter
), ParameterItem {
    lateinit var containingMethod: PsiMethodItem

    override fun name(): String = name

    override fun publicName(): String? {
        if (isKotlin(psiParameter)) {
            // Don't print out names for extension function receiver parameters
            if (isReceiver()) {
                return null
            }
            return name
        } else {
            // Java: Look for @ParameterName annotation
            val annotation = modifiers.annotations().firstOrNull { it.isParameterName() }
            if (annotation != null) {
                return annotation.attributes().firstOrNull()?.value?.value()?.toString()
            }
        }

        return null
    }

    override fun hasDefaultValue(): Boolean {
        return if (isKotlin(psiParameter)) {
            getKtParameter()?.hasDefaultValue() ?: false && defaultValue() != INVALID_VALUE
        } else {
            // Java: Look for @ParameterName annotation
            modifiers.annotations().any { it.isDefaultValue() }
        }
    }

    // Note receiver parameter used to be named $receiver in previous UAST versions, now it is $this$functionName
    private fun isReceiver(): Boolean = parameterIndex == 0 && name.startsWith("\$this\$")

    private fun getKtParameter(): KtParameter? {
        val ktParameters =
            ((containingMethod.psiMethod as? KotlinUMethod)?.sourcePsi as? KtNamedFunction)?.valueParameters
                ?: return null

        // Perform matching based on parameter names, because indices won't work in the
        // presence of @JvmOverloads where UAST generates multiple permutations of the
        // method from the same KtParameters array.

        // Quick lookup first which usually works (lined up from the end to account
        // for receivers for extension methods etc)
        val rem = containingMethod.parameters().size - parameterIndex
        val index = ktParameters.size - rem
        if (index >= 0) {
            val parameter = ktParameters[index]
            if (parameter.name == name) {
                return parameter
            }
        }

        for (parameter in ktParameters) {
            if (parameter.name == name) {
                return parameter
            }
        }

        // Fallback to handle scenario where the real parameter names are hidden by
        // UAST (see UastKotlinPsiParameter which replaces parameter names to p$index)
        if (index >= 0) {
            val parameter = ktParameters[index]
            if (!isReceiver()) {
                return parameter
            }
        }

        return null
    }

    private var defaultValue: String? = null

    override fun defaultValue(): String? {
        if (defaultValue == null) {
            defaultValue = computeDefaultValue()
        }
        return defaultValue
    }

    private fun computeDefaultValue(): String? {
        if (isKotlin(psiParameter)) {
            val ktParameter = getKtParameter() ?: return null
            if (ktParameter.hasDefaultValue()) {
                val defaultValue = ktParameter.defaultValue ?: return null
                if (defaultValue is KtConstantExpression) {
                    return defaultValue.text
                }

                val uastContext = ServiceManager.getService(psiParameter.project, UastContext::class.java)
                    ?: error("UastContext not found")
                val defaultExpression: UExpression = uastContext.convertElement(
                    defaultValue, null,
                    UExpression::class.java
                ) as? UExpression ?: return INVALID_VALUE
                val constant = defaultExpression.evaluate()
                return if (constant != null && constant !is kotlin.Pair<*, *>) {
                    constantToSource(constant)
                } else {
                    // Expression: Compute from UAST rather than just using the source text
                    // such that we can ensure references are fully qualified etc.
                    codebase.printer.toSourceString(defaultExpression)
                }
            }

            return INVALID_VALUE
        } else {
            // Java: Look for @ParameterName annotation
            val annotation = modifiers.annotations().firstOrNull { it.isDefaultValue() }
            if (annotation != null) {
                return annotation.attributes().firstOrNull()?.value?.value()?.toString()
            }
        }

        return null
    }

    override fun type(): TypeItem = type
    override fun containingMethod(): MethodItem = containingMethod

    override fun equals(other: Any?): Boolean {
        if (this === other) {
            return true
        }
        return other is ParameterItem && parameterIndex == other.parameterIndex && containingMethod == other.containingMethod()
    }

    override fun hashCode(): Int {
        return parameterIndex
    }

    override fun toString(): String = "parameter ${name()}"

    override fun isVarArgs(): Boolean {
        return psiParameter.isVarArgs || modifiers.isVarArg()
    }

    companion object {
        fun create(
            codebase: PsiBasedCodebase,
            psiParameter: PsiParameter,
            parameterIndex: Int
        ): PsiParameterItem {
            val name = psiParameter.name ?: "arg${psiParameter.parameterIndex() + 1}"
            val commentText = "" // no javadocs on individual parameters
            val modifiers = modifiers(codebase, psiParameter, commentText)
            val type = codebase.getType(psiParameter.type)
            val parameter = PsiParameterItem(
                codebase = codebase,
                psiParameter = psiParameter,
                name = name,
                parameterIndex = parameterIndex,
                documentation = commentText,
                modifiers = modifiers,
                type = type
            )
            parameter.modifiers.setOwner(parameter)
            return parameter
        }

        fun create(
            codebase: PsiBasedCodebase,
            original: PsiParameterItem
        ): PsiParameterItem {
            val parameter = PsiParameterItem(
                codebase = codebase,
                psiParameter = original.psiParameter,
                name = original.name,
                parameterIndex = original.parameterIndex,
                documentation = original.documentation,
                modifiers = PsiModifierItem.create(codebase, original.modifiers),
                type = PsiTypeItem.create(codebase, original.type)
            )
            parameter.modifiers.setOwner(parameter)
            return parameter
        }

        fun create(
            codebase: PsiBasedCodebase,
            original: List<ParameterItem>
        ): List<PsiParameterItem> {
            return original.map { create(codebase, it as PsiParameterItem) }
        }

        /**
         * Private marker return value from [#computeDefaultValue] signifying that the parameter
         * has a default value but we were unable to compute a suitable static string representation for it
         */
        private const val INVALID_VALUE = "__invalid_value__"
    }
}