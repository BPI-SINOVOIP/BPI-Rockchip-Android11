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

import com.android.SdkConstants
import com.android.SdkConstants.ATTR_VALUE
import com.android.SdkConstants.INT_DEF_ANNOTATION
import com.android.SdkConstants.LONG_DEF_ANNOTATION
import com.android.SdkConstants.STRING_DEF_ANNOTATION
import com.android.tools.lint.annotations.Extractor.ANDROID_INT_DEF
import com.android.tools.lint.annotations.Extractor.ANDROID_LONG_DEF
import com.android.tools.lint.annotations.Extractor.ANDROID_STRING_DEF
import com.android.tools.metalava.ANDROIDX_ANNOTATION_PREFIX
import com.android.tools.metalava.ANDROIDX_NONNULL
import com.android.tools.metalava.ANDROIDX_NULLABLE
import com.android.tools.metalava.ANDROID_NONNULL
import com.android.tools.metalava.ANDROID_NULLABLE
import com.android.tools.metalava.ANDROID_SUPPORT_ANNOTATION_PREFIX
import com.android.tools.metalava.Compatibility
import com.android.tools.metalava.JAVA_LANG_PREFIX
import com.android.tools.metalava.Options
import com.android.tools.metalava.RECENTLY_NONNULL
import com.android.tools.metalava.RECENTLY_NULLABLE
import com.android.tools.metalava.doclava1.ApiPredicate
import com.android.tools.metalava.model.psi.PsiBasedCodebase
import com.android.tools.metalava.options
import java.util.function.Predicate

fun isNullableAnnotation(qualifiedName: String): Boolean {
    return qualifiedName.endsWith("Nullable")
}

fun isNonNullAnnotation(qualifiedName: String): Boolean {
    return qualifiedName.endsWith("NonNull") ||
        qualifiedName.endsWith("NotNull") ||
        qualifiedName.endsWith("Nonnull")
}

interface AnnotationItem {
    val codebase: Codebase

    /** Fully qualified name of the annotation */
    fun qualifiedName(): String?

    /** Fully qualified name of the annotation (prior to name mapping) */
    fun originalName(): String?

    /** Generates source code for this annotation (using fully qualified names) */
    fun toSource(
        target: AnnotationTarget = AnnotationTarget.SIGNATURE_FILE,
        showDefaultAttrs: Boolean = true
    ): String

    /** The applicable targets for this annotation */
    fun targets(): Set<AnnotationTarget>

    /** Attributes of the annotation (may be empty) */
    fun attributes(): List<AnnotationAttribute>

    /** True if this annotation represents @Nullable or @NonNull (or some synonymous annotation) */
    fun isNullnessAnnotation(): Boolean {
        return isNullable() || isNonNull()
    }

    /** True if this annotation represents @Nullable (or some synonymous annotation) */
    fun isNullable(): Boolean {
        return isNullableAnnotation(qualifiedName() ?: return false)
    }

    /** True if this annotation represents @NonNull (or some synonymous annotation) */
    fun isNonNull(): Boolean {
        return isNonNullAnnotation(qualifiedName() ?: return false)
    }

    /** True if this annotation represents @IntDef, @LongDef or @StringDef */
    fun isTypeDefAnnotation(): Boolean {
        val name = qualifiedName() ?: return false
        if (!(name.endsWith("Def"))) {
            return false
        }
        return (INT_DEF_ANNOTATION.isEquals(name) ||
            STRING_DEF_ANNOTATION.isEquals(name) ||
            LONG_DEF_ANNOTATION.isEquals(name) ||
            ANDROID_INT_DEF == name ||
            ANDROID_STRING_DEF == name ||
            ANDROID_LONG_DEF == name)
    }

    /**
     * True if this annotation represents a @ParameterName annotation (or some synonymous annotation).
     * The parameter name should be the default attribute or "value".
     */
    fun isParameterName(): Boolean {
        return qualifiedName()?.endsWith(".ParameterName") ?: return false
    }

    /**
     * True if this annotation represents a @DefaultValue annotation (or some synonymous annotation).
     * The default value should be the default attribute or "value".
     */
    fun isDefaultValue(): Boolean {
        return qualifiedName()?.endsWith(".DefaultValue") ?: return false
    }

