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

import com.android.resources.ResourceType
import com.android.resources.ResourceType.AAPT
import com.android.resources.ResourceType.ANIM
import com.android.resources.ResourceType.ANIMATOR
import com.android.resources.ResourceType.ARRAY
import com.android.resources.ResourceType.ATTR
import com.android.resources.ResourceType.BOOL
import com.android.resources.ResourceType.COLOR
import com.android.resources.ResourceType.DIMEN
import com.android.resources.ResourceType.DRAWABLE
import com.android.resources.ResourceType.FONT
import com.android.resources.ResourceType.FRACTION
import com.android.resources.ResourceType.ID
import com.android.resources.ResourceType.INTEGER
import com.android.resources.ResourceType.INTERPOLATOR
import com.android.resources.ResourceType.LAYOUT
import com.android.resources.ResourceType.MENU
import com.android.resources.ResourceType.MIPMAP
import com.android.resources.ResourceType.NAVIGATION
import com.android.resources.ResourceType.PLURALS
import com.android.resources.ResourceType.PUBLIC
import com.android.resources.ResourceType.RAW
import com.android.resources.ResourceType.SAMPLE_DATA
import com.android.resources.ResourceType.STRING
import com.android.resources.ResourceType.STYLE
import com.android.resources.ResourceType.STYLEABLE
import com.android.resources.ResourceType.STYLE_ITEM
import com.android.resources.ResourceType.TRANSITION
import com.android.resources.ResourceType.XML
import com.android.sdklib.SdkVersionInfo
import com.android.tools.metalava.doclava1.Issues.ABSTRACT_INNER
import com.android.tools.metalava.doclava1.Issues.ACRONYM_NAME
import com.android.tools.metalava.doclava1.Issues.ACTION_VALUE
import com.android.tools.metalava.doclava1.Issues.ALL_UPPER
import com.android.tools.metalava.doclava1.Issues.ANDROID_URI
import com.android.tools.metalava.doclava1.Issues.ARRAY_RETURN
import com.android.tools.metalava.doclava1.Issues.AUTO_BOXING
import com.android.tools.metalava.doclava1.Issues.BAD_FUTURE
import com.android.tools.metalava.doclava1.Issues.BANNED_THROW
import com.android.tools.metalava.doclava1.Issues.BUILDER_SET_STYLE
import com.android.tools.metalava.doclava1.Issues.CALLBACK_INTERFACE
import com.android.tools.metalava.doclava1.Issues.CALLBACK_METHOD_NAME
import com.android.tools.metalava.doclava1.Issues.CALLBACK_NAME
import com.android.tools.metalava.doclava1.Issues.COMMON_ARGS_FIRST
import com.android.tools.metalava.doclava1.Issues.COMPILE_TIME_CONSTANT
import com.android.tools.metalava.doclava1.Issues.CONCRETE_COLLECTION
import com.android.tools.metalava.doclava1.Issues.CONFIG_FIELD_NAME
import com.android.tools.metalava.doclava1.Issues.CONSISTENT_ARGUMENT_ORDER
import com.android.tools.metalava.doclava1.Issues.CONTEXT_FIRST
import com.android.tools.metalava.doclava1.Issues.CONTEXT_NAME_SUFFIX
import com.android.tools.metalava.doclava1.Issues.ENDS_WITH_IMPL
import com.android.tools.metalava.doclava1.Issues.ENUM
import com.android.tools.metalava.doclava1.Issues.EQUALS_AND_HASH_CODE
import com.android.tools.metalava.doclava1.Issues.EXCEPTION_NAME
import com.android.tools.metalava.doclava1.Issues.EXECUTOR_REGISTRATION
import com.android.tools.metalava.doclava1.Issues.EXTENDS_ERROR
import com.android.tools.metalava.doclava1.Issues.Issue
import com.android.tools.metalava.doclava1.Issues.FORBIDDEN_SUPER_CLASS
import com.android.tools.metalava.doclava1.Issues.FRACTION_FLOAT
import com.android.tools.metalava.doclava1.Issues.GENERIC_EXCEPTION
import com.android.tools.metalava.doclava1.Issues.GETTER_SETTER_NAMES
import com.android.tools.metalava.doclava1.Issues.HEAVY_BIT_SET
import com.android.tools.metalava.doclava1.Issues.ILLEGAL_STATE_EXCEPTION
import com.android.tools.metalava.doclava1.Issues.INTENT_BUILDER_NAME
import com.android.tools.metalava.doclava1.Issues.INTENT_NAME
import com.android.tools.metalava.doclava1.Issues.INTERFACE_CONSTANT
import com.android.tools.metalava.doclava1.Issues.INTERNAL_CLASSES
import com.android.tools.metalava.doclava1.Issues.INTERNAL_FIELD
import com.android.tools.metalava.doclava1.Issues.KOTLIN_OPERATOR
import com.android.tools.metalava.doclava1.Issues.LISTENER_INTERFACE
import com.android.tools.metalava.doclava1.Issues.LISTENER_LAST
import com.android.tools.metalava.doclava1.Issues.MANAGER_CONSTRUCTOR
import com.android.tools.metalava.doclava1.Issues.MANAGER_LOOKUP
import com.android.tools.metalava.doclava1.Issues.MENTIONS_GOOGLE
import com.android.tools.metalava.doclava1.Issues.METHOD_NAME_TENSE
import com.android.tools.metalava.doclava1.Issues.METHOD_NAME_UNITS
import com.android.tools.metalava.doclava1.Issues.MIN_MAX_CONSTANT
import com.android.tools.metalava.doclava1.Issues.MISSING_BUILD_METHOD
import com.android.tools.metalava.doclava1.Issues.MISSING_NULLABILITY
import com.android.tools.metalava.doclava1.Issues.MUTABLE_BARE_FIELD
import com.android.tools.metalava.doclava1.Issues.NOT_CLOSEABLE
import com.android.tools.metalava.doclava1.Issues.NO_BYTE_OR_SHORT
import com.android.tools.metalava.doclava1.Issues.NO_CLONE
import com.android.tools.metalava.doclava1.Issues.ON_NAME_EXPECTED
import com.android.tools.metalava.doclava1.Issues.OVERLAPPING_CONSTANTS
import com.android.tools.metalava.doclava1.Issues.PACKAGE_LAYERING
import com.android.tools.metalava.doclava1.Issues.PAIRED_REGISTRATION
import com.android.tools.metalava.doclava1.Issues.PARCELABLE_LIST
import com.android.tools.metalava.doclava1.Issues.PARCEL_CONSTRUCTOR
import com.android.tools.metalava.doclava1.Issues.PARCEL_CREATOR
import com.android.tools.metalava.doclava1.Issues.PARCEL_NOT_FINAL
import com.android.tools.metalava.doclava1.Issues.PERCENTAGE_INT
import com.android.tools.metalava.doclava1.Issues.PROTECTED_MEMBER
import com.android.tools.metalava.doclava1.Issues.PUBLIC_TYPEDEF
import com.android.tools.metalava.doclava1.Issues.RAW_AIDL
import com.android.tools.metalava.doclava1.Issues.REGISTRATION_NAME
import com.android.tools.metalava.doclava1.Issues.RESOURCE_FIELD_NAME
import com.android.tools.metalava.doclava1.Issues.RESOURCE_STYLE_FIELD_NAME
import com.android.tools.metalava.doclava1.Issues.RESOURCE_VALUE_FIELD_NAME
import com.android.tools.metalava.doclava1.Issues.RETHROW_REMOTE_EXCEPTION
import com.android.tools.metalava.doclava1.Issues.SERVICE_NAME
import com.android.tools.metalava.doclava1.Issues.SETTER_RETURNS_THIS
import com.android.tools.metalava.doclava1.Issues.SINGLETON_CONSTRUCTOR
import com.android.tools.metalava.doclava1.Issues.SINGLE_METHOD_INTERFACE
import com.android.tools.metalava.doclava1.Issues.SINGULAR_CALLBACK
import com.android.tools.metalava.doclava1.Issues.START_WITH_LOWER
import com.android.tools.metalava.doclava1.Issues.START_WITH_UPPER
import com.android.tools.metalava.doclava1.Issues.STATIC_UTILS
import com.android.tools.metalava.doclava1.Issues.STREAM_FILES
import com.android.tools.metalava.doclava1.Issues.TOP_LEVEL_BUILDER
import com.android.tools.metalava.doclava1.Issues.UNIQUE_KOTLIN_OPERATOR
import com.android.tools.metalava.doclava1.Issues.USER_HANDLE
import com.android.tools.metalava.doclava1.Issues.USER_HANDLE_NAME
import com.android.tools.metalava.doclava1.Issues.USE_ICU
import com.android.tools.metalava.doclava1.Issues.USE_PARCEL_FILE_DESCRIPTOR
import com.android.tools.metalava.doclava1.Issues.VISIBLY_SYNCHRONIZED
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.AnnotationItem.Companion.getImplicitNullness
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.ConstructorItem
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MemberItem
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.model.ParameterItem
import com.android.tools.metalava.model.TypeItem
import com.android.tools.metalava.model.SetMinSdkVersion
import com.android.tools.metalava.model.psi.PsiMethodItem
import com.android.tools.metalava.model.visitors.ApiVisitor
import com.intellij.psi.JavaRecursiveElementVisitor
import com.intellij.psi.PsiClassObjectAccessExpression
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiSynchronizedStatement
import com.intellij.psi.PsiThisExpression
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.UClassLiteralExpression
import org.jetbrains.uast.UMethod
import org.jetbrains.uast.UQualifiedReferenceExpression
import org.jetbrains.uast.UThisExpression
import org.jetbrains.uast.visitor.AbstractUastVisitor
import java.util.Locale
import java.util.function.Predicate

/**
 * The [ApiLint] analyzer checks the API against a known set of preferred API practices
 * by the Android API council.
 */
