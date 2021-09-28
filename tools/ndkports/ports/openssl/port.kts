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

object : Port() {
    override val name = "openssl"
    override val version = "1.1.1d"
    override val prefabVersion = CMakeCompatibleVersion(1, 1, 1, 4)
    override val url = "https://www.openssl.org/source/openssl-$version.tar.gz"

    override val license = License(
        "Dual OpenSSL and SSLeay License",
        "https://www.openssl.org/source/license-openssl-ssleay.txt"
    )

    override val modules = listOf(
        Module("crypto"),
        Module("ssl")
    )

    override fun configure(
        toolchain: Toolchain,
        sourceDirectory: File,
        buildDirectory: File,
        installDirectory: File,
        workingDirectory: File
    ): Result<Unit, String> {
        buildDirectory.mkdirs()
        return executeProcessStep(
            listOf(
                sourceDirectory.resolve("Configure").absolutePath,
                "android-${toolchain.abi.archName}",
                "-D__ANDROID_API__=${toolchain.api}",
                "--prefix=${installDirectory.absolutePath}",
                "--openssldir=${installDirectory.absolutePath}",
                "shared"
            ),
            buildDirectory,
            additionalEnvironment = mapOf(
                "ANDROID_NDK" to toolchain.ndk.path.absolutePath,
                "PATH" to "${toolchain.binDir}:${System.getenv("PATH")}"
            )
        )
    }

    override fun build(
        toolchain: Toolchain,
        buildDirectory: File
    ): Result<Unit, String> =
        executeProcessStep(
            listOf(
                "make",
                "-j$ncpus",
                "SHLIB_EXT=.so"
            ), buildDirectory,
            additionalEnvironment = mapOf(
                "ANDROID_NDK" to toolchain.ndk.path.absolutePath,
                "PATH" to "${toolchain.binDir}:${System.getenv("PATH")}"
            )
        )

    override fun install(
        toolchain: Toolchain,
        buildDirectory: File,
        installDirectory: File
    ): Result<Unit, String> =
        executeProcessStep(
            listOf("make", "install_sw", "SHLIB_EXT=.so"), buildDirectory,
            additionalEnvironment = mapOf(
                "ANDROID_NDK" to toolchain.ndk.path.absolutePath,
                "PATH" to "${toolchain.binDir}:${System.getenv("PATH")}"
            )
        )
}