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

package com.android.tools.metalava

const val COMPAT_MODE_BY_DEFAULT = true

/**
 * The old API generator code had a number of quirks. Initially we want to simulate these
 * quirks to produce compatible signature files and APIs, but we want to track what these quirks
 * are and be able to turn them off eventually. This class offers more fine grained control
 * of these compatibility behaviors such that we can enable/disable them selectively
 */
var compatibility: Compatibility = Compatibility()

class Compatibility(
    /** Whether compatibility is generally on */
    val compat: Boolean = COMPAT_MODE_BY_DEFAULT
) {

    /** In signature files, use "implements" instead of "extends" for the super class of
     * an interface */
    var extendsForInterfaceSuperClass: Boolean = compat

    /** In signature files, refer to annotations as an "abstract class" instead of an "@interface"
     * and implementing this interface: java.lang.annotation.Annotation */
    var classForAnnotations: Boolean = compat

    /** Add in explicit `valueOf` and `values` methods into enum classes */
    var defaultEnumMethods: Boolean = compat

    /** Whether signature files should contain annotation default values (as is already
     * done for field default values) */
    var includeAnnotationDefaults: Boolean = !compat

    /** In signature files, refer to enums as "class" instead of "enum" */
    var classForEnums: Boolean = compat

    /** Whether to use a nonstandard, compatibility modifier order instead of the Java canonical order.
     * ("deprecated" isn't a real modifier, so in "standard" mode it's listed first, as if it was the
     * `@Deprecated` annotation before the modifier list */
    var nonstandardModifierOrder: Boolean = compat

    /** Whether to include instance methods in annotation classes for the annotation properties */
    var skipAnnotationInstanceMethods: Boolean = compat

    /**
     * In signature files, whether interfaces should also be described as "abstract"
     */
    var abstractInInterfaces: Boolean = compat

    /**
     * In signature files, whether annotation types should also be described as "abstract"
     */
    var abstractInAnnotations: Boolean = compat

    /**
     * In signature files, whether interfaces can be listed as final
     */
    var finalInInterfaces: Boolean = compat

    /**
     * In this signature
     *        public <X extends Throwable> T orElseThrow(Supplier<? extends X> exceptionSupplier) throws X {
     *  doclava1 would treat this as "throws Throwable" instead of "throws X". This variable turns on
     *  this compat behavior.
     * */
    var useErasureInThrows: Boolean = compat

    /**
     * Whether throws classes in methods should be filtered. This should definitely
     * be the case, but doclava1 doesn't. Note that this only applies to signature
     * files, not stub files.
     */
    var filterThrowsClasses: Boolean = !compat

    /** Format `Map<K,V>` as `Map<K, V>` */
    var spaceAfterCommaInTypes: Boolean = compat

    /**
     * Doclava1 omits type parameters in interfaces (in signature files, not in stubs)
     */
    var omitTypeParametersInInterfaces: Boolean = compat

    /** Force methods named "values" in enums to be marked final. This was done by
     * doclava1 with this comment:
     *
     *     Explicitly coerce 'final' state of Java6-compiled enum values() method,
     *     to match the Java5-emitted base API description.
     *
     **/
    var forceFinalInEnumValueMethods: Boolean = compat

    /** Whether signature files and stubs should contain annotations */
    var annotationsInSignatures: Boolean = !compat

    /** Emit errors in the old API diff format */
    var oldErrorOutputFormat: Boolean = false

    /** Whether to include the exit <b>code</b> in the error output next to the id */
    var includeExitCode = oldErrorOutputFormat

    /**
     * When a public class implementing a public interface inherits the implementation
     * of a method in that interface from a hidden super class, the method must be
     * included in the stubs etc (since otherwise subclasses would believe they need
     * to implement that method and can't just inherit it). However, doclava1 does not
     * list these methods. This flag controls this compatibility behavior.
     * Not that this refers only to the signature files, not the stub file generation.
     *
     * An example is StringBuilder#setLength.
     */
    var skipInheritedMethods: Boolean = compat

    /**
     * Similar to [skipInheritedMethods], but for field constants.
     */
    var skipInheritedConstants: Boolean = compat

    /**
     * Whether to include parameter names in the signature file
     */
    var parameterNames: Boolean = !compat

    /**
     * *Some* signatures for doclava1 wrote "<?>" as "<? extends java.lang.Object>",
     * which is equivalent. Metalava does not do that. This flags ensures that the
     * signature files look like the old ones for the specific methods which did this.
     */
    var includeExtendsObjectInWildcard = compat

    /**
     * Whether deprecation should be shown in signature files as an annotation
     * instead of a pseudo-modifier
     */
    var deprecatedAsAnnotation = !compat

    /** Whether synchronized should be part of the output */
    var includeSynchronized = compat

    /** Whether we should omit common packages such as java.lang.* and kotlin.* from signature output */
    var omitCommonPackages = !compat

    /** Whether we should explicitly include retention when class even if not explicitly defined */
    var explicitlyListClassRetention = !compat

    /**
     * If true, a @Deprecated class will automatically deprecate all its inner classes
     * as well.
     */
    var propagateDeprecatedInnerClasses = !compat

    /**
     * If true, a @Deprecated class will automatically deprecate all members (not
     * including inner classes; for that see [propagateDeprecatedInnerClasses]) as well.
     */
    var propagateDeprecatedMembers = !compat

    /**
     * If an overriding method differs from its super method only by final or deprecated
     * and the containing class is final or deprecated, skip it in the signature file
     */
    var hideDifferenceImplicit = !compat

    /** Whether inner enums should be listed as static in the signature file. */
    var staticEnums = compat

    /**
     * The -new_api flag in API check (which generates an XML diff of the differences
     * between two APIs) in doclava was ignoring fields. This flag controls whether
     * we do the same.
     */
    var includeFieldsInApiDiff = !compat

    /**
     * Whether to escape the > character in JDiff XML files. The XML spec does not require
     * this but doclava does it.
     */
    var xmlEscapeGreaterThan = compat

    /**
     * When producing JDiff output for field arrays but where we
     * do not have the value, emit "null" into the JDiff report. This isn't right but matches
     * what doclava did.
     */
    var xmlShowArrayFieldsAsNull = compat

    /**
     * Doclava was missing enum fields in JDiff reports
     */
    var xmlSkipEnumFields = compat

    /**
     * Doclava was missing annotation instance methods in JDiff reports
     */
    var xmlSkipAnnotationMethods = compat

    /** Doclava lists character field values as integers instead of chars */
    var xmlCharAsInt = compat

    /**
     * Doclava listed the superclass of annotations as
     * java.lang.Object.
     */
    var xmlAnnotationAsObject = compat

    // Other examples: sometimes we sort by qualified name, sometimes by full name
}