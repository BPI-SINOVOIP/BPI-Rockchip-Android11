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

import com.android.SdkConstants.DOT_CLASS
import com.android.tools.lint.detector.api.ConstantEvaluator
import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.canonicalizeFloatingPointString
import com.android.tools.metalava.model.javaEscapeString
import com.android.tools.metalava.reporter
import com.android.utils.XmlUtils
import com.intellij.psi.PsiAnnotation
import com.intellij.psi.PsiAnnotationMemberValue
import com.intellij.psi.PsiArrayInitializerMemberValue
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiClassObjectAccessExpression
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiField
import com.intellij.psi.PsiLiteral
import com.intellij.psi.PsiReference
import com.intellij.psi.PsiTypeCastExpression
import com.intellij.psi.PsiVariable
import org.jetbrains.kotlin.name.ClassId
import org.jetbrains.uast.UAnnotation
import org.jetbrains.uast.UBinaryExpression
import org.jetbrains.uast.UBinaryExpressionWithType
import org.jetbrains.uast.UBlockExpression
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UExpression
import org.jetbrains.uast.ULambdaExpression
import org.jetbrains.uast.ULiteralExpression
import org.jetbrains.uast.UReferenceExpression
import org.jetbrains.uast.UUnaryExpression
import org.jetbrains.uast.util.isArrayInitializer
import org.jetbrains.uast.util.isConstructorCall
import org.jetbrains.uast.util.isTypeCast
import java.util.function.Predicate

