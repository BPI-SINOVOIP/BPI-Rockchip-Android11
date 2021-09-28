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

package com.android.tools.metalava

import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.ParameterItem
import com.android.tools.metalava.model.TypeItem
import com.android.tools.metalava.model.visitors.ApiVisitor
import com.intellij.lang.java.lexer.JavaLexer
import org.jetbrains.kotlin.lexer.KtTokens
import org.jetbrains.kotlin.psi.KtObjectDeclaration
import org.jetbrains.kotlin.psi.KtProperty
import org.jetbrains.kotlin.psi.psiUtil.containingClassOrObject
import org.jetbrains.uast.kotlin.KotlinUField

// Enforces the interoperability guidelines outlined in
//   https://android.github.io/kotlin-guides/interop.html
//
// Also potentially makes other API suggestions.
class KotlinInteropChecks(val reporter: Reporter) {
    fun check(codebase: Codebase) {

        codebase.accept(object : ApiVisitor(
            // Sort by source order such that warnings follow source line number order
            methodComparator = MethodItem.sourceOrderComparator,
            fieldComparator = FieldItem.comparator
        ) {
            private var isKotlin = false

            override fun visitClass(cls: ClassItem) {
                isKotlin = cls.isKotlin()
            }

            override fun visitMethod(method: MethodItem) {
                checkMethod(method, isKotlin)
            }

            override fun visitField(field: FieldItem) {
                checkField(field, isKotlin)
            }
        })
    }

    fun checkField(field: FieldItem, isKotlin: Boolean = field.isKotlin()) {
        if (isKotlin) {
            ensureCompanionFieldJvmField(field)
        }
        ensureFieldNameNotKeyword(field)
    }

    fun checkMethod(method: MethodItem, isKotlin: Boolean = method.isKotlin()) {
        if (!method.isConstructor()) {
            if (isKotlin) {
                ensureDefaultParamsHaveJvmOverloads(method)
                ensureCompanionJvmStatic(method)
                ensureExceptionsDocumented(method)
            } else {
                ensureMethodNameNotKeyword(method)
                ensureParameterNamesNotKeywords(method)
            }
            ensureLambdaLastParameter(method)
        }
    }

    private fun ensureExceptionsDocumented(method: MethodItem) {
        if (!method.isKotlin()) {
            return
        }

        val exceptions = method.findThrownExceptions()
        if (exceptions.isEmpty()) {
            return
        }
        val doc = method.documentation
        for (exception in exceptions.sortedBy { it.qualifiedName() }) {
            val checked = !(exception.extends("java.lang.RuntimeException") ||
                exception.extends("java.lang.Error"))
            if (checked) {
                val annotation = method.modifiers.findAnnotation("kotlin.jvm.Throws")
                if (annotation != null) {
                    // There can be multiple values
                    for (attribute in annotation.attributes()) {
                        for (v in attribute.leafValues()) {
                            val source = v.toSource()
                            if (source.endsWith(exception.simpleName() + "::class")) {
                                return
                            }
                        }
                    }
                }
                reporter.report(
                    Issues.DOCUMENT_EXCEPTIONS, method,
                    "Method ${method.containingClass().simpleName()}.${method.name()} appears to be throwing ${exception.qualifiedName()}; this should be recorded with a @Throws annotation; see https://android.github.io/kotlin-guides/interop.html#document-exceptions"
                )
            } else {
                if (!doc.contains(exception.simpleName())) {
                    reporter.report(
                        Issues.DOCUMENT_EXCEPTIONS, method,
                        "Method ${method.containingClass().simpleName()}.${method.name()} appears to be throwing ${exception.qualifiedName()}; this should be listed in the documentation; see https://android.github.io/kotlin-guides/interop.html#document-exceptions"
                    )
                }
            }
        }
    }

