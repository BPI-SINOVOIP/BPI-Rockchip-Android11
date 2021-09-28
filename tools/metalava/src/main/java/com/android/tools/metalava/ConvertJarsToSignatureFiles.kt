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
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.model.SUPPORT_TYPE_USE_ANNOTATIONS
import com.android.tools.metalava.model.visitors.ApiVisitor
import com.google.common.io.ByteStreams
import org.objectweb.asm.ClassReader
import org.objectweb.asm.Opcodes
import org.objectweb.asm.tree.ClassNode
import org.objectweb.asm.tree.FieldNode
import org.objectweb.asm.tree.MethodNode
import java.io.File
import java.io.IOException
import java.util.function.Predicate
import java.util.zip.ZipFile

/**
 * In an Android source tree, rewrite the signature files in prebuilts/sdk
 * by reading what's actually there in the android.jar files.
 */
class ConvertJarsToSignatureFiles {
    fun convertJars(root: File) {
        var api = 1
        while (true) {
            val apiJar = File(
                root,
                if (api <= 3)
                    "prebuilts/tools/common/api-versions/android-$api/android.jar"
                else
                    "prebuilts/sdk/$api/public/android.jar"
            )
            if (!apiJar.isFile) {
                break
            }
            val signatureFile = "prebuilts/sdk/$api/public/api/android.txt"
            val oldApiFile = File(root, "prebuilts/sdk/$api/public/api/android.txt")
            val newApiFile =
                // Place new-style signature files in separate files?
                // File(root, "prebuilts/sdk/$api/public/api/android.${if (options.compatOutput) "txt" else "v2.txt"}")
                File(root, "prebuilts/sdk/$api/public/api/android.txt")

            progress("Writing signature files $signatureFile for $apiJar")

            // Treat android.jar file as not filtered since they contain misc stuff that shouldn't be
            // there: package private super classes etc.
            val jarCodebase = loadFromJarFile(apiJar, null, preFiltered = false)
            val apiEmit = ApiType.PUBLIC_API.getEmitFilter()
            val apiReference = ApiType.PUBLIC_API.getReferenceFilter()

            if (api >= 28) {
                // As of API 28 we'll put nullness annotations into the jar but some of them
                // may be @RecentlyNullable/@RecentlyNonNull. Translate these back into
                // normal @Nullable/@NonNull
                jarCodebase.accept(object : ApiVisitor() {
                    override fun visitItem(item: Item) {
                        unmarkRecent(item)
                        super.visitItem(item)
                    }

                    private fun unmarkRecent(new: Item) {
                        val annotation = NullnessMigration.findNullnessAnnotation(new) ?: return
                        // Nullness information change: Add migration annotation
                        val annotationClass = if (annotation.isNullable()) ANDROIDX_NULLABLE else ANDROIDX_NONNULL

                        val modifiers = new.mutableModifiers()
                        modifiers.removeAnnotation(annotation)

                        // Don't map annotation names - this would turn newly non null back into non null
                        modifiers.addAnnotation(new.codebase.createAnnotation("@$annotationClass", new, mapName = false))
                    }
                })
                assert(!SUPPORT_TYPE_USE_ANNOTATIONS) { "We'll need to rewrite type annotations here too" }
            }

            // Sadly the old signature files have some APIs recorded as deprecated which
            // are not in fact deprecated in the jar files. Try to pull this back in.

            val oldRemovedFile = File(root, "prebuilts/sdk/$api/public/api/removed.txt")
            if (oldRemovedFile.isFile) {
                val oldCodebase = SignatureFileLoader.load(oldRemovedFile)
                val visitor = object : ComparisonVisitor() {
                    override fun compare(old: MethodItem, new: MethodItem) {
                        new.removed = true
                        progress("Removed $old")
                    }

                    override fun compare(old: FieldItem, new: FieldItem) {
                        new.removed = true
                        progress("Removed $old")
                    }
                }
                CodebaseComparator().compare(visitor, oldCodebase, jarCodebase, null)
            }

            // Read deprecated attributes. Seem to be missing from code model;
            // try to read via ASM instead since it must clearly be there.
            markDeprecated(jarCodebase, apiJar, apiJar.path)

            // ASM doesn't seem to pick up everything that's actually there according to
            // javap. So as another fallback, read from the existing signature files:
            if (oldApiFile.isFile) {
                val oldCodebase = SignatureFileLoader.load(oldApiFile)
                val visitor = object : ComparisonVisitor() {
                    override fun compare(old: Item, new: Item) {
                        if (old.deprecated && !new.deprecated && old !is PackageItem) {
                            new.deprecated = true
                            progress("Recorded deprecation from previous signature file for $old")
                        }
                    }
                }
                CodebaseComparator().compare(visitor, oldCodebase, jarCodebase, null)
            }

            createReportFile(jarCodebase, newApiFile, "API") { printWriter ->
                SignatureWriter(printWriter, apiEmit, apiReference, jarCodebase.preFiltered)
            }

            // Delete older redundant .xml files
            val xmlFile = File(newApiFile.parentFile, "android.xml")
            if (xmlFile.isFile) {
                xmlFile.delete()
            }

            api++
        }
    }

