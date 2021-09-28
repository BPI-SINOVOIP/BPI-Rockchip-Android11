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

import com.google.prefab.api.AndroidAbiMetadata
import com.google.prefab.api.ModuleMetadataV1
import com.google.prefab.api.PackageMetadataV1
import kotlinx.serialization.json.Json
import kotlinx.serialization.stringify
import org.apache.maven.model.Dependency
import org.apache.maven.model.Developer
import org.apache.maven.model.License
import org.apache.maven.model.Scm
import org.apache.maven.model.io.DefaultModelWriter
import org.apache.maven.project.MavenProject
import org.redundent.kotlin.xml.xml
import java.io.File

class PrefabPackageBuilder(
    private val port: Port,
    private val directory: File,
    private val sourceDirectory: File,
    private val publishToMavenLocal: Boolean,
    private val ndk: Ndk,
    private val abiToApiMap: Map<Abi, Int>,
    private val portsByName: Map<String, Port>
) {
    private val packageDirectory = directory.resolve("aar")
    private val prefabDirectory = packageDirectory.resolve("prefab")
    private val modulesDirectory = prefabDirectory.resolve("modules")

    private val packageComponents = listOf(
        "com",
        "android",
        "ndk",
        "thirdparty",
        port.name
    )

    private val packageName = packageComponents.joinToString(".")
    private val groupComponents = packageComponents.dropLast(1)
    private val groupId = groupComponents.joinToString(".")
    private val artifactId = packageComponents.last()

    private val mavenProject = MavenProject().also {
        it.name = port.name
        it.description = "The ndkports AAR for ${port.name}."
        it.url = "https://android.googlesource.com/platform/tools/ndkports"
        it.groupId = groupId
        it.artifactId = artifactId
        it.version = port.mavenVersion
        it.packaging = "aar"
        it.licenses = listOf(
            License().also { license ->
                license.name = port.license.name
                license.url = port.license.url
                license.distribution = "repo"
            }
        )
        it.developers = listOf(
            Developer().also { developer ->
                developer.name = "The Android Open Source Project"
            }
        )
        it.scm = Scm().also { scm ->
            scm.url = "https://android.googlesource.com/platform/tools/ndkports"
            scm.connection = "scm:git:https://android.googlesource.com/platform/tools/ndkports"
        }
        it.dependencies = port.dependencies.map { depName ->
            val depPort = portsByName[depName] ?: throw RuntimeException(
                "${port.name} depends on unknown port: $depName"
            )
            Dependency().also { dep ->
                dep.artifactId = depPort.name
                dep.groupId = groupId
                dep.version = depPort.mavenVersion
                dep.type = "aar"
                // TODO: Make this an option in the Port.
                // We currently only have one dependency from curl to OpenSSL,
                // and that's (from the perspective of the AAR consumer), a
                // runtime dependency. If we ever have compile dependencies,
                // we'll want to make it possible for each port to customize its
                // scope.
                dep.scope = "runtime"
            }
        }
        // TODO: Issue management?
    }

    private fun preparePackageDirectory() {
        if (packageDirectory.exists()) {
            packageDirectory.deleteRecursively()
        }
        modulesDirectory.mkdirs()
    }

    private fun makePackageMetadata() {
        prefabDirectory.resolve("prefab.json").writeText(
            Json.stringify(
                PackageMetadataV1(
                    port.name,
                    schemaVersion = 1,
                    dependencies = port.dependencies,
                    version = port.prefabVersion.toString()
                )
            )
        )
    }

    private fun makeModuleMetadata(module: Module, moduleDirectory: File) {
        moduleDirectory.resolve("module.json").writeText(
            Json.stringify(
                ModuleMetadataV1(
                    exportLibraries = module.dependencies
                )
            )
        )
    }

    private fun installLibForAbi(module: Module, abi: Abi, libsDir: File) {
        val libName = "lib${module.name}.so"
        val installDirectory = libsDir.resolve("android.${abi.abiName}").apply {
            mkdirs()
        }

        directory.resolve("install/$abi/lib/$libName")
            .copyTo(installDirectory.resolve(libName))

        val api = abiToApiMap.getOrElse(abi) {
            throw RuntimeException(
                "No API level specified for ${abi.abiName}"
            )
        }

        installDirectory.resolve("abi.json").writeText(
            Json.stringify(
                AndroidAbiMetadata(
                    abi = abi.abiName,
                    api = api,
                    ndk = ndk.version.major,
                    stl = "c++_shared"
                )
            )
        )
    }

    private fun installLicense() {
        val src = sourceDirectory.resolve(port.licensePath)
        val dest = packageDirectory.resolve("META-INF")
            .resolve(File(port.licensePath).name)
        src.copyTo(dest)
    }

    private fun createAndroidManifest() {
        packageDirectory.resolve("AndroidManifest.xml")
            .writeText(xml("manifest") {
                attributes(
                    "xmlns:android" to "http://schemas.android.com/apk/res/android",
                    "package" to packageName,
                    "android:versionCode" to 1,
                    "android:versionName" to "1.0"
                )

                "uses-sdk" {
                    attributes(
                        "android:minSdkVersion" to 16,
                        "android:targetSdkVersion" to 29
                    )
                }
            }.toString())
    }

    private fun createPom(pomFile: File) {
        DefaultModelWriter().write(pomFile, null, mavenProject.model)
    }

    private fun installToLocalMaven(archive: File, pomFile: File) {
        val pb = ProcessBuilder(
            listOf(
                "mvn",
                "install:install-file",
                "-Dfile=$archive",
                "-DpomFile=$pomFile"
            )
        )
            .redirectOutput(ProcessBuilder.Redirect.INHERIT)
            .redirectError(ProcessBuilder.Redirect.INHERIT)

        return pb.start()
            .waitFor().let {
                if (it != 0) {
                    throw RuntimeException(
                        "Failed to install archive to local maven " +
                                "repository: $archive $pomFile"
                    )
                }
            }
    }

    private fun createArchive() {
        val archive = directory.resolve("${port.name}.aar")
        val pomFile = directory.resolve("${port.name}.pom")
        createZipFromDirectory(archive, packageDirectory)
        createPom(pomFile)
        if (publishToMavenLocal) {
            installToLocalMaven(archive, pomFile)
        }
    }

    fun build() {
        preparePackageDirectory()
        makePackageMetadata()
        for (module in port.modules) {
            val moduleDirectory = modulesDirectory.resolve(module.name).apply {
                mkdirs()
            }

            makeModuleMetadata(module, moduleDirectory)

            if (module.includesPerAbi) {
                TODO()
            } else {
                // TODO: Perform sanity check.
                directory.resolve("install/${Abi.Arm}/include")
                    .copyRecursively(moduleDirectory.resolve("include"))
            }

            val libsDir = moduleDirectory.resolve("libs").apply { mkdirs() }
            for (abi in Abi.values()) {
                installLibForAbi(module, abi, libsDir)
            }
        }

        installLicense()

        createAndroidManifest()
        createArchive()
    }
}
