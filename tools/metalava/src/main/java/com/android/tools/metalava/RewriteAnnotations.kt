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

import com.android.SdkConstants
import com.android.SdkConstants.DOT_CLASS
import com.android.SdkConstants.DOT_JAR
import com.android.tools.metalava.model.AnnotationRetention
import com.android.tools.metalava.model.Codebase
import com.google.common.io.Closer
import org.objectweb.asm.ClassReader
import org.objectweb.asm.ClassVisitor
import org.objectweb.asm.ClassWriter
import org.objectweb.asm.Opcodes
import org.objectweb.asm.Opcodes.ASM6
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.file.attribute.FileTime
import java.util.jar.JarEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream
import kotlin.text.Charsets.UTF_8

/**
 * Converts public stub annotation sources into package private annotation sources.
 * This is needed for the stub sources, where we want to reference annotations that aren't
 * public, but (a) they need to be public during compilation, and (b) they need to be
 * package private when compiled and packaged on their own such that annotation processors
 * can find them. See b/110532131 for details.
 */
class RewriteAnnotations {
    /** Modifies annotation source files such that they are package private */
    fun modifyAnnotationSources(codebase: Codebase?, source: File, target: File, pkg: String = "") {
        val fileName = source.name
        if (fileName.endsWith(SdkConstants.DOT_JAVA)) {
            if (!options.includeSourceRetentionAnnotations) {
                // Only copy non-source retention annotation classes
                val qualifiedName = pkg + "." + fileName.substring(0, fileName.indexOf('.'))
                if (hasSourceRetention(codebase, qualifiedName)) {
                    return
                }
            }

            // Copy and convert
            target.parentFile.mkdirs()
            target.writeText(
                source.readText(UTF_8).replace(
                    "\npublic @interface",
                    "\n@interface"
                )
            )
        } else if (source.isDirectory) {
            val newPackage = if (pkg.isEmpty()) fileName else "$pkg.$fileName"
            source.listFiles()?.forEach {
                modifyAnnotationSources(codebase, it, File(target, it.name), newPackage)
            }
        }
    }

    /** Copies annotation source files from [source] to [target] */
    fun copyAnnotations(codebase: Codebase, source: File, target: File, pkg: String = "") {
        val fileName = source.name
        if (fileName.endsWith(SdkConstants.DOT_JAVA)) {
            if (!options.includeSourceRetentionAnnotations) {
                // Only copy non-source retention annotation classes
                val qualifiedName = pkg + "." + fileName.substring(0, fileName.indexOf('.'))
                if (hasSourceRetention(codebase, qualifiedName)) {
                    return
                }
            }

            // Copy and convert
            target.parentFile.mkdirs()
            source.copyTo(target)
        } else if (source.isDirectory) {
            val newPackage = if (pkg.isEmpty()) fileName else "$pkg.$fileName"
            source.listFiles()?.forEach {
                copyAnnotations(codebase, it, File(target, it.name), newPackage)
            }
        }
    }

    /** Writes the bytecode for the compiled annotations in the given file list such that they are package private */
    fun rewriteAnnotations(files: List<File>) {
        for (file in files) {
            // Jump directly into androidx/annotation if it appears we were invoked at the top level
            if (file.isDirectory) {
                val android = File(file, "android${File.separator}annotation/")
                if (android.isDirectory) {
                    rewriteAnnotations(android)
                    val androidx = File(file, "androidx${File.separator}annotation/")
                    if (androidx.isDirectory) {
                        rewriteAnnotations(androidx)
                    }
                    continue
                }
            }

            rewriteAnnotations(file)
        }
    }

    /** Returns true if the given annotation class name has source retention as far as the stub
     * annotations are concerned.
     */
    private fun hasSourceRetention(codebase: Codebase?, qualifiedName: String): Boolean {
        when {
            qualifiedName == RECENTLY_NULLABLE ||
                qualifiedName == RECENTLY_NONNULL ||
                qualifiedName == ANDROID_NULLABLE ||
                qualifiedName == ANDROID_NONNULL -> return false
            qualifiedName.startsWith("androidx.annotation.") -> return true
        }

        // See if the annotation is pointing to an annotation class that is part of the API; if not, skip it.
        if (codebase != null) {
            val cls = codebase.findClass(qualifiedName) ?: return true
            return cls.isAnnotationType() && cls.getRetention() == AnnotationRetention.SOURCE
        }

        return false
    }