    /** Returns the given named attribute if specified */
    fun findAttribute(name: String?): AnnotationAttribute? {
        val actualName = name ?: ATTR_VALUE
        return attributes().firstOrNull { it.name == actualName }
    }

    /** Find the class declaration for the given annotation */
    fun resolve(): ClassItem? {
        return codebase.findClass(qualifiedName() ?: return null)
    }

    /** If this annotation has a typedef annotation associated with it, return it */
    fun findTypedefAnnotation(): AnnotationItem? {
        val className = originalName() ?: return null
        return codebase.findClass(className)?.modifiers?.annotations()?.firstOrNull { it.isTypeDefAnnotation() }
    }

    /** Returns the retention of this annotation */
    val retention: AnnotationRetention
        get() {
            val name = qualifiedName()
            if (name != null) {
                val cls = codebase.findClass(name) ?: (codebase as? PsiBasedCodebase)?.findOrCreateClass(name)
                if (cls != null) {
                    if (cls.isAnnotationType()) {
                        return cls.getRetention()
                    }
                }
            }

            return AnnotationRetention.CLASS
        }

    companion object {
        /** The simple name of an annotation, which is the annotation name (not qualified name) prefixed by @ */
        fun simpleName(item: AnnotationItem): String {
            val qualifiedName = item.qualifiedName() ?: return ""
            return "@${qualifiedName.substring(qualifiedName.lastIndexOf('.') + 1)}"
        }

        /**
         * Maps an annotation name to the name to be used in signatures/stubs/external annotation files.
         * Annotations that should not be exported are mapped to null.
         */
        fun mapName(
            codebase: Codebase,
            qualifiedName: String?,
            filter: Predicate<Item>? = null,
            target: AnnotationTarget = AnnotationTarget.SIGNATURE_FILE
        ): String? {
            qualifiedName ?: return null
            if (options.passThroughAnnotations.contains(qualifiedName)) {
                return qualifiedName
            }
            when (qualifiedName) {
                // Resource annotations
                "android.support.annotation.AnimRes",
                "android.annotation.AnimRes" -> return "androidx.annotation.AnimRes"
                "android.support.annotation.AnimatorRes",
                "android.annotation.AnimatorRes" -> return "androidx.annotation.AnimatorRes"
                "android.support.annotation.AnyRes",
                "android.annotation.AnyRes" -> return "androidx.annotation.AnyRes"
                "android.support.annotation.ArrayRes",
                "android.annotation.ArrayRes" -> return "androidx.annotation.ArrayRes"
                "android.support.annotation.AttrRes",
                "android.annotation.AttrRes" -> return "androidx.annotation.AttrRes"
                "android.support.annotation.BoolRes",
                "android.annotation.BoolRes" -> return "androidx.annotation.BoolRes"
                "android.support.annotation.ColorRes",
                "android.annotation.ColorRes" -> return "androidx.annotation.ColorRes"
                "android.support.annotation.DimenRes",
                "android.annotation.DimenRes" -> return "androidx.annotation.DimenRes"
                "android.support.annotation.DrawableRes",
                "android.annotation.DrawableRes" -> return "androidx.annotation.DrawableRes"
                "android.support.annotation.FontRes",
                "android.annotation.FontRes" -> return "androidx.annotation.FontRes"
                "android.support.annotation.FractionRes",
                "android.annotation.FractionRes" -> return "androidx.annotation.FractionRes"
                "android.support.annotation.IdRes",
                "android.annotation.IdRes" -> return "androidx.annotation.IdRes"
                "android.support.annotation.IntegerRes",
                "android.annotation.IntegerRes" -> return "androidx.annotation.IntegerRes"
                "android.support.annotation.InterpolatorRes",
                "android.annotation.InterpolatorRes" -> return "androidx.annotation.InterpolatorRes"
                "android.support.annotation.LayoutRes",
                "android.annotation.LayoutRes" -> return "androidx.annotation.LayoutRes"
                "android.support.annotation.MenuRes",
                "android.annotation.MenuRes" -> return "androidx.annotation.MenuRes"
                "android.support.annotation.PluralsRes",
                "android.annotation.PluralsRes" -> return "androidx.annotation.PluralsRes"
                "android.support.annotation.RawRes",
                "android.annotation.RawRes" -> return "androidx.annotation.RawRes"
                "android.support.annotation.StringRes",
                "android.annotation.StringRes" -> return "androidx.annotation.StringRes"
                "android.support.annotation.StyleRes",
                "android.annotation.StyleRes" -> return "androidx.annotation.StyleRes"
                "android.support.annotation.StyleableRes",
                "android.annotation.StyleableRes" -> return "androidx.annotation.StyleableRes"
                "android.support.annotation.TransitionRes",
                "android.annotation.TransitionRes" -> return "androidx.annotation.TransitionRes"
                "android.support.annotation.XmlRes",
                "android.annotation.XmlRes" -> return "androidx.annotation.XmlRes"

                // Threading
                "android.support.annotation.AnyThread",
                "android.annotation.AnyThread" -> return "androidx.annotation.AnyThread"
                "android.support.annotation.BinderThread",
                "android.annotation.BinderThread" -> return "androidx.annotation.BinderThread"
                "android.support.annotation.MainThread",
                "android.annotation.MainThread" -> return "androidx.annotation.MainThread"
                "android.support.annotation.UiThread",
                "android.annotation.UiThread" -> return "androidx.annotation.UiThread"
                "android.support.annotation.WorkerThread",
                "android.annotation.WorkerThread" -> return "androidx.annotation.WorkerThread"

                // Colors
                "android.support.annotation.ColorInt",
                "android.annotation.ColorInt" -> return "androidx.annotation.ColorInt"
                "android.support.annotation.ColorLong",
                "android.annotation.ColorLong" -> return "androidx.annotation.ColorLong"
                "android.support.annotation.HalfFloat",
                "android.annotation.HalfFloat" -> return "androidx.annotation.HalfFloat"

                // Ranges and sizes
                "android.support.annotation.FloatRange",
                "android.annotation.FloatRange" -> return "androidx.annotation.FloatRange"
                "android.support.annotation.IntRange",
                "android.annotation.IntRange" -> return "androidx.annotation.IntRange"
                "android.support.annotation.Size",
                "android.annotation.Size" -> return "androidx.annotation.Size"
                "android.support.annotation.Px",
                "android.annotation.Px" -> return "androidx.annotation.Px"
                "android.support.annotation.Dimension",
                "android.annotation.Dimension" -> return "androidx.annotation.Dimension"

                // Null
                // We only change recently/newly nullable annotation in stubs
                RECENTLY_NULLABLE -> return if (target == AnnotationTarget.SDK_STUBS_FILE) qualifiedName else ANDROIDX_NULLABLE
                RECENTLY_NONNULL -> return if (target == AnnotationTarget.SDK_STUBS_FILE) qualifiedName else ANDROIDX_NONNULL

                ANDROIDX_NULLABLE,
                ANDROID_NULLABLE,
                "android.support.annotation.Nullable",
                "libcore.util.Nullable",
                "org.jetbrains.annotations.Nullable" -> return nullableAnnotationName(target)

                ANDROIDX_NONNULL,
                ANDROID_NONNULL,
                "android.support.annotation.NonNull",
                "libcore.util.NonNull",
                "org.jetbrains.annotations.NotNull" -> return nonNullAnnotationName(target)

                // Typedefs
                "android.support.annotation.IntDef",
                "android.annotation.IntDef" -> return "androidx.annotation.IntDef"
                "android.support.annotation.StringDef",
                "android.annotation.StringDef" -> return "androidx.annotation.StringDef"
                "android.support.annotation.LongDef",
                "android.annotation.LongDef" -> return "androidx.annotation.LongDef"

                // Misc
                "android.support.annotation.CallSuper",
                "android.annotation.CallSuper" -> return "androidx.annotation.CallSuper"
                "android.support.annotation.CheckResult",
                "android.annotation.CheckResult" -> return "androidx.annotation.CheckResult"
                "android.support.annotation.RequiresPermission",
                "android.annotation.RequiresPermission" -> return "androidx.annotation.RequiresPermission"
                "android.annotation.RequiresPermission.Read" -> return "androidx.annotation.RequiresPermission.Read"
                "android.annotation.RequiresPermission.Write" -> return "androidx.annotation.RequiresPermission.Write"

                // These aren't support annotations, but could/should be:
                "android.annotation.CurrentTimeMillisLong",
                "android.annotation.DurationMillisLong",
                "android.annotation.ElapsedRealtimeLong",
                "android.annotation.UserIdInt",
                "android.annotation.BytesLong",

                // These aren't support annotations
                "android.annotation.AppIdInt",
                "android.annotation.SuppressAutoDoc",
                "android.annotation.SystemApi",
                "android.annotation.TestApi",
                "android.annotation.CallbackExecutor",
                "android.annotation.Condemned",
                "android.annotation.Hide",

                "android.annotation.Widget" -> {
                    // Remove, unless (a) public or (b) specifically included in --showAnnotations
                    return if (options.showAnnotations.matches(qualifiedName)) {
                        qualifiedName
                    } else if (filter != null) {
                        val cls = codebase.findClass(qualifiedName)
                        if (cls != null && filter.test(cls)) {
                            qualifiedName
                        } else {
                            null
                        }
                    } else {
                        qualifiedName
                    }
                }

                // Included for analysis, but should not be exported:
                "android.annotation.BroadcastBehavior",
                "android.annotation.SdkConstant",
                "android.annotation.RequiresFeature",
                "android.annotation.SystemService" -> return qualifiedName

                // Should not be mapped to a different package name:
                "android.annotation.TargetApi",
                "android.annotation.SuppressLint" -> return qualifiedName

                else -> {
                    // Some new annotations added to the platform: assume they are support annotations?
                    return when {
                        // Special Kotlin annotations recognized by the compiler: map to supported package name
                        qualifiedName.endsWith(".ParameterName") || qualifiedName.endsWith(".DefaultValue") ->
                            "kotlin.annotations.jvm.internal${qualifiedName.substring(qualifiedName.lastIndexOf('.'))}"

                        // Other third party nullness annotations?
                        isNullableAnnotation(qualifiedName) -> nullableAnnotationName(target)
                        isNonNullAnnotation(qualifiedName) -> nonNullAnnotationName(target)

                        // Support library annotations are all included, as is the built-in stuff like @Retention
                        qualifiedName.startsWith(ANDROIDX_ANNOTATION_PREFIX) -> return qualifiedName
                        qualifiedName.startsWith(JAVA_LANG_PREFIX) -> return qualifiedName

                        // Unknown Android platform annotations
                        qualifiedName.startsWith("android.annotation.") -> {
                            // Remove, unless specifically included in --showAnnotations
                            return if (options.showAnnotations.matches(qualifiedName)) {
                                qualifiedName
                            } else {
                                null
                            }
                        }

                        qualifiedName.startsWith(ANDROID_SUPPORT_ANNOTATION_PREFIX) -> {
                            return mapName(
                                codebase,
                                ANDROIDX_ANNOTATION_PREFIX + qualifiedName.substring(ANDROID_SUPPORT_ANNOTATION_PREFIX.length),
                                filter,
                                target
                            )
                        }

                        else -> {
                            // Remove, unless (a) public or (b) specifically included in --showAnnotations
                            return if (options.showAnnotations.matches(qualifiedName)) {
                                qualifiedName
                            } else if (filter != null) {
                                val cls = codebase.findClass(qualifiedName)
                                if (cls != null && filter.test(cls)) {
                                    qualifiedName
                                } else {
                                    null
                                }
                            } else {
                                qualifiedName
                            }
                        }
                    }
                }
            }
        }

        private fun nullableAnnotationName(target: AnnotationTarget) =
            if (target == AnnotationTarget.SDK_STUBS_FILE) ANDROID_NULLABLE else ANDROIDX_NULLABLE

        private fun nonNullAnnotationName(target: AnnotationTarget) =
            if (target == AnnotationTarget.SDK_STUBS_FILE) ANDROID_NONNULL else ANDROIDX_NONNULL

        /** The applicable targets for this annotation */
        fun computeTargets(annotation: AnnotationItem, codebase: Codebase): Set<AnnotationTarget> {
            val qualifiedName = annotation.qualifiedName() ?: return NO_ANNOTATION_TARGETS
            if (options.passThroughAnnotations.contains(qualifiedName)) {
                return ANNOTATION_IN_ALL_STUBS
            }
            when (qualifiedName) {

                // The typedef annotations are special: they should not be in the signature
                // files, but we want to include them in the external annotations file such that tools
                // can enforce them.
                "android.support.annotation.IntDef",
                "android.annotation.IntDef",
                "androidx.annotation.IntDef",
                "android.support.annotation.StringDef",
                "android.annotation.StringDef",
                "androidx.annotation.StringDef",
                "android.support.annotation.LongDef",
                "android.annotation.LongDef",
                "androidx.annotation.LongDef" -> return ANNOTATION_EXTERNAL_ONLY

                // Skip known annotations that we (a) never want in external annotations and (b) we are
                // specially overwriting anyway in the stubs (and which are (c) not API significant)
                "java.lang.annotation.Native",
                "java.lang.SuppressWarnings",
                "java.lang.Override" -> return NO_ANNOTATION_TARGETS

                "java.lang.Deprecated", // tracked separately as a pseudo-modifier

                // Below this when-statement we perform the correct lookup: check API predicate, and check
                // that retention is class or runtime, but we've hardcoded the answers here
                // for some common annotations.

                "android.view.ViewDebug.ExportedProperty",
                "android.widget.RemoteViews.RemoteView",
                "android.view.ViewDebug.CapturedViewProperty",

                "kotlin.annotation.Target",
                "kotlin.annotation.Retention",
                "kotlin.annotation.Repeatable",
                "kotlin.annotation.MustBeDocumented",
                "kotlin.DslMarker",
                "kotlin.PublishedApi",
                "kotlin.ExtensionFunctionType",

                "java.lang.FunctionalInterface",
                "java.lang.SafeVarargs",
                "java.lang.annotation.Documented",
                "java.lang.annotation.Inherited",
                "java.lang.annotation.Repeatable",
                "java.lang.annotation.Retention",
                "java.lang.annotation.Target" -> return ANNOTATION_IN_ALL_STUBS
            }

            // @android.annotation.Nullable and NonNullable specially recognized annotations by the Kotlin
            // compiler 1.3 and above: they always go in the stubs.
            if (qualifiedName == ANDROID_NULLABLE ||
                qualifiedName == ANDROID_NONNULL ||
                qualifiedName == ANDROIDX_NULLABLE ||
                qualifiedName == ANDROIDX_NONNULL
            ) {
                return ANNOTATION_IN_ALL_STUBS
            }

            if (qualifiedName.startsWith("android.annotation.")) {
                // internal annotations not mapped to androidx: things like @SystemApi. Skip from
                // stubs, external annotations, signature files, etc.
                return NO_ANNOTATION_TARGETS
            }

            // @RecentlyNullable and @RecentlyNonNull are specially recognized annotations by the Kotlin
            // compiler: they always go in the stubs.
            if (qualifiedName == RECENTLY_NULLABLE ||
                qualifiedName == RECENTLY_NONNULL
            ) {
                return ANNOTATION_IN_ALL_STUBS
            }

            // Determine the retention of the annotation: source retention annotations go
            // in the external annotations file, class and runtime annotations go in
            // the stubs files (except for the androidx annotations which are not included
            // in the SDK and therefore cannot be referenced from it due to apt's unfortunate
            // habit of loading all annotation classes it encounters.)

            if (qualifiedName.startsWith("androidx.annotation.")) {
                if (options.includeSourceRetentionAnnotations) {
                    return ANNOTATION_IN_ALL_STUBS
                }

                if (qualifiedName == ANDROIDX_NULLABLE || qualifiedName == ANDROIDX_NONNULL) {
                    // Right now, nullness annotations (other than @RecentlyNullable and @RecentlyNonNull)
                    // have to go in external annotations since they aren't in the class path for
                    // annotation processors. However, we do want them showing up in the documentation using
                    // their real annotation names.
                    return ANNOTATION_IN_DOC_STUBS_AND_EXTERNAL
                }

                return ANNOTATION_EXTERNAL
            }

            // See if the annotation is pointing to an annotation class that is part of the API; if not, skip it.
            val cls = codebase.findClass(qualifiedName) ?: return NO_ANNOTATION_TARGETS
            if (!ApiPredicate().test(cls)) {
                if (options.typedefMode != Options.TypedefMode.NONE) {
                    if (cls.modifiers.annotations().any { it.isTypeDefAnnotation() }) {
                        return ANNOTATION_SIGNATURE_ONLY
                    }
                }

                return NO_ANNOTATION_TARGETS
            }

            if (cls.isAnnotationType()) {
                val retention = cls.getRetention()
                if (retention == AnnotationRetention.RUNTIME || retention == AnnotationRetention.CLASS) {
                    return ANNOTATION_IN_SDK_STUBS
                }
            }

            return ANNOTATION_EXTERNAL
        }

        /**
         * Given a "full" annotation name, shortens it by removing redundant package names.
         * This is intended to be used by the [Compatibility.omitCommonPackages] flag
         * to reduce clutter in signature files.
         *
         * For example, this method will convert `@androidx.annotation.Nullable` to just
         * `@Nullable`, and `@androidx.annotation.IntRange(from=20)` to `IntRange(from=20)`.
         */
        fun shortenAnnotation(source: String): String {
            return when {
                source == "@java.lang.Deprecated" -> "@Deprecated"
                source.startsWith("android.annotation.", 1) -> {
                    "@" + source.substring("@android.annotation.".length)
                }
                source.startsWith(ANDROID_SUPPORT_ANNOTATION_PREFIX, 1) -> {
                    "@" + source.substring("@android.support.annotation.".length)
                }
                source.startsWith(ANDROIDX_ANNOTATION_PREFIX, 1) -> {
                    "@" + source.substring("@androidx.annotation.".length)
                }
                else -> source
            }
        }

        /**
         * Reverses the [shortenAnnotation] method. Intended for use when reading in signature files
         * that contain shortened type references.
         */
        fun unshortenAnnotation(source: String): String {
            return when {
                source == "@Deprecated" -> "@java.lang.Deprecated"
                // These 3 annotations are in the android.annotation. package, not android.support.annotation
                source.startsWith("@SystemService") ||
                    source.startsWith("@TargetApi") ||
                    source.startsWith("@SuppressLint") ->
                    "@android.annotation." + source.substring(1)
                // If the first character of the name (after "@") is lower-case, then
                // assume it's a package name, so no need to shorten it.
                source.startsWith("@") && source[1].isLowerCase() -> source
                else -> {
                    "@androidx.annotation." + source.substring(1)
                }
            }
        }

        /**
         * If the given element has an *implicit* nullness, return it. This returns
         * true for implicitly nullable elements, such as the parameter to the equals
         * method, false for implicitly non null elements (such as annotation type
         * members), and null if there is no implicit nullness.
         */
        fun getImplicitNullness(item: Item): Boolean? {
            var nullable: Boolean? = null

            // Constant field not initialized to null?
            if (item is FieldItem &&
                (item.isEnumConstant() || item.modifiers.isFinal() && item.initialValue(false) != null)
            ) {
                // Assigned to constant: not nullable
                nullable = false
            }

            // Annotation type members cannot be null
            if (item is MemberItem && item.containingClass().isAnnotationType()) {
                nullable = false
            }

            // Equals and toString have known nullness
            if (item is MethodItem && item.name() == "toString" && item.parameters().isEmpty()) {
                nullable = false
            } else if (item is ParameterItem && item.containingMethod().name() == "equals" &&
                item.containingMethod().parameters().size == 1
            ) {
                nullable = true
            }

            return nullable
        }
    }
}

