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

/**
 * A version number that is compatible with CMake's package version format.
 *
 * https://cmake.org/cmake/help/latest/manual/cmake-packages.7.html#package-version-file
 *
 * CMake package versions *must* be numeric with a maximum of four dot separated
 * components.
 */
data class CMakeCompatibleVersion(
    val major: Int,
    val minor: Int?,
    val patch: Int?,
    val tweak: Int?
) {
    init {
        if (tweak != null) {
            require(patch != null)
        }

        if (patch != null) {
            require(minor != null)
        }
    }

    override fun toString(): String =
        listOfNotNull(major, minor, patch, tweak).joinToString(".")

    companion object {
        private val versionRegex = Regex(
            """^(\d+)(?:\.(\d+)(?:\.(\d+)(?:\.(\d+))?)?)?$"""
        )

        fun parse(versionString: String): CMakeCompatibleVersion {
            val match = versionRegex.find(versionString)
            require(match != null) {
                "$versionString is not in major[.minor[.patch[.tweak]]] format"
            }
            return CMakeCompatibleVersion(
                match.groups[1]!!.value.toInt(),
                match.groups[2]?.value?.toInt(),
                match.groups[3]?.value?.toInt(),
                match.groups[4]?.value?.toInt()
            )
        }
    }
}