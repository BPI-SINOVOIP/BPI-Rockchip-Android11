/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tools.metalava.model

import com.android.SdkConstants.ANDROID_URI
import com.android.SdkConstants.ATTR_MIN_SDK_VERSION
import com.android.SdkConstants.ATTR_NAME
import com.android.SdkConstants.TAG_PERMISSION
import com.android.SdkConstants.TAG_USES_SDK
import com.android.tools.metalava.CodebaseComparator
import com.android.tools.metalava.ComparisonVisitor
import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.psi.CodePrinter
import com.android.tools.metalava.model.text.TextBackedAnnotationItem
import com.android.tools.metalava.model.visitors.ItemVisitor
import com.android.tools.metalava.model.visitors.TypeVisitor
import com.android.tools.metalava.reporter
import com.android.utils.XmlUtils.getFirstSubTagByName
import com.android.utils.XmlUtils.getNextTagByName
import com.intellij.psi.PsiFile
import org.intellij.lang.annotations.Language
import org.objectweb.asm.Type
import org.objectweb.asm.tree.ClassNode
import org.objectweb.asm.tree.FieldInsnNode
import org.objectweb.asm.tree.FieldNode
import org.objectweb.asm.tree.MethodInsnNode
import org.objectweb.asm.tree.MethodNode
import java.io.File
import java.util.function.Predicate
import kotlin.text.Charsets.UTF_8

/**
 * Represents a complete unit of code -- typically in the form of a set
 * of source trees, but also potentially backed by .jar files or even
 * signature files
 */
interface Codebase {
    /** Description of what this codebase is (useful during debugging) */
    var description: String

    /**
     * The location of the API. Could point to a signature file, or a directory
     * root for source files, or a jar file, etc.
     */
    var location: File

    /** The API level of this codebase, or -1 if not known */
    var apiLevel: Int

    /** The packages in the codebase (may include packages that are not included in the API) */
    fun getPackages(): PackageList

    /**
     * The package documentation, if any - this returns overview.html files for each package
     * that provided one. Not all codebases provide this.
     */
    fun getPackageDocs(): PackageDocs?

    /** The rough size of the codebase (package count) */
    fun size(): Int

    /** Returns a class identified by fully qualified name, if in the codebase */
    fun findClass(className: String): ClassItem?

    /** Returns a package identified by fully qualified name, if in the codebase */
    fun findPackage(pkgName: String): PackageItem?

    /** Returns true if this codebase supports documentation. */
    fun supportsDocumentation(): Boolean

    /**
     * Returns true if this codebase corresponds to an already trusted API (e.g.
     * is read in from something like an existing signature file); in that case,
     * signature checks etc will not be performed.
     */
    fun trustedApi(): Boolean

    fun accept(visitor: ItemVisitor) {
        getPackages().accept(visitor)
    }

    fun acceptTypes(visitor: TypeVisitor) {
        getPackages().acceptTypes(visitor)
    }

    /**
     * Visits this codebase and compares it with another codebase, informing the visitors about
     * the correlations and differences that it finds
     */
    fun compareWith(visitor: ComparisonVisitor, other: Codebase, filter: Predicate<Item>? = null) {
        CodebaseComparator().compare(visitor, other, this, filter)
    }

    /**
     * Creates an annotation item for the given (fully qualified) Java source
     */
    fun createAnnotation(
        @Language("JAVA") source: String,
        context: Item? = null,
        mapName: Boolean = true
    ): AnnotationItem = TextBackedAnnotationItem(
        this, source, mapName
    )

    /**
     * Returns true if the codebase contains one or more Kotlin files
     */
    fun hasKotlin(): Boolean {
        return units.any { it.fileType.name == "Kotlin" }
    }

    /**
     * Returns true if the codebase contains one or more Java files
     */
    fun hasJava(): Boolean {
        return units.any { it.fileType.name == "JAVA" }
    }

    /** The manifest to associate with this codebase, if any */
    var manifest: File?

    /**
     * Returns the permission level of the named permission, if specified
     * in the manifest. This method should only be called if the codebase has
     * been configured with a manifest
     */
    fun getPermissionLevel(name: String): String?

    fun getMinSdkVersion(): MinSdkVersion

    /** Clear the [Item.tag] fields (prior to iteration like DFS) */
    fun clearTags() {
        getPackages().packages.forEach { pkg -> pkg.allClasses().forEach { cls -> cls.tag = false } }
    }

    /** Reports that the given operation is unsupported for this codebase type */
    fun unsupported(desc: String? = null): Nothing

    /** Discards this model */
    fun dispose() {
        description += " [disposed]"
    }

    /** If this codebase was filtered from another codebase, this points to the original */
    var original: Codebase?

    /** Returns the compilation units used in this codebase (may be empty
     * when the codebase is not loaded from source, such as from .jar files or
     * from signature files) */
    var units: List<PsiFile>

    /**
     * Printer which can convert PSI, UAST and constants into source code,
     * with ability to filter out elements that are not part of a codebase etc
     */
    val printer: CodePrinter

    /** If true, this codebase has already been filtered */
    val preFiltered: Boolean

    /** Finds the given class by JVM owner */
    fun findClassByOwner(owner: String, apiFilter: Predicate<Item>): ClassItem? {
        val className = owner.replace('/', '.').replace('$', '.')
        val cls = findClass(className)
        return if (cls != null && apiFilter.test(cls)) {
            cls
        } else {
            null
        }
    }