/** Default implementation of an annotation item */
abstract class DefaultAnnotationItem(override val codebase: Codebase) : AnnotationItem {
    private var targets: Set<AnnotationTarget>? = null

    override fun targets(): Set<AnnotationTarget> {
        if (targets == null) {
            targets = AnnotationItem.computeTargets(this, codebase)
        }
        return targets!!
    }
}

/** An attribute of an annotation, such as "value" */
interface AnnotationAttribute {
    /** The name of the annotation */
    val name: String
    /** The annotation value */
    val value: AnnotationAttributeValue

    /**
     * Return all leaf values; this flattens the complication of handling
     * {@code @SuppressLint("warning")} and {@code @SuppressLint({"warning1","warning2"})
     */
    fun leafValues(): List<AnnotationAttributeValue> {
        val result = mutableListOf<AnnotationAttributeValue>()
        AnnotationAttributeValue.addValues(value, result)
        return result
    }
}

/** An annotation value */
interface AnnotationAttributeValue {
    /** Generates source code for this annotation value */
    fun toSource(): String

    /** The value of the annotation */
    fun value(): Any?

    /** If the annotation declaration references a field (or class etc), return the resolved class */
    fun resolve(): Item?

    companion object {
        fun addValues(value: AnnotationAttributeValue, into: MutableList<AnnotationAttributeValue>) {
            if (value is AnnotationArrayAttributeValue) {
                for (v in value.values) {
                    addValues(v, into)
                }
            } else if (value is AnnotationSingleAttributeValue) {
                into.add(value)
            }
        }
    }
}

