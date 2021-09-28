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
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

private fun zipDirectory(name: String, zipOut: ZipOutputStream) {
    zipOut.putNextEntry(ZipEntry("$name/"))
    zipOut.closeEntry()
}

private fun zipFile(file: File, name: String, zipOut: ZipOutputStream) {
    zipOut.putNextEntry(ZipEntry(name))
    FileInputStream(file).use {
        it.copyTo(zipOut)
    }
}

private fun zip(file: File, name: String, zipOut: ZipOutputStream) {
    if (file.isDirectory) {
        zipDirectory(name, zipOut)
    } else {
        zipFile(file, name, zipOut)
    }
}

fun createZipFromDirectory(output: File, input: File) {
    FileOutputStream(output).use { fos ->
        ZipOutputStream(fos).use { zos ->
            input.walk().filter { it != input }.forEach {
                zip(it, it.relativeTo(input).path, zos)
            }
        }
    }
}