    /** Writes the bytecode for the compiled annotations in the given file such that they are package private */
    private fun rewriteAnnotations(file: File) {
        when {
            file.isDirectory -> file.listFiles()?.forEach { rewriteAnnotations(it) }
            file.path.endsWith(DOT_CLASS) -> rewriteClassFile(file)
            file.path.endsWith(DOT_JAR) -> rewriteJar(file)
        }
    }

    private fun rewriteClassFile(file: File) {
        if (file.name.contains("$")) {
            return // Not worrying about inner classes
        }
        val bytes = file.readBytes()
        val rewritten = rewriteClass(bytes, file.path) ?: return
        file.writeBytes(rewritten)
    }

    private fun rewriteClass(bytes: ByteArray, path: String): ByteArray? {
        return try {
            val reader = ClassReader(bytes)
            rewriteOuterClass(reader)
        } catch (ioe: IOException) {
            error("Could not process " + path + ": " + ioe.localizedMessage)
        }
    }

    private fun rewriteOuterClass(reader: ClassReader): ByteArray? {
        val classWriter = ClassWriter(ASM6)
        var skip = true
        val classVisitor = object : ClassVisitor(ASM6, classWriter) {
            override fun visit(
                version: Int,
                access: Int,
                name: String,
                signature: String?,
                superName: String?,
                interfaces: Array<out String>?
            ) {
                // Only process public annotations in android.annotation and androidx.annotation
                if (access and Opcodes.ACC_PUBLIC != 0 &&
                    access and Opcodes.ACC_ANNOTATION != 0 &&
                    (name.startsWith("android/annotation/") || name.startsWith("androidx/annotation/"))
                ) {
                    skip = false
                    val flagsWithoutPublic = access and Opcodes.ACC_PUBLIC.inv()
                    super.visit(version, flagsWithoutPublic, name, signature, superName, interfaces)
                }
            }
        }

        reader.accept(classVisitor, 0)
        return if (skip) {
            null
        } else {
            classWriter.toByteArray()
        }
    }

    private fun rewriteJar(file: File) {
        val temp = File(file.path + ".temp-$PROGRAM_NAME")
        rewriteJar(file, temp)
        file.delete()
        temp.renameTo(file)
    }

    private val zeroTime = FileTime.fromMillis(0)

    private fun rewriteJar(from: File, to: File/*, filter: Predicate<String>?*/) {
        Closer.create().use { closer ->
            val fos = closer.register(FileOutputStream(to))
            val bos = closer.register(BufferedOutputStream(fos))
            val zos = closer.register(ZipOutputStream(bos))

            val fis = closer.register(FileInputStream(from))
            val bis = closer.register(BufferedInputStream(fis))
            val zis = closer.register(ZipInputStream(bis))

            while (true) {
                val entry = zis.nextEntry ?: break
                val name = entry.name
                val newEntry: JarEntry

                // Preserve the STORED method of the input entry.
                newEntry = if (entry.method == JarEntry.STORED) {
                    val jarEntry = JarEntry(entry)
                    jarEntry.size = entry.size
                    jarEntry.compressedSize = entry.compressedSize
                    jarEntry.crc = entry.crc
                    jarEntry
                } else {
                    // Create a new entry so that the compressed len is recomputed.
                    JarEntry(name)
                }

                newEntry.lastAccessTime = zeroTime
                newEntry.creationTime = zeroTime
                newEntry.lastModifiedTime = entry.lastModifiedTime

                // add the entry to the jar archive
                zos.putNextEntry(newEntry)

                // read the content of the entry from the input stream, and write it into the archive.
                if (name.endsWith(DOT_CLASS) &&
                    (name.startsWith("android/annotation/") || name.startsWith("androidx/annotation/")) &&
                    name.indexOf("$") == -1 &&
                    !entry.isDirectory
                ) {
                    val bytes = zis.readBytes()
                    val rewritten = rewriteClass(bytes, name)
                    if (rewritten != null) {
                        zos.write(rewritten)
                    } else {
                        zos.write(bytes)
                    }
                } else {
                    zis.copyTo(zos)
                }

                zos.closeEntry()
                zis.closeEntry()
            }
        }
    }
}
