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
import java.lang.RuntimeException

class Ndk(val path: File) {
    val version = NdkVersion.fromNdk(path)

    private val llvmBaseDir = path.resolve("toolchains/llvm/prebuilt")
    val hostTag: String = llvmBaseDir.let {
        val files = it.list()
            ?: throw RuntimeException("Unable to get file list for $it")

        if (files.size != 1) {
            throw RuntimeException("Expected exactly one directory in $it")
        }

        files.first()
    }

    private val toolchainDirectory = llvmBaseDir.resolve(hostTag)
    val toolchainBinDirectory = toolchainDirectory.resolve("bin")
}