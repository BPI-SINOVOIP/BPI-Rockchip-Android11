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

object : MesonPort() {
    override val name = "jsoncpp"
    override val version = "1.9.1"
    override val url =
        "https://github.com/open-source-parsers/jsoncpp/archive/$version.tar.gz"

    override val license = License(
        "The JsonCpp License",
        "https://github.com/open-source-parsers/jsoncpp/blob/master/LICENSE"
    )

    override val modules = listOf(
        Module("jsoncpp")
    )

    override fun fetchSource(
        sourceDirectory: File,
        workingDirectory: File
    ): Result<Unit, String> =
        super.fetchSource(sourceDirectory, workingDirectory).onSuccess {
            // jsoncpp has a "version" file on the include path that conflicts
            // with https://en.cppreference.com/w/cpp/header/version. Remove it
            // so we can build.
            sourceDirectory.resolve("version").delete()
        }
}