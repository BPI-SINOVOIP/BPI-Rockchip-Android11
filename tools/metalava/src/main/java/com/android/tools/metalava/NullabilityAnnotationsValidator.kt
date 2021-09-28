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
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.ParameterItem
import com.android.tools.metalava.model.SUPPORT_TYPE_USE_ANNOTATIONS
import com.android.tools.metalava.model.TypeItem
import com.android.tools.metalava.model.visitors.ApiVisitor
import com.google.common.io.Files
import java.io.File
import java.io.PrintWriter
import kotlin.text.Charsets.UTF_8

private const val RETURN_LABEL = "return value"

/**
 * Class that validates nullability annotations in the codebase.
 */
class NullabilityAnnotationsValidator {

    private enum class ErrorType {
        MULTIPLE,
        ON_PRIMITIVE,
        BAD_TYPE_PARAM,
    }

    private interface Issue {
        val method: MethodItem
    }

    private data class Error(
        override val method: MethodItem,
        val label: String,
        val type: ErrorType
    ) : Issue {
        override fun toString(): String {
            return "ERROR: $method, $label, $type"
        }
    }

    private enum class WarningType {
        MISSING,
    }

    private data class Warning(
        override val method: MethodItem,
        val label: String,
        val type: WarningType
    ) : Issue {
        override fun toString(): String {
            return "WARNING: $method, $label, $type"
        }
    }

    private val errors: MutableList<Error> = mutableListOf()
    private val warnings: MutableList<Warning> = mutableListOf()

    /**
     * Validate all of the methods in the classes named in [topLevelClassNames] and in all their
     * nested classes. Violations are stored by the validator and will be reported by [report].
     */
    fun validateAll(codebase: Codebase, topLevelClassNames: List<String>) {
        for (topLevelClassName in topLevelClassNames) {
            val topLevelClass = codebase.findClass(topLevelClassName)
                ?: throw DriverException("Trying to validate nullability annotations for class $topLevelClassName which could not be found in main codebase")
            // Visit methods to check their return type, and parameters to check them. Don't visit
            // constructors as we don't want to check their return types. This visits members of
            // inner classes as well.
            topLevelClass.accept(object : ApiVisitor(visitConstructorsAsMethods = false) {

                override fun visitMethod(method: MethodItem) {
                    checkItem(method, RETURN_LABEL, method.returnType(), method)
                }

                override fun visitParameter(parameter: ParameterItem) {
                    checkItem(parameter.containingMethod(), parameter.toString(), parameter.type(), parameter)
                }
            })
        }
    }

    /**
     * As [validateAll], reading the list of class names from [topLevelClassesList]. The file names
     * one top-level class per line, and lines starting with # are skipped. Does nothing if
     * [topLevelClassesList] is null.
     */
    fun validateAllFrom(codebase: Codebase, topLevelClassesList: File?) {
        if (topLevelClassesList != null) {
            val classes =
                Files.readLines(topLevelClassesList, UTF_8)
                    .filterNot { it.isBlank() }
                    .map { it.trim() }
                    .filterNot { it.startsWith("#") }
            validateAll(codebase, classes)
        }
    }

    private fun checkItem(method: MethodItem, label: String, type: TypeItem?, item: Item) {
        if (type == null) {
            throw DriverException("Missing type on $method item $label")
        }
        if (method.isEnumValueOfString()) {
            // Don't validate an enum's valueOf(String) method, which doesn't exist in source.
            return
        }
        val annotations = item.modifiers.annotations()
        val nullabilityAnnotations = annotations.filter(this::isAnyNullabilityAnnotation)
        if (nullabilityAnnotations.size > 1) {
            errors.add(Error(method, label, ErrorType.MULTIPLE))
            return
        }
        checkItemNullability(type, nullabilityAnnotations.firstOrNull(), method, label)
        // TODO: When type annotations are supported, we should check all the type parameters too.
        // We can do invoke this method recursively, using a suitably descriptive label.
        assert(!SUPPORT_TYPE_USE_ANNOTATIONS)
    }

    private fun isNullFromTypeParam(it: AnnotationItem) =
        it.qualifiedName()?.endsWith("NullFromTypeParam") == true

    private fun isAnyNullabilityAnnotation(it: AnnotationItem) =
        it.isNullnessAnnotation() || isNullFromTypeParam(it)

    private fun checkItemNullability(
        type: TypeItem,
        nullability: AnnotationItem?,
        method: MethodItem,
        label: String
    ) {
        when {
            // Primitive (may not have nullability):
            type.primitive -> {
                if (nullability != null) {
                    errors.add(Error(method, label, ErrorType.ON_PRIMITIVE))
                }
            }
            // Array (see comment):
            type.arrayDimensions() > 0 -> {
                // TODO: When type annotations are supported, we should check the annotation on both
                // the array itself and the component type. Until then, there's nothing we can
                // safely do, because e.g. a method parameter declared as '@NonNull Object[]' means
                // a non-null array of unspecified-nullability Objects if that is a PARAMETER
                // annotation, but an unspecified-nullability array of non-null Objects if that is a
                // TYPE_USE annotation.
                assert(!SUPPORT_TYPE_USE_ANNOTATIONS)
            }
            // Type parameter reference (should have nullability):
            type.asTypeParameter() != null -> {
                if (nullability == null) {
                    warnings.add(Warning(method, label, WarningType.MISSING))
                }
            }
            // Anything else (should have nullability, may not be null-from-type-param):
            else -> {
                when {
                    nullability == null -> warnings.add(Warning(method, label, WarningType.MISSING))
                    isNullFromTypeParam(nullability) ->
                        errors.add(Error(method, label, ErrorType.BAD_TYPE_PARAM))
                }
            }
        }
    }

    /**
     * Report on any violations found during earlier validation calls.
     */
    fun report() {
        errors.sortBy { it.toString() }
        warnings.sortBy { it.toString() }
        val warningsTxtFile = options.nullabilityWarningsTxt
        val fatalIssues = mutableListOf<Issue>()
        val nonFatalIssues = mutableListOf<Issue>()

        // Errors are fatal iff options.nullabilityErrorsFatal is set.
        if (options.nullabilityErrorsFatal) {
            fatalIssues.addAll(errors)
        } else {
            nonFatalIssues.addAll(errors)
        }

        // Warnings go to the configured .txt file if present, which means they're not fatal.
        // Else they're fatal iff options.nullabilityErrorsFatal is set.
        if (warningsTxtFile == null && options.nullabilityErrorsFatal) {
            fatalIssues.addAll(warnings)
        } else {
            nonFatalIssues.addAll(warnings)
        }

        // Fatal issues are thrown.
        if (fatalIssues.isNotEmpty()) {
            fatalIssues.forEach { reporter.report(Issues.INVALID_NULLABILITY_ANNOTATION, it.method, it.toString()) }
        }

        // Non-fatal issues are written to the warnings .txt file if present, else logged.
        if (warningsTxtFile != null) {
            PrintWriter(Files.asCharSink(warningsTxtFile, UTF_8).openBufferedStream()).use { w ->
                nonFatalIssues.forEach { w.println(it) }
            }
        } else {
            nonFatalIssues.forEach {
                reporter.report(Issues.INVALID_NULLABILITY_ANNOTATION_WARNING, it.method, "Nullability issue: $it")
            }
        }
    }
}

private fun MethodItem.isEnumValueOfString() =
    containingClass().isEnum() && name() == "valueOf" && parameters().map {
        it.type().toTypeString()
    } == listOf("java.lang.String")