class ApiLint(private val codebase: Codebase, private val oldCodebase: Codebase?, private val reporter: Reporter) : ApiVisitor(
    // Sort by source order such that warnings follow source line number order
    methodComparator = MethodItem.sourceOrderComparator,
    fieldComparator = FieldItem.comparator,
    ignoreShown = options.showUnannotated
) {
    private fun report(id: Issue, item: Item, message: String, element: PsiElement? = null) {
        // Don't flag api warnings on deprecated APIs; these are obviously already known to
        // be problematic.
        if (item.deprecated) {
            return
        }

        // With show annotations we might be flagging API that is filtered out: hide these here
        val testItem = if (item is ParameterItem) item.containingMethod() else item
        if (!filterEmit.test(testItem)) {
            return
        }

        reporter.report(id, item, message, element)
    }

    private fun check() {
        val prevCount = reporter.totalCount

        if (oldCodebase != null) {
            // Only check the new APIs
            CodebaseComparator().compare(object : ComparisonVisitor() {
                override fun added(new: Item) {
                    new.accept(this@ApiLint)
                }
            }, oldCodebase, codebase, filterReference)
        } else {
            // No previous codebase to compare with: visit the whole thing
            codebase.accept(this)
        }

        val apiLintIssues = reporter.totalCount - prevCount
        if (apiLintIssues > 0) {
            // We've reported API lint violations; emit some verbiage to explain
            // how to suppress the error rules.
            options.stdout.println("\n$apiLintIssues new API lint issues were found.")
            val baseline = options.baseline
            if (baseline?.updateFile != null && baseline.file != null && !baseline.silentUpdate) {
                options.stdout.println("""
                ************************************************************
                Your API changes are triggering API Lint warnings or errors.
                To make these errors go away, fix the code according to the
                error and/or warning messages above.

                If it's not possible to do so, there are two workarounds:

                1. You can suppress the errors with @SuppressLint("<id>")
                2. You can update the baseline by executing the following
                   command:
                       cp \
                       ${baseline.updateFile} \
                       ${baseline.file}
                   To submit the revised baseline.txt to the main Android
                   repository, you will need approval.
                ************************************************************
                """.trimIndent())
            } else {
                options.stdout.println("See tools/metalava/API-LINT.md for how to handle these.")
            }
        }
    }

    override fun skip(item: Item): Boolean {
        return super.skip(item) || item is ClassItem && !isInteresting(item)
    }

    // The previous Kotlin interop tests are also part of API lint now (though they can be
    // run independently as well; therefore, only run them here if not running separately)
    private val kotlinInterop = if (!options.checkKotlinInterop) KotlinInteropChecks(reporter) else null

    override fun visitClass(cls: ClassItem) {
        val methods = cls.filteredMethods(filterReference).asSequence()
        val fields = cls.filteredFields(filterReference, showUnannotated).asSequence()
        val constructors = cls.filteredConstructors(filterReference)
        val superClass = cls.filteredSuperclass(filterReference)
        val interfaces = cls.filteredInterfaceTypes(filterReference).asSequence()
        val allMethods = methods.asSequence() + constructors.asSequence()
        checkClass(
            cls, methods, constructors, allMethods, fields, superClass, interfaces,
            filterReference
        )
    }

    override fun visitMethod(method: MethodItem) {
        checkMethod(method, filterReference)
        val returnType = method.returnType()
        if (returnType != null) {
            checkType(returnType, method)
        }
        for (parameter in method.parameters()) {
            checkType(parameter.type(), parameter)
        }
        kotlinInterop?.checkMethod(method)
    }

    override fun visitField(field: FieldItem) {
        checkField(field)
        checkType(field.type(), field)
        kotlinInterop?.checkField(field)
    }

    private fun checkType(type: TypeItem, item: Item) {
        val typeString = type.toTypeString()
        checkPfd(typeString, item)
        checkNumbers(typeString, item)
        checkCollections(type, item)
        checkCollectionsOverArrays(type, typeString, item)
        checkBoxed(type, item)
        checkIcu(type, typeString, item)
        checkBitSet(type, typeString, item)
        checkHasNullability(item)
        checkUri(typeString, item)
        checkFutures(typeString, item)
    }

    private fun checkClass(
        cls: ClassItem,
        methods: Sequence<MethodItem>,
        constructors: Sequence<ConstructorItem>,
        methodsAndConstructors: Sequence<MethodItem>,
        fields: Sequence<FieldItem>,
        superClass: ClassItem?,
        interfaces: Sequence<TypeItem>,
        filterReference: Predicate<Item>
    ) {
        checkEquals(methods)
        checkEnums(cls)
        checkClassNames(cls)
        checkCallbacks(cls, methods)
        checkListeners(cls, methods)
        checkParcelable(cls, methods, constructors, fields)
        checkRegistrationMethods(cls, methods)
        checkHelperClasses(cls, methods, fields)
        checkBuilder(cls, methods, superClass)
        checkAidl(cls, superClass, interfaces)
        checkInternal(cls)
        checkLayering(cls, methodsAndConstructors, fields)
        checkBooleans(methods)
        checkFlags(fields)
        checkGoogle(cls, methods, fields)
        checkManager(cls, methods, constructors)
        checkStaticUtils(cls, methods, constructors, fields)
        checkCallbackHandlers(cls, methodsAndConstructors, superClass)
        checkResourceNames(cls, fields)
        checkFiles(methodsAndConstructors)
        checkManagerList(cls, methods)
        checkAbstractInner(cls)
        checkRuntimeExceptions(methodsAndConstructors, filterReference)
        checkError(cls, superClass)
        checkCloseable(cls, methods)
        checkNotKotlinOperator(methods)
        checkUserHandle(cls, methods)
        checkParams(cls)
        checkSingleton(cls, methods, constructors)
        checkExtends(cls)
        checkTypedef(cls)

        // TODO: Not yet working
        // checkOverloadArgs(cls, methods)
    }

    private fun checkField(
        field: FieldItem
    ) {
        val modifiers = field.modifiers
        if (modifiers.isStatic() && modifiers.isFinal()) {
            checkConstantNames(field)
            checkActions(field)
            checkIntentExtras(field)
        }
        checkProtected(field)
        checkServices(field)
        checkFieldName(field)
    }

    private fun checkMethod(
        method: MethodItem,
        filterReference: Predicate<Item>
    ) {
        if (!method.isConstructor()) {
            checkMethodNames(method)
            checkProtected(method)
            checkSynchronized(method)
            checkIntentBuilder(method)
            checkUnits(method)
            checkTense(method)
            checkClone(method)
        }
        checkExceptions(method, filterReference)
        checkContextFirst(method)
        checkListenerLast(method)
    }

    private fun checkEnums(cls: ClassItem) {
        /*
            def verify_enums(clazz):
                """Enums are bad, mmkay?"""
                if "extends java.lang.Enum" in clazz.raw:
                    error(clazz, None, "F5", "Enums are not allowed")
         */
        if (cls.isEnum()) {
            report(ENUM, cls, "Enums are discouraged in Android APIs")
        }
    }

    private fun checkMethodNames(method: MethodItem) {
        /*
            def verify_method_names(clazz):
                """Try catching malformed method names, like Foo() or getMTU()."""
                if clazz.fullname.startswith("android.opengl"): return
                if clazz.fullname.startswith("android.renderscript"): return
                if clazz.fullname == "android.system.OsConstants": return

                for m in clazz.methods:
                    if re.search("[A-Z]{2,}", m.name) is not None:
                        warn(clazz, m, "S1", "Method names with acronyms should be getMtu() instead of getMTU()")
                    if re.match("[^a-z]", m.name):
                        error(clazz, m, "S1", "Method name must start with lowercase char")
         */

        // Existing violations
        val containing = method.containingClass().qualifiedName()
        if (containing.startsWith("android.opengl") ||
            containing.startsWith("android.renderscript") ||
            containing.startsWith("android.database.sqlite.") ||
            containing == "android.system.OsConstants"
        ) {
            return
        }

        val name = method.name()
        val first = name[0]

        when {
            first !in 'a'..'z' -> report(START_WITH_LOWER, method, "Method name must start with lowercase char: $name")
            hasAcronyms(name) -> {
                report(
                    ACRONYM_NAME, method,
                    "Acronyms should not be capitalized in method names: was `$name`, should this be `${decapitalizeAcronyms(
                        name
                    )}`?"
                )
            }
        }
    }

    private fun checkClassNames(cls: ClassItem) {
        /*
            def verify_class_names(clazz):
                """Try catching malformed class names like myMtp or MTPUser."""
                if clazz.fullname.startswith("android.opengl"): return
                if clazz.fullname.startswith("android.renderscript"): return
                if re.match("android\.R\.[a-z]+", clazz.fullname): return

                if re.search("[A-Z]{2,}", clazz.name) is not None:
                    warn(clazz, None, "S1", "Class names with acronyms should be Mtp not MTP")
                if re.match("[^A-Z]", clazz.name):
                    error(clazz, None, "S1", "Class must start with uppercase char")
                if clazz.name.endswith("Impl"):
                    error(clazz, None, None, "Don't expose your implementation details")
         */

        // Existing violations
        val qualifiedName = cls.qualifiedName()
        if (qualifiedName.startsWith("android.opengl") ||
            qualifiedName.startsWith("android.renderscript") ||
            qualifiedName.startsWith("android.database.sqlite.") ||
            qualifiedName.startsWith("android.R.")
        ) {
            return
        }

        val name = cls.simpleName()
        val first = name[0]
        when {
            first !in 'A'..'Z' -> {
                report(
                    START_WITH_UPPER, cls,
                    "Class must start with uppercase char: $name"
                )
            }
            hasAcronyms(name) -> {
                report(
                    ACRONYM_NAME, cls,
                    "Acronyms should not be capitalized in class names: was `$name`, should this be `${decapitalizeAcronyms(
                        name
                    )}`?"
                )
            }
            name.endsWith("Impl") -> {
                report(
                    ENDS_WITH_IMPL, cls,
                    "Don't expose your implementation details: `$name` ends with `Impl`"
                )
            }
        }
    }

    private fun checkConstantNames(field: FieldItem) {
        /*
            def verify_constants(clazz):
                """All static final constants must be FOO_NAME style."""
                if re.match("android\.R\.[a-z]+", clazz.fullname): return
                if clazz.fullname.startswith("android.os.Build"): return
                if clazz.fullname == "android.system.OsConstants": return

                req = ["java.lang.String","byte","short","int","long","float","double","boolean","char"]
                for f in clazz.fields:
                    if "static" in f.split and "final" in f.split:
                        if re.match("[A-Z0-9_]+", f.name) is None:
                            error(clazz, f, "C2", "Constant field names must be FOO_NAME")
                        if f.typ != "java.lang.String":
                            if f.name.startswith("MIN_") or f.name.startswith("MAX_"):
                                warn(clazz, f, "C8", "If min/max could change in future, make them dynamic methods")
                        if f.typ in req and f.value is None:
                            error(clazz, f, None, "All constants must be defined at compile time")
         */
        // Existing violations
        val qualified = field.containingClass().qualifiedName()
        if (qualified.startsWith("android.os.Build") ||
            qualified == "android.system.OsConstants" ||
            qualified == "android.media.MediaCodecInfo" ||
            qualified.startsWith("android.opengl.") ||
            qualified.startsWith("android.R.")
        ) {
            return
        }

        val name = field.name()
        if (!constantNamePattern.matches(name)) {
            val suggested = SdkVersionInfo.camelCaseToUnderlines(name).toUpperCase(Locale.US)
            report(
                ALL_UPPER, field,
                "Constant field names must be named with only upper case characters: `$qualified#$name`, should be `$suggested`?"
            )
        } else if ((name.startsWith("MIN_") || name.startsWith("MAX_")) && !field.type().isString()) {
            report(
                MIN_MAX_CONSTANT, field,
                "If min/max could change in future, make them dynamic methods: $qualified#$name"
            )
        } else if ((field.type().primitive || field.type().isString()) && field.initialValue(true) == null) {
            report(
                COMPILE_TIME_CONSTANT, field,
                "All constants must be defined at compile time: $qualified#$name"
            )
        }
    }

    private fun checkCallbacks(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_callbacks(clazz):
                """Verify Callback classes.
                All callback classes must be abstract.
                All methods must follow onFoo() naming style."""
                if clazz.fullname == "android.speech.tts.SynthesisCallback": return

                if clazz.name.endswith("Callbacks"):
                    error(clazz, None, "L1", "Callback class names should be singular")
                if clazz.name.endswith("Observer"):
                    warn(clazz, None, "L1", "Class should be named FooCallback")

                if clazz.name.endswith("Callback"):
                    if "interface" in clazz.split:
                        error(clazz, None, "CL3", "Callbacks must be abstract class to enable extension in future API levels")

                    for m in clazz.methods:
                        if not re.match("on[A-Z][a-z]*", m.name):
                            error(clazz, m, "L1", "Callback method names must be onFoo() style")

            )
         */

        // Existing violations
        val qualified = cls.qualifiedName()
        if (qualified == "android.speech.tts.SynthesisCallback") {
            return
        }

        val name = cls.simpleName()
        when {
            name.endsWith("Callbacks") -> {
                report(
                    SINGULAR_CALLBACK, cls,
                    "Callback class names should be singular: $name"
                )
            }
            name.endsWith("Observer") -> {
                val prefix = name.removeSuffix("Observer")
                report(
                    CALLBACK_NAME, cls,
                    "Class should be named ${prefix}Callback"
                )
            }
            name.endsWith("Callback") -> {
                if (cls.isInterface()) {
                    report(
                        CALLBACK_INTERFACE, cls,
                        "Callbacks must be abstract class instead of interface to enable extension in future API levels: $name"
                    )
                } else {
                    for (method in methods) {
                        val methodName = method.name()
                        if (!onCallbackNamePattern.matches(methodName)) {
                            report(
                                CALLBACK_METHOD_NAME, cls,
                                "Callback method names must follow the on<Something> style: $methodName"
                            )
                        }
                    }
                }
            }
        }
    }

    private fun checkListeners(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_listeners(clazz):
                """Verify Listener classes.
                All Listener classes must be interface.
                All methods must follow onFoo() naming style.
                If only a single method, it must match class name:
                    interface OnFooListener { void onFoo() }"""

                if clazz.name.endswith("Listener"):
                    if " abstract class " in clazz.raw:
                        error(clazz, None, "L1", "Listeners should be an interface, or otherwise renamed Callback")

                    for m in clazz.methods:
                        if not re.match("on[A-Z][a-z]*", m.name):
                            error(clazz, m, "L1", "Listener method names must be onFoo() style")

                    if len(clazz.methods) == 1 and clazz.name.startswith("On"):
                        m = clazz.methods[0]
                        if (m.name + "Listener").lower() != clazz.name.lower():
                            error(clazz, m, "L1", "Single listener method name must match class name")
         */

        val name = cls.simpleName()
        if (name.endsWith("Listener")) {
            if (cls.isClass()) {
                report(
                    LISTENER_INTERFACE, cls,
                    "Listeners should be an interface, or otherwise renamed Callback: $name"
                )
            } else {
                for (method in methods) {
                    val methodName = method.name()
                    if (!onCallbackNamePattern.matches(methodName)) {
                        report(
                            CALLBACK_METHOD_NAME, cls,
                            "Listener method names must follow the on<Something> style: $methodName"
                        )
                    }
                }

                if (methods.count() == 1) {
                    val method = methods.first()
                    val methodName = method.name()
                    if (methodName.startsWith("On") &&
                        !("${methodName}Listener").equals(cls.simpleName(), ignoreCase = true)
                    ) {
                        report(
                            SINGLE_METHOD_INTERFACE, cls,
                            "Single listener method name must match class name"
                        )
                    }
                }
            }
        }
    }

    private fun checkActions(field: FieldItem) {
        /*
            def verify_actions(clazz):
                """Verify intent actions.
                All action names must be named ACTION_FOO.
                All action values must be scoped by package and match name:
                    package android.foo {
                        String ACTION_BAR = "android.foo.action.BAR";
                    }"""
                for f in clazz.fields:
                    if f.value is None: continue
                    if f.name.startswith("EXTRA_"): continue
                    if f.name == "SERVICE_INTERFACE" or f.name == "PROVIDER_INTERFACE": continue
                    if "INTERACTION" in f.name: continue

                    if "static" in f.split and "final" in f.split and f.typ == "java.lang.String":
                        if "_ACTION" in f.name or "ACTION_" in f.name or ".action." in f.value.lower():
                            if not f.name.startswith("ACTION_"):
                                error(clazz, f, "C3", "Intent action constant name must be ACTION_FOO")
                            else:
                                if clazz.fullname == "android.content.Intent":
                                    prefix = "android.intent.action"
                                elif clazz.fullname == "android.provider.Settings":
                                    prefix = "android.settings"
                                elif clazz.fullname == "android.app.admin.DevicePolicyManager" or clazz.fullname == "android.app.admin.DeviceAdminReceiver":
                                    prefix = "android.app.action"
                                else:
                                    prefix = clazz.pkg.name + ".action"
                                expected = prefix + "." + f.name[7:]
                                if f.value != expected:
                                    error(clazz, f, "C4", "Inconsistent action value; expected '%s'" % (expected))
         */

        val name = field.name()
        if (name.startsWith("EXTRA_") || name == "SERVICE_INTERFACE" || name == "PROVIDER_INTERFACE") {
            return
        }
        if (!field.type().isString()) {
            return
        }
        val value = field.initialValue(true) as? String ?: return
        if (!(name.contains("_ACTION") || name.contains("ACTION_") || value.contains(".action."))) {
            return
        }
        if (!name.startsWith("ACTION_")) {
            report(
                INTENT_NAME, field,
                "Intent action constant name must be ACTION_FOO: $name"
            )
            return
        }
        val className = field.containingClass().qualifiedName()
        val prefix = when (className) {
            "android.content.Intent" -> "android.intent.action"
            "android.provider.Settings" -> "android.settings"
            "android.app.admin.DevicePolicyManager", "android.app.admin.DeviceAdminReceiver" -> "android.app.action"
            else -> field.containingClass().containingPackage().qualifiedName() + ".action"
        }
        val expected = prefix + "." + name.substring(7)
        if (value != expected) {
            report(
                ACTION_VALUE, field,
                "Inconsistent action value; expected `$expected`, was `$value`"
            )
        }
    }

    private fun checkIntentExtras(field: FieldItem) {
        /*
            def verify_extras(clazz):
                """Verify intent extras.
                All extra names must be named EXTRA_FOO.
                All extra values must be scoped by package and match name:
                    package android.foo {
                        String EXTRA_BAR = "android.foo.extra.BAR";
                    }"""
                if clazz.fullname == "android.app.Notification": return
                if clazz.fullname == "android.appwidget.AppWidgetManager": return

                for f in clazz.fields:
                    if f.value is None: continue
                    if f.name.startswith("ACTION_"): continue

                    if "static" in f.split and "final" in f.split and f.typ == "java.lang.String":
                        if "_EXTRA" in f.name or "EXTRA_" in f.name or ".extra" in f.value.lower():
                            if not f.name.startswith("EXTRA_"):
                                error(clazz, f, "C3", "Intent extra must be EXTRA_FOO")
                            else:
                                if clazz.pkg.name == "android.content" and clazz.name == "Intent":
                                    prefix = "android.intent.extra"
                                elif clazz.pkg.name == "android.app.admin":
                                    prefix = "android.app.extra"
                                else:
                                    prefix = clazz.pkg.name + ".extra"
                                expected = prefix + "." + f.name[6:]
                                if f.value != expected:
                                    error(clazz, f, "C4", "Inconsistent extra value; expected '%s'" % (expected))


         */
        val className = field.containingClass().qualifiedName()
        if (className == "android.app.Notification" || className == "android.appwidget.AppWidgetManager") {
            return
        }

        val name = field.name()
        if (name.startsWith("ACTION_") || !field.type().isString()) {
            return
        }
        val value = field.initialValue(true) as? String ?: return
        if (!(name.contains("_EXTRA") || name.contains("EXTRA_") || value.contains(".extra"))) {
            return
        }
        if (!name.startsWith("EXTRA_")) {
            report(
                INTENT_NAME, field,
                "Intent extra constant name must be EXTRA_FOO: $name"
            )
            return
        }

        val packageName = field.containingClass().containingPackage().qualifiedName()
        val prefix = when {
            className == "android.content.Intent" -> "android.intent.extra"
            packageName == "android.app.admin" -> "android.app.extra"
            else -> "$packageName.extra"
        }
        val expected = prefix + "." + name.substring(6)
        if (value != expected) {
            report(
                ACTION_VALUE, field,
                "Inconsistent extra value; expected `$expected`, was `$value`"
            )
        }
    }

    private fun checkEquals(methods: Sequence<MethodItem>) {
        /*
            def verify_equals(clazz):
                """Verify that equals() and hashCode() must be overridden together."""
                eq = False
                hc = False
                for m in clazz.methods:
                    if " static " in m.raw: continue
                    if "boolean equals(java.lang.Object)" in m.raw: eq = True
                    if "int hashCode()" in m.raw: hc = True
                if eq != hc:
                    error(clazz, None, "M8", "Must override both equals and hashCode; missing one")
         */
        var equalsMethod: MethodItem? = null
        var hashCodeMethod: MethodItem? = null

        for (method in methods) {
            if (isEqualsMethod(method)) {
                equalsMethod = method
            } else if (isHashCodeMethod(method)) {
                hashCodeMethod = method
            }
        }
        if ((equalsMethod == null) != (hashCodeMethod == null)) {
            val method = equalsMethod ?: hashCodeMethod!!
            report(
                EQUALS_AND_HASH_CODE, method,
                "Must override both equals and hashCode; missing one in ${method.containingClass().qualifiedName()}"
            )
        }
    }

    private fun isEqualsMethod(method: MethodItem): Boolean {
        return method.name() == "equals" && method.parameters().size == 1 &&
            method.parameters()[0].type().isJavaLangObject() &&
            !method.modifiers.isStatic()
    }

    private fun isHashCodeMethod(method: MethodItem): Boolean {
        return method.name() == "hashCode" && method.parameters().isEmpty() &&
            !method.modifiers.isStatic()
    }

    private fun checkParcelable(
        cls: ClassItem,
        methods: Sequence<MethodItem>,
        constructors: Sequence<MethodItem>,
        fields: Sequence<FieldItem>
    ) {
        /*
            def verify_parcelable(clazz):
                """Verify that Parcelable objects aren't hiding required bits."""
                if "implements android.os.Parcelable" in clazz.raw:
                    creator = [ i for i in clazz.fields if i.name == "CREATOR" ]
                    write = [ i for i in clazz.methods if i.name == "writeToParcel" ]
                    describe = [ i for i in clazz.methods if i.name == "describeContents" ]

                    if len(creator) == 0 or len(write) == 0 or len(describe) == 0:
                        error(clazz, None, "FW3", "Parcelable requires CREATOR, writeToParcel, and describeContents; missing one")

                    if ((" final class " not in clazz.raw) and
                        (" final deprecated class " not in clazz.raw)):
                        error(clazz, None, "FW8", "Parcelable classes must be final")

                    for c in clazz.ctors:
                        if c.args == ["android.os.Parcel"]:
                            error(clazz, c, "FW3", "Parcelable inflation is exposed through CREATOR, not raw constructors")
         */

        if (!cls.implements("android.os.Parcelable")) {
            return
        }

        if (fields.none { it.name() == "CREATOR" }) {
            report(
                PARCEL_CREATOR, cls,
                "Parcelable requires a `CREATOR` field; missing in ${cls.qualifiedName()}"
            )
        }
        if (methods.none { it.name() == "writeToParcel" }) {
            report(
                PARCEL_CREATOR, cls,
                "Parcelable requires `void writeToParcel(Parcel, int)`; missing in ${cls.qualifiedName()}"
            )
        }
        if (methods.none { it.name() == "describeContents" }) {
            report(
                PARCEL_CREATOR, cls,
                "Parcelable requires `public int describeContents()`; missing in ${cls.qualifiedName()}"
            )
        }

        if (!cls.modifiers.isFinal()) {
            report(
                PARCEL_NOT_FINAL, cls,
                "Parcelable classes must be final: ${cls.qualifiedName()} is not final"
            )
        }

        val parcelConstructor = constructors.firstOrNull {
            val parameters = it.parameters()
            parameters.size == 1 && parameters[0].type().toTypeString() == "android.os.Parcel"
        }

        if (parcelConstructor != null) {
            report(
                PARCEL_CONSTRUCTOR, parcelConstructor,
                "Parcelable inflation is exposed through CREATOR, not raw constructors, in ${cls.qualifiedName()}"
            )
        }
    }

    private fun checkProtected(member: MemberItem) {
        /*
        def verify_protected(clazz):
            """Verify that no protected methods or fields are allowed."""
            for m in clazz.methods:
                if m.name == "finalize": continue
                if "protected" in m.split:
                    error(clazz, m, "M7", "Protected methods not allowed; must be public")
            for f in clazz.fields:
                if "protected" in f.split:
                    error(clazz, f, "M7", "Protected fields not allowed; must be public")
         */
        val modifiers = member.modifiers
        if (modifiers.isProtected()) {
            if (member.name() == "finalize" && member is MethodItem && member.parameters().isEmpty()) {
                return
            }

            report(
                PROTECTED_MEMBER, member,
                "Protected ${if (member is MethodItem) "methods" else "fields"} not allowed; must be public: ${member.describe()}}"
            )
        }
    }

    private fun checkFieldName(field: FieldItem) {
        /*
        def verify_fields(clazz):
            """Verify that all exposed fields are final.
            Exposed fields must follow myName style.
            Catch internal mFoo objects being exposed."""

            IGNORE_BARE_FIELDS = [
                "android.app.ActivityManager.RecentTaskInfo",
                "android.app.Notification",
                "android.content.pm.ActivityInfo",
                "android.content.pm.ApplicationInfo",
                "android.content.pm.ComponentInfo",
                "android.content.pm.ResolveInfo",
                "android.content.pm.FeatureGroupInfo",
                "android.content.pm.InstrumentationInfo",
                "android.content.pm.PackageInfo",
                "android.content.pm.PackageItemInfo",
                "android.content.res.Configuration",
                "android.graphics.BitmapFactory.Options",
                "android.os.Message",
                "android.system.StructPollfd",
            ]

            for f in clazz.fields:
                if not "final" in f.split:
                    if clazz.fullname in IGNORE_BARE_FIELDS:
                        pass
                    elif clazz.fullname.endswith("LayoutParams"):
                        pass
                    elif clazz.fullname.startswith("android.util.Mutable"):
                        pass
                    else:
                        error(clazz, f, "F2", "Bare fields must be marked final, or add accessors if mutable")

                if "static" not in f.split and "property" not in f.split:
                    if not re.match("[a-z]([a-zA-Z]+)?", f.name):
                        error(clazz, f, "S1", "Non-static fields must be named using myField style")

                if re.match("[ms][A-Z]", f.name):
                    error(clazz, f, "F1", "Internal objects must not be exposed")

                if re.match("[A-Z_]+", f.name):
                    if "static" not in f.split or "final" not in f.split:
                        error(clazz, f, "C2", "Constants must be marked static final")
         */
        val className = field.containingClass().qualifiedName()
        val modifiers = field.modifiers
        if (!modifiers.isFinal()) {
            if (className !in classesWithBareFields &&
                    !className.endsWith("LayoutParams") &&
                    !className.startsWith("android.util.Mutable")) {
                report(MUTABLE_BARE_FIELD, field,
                        "Bare field ${field.name()} must be marked final, or moved behind accessors if mutable")
            }
        }
        if (!modifiers.isStatic()) {
            if (!fieldNamePattern.matches(field.name())) {
                report(START_WITH_LOWER, field,
                        "Non-static field ${field.name()} must be named using fooBar style")
            }
        }
        if (internalNamePattern.matches(field.name())) {
            report(INTERNAL_FIELD, field,
                    "Internal field ${field.name()} must not be exposed")
        }
        if (constantNamePattern.matches(field.name())) {
            if (!modifiers.isStatic() || !modifiers.isFinal()) {
                report(ALL_UPPER, field,
                        "Constant ${field.name()} must be marked static final")
            }
        }
    }

    private fun checkRegistrationMethods(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_register(clazz):
                """Verify parity of registration methods.
                Callback objects use register/unregister methods.
                Listener objects use add/remove methods."""
                methods = [ m.name for m in clazz.methods ]
                for m in clazz.methods:
                    if "Callback" in m.raw:
                        if m.name.startswith("register"):
                            other = "unregister" + m.name[8:]
                            if other not in methods:
                                error(clazz, m, "L2", "Missing unregister method")
                        if m.name.startswith("unregister"):
                            other = "register" + m.name[10:]
                            if other not in methods:
                                error(clazz, m, "L2", "Missing register method")

                        if m.name.startswith("add") or m.name.startswith("remove"):
                            error(clazz, m, "L3", "Callback methods should be named register/unregister")

                    if "Listener" in m.raw:
                        if m.name.startswith("add"):
                            other = "remove" + m.name[3:]
                            if other not in methods:
                                error(clazz, m, "L2", "Missing remove method")
                        if m.name.startswith("remove") and not m.name.startswith("removeAll"):
                            other = "add" + m.name[6:]
                            if other not in methods:
                                error(clazz, m, "L2", "Missing add method")

                        if m.name.startswith("register") or m.name.startswith("unregister"):
                            error(clazz, m, "L3", "Listener methods should be named add/remove")
         */

        /** Make sure that there is a corresponding method */
        fun ensureMatched(cls: ClassItem, methods: Sequence<MethodItem>, method: MethodItem, name: String) {
            for (candidate in methods) {
                if (candidate.name() == name) {
                    return
                }
            }

            report(
                PAIRED_REGISTRATION, method,
                "Found ${method.name()} but not $name in ${cls.qualifiedName()}"
            )
        }

        for (method in methods) {
            val name = method.name()
            // the python version looks for any substring, but that includes a lot of other stuff, like plurals
            if (name.endsWith("Callback")) {
                if (name.startsWith("register")) {
                    val unregister = "unregister" + name.substring(8) // "register".length
                    ensureMatched(cls, methods, method, unregister)
                } else if (name.startsWith("unregister")) {
                    val unregister = "register" + name.substring(10) // "unregister".length
                    ensureMatched(cls, methods, method, unregister)
                }
                if (name.startsWith("add") || name.startsWith("remove")) {
                    report(
                        REGISTRATION_NAME, method,
                        "Callback methods should be named register/unregister; was $name"
                    )
                }
            } else if (name.endsWith("Listener")) {
                if (name.startsWith("add")) {
                    val unregister = "remove" + name.substring(3) // "add".length
                    ensureMatched(cls, methods, method, unregister)
                } else if (name.startsWith("remove") && !name.startsWith("removeAll")) {
                    val unregister = "add" + name.substring(6) // "remove".length
                    ensureMatched(cls, methods, method, unregister)
                }
                if (name.startsWith("register") || name.startsWith("unregister")) {
                    report(
                        REGISTRATION_NAME, method,
                        "Listener methods should be named add/remove; was $name"
                    )
                }
            }
        }
    }

    private fun checkSynchronized(method: MethodItem) {
        /*
            def verify_sync(clazz):
                """Verify synchronized methods aren't exposed."""
                for m in clazz.methods:
                    if "synchronized" in m.split:
                        error(clazz, m, "M5", "Internal locks must not be exposed")
         */

        fun reportError(method: MethodItem, psi: PsiElement? = null) {
            val message = StringBuilder("Internal locks must not be exposed")
            if (psi != null) {
                message.append(" (synchronizing on this or class is still externally observable)")
            }
            message.append(": ")
            message.append(method.describe())
            report(VISIBLY_SYNCHRONIZED, method, message.toString(), psi)
        }

        if (method.modifiers.isSynchronized()) {
            reportError(method)
        } else if (method is PsiMethodItem) {
            val psiMethod = method.psiMethod
            if (psiMethod is UMethod) {
                psiMethod.accept(object : AbstractUastVisitor() {
                    override fun afterVisitCallExpression(node: UCallExpression) {
                        super.afterVisitCallExpression(node)

                        if (node.methodName == "synchronized" && node.receiver == null) {
                            val arg = node.valueArguments.firstOrNull()
                            if (arg is UThisExpression ||
                                arg is UClassLiteralExpression ||
                                arg is UQualifiedReferenceExpression && arg.receiver is UClassLiteralExpression
                            ) {
                                reportError(method, arg.sourcePsi ?: node.sourcePsi ?: node.javaPsi)
                            }
                        }
                    }
                })
            } else {
                psiMethod.body?.accept(object : JavaRecursiveElementVisitor() {
                    override fun visitSynchronizedStatement(statement: PsiSynchronizedStatement) {
                        super.visitSynchronizedStatement(statement)

                        val lock = statement.lockExpression
                        if (lock == null || lock is PsiThisExpression ||
                            // locking on any class is visible
                            lock is PsiClassObjectAccessExpression
                        ) {
                            reportError(method, lock ?: statement)
                        }
                    }
                })
            }
        }
    }

    private fun checkIntentBuilder(method: MethodItem) {
        /*
            def verify_intent_builder(clazz):
                """Verify that Intent builders are createFooIntent() style."""
                if clazz.name == "Intent": return

                for m in clazz.methods:
                    if m.typ == "android.content.Intent":
                        if m.name.startswith("create") and m.name.endswith("Intent"):
                            pass
                        else:
                            warn(clazz, m, "FW1", "Methods creating an Intent should be named createFooIntent()")
         */
        if (method.returnType()?.toTypeString() == "android.content.Intent") {
            val name = method.name()
            if (name.startsWith("create") && name.endsWith("Intent")) {
                return
            }
            if (method.containingClass().simpleName() == "Intent") {
                return
            }

            report(
                INTENT_BUILDER_NAME, method,
                "Methods creating an Intent should be named `create<Foo>Intent()`, was `$name`"
            )
        }
    }

    private fun checkHelperClasses(cls: ClassItem, methods: Sequence<MethodItem>, fields: Sequence<FieldItem>) {
        /*
            def verify_helper_classes(clazz):
                """Verify that helper classes are named consistently with what they extend.
                All developer extendable methods should be named onFoo()."""
                test_methods = False
                if "extends android.app.Service" in clazz.raw:
                    test_methods = True
                    if not clazz.name.endswith("Service"):
                        error(clazz, None, "CL4", "Inconsistent class name; should be FooService")

                    found = False
                    for f in clazz.fields:
                        if f.name == "SERVICE_INTERFACE":
                            found = True
                            if f.value != clazz.fullname:
                                error(clazz, f, "C4", "Inconsistent interface constant; expected '%s'" % (clazz.fullname))

                if "extends android.content.ContentProvider" in clazz.raw:
                    test_methods = True
                    if not clazz.name.endswith("Provider"):
                        error(clazz, None, "CL4", "Inconsistent class name; should be FooProvider")

                    found = False
                    for f in clazz.fields:
                        if f.name == "PROVIDER_INTERFACE":
                            found = True
                            if f.value != clazz.fullname:
                                error(clazz, f, "C4", "Inconsistent interface constant; expected '%s'" % (clazz.fullname))

                if "extends android.content.BroadcastReceiver" in clazz.raw:
                    test_methods = True
                    if not clazz.name.endswith("Receiver"):
                        error(clazz, None, "CL4", "Inconsistent class name; should be FooReceiver")

                if "extends android.app.Activity" in clazz.raw:
                    test_methods = True
                    if not clazz.name.endswith("Activity"):
                        error(clazz, None, "CL4", "Inconsistent class name; should be FooActivity")

                if test_methods:
                    for m in clazz.methods:
                        if "final" in m.split: continue
// Note: This regex seems wrong:
                        if not re.match("on[A-Z]", m.name):
                            if "abstract" in m.split:
                                warn(clazz, m, None, "Methods implemented by developers should be named onFoo()")
                            else:
                                warn(clazz, m, None, "If implemented by developer, should be named onFoo(); otherwise consider marking final")

         */

        fun ensureFieldValue(fields: Sequence<FieldItem>, fieldName: String, fieldValue: String) {
            fields.firstOrNull { it.name() == fieldName }?.let { field ->
                if (field.initialValue(true) != fieldValue) {
                    report(
                        INTERFACE_CONSTANT, field,
                        "Inconsistent interface constant; expected '$fieldValue'`"
                    )
                }
            }
        }

        fun ensureContextNameSuffix(cls: ClassItem, suffix: String) {
            if (!cls.simpleName().endsWith(suffix)) {
                report(
                    CONTEXT_NAME_SUFFIX, cls,
                    "Inconsistent class name; should be `<Foo>$suffix`, was `${cls.simpleName()}`"
                )
            }
        }

        var testMethods = false

        when {
            cls.extends("android.app.Service") -> {
                testMethods = true
                ensureContextNameSuffix(cls, "Service")
                ensureFieldValue(fields, "SERVICE_INTERFACE", cls.qualifiedName())
            }
            cls.extends("android.content.ContentProvider") -> {
                testMethods = true
                ensureContextNameSuffix(cls, "Provider")
                ensureFieldValue(fields, "PROVIDER_INTERFACE", cls.qualifiedName())
            }
            cls.extends("android.content.BroadcastReceiver") -> {
                testMethods = true
                ensureContextNameSuffix(cls, "Receiver")
            }
            cls.extends("android.app.Activity") -> {
                testMethods = true
                ensureContextNameSuffix(cls, "Activity")
            }
        }

        if (testMethods) {
            for (method in methods) {
                val modifiers = method.modifiers
                if (modifiers.isFinal() || modifiers.isStatic()) {
                    continue
                }
                val name = method.name()
                if (!onCallbackNamePattern.matches(name)) {
                    val message =
                        if (modifiers.isAbstract()) {
                            "Methods implemented by developers should follow the on<Something> style, was `$name`"
                        } else {
                            "If implemented by developer, should follow the on<Something> style; otherwise consider marking final"
                        }
                    report(ON_NAME_EXPECTED, method, message)
                }
            }
        }
    }

    private fun checkBuilder(cls: ClassItem, methods: Sequence<MethodItem>, superClass: ClassItem?) {
        /*
            def verify_builder(clazz):
                """Verify builder classes.
                Methods should return the builder to enable chaining."""
                if " extends " in clazz.raw: return
                if not clazz.name.endswith("Builder"): return

                if clazz.name != "Builder":
                    warn(clazz, None, None, "Builder should be defined as inner class")

                has_build = False
                for m in clazz.methods:
                    if m.name == "build":
                        has_build = True
                        continue

                    if m.name.startswith("get"): continue
                    if m.name.startswith("clear"): continue

                    if m.name.startswith("with"):
                        warn(clazz, m, None, "Builder methods names should use setFoo() style")

                    if m.name.startswith("set"):
                        if not m.typ.endswith(clazz.fullname):
                            warn(clazz, m, "M4", "Methods must return the builder object")

                if not has_build:
                    warn(clazz, None, None, "Missing build() method")
         */
        if (!cls.simpleName().endsWith("Builder")) {
            return
        }
        if (superClass != null && !superClass.isJavaLangObject()) {
            return
        }
        if (cls.isTopLevelClass()) {
            report(
                TOP_LEVEL_BUILDER, cls,
                "Builder should be defined as inner class: ${cls.qualifiedName()}"
            )
        }
        var hasBuild = false
        for (method in methods) {
            val name = method.name()
            if (name == "build") {
                hasBuild = true
                continue
            } else if (name.startsWith("get") || name.startsWith("clear")) {
                continue
            } else if (name.startsWith("with")) {
                report(
                    BUILDER_SET_STYLE, method,
                    "Builder methods names should use setFoo() style: ${method.describe()}"
                )
            } else if (name.startsWith("set")) {
                val returnType = method.returnType()?.toTypeString() ?: ""
                if (returnType != cls.toType().toTypeString()) {
                    report(
                        SETTER_RETURNS_THIS, method,
                        "Methods must return the builder object (return type ${cls.toType().toTypeString()} instead of $returnType): ${method.describe()}"
                    )
                }
            }
        }
        if (!hasBuild) {
            report(
                MISSING_BUILD_METHOD, cls,
                "${cls.qualifiedName()} does not declare a `build()` method, but builder classes are expected to"
            )
        }
    }

    private fun checkAidl(cls: ClassItem, superClass: ClassItem?, interfaces: Sequence<TypeItem>) {
        /*
            def verify_aidl(clazz):
                """Catch people exposing raw AIDL."""
                if "extends android.os.Binder" in clazz.raw or "implements android.os.IInterface" in clazz.raw:
                    error(clazz, None, None, "Raw AIDL interfaces must not be exposed")
        */

        // Instead of ClassItem.implements() and .extends() which performs hierarchy
        // searches, here we only want to flag directly extending or implementing:
        val extendsBinder = superClass?.qualifiedName() == "android.os.Binder"
        val implementsIInterface = interfaces.any { it.toTypeString() == "android.os.IInterface" }
        if (extendsBinder || implementsIInterface) {
            val problem = if (extendsBinder) {
                "extends Binder"
            } else {
                "implements IInterface"
            }
            report(
                RAW_AIDL, cls,
                "Raw AIDL interfaces must not be exposed: ${cls.simpleName()} $problem"
            )
        }
    }

    private fun checkInternal(cls: ClassItem) {
        /*
            def verify_internal(clazz):
                """Catch people exposing internal classes."""
                if clazz.pkg.name.startswith("com.android"):
                    error(clazz, None, None, "Internal classes must not be exposed")
        */

        if (cls.qualifiedName().startsWith("com.android.")) {
            report(
                INTERNAL_CLASSES, cls,
                "Internal classes must not be exposed"
            )
        }
    }

    private fun checkLayering(
        cls: ClassItem,
        methodsAndConstructors: Sequence<MethodItem>,
        fields: Sequence<FieldItem>
    ) {
        /*
            def verify_layering(clazz):
                """Catch package layering violations.
                For example, something in android.os depending on android.app."""
                ranking = [
                    ["android.service","android.accessibilityservice","android.inputmethodservice","android.printservice","android.appwidget","android.webkit","android.preference","android.gesture","android.print"],
                    "android.app",
                    "android.widget",
                    "android.view",
                    "android.animation",
                    "android.provider",
                    ["android.content","android.graphics.drawable"],
                    "android.database",
                    "android.text",
                    "android.graphics",
                    "android.os",
                    "android.util"
                ]

                def rank(p):
                    for i in range(len(ranking)):
                        if isinstance(ranking[i], list):
                            for j in ranking[i]:
                                if p.startswith(j): return i
                        else:
                            if p.startswith(ranking[i]): return i

                cr = rank(clazz.pkg.name)
                if cr is None: return

                for f in clazz.fields:
                    ir = rank(f.typ)
                    if ir and ir < cr:
                        warn(clazz, f, "FW6", "Field type violates package layering")

                for m in clazz.methods:
                    ir = rank(m.typ)
                    if ir and ir < cr:
                        warn(clazz, m, "FW6", "Method return type violates package layering")
                    for arg in m.args:
                        ir = rank(arg)
                        if ir and ir < cr:
                            warn(clazz, m, "FW6", "Method argument type violates package layering")

        */

        fun packageRank(pkg: PackageItem): Int {
            return when (pkg.qualifiedName()) {
                "android.service",
                "android.accessibilityservice",
                "android.inputmethodservice",
                "android.printservice",
                "android.appwidget",
                "android.webkit",
                "android.preference",
                "android.gesture",
                "android.print" -> 10

                "android.app" -> 20
                "android.widget" -> 30
                "android.view" -> 40
                "android.animation" -> 50
                "android.provider" -> 60

                "android.content",
                "android.graphics.drawable" -> 70

                "android.database" -> 80
                "android.text" -> 90
                "android.graphics" -> 100
                "android.os" -> 110
                "android.util" -> 120
                else -> -1
            }
        }

        fun getTypePackage(type: TypeItem?): PackageItem? {
            return if (type == null || type.primitive) {
                null
            } else {
                type.asClass()?.containingPackage()
            }
        }

        fun getTypeRank(type: TypeItem?): Int {
            type ?: return -1
            val pkg = getTypePackage(type) ?: return -1
            return packageRank(pkg)
        }

        val classPackage = cls.containingPackage()
        val classRank = packageRank(classPackage)
        if (classRank == -1) {
            return
        }
        for (field in fields) {
            val fieldTypeRank = getTypeRank(field.type())
            if (fieldTypeRank != -1 && fieldTypeRank < classRank) {
                report(
                    PACKAGE_LAYERING, cls,
                    "Field type `${field.type().toTypeString()}` violates package layering: nothing in `$classPackage` should depend on `${getTypePackage(
                        field.type()
                    )}`"
                )
            }
        }

        for (method in methodsAndConstructors) {
            val returnType = method.returnType()
            if (returnType != null) { // not a constructor
                val returnTypeRank = getTypeRank(returnType)
                if (returnTypeRank != -1 && returnTypeRank < classRank) {
                    report(
                        PACKAGE_LAYERING, cls,
                        "Method return type `${returnType.toTypeString()}` violates package layering: nothing in `$classPackage` should depend on `${getTypePackage(
                            returnType
                        )}`"
                    )
                }
            }

            for (parameter in method.parameters()) {
                val parameterTypeRank = getTypeRank(parameter.type())
                if (parameterTypeRank != -1 && parameterTypeRank < classRank) {
                    report(
                        PACKAGE_LAYERING, cls,
                        "Method parameter type `${parameter.type().toTypeString()}` violates package layering: nothing in `$classPackage` should depend on `${getTypePackage(
                            parameter.type()
                        )}`"
                    )
                }
            }
        }
    }

    private fun checkBooleans(methods: Sequence<MethodItem>) {
        /*
            Correct:

            void setVisible(boolean visible);
            boolean isVisible();

            void setHasTransientState(boolean hasTransientState);
            boolean hasTransientState();

            void setCanRecord(boolean canRecord);
            boolean canRecord();

            void setShouldFitWidth(boolean shouldFitWidth);
            boolean shouldFitWidth();

            void setWiFiRoamingSettingEnabled(boolean enabled)
            boolean isWiFiRoamingSettingEnabled()
        */

        fun errorIfExists(methods: Sequence<MethodItem>, trigger: String, expected: String, actual: String) {
            for (method in methods) {
                if (method.name() == actual) {
                    report(
                        GETTER_SETTER_NAMES, method,
                        "Symmetric method for `$trigger` must be named `$expected`; was `$actual`"
                    )
                }
            }
        }

        data class GetterSetterPattern(val getter: String, val setter: String)
        val goodPrefixes = listOf(
                GetterSetterPattern("has", "setHas"),
                GetterSetterPattern("can", "setCan"),
                GetterSetterPattern("should", "setShould"),
                GetterSetterPattern("is", "set")
        )
        fun List<GetterSetterPattern>.match(name: String, prop: (GetterSetterPattern) -> String) = firstOrNull {
            name.startsWith(prop(it)) && name.getOrNull(prop(it).length)?.isUpperCase() ?: false
        }

        val badGetterPrefixes = listOf("isHas", "isCan", "isShould", "get", "is")
        val badSetterPrefixes = listOf("setIs", "set")

        fun isGetter(method: MethodItem): Boolean {
            val returnType = method.returnType() ?: return false
            return method.parameters().isEmpty() && returnType.primitive && returnType.toTypeString() == "boolean"
        }

        fun isSetter(method: MethodItem): Boolean {
            return method.parameters().size == 1 && method.parameters()[0].type().toTypeString() == "boolean"
        }

        for (method in methods) {
            val name = method.name()
            if (isGetter(method)) {
                val pattern = goodPrefixes.match(name, GetterSetterPattern::getter) ?: continue
                val target = name.substring(pattern.getter.length)
                val expectedSetter = "${pattern.setter}$target"

                badSetterPrefixes.forEach {
                    val actualSetter = "${it}$target"
                    if (actualSetter != expectedSetter) {
                        errorIfExists(methods, name, expectedSetter, actualSetter)
                    }
                }
            } else if (isSetter(method)) {
                val pattern = goodPrefixes.match(name, GetterSetterPattern::setter) ?: continue
                val target = name.substring(pattern.setter.length)
                val expectedGetter = "${pattern.getter}$target"

                badGetterPrefixes.forEach {
                    val actualGetter = "${it}$target"
                    if (actualGetter != expectedGetter) {
                        errorIfExists(methods, name, expectedGetter, actualGetter)
                    }
                }
            }
        }
    }

    private fun checkCollections(
        type: TypeItem,
        item: Item
    ) {
        /*
            def verify_collections(clazz):
                """Verifies that collection types are interfaces."""
                if clazz.fullname == "android.os.Bundle": return

                bad = ["java.util.Vector", "java.util.LinkedList", "java.util.ArrayList", "java.util.Stack",
                       "java.util.HashMap", "java.util.HashSet", "android.util.ArraySet", "android.util.ArrayMap"]
                for m in clazz.methods:
                    if m.typ in bad:
                        error(clazz, m, "CL2", "Return type is concrete collection; must be higher-level interface")
                    for arg in m.args:
                        if arg in bad:
                            error(clazz, m, "CL2", "Argument is concrete collection; must be higher-level interface")
        */

        if (type.primitive) {
            return
        }

        val raw = type.asClass()?.qualifiedName()
        when (raw) {
            "java.util.Vector",
            "java.util.LinkedList",
            "java.util.ArrayList",
            "java.util.Stack",
            "java.util.HashMap",
            "java.util.HashSet",
            "android.util.ArraySet",
            "android.util.ArrayMap" -> {
                if (item.containingClass()?.qualifiedName() == "android.os.Bundle") {
                    return
                }
                val where = when (item) {
                    is MethodItem -> "Return type"
                    is FieldItem -> "Field type"
                    else -> "Parameter type"
                }
                val erased = type.toErasedTypeString()
                report(
                    CONCRETE_COLLECTION, item,
                    "$where is concrete collection (`$erased`); must be higher-level interface"
                )
            }
        }
    }

    fun Item.containingClass(): ClassItem? {
        return when (this) {
            is MemberItem -> this.containingClass()
            is ParameterItem -> this.containingMethod().containingClass()
            is ClassItem -> this
            else -> null
        }
    }

    private fun checkFlags(fields: Sequence<FieldItem>) {
        /*
            def verify_flags(clazz):
                """Verifies that flags are non-overlapping."""
                known = collections.defaultdict(int)
                for f in clazz.fields:
                    if "FLAG_" in f.name:
                        try:
                            val = int(f.value)
                        except:
                            continue

                        scope = f.name[0:f.name.index("FLAG_")]
                        if val & known[scope]:
                            warn(clazz, f, "C1", "Found overlapping flag constant value")
                        known[scope] |= val

        */
        var known: MutableMap<String, Int>? = null
        var valueToFlag: MutableMap<Int?, String>? = null
        for (field in fields) {
            val name = field.name()
            val index = name.indexOf("FLAG_")
            if (index != -1) {
                val value = field.initialValue() as? Int ?: continue
                val scope = name.substring(0, index)
                val prev = known?.get(scope) ?: 0
                if (known != null && (prev and value) != 0) {
                    val prevName = valueToFlag?.get(prev)
                    report(
                        OVERLAPPING_CONSTANTS, field,
                        "Found overlapping flag constant values: `$name` with value $value (0x${Integer.toHexString(
                            value
                        )}) and overlapping flag value $prev (0x${Integer.toHexString(prev)}) from `$prevName`"
                    )
                }
                if (known == null) {
                    known = mutableMapOf()
                }
                known[scope] = value
                if (valueToFlag == null) {
                    valueToFlag = mutableMapOf()
                }
                valueToFlag[value] = name
            }
        }
    }

    private fun checkExceptions(method: MethodItem, filterReference: Predicate<Item>) {
        /*
            def verify_exception(clazz):
                """Verifies that methods don't throw generic exceptions."""
                for m in clazz.methods:
                    for t in m.throws:
                        if t in ["java.lang.Exception", "java.lang.Throwable", "java.lang.Error"]:
                            error(clazz, m, "S1", "Methods must not throw generic exceptions")

                        if t in ["android.os.RemoteException"]:
                            if clazz.name == "android.content.ContentProviderClient": continue
                            if clazz.name == "android.os.Binder": continue
                            if clazz.name == "android.os.IBinder": continue

                            error(clazz, m, "FW9", "Methods calling into system server should rethrow RemoteException as RuntimeException")

                        if len(m.args) == 0 and t in ["java.lang.IllegalArgumentException", "java.lang.NullPointerException"]:
                            warn(clazz, m, "S1", "Methods taking no arguments should throw IllegalStateException")
        */
        for (exception in method.filteredThrowsTypes(filterReference)) {
            val qualifiedName = exception.qualifiedName()
            when (qualifiedName) {
                "java.lang.Exception",
                "java.lang.Throwable",
                "java.lang.Error" -> {
                    report(
                        GENERIC_EXCEPTION, method,
                        "Methods must not throw generic exceptions (`$qualifiedName`)"
                    )
                }
                "android.os.RemoteException" -> {
                    when (method.containingClass().qualifiedName()) {
                        "android.content.ContentProviderClient",
                        "android.os.Binder",
                        "android.os.IBinder" -> {
                            // exceptions
                        }
                        else -> {
                            report(
                                RETHROW_REMOTE_EXCEPTION, method,
                                "Methods calling system APIs should rethrow `RemoteException` as `RuntimeException` (but do not list it in the throws clause)"
                            )
                        }
                    }
                }
                "java.lang.IllegalArgumentException",
                "java.lang.NullPointerException" -> {
                    if (method.parameters().isEmpty()) {
                        report(
                            ILLEGAL_STATE_EXCEPTION, method,
                            "Methods taking no arguments should throw `IllegalStateException` instead of `$qualifiedName`"
                        )
                    }
                }
            }
        }
    }

    private fun checkGoogle(cls: ClassItem, methods: Sequence<MethodItem>, fields: Sequence<FieldItem>) {
        /*
            def verify_google(clazz):
                """Verifies that APIs never reference Google."""

                if re.search("google", clazz.raw, re.IGNORECASE):
                    error(clazz, None, None, "Must never reference Google")

                test = []
                test.extend(clazz.ctors)
                test.extend(clazz.fields)
                test.extend(clazz.methods)

                for t in test:
                    if re.search("google", t.raw, re.IGNORECASE):
                        error(clazz, t, None, "Must never reference Google")
        */

        fun checkName(name: String, item: Item) {
            if (name.contains("Google", ignoreCase = true)) {
                report(
                    MENTIONS_GOOGLE, item,
                    "Must never reference Google (`$name`)"
                )
            }
        }

        checkName(cls.simpleName(), cls)
        for (method in methods) {
            checkName(method.name(), method)
        }
        for (field in fields) {
            checkName(field.name(), field)
        }
    }

    private fun checkBitSet(type: TypeItem, typeString: String, item: Item) {
        if (typeString.startsWith("java.util.BitSet") &&
            type.asClass()?.qualifiedName() == "java.util.BitSet"
        ) {
            report(
                HEAVY_BIT_SET, item,
                "Type must not be heavy BitSet (${item.describe()})"
            )
        }
    }

    private fun checkManager(cls: ClassItem, methods: Sequence<MethodItem>, constructors: Sequence<ConstructorItem>) {
        /*
            def verify_manager(clazz):
                """Verifies that FooManager is only obtained from Context."""

                if not clazz.name.endswith("Manager"): return

                for c in clazz.ctors:
                    error(clazz, c, None, "Managers must always be obtained from Context; no direct constructors")

                for m in clazz.methods:
                    if m.typ == clazz.fullname:
                        error(clazz, m, None, "Managers must always be obtained from Context")

        */
        if (!cls.simpleName().endsWith("Manager")) {
            return
        }
        for (method in constructors) {
            method.modifiers.isPublic()
            method.modifiers.isPrivate()
            report(
                MANAGER_CONSTRUCTOR, method,
                "Managers must always be obtained from Context; no direct constructors"
            )
        }
        for (method in methods) {
            if (method.returnType()?.asClass() == cls) {
                report(
                    MANAGER_LOOKUP, method,
                    "Managers must always be obtained from Context (`${method.name()}`)"
                )
            }
        }
    }

    private fun checkHasNullability(item: Item) {
        if (item.requiresNullnessInfo() && !item.hasNullnessInfo() &&
                getImplicitNullness(item) == null) {
            val type = item.type()
            if (type != null && type.isTypeParameter()) {
                // Generic types should have declarations of nullability set at the site of where
                // the type is set, so that for Foo<T>, T does not need to specify nullability, but
                // for Foo<Bar>, Bar does.
                return // Do not enforce nullability for generics
            }
            val where = when (item) {
                is ParameterItem -> "parameter `${item.name()}` in method `${item.parent()?.name()}`"
                is FieldItem -> {
                    if (item.isKotlin()) {
                        if (item.name() == "INSTANCE") {
                            // Kotlin compiler is not marking it with a nullability annotation
                            // https://youtrack.jetbrains.com/issue/KT-33226
                            return
                        }
                        if (item.modifiers.isCompanion()) {
                            // Kotlin compiler is not marking it with a nullability annotation
                            // https://youtrack.jetbrains.com/issue/KT-33314
                            return
                        }
                    }
                    "field `${item.name()}` in class `${item.parent()}`"
                }

                is ConstructorItem -> "constructor `${item.name()}` return"
                is MethodItem -> {
                    // For methods requiresNullnessInfo and hasNullnessInfo considers both parameters and return,
                    // only warn about non-annotated returns here as parameters will get visited individually.
                    if (item.isConstructor() || item.returnType()?.primitive == true) return
                    if (item.modifiers.hasNullnessInfo()) return
                    "method `${item.name()}` return"
                }
                else -> throw IllegalStateException("Unexpected item type: $item")
            }
            report(MISSING_NULLABILITY, item, "Missing nullability on $where")
        }
    }

    private fun checkBoxed(type: TypeItem, item: Item) {
        /*
            def verify_boxed(clazz):
                """Verifies that methods avoid boxed primitives."""

                boxed = ["java.lang.Number","java.lang.Byte","java.lang.Double","java.lang.Float","java.lang.Integer","java.lang.Long","java.lang.Short"]

                for c in clazz.ctors:
                    for arg in c.args:
                        if arg in boxed:
                            error(clazz, c, "M11", "Must avoid boxed primitives")

                for f in clazz.fields:
                    if f.typ in boxed:
                        error(clazz, f, "M11", "Must avoid boxed primitives")

                for m in clazz.methods:
                    if m.typ in boxed:
                        error(clazz, m, "M11", "Must avoid boxed primitives")
                    for arg in m.args:
                        if arg in boxed:
                            error(clazz, m, "M11", "Must avoid boxed primitives")
        */

        fun isBoxType(qualifiedName: String): Boolean {
            return when (qualifiedName) {
                "java.lang.Number",
                "java.lang.Byte",
                "java.lang.Double",
                "java.lang.Float",
                "java.lang.Integer",
                "java.lang.Long",
                "java.lang.Short" ->
                    true
                else ->
                    false
            }
        }

        val qualifiedName = type.asClass()?.qualifiedName() ?: return
        if (isBoxType(qualifiedName)) {
            report(
                AUTO_BOXING, item,
                "Must avoid boxed primitives (`$qualifiedName`)"
            )
        }
    }

    private fun checkStaticUtils(
        cls: ClassItem,
        methods: Sequence<MethodItem>,
        constructors: Sequence<ConstructorItem>,
        fields: Sequence<FieldItem>
    ) {
        /*
            def verify_static_utils(clazz):
                """Verifies that helper classes can't be constructed."""
                if clazz.fullname.startswith("android.opengl"): return
                if clazz.fullname.startswith("android.R"): return

                # Only care about classes with default constructors
                if len(clazz.ctors) == 1 and len(clazz.ctors[0].args) == 0:
                    test = []
                    test.extend(clazz.fields)
                    test.extend(clazz.methods)

                    if len(test) == 0: return
                    for t in test:
                        if "static" not in t.split:
                            return

                    error(clazz, None, None, "Fully-static utility classes must not have constructor")
        */
        if (!cls.isClass()) {
            return
        }

        val hasDefaultConstructor = cls.hasImplicitDefaultConstructor() || run {
            if (constructors.count() == 1) {
                val constructor = constructors.first()
                constructor.parameters().isEmpty() && constructor.modifiers.isPublic()
            } else {
                false
            }
        }

        if (hasDefaultConstructor) {
            val qualifiedName = cls.qualifiedName()
            if (qualifiedName.startsWith("android.opengl.") ||
                qualifiedName.startsWith("android.R.") ||
                qualifiedName == "android.R"
            ) {
                return
            }

            if (methods.none() && fields.none()) {
                return
            }

            if (methods.none { !it.modifiers.isStatic() } &&
                fields.none { !it.modifiers.isStatic() }) {
                report(
                    STATIC_UTILS, cls,
                    "Fully-static utility classes must not have constructor"
                )
            }
        }
    }

    private fun checkOverloadArgs(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_overload_args(clazz):
                """Verifies that method overloads add new arguments at the end."""
                if clazz.fullname.startswith("android.opengl"): return

                overloads = collections.defaultdict(list)
                for m in clazz.methods:
                    if "deprecated" in m.split: continue
                    overloads[m.name].append(m)

                for name, methods in overloads.items():
                    if len(methods) <= 1: continue

                    # Look for arguments common across all overloads
                    def cluster(args):
                        count = collections.defaultdict(int)
                        res = set()
                        for i in range(len(args)):
                            a = args[i]
                            res.add("%s#%d" % (a, count[a]))
                            count[a] += 1
                        return res

                    common_args = cluster(methods[0].args)
                    for m in methods:
                        common_args = common_args & cluster(m.args)

                    if len(common_args) == 0: continue

                    # Require that all common arguments are present at start of signature
                    locked_sig = None
                    for m in methods:
                        sig = m.args[0:len(common_args)]
                        if not common_args.issubset(cluster(sig)):
                            warn(clazz, m, "M2", "Expected common arguments [%s] at beginning of overloaded method" % (", ".join(common_args)))
                        elif not locked_sig:
                            locked_sig = sig
                        elif locked_sig != sig:
                            error(clazz, m, "M2", "Expected consistent argument ordering between overloads: %s..." % (", ".join(locked_sig)))
        */

        if (cls.qualifiedName().startsWith("android.opengl")) {
            return
        }

        val overloads = mutableMapOf<String, MutableList<MethodItem>>()
        for (method in methods) {
            if (!method.deprecated) {
                val name = method.name()
                val list = overloads[name] ?: run {
                    val new = mutableListOf<MethodItem>()
                    overloads[name] = new
                    new
                }
                list.add(method)
            }
        }

        // Look for arguments common across all overloads
        fun cluster(args: List<ParameterItem>): MutableSet<String> {
            val count = mutableMapOf<String, Int>()
            val res = mutableSetOf<String>()
            for (parameter in args) {
                val a = parameter.type().toTypeString()
                val currCount = count[a] ?: 1
                res.add("$a#$currCount")
                count[a] = currCount + 1
            }
            return res
        }

        for ((_, methodList) in overloads.entries) {
            if (methodList.size <= 1) {
                continue
            }

            val commonArgs = cluster(methodList[0].parameters())
            for (m in methodList) {
                val clustered = cluster(m.parameters())
                commonArgs.removeAll(clustered)
            }
            if (commonArgs.isEmpty()) {
                continue
            }

            // Require that all common arguments are present at the start of the signature
            var lockedSig: List<ParameterItem>? = null
            val commonArgCount = commonArgs.size
            for (m in methodList) {
                val sig = m.parameters().subList(0, commonArgCount)
                val cluster = cluster(sig)
                if (!cluster.containsAll(commonArgs)) {
                    report(
                        COMMON_ARGS_FIRST, m,
                        "Expected common arguments ${commonArgs.joinToString()}} at beginning of overloaded method ${m.describe()}"
                    )
                } else if (lockedSig == null) {
                    lockedSig = sig
                } else if (lockedSig != sig) {
                    report(
                        CONSISTENT_ARGUMENT_ORDER, m,
                        "Expected consistent argument ordering between overloads: ${lockedSig.joinToString()}}"
                    )
                }
            }
        }
    }

    private fun checkCallbackHandlers(
        cls: ClassItem,
        methodsAndConstructors: Sequence<MethodItem>,
        superClass: ClassItem?
    ) {
        /*
            def verify_callback_handlers(clazz):
                """Verifies that methods adding listener/callback have overload
                for specifying delivery thread."""

                # Ignore UI packages which assume main thread
                skip = [
                    "animation",
                    "view",
                    "graphics",
                    "transition",
                    "widget",
                    "webkit",
                ]
                for s in skip:
                    if s in clazz.pkg.name_path: return
                    if s in clazz.extends_path: return

                # Ignore UI classes which assume main thread
                if "app" in clazz.pkg.name_path or "app" in clazz.extends_path:
                    for s in ["ActionBar","Dialog","Application","Activity","Fragment","Loader"]:
                        if s in clazz.fullname: return
                if "content" in clazz.pkg.name_path or "content" in clazz.extends_path:
                    for s in ["Loader"]:
                        if s in clazz.fullname: return

                found = {}
                by_name = collections.defaultdict(list)
                examine = clazz.ctors + clazz.methods
                for m in examine:
                    if m.name.startswith("unregister"): continue
                    if m.name.startswith("remove"): continue
                    if re.match("on[A-Z]+", m.name): continue

                    by_name[m.name].append(m)

                    for a in m.args:
                        if a.endswith("Listener") or a.endswith("Callback") or a.endswith("Callbacks"):
                            found[m.name] = m

                for f in found.values():
                    takes_handler = False
                    takes_exec = False
                    for m in by_name[f.name]:
                        if "android.os.Handler" in m.args:
                            takes_handler = True
                        if "java.util.concurrent.Executor" in m.args:
                            takes_exec = True
                    if not takes_exec:
                        warn(clazz, f, "L1", "Registration methods should have overload that accepts delivery Executor")

        */

        // Note: In the above we compute takes_handler but it's not used; is this an incomplete
        // check?

        fun packageContainsSegment(packageName: String?, segment: String): Boolean {
            packageName ?: return false
            return (packageName.contains(segment) &&
                (packageName.contains(".$segment.") || packageName.endsWith(".$segment")))
        }

        fun skipPackage(packageName: String?): Boolean {
            packageName ?: return false
            for (segment in uiPackageParts) {
                if (packageContainsSegment(packageName, segment)) {
                    return true
                }
            }

            return false
        }

        // Ignore UI packages which assume main thread
        val classPackage = cls.containingPackage().qualifiedName()
        val extendsPackage = superClass?.containingPackage()?.qualifiedName()

        if (skipPackage(classPackage) || skipPackage(extendsPackage)) {
            return
        }

        // Ignore UI classes which assume main thread
        if (packageContainsSegment(classPackage, "app") ||
            packageContainsSegment(extendsPackage, "app")
        ) {
            val fullName = cls.fullName()
            if (fullName.contains("ActionBar") ||
                fullName.contains("Dialog") ||
                fullName.contains("Application") ||
                fullName.contains("Activity") ||
                fullName.contains("Fragment") ||
                fullName.contains("Loader")
            ) {
                return
            }
        }
        if (packageContainsSegment(classPackage, "content") ||
            packageContainsSegment(extendsPackage, "content")
        ) {
            val fullName = cls.fullName()
            if (fullName.contains("Loader")) {
                return
            }
        }

        val found = mutableMapOf<String, MethodItem>()
        val byName = mutableMapOf<String, MutableList<MethodItem>>()
        for (method in methodsAndConstructors) {
            val name = method.name()
            if (name.startsWith("unregister")) {
                continue
            }
            if (name.startsWith("remove")) {
                continue
            }
            if (name.startsWith("on") && onCallbackNamePattern.matches(name)) {
                continue
            }

            val list = byName[name] ?: run {
                val new = mutableListOf<MethodItem>()
                byName[name] = new
                new
            }
            list.add(method)

            for (parameter in method.parameters()) {
                val type = parameter.type().toTypeString()
                if (type.endsWith("Listener") ||
                    type.endsWith("Callback") ||
                    type.endsWith("Callbacks")
                ) {
                    found[name] = method
                }
            }
        }

        for (f in found.values) {
            var takesExec = false

            // TODO: apilint computed takes_handler but did not use it; should we add more checks or conditions?
            // var takesHandler = false

            val name = f.name()
            for (method in byName[name]!!) {
                // if (method.parameters().any { it.type().toTypeString() == "android.os.Handler" }) {
                //    takesHandler = true
                // }
                if (method.parameters().any { it.type().toTypeString() == "java.util.concurrent.Executor" }) {
                    takesExec = true
                }
            }
            if (!takesExec) {
                report(
                    EXECUTOR_REGISTRATION, f,
                    "Registration methods should have overload that accepts delivery Executor: `$name`"
                )
            }
        }
    }

    private fun checkContextFirst(method: MethodItem) {
        /*
            def verify_context_first(clazz):
                """Verifies that methods accepting a Context keep it the first argument."""
                examine = clazz.ctors + clazz.methods
                for m in examine:
                    if len(m.args) > 1 and m.args[0] != "android.content.Context":
                        if "android.content.Context" in m.args[1:]:
                            error(clazz, m, "M3", "Context is distinct, so it must be the first argument")
                    if len(m.args) > 1 and m.args[0] != "android.content.ContentResolver":
                        if "android.content.ContentResolver" in m.args[1:]:
                            error(clazz, m, "M3", "ContentResolver is distinct, so it must be the first argument")
        */
        val parameters = method.parameters()
        if (parameters.size > 1 && parameters[0].type().toTypeString() != "android.content.Context") {
            for (i in 1 until parameters.size) {
                val p = parameters[i]
                if (p.type().toTypeString() == "android.content.Context") {
                    report(
                        CONTEXT_FIRST, p,
                        "Context is distinct, so it must be the first argument (method `${method.name()}`)"
                    )
                }
            }
        }
        if (parameters.size > 1 && parameters[0].type().toTypeString() != "android.content.ContentResolver") {
            for (i in 1 until parameters.size) {
                val p = parameters[i]
                if (p.type().toTypeString() == "android.content.ContentResolver") {
                    report(
                        CONTEXT_FIRST, p,
                        "ContentResolver is distinct, so it must be the first argument (method `${method.name()}`)"
                    )
                }
            }
        }
    }

    private fun checkListenerLast(method: MethodItem) {
        /*
            def verify_listener_last(clazz):
                """Verifies that methods accepting a Listener or Callback keep them as last arguments."""
                examine = clazz.ctors + clazz.methods
                for m in examine:
                    if "Listener" in m.name or "Callback" in m.name: continue
                    found = False
                    for a in m.args:
                        if a.endswith("Callback") or a.endswith("Callbacks") or a.endswith("Listener"):
                            found = True
                        elif found:
                            warn(clazz, m, "M3", "Listeners should always be at end of argument list")
                    */

        val name = method.name()
        if (name.contains("Listener") || name.contains("Callback")) {
            return
        }

        val parameters = method.parameters()
        if (parameters.size > 1) {
            var found = false
            for (parameter in parameters) {
                val type = parameter.type().toTypeString()
                if (type.endsWith("Callback") || type.endsWith("Callbacks") || type.endsWith("Listener")) {
                    found = true
                } else if (found) {
                    report(
                        LISTENER_LAST, parameter,
                        "Listeners should always be at end of argument list (method `${method.name()}`)"
                    )
                }
            }
        }
    }

    private fun checkResourceNames(cls: ClassItem, fields: Sequence<FieldItem>) {
        /*
            def verify_resource_names(clazz):
                """Verifies that resource names have consistent case."""
                if not re.match("android\.R\.[a-z]+", clazz.fullname): return

                # Resources defined by files are foo_bar_baz
                if clazz.name in ["anim","animator","color","dimen","drawable","interpolator","layout","transition","menu","mipmap","string","plurals","raw","xml"]:
                    for f in clazz.fields:
                        if re.match("config_[a-z][a-zA-Z1-9]*$", f.name): continue
                        if f.name.startswith("config_"):
                            error(clazz, f, None, "Expected config name to be config_fooBarBaz style")

                        if re.match("[a-z1-9_]+$", f.name): continue
                        error(clazz, f, None, "Expected resource name in this class to be foo_bar_baz style")

                # Resources defined inside files are fooBarBaz
                if clazz.name in ["array","attr","id","bool","fraction","integer"]:
                    for f in clazz.fields:
                        if re.match("config_[a-z][a-zA-Z1-9]*$", f.name): continue
                        if re.match("layout_[a-z][a-zA-Z1-9]*$", f.name): continue
                        if re.match("state_[a-z_]*$", f.name): continue

                        if re.match("[a-z][a-zA-Z1-9]*$", f.name): continue
                        error(clazz, f, "C7", "Expected resource name in this class to be fooBarBaz style")

                # Styles are FooBar_Baz
                if clazz.name in ["style"]:
                    for f in clazz.fields:
                        if re.match("[A-Z][A-Za-z1-9]+(_[A-Z][A-Za-z1-9]+?)*$", f.name): continue
                        error(clazz, f, "C7", "Expected resource name in this class to be FooBar_Baz style")
        */
        if (!cls.qualifiedName().startsWith("android.R.")) {
            return
        }

        val resourceType = ResourceType.fromClassName(cls.simpleName()) ?: return
        when (resourceType) {
            ANIM,
            ANIMATOR,
            COLOR,
            DIMEN,
            DRAWABLE,
            FONT,
            INTERPOLATOR,
            LAYOUT,
            MENU,
            MIPMAP,
            NAVIGATION,
            PLURALS,
            RAW,
            STRING,
            TRANSITION,
            XML -> {
                // Resources defined by files are foo_bar_baz
                // Note: it's surprising that dimen, plurals and string are in this list since
                // they are value resources, not file resources, but keeping api lint compatibility
                // for now.

                for (field in fields) {
                    val name = field.name()
                    if (name.startsWith("config_")) {
                        if (!configFieldPattern.matches(name)) {
                            report(
                                CONFIG_FIELD_NAME, field,
                                "Expected config name to be in the `config_fooBarBaz` style, was `$name`"
                            )
                        }
                        continue
                    }
                    if (!resourceFileFieldPattern.matches(name)) {
                        report(
                            RESOURCE_FIELD_NAME, field,
                            "Expected resource name in `${cls.qualifiedName()}` to be in the `foo_bar_baz` style, was `$name`"
                        )
                    }
                }
            }

            ARRAY,
            ATTR,
            BOOL,
            FRACTION,
            ID,
            INTEGER -> {
                // Resources defined inside files are fooBarBaz
                for (field in fields) {
                    val name = field.name()
                    if (name.startsWith("config_") && configFieldPattern.matches(name)) {
                        continue
                    }
                    if (name.startsWith("layout_") && layoutFieldPattern.matches(name)) {
                        continue
                    }
                    if (name.startsWith("state_") && stateFieldPattern.matches(name)) {
                        continue
                    }
                    if (resourceValueFieldPattern.matches(name)) {
                        continue
                    }
                    report(
                        RESOURCE_VALUE_FIELD_NAME, field,
                        "Expected resource name in `${cls.qualifiedName()}` to be in the `fooBarBaz` style, was `$name`"
                    )
                }
            }

            STYLE -> {
                for (field in fields) {
                    val name = field.name()
                    if (!styleFieldPattern.matches(name)) {
                        report(
                            RESOURCE_STYLE_FIELD_NAME, field,
                            "Expected resource name in `${cls.qualifiedName()}` to be in the `FooBar_Baz` style, was `$name`"
                        )
                    }
                }
            }

            STYLEABLE, // appears as R class but name check is implicitly done as part of style class check
                // DECLARE_STYLEABLE,
            STYLE_ITEM,
            PUBLIC,
            SAMPLE_DATA,
            AAPT -> {
                // no-op; these are resource "types" in XML but not present as R classes
                // Listed here explicitly to force compiler error as new resource types
                // are added.
            }
        }
    }

    private fun checkFiles(methodsAndConstructors: Sequence<MethodItem>) {
        /*
            def verify_files(clazz):
                """Verifies that methods accepting File also accept streams."""

                has_file = set()
                has_stream = set()

                test = []
                test.extend(clazz.ctors)
                test.extend(clazz.methods)

                for m in test:
                    if "java.io.File" in m.args:
                        has_file.add(m)
                    if "java.io.FileDescriptor" in m.args or "android.os.ParcelFileDescriptor" in m.args or "java.io.InputStream" in m.args or "java.io.OutputStream" in m.args:
                        has_stream.add(m.name)

                for m in has_file:
                    if m.name not in has_stream:
                        warn(clazz, m, "M10", "Methods accepting File should also accept FileDescriptor or streams")
        */

        var hasFile: MutableSet<MethodItem>? = null
        var hasStream: MutableSet<String>? = null
        for (method in methodsAndConstructors) {
            for (parameter in method.parameters()) {
                val type = parameter.type().toTypeString()
                when (type) {
                    "java.io.File" -> {
                        val set = hasFile ?: run {
                            val new = mutableSetOf<MethodItem>()
                            hasFile = new
                            new
                        }
                        set.add(method)
                    }
                    "java.io.FileDescriptor",
                    "android.os.ParcelFileDescriptor",
                    "java.io.InputStream",
                    "java.io.OutputStream" -> {
                        val set = hasStream ?: run {
                            val new = mutableSetOf<String>()
                            hasStream = new
                            new
                        }
                        set.add(method.name())
                    }
                }
            }
        }
        val files = hasFile
        if (files != null) {
            val streams = hasStream
            for (method in files) {
                if (streams == null || !streams.contains(method.name())) {
                    report(
                        STREAM_FILES, method,
                        "Methods accepting `File` should also accept `FileDescriptor` or streams: ${method.describe()}"
                    )
                }
            }
        }
    }

    private fun checkManagerList(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_manager_list(clazz):
                """Verifies that managers return List<? extends Parcelable> instead of arrays."""

                if not clazz.name.endswith("Manager"): return

                for m in clazz.methods:
                    if m.typ.startswith("android.") and m.typ.endswith("[]"):
                        warn(clazz, m, None, "Methods should return List<? extends Parcelable> instead of Parcelable[] to support ParceledListSlice under the hood")
        */
        if (!cls.simpleName().endsWith("Manager")) {
            return
        }
        for (method in methods) {
            val returnType = method.returnType() ?: continue
            if (returnType.primitive) {
                return
            }
            val type = returnType.toTypeString()
            if (type.startsWith("android.") && returnType.isArray()) {
                report(
                    PARCELABLE_LIST, method,
                    "Methods should return `List<? extends Parcelable>` instead of `Parcelable[]` to support `ParceledListSlice` under the hood: ${method.describe()}"
                )
            }
        }
    }

    private fun checkAbstractInner(cls: ClassItem) {
        /*
            def verify_abstract_inner(clazz):
                """Verifies that abstract inner classes are static."""

                if re.match(".+?\.[A-Z][^\.]+\.[A-Z]", clazz.fullname):
                    if " abstract " in clazz.raw and " static " not in clazz.raw:
                        warn(clazz, None, None, "Abstract inner classes should be static to improve testability")
        */
        if (!cls.isTopLevelClass() && cls.isClass() && cls.modifiers.isAbstract() && !cls.modifiers.isStatic()) {
            report(
                ABSTRACT_INNER, cls,
                "Abstract inner classes should be static to improve testability: ${cls.describe()}"
            )
        }
    }

    private fun checkRuntimeExceptions(
        methodsAndConstructors: Sequence<MethodItem>,
        filterReference: Predicate<Item>
    ) {
        /*
            def verify_runtime_exceptions(clazz):
                """Verifies that runtime exceptions aren't listed in throws."""

                banned = [
                    "java.lang.NullPointerException",
                    "java.lang.ClassCastException",
                    "java.lang.IndexOutOfBoundsException",
                    "java.lang.reflect.UndeclaredThrowableException",
                    "java.lang.reflect.MalformedParametersException",
                    "java.lang.reflect.MalformedParameterizedTypeException",
                    "java.lang.invoke.WrongMethodTypeException",
                    "java.lang.EnumConstantNotPresentException",
                    "java.lang.IllegalMonitorStateException",
                    "java.lang.SecurityException",
                    "java.lang.UnsupportedOperationException",
                    "java.lang.annotation.AnnotationTypeMismatchException",
                    "java.lang.annotation.IncompleteAnnotationException",
                    "java.lang.TypeNotPresentException",
                    "java.lang.IllegalStateException",
                    "java.lang.ArithmeticException",
                    "java.lang.IllegalArgumentException",
                    "java.lang.ArrayStoreException",
                    "java.lang.NegativeArraySizeException",
                    "java.util.MissingResourceException",
                    "java.util.EmptyStackException",
                    "java.util.concurrent.CompletionException",
                    "java.util.concurrent.RejectedExecutionException",
                    "java.util.IllformedLocaleException",
                    "java.util.ConcurrentModificationException",
                    "java.util.NoSuchElementException",
                    "java.io.UncheckedIOException",
                    "java.time.DateTimeException",
                    "java.security.ProviderException",
                    "java.nio.BufferUnderflowException",
                    "java.nio.BufferOverflowException",
                ]

                examine = clazz.ctors + clazz.methods
                for m in examine:
                    for t in m.throws:
                        if t in banned:
                            error(clazz, m, None, "Methods must not mention RuntimeException subclasses in throws clauses")

        */
        for (method in methodsAndConstructors) {
            for (throws in method.filteredThrowsTypes(filterReference)) {
                when (throws.qualifiedName()) {
                    "java.lang.NullPointerException",
                    "java.lang.ClassCastException",
                    "java.lang.IndexOutOfBoundsException",
                    "java.lang.reflect.UndeclaredThrowableException",
                    "java.lang.reflect.MalformedParametersException",
                    "java.lang.reflect.MalformedParameterizedTypeException",
                    "java.lang.invoke.WrongMethodTypeException",
                    "java.lang.EnumConstantNotPresentException",
                    "java.lang.IllegalMonitorStateException",
                    "java.lang.SecurityException",
                    "java.lang.UnsupportedOperationException",
                    "java.lang.annotation.AnnotationTypeMismatchException",
                    "java.lang.annotation.IncompleteAnnotationException",
                    "java.lang.TypeNotPresentException",
                    "java.lang.IllegalStateException",
                    "java.lang.ArithmeticException",
                    "java.lang.IllegalArgumentException",
                    "java.lang.ArrayStoreException",
                    "java.lang.NegativeArraySizeException",
                    "java.util.MissingResourceException",
                    "java.util.EmptyStackException",
                    "java.util.concurrent.CompletionException",
                    "java.util.concurrent.RejectedExecutionException",
                    "java.util.IllformedLocaleException",
                    "java.util.ConcurrentModificationException",
                    "java.util.NoSuchElementException",
                    "java.io.UncheckedIOException",
                    "java.time.DateTimeException",
                    "java.security.ProviderException",
                    "java.nio.BufferUnderflowException",
                    "java.nio.BufferOverflowException" -> {
                        report(
                            BANNED_THROW, method,
                            "Methods must not mention RuntimeException subclasses in throws clauses (was `${throws.qualifiedName()}`)"
                        )
                    }
                }
            }
        }
    }

    private fun checkError(cls: ClassItem, superClass: ClassItem?) {
        /*
            def verify_error(clazz):
                """Verifies that we always use Exception instead of Error."""
                if not clazz.extends: return
                if clazz.extends.endswith("Error"):
                    error(clazz, None, None, "Trouble must be reported through an Exception, not Error")
                if clazz.extends.endswith("Exception") and not clazz.name.endswith("Exception"):
                    error(clazz, None, None, "Exceptions must be named FooException")
        */
        superClass ?: return
        if (superClass.simpleName().endsWith("Error")) {
            report(
                EXTENDS_ERROR, cls,
                "Trouble must be reported through an `Exception`, not an `Error` (`${cls.simpleName()}` extends `${superClass.simpleName()}`)"
            )
        }
        if (superClass.simpleName().endsWith("Exception") && !cls.simpleName().endsWith("Exception")) {
            report(
                EXCEPTION_NAME, cls,
                "Exceptions must be named `FooException`, was `${cls.simpleName()}`"
            )
        }
    }

    private fun checkUnits(method: MethodItem) {
        /*
            def verify_units(clazz):
                """Verifies that we use consistent naming for units."""

                # If we find K, recommend replacing with V
                bad = {
                    "Ns": "Nanos",
                    "Ms": "Millis or Micros",
                    "Sec": "Seconds", "Secs": "Seconds",
                    "Hr": "Hours", "Hrs": "Hours",
                    "Mo": "Months", "Mos": "Months",
                    "Yr": "Years", "Yrs": "Years",
                    "Byte": "Bytes", "Space": "Bytes",
                }

                for m in clazz.methods:
                    if m.typ not in ["short","int","long"]: continue
                    for k, v in bad.iteritems():
                        if m.name.endswith(k):
                            error(clazz, m, None, "Expected method name units to be " + v)
                    if m.name.endswith("Nanos") or m.name.endswith("Micros"):
                        warn(clazz, m, None, "Returned time values are strongly encouraged to be in milliseconds unless you need the extra precision")
                    if m.name.endswith("Seconds"):
                        error(clazz, m, None, "Returned time values must be in milliseconds")

                for m in clazz.methods:
                    typ = m.typ
                    if typ == "void":
                        if len(m.args) != 1: continue
                        typ = m.args[0]

                    if m.name.endswith("Fraction") and typ != "float":
                        error(clazz, m, None, "Fractions must use floats")
                    if m.name.endswith("Percentage") and typ != "int":
                        error(clazz, m, None, "Percentage must use ints")

        */
        val returnType = method.returnType() ?: return
        var type = returnType.toTypeString()
        val name = method.name()
        if (type == "int" || type == "long" || type == "short") {
            if (badUnits.any { name.endsWith(it.key) }) {
                val badUnit = badUnits.keys.find { name.endsWith(it) }
                val value = badUnits[badUnit]
                report(
                    METHOD_NAME_UNITS, method,
                    "Expected method name units to be `$value`, was `$badUnit` in `$name`"
                )
            } else if (name.endsWith("Nanos") || name.endsWith("Micros")) {
                report(
                    METHOD_NAME_UNITS, method,
                    "Returned time values are strongly encouraged to be in milliseconds unless you need the extra precision, was `$name`"
                )
            } else if (name.endsWith("Seconds")) {
                report(
                    METHOD_NAME_UNITS, method,
                    "Returned time values must be in milliseconds, was `$name`"
                )
            }
        } else if (type == "void") {
            if (method.parameters().size != 1) {
                return
            }
            type = method.parameters()[0].type().toTypeString()
        }
        if (name.endsWith("Fraction") && type != "float") {
            report(
                FRACTION_FLOAT, method,
                "Fractions must use floats, was `$type` in `$name`"
            )
        } else if (name.endsWith("Percentage") && type != "int") {
            report(
                PERCENTAGE_INT, method,
                "Percentage must use ints, was `$type` in `$name`"
            )
        }
    }

    private fun checkCloseable(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_closable(clazz):
                """Verifies that classes are AutoClosable."""
                if "implements java.lang.AutoCloseable" in clazz.raw: return
                if "implements java.io.Closeable" in clazz.raw: return

                for m in clazz.methods:
                    if len(m.args) > 0: continue
                    if m.name in ["close","release","destroy","finish","finalize","disconnect","shutdown","stop","free","quit"]:
                        warn(clazz, m, None, "Classes that release resources should implement AutoClosable and CloseGuard")
                        return
         */
        // AutoClosable has been added in API 19, so libraries with minSdkVersion <19 cannot use it. If the version
        // is not set, then keep the check enabled.
        val minSdkVersion = codebase.getMinSdkVersion()
        if (minSdkVersion is SetMinSdkVersion && minSdkVersion.value < 19) {
            return
        }

        val foundMethods = methods.filter { method ->
            when (method.name()) {
                "close", "release", "destroy", "finish", "finalize", "disconnect", "shutdown", "stop", "free", "quit" -> true
                else -> false
            }
        }
        if (foundMethods.iterator().hasNext() && !cls.implements("java.lang.AutoCloseable")) { // includes java.io.Closeable
            val foundMethodsDescriptions = foundMethods.joinToString { method -> "${method.name()}()" }
            report(
                NOT_CLOSEABLE, cls,
                "Classes that release resources ($foundMethodsDescriptions) should implement AutoClosable and CloseGuard: ${cls.describe()}"
            )
        }
    }

    private fun checkNotKotlinOperator(methods: Sequence<MethodItem>) {
        /*
            def verify_method_name_not_kotlin_operator(clazz):
                """Warn about method names which become operators in Kotlin."""

                binary = set()

                def unique_binary_op(m, op):
                    if op in binary:
                        error(clazz, m, None, "Only one of '{0}' and '{0}Assign' methods should be present for Kotlin".format(op))
                    binary.add(op)

                for m in clazz.methods:
                    if 'static' in m.split:
                        continue

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#unary-prefix-operators
                    if m.name in ["unaryPlus", "unaryMinus", "not"] and len(m.args) == 0:
                        warn(clazz, m, None, "Method can be invoked as a unary operator from Kotlin")

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#increments-and-decrements
                    if m.name in ["inc", "dec"] and len(m.args) == 0 and m.typ != "void":
                        # This only applies if the return type is the same or a subtype of the enclosing class, but we have no
                        # practical way of checking that relationship here.
                        warn(clazz, m, None, "Method can be invoked as a pre/postfix inc/decrement operator from Kotlin")

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#arithmetic
                    if m.name in ["plus", "minus", "times", "div", "rem", "mod", "rangeTo"] and len(m.args) == 1:
                        warn(clazz, m, None, "Method can be invoked as a binary operator from Kotlin")
                        unique_binary_op(m, m.name)

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#in
                    if m.name == "contains" and len(m.args) == 1 and m.typ == "boolean":
                        warn(clazz, m, None, "Method can be invoked as a "in" operator from Kotlin")

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#indexed
                    if (m.name == "get" and len(m.args) > 0) or (m.name == "set" and len(m.args) > 1):
                        warn(clazz, m, None, "Method can be invoked with an indexing operator from Kotlin")

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#invoke
                    if m.name == "invoke":
                        warn(clazz, m, None, "Method can be invoked with function call syntax from Kotlin")

                    # https://kotlinlang.org/docs/reference/operator-overloading.html#assignments
                    if m.name in ["plusAssign", "minusAssign", "timesAssign", "divAssign", "remAssign", "modAssign"] \
                            and len(m.args) == 1 \
                            and m.typ == "void":
                        warn(clazz, m, None, "Method can be invoked as a compound assignment operator from Kotlin")
                        unique_binary_op(m, m.name[:-6])  # Remove "Assign" suffix

         */

        fun flagKotlinOperator(method: MethodItem, message: String) {
            if (method.isKotlin()) {
                report(
                    KOTLIN_OPERATOR, method,
                    "Note that adding the `operator` keyword would allow calling this method using operator syntax")
            } else {
                report(
                    KOTLIN_OPERATOR, method,
                    "$message (this is usually desirable; just make sure it makes sense for this type of object)"
                )
            }
        }

        for (method in methods) {
            if (method.modifiers.isStatic() || method.modifiers.isOperator() || method.superMethods().isNotEmpty()) {
                continue
            }
            val name = method.name()
            when (name) {
                // https://kotlinlang.org/docs/reference/operator-overloading.html#unary-prefix-operators
                "unaryPlus", "unaryMinus", "not" -> {
                    if (method.parameters().isEmpty()) {
                        flagKotlinOperator(
                            method, "Method can be invoked as a unary operator from Kotlin: `$name`"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#increments-and-decrements
                "inc", "dec" -> {
                    if (method.parameters().isEmpty() && method.returnType()?.toTypeString() != "void") {
                        flagKotlinOperator(
                            method, "Method can be invoked as a pre/postfix inc/decrement operator from Kotlin: `$name`"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#arithmetic
                "plus", "minus", "times", "div", "rem", "mod", "rangeTo" -> {
                    if (method.parameters().size == 1) {
                        flagKotlinOperator(
                            method, "Method can be invoked as a binary operator from Kotlin: `$name`"
                        )
                    }
                    val assignName = name + "Assign"

                    if (methods.any {
                            it.name() == assignName &&
                                it.parameters().size == 1 &&
                                it.returnType()?.toTypeString() == "void"
                        }) {
                        report(
                            UNIQUE_KOTLIN_OPERATOR, method,
                            "Only one of `$name` and `${name}Assign` methods should be present for Kotlin"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#in
                "contains" -> {
                    if (method.parameters().size == 1 && method.returnType()?.toTypeString() == "boolean") {
                        flagKotlinOperator(
                            method, "Method can be invoked as a \"in\" operator from Kotlin: `$name`"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#indexed
                "get" -> {
                    if (method.parameters().isNotEmpty()) {
                        flagKotlinOperator(
                            method, "Method can be invoked with an indexing operator from Kotlin: `$name`"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#indexed
                "set" -> {
                    if (method.parameters().size > 1) {
                        flagKotlinOperator(
                            method, "Method can be invoked with an indexing operator from Kotlin: `$name`"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#invoke
                "invoke" -> {
                    if (method.parameters().size > 1) {
                        flagKotlinOperator(
                            method, "Method can be invoked with function call syntax from Kotlin: `$name`"
                        )
                    }
                }
                // https://kotlinlang.org/docs/reference/operator-overloading.html#assignments
                "plusAssign", "minusAssign", "timesAssign", "divAssign", "remAssign", "modAssign" -> {
                    if (method.parameters().size == 1 && method.returnType()?.toTypeString() == "void") {
                        flagKotlinOperator(
                            method, "Method can be invoked as a compound assignment operator from Kotlin: `$name`"
                        )
                    }
                }
            }
        }
    }

    private fun checkCollectionsOverArrays(type: TypeItem, typeString: String, item: Item) {
        /*
            def verify_collections_over_arrays(clazz):
                """Warn that [] should be Collections."""

                safe = ["java.lang.String[]","byte[]","short[]","int[]","long[]","float[]","double[]","boolean[]","char[]"]
                for m in clazz.methods:
                    if m.typ.endswith("[]") and m.typ not in safe:
                        warn(clazz, m, None, "Method should return Collection<> (or subclass) instead of raw array")
                    for arg in m.args:
                        if arg.endswith("[]") and arg not in safe:
                            warn(clazz, m, None, "Method argument should be Collection<> (or subclass) instead of raw array")

         */

        if (!type.isArray() || typeString.endsWith("...")) {
            return
        }

        when (typeString) {
            "java.lang.String[]",
            "byte[]",
            "short[]",
            "int[]",
            "long[]",
            "float[]",
            "double[]",
            "boolean[]",
            "char[]" -> {
                return
            }
            else -> {
                val action = when (item) {
                    is MethodItem -> {
                        if (item.name() == "values" && item.containingClass().isEnum()) {
                            return
                        }
                        "Method should return"
                    }
                    is FieldItem -> "Field should be"
                    else -> "Method parameter should be"
                }
                val component = type.asClass()?.simpleName() ?: ""
                report(
                    ARRAY_RETURN, item,
                    "$action Collection<$component> (or subclass) instead of raw array; was `$typeString`"
                )
            }
        }
    }

    private fun checkUserHandle(cls: ClassItem, methods: Sequence<MethodItem>) {
        /*
            def verify_user_handle(clazz):
                """Methods taking UserHandle should be ForUser or AsUser."""
                if clazz.name.endswith("Listener") or clazz.name.endswith("Callback") or clazz.name.endswith("Callbacks"): return
                if clazz.fullname == "android.app.admin.DeviceAdminReceiver": return
                if clazz.fullname == "android.content.pm.LauncherApps": return
                if clazz.fullname == "android.os.UserHandle": return
                if clazz.fullname == "android.os.UserManager": return

                for m in clazz.methods:
                    if re.match("on[A-Z]+", m.name): continue

                    has_arg = "android.os.UserHandle" in m.args
                    has_name = m.name.endswith("AsUser") or m.name.endswith("ForUser")

                    if clazz.fullname.endswith("Manager") and has_arg:
                        warn(clazz, m, None, "When a method overload is needed to target a specific "
                             "UserHandle, callers should be directed to use "
                             "Context.createPackageContextAsUser() and re-obtain the relevant "
                             "Manager, and no new API should be added")
                    elif has_arg and not has_name:
                        warn(clazz, m, None, "Method taking UserHandle should be named 'doFooAsUser' "
                             "or 'queryFooForUser'")

         */
        val qualifiedName = cls.qualifiedName()
        if (qualifiedName == "android.app.admin.DeviceAdminReceiver" ||
            qualifiedName == "android.content.pm.LauncherApps" ||
            qualifiedName == "android.os.UserHandle" ||
            qualifiedName == "android.os.UserManager"
        ) {
            return
        }

        for (method in methods) {
            val parameters = method.parameters()
            if (parameters.isEmpty()) {
                continue
            }
            val name = method.name()
            if (name.startsWith("on") && onCallbackNamePattern.matches(name)) {
                continue
            }
            val hasArg = parameters.any { it.type().toTypeString() == "android.os.UserHandle" }
            if (!hasArg) {
                continue
            }
            if (qualifiedName.endsWith("Manager")) {
                report(
                    USER_HANDLE, method,
                    "When a method overload is needed to target a specific " +
                        "UserHandle, callers should be directed to use " +
                        "Context.createPackageContextAsUser() and re-obtain the relevant " +
                        "Manager, and no new API should be added"
                )
            } else if (!(name.endsWith("AsUser") || name.endsWith("ForUser"))) {
                report(
                    USER_HANDLE_NAME, method,
                    "Method taking UserHandle should be named `doFooAsUser` or `queryFooForUser`, was `$name`"
                )
            }
        }
    }

    private fun checkParams(cls: ClassItem) {
        /*
            def verify_params(clazz):
                """Parameter classes should be 'Params'."""
                if clazz.name.endswith("Params"): return
                if clazz.fullname == "android.app.ActivityOptions": return
                if clazz.fullname == "android.app.BroadcastOptions": return
                if clazz.fullname == "android.os.Bundle": return
                if clazz.fullname == "android.os.BaseBundle": return
                if clazz.fullname == "android.os.PersistableBundle": return

                bad = ["Param","Parameter","Parameters","Args","Arg","Argument","Arguments","Options","Bundle"]
                for b in bad:
                    if clazz.name.endswith(b):
                        error(clazz, None, None, "Classes holding a set of parameters should be called 'FooParams'")
         */

        val qualifiedName = cls.qualifiedName()
        for (suffix in badParameterClassNames) {
            if (qualifiedName.endsWith(suffix) && !((qualifiedName.endsWith("Params") ||
                    qualifiedName == "android.app.ActivityOptions" ||
                    qualifiedName == "android.app.BroadcastOptions" ||
                    qualifiedName == "android.os.Bundle" ||
                    qualifiedName == "android.os.BaseBundle" ||
                    qualifiedName == "android.os.PersistableBundle"))
            ) {
                report(
                    USER_HANDLE_NAME, cls,
                    "Classes holding a set of parameters should be called `FooParams`, was `${cls.simpleName()}`"
                )
            }
        }
    }

    private fun checkServices(field: FieldItem) {
        /*
            def verify_services(clazz):
                """Service name should be FOO_BAR_SERVICE = 'foo_bar'."""
                if clazz.fullname != "android.content.Context": return

                for f in clazz.fields:
                    if f.typ != "java.lang.String": continue
                    found = re.match(r"([A-Z_]+)_SERVICE", f.name)
                    if found:
                        expected = found.group(1).lower()
                        if f.value != expected:
                            error(clazz, f, "C4", "Inconsistent service value; expected '%s'" % (expected))
         */
        val type = field.type()
        if (!type.isString() || !field.modifiers.isFinal() || !field.modifiers.isStatic() ||
            field.containingClass().qualifiedName() != "android.content.Context") {
            return
        }
        val name = field.name()
        val endsWithService = name.endsWith("_SERVICE")
        val value = field.initialValue(requireConstant = true) as? String

        if (value == null) {
            val mustEndInService =
                if (!endsWithService) " and its name must end with `_SERVICE`" else ""

            report(
                SERVICE_NAME, field, "Non-constant service constant `$name`. Must be static," +
                    " final and initialized with a String literal$mustEndInService."
            )
            return
        }

        if (name.endsWith("_MANAGER_SERVICE")) {
            report(
                SERVICE_NAME, field,
                "Inconsistent service constant name; expected " +
                    "`${name.removeSuffix("_MANAGER_SERVICE")}_SERVICE`, was `$name`"
            )
        } else if (endsWithService) {
            val service = name.substring(0, name.length - "_SERVICE".length).toLowerCase(Locale.US)
            if (service != value) {
                report(
                    SERVICE_NAME, field,
                    "Inconsistent service value; expected `$service`, was `$value` (Note: Do not" +
                        " change the name of already released services, which will break tools" +
                        " using `adb shell dumpsys`." +
                        " Instead add `@SuppressLint(\"${SERVICE_NAME.name}\"))`"
                )
            }
        } else {
            val valueUpper = value.toUpperCase(Locale.US)
            report(
                SERVICE_NAME, field, "Inconsistent service constant name;" +
                    " expected `${valueUpper}_SERVICE`, was `$name`"
            )
        }
    }

    private fun checkTense(method: MethodItem) {
        /*
            def verify_tense(clazz):
                """Verify tenses of method names."""
                if clazz.fullname.startswith("android.opengl"): return

                for m in clazz.methods:
                    if m.name.endswith("Enable"):
                        warn(clazz, m, None, "Unexpected tense; probably meant 'enabled'")
         */
        val name = method.name()
        if (name.endsWith("Enable")) {
            if (method.containingClass().qualifiedName().startsWith("android.opengl")) {
                return
            }
            report(
                METHOD_NAME_TENSE, method,
                "Unexpected tense; probably meant `enabled`, was `$name`"
            )
        }
    }

    private fun checkIcu(type: TypeItem, typeString: String, item: Item) {
        /*
            def verify_icu(clazz):
                """Verifies that richer ICU replacements are used."""
                better = {
                    "java.util.TimeZone": "android.icu.util.TimeZone",
                    "java.util.Calendar": "android.icu.util.Calendar",
                    "java.util.Locale": "android.icu.util.ULocale",
                    "java.util.ResourceBundle": "android.icu.util.UResourceBundle",
                    "java.util.SimpleTimeZone": "android.icu.util.SimpleTimeZone",
                    "java.util.StringTokenizer": "android.icu.util.StringTokenizer",
                    "java.util.GregorianCalendar": "android.icu.util.GregorianCalendar",
                    "java.lang.Character": "android.icu.lang.UCharacter",
                    "java.text.BreakIterator": "android.icu.text.BreakIterator",
                    "java.text.Collator": "android.icu.text.Collator",
                    "java.text.DecimalFormatSymbols": "android.icu.text.DecimalFormatSymbols",
                    "java.text.NumberFormat": "android.icu.text.NumberFormat",
                    "java.text.DateFormatSymbols": "android.icu.text.DateFormatSymbols",
                    "java.text.DateFormat": "android.icu.text.DateFormat",
                    "java.text.SimpleDateFormat": "android.icu.text.SimpleDateFormat",
                    "java.text.MessageFormat": "android.icu.text.MessageFormat",
                    "java.text.DecimalFormat": "android.icu.text.DecimalFormat",
                }

                for m in clazz.ctors + clazz.methods:
                    types = []
                    types.extend(m.typ)
                    types.extend(m.args)
                    for arg in types:
                        if arg in better:
                            warn(clazz, m, None, "Type %s should be replaced with richer ICU type %s" % (arg, better[arg]))
         */
        if (type.primitive) {
            return
        }
        // ICU types have been added in API 24, so libraries with minSdkVersion <24 cannot use them.
        // If the version is not set, then keep the check enabled.
        val minSdkVersion = codebase.getMinSdkVersion()
        if (minSdkVersion is SetMinSdkVersion && minSdkVersion.value < 24) {
            return
        }
        val better = when (typeString) {
            "java.util.TimeZone" -> "android.icu.util.TimeZone"
            "java.util.Calendar" -> "android.icu.util.Calendar"
            "java.util.Locale" -> "android.icu.util.ULocale"
            "java.util.ResourceBundle" -> "android.icu.util.UResourceBundle"
            "java.util.SimpleTimeZone" -> "android.icu.util.SimpleTimeZone"
            "java.util.StringTokenizer" -> "android.icu.util.StringTokenizer"
            "java.util.GregorianCalendar" -> "android.icu.util.GregorianCalendar"
            "java.lang.Character" -> "android.icu.lang.UCharacter"
            "java.text.BreakIterator" -> "android.icu.text.BreakIterator"
            "java.text.Collator" -> "android.icu.text.Collator"
            "java.text.DecimalFormatSymbols" -> "android.icu.text.DecimalFormatSymbols"
            "java.text.NumberFormat" -> "android.icu.text.NumberFormat"
            "java.text.DateFormatSymbols" -> "android.icu.text.DateFormatSymbols"
            "java.text.DateFormat" -> "android.icu.text.DateFormat"
            "java.text.SimpleDateFormat" -> "android.icu.text.SimpleDateFormat"
            "java.text.MessageFormat" -> "android.icu.text.MessageFormat"
            "java.text.DecimalFormat" -> "android.icu.text.DecimalFormat"
            else -> return
        }
        report(
            USE_ICU, item,
            "Type `$typeString` should be replaced with richer ICU type `$better`"
        )
    }

    private fun checkClone(method: MethodItem) {
        /*
            def verify_clone(clazz):
                """Verify that clone() isn't implemented; see EJ page 61."""
                for m in clazz.methods:
                    if m.name == "clone":
                        error(clazz, m, None, "Provide an explicit copy constructor instead of implementing clone()")
         */
        if (method.name() == "clone" && method.parameters().isEmpty()) {
            report(
                NO_CLONE, method,
                "Provide an explicit copy constructor instead of implementing `clone()`"
            )
        }
    }

    private fun checkPfd(type: String, item: Item) {
        /*
            def verify_pfd(clazz):
                """Verify that android APIs use PFD over FD."""
                examine = clazz.ctors + clazz.methods
                for m in examine:
                    if m.typ == "java.io.FileDescriptor":
                        error(clazz, m, "FW11", "Must use ParcelFileDescriptor")
                    if m.typ == "int":
                        if "Fd" in m.name or "FD" in m.name or "FileDescriptor" in m.name:
                            error(clazz, m, "FW11", "Must use ParcelFileDescriptor")
                    for arg in m.args:
                        if arg == "java.io.FileDescriptor":
                            error(clazz, m, "FW11", "Must use ParcelFileDescriptor")

                for f in clazz.fields:
                    if f.typ == "java.io.FileDescriptor":
                        error(clazz, f, "FW11", "Must use ParcelFileDescriptor")

         */
        if (item.containingClass()?.qualifiedName() in lowLevelFileClassNames ||
            isServiceDumpMethod(item)) {
            return
        }

        if (type == "java.io.FileDescriptor") {
            report(
                USE_PARCEL_FILE_DESCRIPTOR, item,
                "Must use ParcelFileDescriptor instead of FileDescriptor in ${item.describe()}"
            )
        } else if (type == "int" && item is MethodItem) {
            val name = item.name()
            if (name.contains("Fd") || name.contains("FD") || name.contains("FileDescriptor", ignoreCase = true)) {
                report(
                    USE_PARCEL_FILE_DESCRIPTOR, item,
                    "Must use ParcelFileDescriptor instead of FileDescriptor in ${item.describe()}"
                )
            }
        }
    }

    private fun checkNumbers(type: String, item: Item) {
        /*
            def verify_numbers(clazz):
                """Discourage small numbers types like short and byte."""

                discouraged = ["short","byte"]

                for c in clazz.ctors:
                    for arg in c.args:
                        if arg in discouraged:
                            warn(clazz, c, "FW12", "Should avoid odd sized primitives; use int instead")

                for f in clazz.fields:
                    if f.typ in discouraged:
                        warn(clazz, f, "FW12", "Should avoid odd sized primitives; use int instead")

                for m in clazz.methods:
                    if m.typ in discouraged:
                        warn(clazz, m, "FW12", "Should avoid odd sized primitives; use int instead")
                    for arg in m.args:
                        if arg in discouraged:
                            warn(clazz, m, "FW12", "Should avoid odd sized primitives; use int instead")
         */
        if (type == "short" || type == "byte") {
            report(
                NO_BYTE_OR_SHORT, item,
                "Should avoid odd sized primitives; use `int` instead of `$type` in ${item.describe()}"
            )
        }
    }

    private fun checkSingleton(
        cls: ClassItem,
        methods: Sequence<MethodItem>,
        constructors: Sequence<ConstructorItem>
    ) {
        /*
            def verify_singleton(clazz):
                """Catch singleton objects with constructors."""

                singleton = False
                for m in clazz.methods:
                    if m.name.startswith("get") and m.name.endswith("Instance") and " static " in m.raw:
                        singleton = True

                if singleton:
                    for c in clazz.ctors:
                        error(clazz, c, None, "Singleton classes should use getInstance() methods")
         */
        if (constructors.none()) {
            return
        }
        if (methods.any { it.name().startsWith("get") && it.name().endsWith("Instance") && it.modifiers.isStatic() }) {
            for (constructor in constructors) {
                report(
                    SINGLETON_CONSTRUCTOR, constructor,
                    "Singleton classes should use `getInstance()` methods: `${cls.simpleName()}`"
                )
            }
        }
    }

    private fun checkExtends(cls: ClassItem) {
        // Call cls.superClass().extends() instead of cls.extends() since extends returns true for self
        val superCls = cls.superClass() ?: return
        if (superCls.extends("android.os.AsyncTask")) {
            report(
                FORBIDDEN_SUPER_CLASS, cls,
                "${cls.simpleName()} should not extend `AsyncTask`. AsyncTask is an implementation detail. Expose a listener or, in androidx, a `ListenableFuture` API instead"
            )
        }
        if (superCls.extends("android.app.Activity")) {
            report(
                FORBIDDEN_SUPER_CLASS, cls,
                "${cls.simpleName()} should not extend `Activity`. Activity subclasses are impossible to compose. Expose a composable API instead."
            )
        }
        badFutureTypes.firstOrNull { cls.extendsOrImplements(it) }?.let {
            val extendOrImplement = if (cls.extends(it)) "extend" else "implement"
            report(
                BAD_FUTURE, cls, "${cls.simpleName()} should not $extendOrImplement `$it`." +
                    " In AndroidX, use (but do not extend) ListenableFuture. In platform, use a combination of Consumer<T>, Executor, and CancellationSignal`."
            )
        }
    }

    private fun checkTypedef(cls: ClassItem) {
        /*
        def verify_intdef(clazz):
            """intdefs must be @hide, because the constant names cannot be stored in
               the stubs (only the values are, which is not useful)"""
            if "@interface" not in clazz.split:
                return
            if "@IntDef" in clazz.annotations or "@LongDef" in clazz.annotations:
                error(clazz, None, None, "@IntDef and @LongDef annotations must be @hide")
         */
        if (cls.isAnnotationType()) {
            cls.modifiers.annotations().firstOrNull { it.isTypeDefAnnotation() }?.let {
                report(PUBLIC_TYPEDEF, cls, "Don't expose ${AnnotationItem.simpleName(it)}: ${cls.simpleName()} must be hidden.")
            }
        }
    }

    private fun checkUri(typeString: String, item: Item) {
        /*
        def verify_uris(clazz):
            bad = ["java.net.URL", "java.net.URI", "android.net.URL"]

            for f in clazz.fields:
                if f.typ in bad:
                    error(clazz, f, None, "Field must be android.net.Uri instead of " + f.typ)

            for m in clazz.methods + clazz.ctors:
                if m.typ in bad:
                    error(clazz, m, None, "Must return android.net.Uri instead of " + m.typ)
                for arg in m.args:
                    if arg in bad:
                        error(clazz, m, None, "Argument must take android.net.Uri instead of " + arg)
         */
        badUriTypes.firstOrNull { typeString.contains(it) }?.let {
            report(
                ANDROID_URI, item, "Use android.net.Uri instead of $it (${item.describe()})"
            )
        }
    }

    private fun checkFutures(typeString: String, item: Item) {
        badFutureTypes.firstOrNull { typeString.contains(it) }?.let {
            report(
                BAD_FUTURE, item, "Use ListenableFuture (library), " +
                    "or a combination of Consumer<T>, Executor, and CancellationSignal (platform) instead of $it (${item.describe()})"
            )
        }
    }

    private fun isInteresting(cls: ClassItem): Boolean {
        val name = cls.qualifiedName()
        for (prefix in options.checkApiIgnorePrefix) {
            if (name.startsWith(prefix)) {
                return false
            }
        }
        return true
    }

    companion object {

        private val badParameterClassNames = listOf(
            "Param", "Parameter", "Parameters", "Args", "Arg", "Argument", "Arguments", "Options", "Bundle"
        )

        private val badUriTypes = listOf("java.net.URL", "java.net.URI", "android.net.URL")

        private val badFutureTypes = listOf(
            "java.util.concurrent.CompletableFuture",
            "java.util.concurrent.Future"
        )

        /**
         * Classes for manipulating file descriptors directly, where using ParcelFileDescriptor
         * isn't required
         */
        private val lowLevelFileClassNames = listOf(
            "android.os.FileUtils",
            "android.system.Os",
            "android.net.util.SocketUtils",
            "android.os.NativeHandle",
            "android.os.ParcelFileDescriptor"
        )

        /**
         * Classes which already use bare fields extensively, and bare fields are thus allowed for
         * consistency with existing API surface.
         */
        private val classesWithBareFields = listOf(
            "android.app.ActivityManager.RecentTaskInfo",
            "android.app.Notification",
            "android.content.pm.ActivityInfo",
            "android.content.pm.ApplicationInfo",
            "android.content.pm.ComponentInfo",
            "android.content.pm.ResolveInfo",
            "android.content.pm.FeatureGroupInfo",
            "android.content.pm.InstrumentationInfo",
            "android.content.pm.PackageInfo",
            "android.content.pm.PackageItemInfo",
            "android.content.res.Configuration",
            "android.graphics.BitmapFactory.Options",
            "android.os.Message",
            "android.system.StructPollfd"
        )

        private val badUnits = mapOf(
            "Ns" to "Nanos",
            "Ms" to "Millis or Micros",
            "Sec" to "Seconds",
            "Secs" to "Seconds",
            "Hr" to "Hours",
            "Hrs" to "Hours",
            "Mo" to "Months",
            "Mos" to "Months",
            "Yr" to "Years",
            "Yrs" to "Years",
            "Byte" to "Bytes",
            "Space" to "Bytes"
        )
        private val uiPackageParts = listOf(
            "animation",
            "view",
            "graphics",
            "transition",
            "widget",
            "webkit"
        )

        private val constantNamePattern = Regex("[A-Z0-9_]+")
        private val internalNamePattern = Regex("[ms][A-Z0-9].*")
        private val fieldNamePattern = Regex("[a-z].*")
        private val onCallbackNamePattern = Regex("on[A-Z][a-z][a-zA-Z1-9]*")
        private val configFieldPattern = Regex("config_[a-z][a-zA-Z1-9]*")
        private val layoutFieldPattern = Regex("layout_[a-z][a-zA-Z1-9]*")
        private val stateFieldPattern = Regex("state_[a-z_]+")
        private val resourceFileFieldPattern = Regex("[a-z1-9_]+")
        private val resourceValueFieldPattern = Regex("[a-z][a-zA-Z1-9]*")
        private val styleFieldPattern = Regex("[A-Z][A-Za-z1-9]+(_[A-Z][A-Za-z1-9]+?)*")

        private val acronymPattern2 = Regex("([A-Z]){2,}")
        private val acronymPattern3 = Regex("([A-Z]){3,}")

        private val serviceDumpMethodParameterTypes =
            listOf("java.io.FileDescriptor", "java.io.PrintWriter", "java.lang.String[]")

        private fun isServiceDumpMethod(item: Item) = when (item) {
            is MethodItem -> isServiceDumpMethod(item)
            is ParameterItem -> isServiceDumpMethod(item.containingMethod())
            else -> false
        }

        private fun isServiceDumpMethod(item: MethodItem) = item.name() == "dump" &&
            item.containingClass().extends("android.app.Service") &&
            item.parameters().map { it.type().toTypeString() } == serviceDumpMethodParameterTypes

        private fun hasAcronyms(name: String): Boolean {
            // Require 3 capitals, or 2 if it's at the end of a word.
            val result = acronymPattern2.find(name) ?: return false
            return result.range.start == name.length - 2 || acronymPattern3.find(name) != null
        }

        private fun getFirstAcronym(name: String): String? {
            // Require 3 capitals, or 2 if it's at the end of a word.
            val result = acronymPattern2.find(name) ?: return null
            if (result.range.start == name.length - 2) {
                return name.substring(name.length - 2)
            }
            val result2 = acronymPattern3.find(name)
            return if (result2 != null) {
                name.substring(result2.range.start, result2.range.endInclusive + 1)
            } else {
                null
            }
        }

        /** for something like "HTMLWriter", returns "HtmlWriter" */
        private fun decapitalizeAcronyms(name: String): String {
            var s = name

            if (s.none { it.isLowerCase() }) {
                // The entire thing is capitalized. If so, just perform
                // normal capitalization, but try dropping _'s.
                return SdkVersionInfo.underlinesToCamelCase(s.toLowerCase(Locale.US)).capitalize()
            }

            while (true) {
                val acronym = getFirstAcronym(s) ?: return s
                val index = s.indexOf(acronym)
                if (index == -1) {
                    return s
                }
                // The last character, if not the end of the string, is probably the beginning of the
                // next word so capitalize it
                s = if (index == s.length - acronym.length) {
                    // acronym at the end of the word word
                    val decapitalized = acronym[0] + acronym.substring(1).toLowerCase(Locale.US)
                    s.replace(acronym, decapitalized)
                } else {
                    val replacement = acronym[0] + acronym.substring(
                        1,
                        acronym.length - 1
                    ).toLowerCase(Locale.US) + acronym[acronym.length - 1]
                    s.replace(acronym, replacement)
                }
            }
        }

        fun check(codebase: Codebase, oldCodebase: Codebase?, reporter: Reporter) {
            ApiLint(codebase, oldCodebase, reporter).check()
        }
    }
}
