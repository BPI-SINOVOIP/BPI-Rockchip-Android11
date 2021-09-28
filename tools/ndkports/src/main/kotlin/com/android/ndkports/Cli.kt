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

import com.github.ajalt.clikt.core.CliktCommand
import com.github.ajalt.clikt.parameters.arguments.argument
import com.github.ajalt.clikt.parameters.arguments.multiple
import com.github.ajalt.clikt.parameters.arguments.validate
import com.github.ajalt.clikt.parameters.options.convert
import com.github.ajalt.clikt.parameters.options.default
import com.github.ajalt.clikt.parameters.options.flag
import com.github.ajalt.clikt.parameters.options.option
import com.github.ajalt.clikt.parameters.options.required
import com.github.ajalt.clikt.parameters.types.file
import de.swirtz.ktsrunner.objectloader.KtsObjectLoader
import java.io.File
import java.io.FileNotFoundException
import java.lang.RuntimeException
import kotlin.system.exitProcess

class Cli : CliktCommand(help = "ndkports") {
    private val outDir: File by option(
        "-o",
        "--out",
        help = "Build directory."
    ).file().default(File("out"))

    private val publishToMavenLocal: Boolean by option(
        help = "Publish AARs to the local Maven repository (~/.m2/repository)"
    ).flag()

    private val packages: List<String> by argument(
        help = "Names of packages to build."
    ).multiple().validate {
        require(it.isNotEmpty()) { "must provide at least one package" }
    }

    private val ndk: Ndk by option().convert { Ndk(File(it)) }.required()

    private fun loadPort(name: String): Port {
        val portDir = File("ports").resolve(name).also {
            if (!it.exists()) {
                throw FileNotFoundException("Could not find ${it.path}")
            }
        }

        val portFile = portDir.resolve("port.kts").also {
            if (!it.exists()) {
                throw FileNotFoundException("Could not find ${it.path}")
            }
        }

        return KtsObjectLoader().load(portFile.reader())
    }

    override fun run() {
        println("Building packages: ${packages.joinToString(", ")}")
        val portsByName = packages.map { loadPort(it) }.associateBy { it.name }
        for (port in portsByName.values) {
            val workingDirectory =
                outDir.resolve(port.name).also { it.mkdirs() }
            val sourceDirectory = workingDirectory.resolve("src")

            port.fetchSource(sourceDirectory, workingDirectory).onFailure {
                println(it)
                exitProcess(1)
            }

            val apiForAbi = mapOf(
                Abi.Arm to 16,
                Abi.Arm64 to 21,
                Abi.X86 to 16,
                Abi.X86_64 to 21
            )
            for (abi in Abi.values()) {
                val api = apiForAbi.getOrElse(abi) {
                    throw RuntimeException(
                        "No API level specified for ${abi.abiName}"
                    )
                }
                val toolchain = Toolchain(ndk, abi, api)

                val buildDirectory = workingDirectory.resolve("build/$abi")
                val installDirectory = installDirectoryForPort(
                    port.name, workingDirectory, toolchain
                )

                port.run(
                    toolchain,
                    sourceDirectory,
                    buildDirectory,
                    installDirectory,
                    workingDirectory
                ).onFailure {
                    println(it)
                    exitProcess(1)
                }
            }

            PrefabPackageBuilder(
                port,
                workingDirectory,
                sourceDirectory,
                publishToMavenLocal,
                ndk,
                apiForAbi,
                portsByName
            ).build()
        }
    }
}

fun main(args: Array<String>) {
    Cli().main(args)
}