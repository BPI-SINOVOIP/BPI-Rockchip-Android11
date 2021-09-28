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

abstract class MesonPort : Port() {
    enum class DefaultLibraryType(val argument: String) {
        Both("both"),
        Shared("shared"),
        Static("static")
    }

    open val defaultLibraryType: DefaultLibraryType = DefaultLibraryType.Shared

    override fun configure(
        toolchain: Toolchain,
        sourceDirectory: File,
        buildDirectory: File,
        installDirectory: File,
        workingDirectory: File
    ): Result<Unit, String> {
        val cpuFamily = when (toolchain.abi) {
            Abi.Arm -> "arm"
            Abi.Arm64 -> "aarch64"
            Abi.X86 -> "x86"
            Abi.X86_64 -> "x86_64"
        }

        val cpu = when (toolchain.abi) {
            Abi.Arm -> "armv7a"
            Abi.Arm64 -> "armv8a"
            Abi.X86 -> "i686"
            Abi.X86_64 -> "x86_64"
        }

        val crossFile = workingDirectory.resolve("cross_file.txt").apply {
            writeText("""
            [binaries]
            ar = '${toolchain.ar}'
            c = '${toolchain.clang}'
            cpp = '${toolchain.clangxx}'
            strip = '${toolchain.strip}'

            [host_machine]
            system = 'android'
            cpu_family = '$cpuFamily'
            cpu = '$cpu'
            endian = 'little'
            """.trimIndent())
        }

        return executeProcessStep(
            listOf(
                "meson",
                "--cross-file",
                crossFile.absolutePath,
                "--buildtype",
                "release",
                "--prefix",
                installDirectory.absolutePath,
                "--default-library",
                defaultLibraryType.argument,
                sourceDirectory.absolutePath,
                buildDirectory.absolutePath
            ), workingDirectory
        )
    }

    override fun build(
        toolchain: Toolchain,
        buildDirectory: File
    ): Result<Unit, String> =
        executeProcessStep(listOf("ninja", "-v"), buildDirectory)

    override fun install(
        toolchain: Toolchain,
        buildDirectory: File,
        installDirectory: File
    ): Result<Unit, String> =
        executeProcessStep(listOf("ninja", "-v", "install"), buildDirectory)
}