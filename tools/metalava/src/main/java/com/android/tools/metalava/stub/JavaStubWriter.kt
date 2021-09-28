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

package com.android.tools.metalava.stub

import com.android.tools.metalava.JAVA_LANG_STRING
import com.android.tools.metalava.compatibility
import com.android.tools.metalava.model.AnnotationTarget
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.ConstructorItem
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MemberItem
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.ModifierList
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.model.TypeParameterList
import com.android.tools.metalava.model.psi.EXPAND_DOCUMENTATION
import com.android.tools.metalava.model.psi.PsiClassItem
import com.android.tools.metalava.model.psi.trimDocIndent
import com.android.tools.metalava.model.visitors.ItemVisitor
import com.android.tools.metalava.options
import java.io.PrintWriter
import java.util.function.Predicate

class JavaStubWriter(
    private val writer: PrintWriter,
    private val filterEmit: Predicate<Item>,
    private val filterReference: Predicate<Item>,
    private val generateAnnotations: Boolean = false,
    private val preFiltered: Boolean = true,
    private val docStubs: Boolean
) : ItemVisitor() {
    private val annotationTarget = if (docStubs) AnnotationTarget.DOC_STUBS_FILE else AnnotationTarget.SDK_STUBS_FILE

    override fun visitClass(cls: ClassItem) {
        if (cls.isTopLevelClass()) {
            val qualifiedName = cls.containingPackage().qualifiedName()
            if (qualifiedName.isNotBlank()) {
                writer.println("package $qualifiedName;")
                writer.println()
            }

            @Suppress("ConstantConditionIf")
            if (EXPAND_DOCUMENTATION && options.includeDocumentationInStubs) {
                val compilationUnit = cls.getCompilationUnit()
                compilationUnit?.getImportStatements(filterReference)?.let {
                    for (item in it) {
                        when (item) {
                            is PackageItem ->
                                writer.println("import ${item.qualifiedName()}.*;")
                            is ClassItem ->
                                writer.println("import ${item.qualifiedName()};")
                            is MemberItem ->
                                writer.println("import static ${item.containingClass().qualifiedName()}.${item.name()};")
                        }
                    }
                    writer.println()
                }
            }
        }

        appendDocumentation(cls, writer)

        // "ALL" doesn't do it; compiler still warns unless you actually explicitly list "unchecked"
        writer.println("@SuppressWarnings({\"unchecked\", \"deprecation\", \"all\"})")

        // Need to filter out abstract from the modifiers list and turn it
        // into a concrete method to make the stub compile
        val removeAbstract = cls.modifiers.isAbstract() && (cls.isEnum() || cls.isAnnotationType())

        appendModifiers(cls, removeAbstract)

        when {
            cls.isAnnotationType() -> writer.print("@interface")
            cls.isInterface() -> writer.print("interface")
            cls.isEnum() -> writer.print("enum")
            else -> writer.print("class")
        }

        writer.print(" ")
        writer.print(cls.simpleName())

        generateTypeParameterList(typeList = cls.typeParameterList(), addSpace = false)
        generateSuperClassStatement(cls)
        if (!cls.notStrippable) {
            generateInterfaceList(cls)
        }
        writer.print(" {\n")

        if (cls.isEnum()) {
            var first = true
            // Enums should preserve the original source order, not alphabetical etc sort
            for (field in cls.filteredFields(filterReference, true).sortedBy { it.sortingRank }) {
                if (field.isEnumConstant()) {
                    if (first) {
                        first = false
                    } else {
                        writer.write(",\n")
                    }
                    appendDocumentation(field, writer)

                    // Can't just appendModifiers(field, true, true): enum constants
                    // don't take modifier lists, only annotations
                    ModifierList.writeAnnotations(
                        item = field,
                        target = annotationTarget,
                        runtimeAnnotationsOnly = !generateAnnotations,
                        includeDeprecated = true,
                        writer = writer,
                        separateLines = true,
                        list = field.modifiers,
                        skipNullnessAnnotations = false,
                        omitCommonPackages = false
                    )

                    writer.write(field.name())
                }
            }
            writer.println(";")
        }

        generateMissingConstructors(cls)
    }

    private fun appendDocumentation(item: Item, writer: PrintWriter) {
        if (options.includeDocumentationInStubs || docStubs) {
            val documentation = if (docStubs && EXPAND_DOCUMENTATION) {
                item.fullyQualifiedDocumentation()
            } else {
                item.documentation
            }
            if (documentation.isNotBlank()) {
                val trimmed = trimDocIndent(documentation)
                writer.println(trimmed)
                writer.println()
            }
        }
    }

    override fun afterVisitClass(cls: ClassItem) {
        writer.print("}\n\n")
    }

    private fun appendModifiers(
        item: Item,
        removeAbstract: Boolean = false,
        removeFinal: Boolean = false,
        addPublic: Boolean = false
    ) {
        appendModifiers(item, item.modifiers, removeAbstract, removeFinal, addPublic)
    }

    private fun appendModifiers(
        item: Item,
        modifiers: ModifierList,
        removeAbstract: Boolean,
        removeFinal: Boolean = false,
        addPublic: Boolean = false
    ) {
        val separateLines = item is ClassItem || item is MethodItem

        ModifierList.write(
            writer, modifiers, item,
            target = annotationTarget,
            includeAnnotations = true,
            includeDeprecated = true,
            runtimeAnnotationsOnly = !generateAnnotations,
            removeAbstract = removeAbstract,
            removeFinal = removeFinal,
            addPublic = addPublic,
            separateLines = separateLines
        )
    }

    private fun generateSuperClassStatement(cls: ClassItem) {
        if (cls.isEnum() || cls.isAnnotationType()) {
            // No extends statement for enums and annotations; it's implied by the "enum" and "@interface" keywords
            return
        }

        val superClass = if (preFiltered)
            cls.superClassType()
        else cls.filteredSuperClassType(filterReference)

        if (superClass != null && !superClass.isJavaLangObject()) {
            val qualifiedName = superClass.toTypeString()
            writer.print(" extends ")

            if (qualifiedName.contains("<")) {
                // TODO: I need to push this into the model at filter-time such that clients don't need
                // to remember to do this!!
                val s = superClass.asClass()
                if (s != null) {
                    val map = cls.mapTypeVariables(s)
                    val replaced = superClass.convertTypeString(map)
                    writer.print(replaced)
                    return
                }
            }
            (cls as PsiClassItem).psiClass.superClassType
            writer.print(qualifiedName)
        }
    }

    private fun generateInterfaceList(cls: ClassItem) {
        if (cls.isAnnotationType()) {
            // No extends statement for annotations; it's implied by the "@interface" keyword
            return
        }

        val interfaces = if (preFiltered)
            cls.interfaceTypes().asSequence()
        else cls.filteredInterfaceTypes(filterReference).asSequence()

        if (interfaces.any()) {
            if (cls.isInterface() && cls.superClassType() != null)
                writer.print(", ")
            else writer.print(" implements")
            interfaces.forEachIndexed { index, type ->
                if (index > 0) {
                    writer.print(",")
                }
                writer.print(" ")
                writer.print(type.toTypeString())
            }
        } else if (compatibility.classForAnnotations && cls.isAnnotationType()) {
            writer.print(" implements java.lang.annotation.Annotation")
        }
    }

    private fun generateTypeParameterList(
        typeList: TypeParameterList,
        addSpace: Boolean
    ) {
        // TODO: Do I need to map type variables?

        val typeListString = typeList.toString()
        if (typeListString.isNotEmpty()) {
            writer.print(typeListString)

            if (addSpace) {
                writer.print(' ')
            }
        }
    }

    override fun visitConstructor(constructor: ConstructorItem) {
        if (constructor.containingClass().notStrippable) {
            return
        }
        writeConstructor(constructor, constructor.superConstructor)
    }

    private fun writeConstructor(
        constructor: MethodItem,
        superConstructor: MethodItem?
    ) {
        writer.println()
        appendDocumentation(constructor, writer)
        appendModifiers(constructor, false)
        generateTypeParameterList(
            typeList = constructor.typeParameterList(),
            addSpace = true
        )
        writer.print(constructor.containingClass().simpleName())

        generateParameterList(constructor)
        generateThrowsList(constructor)

        writer.print(" { ")

        writeConstructorBody(constructor, superConstructor)
        writer.println(" }")
    }

    private fun writeConstructorBody(constructor: MethodItem?, superConstructor: MethodItem?) {
        // Find any constructor in parent that we can compile against
        superConstructor?.let { it ->
            val parameters = it.parameters()
            val invokeOnThis = constructor != null && constructor.containingClass() == it.containingClass()
            if (invokeOnThis || parameters.isNotEmpty()) {
                val includeCasts = parameters.isNotEmpty() &&
                    it.containingClass().constructors().filter { filterReference.test(it) }.size > 1
                if (invokeOnThis) {
                    writer.print("this(")
                } else {
                    writer.print("super(")
                }
                parameters.forEachIndexed { index, parameter ->
                    if (index > 0) {
                        writer.write(", ")
                    }
                    val type = parameter.type()
                    if (!type.primitive) {
                        if (includeCasts) {
                            // Types with varargs can't appear as varargs when used as an argument
                            val typeString = type.toErasedTypeString(it).replace("...", "[]")
                            writer.write("(")
                            if (type.asTypeParameter(superConstructor) != null) {
                                // It's a type parameter: see if we should map the type back to the concrete
                                // type in this class
                                val map = constructor?.containingClass()?.mapTypeVariables(it.containingClass())
                                val cast = map?.get(type.toTypeString(context = it)) ?: typeString
                                writer.write(cast)
                            } else {
                                writer.write(typeString)
                            }
                            writer.write(")")
                        }
                        writer.write("null")
                    } else {
                        // Add cast for things like shorts and bytes
                        val typeString = type.toTypeString(context = it)
                        if (typeString != "boolean" && typeString != "int" && typeString != "long") {
                            writer.write("(")
                            writer.write(typeString)
                            writer.write(")")
                        }
                        writer.write(type.defaultValueString())
                    }
                }
                writer.print("); ")
            }
        }

        writeThrowStub()
    }

    private fun generateMissingConstructors(cls: ClassItem) {
        val clsStubConstructor = cls.stubConstructor
        val constructors = cls.filteredConstructors(filterEmit)
        // If the default stub constructor is not publicly visible then it won't be output during the normal visiting
        // so visit it specially to ensure that it is output.
        if (clsStubConstructor != null && !constructors.contains(clsStubConstructor)) {
            visitConstructor(clsStubConstructor)
            return
        }
    }

    override fun visitMethod(method: MethodItem) {
        if (method.containingClass().notStrippable) {
            return
        }
        writeMethod(method.containingClass(), method, false)
    }

    private fun writeMethod(containingClass: ClassItem, method: MethodItem, movedFromInterface: Boolean) {
        val modifiers = method.modifiers
        val isEnum = containingClass.isEnum()
        val isAnnotation = containingClass.isAnnotationType()

        if (isEnum && (method.name() == "values" ||
                method.name() == "valueOf" && method.parameters().size == 1 &&
                method.parameters()[0].type().toTypeString() == JAVA_LANG_STRING)
        ) {
            // Skip the values() and valueOf(String) methods in enums: these are added by
            // the compiler for enums anyway, but was part of the doclava1 signature files
            // so inserted in compat mode.
            return
        }

        writer.println()
        appendDocumentation(method, writer)

        // Need to filter out abstract from the modifiers list and turn it
        // into a concrete method to make the stub compile
        val removeAbstract = modifiers.isAbstract() && (isEnum || isAnnotation) || movedFromInterface

        appendModifiers(method, modifiers, removeAbstract, movedFromInterface)
        generateTypeParameterList(typeList = method.typeParameterList(), addSpace = true)

        val returnType = method.returnType()
        writer.print(
            returnType?.toTypeString(
                outerAnnotations = false,
                innerAnnotations = generateAnnotations,
                filter = filterReference
            )
        )

        writer.print(' ')
        writer.print(method.name())
        generateParameterList(method)
        generateThrowsList(method)

        if (isAnnotation) {
            val default = method.defaultValue()
            if (default.isNotEmpty()) {
                writer.print(" default ")
                writer.print(default)
            }
        }

        if (modifiers.isAbstract() && !removeAbstract && !isEnum || isAnnotation || modifiers.isNative()) {
            writer.println(";")
        } else {
            writer.print(" { ")
            writeThrowStub()
            writer.println(" }")
        }
    }

    override fun visitField(field: FieldItem) {
        // Handled earlier in visitClass
        if (field.isEnumConstant()) {
            return
        }

        if (field.containingClass().notStrippable) {
            return
        }

        writer.println()

        appendDocumentation(field, writer)
        appendModifiers(field, false, false)
        writer.print(
            field.type().toTypeString(
                outerAnnotations = false,
                innerAnnotations = generateAnnotations,
                filter = filterReference
            )
        )
        writer.print(' ')
        writer.print(field.name())
        val needsInitialization =
            field.modifiers.isFinal() && field.initialValue(true) == null && field.containingClass().isClass()
        field.writeValueWithSemicolon(
            writer,
            allowDefaultValue = !needsInitialization,
            requireInitialValue = !needsInitialization
        )
        writer.print("\n")

        if (needsInitialization) {
            if (field.modifiers.isStatic()) {
                writer.print("static ")
            }
            writer.print("{ ${field.name()} = ${field.type().defaultValueString()}; }\n")
        }
    }

    private fun writeThrowStub() {
        writer.write("throw new RuntimeException(\"Stub!\");")
    }

    private fun generateParameterList(method: MethodItem) {
        writer.print("(")
        method.parameters().asSequence().forEachIndexed { i, parameter ->
            if (i > 0) {
                writer.print(", ")
            }
            appendModifiers(parameter, false)
            writer.print(
                parameter.type().toTypeString(
                    outerAnnotations = false,
                    innerAnnotations = generateAnnotations,
                    filter = filterReference
                )
            )
            writer.print(' ')
            val name = parameter.publicName() ?: parameter.name()
            writer.print(name)
        }
        writer.print(")")
    }

    private fun generateThrowsList(method: MethodItem) {
        // Note that throws types are already sorted internally to help comparison matching
        val throws = if (preFiltered) {
            method.throwsTypes().asSequence()
        } else {
            method.filteredThrowsTypes(filterReference).asSequence()
        }
        if (throws.any()) {
            writer.print(" throws ")
            throws.asSequence().sortedWith(ClassItem.fullNameComparator).forEachIndexed { i, type ->
                if (i > 0) {
                    writer.print(", ")
                }
                // TODO: Shouldn't declare raw types here!
                writer.print(type.qualifiedName())
            }
        }
    }
}
