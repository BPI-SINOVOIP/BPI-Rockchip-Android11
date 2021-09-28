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

package com.android.tools.metalava.model

import com.android.tools.metalava.compatibility

open class DefaultModifierList(
    override val codebase: Codebase,
    protected var flags: Int = PACKAGE_PRIVATE,
    protected open var annotations: MutableList<AnnotationItem>? = null
) : MutableModifierList {
    private lateinit var owner: Item

    private operator fun set(mask: Int, set: Boolean) {
        flags = if (set) {
            flags or mask
        } else {
            flags and mask.inv()
        }
    }

    private fun isSet(mask: Int): Boolean {
        return flags and mask != 0
    }

    override fun annotations(): List<AnnotationItem> {
        return annotations ?: emptyList()
    }

    override fun owner(): Item {
        return owner
    }

    fun setOwner(owner: Item) {
        this.owner = owner
    }

    override fun getVisibilityLevel(): VisibilityLevel {
        val visibilityFlags = flags and VISIBILITY_MASK
        val levels = VISIBILITY_LEVEL_ENUMS
        if (visibilityFlags >= levels.size) {
            throw IllegalStateException(
                "Visibility flags are invalid, expected value in range [0, " + levels.size + ") got " + visibilityFlags)
        }
        return levels[visibilityFlags]
    }

    override fun isPublic(): Boolean {
        return getVisibilityLevel() == VisibilityLevel.PUBLIC
    }

    override fun isProtected(): Boolean {
        return getVisibilityLevel() == VisibilityLevel.PROTECTED
    }

    override fun isPrivate(): Boolean {
        return getVisibilityLevel() == VisibilityLevel.PRIVATE
    }

    override fun isStatic(): Boolean {
        return isSet(STATIC)
    }

    override fun isAbstract(): Boolean {
        return isSet(ABSTRACT)
    }

    override fun isFinal(): Boolean {
        return isSet(FINAL)
    }

    override fun isNative(): Boolean {
        return isSet(NATIVE)
    }

    override fun isSynchronized(): Boolean {
        return isSet(SYNCHRONIZED)
    }

    override fun isStrictFp(): Boolean {
        return isSet(STRICT_FP)
    }

    override fun isTransient(): Boolean {
        return isSet(TRANSIENT)
    }

    override fun isVolatile(): Boolean {
        return isSet(VOLATILE)
    }

    override fun isDefault(): Boolean {
        return isSet(DEFAULT)
    }

    fun isDeprecated(): Boolean {
        return isSet(DEPRECATED)
    }

    override fun isVarArg(): Boolean {
        return isSet(VARARG)
    }

    override fun isSealed(): Boolean {
        return isSet(SEALED)
    }

    override fun isInfix(): Boolean {
        return isSet(INFIX)
    }

    override fun isConst(): Boolean {
        return isSet(CONST)
    }

    override fun isSuspend(): Boolean {
        return isSet(SUSPEND)
    }

    override fun isCompanion(): Boolean {
        return isSet(COMPANION)
    }

    override fun isOperator(): Boolean {
        return isSet(OPERATOR)
    }

    override fun isInline(): Boolean {
        return isSet(INLINE)
    }

    override fun setVisibilityLevel(level: VisibilityLevel) {
        flags = (flags and VISIBILITY_MASK.inv()) or level.visibilityFlagValue
    }

    override fun setStatic(static: Boolean) {
        set(STATIC, static)
    }

    override fun setAbstract(abstract: Boolean) {
        set(ABSTRACT, abstract)
    }

    override fun setFinal(final: Boolean) {
        set(FINAL, final)
    }

    override fun setNative(native: Boolean) {
        set(NATIVE, native)
    }

    override fun setSynchronized(synchronized: Boolean) {
        set(SYNCHRONIZED, synchronized)
    }

    override fun setStrictFp(strictfp: Boolean) {
        set(STRICT_FP, strictfp)
    }

    override fun setTransient(transient: Boolean) {
        set(TRANSIENT, transient)
    }

    override fun setVolatile(volatile: Boolean) {
        set(VOLATILE, volatile)
    }

    override fun setDefault(default: Boolean) {
        set(DEFAULT, default)
    }

    override fun setSealed(sealed: Boolean) {
        set(SEALED, sealed)
    }

    override fun setInfix(infix: Boolean) {
        set(INFIX, infix)
    }

    override fun setOperator(operator: Boolean) {
        set(OPERATOR, operator)
    }

    override fun setInline(inline: Boolean) {
        set(INLINE, inline)
    }

    override fun setVarArg(vararg: Boolean) {
        set(VARARG, vararg)
    }

    fun setDeprecated(deprecated: Boolean) {
        set(DEPRECATED, deprecated)
    }

    fun setSuspend(suspend: Boolean) {
        set(SUSPEND, suspend)
    }

    fun setCompanion(companion: Boolean) {
        set(COMPANION, companion)
    }

    override fun addAnnotation(annotation: AnnotationItem) {
        if (annotations == null) {
            annotations = mutableListOf()
        }
        annotations?.add(annotation)
    }

    override fun removeAnnotation(annotation: AnnotationItem) {
        annotations?.remove(annotation)
    }

    override fun clearAnnotations(annotation: AnnotationItem) {
        annotations?.clear()
    }

    override fun isEmpty(): Boolean {
        return flags and DEPRECATED.inv() == 0 // deprecated isn't a real modifier
    }

    override fun isPackagePrivate(): Boolean {
        return flags and VISIBILITY_MASK == PACKAGE_PRIVATE
    }

    // Rename? It's not a full equality, it's whether an override's modifier set is significant
    override fun equivalentTo(other: ModifierList): Boolean {
        if (other is DefaultModifierList) {
            val flags2 = other.flags
            val mask = if (compatibility.includeSynchronized) COMPAT_EQUIVALENCE_MASK else EQUIVALENCE_MASK

            val masked1 = flags and mask
            val masked2 = flags2 and mask
            val same = masked1 xor masked2
            if (same == 0) {
                return true
            } else if (compatibility.hideDifferenceImplicit) {
                if (same == FINAL &&
                    // Only differ in final: not significant if implied by containing class
                    isFinal() && (owner as? MethodItem)?.containingClass()?.modifiers?.isFinal() == true) {
                    return true
                } else if (same == DEPRECATED &&
                    // Only differ in deprecated: not significant if implied by containing class
                    isDeprecated() && (owner as? MethodItem)?.containingClass()?.deprecated == true) {
                    return true
                }
            }
        }
        return false
    }

    companion object {
        const val PRIVATE = 0
        const val INTERNAL = 1
        const val PACKAGE_PRIVATE = 2
        const val PROTECTED = 3
        const val PUBLIC = 4
        const val VISIBILITY_MASK = 0b111

        /**
         * An internal copy of VisibilityLevel.values() to avoid paying the cost of duplicating the array on every
         * call.
         */
        private val VISIBILITY_LEVEL_ENUMS = VisibilityLevel.values()

        // Check that the constants above are consistent with the VisibilityLevel enum, i.e. the mask is large enough
        // to include all allowable values and that each visibility level value is the same as the corresponding enum
        // constant's ordinal.
        init {
            check(PRIVATE == VisibilityLevel.PRIVATE.ordinal)
            check(INTERNAL == VisibilityLevel.INTERNAL.ordinal)
            check(PACKAGE_PRIVATE == VisibilityLevel.PACKAGE_PRIVATE.ordinal)
            check(PROTECTED == VisibilityLevel.PROTECTED.ordinal)
            check(PUBLIC == VisibilityLevel.PUBLIC.ordinal)
            // Calculate the mask required to hold as many different values as there are VisibilityLevel values.
            // Given N visibility levels, the required mask is constructed by determining the MSB in the number N - 1
            // and then setting all bits to the right.
            // e.g. when N is 5 then N - 1 is 4, the MSB is bit 2, and so the mask is what you get when you set bits 2,
            // 1 and 0, i.e. 0b111.
            val expectedMask = (1 shl (32 - Integer.numberOfLeadingZeros(VISIBILITY_LEVEL_ENUMS.size - 1))) - 1
            check(VISIBILITY_MASK == expectedMask)
        }

        const val STATIC = 1 shl 3
        const val ABSTRACT = 1 shl 4
        const val FINAL = 1 shl 5
        const val NATIVE = 1 shl 6
        const val SYNCHRONIZED = 1 shl 7
        const val STRICT_FP = 1 shl 8
        const val TRANSIENT = 1 shl 9
        const val VOLATILE = 1 shl 10
        const val DEFAULT = 1 shl 11
        const val DEPRECATED = 1 shl 12
        const val VARARG = 1 shl 13
        const val SEALED = 1 shl 14
        // 15 currently unused
        const val INFIX = 1 shl 16
        const val OPERATOR = 1 shl 17
        const val INLINE = 1 shl 18
        const val SUSPEND = 1 shl 19
        const val COMPANION = 1 shl 20
        const val CONST = 1 shl 21

        /**
         * Modifiers considered significant to include signature files (and similarly
         * to consider whether an override of a method is different from its super implementation
         */
        private const val EQUIVALENCE_MASK = VISIBILITY_MASK or STATIC or ABSTRACT or
            FINAL or TRANSIENT or VOLATILE or DEPRECATED or VARARG or
            SEALED or INFIX or OPERATOR or SUSPEND or COMPANION

        private const val COMPAT_EQUIVALENCE_MASK = EQUIVALENCE_MASK or SYNCHRONIZED
    }
}