    private fun ensureCompanionFieldJvmField(field: FieldItem) {
        val modifiers = field.modifiers
        if (modifiers.isPublic() && modifiers.isFinal()) {
            // UAST will inline const fields into the surrounding class, so we have to
            // dip into Kotlin PSI to figure out if this field was really declared in
            // a companion object
            val psi = field.psi()
            if (psi is KotlinUField) {
                val sourcePsi = psi.sourcePsi
                if (sourcePsi is KtProperty) {
                    val companionClassName = sourcePsi.containingClassOrObject?.name
                    if (companionClassName == "Companion") {
                        // JvmField cannot be applied to const property (https://github.com/JetBrains/kotlin/blob/dc7b1fbff946d1476cc9652710df85f65664baee/compiler/frontend.java/src/org/jetbrains/kotlin/resolve/jvm/checkers/JvmFieldApplicabilityChecker.kt#L46)
                        if (!modifiers.isConst()) {
                            if (modifiers.findAnnotation("kotlin.jvm.JvmField") == null) {
                                reporter.report(
                                    Issues.MISSING_JVMSTATIC, field,
                                    "Companion object constants like ${field.name()} should be marked @JvmField for Java interoperability; see https://developer.android.com/kotlin/interop#companion_constants"
                                )
                            } else if (modifiers.findAnnotation("kotlin.jvm.JvmStatic") != null) {
                                reporter.report(
                                    Issues.MISSING_JVMSTATIC, field,
                                    "Companion object constants like ${field.name()} should be using @JvmField, not @JvmStatic; see https://developer.android.com/kotlin/interop#companion_constants"
                                )
                            }
                        }
                    }
                } else if (sourcePsi is KtObjectDeclaration && sourcePsi.isCompanion()) {
                    // We are checking if we have public properties that we can expect to be constant
                    // (that is, declared via `val`) but that aren't declared 'const' in a companion
                    // object that are not annotated with @JvmField or annotated with @JvmStatic
                    // https://developer.android.com/kotlin/interop#companion_constants
                    val ktProperties = sourcePsi.declarations.filter { declaration ->
                        declaration is KtProperty && !declaration.isVar && !declaration.hasModifier(
                            KtTokens.CONST_KEYWORD
                        ) && declaration.annotationEntries.filter {
                                annotationEntry -> annotationEntry.shortName!!.asString() == "JvmField"
                        }.isEmpty() }
                    for (ktProperty in ktProperties) {
                        if (ktProperty.annotationEntries.filter { annotationEntry -> annotationEntry.shortName!!.asString() == "JvmStatic" }.isEmpty()) {
                            reporter.report(
                                Issues.MISSING_JVMSTATIC, ktProperty,
                                "Companion object constants like ${ktProperty.name} should be marked @JvmField for Java interoperability; see https://developer.android.com/kotlin/interop#companion_constants"
                            )
                        } else {
                            reporter.report(
                                Issues.MISSING_JVMSTATIC, ktProperty,
                                "Companion object constants like ${ktProperty.name} should be using @JvmField, not @JvmStatic; see https://developer.android.com/kotlin/interop#companion_constants"
                            )
                        }
                    }
                }
            }
        }
    }