/** An annotation value (for a single item, not an array) */
interface AnnotationSingleAttributeValue : AnnotationAttributeValue {
    /** The annotation value, expressed as source code */
    val valueSource: String
    /** The annotation value */
    val value: Any?

    override fun value() = value
}

/** An annotation value for an array of items */
interface AnnotationArrayAttributeValue : AnnotationAttributeValue {
    /** The annotation values */
    val values: List<AnnotationAttributeValue>

    override fun resolve(): Item? {
        error("resolve() should not be called on an array value")
    }

    override fun value() = values.mapNotNull { it.value() }.toTypedArray()
}

class DefaultAnnotationAttribute(
    override val name: String,
    override val value: DefaultAnnotationValue
) : AnnotationAttribute {
    companion object {
        fun create(name: String, value: String): DefaultAnnotationAttribute {
            return DefaultAnnotationAttribute(name, DefaultAnnotationValue.create(value))
        }

        fun createList(source: String): List<AnnotationAttribute> {
            val list = mutableListOf<AnnotationAttribute>() // TODO: default size = 2
            var begin = 0
            var index = 0
            val length = source.length
            while (index < length) {
                val c = source[index]
                if (c == '{') {
                    index = findEnd(source, index + 1, length, '}')
                } else if (c == '"') {
                    index = findEnd(source, index + 1, length, '"')
                } else if (c == ',') {
                    addAttribute(list, source, begin, index)
                    index++
                    begin = index
                    continue
                } else if (c == ' ' && index == begin) {
                    begin++
                }

                index++
            }

            if (begin < length) {
                addAttribute(list, source, begin, length)
            }

            return list
        }

        private fun findEnd(source: String, from: Int, to: Int, sentinel: Char): Int {
            var i = from
            while (i < to) {
                val c = source[i]
                if (c == '\\') {
                    i++
                } else if (c == sentinel) {
                    return i
                }
                i++
            }
            return to
        }

        private fun addAttribute(list: MutableList<AnnotationAttribute>, source: String, from: Int, to: Int) {
            var split = source.indexOf('=', from)
            if (split >= to) {
                split = -1
            }
            val name: String
            val value: String
            val valueBegin: Int
            val valueEnd: Int
            if (split == -1) {
                valueBegin = split + 1
                valueEnd = to
                name = "value"
            } else {
                name = source.substring(from, split).trim()
                valueBegin = split + 1
                valueEnd = to
            }
            value = source.substring(valueBegin, valueEnd).trim()
            list.add(DefaultAnnotationAttribute.create(name, value))
        }
    }

    override fun toString(): String {
        return "DefaultAnnotationAttribute(name='$name', value=$value)"
    }
}

