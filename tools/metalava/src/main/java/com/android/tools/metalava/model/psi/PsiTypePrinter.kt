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

import com.android.tools.metalava.JAVA_LANG_OBJECT
import com.android.tools.metalava.compatibility
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.SUPPORT_TYPE_USE_ANNOTATIONS
import com.android.tools.metalava.model.isNonNullAnnotation
import com.android.tools.metalava.model.isNullableAnnotation
import com.android.tools.metalava.options
import com.intellij.openapi.util.text.StringUtil
import com.intellij.psi.PsiAnnotatedJavaCodeReferenceElement
import com.intellij.psi.PsiAnnotation
import com.intellij.psi.PsiAnonymousClass
import com.intellij.psi.PsiArrayType
import com.intellij.psi.PsiCapturedWildcardType
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiDisjunctionType
import com.intellij.psi.PsiEllipsisType
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiIntersectionType
import com.intellij.psi.PsiModifier
import com.intellij.psi.PsiPackage
import com.intellij.psi.PsiPrimitiveType
import com.intellij.psi.PsiSubstitutor
import com.intellij.psi.PsiType
import com.intellij.psi.PsiWildcardType
import com.intellij.psi.PsiWildcardType.EXTENDS_PREFIX
import com.intellij.psi.PsiWildcardType.SUPER_PREFIX
import com.intellij.psi.impl.PsiImplUtil
import com.intellij.psi.impl.compiled.ClsJavaCodeReferenceElementImpl
import com.intellij.psi.impl.source.PsiClassReferenceType
import com.intellij.psi.impl.source.PsiImmediateClassType
import com.intellij.psi.impl.source.PsiJavaCodeReferenceElementImpl
import com.intellij.psi.impl.source.tree.JavaSourceUtil
import com.intellij.psi.impl.source.tree.java.PsiReferenceExpressionImpl
import com.intellij.psi.util.PsiTreeUtil
import com.intellij.psi.util.PsiUtil
import com.intellij.psi.util.PsiUtilCore
import java.util.Arrays
import java.util.function.Predicate

/**
 * Type printer which can take a [PsiType] and print it to a fully canonical
 * string, in one of two formats:
 *  <li>
 *    <li> Kotlin syntax, e.g. java.lang.Object?
 *    <li> Java syntax, e.g. java.lang.@androidx.annotation.Nullable Object
 *  </li>
 *
 * The main features of this class relative to PsiType.getCanonicalText(annotated)
 * is that it can perform filtering (to remove annotations not part of the API)
 * and Kotlin style printing which cannot be done by simple replacements
 * of @Nullable->? etc since the annotations and the suffixes appear in different
 * places.
 */