    private fun markDeprecated(codebase: Codebase, file: File, path: String) {
        when {
            file.name.endsWith(SdkConstants.DOT_JAR) -> try {
                ZipFile(file).use { jar ->
                    val enumeration = jar.entries()
                    while (enumeration.hasMoreElements()) {
                        val entry = enumeration.nextElement()
                        if (entry.name.endsWith(SdkConstants.DOT_CLASS)) {
                            try {
                                jar.getInputStream(entry).use { `is` ->
                                    val bytes = ByteStreams.toByteArray(`is`)
                                    if (bytes != null) {
                                        markDeprecated(codebase, bytes, path + ":" + entry.name)
                                    }
                                }
                            } catch (e: Exception) {
                                options.stdout.println("Could not read jar file entry ${entry.name} from $file: $e")
                            }
                        }
                    }
                }
            } catch (e: IOException) {
                options.stdout.println("Could not read jar file contents from $file: $e")
            }
            file.isDirectory -> {
                val listFiles = file.listFiles()
                listFiles?.forEach {
                    markDeprecated(codebase, it, it.path)
                }
            }
            file.path.endsWith(SdkConstants.DOT_CLASS) -> {
                val bytes = file.readBytes()
                markDeprecated(codebase, bytes, file.path)
            }
            else -> options.stdout.println("Ignoring entry $file")
        }
    }

    private fun markDeprecated(codebase: Codebase, bytes: ByteArray, path: String) {
        val reader: ClassReader
        val classNode: ClassNode
        try {
            // TODO: We don't actually need to build a DOM.
            reader = ClassReader(bytes)
            classNode = ClassNode()
            reader.accept(classNode, 0)
        } catch (t: Throwable) {
            options.stderr.println("Error processing $path: broken class file?")
            return
        }

        if ((classNode.access and Opcodes.ACC_DEPRECATED) != 0) {
            val item = codebase.findClass(classNode, MATCH_ALL)
            if (item != null && !item.deprecated) {
                item.deprecated = true
                progress("Turned deprecation on for $item")
            }
        }

        val methodList = classNode.methods
        for (f in methodList) {
            val methodNode = f as MethodNode
            if ((methodNode.access and Opcodes.ACC_DEPRECATED) == 0) {
                continue
            }
            val item = codebase.findMethod(classNode, methodNode, MATCH_ALL)
            if (item != null && !item.deprecated) {
                item.deprecated = true
                progress("Turned deprecation on for $item")
            }
        }

        val fieldList = classNode.fields
        for (f in fieldList) {
            val fieldNode = f as FieldNode
            if ((fieldNode.access and Opcodes.ACC_DEPRECATED) == 0) {
                continue
            }
            val item = codebase.findField(classNode, fieldNode, MATCH_ALL)
            if (item != null && !item.deprecated) {
                item.deprecated = true
                progress("Turned deprecation on for $item")
            }
        }
    }

    companion object {
        val MATCH_ALL: Predicate<Item> = Predicate { true }
    }
}