    private fun ensureLambdaLastParameter(method: MethodItem) {
        val parameters = method.parameters()
        if (parameters.size > 1) {
            // Make sure that SAM-compatible parameters are last
            val lastIndex = parameters.size - 1
            if (!isSamCompatible(parameters[lastIndex])) {
                for (i in lastIndex - 1 downTo 0) {
                    val parameter = parameters[i]
                    if (isSamCompatible(parameter)) {
                        val message =
                            "${if (isKotlinLambda(parameter.type())) "lambda" else "SAM-compatible"
                            } parameters (such as parameter ${i + 1}, \"${parameter.name()}\", in ${
                            method.containingClass().qualifiedName()}.${method.name()
                            }) should be last to improve Kotlin interoperability; see " +
                                "https://kotlinlang.org/docs/reference/java-interop.html#sam-conversions"
                        reporter.report(Issues.SAM_SHOULD_BE_LAST, method, message)
                        break
                    }
                }
            }
        }
    }

    private fun ensureCompanionJvmStatic(method: MethodItem) {
        if (method.containingClass().simpleName() == "Companion" && method.isKotlin() && method.modifiers.isPublic()) {
            if (method.isKotlinProperty()) {
                /* Not yet working; can't find the @JvmStatic/@JvmField in the AST
                    // Only flag the read method, not the write method
                    if (method.name().startsWith("get")) {
                        // Find the backing field; *that's* where the @JvmStatic/@JvmField annotations
                        // are available (but the field itself is not visited since it is typically private
                        // and therefore not part of the API visitor. Dip into Kotlin PSI to accurately
                        // find the field name instead of guessing based on getter name.
                        var field: FieldItem? = null
                        val psi = method.psi()
                        if (psi is KotlinUMethod) {
                            val property = psi.sourcePsi as? KtProperty
                            if (property != null) {
                                val propertyName = property.name
                                if (propertyName != null) {
                                    field = method.containingClass().containingClass()?.findField(propertyName)
                                }
                            }
                        }

                        if (field != null) {
                            if (field.modifiers.findAnnotation("kotlin.jvm.JvmStatic") != null) {
                                reporter.report(
                                    Errors.MISSING_JVMSTATIC, method,
                                    "Companion object constants should be using @JvmField, not @JvmStatic; see https://developer.android.com/kotlin/interop#companion_constants"
                                )
                            } else if (field.modifiers.findAnnotation("kotlin.jvm.JvmField") == null) {
                                reporter.report(
                                    Errors.MISSING_JVMSTATIC, method,
                                    "Companion object constants should be marked @JvmField for Java interoperability; see https://developer.android.com/kotlin/interop#companion_constants"
                                )
                            }
                        }
                    }
                    */
            } else if (method.modifiers.findAnnotation("kotlin.jvm.JvmStatic") == null) {
                reporter.report(
                    Issues.MISSING_JVMSTATIC, method,
                    "Companion object methods like ${method.name()} should be marked @JvmStatic for Java interoperability; see https://developer.android.com/kotlin/interop#companion_functions"
                )
            }
        }
    }

    private fun ensureFieldNameNotKeyword(field: FieldItem) {
        checkKotlinKeyword(field.name(), "field", field)
    }

    private fun ensureMethodNameNotKeyword(method: MethodItem) {
        checkKotlinKeyword(method.name(), "method", method)
    }

    private fun ensureDefaultParamsHaveJvmOverloads(method: MethodItem) {
        if (!method.isKotlin()) {
            // Rule does not apply for Java, e.g. if you specify @DefaultValue
            // in Java you still don't have the option of adding @JvmOverloads
            return
        }
        if (method.containingClass().isInterface()) {
            // '@JvmOverloads' annotation cannot be used on interface methods
            // (https://github.com/JetBrains/kotlin/blob/dc7b1fbff946d1476cc9652710df85f65664baee/compiler/frontend.java/src/org/jetbrains/kotlin/resolve/jvm/diagnostics/DefaultErrorMessagesJvm.java#L50)
            return
        }
        val parameters = method.parameters()
        if (parameters.size <= 1) {
            // No need for overloads when there is at most one version...
            return
        }

        var haveDefault = false
        if (parameters.isNotEmpty() && method.isJava()) {
            // Public java parameter names should also not use Kotlin keywords as names
            for (parameter in parameters) {
                if (parameter.hasDefaultValue()) {
                    haveDefault = true
                    break
                }
            }
        }

        if (haveDefault && method.modifiers.findAnnotation("kotlin.jvm.JvmOverloads") == null &&
            // Extension methods and inline functions aren't really useful from Java anyway
            !method.isExtensionMethod() && !method.modifiers.isInline()
        ) {
            reporter.report(
                Issues.MISSING_JVMSTATIC, method,
                "A Kotlin method with default parameter values should be annotated with @JvmOverloads for better Java interoperability; see https://android.github.io/kotlin-guides/interop.html#function-overloads-for-defaults"
            )
        }
    }

    private fun ensureParameterNamesNotKeywords(method: MethodItem) {
        val parameters = method.parameters()

        if (parameters.isNotEmpty() && method.isJava()) {
            // Public java parameter names should also not use Kotlin keywords as names
            for (parameter in parameters) {
                val publicName = parameter.publicName() ?: continue
                checkKotlinKeyword(publicName, "parameter", parameter)
            }
        }
    }

    // Don't use Kotlin hard keywords in Java signatures
    private fun checkKotlinKeyword(name: String, typeLabel: String, item: Item) {
        if (isKotlinHardKeyword(name)) {
            reporter.report(
                Issues.KOTLIN_KEYWORD, item,
                "Avoid $typeLabel names that are Kotlin hard keywords (\"$name\"); see https://android.github.io/kotlin-guides/interop.html#no-hard-keywords"
            )
        } else if (isJavaKeyword(name)) {
            reporter.report(
                Issues.KOTLIN_KEYWORD, item,
                "Avoid $typeLabel names that are Java keywords (\"$name\"); this makes it harder to use the API from Java"
            )
        }
    }

    private fun isSamCompatible(parameter: ParameterItem): Boolean {
        val type = parameter.type()
        if (type.primitive) {
            return false
        }

        if (isKotlinLambda(type)) {
            return true
        }

        val cls = type.asClass() ?: return false
        if (!cls.isInterface()) {
            return false
        }

        if (cls.methods().filter { !it.modifiers.isDefault() }.size != 1) {
            return false
        }

        if (cls.superClass()?.isInterface() == true) {
            return false
        }

        // Some interfaces, while they have a single method are not considered to be SAM that we
        // want to be the last argument because often it leads to unexpected behavior of the
        // trailing lambda.
        when (cls.qualifiedName()) {
            "java.util.concurrent.Executor",
            "java.lang.Iterable" -> return false
        }
        return true
    }

    private fun isKotlinLambda(type: TypeItem) =
        type.toErasedTypeString() == "kotlin.jvm.functions.Function1"

    private fun isKotlinHardKeyword(keyword: String): Boolean {
        // From https://github.com/JetBrains/kotlin/blob/master/core/descriptors/src/org/jetbrains/kotlin/renderer/KeywordStringsGenerated.java
        when (keyword) {
            "as",
            "break",
            "class",
            "continue",
            "do",
            "else",
            "false",
            "for",
            "fun",
            "if",
            "in",
            "interface",
            "is",
            "null",
            "object",
            "package",
            "return",
            "super",
            "this",
            "throw",
            "true",
            "try",
            "typealias",
            "typeof",
            "val",
            "var",
            "when",
            "while"
            -> return true
        }

        return false
    }

    /** Returns true if the given string is a reserved Java keyword  */
    private fun isJavaKeyword(keyword: String): Boolean {
        return JavaLexer.isKeyword(keyword, options.javaLanguageLevel)
    }
}