/** Utility methods */
open class CodePrinter(
    private val codebase: Codebase,
    /** Whether we should inline the values of fields, e.g. instead of "Integer.MAX_VALUE" we'd emit "0x7fffffff" */
    private val inlineFieldValues: Boolean = true,
    /** Whether we should inline constants when possible, e.g. instead of "2*20+2" we'd emit "42" */
    private val inlineConstants: Boolean = true,
    /** Whether we should drop unknown AST nodes instead of inserting the corresponding source text strings */
    private val skipUnknown: Boolean = false,
    /** An optional filter to use to determine if we should emit a reference to an item */
    private val filterReference: Predicate<Item>? = null
) {
    open fun warning(message: String, psiElement: PsiElement? = null) {
        reporter.report(Issues.INTERNAL_ERROR, psiElement, message)
    }

    open fun warning(message: String, uElement: UElement) {
        warning(message, uElement.sourcePsi ?: uElement.javaPsi)
    }

    /** Given an annotation member value, returns the corresponding Java source expression */
    fun toSourceExpression(value: PsiAnnotationMemberValue, owner: Item): String {
        val sb = StringBuilder()
        appendSourceExpression(value, sb, owner)
        return sb.toString()
    }

    private fun appendSourceExpression(value: PsiAnnotationMemberValue, sb: StringBuilder, owner: Item): Boolean {
        if (value is PsiReference) {
            val resolved = value.resolve()
            if (resolved is PsiField) {
                sb.append(resolved.containingClass?.qualifiedName).append('.').append(resolved.name)
                return true
            }
        } else if (value is PsiLiteral) {
            return appendSourceLiteral(value.value, sb, owner)
        } else if (value is PsiClassObjectAccessExpression) {
            sb.append(value.operand.type.canonicalText).append(DOT_CLASS)
            return true
        } else if (value is PsiArrayInitializerMemberValue) {
            sb.append('{')
            var first = true
            val initialLength = sb.length
            for (e in value.initializers) {
                val length = sb.length
                if (first) {
                    first = false
                } else {
                    sb.append(", ")
                }
                val appended = appendSourceExpression(e, sb, owner)
                if (!appended) {
                    // trunk off comma if it bailed for some reason (e.g. constant
                    // filtered out by API etc)
                    sb.setLength(length)
                    if (length == initialLength) {
                        first = true
                    }
                }
            }
            sb.append('}')
            return true
        } else if (value is PsiAnnotation) {
            sb.append('@').append(value.qualifiedName)
            return true
        } else {
            if (value is PsiTypeCastExpression) {
                val type = value.castType?.type
                val operand = value.operand
                if (type != null && operand is PsiAnnotationMemberValue) {
                    sb.append('(')
                    sb.append(type.canonicalText)
                    sb.append(')')
                    return appendSourceExpression(operand, sb, owner)
                }
            }
            val constant = ConstantEvaluator.evaluate(null, value)
            if (constant != null) {
                return appendSourceLiteral(constant, sb, owner)
            }
        }
        reporter.report(Issues.INTERNAL_ERROR, owner, "Unexpected annotation default value $value")
        return false
    }

    fun toSourceString(value: UExpression?): String? {
        value ?: return null
        val sb = StringBuilder()
        return if (appendExpression(sb, value)
        ) {
            sb.toString()
        } else {
            null
        }
    }

    private fun appendExpression(
        sb: StringBuilder,
        expression: UExpression
    ): Boolean {
        if (expression.isArrayInitializer()) {
            val call = expression as UCallExpression
            val initializers = call.valueArguments
            sb.append('{')
            var first = true
            val initialLength = sb.length
            for (e in initializers) {
                val length = sb.length
                if (first) {
                    first = false
                } else {
                    sb.append(", ")
                }
                val appended = appendExpression(sb, e)
                if (!appended) {
                    // truncate trailing comma if it bailed for some reason (e.g. constant
                    // filtered out by API etc)
                    sb.setLength(length)
                    if (length == initialLength) {
                        first = true
                    }
                }
            }
            sb.append('}')
            return sb.length != 2
        } else if (expression is UReferenceExpression) {
            val resolved = expression.resolve()
            when (resolved) {
                is PsiField -> {
                    @Suppress("UnnecessaryVariable")
                    val field = resolved
                    if (!inlineFieldValues) {
                        val value = field.computeConstantValue()
                        if (appendLiteralValue(sb, value)) {
                            return true
                        }
                    }

                    val declaringClass = field.containingClass
                    if (declaringClass == null) {
                        warning("No containing class found for " + field.name, field)
                        return false
                    }
                    val qualifiedName = declaringClass.qualifiedName
                    val fieldName = field.name

                    if (qualifiedName != null) {
                        if (filterReference != null) {
                            val cls = codebase.findClass(qualifiedName)
                            val fld = cls?.findField(fieldName, true)
                            if (fld == null || !filterReference.test(fld)) {
                                // This field is not visible: remove from typedef
                                if (fld != null) {
                                    reporter.report(
                                        Issues.HIDDEN_TYPEDEF_CONSTANT, fld,
                                        "Typedef class references hidden field $fld: removed from typedef metadata"
                                    )
                                }
                                return false
                            }
                        }
                        sb.append(qualifiedName)
                        sb.append('.')
                        sb.append(fieldName)
                        return true
                    }
                    return if (skipUnknown) {
                        false
                    } else {
                        sb.append(expression.asSourceString())
                        true
                    }
                }
                is PsiVariable -> {
                    sb.append(resolved.name)
                    return true
                }
                else -> {
                    if (skipUnknown) {
                        warning("Unexpected reference to $expression", expression)
                        return false
                    }
                    sb.append(expression.asSourceString())
                    return true
                }
            }
        } else if (expression is ULiteralExpression) {
            val literalValue = expression.value
            if (appendLiteralValue(sb, literalValue)) {
                return true
            }
        } else if (expression is UAnnotation) {
            sb.append('@').append(expression.qualifiedName)
            return true
        } else if (expression is UBinaryExpressionWithType) {
            if ((expression).isTypeCast()) {
                sb.append('(').append(expression.type.canonicalText).append(')')
                val operand = expression.operand
                return appendExpression(sb, operand)
            }
            return false
        } else if (expression is UBinaryExpression) {
            if (inlineConstants) {
                val constant = expression.evaluate()
                if (constant != null) {
                    sb.append(constantToSource(constant))
                    return true
                }
            }

            if (appendExpression(sb, expression.leftOperand)) {
                sb.append(' ').append(expression.operator.text).append(' ')
                if (appendExpression(sb, expression.rightOperand)) {
                    return true
                }
            }
        } else if (expression is UUnaryExpression) {
            sb.append(expression.operator.text)
            if (appendExpression(sb, expression.operand)) {
                return true
            }
        } else if (expression is ULambdaExpression) {
            sb.append("{ ")
            val valueParameters = expression.valueParameters
            if (valueParameters.isNotEmpty()) {
                var first = true
                for (parameter in valueParameters) {
                    if (first) {
                        first = false
                    } else {
                        sb.append(", ")
                    }
                    sb.append(parameter.name)
                }
                sb.append(" -> ")
            }
            val body = expression.body

            if (body is UBlockExpression) {
                var first = true
                for (e in body.expressions) {
                    if (first) {
                        first = false
                    } else {
                        sb.append("; ")
                    }
                    if (!appendExpression(sb, e)) {
                        return false
                    }
                }

                // Special case: Turn empty lambda {  } into {}
                if (sb.length > 2) {
                    sb.append(' ')
                } else {
                    sb.setLength(1)
                }
                sb.append('}')
                return true
            } else {
                if (appendExpression(sb, body)) {
                    sb.append(" }")
                    return true
                }
            }
        } else if (expression is UBlockExpression) {
            sb.append('{')
            var first = true
            for (e in expression.expressions) {
                if (first) {
                    first = false
                } else {
                    sb.append("; ")
                }
                if (!appendExpression(sb, e)) {
                    return false
                }
            }
            sb.append('}')
            return true
        } else if (expression.isConstructorCall()) {
            val call = expression as UCallExpression
            val resolved = call.classReference?.resolve()
            if (resolved is PsiClass) {
                sb.append(resolved.qualifiedName)
            } else {
                sb.append(call.classReference?.resolvedName)
            }
            sb.append('(')
            var first = true
            for (arg in call.valueArguments) {
                if (first) {
                    first = false
                } else {
                    sb.append(", ")
                }
                if (!appendExpression(sb, arg)) {
                    return false
                }
            }
            sb.append(')')
            return true
        } else {
            sb.append(expression.asSourceString())
            return true
        }

        // For example, binary expressions like 3 + 4
        val literalValue = ConstantEvaluator.evaluate(null, expression)
        if (literalValue != null) {
            if (appendLiteralValue(sb, literalValue)) {
                return true
            }
        }

        warning("Unexpected annotation expression of type ${expression.javaClass} and is $expression", expression)

        return false
    }

    companion object {
        private fun appendLiteralValue(sb: StringBuilder, literalValue: Any?): Boolean {
            if (literalValue == null) {
                sb.append("null")
                return true
            } else if (literalValue is Number || literalValue is Boolean) {
                sb.append(literalValue.toString())
                return true
            } else if (literalValue is String || literalValue is Char) {
                sb.append('"')
                XmlUtils.appendXmlAttributeValue(sb, literalValue.toString())
                sb.append('"')
                return true
            }
            return false
        }

        fun constantToSource(value: Any?): String {
            if (value == null) {
                return "null"
            }

            when (value) {
                is Int -> {
                    return value.toString()
                }
                is String -> {
                    return "\"${javaEscapeString(value)}\""
                }
                is Long -> {
                    return value.toString() + "L"
                }
                is Boolean -> {
                    return value.toString()
                }
                is Byte -> {
                    return value.toString()
                }
                is Short -> {
                    return value.toString()
                }
                is Float -> {
                    return when (value) {
                        Float.POSITIVE_INFINITY -> "(1.0f/0.0f)"
                        Float.NEGATIVE_INFINITY -> "(-1.0f/0.0f)"
                        Float.NaN -> "(0.0f/0.0f)"
                        else -> {
                            canonicalizeFloatingPointString(value.toString()) + "f"
                        }
                    }
                }
                is Double -> {
                    return when (value) {
                        Double.POSITIVE_INFINITY -> "(1.0/0.0)"
                        Double.NEGATIVE_INFINITY -> "(-1.0/0.0)"
                        Double.NaN -> "(0.0/0.0)"
                        else -> {
                            canonicalizeFloatingPointString(value.toString())
                        }
                    }
                }
                is Char -> {
                    return String.format("'%s'", javaEscapeString(value.toString()))
                }

                is kotlin.Pair<*, *> -> {
                    val first = value.first
                    if (first is ClassId) {
                        return first.packageFqName.asString() + "." + first.relativeClassName.asString()
                    }
                }
            }

            return value.toString()
        }

        fun constantToExpression(constant: Any?): String? {
            return when (constant) {
                is Int -> "0x${Integer.toHexString(constant)}"
                is String -> "\"${javaEscapeString(constant)}\""
                is Long -> "${constant}L"
                is Boolean -> constant.toString()
                is Byte -> Integer.toHexString(constant.toInt())
                is Short -> Integer.toHexString(constant.toInt())
                is Float -> {
                    when (constant) {
                        Float.POSITIVE_INFINITY -> "Float.POSITIVE_INFINITY"
                        Float.NEGATIVE_INFINITY -> "Float.NEGATIVE_INFINITY"
                        Float.NaN -> "Float.NaN"
                        else -> {
                            "${canonicalizeFloatingPointString(constant.toString())}F"
                        }
                    }
                }
                is Double -> {
                    when (constant) {
                        Double.POSITIVE_INFINITY -> "Double.POSITIVE_INFINITY"
                        Double.NEGATIVE_INFINITY -> "Double.NEGATIVE_INFINITY"
                        Double.NaN -> "Double.NaN"
                        else -> {
                            canonicalizeFloatingPointString(constant.toString())
                        }
                    }
                }
                is Char -> {
                    "'${javaEscapeString(constant.toString())}'"
                }
                else -> {
                    null
                }
            }
        }

        private fun appendSourceLiteral(v: Any?, sb: StringBuilder, owner: Item): Boolean {
            if (v == null) {
                sb.append("null")
                return true
            }
            when (v) {
                is Int, is Boolean, is Byte, is Short -> {
                    sb.append(v.toString())
                    return true
                }
                is Long -> {
                    sb.append(v.toString()).append('L')
                    return true
                }
                is String -> {
                    sb.append('"').append(javaEscapeString(v)).append('"')
                    return true
                }
                is Float -> {
                    return when (v) {
                        Float.POSITIVE_INFINITY -> {
                            // This convention (displaying fractions) is inherited from doclava
                            sb.append("(1.0f/0.0f)"); true
                        }
                        Float.NEGATIVE_INFINITY -> {
                            sb.append("(-1.0f/0.0f)"); true
                        }
                        Float.NaN -> {
                            sb.append("(0.0f/0.0f)"); true
                        }
                        else -> {
                            sb.append(canonicalizeFloatingPointString(v.toString()) + "f")
                            true
                        }
                    }
                }
                is Double -> {
                    return when (v) {
                        Double.POSITIVE_INFINITY -> {
                            // This convention (displaying fractions) is inherited from doclava
                            sb.append("(1.0/0.0)"); true
                        }
                        Double.NEGATIVE_INFINITY -> {
                            sb.append("(-1.0/0.0)"); true
                        }
                        Double.NaN -> {
                            sb.append("(0.0/0.0)"); true
                        }
                        else -> {
                            sb.append(canonicalizeFloatingPointString(v.toString()))
                            true
                        }
                    }
                }
                is Char -> {
                    sb.append('\'').append(javaEscapeString(v.toString())).append('\'')
                    return true
                }
                else -> {
                    reporter.report(Issues.INTERNAL_ERROR, owner, "Unexpected literal value $v")
                }
            }

            return false
        }
    }
}
