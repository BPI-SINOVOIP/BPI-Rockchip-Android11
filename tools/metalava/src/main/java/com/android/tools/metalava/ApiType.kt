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

import com.android.SdkConstants.DOT_TXT
import com.android.tools.metalava.doclava1.ApiPredicate
import com.android.tools.metalava.doclava1.ElidingPredicate
import com.android.tools.metalava.doclava1.FilterPredicate
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.Item
import java.io.File
import java.util.function.Predicate

/** Types of APIs emitted (or parsed etc) */
enum class ApiType(val flagName: String, val displayName: String = flagName) {
    /** The public API */
    PUBLIC_API("api", "public") {
        override fun getOptionFile(): File? {
            return options.apiFile
        }

        override fun getEmitFilter(): Predicate<Item> {
            val apiFilter = FilterPredicate(ApiPredicate())
            val apiReference = ApiPredicate(ignoreShown = true)
            return apiFilter.and(ElidingPredicate(apiReference))
        }

        override fun getReferenceFilter(): Predicate<Item> {
            return ApiPredicate(ignoreShown = true)
        }
    },

    /** The API that has been removed */
    REMOVED("removed", "removed") {
        override fun getOptionFile(): File? {
            return options.removedApiFile
        }

        override fun getEmitFilter(): Predicate<Item> {
            val removedFilter = FilterPredicate(ApiPredicate(matchRemoved = true))
            val removedReference = ApiPredicate(ignoreShown = true, ignoreRemoved = true)
            return removedFilter.and(ElidingPredicate(removedReference))
        }

        override fun getReferenceFilter(): Predicate<Item> {
            return ApiPredicate(ignoreShown = true, ignoreRemoved = true)
        }
    },

    /** The private API */
    PRIVATE("private", "private") {
        override fun getOptionFile(): File? {
            return options.privateApiFile
        }

        override fun getEmitFilter(): Predicate<Item> {
            val apiFilter = FilterPredicate(ApiPredicate())
            val memberIsNotCloned: Predicate<Item> = Predicate { !it.isCloned() }
            return memberIsNotCloned.and(apiFilter.negate())
        }

        override fun getReferenceFilter(): Predicate<Item> {
            return Predicate { true }
        }
    },

    /** Everything */
    ALL("all", "all") {
        override fun getOptionFile(): File? {
            return null
        }

        override fun getEmitFilter(): Predicate<Item> {
            return Predicate { it.emit }
        }

        override fun getReferenceFilter(): Predicate<Item> {
            return Predicate { true }
        }
    };

    /** Returns the user-configured file where the API has been written to, if any */
    abstract fun getOptionFile(): File?

    abstract fun getEmitFilter(): Predicate<Item>

    abstract fun getReferenceFilter(): Predicate<Item>

    override fun toString(): String = displayName

    /**
     * Gets the signature file for the given API type. Will create it if not already
     * created.
     */
    fun getSignatureFile(codebase: Codebase, defaultName: String): File {
        val apiType = this
        return apiType.getOptionFile() ?: run {
            val tempFile = createTempFile(defaultName, DOT_TXT)
            tempFile.deleteOnExit()
            val apiEmit = apiType.getEmitFilter()
            val apiReference = apiType.getReferenceFilter()

            createReportFile(codebase, tempFile, null) { printWriter ->
                SignatureWriter(printWriter, apiEmit, apiReference, codebase.preFiltered)
            }

            tempFile
        }
    }
}