abstract class DefaultAnnotationValue : AnnotationAttributeValue {
    companion object {
        fun create(value: String): DefaultAnnotationValue {
            return if (value.startsWith("{")) { // Array
                DefaultAnnotationArrayAttributeValue(value)
            } else {
                DefaultAnnotationSingleAttributeValue(value)
            }
        }
    }

    override fun toString(): String = toSource()
}

class DefaultAnnotationSingleAttributeValue(override val valueSource: String) : DefaultAnnotationValue(),
    AnnotationSingleAttributeValue {
    @Suppress("IMPLICIT_CAST_TO_ANY")
    override val value = when {
        valueSource == SdkConstants.VALUE_TRUE -> true
        valueSource == SdkConstants.VALUE_FALSE -> false
        valueSource.startsWith("\"") -> valueSource.removeSurrounding("\"")
        valueSource.startsWith('\'') -> valueSource.removeSurrounding("'")[0]
        else -> try {
            if (valueSource.contains(".")) {
                valueSource.toDouble()
            } else {
                valueSource.toLong()
            }
        } catch (e: NumberFormatException) {
            valueSource
        }
    }

    override fun resolve(): Item? = null

    override fun toSource() = valueSource
}

class DefaultAnnotationArrayAttributeValue(val value: String) : DefaultAnnotationValue(),
    AnnotationArrayAttributeValue {
    init {
        assert(value.startsWith("{") && value.endsWith("}")) { value }
    }

    override val values = value.substring(1, value.length - 1).split(",").map {
        DefaultAnnotationValue.create(it.trim())
    }.toList()

    override fun toSource() = value
}
