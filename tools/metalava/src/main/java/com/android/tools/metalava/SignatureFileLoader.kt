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

import com.android.tools.metalava.doclava1.ApiFile
import com.android.tools.metalava.doclava1.ApiParseException
import com.android.tools.metalava.model.Codebase
import java.io.File

object SignatureFileLoader {
    private val map = mutableMapOf<File, Codebase>()

    fun load(
        file: File,
        kotlinStyleNulls: Boolean? = null
    ): Codebase {
        return map[file] ?: run {
            val loaded = loadFromSignatureFiles(file, kotlinStyleNulls)
            map[file] = loaded
            loaded
        }
    }

    private fun loadFromSignatureFiles(
        file: File,
        kotlinStyleNulls: Boolean? = null
    ): Codebase {
        try {
            val codebase = ApiFile.parseApi(File(file.path), kotlinStyleNulls ?: false)
            codebase.description = "Codebase loaded from ${file.path}"
            return codebase
        } catch (ex: ApiParseException) {
            val message = "Unable to parse signature file $file: ${ex.message}"
            throw DriverException(message)
        }
    }

    fun loadFiles(files: List<File>, kotlinStyleNulls: Boolean? = null): Codebase {
        if (files.isEmpty()) {
            throw IllegalArgumentException("files must not be empty")
        }
        try {
            return ApiFile.parseApi(files, kotlinStyleNulls ?: false)
        } catch (ex: ApiParseException) {
            val message = "Unable to parse signature file: ${ex.message}"
            throw DriverException(message)
        }
    }
}
