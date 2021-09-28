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

import com.android.tools.lint.detector.api.getInternalName
import com.android.tools.metalava.compatibility
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MemberItem
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.TypeItem
import com.android.tools.metalava.model.TypeParameterItem
import com.intellij.psi.JavaTokenType
import com.intellij.psi.PsiArrayType
import com.intellij.psi.PsiCapturedWildcardType
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiClassType
import com.intellij.psi.PsiCompiledElement
import com.intellij.psi.PsiDisjunctionType
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiEllipsisType
import com.intellij.psi.PsiIntersectionType
import com.intellij.psi.PsiJavaCodeReferenceElement
import com.intellij.psi.PsiJavaToken
import com.intellij.psi.PsiLambdaExpressionType
import com.intellij.psi.PsiPrimitiveType
import com.intellij.psi.PsiRecursiveElementVisitor
import com.intellij.psi.PsiReferenceList
import com.intellij.psi.PsiType
import com.intellij.psi.PsiTypeElement
import com.intellij.psi.PsiTypeParameter
import com.intellij.psi.PsiTypeParameterList
import com.intellij.psi.PsiTypeVisitor
import com.intellij.psi.PsiWhiteSpace
import com.intellij.psi.PsiWildcardType
import com.intellij.psi.util.PsiTypesUtil
import com.intellij.psi.util.TypeConversionUtil
import org.jetbrains.kotlin.asJava.elements.KtLightTypeParameter
import java.util.function.Predicate

