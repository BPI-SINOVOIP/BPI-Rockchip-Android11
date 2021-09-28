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

package com.android.tools.metalava.model

import com.android.tools.metalava.DocLevel
import com.android.tools.metalava.DocLevel.HIDDEN
import com.android.tools.metalava.DocLevel.PACKAGE
import com.android.tools.metalava.DocLevel.PRIVATE
import com.android.tools.metalava.DocLevel.PROTECTED
import com.android.tools.metalava.DocLevel.PUBLIC
import com.android.tools.metalava.Options
import com.android.tools.metalava.compatibility
import com.android.tools.metalava.options
import java.io.Writer

interface ModifierList {
    val codebase: Codebase
    fun annotations(): List<AnnotationItem>

    fun owner(): Item
    fun getVisibilityLevel(): VisibilityLevel
    fun isPublic(): Boolean
    fun isProtected(): Boolean
    fun isPrivate(): Boolean
    fun isStatic(): Boolean
    fun isAbstract(): Boolean
    fun isFinal(): Boolean
    fun isNative(): Boolean
    fun isSynchronized(): Boolean
    fun isStrictFp(): Boolean
    fun isTransient(): Boolean
    fun isVolatile(): Boolean
    fun isDefault(): Boolean

    // Modifier in Kotlin, separate syntax (...) in Java but modeled as modifier here
    fun isVarArg(): Boolean = false

    // Kotlin
    fun isSealed(): Boolean = false
    fun isCompanion(): Boolean = false
    fun isInfix(): Boolean = false
    fun isConst(): Boolean = false
    fun isSuspend(): Boolean = false
    fun isOperator(): Boolean = false
    fun isInline(): Boolean = false
    fun isEmpty(): Boolean

    fun isPackagePrivate() = !(isPublic() || isProtected() || isPrivate())
    fun isPublicOrProtected() = isPublic() || isProtected()

    // Rename? It's not a full equality, it's whether an override's modifier set is significant
    fun equivalentTo(other: ModifierList): Boolean {
        if (isPublic() != other.isPublic()) return false
        if (isProtected() != other.isProtected()) return false
        if (isPrivate() != other.isPrivate()) return false

        if (isStatic() != other.isStatic()) return false
        if (isAbstract() != other.isAbstract()) return false
        if (isFinal() != other.isFinal()) { return false }
        if (compatibility.includeSynchronized && isSynchronized() != other.isSynchronized()) return false
        if (isTransient() != other.isTransient()) return false
        if (isVolatile() != other.isVolatile()) return false

        // Default does not require an override to "remove" it
        // if (isDefault() != other.isDefault()) return false

        return true
    }

    /** Returns true if this modifier list contains any nullness information */
    fun hasNullnessInfo(): Boolean {
        return annotations().any { it.isNonNull() || it.isNullable() }
    }

    /** Returns true if this modifier list contains any a Nullable annotation */
    fun isNullable(): Boolean {
        return annotations().any { it.isNullable() }
    }

    /**
     * Returns true if this modifier list contains any annotations explicitly passed in
     * via [Options.showAnnotations]
     */
    fun hasShowAnnotation(): Boolean {
        if (options.showAnnotations.isEmpty()) {
            return false
        }
        return annotations().any {
            options.showAnnotations.matches(it)
        }
    }

    /**
     * Returns true if this modifier list contains any annotations explicitly passed in
     * via [Options.showSingleAnnotations]
     */
    fun hasShowSingleAnnotation(): Boolean {

        if (options.showSingleAnnotations.isEmpty()) {
            return false
        }
        return annotations().any {
            options.showSingleAnnotations.matches(it)
        }
    }

    /**
     * Returns true if this modifier list contains any annotations explicitly passed in
     * via [Options.hideAnnotations] or any annotations which are themselves annotated
     * with meta-annotations explicitly passed in via [Options.hideMetaAnnotations]
     *
     * @see hasHideMetaAnnotations
     */
    fun hasHideAnnotations(): Boolean {
        if (options.hideAnnotations.isEmpty() && options.hideMetaAnnotations.isEmpty()) {
            return false
        }
        return annotations().any { annotation ->
            options.hideAnnotations.matches(annotation) ||
                annotation.resolve()?.hasHideMetaAnnotation() ?: false
        }
    }

