/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.ndkports

import java.io.File

data class NdkVersion(
    val major: Int,
    val minor: Int,
    val build: Int,
    val qualifier: String?
) {
    companion object {
        private val pkgRevisionRegex = Regex("""^Pkg.Revision\s*=\s*(\S+)$""")
        private val versionRegex = Regex("""^(\d+).(\d+).(\d+)(?:-(\S+))?$""")

        private fun fromString(versionString: String): NdkVersion {
            val match = versionRegex.find(versionString)
            require(match != null) { "Invalid version string" }
            val (major, minor, build, qualifier) = match.destructured
            return NdkVersion(
                major.toInt(),
                minor.toInt(),
                build.toInt(),
                qualifier.takeIf { match.groups[4] != null }
            )
        }

        fun fromSourcePropertiesText(text: String): NdkVersion {
            for (line in text.lines().map { it.trim() }) {
                pkgRevisionRegex.find(line)?.let {
                    return fromString(it.groups.last()!!.value)
                }
            }
            throw RuntimeException(
                "Did not find Pkg.Revision in source.properties"
            )
        }

        fun fromNdk(ndk: File): NdkVersion = fromSourcePropertiesText(
            ndk.resolve("source.properties").readText()
        )
    }
}