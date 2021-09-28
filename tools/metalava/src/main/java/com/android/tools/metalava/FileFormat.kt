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
import com.android.SdkConstants.DOT_XML

/** File formats that metalava can emit APIs to */
enum class FileFormat(val description: String, val version: String? = null) {
    UNKNOWN("?"),
    JDIFF("JDiff"),
    BASELINE("Metalava baseline file", "1.0"),
    SINCE_XML("Metalava API-level file", "1.0"),

    // signature formats should be last to make comparisons work (for example in [configureOptions])
    V1("Doclava signature file", "1.0"),
    V2("Metalava signature file", "2.0"),
    V3("Metalava signature file", "3.0");

    /** Configures the option object such that the output format will be the given format */
    fun configureOptions(options: Options, compatibility: Compatibility) {
        if (this == JDIFF) {
            return
        }
        options.outputFormat = this
        options.compatOutput = this == V1
        options.outputKotlinStyleNulls = this >= V3
        options.outputDefaultValues = this >= V2
        compatibility.omitCommonPackages = this >= V2
        options.includeSignatureFormatVersion = this >= V2
    }

    fun useKotlinStyleNulls(): Boolean {
        return this >= V3
    }

    fun signatureFormatAsInt(): Int {
        return when (this) {
            V1 -> 1
            V2 -> 2
            V3 -> 3

            BASELINE,
            JDIFF,
            SINCE_XML,
            UNKNOWN -> error("this method is only allowed on signature formats, was $this")
        }
    }

    fun outputFlag(): String {
        return if (isSignatureFormat()) {
            "$ARG_FORMAT=v${signatureFormatAsInt()}"
        } else {
            ""
        }
    }

    fun preferredExtension(): String {
        return when (this) {
            V1,
            V2,
            V3 -> DOT_TXT

            BASELINE -> DOT_TXT

            JDIFF, SINCE_XML -> DOT_XML

            UNKNOWN -> ""
        }
    }

    fun header(): String? {
        val prefix = headerPrefix() ?: return null
        return prefix + version + "\n"
    }

    fun headerPrefix(): String? {
        return when (this) {
            V1 -> null
            V2, V3 -> "// Signature format: "
            BASELINE -> "// Baseline format: "
            JDIFF, SINCE_XML -> "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            UNKNOWN -> null
        }
    }

    fun isSignatureFormat(): Boolean {
        return this == V1 || this == V2 || this == V3
    }

    companion object {
        private fun firstLine(s: String): String {
            val index = s.indexOf('\n')
            if (index == -1) {
                return s
            }
            // Chop off \r if a Windows \r\n file
            val end = if (index > 0 && s[index - 1] == '\r') index - 1 else index
            return s.substring(0, end)
        }

        fun parseHeader(fileContents: String): FileFormat {
            val firstLine = firstLine(fileContents)
            for (format in values()) {
                val header = format.header()
                if (header == null) {
                    if (firstLine.startsWith("package ")) {
                        // Old signature files
                        return V1
                    } else if (firstLine.startsWith("<api")) {
                        return JDIFF
                    }
                } else if (header.startsWith(firstLine)) {
                    if (format == JDIFF) {
                        if (!fileContents.contains("<api")) {
                            // The JDIFF header is the general XML header: don't accept XML documents that
                            // don't contain an empty API definition
                            return UNKNOWN
                        }
                        // Both JDiff and API-level files use <api> as the root tag (unfortunate but too late to
                        // change) so distinguish on whether the file contains any since elementss
                        if (fileContents.contains("since=")) {
                            return SINCE_XML
                        }
                    }
                    return format
                }
            }

            return UNKNOWN
        }
    }
}