    /**
     * Returns true if this modifier list contains any meta-annotations explicitly passed in
     * via [Options.hideMetaAnnotations].
     *
     * Hidden meta-annotations allow Metalava to handle concepts like Kotlin's [Experimental],
     * which allows developers to create annotations that describe experimental features -- sets
     * of distinct and potentially overlapping unstable API surfaces. Libraries may wish to exclude
     * such sets of APIs from tracking and stub JAR generation by passing [Experimental] as a
     * hidden meta-annotation.
     */
    fun hasHideMetaAnnotations(): Boolean {
        if (options.hideMetaAnnotations.isEmpty()) {
            return false
        }
        return annotations().any { annotation ->
            options.hideMetaAnnotations.contains(annotation.qualifiedName())
        }
    }

    /** Returns true if this modifier list contains the given annotation */
    fun isAnnotatedWith(qualifiedName: String): Boolean {
        return findAnnotation(qualifiedName) != null
    }

    /** Returns the annotation of the given qualified name if found in this modifier list */
    fun findAnnotation(qualifiedName: String): AnnotationItem? {
        val mappedName = AnnotationItem.mapName(codebase, qualifiedName)
        return annotations().firstOrNull {
            mappedName == it.qualifiedName()
        }
    }

    /** Returns true if this modifier list has adequate access */
    fun checkLevel() = checkLevel(options.docLevel)

    /**
     * Returns true if this modifier list has access modifiers that
     * are adequate for the given documentation level
     */
    fun checkLevel(level: DocLevel): Boolean {
        if (level == HIDDEN) {
            return true
        } else if (owner().isHiddenOrRemoved()) {
            return false
        }
        return when (level) {
            PUBLIC -> isPublic()
            PROTECTED -> isPublic() || isProtected()
            PACKAGE -> !isPrivate()
            PRIVATE, HIDDEN -> true
        }
    }

    /**
     * Returns true if the visibility modifiers in this modifier list is as least as visible
     * as the ones in the given [other] modifier list
     */
    fun asAccessibleAs(other: ModifierList): Boolean {
        val otherLevel = other.getVisibilityLevel()
        val thisLevel = getVisibilityLevel()
        // Generally the access level enum order determines relative visibility. However, there is an exception because
        // package private and internal are not directly comparable.
        val result = thisLevel >= otherLevel
        return when (otherLevel) {
            VisibilityLevel.PACKAGE_PRIVATE -> result && thisLevel != VisibilityLevel.INTERNAL
            VisibilityLevel.INTERNAL -> result && thisLevel != VisibilityLevel.PACKAGE_PRIVATE
            else -> result
        }
    }

    /** User visible description of the visibility in this modifier list */
    fun getVisibilityString(): String {
        return getVisibilityLevel().userVisibleDescription
    }

    /**
     * Like [getVisibilityString], but package private has no modifiers; this typically corresponds to
     * the source code for the visibility modifiers in the modifier list
     */
    fun getVisibilityModifiers(): String {
        return getVisibilityLevel().sourceCodeModifier
    }

