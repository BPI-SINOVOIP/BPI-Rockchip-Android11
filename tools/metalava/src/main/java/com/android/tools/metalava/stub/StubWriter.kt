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

package com.android.tools.metalava.stub

import com.android.tools.metalava.doclava1.ApiPredicate
import com.android.tools.metalava.doclava1.FilterPredicate
import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.AnnotationTarget
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.ConstructorItem
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.ModifierList
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.model.psi.EXPAND_DOCUMENTATION
import com.android.tools.metalava.model.psi.trimDocIndent
import com.android.tools.metalava.model.visitors.ApiVisitor
import com.android.tools.metalava.model.visitors.ItemVisitor
import com.android.tools.metalava.options
import com.android.tools.metalava.reporter
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.io.IOException
import java.io.PrintWriter

class StubWriter(
    private val codebase: Codebase,
    private val stubsDir: File,
    private val generateAnnotations: Boolean = false,
    private val preFiltered: Boolean = true,
    private val docStubs: Boolean
) : ApiVisitor(
    visitConstructorsAsMethods = false,
    nestInnerClasses = true,
    inlineInheritedFields = true,
    fieldComparator = FieldItem.comparator,
    // Methods are by default sorted in source order in stubs, to encourage methods
    // that are near each other in the source to show up near each other in the documentation
    methodComparator = MethodItem.sourceOrderComparator,
    filterEmit = FilterPredicate(ApiPredicate(ignoreShown = true, includeDocOnly = docStubs))
        // In stubs we have to include non-strippable things too. This is an error in the API,
        // and we've removed all of it from the framework, but there are libraries which still
        // have reference errors.
        .or { it is ClassItem && it.notStrippable },
    filterReference = ApiPredicate(ignoreShown = true, includeDocOnly = docStubs),
    includeEmptyOuterClasses = true
) {
    private val annotationTarget =
        if (docStubs) AnnotationTarget.DOC_STUBS_FILE else AnnotationTarget.SDK_STUBS_FILE

    private val sourceList = StringBuilder(20000)

    /** Writes a source file list of the generated stubs */
    fun writeSourceList(target: File, root: File?) {
        target.parentFile?.mkdirs()
        val contents = if (root != null) {
            val path = root.path.replace('\\', '/') + "/"
            sourceList.toString().replace(path, "")
        } else {
            sourceList.toString()
        }
        target.writeText(contents)
    }

    private fun startFile(sourceFile: File) {
        if (sourceList.isNotEmpty()) {
            sourceList.append(' ')
        }
        sourceList.append(sourceFile.path.replace('\\', '/'))
    }

    override fun visitPackage(pkg: PackageItem) {
        getPackageDir(pkg, create = true)

        writePackageInfo(pkg)

        if (docStubs) {
            codebase.getPackageDocs()?.let { packageDocs ->
                packageDocs.getOverviewDocumentation(pkg)?.let { writeDocOverview(pkg, it) }
            }
        }
    }

    fun writeDocOverview(pkg: PackageItem, content: String) {
        if (content.isBlank()) {
            return
        }

        val sourceFile = File(getPackageDir(pkg), "overview.html")
        val overviewWriter = try {
            PrintWriter(BufferedWriter(FileWriter(sourceFile)))
        } catch (e: IOException) {
            reporter.report(Issues.IO_ERROR, sourceFile, "Cannot open file for write.")
            return
        }

        // Should we include this in our stub list?
        //     startFile(sourceFile)

        overviewWriter.println(content)
        overviewWriter.flush()
        overviewWriter.close()
    }

    private fun writePackageInfo(pkg: PackageItem) {
        val annotations = pkg.modifiers.annotations()
        if (annotations.isNotEmpty() && generateAnnotations || !pkg.documentation.isBlank()) {
            val sourceFile = File(getPackageDir(pkg), "package-info.java")
            val packageInfoWriter = try {
                PrintWriter(BufferedWriter(FileWriter(sourceFile)))
            } catch (e: IOException) {
                reporter.report(Issues.IO_ERROR, sourceFile, "Cannot open file for write.")
                return
            }
            startFile(sourceFile)

            appendDocumentation(pkg, packageInfoWriter)

            if (annotations.isNotEmpty()) {
                ModifierList.writeAnnotations(
                    list = pkg.modifiers,
                    separateLines = true,
                    // Some bug in UAST triggers duplicate nullability annotations
                    // here; make sure the are filtered out
                    filterDuplicates = true,
                    target = annotationTarget,
                    writer = packageInfoWriter
                )
            }
            packageInfoWriter.println("package ${pkg.qualifiedName()};")

            packageInfoWriter.flush()
            packageInfoWriter.close()
        }
    }

    private fun getPackageDir(packageItem: PackageItem, create: Boolean = true): File {
        val relative = packageItem.qualifiedName().replace('.', File.separatorChar)
        val dir = File(stubsDir, relative)
        if (create && !dir.isDirectory) {
            val ok = dir.mkdirs()
            if (!ok) {
                throw IOException("Could not create $dir")
            }
        }

        return dir
    }

    private fun getClassFile(classItem: ClassItem): File {
        assert(classItem.containingClass() == null) { "Should only be called on top level classes" }
        // TODO: Look up compilation unit language
        return File(getPackageDir(classItem.containingPackage()), "${classItem.simpleName()}.java")
    }

    /**
     * Between top level class files the [textWriter] field doesn't point to a real file; it
     * points to this writer, which redirects to the error output. Nothing should be written
     * to the writer at that time.
     */
    private var errorTextWriter = PrintWriter(options.stderr)

    /** The writer to write the stubs file to */
    private var textWriter: PrintWriter = errorTextWriter

    private var stubWriter: ItemVisitor? = null

    override fun visitClass(cls: ClassItem) {
        if (cls.isTopLevelClass()) {
            val sourceFile = getClassFile(cls)
            textWriter = try {
                PrintWriter(BufferedWriter(FileWriter(sourceFile)))
            } catch (e: IOException) {
                reporter.report(Issues.IO_ERROR, sourceFile, "Cannot open file for write.")
                errorTextWriter
            }

            startFile(sourceFile)

            stubWriter = JavaStubWriter(textWriter, filterEmit, filterReference, generateAnnotations, preFiltered, docStubs)

            // Copyright statements from the original file?
            val compilationUnit = cls.getCompilationUnit()
            compilationUnit?.getHeaderComments()?.let { textWriter.println(it) }
        }
        stubWriter?.visitClass(cls)
    }

    private fun appendDocumentation(item: Item, writer: PrintWriter) {
        if (options.includeDocumentationInStubs || docStubs) {
            val documentation = if (docStubs && EXPAND_DOCUMENTATION) {
                item.fullyQualifiedDocumentation()
            } else {
                item.documentation
            }
            if (documentation.isNotBlank()) {
                val trimmed = trimDocIndent(documentation)
                writer.println(trimmed)
                writer.println()
            }
        }
    }

    override fun afterVisitClass(cls: ClassItem) {
        stubWriter?.afterVisitClass(cls)

        if (cls.isTopLevelClass()) {
            textWriter.flush()
            textWriter.close()
            textWriter = errorTextWriter
            stubWriter = null
        }
    }

    override fun visitConstructor(constructor: ConstructorItem) {
        stubWriter?.visitConstructor(constructor)
    }

    override fun afterVisitConstructor(constructor: ConstructorItem) {
        stubWriter?.afterVisitConstructor(constructor)
    }

    override fun visitMethod(method: MethodItem) {
        stubWriter?.visitMethod(method)
    }

    override fun afterVisitMethod(method: MethodItem) {
        stubWriter?.afterVisitMethod(method)
    }

    override fun visitField(field: FieldItem) {
        stubWriter?.visitField(field)
    }

    override fun afterVisitField(field: FieldItem) {
        stubWriter?.afterVisitField(field)
    }
}