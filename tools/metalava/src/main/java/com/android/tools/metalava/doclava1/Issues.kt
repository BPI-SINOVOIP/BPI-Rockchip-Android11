/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
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
package com.android.tools.metalava.doclava1

import com.android.sdklib.SdkVersionInfo
import com.android.tools.metalava.Severity
import java.util.ArrayList
import java.util.HashMap
import java.util.Locale
import kotlin.reflect.full.declaredMemberProperties

// Copied from doclava1 (and a bunch of stuff left alone preserving to have same error id's)
object Issues {
    private val allIssues: MutableList<Issue> = ArrayList(200)
    private val nameToIssue: MutableMap<String, Issue> = HashMap(200)
    private val idToIssue: MutableMap<Int, Issue> = HashMap(200)

    val PARSE_ERROR = Issue(1, Severity.ERROR)
    val ADDED_PACKAGE = Issue(2, Severity.WARNING, Category.COMPATIBILITY)
    val ADDED_CLASS = Issue(3, Severity.WARNING, Category.COMPATIBILITY)
    val ADDED_METHOD = Issue(4, Severity.WARNING, Category.COMPATIBILITY)
    val ADDED_FIELD = Issue(5, Severity.WARNING, Category.COMPATIBILITY)
    val ADDED_INTERFACE = Issue(6, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_PACKAGE = Issue(7, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_CLASS = Issue(8, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_METHOD = Issue(9, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_FIELD = Issue(10, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_INTERFACE = Issue(11, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_STATIC = Issue(12, Severity.WARNING, Category.COMPATIBILITY)
    val ADDED_FINAL = Issue(13, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_TRANSIENT = Issue(14, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_VOLATILE = Issue(15, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_TYPE = Issue(16, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_VALUE = Issue(17, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_SUPERCLASS = Issue(18, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_SCOPE = Issue(19, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_ABSTRACT = Issue(20, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_THROWS = Issue(21, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_NATIVE = Issue(22, Severity.HIDDEN, Category.COMPATIBILITY)
    val CHANGED_CLASS = Issue(23, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_DEPRECATED = Issue(24, Severity.WARNING, Category.COMPATIBILITY)
    val CHANGED_SYNCHRONIZED = Issue(25, Severity.WARNING, Category.COMPATIBILITY)
    val ADDED_FINAL_UNINSTANTIABLE = Issue(26, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_FINAL = Issue(27, Severity.WARNING, Category.COMPATIBILITY)
    val REMOVED_DEPRECATED_CLASS = Issue(28, REMOVED_CLASS, Category.COMPATIBILITY)
    val REMOVED_DEPRECATED_METHOD = Issue(29, REMOVED_METHOD, Category.COMPATIBILITY)
    val REMOVED_DEPRECATED_FIELD = Issue(30, REMOVED_FIELD, Category.COMPATIBILITY)
    val ADDED_ABSTRACT_METHOD = Issue(31, ADDED_METHOD, Category.COMPATIBILITY)
    val ADDED_REIFIED = Issue(32, Severity.WARNING, Category.COMPATIBILITY)

    // Issues in javadoc generation
    val UNRESOLVED_LINK = Issue(101, Severity.LINT, Category.DOCUMENTATION)
    val BAD_INCLUDE_TAG = Issue(102, Severity.LINT, Category.DOCUMENTATION)
    val UNKNOWN_TAG = Issue(103, Severity.LINT, Category.DOCUMENTATION)
    val UNKNOWN_PARAM_TAG_NAME = Issue(104, Severity.LINT, Category.DOCUMENTATION)
    val UNDOCUMENTED_PARAMETER = Issue(105, Severity.HIDDEN, Category.DOCUMENTATION)
    val BAD_ATTR_TAG = Issue(106, Severity.LINT, Category.DOCUMENTATION)
    val BAD_INHERITDOC = Issue(107, Severity.HIDDEN, Category.DOCUMENTATION)
    val HIDDEN_LINK = Issue(108, Severity.LINT, Category.DOCUMENTATION)
    val HIDDEN_CONSTRUCTOR = Issue(109, Severity.WARNING, Category.DOCUMENTATION)
    val UNAVAILABLE_SYMBOL = Issue(110, Severity.WARNING, Category.DOCUMENTATION)
    val HIDDEN_SUPERCLASS = Issue(111, Severity.WARNING, Category.DOCUMENTATION)
    val DEPRECATED = Issue(112, Severity.HIDDEN, Category.DOCUMENTATION)
    val DEPRECATION_MISMATCH = Issue(113, Severity.ERROR, Category.DOCUMENTATION)
    val MISSING_COMMENT = Issue(114, Severity.LINT, Category.DOCUMENTATION)
    val IO_ERROR = Issue(115, Severity.ERROR)
    val NO_SINCE_DATA = Issue(116, Severity.HIDDEN, Category.DOCUMENTATION)
    val NO_FEDERATION_DATA = Issue(117, Severity.WARNING, Category.DOCUMENTATION)
    val BROKEN_SINCE_FILE = Issue(118, Severity.ERROR, Category.DOCUMENTATION)
    val INVALID_CONTENT_TYPE = Issue(119, Severity.ERROR, Category.DOCUMENTATION)
    val INVALID_SAMPLE_INDEX = Issue(120, Severity.ERROR, Category.DOCUMENTATION)
    val HIDDEN_TYPE_PARAMETER = Issue(121, Severity.WARNING, Category.DOCUMENTATION)
    val PRIVATE_SUPERCLASS = Issue(122, Severity.WARNING, Category.DOCUMENTATION)
    val NULLABLE = Issue(123, Severity.HIDDEN, Category.DOCUMENTATION)
    val INT_DEF = Issue(124, Severity.HIDDEN, Category.DOCUMENTATION)
    val REQUIRES_PERMISSION = Issue(125, Severity.LINT, Category.DOCUMENTATION)
    val BROADCAST_BEHAVIOR = Issue(126, Severity.LINT, Category.DOCUMENTATION)
    val SDK_CONSTANT = Issue(127, Severity.LINT, Category.DOCUMENTATION)
    val TODO = Issue(128, Severity.LINT, Category.DOCUMENTATION)
    val NO_ARTIFACT_DATA = Issue(129, Severity.HIDDEN, Category.DOCUMENTATION)
    val BROKEN_ARTIFACT_FILE = Issue(130, Severity.ERROR, Category.DOCUMENTATION)

    // Metalava warnings (not from doclava)

    val TYPO = Issue(131, Severity.WARNING, Category.DOCUMENTATION)
    val MISSING_PERMISSION = Issue(132, Severity.LINT, Category.DOCUMENTATION)
    val MULTIPLE_THREAD_ANNOTATIONS = Issue(133, Severity.LINT, Category.DOCUMENTATION)
    val UNRESOLVED_CLASS = Issue(134, Severity.LINT, Category.DOCUMENTATION)
    val INVALID_NULL_CONVERSION = Issue(135, Severity.ERROR, Category.COMPATIBILITY)
    val PARAMETER_NAME_CHANGE = Issue(136, Severity.ERROR, Category.COMPATIBILITY)
    val OPERATOR_REMOVAL = Issue(137, Severity.ERROR, Category.COMPATIBILITY)
    val INFIX_REMOVAL = Issue(138, Severity.ERROR, Category.COMPATIBILITY)
    val VARARG_REMOVAL = Issue(139, Severity.ERROR, Category.COMPATIBILITY)
    val ADD_SEALED = Issue(140, Severity.ERROR, Category.COMPATIBILITY)
    val ANNOTATION_EXTRACTION = Issue(146, Severity.ERROR)
    val SUPERFLUOUS_PREFIX = Issue(147, Severity.WARNING)
    val HIDDEN_TYPEDEF_CONSTANT = Issue(148, Severity.ERROR)
    val EXPECTED_PLATFORM_TYPE = Issue(149, Severity.HIDDEN)
    val INTERNAL_ERROR = Issue(150, Severity.ERROR)
    val RETURNING_UNEXPECTED_CONSTANT = Issue(151, Severity.WARNING)
    val DEPRECATED_OPTION = Issue(152, Severity.WARNING)
    val BOTH_PACKAGE_INFO_AND_HTML = Issue(153, Severity.WARNING, Category.DOCUMENTATION)
    // The plan is for this to be set as an error once (1) existing code is marked as @deprecated
    // and (2) the principle is adopted by the API council
    val REFERENCES_DEPRECATED = Issue(154, Severity.HIDDEN)
    val UNHIDDEN_SYSTEM_API = Issue(155, Severity.ERROR)
    val SHOWING_MEMBER_IN_HIDDEN_CLASS = Issue(156, Severity.ERROR)
    val INVALID_NULLABILITY_ANNOTATION = Issue(157, Severity.ERROR)
    val REFERENCES_HIDDEN = Issue(158, Severity.ERROR)
    val IGNORING_SYMLINK = Issue(159, Severity.INFO)
    val INVALID_NULLABILITY_ANNOTATION_WARNING = Issue(160, Severity.WARNING)
    // The plan is for this to be set as an error once (1) existing code is marked as @deprecated
    // and (2) the principle is adopted by the API council
    val EXTENDS_DEPRECATED = Issue(161, Severity.HIDDEN)
    val FORBIDDEN_TAG = Issue(162, Severity.ERROR)
    val MISSING_COLUMN = Issue(163, Severity.WARNING, Category.DOCUMENTATION)
    val INVALID_SYNTAX = Issue(164, Severity.ERROR)

    // API lint
    val START_WITH_LOWER = Issue(300, Severity.ERROR, Category.API_LINT, "S1")
    val START_WITH_UPPER = Issue(301, Severity.ERROR, Category.API_LINT, "S1")
    val ALL_UPPER = Issue(302, Severity.ERROR, Category.API_LINT, "C2")
    val ACRONYM_NAME = Issue(303, Severity.WARNING, Category.API_LINT, "S1")
    val ENUM = Issue(304, Severity.ERROR, Category.API_LINT, "F5")
    val ENDS_WITH_IMPL = Issue(305, Severity.ERROR, Category.API_LINT)
    val MIN_MAX_CONSTANT = Issue(306, Severity.WARNING, Category.API_LINT, "C8")
    val COMPILE_TIME_CONSTANT = Issue(307, Severity.ERROR, Category.API_LINT)
    val SINGULAR_CALLBACK = Issue(308, Severity.ERROR, Category.API_LINT, "L1")
    val CALLBACK_NAME = Issue(309, Severity.WARNING, Category.API_LINT, "L1")
    val CALLBACK_INTERFACE = Issue(310, Severity.ERROR, Category.API_LINT, "CL3")
    val CALLBACK_METHOD_NAME = Issue(311, Severity.ERROR, Category.API_LINT, "L1")
    val LISTENER_INTERFACE = Issue(312, Severity.ERROR, Category.API_LINT, "L1")
    val SINGLE_METHOD_INTERFACE = Issue(313, Severity.ERROR, Category.API_LINT, "L1")
    val INTENT_NAME = Issue(314, Severity.ERROR, Category.API_LINT, "C3")
    val ACTION_VALUE = Issue(315, Severity.ERROR, Category.API_LINT, "C4")
    val EQUALS_AND_HASH_CODE = Issue(316, Severity.ERROR, Category.API_LINT, "M8")
    val PARCEL_CREATOR = Issue(317, Severity.ERROR, Category.API_LINT, "FW3")
    val PARCEL_NOT_FINAL = Issue(318, Severity.ERROR, Category.API_LINT, "FW8")
    val PARCEL_CONSTRUCTOR = Issue(319, Severity.ERROR, Category.API_LINT, "FW3")
    val PROTECTED_MEMBER = Issue(320, Severity.ERROR, Category.API_LINT, "M7")
    val PAIRED_REGISTRATION = Issue(321, Severity.ERROR, Category.API_LINT, "L2")
    val REGISTRATION_NAME = Issue(322, Severity.ERROR, Category.API_LINT, "L3")
    val VISIBLY_SYNCHRONIZED = Issue(323, Severity.ERROR, Category.API_LINT, "M5")
    val INTENT_BUILDER_NAME = Issue(324, Severity.WARNING, Category.API_LINT, "FW1")
    val CONTEXT_NAME_SUFFIX = Issue(325, Severity.ERROR, Category.API_LINT, "C4")
    val INTERFACE_CONSTANT = Issue(326, Severity.ERROR, Category.API_LINT, "C4")
    val ON_NAME_EXPECTED = Issue(327, Severity.WARNING, Category.API_LINT)
    val TOP_LEVEL_BUILDER = Issue(328, Severity.WARNING, Category.API_LINT)
    val MISSING_BUILD_METHOD = Issue(329, Severity.WARNING, Category.API_LINT)
    val BUILDER_SET_STYLE = Issue(330, Severity.WARNING, Category.API_LINT)
    val SETTER_RETURNS_THIS = Issue(331, Severity.WARNING, Category.API_LINT, "M4")
    val RAW_AIDL = Issue(332, Severity.ERROR, Category.API_LINT)
    val INTERNAL_CLASSES = Issue(333, Severity.ERROR, Category.API_LINT)
    val PACKAGE_LAYERING = Issue(334, Severity.WARNING, Category.API_LINT, "FW6")
    val GETTER_SETTER_NAMES = Issue(335, Severity.ERROR, Category.API_LINT, "M6")
    val CONCRETE_COLLECTION = Issue(336, Severity.ERROR, Category.API_LINT, "CL2")
    val OVERLAPPING_CONSTANTS = Issue(337, Severity.WARNING, Category.API_LINT, "C1")
    val GENERIC_EXCEPTION = Issue(338, Severity.ERROR, Category.API_LINT, "S1")
    val ILLEGAL_STATE_EXCEPTION = Issue(339, Severity.WARNING, Category.API_LINT, "S1")
    val RETHROW_REMOTE_EXCEPTION = Issue(340, Severity.ERROR, Category.API_LINT, "FW9")
    val MENTIONS_GOOGLE = Issue(341, Severity.ERROR, Category.API_LINT)
    val HEAVY_BIT_SET = Issue(342, Severity.ERROR, Category.API_LINT)
    val MANAGER_CONSTRUCTOR = Issue(343, Severity.ERROR, Category.API_LINT)
    val MANAGER_LOOKUP = Issue(344, Severity.ERROR, Category.API_LINT)
    val AUTO_BOXING = Issue(345, Severity.ERROR, Category.API_LINT, "M11")
    val STATIC_UTILS = Issue(346, Severity.ERROR, Category.API_LINT)
    val CONTEXT_FIRST = Issue(347, Severity.ERROR, Category.API_LINT, "M3")
    val LISTENER_LAST = Issue(348, Severity.WARNING, Category.API_LINT, "M3")
    val EXECUTOR_REGISTRATION = Issue(349, Severity.WARNING, Category.API_LINT, "L1")
    val CONFIG_FIELD_NAME = Issue(350, Severity.ERROR, Category.API_LINT)
    val RESOURCE_FIELD_NAME = Issue(351, Severity.ERROR, Category.API_LINT)
    val RESOURCE_VALUE_FIELD_NAME = Issue(352, Severity.ERROR, Category.API_LINT, "C7")
    val RESOURCE_STYLE_FIELD_NAME = Issue(353, Severity.ERROR, Category.API_LINT, "C7")
    val STREAM_FILES = Issue(354, Severity.WARNING, Category.API_LINT, "M10")
    val PARCELABLE_LIST = Issue(355, Severity.WARNING, Category.API_LINT)
    val ABSTRACT_INNER = Issue(356, Severity.WARNING, Category.API_LINT)
    val BANNED_THROW = Issue(358, Severity.ERROR, Category.API_LINT)
    val EXTENDS_ERROR = Issue(359, Severity.ERROR, Category.API_LINT)
    val EXCEPTION_NAME = Issue(360, Severity.ERROR, Category.API_LINT)
    val METHOD_NAME_UNITS = Issue(361, Severity.ERROR, Category.API_LINT)
    val FRACTION_FLOAT = Issue(362, Severity.ERROR, Category.API_LINT)
    val PERCENTAGE_INT = Issue(363, Severity.ERROR, Category.API_LINT)
    val NOT_CLOSEABLE = Issue(364, Severity.WARNING, Category.API_LINT)
    val KOTLIN_OPERATOR = Issue(365, Severity.INFO, Category.API_LINT)
    val ARRAY_RETURN = Issue(366, Severity.WARNING, Category.API_LINT)
    val USER_HANDLE = Issue(367, Severity.WARNING, Category.API_LINT)
    val USER_HANDLE_NAME = Issue(368, Severity.WARNING, Category.API_LINT)
    val SERVICE_NAME = Issue(369, Severity.ERROR, Category.API_LINT, "C4")
    val METHOD_NAME_TENSE = Issue(370, Severity.WARNING, Category.API_LINT)
    val NO_CLONE = Issue(371, Severity.ERROR, Category.API_LINT)
    val USE_ICU = Issue(372, Severity.WARNING, Category.API_LINT)
    val USE_PARCEL_FILE_DESCRIPTOR = Issue(373, Severity.ERROR, Category.API_LINT, "FW11")
    val NO_BYTE_OR_SHORT = Issue(374, Severity.WARNING, Category.API_LINT, "FW12")
    val SINGLETON_CONSTRUCTOR = Issue(375, Severity.ERROR, Category.API_LINT)
    val COMMON_ARGS_FIRST = Issue(376, Severity.WARNING, Category.API_LINT, "M2")
    val CONSISTENT_ARGUMENT_ORDER = Issue(377, Severity.ERROR, Category.API_LINT, "M2")
    val KOTLIN_KEYWORD = Issue(378, Severity.ERROR, Category.API_LINT) // Formerly 141
    val UNIQUE_KOTLIN_OPERATOR = Issue(379, Severity.ERROR, Category.API_LINT)
    val SAM_SHOULD_BE_LAST = Issue(380, Severity.WARNING, Category.API_LINT) // Formerly 142
    val MISSING_JVMSTATIC = Issue(381, Severity.WARNING, Category.API_LINT) // Formerly 143
    val DEFAULT_VALUE_CHANGE = Issue(382, Severity.ERROR, Category.API_LINT) // Formerly 144
    val DOCUMENT_EXCEPTIONS = Issue(383, Severity.ERROR, Category.API_LINT) // Formerly 145
    val FORBIDDEN_SUPER_CLASS = Issue(384, Severity.ERROR, Category.API_LINT)
    val MISSING_NULLABILITY = Issue(385, Severity.ERROR, Category.API_LINT)
    val MUTABLE_BARE_FIELD = Issue(386, Severity.ERROR, Category.API_LINT, "F2")
    val INTERNAL_FIELD = Issue(387, Severity.ERROR, Category.API_LINT, "F2")
    val PUBLIC_TYPEDEF = Issue(388, Severity.ERROR, Category.API_LINT, "FW15")
    val ANDROID_URI = Issue(389, Severity.ERROR, Category.API_LINT, "FW14")
    val BAD_FUTURE = Issue(390, Severity.ERROR, Category.API_LINT)

    fun findIssueById(id: Int): Issue? {
        return idToIssue[id]
    }

    fun findIssueById(id: String?): Issue? {
        return nameToIssue[id]
    }

    fun findIssueByIdIgnoringCase(id: String): Issue? {
        for (e in allIssues) {
            if (id.equals(e.name, ignoreCase = true)) {
                return e
            }
        }
        return null
    }

    class Issue private constructor(
        val code: Int,
        val defaultLevel: Severity,
        /**
         * When `level` is set to [Severity.INHERIT], this is the parent from
         * which the issue will inherit its level.
         */
        val parent: Issue?,
        /** Applicable category  */
        val category: Category,
        /** Related rule, if any  */
        val rule: String?,
        /** Related explanation, if any  */
        val explanation: String?
    ) {
        /**
         * The name of this issue
         */
        lateinit var name: String
            internal set

        internal constructor(
            code: Int,
            defaultLevel: Severity,
            category: Category = Category.UNKNOWN
        ) : this(code, defaultLevel, null, category, null, null)

        internal constructor(
            code: Int,
            defaultLevel: Severity,
            category: Category,
            rule: String
        ) : this(code, defaultLevel, null, category, rule, null)

        internal constructor(
            code: Int,
            parent: Issue,
            category: Category
        ) : this(code, Severity.INHERIT, parent, category, null, null)

        override fun toString(): String {
            return "Issue #$code ($name)"
        }

        init {
            allIssues.add(this)
        }
    }

    enum class Category(val description: String, val ruleLink: String?) {
        COMPATIBILITY("Compatibility", null),
        DOCUMENTATION("Documentation", null),
        API_LINT("API Lint", "go/android-api-guidelines"),
        UNKNOWN("Default", null)
    }

    init { // Initialize issue names based on the field names
        for (property in Issues::class.declaredMemberProperties) {
            if (property.returnType.classifier != Issue::class) continue
            val issue = property.getter.call(Issues) as Issue

            issue.name = SdkVersionInfo.underlinesToCamelCase(property.name.toLowerCase(Locale.US))
            nameToIssue[issue.name] = issue
            idToIssue[issue.code] = issue
        }
        for (issue in allIssues) {
            check(issue.name != "")
        }
    }
}