    companion object {
        fun write(
            writer: Writer,
            modifiers: ModifierList,
            item: Item,
            target: AnnotationTarget,
            // TODO: "deprecated" isn't a modifier; clarify method name
            includeDeprecated: Boolean = false,
            includeAnnotations: Boolean = true,
            runtimeAnnotationsOnly: Boolean = false,
            skipNullnessAnnotations: Boolean = false,
            omitCommonPackages: Boolean = false,
            removeAbstract: Boolean = false,
            removeFinal: Boolean = false,
            addPublic: Boolean = false,
            separateLines: Boolean = false
        ) {

            val list = if (removeAbstract || removeFinal || addPublic) {
                class AbstractFiltering : ModifierList by modifiers {
                    override fun isAbstract(): Boolean {
                        return if (removeAbstract) false else modifiers.isAbstract()
                    }

                    override fun isFinal(): Boolean {
                        return if (removeFinal) false else modifiers.isFinal()
                    }

                    override fun getVisibilityLevel(): VisibilityLevel {
                        return if (addPublic) VisibilityLevel.PUBLIC else modifiers.getVisibilityLevel()
                    }
                }
                AbstractFiltering()
            } else {
                modifiers
            }

            if (includeAnnotations) {
                writeAnnotations(
                    item,
                    target,
                    runtimeAnnotationsOnly,
                    includeDeprecated,
                    writer,
                    separateLines,
                    list,
                    skipNullnessAnnotations,
                    omitCommonPackages
                )
            } else {
                // We always include @Deprecated annotation in stub files
                if (item.deprecated && target.isStubsFile()) {
                    writer.write("@Deprecated")
                    writer.write(if (separateLines) "\n" else " ")
                }
            }

            if (item is PackageItem) {
                // Packages use a modifier list, but only annotations apply
                return
            }

            // Kotlin order:
            //   https://kotlinlang.org/docs/reference/coding-conventions.html#modifiers

            // Abstract: should appear in interfaces if in compat mode
            val classItem = item as? ClassItem
            val methodItem = item as? MethodItem

            // Order based on the old stubs code: TODO, use Java standard order instead?

            if (compatibility.nonstandardModifierOrder) {
                val visibilityLevel = list.getVisibilityLevel()
                if (visibilityLevel != VisibilityLevel.PACKAGE_PRIVATE) {
                    writer.write(visibilityLevel.sourceCodeModifier + " ")
                }

                if (list.isDefault()) {
                    writer.write("default ")
                }

                if (list.isStatic() && (compatibility.staticEnums || classItem == null || !classItem.isEnum())) {
                    writer.write("static ")
                }

                if (list.isFinal() &&
                    // Don't show final on parameters: that's an implementation side detail
                    item !is ParameterItem &&
                    (classItem?.isEnum() != true || compatibility.finalInInterfaces) ||
                    compatibility.forceFinalInEnumValueMethods &&
                    methodItem?.name() == "values" && methodItem.containingClass().isEnum()
                ) {
                    writer.write("final ")
                }

                if (list.isSealed()) {
                    writer.write("sealed ")
                }

                if (list.isSuspend()) {
                    writer.write("suspend ")
                }

                if (list.isInline()) {
                    writer.write("inline ")
                }

                if (list.isInfix()) {
                    writer.write("infix ")
                }

                if (list.isOperator()) {
                    writer.write("operator ")
                }

                val isInterface = classItem?.isInterface() == true ||
                    (methodItem?.containingClass()?.isInterface() == true &&
                        !list.isDefault() && !list.isStatic())

                if ((compatibility.abstractInInterfaces && isInterface ||
                        list.isAbstract() &&
                        (classItem?.isEnum() != true &&
                            (compatibility.abstractInAnnotations || classItem?.isAnnotationType() != true))) &&
                    (!isInterface || compatibility.abstractInInterfaces)
                ) {
                    writer.write("abstract ")
                }

                if (list.isNative() && target.isStubsFile()) {
                    writer.write("native ")
                }

                if (item.deprecated && includeDeprecated && !target.isStubsFile() && !compatibility.deprecatedAsAnnotation) {
                    writer.write("deprecated ")
                }

                if (list.isSynchronized() && (compatibility.includeSynchronized || target.isStubsFile())) {
                    writer.write("synchronized ")
                }

                if (list.isTransient()) {
                    writer.write("transient ")
                }

                if (list.isVolatile()) {
                    writer.write("volatile ")
                }
            } else {
                if (item.deprecated && includeDeprecated && !target.isStubsFile() && !compatibility.deprecatedAsAnnotation) {
                    writer.write("deprecated ")
                }

                val visibilityLevel = list.getVisibilityLevel()
                if (visibilityLevel != VisibilityLevel.PACKAGE_PRIVATE) {
                    writer.write(visibilityLevel.sourceCodeModifier + " ")
                }

                val isInterface = classItem?.isInterface() == true ||
                    (methodItem?.containingClass()?.isInterface() == true &&
                        !list.isDefault() && !list.isStatic())

                if ((compatibility.abstractInInterfaces && isInterface ||
                        list.isAbstract() &&
                        (classItem?.isEnum() != true &&
                            (compatibility.abstractInAnnotations || classItem?.isAnnotationType() != true))) &&
                    (!isInterface || compatibility.abstractInInterfaces)
                ) {
                    writer.write("abstract ")
                }

                if (list.isDefault() && item !is ParameterItem) {
                    writer.write("default ")
                }

                if (list.isStatic() && (compatibility.staticEnums || classItem == null || !classItem.isEnum())) {
                    writer.write("static ")
                }

                if (list.isFinal() &&
                    // Don't show final on parameters: that's an implementation side detail
                    item !is ParameterItem &&
                    (classItem?.isEnum() != true || compatibility.finalInInterfaces)
                ) {
                    writer.write("final ")
                }

                if (list.isSealed()) {
                    writer.write("sealed ")
                }

                if (list.isSuspend()) {
                    writer.write("suspend ")
                }

                if (list.isInline()) {
                    writer.write("inline ")
                }

                if (list.isInfix()) {
                    writer.write("infix ")
                }

                if (list.isOperator()) {
                    writer.write("operator ")
                }

                if (list.isTransient()) {
                    writer.write("transient ")
                }

                if (list.isVolatile()) {
                    writer.write("volatile ")
                }

                if (list.isSynchronized() && (compatibility.includeSynchronized || target.isStubsFile())) {
                    writer.write("synchronized ")
                }

                if (list.isNative() && target.isStubsFile()) {
                    writer.write("native ")
                }
            }
        }

        fun writeAnnotations(
            item: Item,
            target: AnnotationTarget,
            runtimeAnnotationsOnly: Boolean,
            includeDeprecated: Boolean,
            writer: Writer,
            separateLines: Boolean,
            list: ModifierList,
            skipNullnessAnnotations: Boolean,
            omitCommonPackages: Boolean
        ) {
            //  if includeDeprecated we want to do it
            //  unless runtimeOnly is false, in which case we'd include it too
            // e.g. emit @Deprecated if includeDeprecated && !runtimeOnly
            if (item.deprecated &&
                (compatibility.deprecatedAsAnnotation || target.isStubsFile()) &&
                (runtimeAnnotationsOnly || includeDeprecated)
            ) {
                writer.write("@Deprecated")
                writer.write(if (separateLines) "\n" else " ")
            }

            writeAnnotations(
                list = list,
                runtimeAnnotationsOnly = runtimeAnnotationsOnly,
                skipNullnessAnnotations = skipNullnessAnnotations,
                omitCommonPackages = omitCommonPackages,
                separateLines = separateLines,
                writer = writer,
                target = target
            )
        }

        fun writeAnnotations(
            list: ModifierList,
            skipNullnessAnnotations: Boolean = false,
            runtimeAnnotationsOnly: Boolean = false,
            omitCommonPackages: Boolean = false,
            separateLines: Boolean = false,
            filterDuplicates: Boolean = false,
            writer: Writer,
            target: AnnotationTarget
        ) {
            var annotations = list.annotations()

            // Ensure stable signature file order
            if (annotations.size > 1) {
                annotations = annotations.sortedBy { it.qualifiedName() }
            }

            if (annotations.isNotEmpty()) {
                var index = -1
                for (annotation in annotations) {
                    index++

                    if (runtimeAnnotationsOnly && annotation.retention != AnnotationRetention.RUNTIME) {
                        continue
                    }

                    var printAnnotation = annotation
                    if (!annotation.targets().contains(target)) {
                        continue
                    } else if ((annotation.isNullnessAnnotation())) {
                        if (skipNullnessAnnotations) {
                            continue
                        }
                    } else if (annotation.qualifiedName() == "java.lang.Deprecated") {
                        // Special cased in stubs and signature files: emitted first
                        continue
                    } else if (options.typedefMode == Options.TypedefMode.INLINE) {
                        val typedef = annotation.findTypedefAnnotation()
                        if (typedef != null) {
                            printAnnotation = typedef
                        }
                    } else if (options.typedefMode == Options.TypedefMode.REFERENCE &&
                        annotation.targets() === ANNOTATION_SIGNATURE_ONLY &&
                        annotation.findTypedefAnnotation() != null) {
                        // For annotation references, only include the simple name
                        writer.write("@")
                        writer.write(annotation.resolve()?.simpleName() ?: annotation.qualifiedName()!!)
                        if (separateLines) {
                            writer.write("\n")
                        } else {
                            writer.write(" ")
                        }
                        continue
                    }

                    // Optionally filter out duplicates
                    if (index > 0 && filterDuplicates) {
                        val qualifiedName = annotation.qualifiedName()
                        var found = false
                        for (i in 0 until index) {
                            val prev = annotations[i]
                            if (prev.qualifiedName() == qualifiedName) {
                                found = true
                                break
                            }
                        }
                        if (found) {
                            continue
                        }
                    }

                    val source = printAnnotation.toSource(target, showDefaultAttrs = false)

                    if (omitCommonPackages) {
                        writer.write(AnnotationItem.shortenAnnotation(source))
                    } else {
                        writer.write(source)
                    }
                    if (separateLines) {
                        writer.write("\n")
                    } else {
                        writer.write(" ")
                    }
                }
            }
        }
    }
}