class PsiTypePrinter(
    private val codebase: Codebase,
    private val filter: Predicate<Item>? = null,
    private val mapAnnotations: Boolean = false,
    private val kotlinStyleNulls: Boolean = options.outputKotlinStyleNulls,
    private val supportTypeUseAnnotations: Boolean = SUPPORT_TYPE_USE_ANNOTATIONS
) {
    // This class inlines a lot of methods from IntelliJ, but with (a) annotated=true, (b) calling local
    // getCanonicalText methods instead of instance methods, and (c) deferring annotations if kotlinStyleNulls
    // is true and instead printing it out as a suffix. Dead code paths are also removed.

    fun getAnnotatedCanonicalText(type: PsiType, elementAnnotations: List<AnnotationItem>? = null): String {
        return getCanonicalText(type, elementAnnotations)
    }

    private fun appendNullnessSuffix(
        annotations: Array<PsiAnnotation>,
        sb: StringBuilder,
        elementAnnotations: List<AnnotationItem>?
    ) {
        val nullable = getNullable(annotations, elementAnnotations)
        appendNullnessSuffix(nullable, sb) // else: non null
    }

    private fun appendNullnessSuffix(
        list: List<PsiAnnotation>?,
        buffer: StringBuilder,
        elementAnnotations: List<AnnotationItem>?

    ) {
        val nullable: Boolean? = getNullable(list, elementAnnotations)
        appendNullnessSuffix(nullable, buffer) // else: not null: no suffix
    }

    private fun appendNullnessSuffix(nullable: Boolean?, sb: StringBuilder) {
        if (nullable == true) {
            sb.append('?')
        } else if (nullable == null) {
            sb.append('!')
        }
    }

    private fun getCanonicalText(
        type: PsiType,
        elementAnnotations: List<AnnotationItem>?
    ): String {
        when (type) {
            is PsiClassReferenceType -> return getCanonicalText(type, elementAnnotations)
            is PsiPrimitiveType -> return getCanonicalText(type, elementAnnotations)
            is PsiImmediateClassType -> return getCanonicalText(type, elementAnnotations)
            is PsiEllipsisType -> return getText(
                type,
                getCanonicalText(type.componentType, null),
                "..."
            )
            is PsiArrayType -> return getCanonicalText(type, elementAnnotations)
            is PsiWildcardType -> {
                val bound = type.bound
                // Don't include ! in type bounds
                val suffix = if (bound == null) null else getCanonicalText(bound, elementAnnotations).removeSuffix("!")
                return getText(type, suffix, elementAnnotations)
            }
            is PsiCapturedWildcardType ->
                // Based on PsiCapturedWildcardType.getCanonicalText(true)
                return getCanonicalText(type.wildcard, elementAnnotations)
            is PsiDisjunctionType ->
                // Based on PsiDisjunctionType.getCanonicalText(true)
                return StringUtil.join<PsiType>(type.disjunctions, { psiType ->
                    getCanonicalText(
                        psiType,
                        elementAnnotations
                    )
                }, " | ")
            is PsiIntersectionType -> return getCanonicalText(type.conjuncts[0], elementAnnotations)
            else -> return type.getCanonicalText(true)
        }
    }

    // From PsiWildcardType.getText, with qualified always true
    private fun getText(
        type: PsiWildcardType,
        suffix: String?,
        elementAnnotations: List<AnnotationItem>?
    ): String {
        val annotations = type.annotations
        if (annotations.isEmpty() && suffix == null) return "?"

        val sb = StringBuilder()
        appendAnnotations(sb, annotations, elementAnnotations)
        if (suffix == null) {
            sb.append('?')
        } else {
            if (suffix == JAVA_LANG_OBJECT &&
                !compatibility.includeExtendsObjectInWildcard &&
                type.isExtends
            ) {
                sb.append('?')
            } else {
                sb.append(if (type.isExtends) EXTENDS_PREFIX else SUPER_PREFIX)
                sb.append(suffix)
            }
        }
        return sb.toString()
    }

    // From PsiEllipsisType.getText, with qualified always true
    private fun getText(
        type: PsiEllipsisType,
        prefix: String,
        suffix: String
    ): String {
        val sb = StringBuilder(prefix.length + suffix.length)
        sb.append(prefix)
        val annotations = type.annotations
        if (annotations.isNotEmpty()) {
            appendAnnotations(sb, annotations, null)
        }
        sb.append(suffix)

        // No kotlin style suffix here: vararg parameters aren't nullable

        return sb.toString()
    }

    // From PsiArrayType.getCanonicalText(true))
    private fun getCanonicalText(type: PsiArrayType, elementAnnotations: List<AnnotationItem>?): String {
        return getText(type, getCanonicalText(type.componentType, null), "[]", elementAnnotations)
    }

    // From PsiArrayType.getText(String,String,boolean,boolean), with qualified = true
    private fun getText(
        type: PsiArrayType,
        prefix: String,
        suffix: String,
        elementAnnotations: List<AnnotationItem>?
    ): String {
        val sb = StringBuilder(prefix.length + suffix.length)
        sb.append(prefix)
        val annotations = type.annotations

        if (annotations.isNotEmpty()) {
            val originalLength = sb.length
            sb.append(' ')
            appendAnnotations(sb, annotations, elementAnnotations)
            if (sb.length == originalLength + 1) {
                // Didn't emit any annotations (e.g. skipped because only null annotations and replacing with ?)
                sb.setLength(originalLength)
            }
        }
        sb.append(suffix)

        if (kotlinStyleNulls) {
            appendNullnessSuffix(annotations, sb, elementAnnotations)
        }

        return sb.toString()
    }

    // Copied from PsiPrimitiveType.getCanonicalText(true))
    private fun getCanonicalText(type: PsiPrimitiveType, elementAnnotations: List<AnnotationItem>?): String {
        return getText(type, elementAnnotations)
    }

    // Copied from PsiPrimitiveType.getText(boolean, boolean), with annotated = true and qualified = true
    private fun getText(
        type: PsiPrimitiveType,
        elementAnnotations: List<AnnotationItem>?
    ): String {
        val annotations = type.annotations
        if (annotations.isEmpty()) return type.name

        val sb = StringBuilder()
        appendAnnotations(sb, annotations, elementAnnotations)
        sb.append(type.name)
        return sb.toString()
    }

    private fun getCanonicalText(type: PsiClassReferenceType, elementAnnotations: List<AnnotationItem>?): String {
        val reference = type.reference
        if (reference is PsiAnnotatedJavaCodeReferenceElement) {
            val annotations = type.annotations

            when (reference) {
                is ClsJavaCodeReferenceElementImpl -> {
                    // From ClsJavaCodeReferenceElementImpl.getCanonicalText(boolean PsiAnnotation[])
                    val text = reference.getCanonicalText()

                    val sb = StringBuilder()

                    val prefix = getOuterClassRef(text)
                    var tailStart = 0
                    if (!StringUtil.isEmpty(prefix)) {
                        sb.append(prefix).append('.')
                        tailStart = prefix.length + 1
                    }

                    appendAnnotations(sb, Arrays.asList(*annotations), elementAnnotations)

                    sb.append(text, tailStart, text.length)

                    if (kotlinStyleNulls) {
                        appendNullnessSuffix(annotations, sb, elementAnnotations)
                    }

                    return sb.toString()
                }
                is PsiJavaCodeReferenceElementImpl -> return getCanonicalText(
                    reference,
                    annotations,
                    reference.containingFile,
                    elementAnnotations,
                    kotlinStyleNulls
                )
                else -> // Unexpected implementation: fallback
                    return reference.getCanonicalText(true, if (annotations.isEmpty()) null else annotations)
            }
        }
        return reference.canonicalText
    }

    // From PsiJavaCodeReferenceElementImpl.getCanonicalText(bool PsiAnnotation[], PsiFile)
    private fun getCanonicalText(
        reference: PsiJavaCodeReferenceElementImpl,
        annotations: Array<PsiAnnotation>?,
        containingFile: PsiFile,
        elementAnnotations: List<AnnotationItem>?,
        allowKotlinSuffix: Boolean
    ): String {
        var remaining = annotations
        val kind = reference.getKindEnum(containingFile)
        when (kind) {
            PsiJavaCodeReferenceElementImpl.Kind.CLASS_NAME_KIND,
            PsiJavaCodeReferenceElementImpl.Kind.CLASS_OR_PACKAGE_NAME_KIND,
            PsiJavaCodeReferenceElementImpl.Kind.CLASS_IN_QUALIFIED_NEW_KIND -> {
                val results = PsiImplUtil.multiResolveImpl(
                    containingFile.project,
                    containingFile,
                    reference,
                    false,
                    PsiReferenceExpressionImpl.OurGenericsResolver.INSTANCE
                )
                val target = if (results.size == 1) results[0].element else null
                when (target) {
                    is PsiClass -> {
                        val buffer = StringBuilder()
                        val qualifier = reference.qualifier
                        var prefix: String? = null
                        if (qualifier is PsiJavaCodeReferenceElementImpl) {
                            prefix = getCanonicalText(
                                qualifier,
                                remaining,
                                containingFile,
                                null,
                                false
                            )
                            remaining = null
                        } else {
                            val fqn = target.qualifiedName
                            if (fqn != null) {
                                prefix = StringUtil.getPackageName(fqn)
                            }
                        }

                        if (!StringUtil.isEmpty(prefix)) {
                            buffer.append(prefix)
                            buffer.append('.')
                        }

                        val list = if (remaining != null) Arrays.asList(*remaining) else getAnnotations(reference)
                        appendAnnotations(buffer, list, elementAnnotations)

                        buffer.append(target.name)

                        appendTypeArgs(
                            buffer,
                            reference.typeParameters,
                            null
                        )

                        if (allowKotlinSuffix && kotlinStyleNulls) {
                            appendNullnessSuffix(list, buffer, elementAnnotations)
                        }

                        return buffer.toString()
                    }
                    is PsiPackage -> return target.qualifiedName
                    else -> return JavaSourceUtil.getReferenceText(reference)
                }
            }

            PsiJavaCodeReferenceElementImpl.Kind.PACKAGE_NAME_KIND,
            PsiJavaCodeReferenceElementImpl.Kind.CLASS_FQ_NAME_KIND,
            PsiJavaCodeReferenceElementImpl.Kind.CLASS_FQ_OR_PACKAGE_NAME_KIND ->
                return JavaSourceUtil.getReferenceText(reference)

            else -> {
                error("Unexpected kind $kind")
            }
        }
    }

    private fun getNullable(list: List<PsiAnnotation>?, elementAnnotations: List<AnnotationItem>?): Boolean? {
        if (elementAnnotations != null) {
            for (annotation in elementAnnotations) {
                if (annotation.isNullable()) {
                    return true
                } else if (annotation.isNonNull()) {
                    return false
                }
            }
        }

        list ?: return null

        for (annotation in list) {
            val name = annotation.qualifiedName ?: continue
            if (isNullableAnnotation(name)) {
                return true
            } else if (isNonNullAnnotation(name)) {
                return false
            }
        }

        return null
    }

    private fun getNullable(list: Array<PsiAnnotation>?, elementAnnotations: List<AnnotationItem>?): Boolean? {
        if (elementAnnotations != null) {
            for (annotation in elementAnnotations) {
                if (annotation.isNullable()) {
                    return true
                } else if (annotation.isNonNull()) {
                    return false
                }
            }
        }

        list ?: return null

        for (annotation in list) {
            val name = annotation.qualifiedName ?: continue
            if (isNullableAnnotation(name)) {
                return true
            } else if (isNonNullAnnotation(name)) {
                return false
            }
        }

        return null
    }

    // From PsiNameHelper.appendTypeArgs, but with annotated = true and canonical = true
    private fun appendTypeArgs(
        sb: StringBuilder,
        types: Array<PsiType>,
        elementAnnotations: List<AnnotationItem>?
    ) {
        if (types.isEmpty()) return

        sb.append('<')
        for (i in types.indices) {
            if (i > 0) {
                sb.append(if (!compatibility.spaceAfterCommaInTypes) "," else ", ")
            }

            val type = types[i]
            sb.append(getCanonicalText(type, elementAnnotations))
        }
        sb.append('>')
    }

    // From PsiJavaCodeReferenceElementImpl.getAnnotations()
    private fun getAnnotations(reference: PsiJavaCodeReferenceElementImpl): List<PsiAnnotation> {
        val annotations = PsiTreeUtil.getChildrenOfTypeAsList(reference, PsiAnnotation::class.java)

        if (!reference.isQualified) {
            val modifierList = PsiImplUtil.findNeighbourModifierList(reference)
            if (modifierList != null) {
                PsiImplUtil.collectTypeUseAnnotations(modifierList, annotations)
            }
        }

        return annotations
    }

    // From ClsJavaCodeReferenceElementImpl
    private fun getOuterClassRef(ref: String): String {
        var stack = 0
        for (i in ref.length - 1 downTo 0) {
            val c = ref[i]
            when (c) {
                '<' -> stack--
                '>' -> stack++
                '.' -> if (stack == 0) return ref.substring(0, i)
            }
        }

        return ""
    }

    // From PsiNameHelper.appendAnnotations

    private fun appendAnnotations(
        sb: StringBuilder,
        annotations: Array<PsiAnnotation>,
        elementAnnotations: List<AnnotationItem>?
    ): Boolean {
        return appendAnnotations(sb, Arrays.asList(*annotations), elementAnnotations)
    }

    private fun mapAnnotation(qualifiedName: String?): String? {
        qualifiedName ?: return null
        if (kotlinStyleNulls && (isNullableAnnotation(qualifiedName) || isNonNullAnnotation(qualifiedName))) {
            return null
        }
        if (!supportTypeUseAnnotations) {
            return null
        }

        val mapped =
            if (mapAnnotations) {
                AnnotationItem.mapName(codebase, qualifiedName) ?: return null
            } else {
                qualifiedName
            }

        if (filter != null) {
            val item = codebase.findClass(mapped)
            if (item == null || !filter.test(item)) {
                return null
            }
        }

        return mapped
    }

    // From PsiNameHelper.appendAnnotations, with deltas to optionally map names

    private fun appendAnnotations(
        sb: StringBuilder,
        annotations: List<PsiAnnotation>,
        elementAnnotations: List<AnnotationItem>?
    ): Boolean {
        var updated = false
        for (annotation in annotations) {
            val name = mapAnnotation(annotation.qualifiedName)
            if (name != null) {
                sb.append('@').append(name).append(annotation.parameterList.text).append(' ')
                updated = true
            }
        }

        if (elementAnnotations != null) {
            for (annotation in elementAnnotations) {
                val name = mapAnnotation(annotation.qualifiedName())
                if (name != null) {
                    sb.append(annotation.toSource()).append(' ')
                    updated = true
                }
            }
        }
        return updated
    }

    // From PsiImmediateClassType

    private fun getCanonicalText(type: PsiImmediateClassType, elementAnnotations: List<AnnotationItem>?): String {
        return getText(type, elementAnnotations)
    }

    private fun getText(
        type: PsiImmediateClassType,
        elementAnnotations: List<AnnotationItem>?
    ): String {
        val cls = type.resolve() ?: return ""
        val buffer = StringBuilder()
        buildText(type, cls, PsiSubstitutor.EMPTY, buffer, elementAnnotations)
        return buffer.toString()
    }

    private fun buildText(
        type: PsiImmediateClassType,
        aClass: PsiClass,
        substitutor: PsiSubstitutor,
        buffer: StringBuilder,
        elementAnnotations: List<AnnotationItem>?
    ) {
        if (aClass is PsiAnonymousClass) {
            val baseResolveResult = aClass.baseClassType.resolveGenerics()
            val baseClass = baseResolveResult.element
            if (baseClass != null) {
                buildText(type, baseClass, baseResolveResult.substitutor, buffer, null)
            } else {
                buffer.append(aClass.baseClassReference.canonicalText)
            }
            return
        }

        var enclosingClass: PsiClass? = null
        if (!aClass.hasModifierProperty(PsiModifier.STATIC)) {
            val parent = aClass.parent
            if (parent is PsiClass && parent !is PsiAnonymousClass) {
                enclosingClass = parent
            }
        }
        if (enclosingClass != null) {
            buildText(type, enclosingClass, substitutor, buffer, null)
            buffer.append('.')
        } else {
            val fqn = aClass.qualifiedName
            if (fqn != null) {
                val prefix = StringUtil.getPackageName(fqn)
                if (!StringUtil.isEmpty(prefix)) {
                    buffer.append(prefix)
                    buffer.append('.')
                }
            }
        }

        val annotations = type.annotations
        appendAnnotations(buffer, annotations, elementAnnotations)

        buffer.append(aClass.name)

        val typeParameters = aClass.typeParameters
        if (typeParameters.isNotEmpty()) {
            var pos = buffer.length
            buffer.append('<')

            for (i in typeParameters.indices) {
                val typeParameter = typeParameters[i]
                PsiUtilCore.ensureValid(typeParameter)

                if (i > 0) {
                    buffer.append(',')
                }

                val substitutionResult = substitutor.substitute(typeParameter)
                if (substitutionResult == null) {
                    buffer.setLength(pos)
                    pos = -1
                    break
                }
                PsiUtil.ensureValidType(substitutionResult)

                buffer.append(getCanonicalText(substitutionResult, null)) // not passing in merge annotations here
            }

            if (pos >= 0) {
                buffer.append('>')
            }
        }

        if (kotlinStyleNulls) {
            appendNullnessSuffix(annotations, buffer, elementAnnotations)
        }
    }
}