/** Represents a type backed by PSI */
class PsiTypeItem private constructor(
    private val codebase: PsiBasedCodebase,
    var psiType: PsiType
) : TypeItem {
    private var toString: String? = null
    private var toAnnotatedString: String? = null
    private var toInnerAnnotatedString: String? = null
    private var toErasedString: String? = null
    private var asClass: PsiClassItem? = null

    override fun toString(): String {
        return toTypeString()
    }

    override fun toTypeString(
        outerAnnotations: Boolean,
        innerAnnotations: Boolean,
        erased: Boolean,
        kotlinStyleNulls: Boolean,
        context: Item?,
        filter: Predicate<Item>?
    ): String {
        assert(innerAnnotations || !outerAnnotations) // Can't supply outer=true,inner=false

        if (filter != null) {
            // No caching when specifying filter.
            // TODO: When we support type use annotations, here we need to deal with markRecent
            //  and clearAnnotations not really having done their job.
            return toTypeString(
                codebase = codebase,
                type = psiType,
                outerAnnotations = outerAnnotations,
                innerAnnotations = innerAnnotations,
                erased = erased,
                kotlinStyleNulls = kotlinStyleNulls,
                context = context,
                filter = filter
            )
        }

        return if (erased) {
            if (kotlinStyleNulls && (innerAnnotations || outerAnnotations)) {
                // Not cached: Not common
                toTypeString(
                    codebase = codebase,
                    type = psiType,
                    outerAnnotations = outerAnnotations,
                    innerAnnotations = innerAnnotations,
                    erased = erased,
                    kotlinStyleNulls = kotlinStyleNulls,
                    context = context,
                    filter = filter
                )
            } else {
                if (toErasedString == null) {
                    toErasedString = toTypeString(
                        codebase = codebase,
                        type = psiType,
                        outerAnnotations = outerAnnotations,
                        innerAnnotations = innerAnnotations,
                        erased = erased,
                        kotlinStyleNulls = kotlinStyleNulls,
                        context = context,
                        filter = filter
                    )
                }
                toErasedString!!
            }
        } else {
            when {
                kotlinStyleNulls && outerAnnotations -> {
                    if (toAnnotatedString == null) {
                        toAnnotatedString = toTypeString(
                            codebase = codebase,
                            type = psiType,
                            outerAnnotations = outerAnnotations,
                            innerAnnotations = innerAnnotations,
                            erased = erased,
                            kotlinStyleNulls = kotlinStyleNulls,
                            context = context,
                            filter = filter
                        )
                    }
                    toAnnotatedString!!
                }
                kotlinStyleNulls && innerAnnotations -> {
                    if (toInnerAnnotatedString == null) {
                        toInnerAnnotatedString = toTypeString(
                            codebase = codebase,
                            type = psiType,
                            outerAnnotations = outerAnnotations,
                            innerAnnotations = innerAnnotations,
                            erased = erased,
                            kotlinStyleNulls = kotlinStyleNulls,
                            context = context,
                            filter = filter
                        )
                    }
                    toInnerAnnotatedString!!
                }
                else -> {
                    if (toString == null) {
                        toString = TypeItem.formatType(
                            getCanonicalText(
                                codebase = codebase,
                                owner = context,
                                type = psiType,
                                annotated = false,
                                mapAnnotations = false,
                                kotlinStyleNulls = kotlinStyleNulls,
                                filter = filter
                            )
                        )
                    }
                    toString!!
                }
            }
        }
    }

    override fun toErasedTypeString(context: Item?): String {
        return toTypeString(
            outerAnnotations = false,
            innerAnnotations = false,
            erased = true,
            kotlinStyleNulls = false,
            context = context
        )
    }

    override fun arrayDimensions(): Int {
        return psiType.arrayDimensions
    }

    override fun internalName(): String {
        if (primitive) {
            val signature = getPrimitiveSignature(toString())
            if (signature != null) {
                return signature
            }
        }
        val sb = StringBuilder()
        appendJvmSignature(sb, psiType)
        return sb.toString()
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true

        return when (other) {
            is TypeItem -> TypeItem.equalsWithoutSpace(toTypeString(), other.toTypeString())
            else -> false
        }
    }

    override fun asClass(): PsiClassItem? {
        if (primitive) {
            return null
        }
        if (asClass == null) {
            asClass = codebase.findClass(psiType)
        }
        return asClass
    }

    override fun asTypeParameter(context: MemberItem?): TypeParameterItem? {
        val cls = asClass() ?: return null
        return cls as? PsiTypeParameterItem
    }

    override fun hashCode(): Int {
        return psiType.hashCode()
    }

    override val primitive: Boolean
        get() = psiType is PsiPrimitiveType

    override fun defaultValue(): Any? {
        return PsiTypesUtil.getDefaultValue(psiType)
    }

    override fun defaultValueString(): String {
        return PsiTypesUtil.getDefaultValueOfType(psiType)
    }

    override fun typeArgumentClasses(): List<ClassItem> {
        if (primitive) {
            return emptyList()
        }

        val classes = mutableListOf<ClassItem>()
        psiType.accept(object : PsiTypeVisitor<PsiType>() {
            override fun visitType(type: PsiType?): PsiType? {
                return type
            }

            override fun visitClassType(classType: PsiClassType): PsiType? {
                codebase.findClass(classType)?.let {
                    if (!it.isTypeParameter && !classes.contains(it)) {
                        classes.add(it)
                    }
                }
                for (type in classType.parameters) {
                    type.accept(this)
                }
                return classType
            }

            override fun visitWildcardType(wildcardType: PsiWildcardType): PsiType? {
                if (wildcardType.isExtends) {
                    wildcardType.extendsBound.accept(this)
                }
                if (wildcardType.isSuper) {
                    wildcardType.superBound.accept(this)
                }
                if (wildcardType.isBounded) {
                    wildcardType.bound?.accept(this)
                }
                return wildcardType
            }

            override fun visitPrimitiveType(primitiveType: PsiPrimitiveType): PsiType? {
                return primitiveType
            }

            override fun visitEllipsisType(ellipsisType: PsiEllipsisType): PsiType? {
                ellipsisType.componentType.accept(this)
                return ellipsisType
            }

            override fun visitArrayType(arrayType: PsiArrayType): PsiType? {
                arrayType.componentType.accept(this)
                return arrayType
            }

            override fun visitLambdaExpressionType(lambdaExpressionType: PsiLambdaExpressionType): PsiType? {
                for (superType in lambdaExpressionType.superTypes) {
                    superType.accept(this)
                }
                return lambdaExpressionType
            }

            override fun visitCapturedWildcardType(capturedWildcardType: PsiCapturedWildcardType): PsiType? {
                capturedWildcardType.upperBound.accept(this)
                return capturedWildcardType
            }

            override fun visitDisjunctionType(disjunctionType: PsiDisjunctionType): PsiType? {
                for (type in disjunctionType.disjunctions) {
                    type.accept(this)
                }
                return disjunctionType
            }

            override fun visitIntersectionType(intersectionType: PsiIntersectionType): PsiType? {
                for (type in intersectionType.conjuncts) {
                    type.accept(this)
                }
                return intersectionType
            }
        })

        return classes
    }

    override fun convertType(replacementMap: Map<String, String>?, owner: Item?): TypeItem {
        val s = convertTypeString(replacementMap)
        return create(codebase, codebase.createPsiType(s, owner?.psi()))
    }

    override fun hasTypeArguments(): Boolean {
        val type = psiType
        return type is PsiClassType && type.hasParameters()
    }

    override fun markRecent() {
        val source = toTypeString(outerAnnotations = true, innerAnnotations = true)
            .replace(".NonNull", ".RecentlyNonNull")
        // TODO: Pass in a context!
        psiType = codebase.createPsiType(source)
        toAnnotatedString = null
        toInnerAnnotatedString = null
    }

    override fun scrubAnnotations() {
        toAnnotatedString = toTypeString(outerAnnotations = false, innerAnnotations = false)
        toInnerAnnotatedString = toAnnotatedString
    }

    companion object {
        private fun getPrimitiveSignature(typeName: String): String? = when (typeName) {
            "boolean" -> "Z"
            "byte" -> "B"
            "char" -> "C"
            "short" -> "S"
            "int" -> "I"
            "long" -> "J"
            "float" -> "F"
            "double" -> "D"
            "void" -> "V"
            else -> null
        }

        private fun appendJvmSignature(
            buffer: StringBuilder,
            type: PsiType?
        ): Boolean {
            if (type == null) {
                return false
            }

            val psiType = TypeConversionUtil.erasure(type)

            when (psiType) {
                is PsiArrayType -> {
                    buffer.append('[')
                    appendJvmSignature(buffer, psiType.componentType)
                }
                is PsiClassType -> {
                    val resolved = psiType.resolve() ?: return false
                    if (!appendJvmTypeName(buffer, resolved)) {
                        return false
                    }
                }
                is PsiPrimitiveType -> buffer.append(getPrimitiveSignature(psiType.canonicalText))
                else -> return false
            }
            return true
        }

        private fun appendJvmTypeName(
            signature: StringBuilder,
            outerClass: PsiClass
        ): Boolean {
            val className = getInternalName(outerClass) ?: return false
            signature.append('L').append(className).append(';')
            return true
        }

        fun toTypeString(
            codebase: PsiBasedCodebase,
            type: PsiType,
            outerAnnotations: Boolean,
            innerAnnotations: Boolean,
            erased: Boolean,
            kotlinStyleNulls: Boolean,
            context: Item?,
            filter: Predicate<Item>?
        ): String {
            if (erased) {
                // Recurse with raw type and erase=false
                return toTypeString(
                    codebase,
                    TypeConversionUtil.erasure(type),
                    outerAnnotations,
                    innerAnnotations,
                    false,
                    kotlinStyleNulls,
                    context,
                    filter
                )
            }

            val typeString =
                if (kotlinStyleNulls && (innerAnnotations || outerAnnotations)) {
                    try {
                        getCanonicalText(codebase, context, type, true, true, kotlinStyleNulls, filter)
                    } catch (ignore: Throwable) {
                        type.canonicalText
                    }
                } else {
                    type.canonicalText
                }

            return TypeItem.formatType(typeString)
        }

        private fun getCanonicalText(
            codebase: PsiBasedCodebase,
            owner: Item?,
            type: PsiType,
            annotated: Boolean,
            mapAnnotations: Boolean,
            kotlinStyleNulls: Boolean,
            filter: Predicate<Item>?
        ): String {
            return try {
                if (annotated && kotlinStyleNulls) {
                    // Any nullness annotations on the element to merge in? When we have something like
                    //  @Nullable String foo
                    // the Nullable annotation can be on the element itself rather than the type,
                    // so if we print the type without knowing the nullness annotation on the
                    // element, we'll think it's unannotated and we'll display it as "String!".
                    val nullness = owner?.modifiers?.annotations()?.firstOrNull { it.isNullnessAnnotation() }
                    var elementAnnotations = if (nullness != null) { listOf(nullness) } else null

                    val implicitNullness = if (owner != null) AnnotationItem.getImplicitNullness(owner) else null
                    val annotatedType = if (implicitNullness != null) {
                        val provider = if (implicitNullness == true) {
                            codebase.getNullableAnnotationProvider()
                        } else {
                            codebase.getNonNullAnnotationProvider()
                        }

                        if (implicitNullness == false &&
                            owner is MethodItem &&
                            owner.containingClass().isAnnotationType() &&
                            type is PsiArrayType
                        ) {
                            // For arrays in annotations not only is the method itself non null but so
                            // is the component type
                            type.componentType.annotate(provider).createArrayType().annotate(provider)
                        } else {
                            type.annotate(provider)
                        }
                    } else if (nullness != null && owner.modifiers.isVarArg() && owner.isKotlin() && type is PsiEllipsisType) {
                        // Varargs the annotation applies to the component type instead
                        val nonNullProvider = codebase.getNonNullAnnotationProvider()
                        val provider = if (nullness.isNonNull()) {
                            nonNullProvider
                        } else codebase.getNullableAnnotationProvider()
                        val componentType = type.componentType.annotate(provider)
                        elementAnnotations = null
                        PsiEllipsisType(componentType, nonNullProvider)
                    } else {
                        type
                    }
                    val printer = PsiTypePrinter(codebase, filter, mapAnnotations, kotlinStyleNulls)

                    printer.getAnnotatedCanonicalText(
                        annotatedType,
                        elementAnnotations
                    )
                } else if (annotated) {
                    type.getCanonicalText(true)
                } else {
                    type.getCanonicalText(false)
                }
            } catch (e: Throwable) {
                return type.getCanonicalText(false)
            }
        }

        fun create(codebase: PsiBasedCodebase, psiType: PsiType): PsiTypeItem {
            return PsiTypeItem(codebase, psiType)
        }

        fun create(codebase: PsiBasedCodebase, original: PsiTypeItem): PsiTypeItem {
            return PsiTypeItem(codebase, original.psiType)
        }

        fun typeParameterList(typeList: PsiTypeParameterList?): String? {
            if (typeList != null && typeList.typeParameters.isNotEmpty()) {
                // TODO: Filter the type list classes? Try to construct a typelist of a private API!
                // We can't just use typeList.text here, because that just
                // uses the declaration from the source, which may not be
                // fully qualified - e.g. we might get
                //    <T extends View> instead of <T extends android.view.View>
                // Therefore, we'll need to compute it ourselves; I can't find
                // a utility for this
                val sb = StringBuilder()
                typeList.accept(object : PsiRecursiveElementVisitor() {
                    override fun visitElement(element: PsiElement) {
                        if (element is PsiTypeParameterList) {
                            val typeParameters = element.typeParameters
                            if (typeParameters.isEmpty()) {
                                return
                            }
                            sb.append("<")
                            var first = true
                            for (parameter in typeParameters) {
                                if (!first) {
                                    sb.append(", ")
                                }
                                first = false
                                visitElement(parameter)
                            }
                            sb.append(">")
                            return
                        } else if (element is PsiTypeParameter) {
                            if (element is KtLightTypeParameter && element.kotlinOrigin.text.startsWith("reified")) {
                                sb.append("reified ")
                            }
                            sb.append(element.name)
                            // TODO: How do I get super -- e.g. "Comparable<? super T>"
                            val extendsList = element.extendsList
                            val refList = extendsList.referenceElements
                            if (refList.isNotEmpty()) {
                                sb.append(" extends ")
                                var first = true
                                for (refElement in refList) {
                                    if (!first) {
                                        sb.append(" & ")
                                    } else {
                                        first = false
                                    }

                                    if (refElement is PsiJavaCodeReferenceElement) {
                                        visitElement(refElement)
                                        continue
                                    }
                                    val resolved = refElement.resolve()
                                    if (resolved is PsiClass) {
                                        sb.append(resolved.qualifiedName ?: resolved.name)
                                        resolved.typeParameterList?.accept(this)
                                    } else {
                                        sb.append(refElement.referenceName)
                                    }
                                }
                            } else {
                                val extendsListTypes = element.extendsListTypes
                                if (extendsListTypes.isNotEmpty()) {
                                    sb.append(" extends ")
                                    var first = true
                                    for (type in extendsListTypes) {
                                        if (!first) {
                                            sb.append(" & ")
                                        } else {
                                            first = false
                                        }
                                        val resolved = type.resolve()
                                        if (resolved == null) {
                                            sb.append(type.className)
                                        } else {
                                            sb.append(resolved.qualifiedName ?: resolved.name)
                                            resolved.typeParameterList?.accept(this)
                                        }
                                    }
                                }
                            }
                            return
                        } else if (element is PsiJavaCodeReferenceElement) {
                            val resolved = element.resolve()
                            if (resolved is PsiClass) {
                                if (resolved.qualifiedName == null) {
                                    sb.append(resolved.name)
                                } else {
                                    sb.append(resolved.qualifiedName)
                                }
                                val typeParameters = element.parameterList
                                if (typeParameters != null) {
                                    val typeParameterElements = typeParameters.typeParameterElements
                                    if (typeParameterElements.isEmpty()) {
                                        return
                                    }

                                    // When reading in this from bytecode, the order is sometimes wrong
                                    // (for example, for
                                    //    public interface BaseStream<T, S extends BaseStream<T, S>>
                                    // the extends type BaseStream<T, S> will return the typeParameterElements
                                    // as [S,T] instead of [T,S]. However, the typeParameters.typeArguments
                                    // list is correct, so order the elements by the typeArguments array instead

                                    // Special case: just one type argument: no sorting issue
                                    if (typeParameterElements.size == 1) {
                                        sb.append("<")
                                        var first = true
                                        for (parameter in typeParameterElements) {
                                            if (!first) {
                                                sb.append(", ")
                                            }
                                            first = false
                                            visitElement(parameter)
                                        }
                                        sb.append(">")
                                        return
                                    }

                                    // More than one type argument

                                    val typeArguments = typeParameters.typeArguments
                                    if (typeArguments.isNotEmpty()) {
                                        sb.append("<")
                                        var first = true
                                        for (parameter in typeArguments) {
                                            if (!first) {
                                                sb.append(", ")
                                            }
                                            first = false
                                            // Try to match up a type parameter element
                                            var found = false
                                            for (typeElement in typeParameterElements) {
                                                if (parameter == typeElement.type) {
                                                    found = true
                                                    visitElement(typeElement)
                                                    break
                                                }
                                            }
                                            if (!found) {
                                                // No type element matched: use type instead
                                                val classType = PsiTypesUtil.getPsiClass(parameter)
                                                if (classType != null) {
                                                    visitElement(classType)
                                                } else {
                                                    sb.append(parameter.canonicalText)
                                                }
                                            }
                                        }
                                        sb.append(">")
                                    }
                                }
                                return
                            }
                        } else if (element is PsiTypeElement) {
                            val type = element.type
                            if (type is PsiWildcardType) {
                                sb.append("?")
                                if (type.isBounded) {
                                    if (type.isExtends) {
                                        sb.append(" extends ")
                                        sb.append(type.extendsBound.canonicalText)
                                    }
                                    if (type.isSuper) {
                                        sb.append(" super ")
                                        sb.append(type.superBound.canonicalText)
                                    }
                                }
                                return
                            }
                            sb.append(type.canonicalText)
                            return
                        } else if (element is PsiJavaToken && element.tokenType == JavaTokenType.COMMA) {
                            sb.append(",")
                            if (compatibility.spaceAfterCommaInTypes) {
                                if (element.nextSibling == null || element.nextSibling !is PsiWhiteSpace) {
                                    sb.append(" ")
                                }
                            }
                            return
                        }
                        if (element.firstChild == null) { // leaf nodes only
                            if (element is PsiCompiledElement) {
                                if (element is PsiReferenceList) {
                                    val referencedTypes = element.referencedTypes
                                    var first = true
                                    for (referenceType in referencedTypes) {
                                        if (first) {
                                            first = false
                                        } else {
                                            sb.append(", ")
                                        }
                                        sb.append(referenceType.canonicalText)
                                    }
                                }
                            } else {
                                sb.append(element.text)
                            }
                        }
                        super.visitElement(element)
                    }
                })

                val typeString = sb.toString()
                return TypeItem.cleanupGenerics(typeString)
            }

            return null
        }

        fun typeParameterClasses(codebase: PsiBasedCodebase, typeList: PsiTypeParameterList?): List<ClassItem> {
            if (typeList != null && typeList.typeParameters.isNotEmpty()) {
                val list = mutableListOf<ClassItem>()
                typeList.accept(object : PsiRecursiveElementVisitor() {
                    override fun visitElement(element: PsiElement) {
                        if (element is PsiTypeParameterList) {
                            val typeParameters = element.typeParameters
                            for (parameter in typeParameters) {
                                visitElement(parameter)
                            }
                            return
                        } else if (element is PsiTypeParameter) {
                            val extendsList = element.extendsList
                            val refList = extendsList.referenceElements
                            if (refList.isNotEmpty()) {
                                for (refElement in refList) {
                                    if (refElement is PsiJavaCodeReferenceElement) {
                                        visitElement(refElement)
                                        continue
                                    }
                                    val resolved = refElement.resolve()
                                    if (resolved is PsiClass) {
                                        addRealClass(
                                            list,
                                            codebase.findOrCreateClass(resolved)
                                        )
                                        resolved.typeParameterList?.accept(this)
                                    }
                                }
                            } else {
                                val extendsListTypes = element.extendsListTypes
                                if (extendsListTypes.isNotEmpty()) {
                                    for (type in extendsListTypes) {
                                        val resolved = type.resolve()
                                        if (resolved != null) {
                                            addRealClass(
                                                list, codebase.findOrCreateClass(resolved)
                                            )
                                            resolved.typeParameterList?.accept(this)
                                        }
                                    }
                                }
                            }
                            return
                        } else if (element is PsiJavaCodeReferenceElement) {
                            val resolved = element.resolve()
                            if (resolved is PsiClass) {
                                addRealClass(
                                    list,
                                    codebase.findOrCreateClass(resolved)
                                )
                                element.parameterList?.accept(this)
                                return
                            }
                        } else if (element is PsiTypeElement) {
                            val type = element.type
                            if (type is PsiWildcardType) {
                                if (type.isBounded) {
                                    addRealClass(
                                        codebase,
                                        list, type.bound
                                    )
                                }
                                if (type.isExtends) {
                                    addRealClass(
                                        codebase,
                                        list, type.extendsBound
                                    )
                                }
                                if (type.isSuper) {
                                    addRealClass(
                                        codebase,
                                        list, type.superBound
                                    )
                                }
                                return
                            }
                            return
                        }
                        super.visitElement(element)
                    }
                })

                return list
            } else {
                return emptyList()
            }
        }

        private fun addRealClass(codebase: PsiBasedCodebase, classes: MutableList<ClassItem>, type: PsiType?) {
            codebase.findClass(type ?: return)?.let {
                addRealClass(classes, it)
            }
        }

        private fun addRealClass(classes: MutableList<ClassItem>, cls: ClassItem) {
            if (!cls.isTypeParameter && !classes.contains(cls)) { // typically small number of items, don't need Set
                classes.add(cls)
            }
        }
    }
}