    fun findClass(node: ClassNode, apiFilter: Predicate<Item>): ClassItem? {
        return findClassByOwner(node.name, apiFilter)
    }

    fun findMethod(node: MethodInsnNode, apiFilter: Predicate<Item>): MethodItem? {
        val cls = findClassByOwner(node.owner, apiFilter) ?: return null
        val types = Type.getArgumentTypes(node.desc)
        val parameters = if (types.isNotEmpty()) {
            val sb = StringBuilder()
            for (type in types) {
                if (!sb.isEmpty()) {
                    sb.append(", ")
                }
                sb.append(type.className.replace('/', '.').replace('$', '.'))
            }
            sb.toString()
        } else {
            ""
        }
        val methodName = if (node.name == "<init>") cls.simpleName() else node.name
        val method = cls.findMethod(methodName, parameters)
        return if (method != null && apiFilter.test(method)) {
            method
        } else {
            null
        }
    }

    fun findMethod(classNode: ClassNode, node: MethodNode, apiFilter: Predicate<Item>): MethodItem? {
        val cls = findClass(classNode, apiFilter) ?: return null
        val types = Type.getArgumentTypes(node.desc)
        val parameters = if (types.isNotEmpty()) {
            val sb = StringBuilder()
            for (type in types) {
                if (!sb.isEmpty()) {
                    sb.append(", ")
                }
                sb.append(type.className.replace('/', '.').replace('$', '.'))
            }
            sb.toString()
        } else {
            ""
        }
        val methodName = if (node.name == "<init>") cls.simpleName() else node.name
        val method = cls.findMethod(methodName, parameters)
        return if (method != null && apiFilter.test(method)) {
            method
        } else {
            null
        }
    }

    fun findField(classNode: ClassNode, node: FieldNode, apiFilter: Predicate<Item>): FieldItem? {
        val cls = findClass(classNode, apiFilter) ?: return null
        val field = cls.findField(node.name)
        return if (field != null && apiFilter.test(field)) {
            field
        } else {
            null
        }
    }

    fun findField(node: FieldInsnNode, apiFilter: Predicate<Item>): FieldItem? {
        val cls = findClassByOwner(node.owner, apiFilter) ?: return null
        val field = cls.findField(node.name)
        return if (field != null && apiFilter.test(field)) {
            field
        } else {
            null
        }
    }

    fun isEmpty(): Boolean {
        return getPackages().packages.isEmpty()
    }
}

sealed class MinSdkVersion
data class SetMinSdkVersion(val value: Int) : MinSdkVersion()
object UnsetMinSdkVersion : MinSdkVersion()

abstract class DefaultCodebase(override var location: File) : Codebase {
    override var manifest: File? = null
    private var permissions: Map<String, String>? = null
    private var minSdkVersion: MinSdkVersion? = null
    override var original: Codebase? = null
    override var units: List<PsiFile> = emptyList()
    override var apiLevel: Int = -1
    @Suppress("LeakingThis")
    override val printer = CodePrinter(this)
    @Suppress("LeakingThis")
    override var preFiltered: Boolean = original != null

    override fun getPermissionLevel(name: String): String? {
        if (permissions == null) {
            assert(manifest != null) {
                "This method should only be called when a manifest has been configured on the codebase"
            }
            try {
                val map = HashMap<String, String>(600)
                val doc = parseDocument(manifest?.readText(UTF_8) ?: "", true)
                var current = getFirstSubTagByName(doc.documentElement, TAG_PERMISSION)
                while (current != null) {
                    val permissionName = current.getAttributeNS(ANDROID_URI, ATTR_NAME)
                    val protectionLevel = current.getAttributeNS(ANDROID_URI, "protectionLevel")
                    map[permissionName] = protectionLevel
                    current = getNextTagByName(current, TAG_PERMISSION)
                }
                permissions = map
            } catch (error: Throwable) {
                reporter.report(Issues.PARSE_ERROR, manifest, "Failed to parse $manifest: ${error.message}")
                permissions = emptyMap()
            }
        }

        return permissions!![name]
    }

    override fun getMinSdkVersion(): MinSdkVersion {
        if (minSdkVersion == null) {
            if (manifest == null) {
                minSdkVersion = UnsetMinSdkVersion
                return minSdkVersion!!
            }
            try {
                val doc = parseDocument(manifest?.readText(UTF_8) ?: "", true)
                val usesSdk = getFirstSubTagByName(doc.documentElement, TAG_USES_SDK)
                minSdkVersion = if (usesSdk == null) {
                    UnsetMinSdkVersion
                } else {
                    val value = usesSdk.getAttributeNS(ANDROID_URI, ATTR_MIN_SDK_VERSION)
                    if (value.isEmpty()) UnsetMinSdkVersion else SetMinSdkVersion(value.toInt())
                }
            } catch (error: Throwable) {
                reporter.report(Issues.PARSE_ERROR, manifest, "Failed to parse $manifest: ${error.message}")
                minSdkVersion = UnsetMinSdkVersion
            }
        }
        return minSdkVersion!!
    }

    override fun getPackageDocs(): PackageDocs? = null

    override fun unsupported(desc: String?): Nothing {
        error(desc ?: "This operation is not available on this type of codebase (${this.javaClass.simpleName})")
    }
}
