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

import com.android.tools.metalava.Options
import com.android.tools.metalava.options

/** Various places where a given annotation can be written */
enum class AnnotationTarget {
    /** Write the annotation into the signature file */
    SIGNATURE_FILE,
    /** Write the annotation into stub source files */
    SDK_STUBS_FILE,
    /** Write the annotation into doc stub source files */
    DOC_STUBS_FILE,
    /** Write the annotation into external annotation files */
    EXTERNAL_ANNOTATIONS_FILE,
    /** Don't write the annotation anywhere */
    NONE;

    /** Is this target a stubs file? */
    fun isStubsFile(): Boolean {
        return this == SDK_STUBS_FILE || this == DOC_STUBS_FILE
    }
}

/** Don't write this annotation anywhere; it is not API significant. */
val NO_ANNOTATION_TARGETS = setOf(AnnotationTarget.NONE)

/**
 * Annotation is API significant: write it into the signature file and stub source code.
 * This would normally be the case for all (API significant) class-retention annotations,
 * but unfortunately due to apt (the annotation processor) attempting to load all
 * classes for annotation references that it comes across, that means we cannot
 * compile the stubs with the androidx annotations and leave those in the SDK; apt
 * would also need to have androidx on the classpath. So instead we put all these
 * annotations (except for @RecentlyNullable and @RecentlyNonNull, which are not part
 * of androidx, and which we include as package private in the SDK, something we cannot
 * do with the others since their class definitions conflict with the real androidx library
 * when present) into the external annotations file.
 *
 * Also includes documentation stubs.
 */
val ANNOTATION_IN_ALL_STUBS = setOf(
    AnnotationTarget.SIGNATURE_FILE,
    AnnotationTarget.SDK_STUBS_FILE,
    AnnotationTarget.DOC_STUBS_FILE
)

/**
 * Like [ANNOTATION_IN_ALL_STUBS], but limited to SDK stubs, not included in documentation stubs.
 * Example: RecentlyNonNull.
 */
val ANNOTATION_IN_SDK_STUBS = setOf(AnnotationTarget.SIGNATURE_FILE, AnnotationTarget.SDK_STUBS_FILE)

/**
 * Like [ANNOTATION_IN_ALL_STUBS], but limited to documentation stubs, not included in SDK stubs.
 * These are also placed in external annotations since they don't appear in the SDK.
 *
 * Example: NonNull.
 */
val ANNOTATION_IN_DOC_STUBS_AND_EXTERNAL = setOf(
    AnnotationTarget.SIGNATURE_FILE,
    AnnotationTarget.DOC_STUBS_FILE,
    AnnotationTarget.EXTERNAL_ANNOTATIONS_FILE
)

/** Annotation is API significant: write it into the signature file and into external annotations file. */
val ANNOTATION_EXTERNAL = setOf(AnnotationTarget.SIGNATURE_FILE, AnnotationTarget.EXTERNAL_ANNOTATIONS_FILE)

/** Write it only into the external annotations file, not the signature file */
val ANNOTATION_EXTERNAL_ONLY = if (options.typedefMode == Options.TypedefMode.INLINE ||
    options.typedefMode == Options.TypedefMode.NONE) // just here for compatibility purposes
    setOf(AnnotationTarget.SIGNATURE_FILE, AnnotationTarget.EXTERNAL_ANNOTATIONS_FILE)
else
    setOf(AnnotationTarget.EXTERNAL_ANNOTATIONS_FILE)

/** Write it only into the he signature file */
val ANNOTATION_SIGNATURE_ONLY = setOf(AnnotationTarget.SIGNATURE_FILE)
