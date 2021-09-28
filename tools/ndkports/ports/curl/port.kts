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

object : AutoconfPort() {
    override val name = "curl"
    override val version = "7.66.0"
    override val url = "https://curl.haxx.se/download/curl-$version.tar.gz"
    override val licensePath = "COPYING"

    override val license = License(
        "The curl License", "https://curl.haxx.se/docs/copyright.html"
    )

    override val dependencies = listOf("openssl")

    override val modules = listOf(
        Module(
            "curl",
            dependencies = listOf("//openssl:crypto", "//openssl:ssl")
        )
    )

    override fun configureArgs(
        workingDirectory: File,
        toolchain: Toolchain
    ): List<String> {
        val sslPrefix = installDirectoryForPort(
            "openssl",
            workingDirectory,
            toolchain
        ).absolutePath
        return listOf(
            "--disable-ntlm-wb",
            "--enable-ipv6",
            "--with-zlib",
            "--with-ca-path=/system/etc/security/cacerts",
            "--with-ssl=$sslPrefix"
        )
    }

    override fun configureEnv(
        workingDirectory: File,
        toolchain: Toolchain
    ): Map<String, String> = mapOf(
        // aarch64 still defaults to bfd which transitively checks libraries.
        // When curl is linking one of its own libraries which depends on
        // openssl, it doesn't pass -rpath-link to be able to find the SSL
        // libraries and fails to build because of it.
        //
        // TODO: Switch to lld once we're using r21.
        "LDFLAGS" to "-fuse-ld=gold"
    )
}