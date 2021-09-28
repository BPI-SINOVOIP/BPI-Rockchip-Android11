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
@file:JvmName("Driver")

package com.android.tools.metalava

import com.android.SdkConstants.DOT_JAR
import com.android.SdkConstants.DOT_JAVA
import com.android.SdkConstants.DOT_KT
import com.android.SdkConstants.DOT_TXT
import com.android.ide.common.process.CachedProcessOutputHandler
import com.android.ide.common.process.DefaultProcessExecutor
import com.android.ide.common.process.ProcessInfoBuilder
import com.android.ide.common.process.ProcessOutput
import com.android.ide.common.process.ProcessOutputHandler
import com.android.tools.lint.KotlinLintAnalyzerFacade
import com.android.tools.lint.LintCoreApplicationEnvironment
import com.android.tools.lint.LintCoreProjectEnvironment
import com.android.tools.lint.annotations.Extractor
import com.android.tools.lint.checks.infrastructure.ClassName
import com.android.tools.lint.detector.api.assertionsEnabled
import com.android.tools.metalava.CompatibilityCheck.CheckRequest
import com.android.tools.metalava.apilevels.ApiGenerator
import com.android.tools.metalava.doclava1.ApiPredicate
import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.doclava1.FilterPredicate
import com.android.tools.metalava.doclava1.TextCodebase
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.PackageDocs
import com.android.tools.metalava.model.psi.PsiBasedCodebase
import com.android.tools.metalava.model.psi.packageHtmlToJavadoc
import com.android.tools.metalava.model.visitors.ApiVisitor
import com.android.tools.metalava.stub.StubWriter
import com.android.utils.StdLogger
import com.android.utils.StdLogger.Level.ERROR
import com.google.common.base.Stopwatch
import com.google.common.collect.Lists
import com.google.common.io.Files
import com.intellij.core.CoreApplicationEnvironment
import com.intellij.openapi.diagnostic.DefaultLogger
import com.intellij.openapi.extensions.Extensions
import com.intellij.openapi.roots.LanguageLevelProjectExtension
import com.intellij.openapi.util.Disposer
import com.intellij.pom.java.LanguageLevel
import com.intellij.psi.javadoc.CustomJavadocTagProvider
import com.intellij.psi.javadoc.JavadocTagInfo
import java.io.File
import java.io.IOException
import java.io.OutputStream
import java.io.OutputStreamWriter
import java.io.PrintWriter
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeUnit.SECONDS
import java.util.function.Predicate
import kotlin.text.Charsets.UTF_8

const val PROGRAM_NAME = "metalava"
const val HELP_PROLOGUE = "$PROGRAM_NAME extracts metadata from source code to generate artifacts such as the " +
    "signature files, the SDK stub files, external annotations etc."

@Suppress("PropertyName") // Can't mark const because trimIndent() :-(
val BANNER: String = """
                _        _
 _ __ ___   ___| |_ __ _| | __ ___   ____ _
| '_ ` _ \ / _ \ __/ _` | |/ _` \ \ / / _` |
| | | | | |  __/ || (_| | | (_| |\ V / (_| |
|_| |_| |_|\___|\__\__,_|_|\__,_| \_/ \__,_|
""".trimIndent()

fun main(args: Array<String>) {
    run(args, setExitCode = true)
}

internal var hasFileReadViolations = false

/**
 * The metadata driver is a command line interface to extracting various metadata
 * from a source tree (or existing signature files etc). Run with --help to see
 * more details.
 */
fun run(
    originalArgs: Array<String>,
    stdout: PrintWriter = PrintWriter(OutputStreamWriter(System.out)),
    stderr: PrintWriter = PrintWriter(OutputStreamWriter(System.err)),
    setExitCode: Boolean = false
): Boolean {
    var exitCode = 0

    try {
        val modifiedArgs = preprocessArgv(originalArgs)

        progress("$PROGRAM_NAME started\n")

        // Dump the arguments, and maybe generate a rerun-script.
        maybeDumpArgv(stdout, originalArgs, modifiedArgs)

        // Actual work begins here.
        compatibility = Compatibility(compat = Options.useCompatMode(modifiedArgs))
        options = Options(modifiedArgs, stdout, stderr)

        maybeActivateSandbox()

        processFlags()

        if (options.allReporters.any { it.hasErrors() } && !options.passBaselineUpdates) {
            exitCode = -1
        }
        if (hasFileReadViolations) {
            stderr.println("$PROGRAM_NAME detected access to files that are not explicitly specified. See ${options.strictInputViolationsFile} for details.")
            if (options.strictInputFiles == Options.StrictInputFileMode.STRICT) {
                exitCode = -1
            }
        }
    } catch (e: DriverException) {
        stdout.flush()
        stderr.flush()
        if (e.stderr.isNotBlank()) {
            stderr.println("\n${e.stderr}")
        }
        if (e.stdout.isNotBlank()) {
            stdout.println("\n${e.stdout}")
        }
        exitCode = e.exitCode
    } finally {
        Disposer.dispose(LintCoreApplicationEnvironment.get().parentDisposable)
    }

    // Update and close all baseline files.
    options.allBaselines.forEach { baseline ->
        if (options.verbose) {
            baseline.dumpStats(options.stdout)
        }
        if (baseline.close()) {
            if (!options.quiet) {
                stdout.println("$PROGRAM_NAME wrote updated baseline to ${baseline.updateFile}")
            }
        }
    }

    options.reportEvenIfSuppressedWriter?.close()
    options.strictInputViolationsPrintWriter?.close()

    // Show failure messages, if any.
    options.allReporters.forEach {
        it.writeErrorMessage(stderr)
    }

    stdout.flush()
    stderr.flush()

    if (setExitCode) {
        exit(exitCode)
    }

    return exitCode == 0
}

private fun exit(exitCode: Int = 0) {
    if (options.verbose) {
        progress("$PROGRAM_NAME exiting with exit code $exitCode\n")
    }
    options.stdout.flush()
    options.stderr.flush()
    System.exit(exitCode)
}

private fun maybeActivateSandbox() {
    // Set up a sandbox to detect access to files that are not explicitly specified.
    if (options.strictInputFiles == Options.StrictInputFileMode.PERMISSIVE) {
        return
    }

    val writer = options.strictInputViolationsPrintWriter!!

    // Writes all violations to [Options.strictInputFiles].
    // If Options.StrictInputFile.Mode is STRICT, then all violations on reads are logged, and the
    // tool exits with a negative error code if there are any file read violations. Directory read
    // violations are logged, but are considered to be a "warning" and doesn't affect the exit code.
    // If STRICT_WARN, all violations on reads are logged similar to STRICT, but the exit code is
    // unaffected.
    // If STRICT_WITH_STACK, similar to STRICT, but also logs the stack trace to
    // Options.strictInputFiles.
    // See [FileReadSandbox] for the details.
    FileReadSandbox.activate(object : FileReadSandbox.Listener {
        var seen = mutableSetOf<String>()
        override fun onViolation(absolutePath: String, isDirectory: Boolean) {
            if (!seen.contains(absolutePath)) {
                val suffix = if (isDirectory) "/" else ""
                writer.println("$absolutePath$suffix")
                if (options.strictInputFiles == Options.StrictInputFileMode.STRICT_WITH_STACK) {
                    Throwable().printStackTrace(writer)
                }
                seen.add(absolutePath)
                if (!isDirectory) {
                    hasFileReadViolations = true
                }
            }
        }
    })
}

private fun processFlags() {
    val stopwatch = Stopwatch.createStarted()

    processNonCodebaseFlags()

    val sources = options.sources
    val codebase =
        if (sources.size >= 1 && sources[0].path.endsWith(DOT_TXT)) {
            // Make sure all the source files have .txt extensions.
            sources.firstOrNull { !it.path.endsWith(DOT_TXT) }?. let {
                throw DriverException("Inconsistent input file types: The first file is of $DOT_TXT, but detected different extension in ${it.path}")
            }
            SignatureFileLoader.loadFiles(sources, options.inputKotlinStyleNulls)
        } else if (options.apiJar != null) {
            loadFromJarFile(options.apiJar!!)
        } else if (sources.size == 1 && sources[0].path.endsWith(DOT_JAR)) {
            loadFromJarFile(sources[0])
        } else if (sources.isNotEmpty() || options.sourcePath.isNotEmpty()) {
            loadFromSources()
        } else {
            return
        }
    options.manifest?.let { codebase.manifest = it }

    if (options.verbose) {
        progress("$PROGRAM_NAME analyzed API in ${stopwatch.elapsed(TimeUnit.SECONDS)} seconds\n")
    }

    options.subtractApi?.let {
        progress("Subtracting API: ")
        subtractApi(codebase, it)
    }

    val androidApiLevelXml = options.generateApiLevelXml
    val apiLevelJars = options.apiLevelJars
    if (androidApiLevelXml != null && apiLevelJars != null) {
        progress("Generating API levels XML descriptor file, ${androidApiLevelXml.name}: ")
        ApiGenerator.generate(apiLevelJars, androidApiLevelXml, codebase)
    }

    if (options.docStubsDir != null && codebase.supportsDocumentation()) {
        progress("Enhancing docs: ")
        val docAnalyzer = DocAnalyzer(codebase)
        docAnalyzer.enhance()

        val applyApiLevelsXml = options.applyApiLevelsXml
        if (applyApiLevelsXml != null) {
            progress("Applying API levels")
            docAnalyzer.applyApiLevels(applyApiLevelsXml)
        }
    }

    // Generate the documentation stubs *before* we migrate nullness information.
    options.docStubsDir?.let {
        createStubFiles(
            it, codebase, docStubs = true,
            writeStubList = options.docStubsSourceList != null
        )
    }

    // Based on the input flags, generates various output files such
    // as signature files and/or stubs files
    options.apiFile?.let { apiFile ->
        val apiType = ApiType.PUBLIC_API
        val apiEmit = apiType.getEmitFilter()
        val apiReference = apiType.getReferenceFilter()

        createReportFile(codebase, apiFile, "API") { printWriter ->
            SignatureWriter(printWriter, apiEmit, apiReference, codebase.preFiltered)
        }
    }

    options.dexApiFile?.let { apiFile ->
        val apiFilter = FilterPredicate(ApiPredicate())
        val memberIsNotCloned: Predicate<Item> = Predicate { !it.isCloned() }
        val apiReference = ApiPredicate(ignoreShown = true)
        val dexApiEmit = memberIsNotCloned.and(apiFilter)

        createReportFile(
            codebase, apiFile, "DEX API"
        ) { printWriter -> DexApiWriter(printWriter, dexApiEmit, apiReference) }
    }

    options.apiXmlFile?.let { apiFile ->
        val apiType = ApiType.PUBLIC_API
        val apiEmit = apiType.getEmitFilter()
        val apiReference = apiType.getReferenceFilter()

        createReportFile(codebase, apiFile, "XML API") { printWriter ->
            JDiffXmlWriter(printWriter, apiEmit, apiReference, codebase.preFiltered)
        }
    }

    options.dexApiMappingFile?.let { apiFile ->
        val apiType = ApiType.ALL
        val apiEmit = apiType.getEmitFilter()
        val apiReference = apiType.getReferenceFilter()

        createReportFile(
            codebase, apiFile, "DEX API Mapping"
        ) { printWriter ->
            DexApiWriter(
                printWriter, apiEmit, apiReference,
                membersOnly = true,
                includePositions = true
            )
        }
    }

    options.removedApiFile?.let { apiFile ->
        val unfiltered = codebase.original ?: codebase

        val apiType = ApiType.REMOVED
        val removedEmit = apiType.getEmitFilter()
        val removedReference = apiType.getReferenceFilter()

        createReportFile(unfiltered, apiFile, "removed API") { printWriter ->
            SignatureWriter(printWriter, removedEmit, removedReference, codebase.original != null)
        }
    }

    options.removedDexApiFile?.let { apiFile ->
        val unfiltered = codebase.original ?: codebase

        val removedFilter = FilterPredicate(ApiPredicate(matchRemoved = true))
        val removedReference = ApiPredicate(ignoreShown = true, ignoreRemoved = true)
        val memberIsNotCloned: Predicate<Item> = Predicate { !it.isCloned() }
        val removedDexEmit = memberIsNotCloned.and(removedFilter)

        createReportFile(
            unfiltered, apiFile, "removed DEX API"
        ) { printWriter -> DexApiWriter(printWriter, removedDexEmit, removedReference) }
    }

    options.privateApiFile?.let { apiFile ->
        val apiType = ApiType.PRIVATE
        val privateEmit = apiType.getEmitFilter()
        val privateReference = apiType.getReferenceFilter()

        createReportFile(codebase, apiFile, "private API") { printWriter ->
            SignatureWriter(printWriter, privateEmit, privateReference, codebase.original != null)
        }
    }

    options.privateDexApiFile?.let { apiFile ->
        val apiFilter = FilterPredicate(ApiPredicate())
        val privateEmit = apiFilter.negate()
        val privateReference = Predicate<Item> { true }

        createReportFile(
            codebase, apiFile, "private DEX API"
        ) { printWriter ->
            DexApiWriter(
                printWriter, privateEmit, privateReference, inlineInheritedFields = false
            )
        }
    }

    options.proguard?.let { proguard ->
        val apiEmit = FilterPredicate(ApiPredicate())
        val apiReference = ApiPredicate(ignoreShown = true)
        createReportFile(
            codebase, proguard, "Proguard file"
        ) { printWriter -> ProguardWriter(printWriter, apiEmit, apiReference) }
    }

    options.sdkValueDir?.let { dir ->
        dir.mkdirs()
        SdkFileWriter(codebase, dir).generate()
    }

    for (check in options.compatibilityChecks) {
        checkCompatibility(codebase, check)
    }

    val previousApiFile = options.migrateNullsFrom
    if (previousApiFile != null) {
        val previous =
            if (previousApiFile.path.endsWith(DOT_JAR)) {
                loadFromJarFile(previousApiFile)
            } else {
                SignatureFileLoader.load(
                    file = previousApiFile,
                    kotlinStyleNulls = options.inputKotlinStyleNulls
                )
            }

        // If configured, checks for newly added nullness information compared
        // to the previous stable API and marks the newly annotated elements
        // as migrated (which will cause the Kotlin compiler to treat problems
        // as warnings instead of errors

        migrateNulls(codebase, previous)

        previous.dispose()
    }

    convertToWarningNullabilityAnnotations(codebase, options.forceConvertToWarningNullabilityAnnotations)

    // Now that we've migrated nullness information we can proceed to write non-doc stubs, if any.

    options.stubsDir?.let {
        createStubFiles(
            it, codebase, docStubs = false,
            writeStubList = options.stubsSourceList != null
        )

        val stubAnnotations = options.copyStubAnnotationsFrom
        if (stubAnnotations != null) {
            // Support pointing to both stub-annotations and stub-annotations/src/main/java
            val src = File(stubAnnotations, "src${File.separator}main${File.separator}java")
            val source = if (src.isDirectory) src else stubAnnotations
            source.listFiles()?.forEach { file ->
                RewriteAnnotations().copyAnnotations(codebase, file, File(it, file.name))
            }
        }
    }

    if (options.docStubsDir == null && options.stubsDir == null) {
        val writeStubsFile: (File) -> Unit = { file ->
            val root = File("").absoluteFile
            val rootPath = root.path
            val contents = sources.joinToString(" ") {
                val path = it.path
                if (path.startsWith(rootPath)) {
                    path.substring(rootPath.length)
                } else {
                    path
                }
            }
            file.writeText(contents)
        }
        options.stubsSourceList?.let(writeStubsFile)
        options.docStubsSourceList?.let(writeStubsFile)
    }
    options.externalAnnotations?.let { extractAnnotations(codebase, it) }

    // Coverage stats?
    if (options.dumpAnnotationStatistics) {
        progress("Measuring annotation statistics: ")
        AnnotationStatistics(codebase).count()
    }
    if (options.annotationCoverageOf.isNotEmpty()) {
        progress("Measuring annotation coverage: ")
        AnnotationStatistics(codebase).measureCoverageOf(options.annotationCoverageOf)
    }

    if (options.verbose) {
        val packageCount = codebase.size()
        progress("$PROGRAM_NAME finished handling $packageCount packages in ${stopwatch.elapsed(SECONDS)} seconds\n")
    }

    invokeDocumentationTool()
}

fun subtractApi(codebase: Codebase, subtractApiFile: File) {
    val path = subtractApiFile.path
    val oldCodebase =
        when {
            path.endsWith(DOT_TXT) -> SignatureFileLoader.load(subtractApiFile)
            path.endsWith(DOT_JAR) -> loadFromJarFile(subtractApiFile)
            else -> throw DriverException("Unsupported $ARG_SUBTRACT_API format, expected .txt or .jar: ${subtractApiFile.name}")
        }

    CodebaseComparator().compare(object : ComparisonVisitor() {
        override fun compare(old: ClassItem, new: ClassItem) {
            new.included = false
            new.emit = false
        }
    }, oldCodebase, codebase, ApiType.ALL.getReferenceFilter())
}

fun processNonCodebaseFlags() {
    // --copy-annotations?
    val privateAnnotationsSource = options.privateAnnotationsSource
    val privateAnnotationsTarget = options.privateAnnotationsTarget
    if (privateAnnotationsSource != null && privateAnnotationsTarget != null) {
        val rewrite = RewriteAnnotations()
        // Support pointing to both stub-annotations and stub-annotations/src/main/java
        val src = File(privateAnnotationsSource, "src${File.separator}main${File.separator}java")
        val source = if (src.isDirectory) src else privateAnnotationsSource
        source.listFiles()?.forEach { file ->
            rewrite.modifyAnnotationSources(null, file, File(privateAnnotationsTarget, file.name))
        }
    }

    // --rewrite-annotations?
    options.rewriteAnnotations?.let { RewriteAnnotations().rewriteAnnotations(it) }

    // Convert android.jar files?
    options.androidJarSignatureFiles?.let { root ->
        // Generate API signature files for all the historical JAR files
        ConvertJarsToSignatureFiles().convertJars(root)
    }

    for (convert in options.convertToXmlFiles) {
        val signatureApi = SignatureFileLoader.load(
            file = convert.fromApiFile,
            kotlinStyleNulls = options.inputKotlinStyleNulls
        )

        val apiType = ApiType.ALL
        val apiEmit = apiType.getEmitFilter()
        val strip = convert.strip
        val apiReference = if (strip) apiType.getEmitFilter() else apiType.getReferenceFilter()
        val baseFile = convert.baseApiFile

        val outputApi =
            if (baseFile != null) {
                // Convert base on a diff
                val baseApi = SignatureFileLoader.load(
                    file = baseFile,
                    kotlinStyleNulls = options.inputKotlinStyleNulls
                )

                val includeFields =
                    if (convert.outputFormat == FileFormat.V2) true else compatibility.includeFieldsInApiDiff
                TextCodebase.computeDelta(baseFile, baseApi, signatureApi, includeFields)
            } else {
                signatureApi
            }

        if (outputApi.isEmpty() && baseFile != null && compatibility.compat) {
            // doclava compatibility: emits error warning instead of emitting empty <api/> element
            options.stdout.println("No API change detected, not generating diff")
        } else {
            val output = convert.outputFile
            if (convert.outputFormat == FileFormat.JDIFF) {
                // See JDiff's XMLToAPI#nameAPI
                val apiName = convert.outputFile.nameWithoutExtension.replace(' ', '_')
                createReportFile(outputApi, output, "JDiff File") { printWriter ->
                    JDiffXmlWriter(printWriter, apiEmit, apiReference, signatureApi.preFiltered && !strip, apiName)
                }
            } else {
                val prevOptions = options
                val prevCompatibility = compatibility
                try {
                    when (convert.outputFormat) {
                        FileFormat.V1 -> {
                            compatibility = Compatibility(true)
                            options = Options(emptyArray(), options.stdout, options.stderr)
                            FileFormat.V1.configureOptions(options, compatibility)
                        }
                        FileFormat.V2 -> {
                            compatibility = Compatibility(false)
                            options = Options(emptyArray(), options.stdout, options.stderr)
                            FileFormat.V2.configureOptions(options, compatibility)
                        }
                        else -> error("Unsupported format ${convert.outputFormat}")
                    }

                    createReportFile(outputApi, output, "Diff API File") { printWriter ->
                        SignatureWriter(
                            printWriter, apiEmit, apiReference, signatureApi.preFiltered && !strip
                        )
                    }
                } finally {
                    options = prevOptions
                    compatibility = prevCompatibility
                }
            }
        }
    }
}

/**
 * Checks compatibility of the given codebase with the codebase described in the
 * signature file.
 */
fun checkCompatibility(
    codebase: Codebase,
    check: CheckRequest
) {
    progress("Checking API compatibility ($check): ")
    val signatureFile = check.file

    val current =
        if (signatureFile.path.endsWith(DOT_JAR)) {
            loadFromJarFile(signatureFile)
        } else {
            SignatureFileLoader.load(
                file = signatureFile,
                kotlinStyleNulls = options.inputKotlinStyleNulls
            )
        }

    if (current is TextCodebase && current.format > FileFormat.V1 && options.outputFormat == FileFormat.V1) {
        throw DriverException("Cannot perform compatibility check of signature file $signatureFile in format ${current.format} without analyzing current codebase with $ARG_FORMAT=${current.format}")
    }

    var base: Codebase? = null
    val releaseType = check.releaseType
    val apiType = check.apiType

    // If diffing with a system-api or test-api (or other signature-based codebase
    // generated from --show-annotations), the API is partial: it's only listing
    // the API that is *different* from the base API. This really confuses the
    // codebase comparison when diffing with a complete codebase, since it looks like
    // many classes and members have been added and removed. Therefore, the comparison
    // is simpler if we just make the comparison with the same generated signature
    // file. If we've only emitted one for the new API, use it directly, if not, generate
    // it first
    val new =
        if (check.codebase != null) {
            SignatureFileLoader.load(
                file = check.codebase,
                kotlinStyleNulls = options.inputKotlinStyleNulls
            )
        } else if (!options.showUnannotated || apiType != ApiType.PUBLIC_API) {
            val apiFile = apiType.getSignatureFile(codebase, "compat-check-signatures-$apiType")

            // Fast path: if the signature files are identical, we're already good!
            if (apiFile.readText(UTF_8) == signatureFile.readText(UTF_8)) {
                return
            }

            base = codebase

            SignatureFileLoader.load(
                file = apiFile,
                kotlinStyleNulls = options.inputKotlinStyleNulls
            )
        } else {
            // Fast path: if we've already generated a signature file and it's identical, we're good!
            val apiFile = options.apiFile
            if (apiFile != null && apiFile.readText(UTF_8) == signatureFile.readText(UTF_8)) {
                return
            }

            codebase
        }

    // If configured, compares the new API with the previous API and reports
    // any incompatibilities.
    CompatibilityCheck.checkCompatibility(new, current, releaseType, apiType, base)

    // Make sure the text files are identical too? (only applies for *current.txt;
    // last-released is expected to differ)
    if (releaseType == ReleaseType.DEV && !options.allowCompatibleDifferences) {
        val apiFile = if (new.location.isFile)
            new.location
        else
            apiType.getSignatureFile(codebase, "compat-diff-signatures-$apiType")

        fun getCanonicalSignatures(file: File): String {
            // Get rid of trailing newlines and Windows line endings
            val text = file.readText(UTF_8)
            return text.replace("\r\n", "\n").trim()
        }
        val currentTxt = getCanonicalSignatures(signatureFile)
        val newTxt = getCanonicalSignatures(apiFile)
        if (newTxt != currentTxt) {
            val diff = getNativeDiff(signatureFile, apiFile) ?: getDiff(currentTxt, newTxt, 1)
            val updateApi = if (isBuildingAndroid())
                "Run make update-api to update.\n"
            else
                ""
            val message =
                """
                    Aborting: Your changes have resulted in differences in the signature file
                    for the ${apiType.displayName} API.

                    The changes may be compatible, but the signature file needs to be updated.
                    $updateApi
                    Diffs:
                """.trimIndent() + "\n" + diff

            throw DriverException(exitCode = -1, stderr = message)
        }
    }
}

fun createTempFile(namePrefix: String, nameSuffix: String): File {
    val tempFolder = options.tempFolder
    return if (tempFolder != null) {
        val preferred = File(tempFolder, namePrefix + nameSuffix)
        if (!preferred.exists()) {
            return preferred
        }
        File.createTempFile(namePrefix, nameSuffix, tempFolder)
    } else {
        File.createTempFile(namePrefix, nameSuffix)
    }
}

fun invokeDocumentationTool() {
    if (options.noDocs) {
        return
    }

    val args = options.invokeDocumentationToolArguments
    if (args.isNotEmpty()) {
        if (!options.quiet) {
            options.stdout.println(
                "Invoking external documentation tool ${args[0]} with arguments\n\"${
                args.slice(1 until args.size).joinToString(separator = "\",\n\"") { it }}\""
            )
            options.stdout.flush()
        }

        val builder = ProcessInfoBuilder()

        builder.setExecutable(File(args[0]))
        builder.addArgs(args.slice(1 until args.size))

        val processOutputHandler =
            if (options.quiet) {
                CachedProcessOutputHandler()
            } else {
                object : ProcessOutputHandler {
                    override fun handleOutput(processOutput: ProcessOutput?) {
                    }

                    override fun createOutput(): ProcessOutput {
                        val out = PrintWriterOutputStream(options.stdout)
                        val err = PrintWriterOutputStream(options.stderr)
                        return object : ProcessOutput {
                            override fun getStandardOutput(): OutputStream {
                                return out
                            }

                            override fun getErrorOutput(): OutputStream {
                                return err
                            }

                            override fun close() {
                                out.flush()
                                err.flush()
                            }
                        }
                    }
                }
            }

        val result = DefaultProcessExecutor(StdLogger(ERROR))
            .execute(builder.createProcess(), processOutputHandler)

        val exitCode = result.exitValue
        if (!options.quiet) {
            options.stdout.println("${args[0]} finished with exitCode $exitCode")
            options.stdout.flush()
        }
        if (exitCode != 0) {
            val stdout = if (processOutputHandler is CachedProcessOutputHandler)
                processOutputHandler.processOutput.standardOutputAsString
            else ""
            val stderr = if (processOutputHandler is CachedProcessOutputHandler)
                processOutputHandler.processOutput.errorOutputAsString
            else ""
            throw DriverException(
                stdout = "Invoking documentation tool ${args[0]} failed with exit code $exitCode\n$stdout",
                stderr = stderr,
                exitCode = exitCode
            )
        }
    }
}

class PrintWriterOutputStream(private val writer: PrintWriter) : OutputStream() {

    override fun write(b: ByteArray) {
        writer.write(String(b, UTF_8))
    }

    override fun write(b: Int) {
        write(byteArrayOf(b.toByte()), 0, 1)
    }

    override fun write(b: ByteArray, off: Int, len: Int) {
        writer.write(String(b, off, len, UTF_8))
    }

    override fun flush() {
        writer.flush()
    }

    override fun close() {
        writer.close()
    }
}

private fun migrateNulls(codebase: Codebase, previous: Codebase) {
    previous.compareWith(NullnessMigration(), codebase)
}

private fun convertToWarningNullabilityAnnotations(codebase: Codebase, filter: PackageFilter?) {
    if (filter != null) {
        // Our caller has asked for these APIs to not trigger nullness errors (only warnings) if
        // their callers make incorrect nullness assumptions (for example, calling a function on a
        // reference of nullable type). The way to communicate this to kotlinc is to mark these
        // APIs as RecentlyNullable/RecentlyNonNull
        codebase.accept(MarkPackagesAsRecent(filter))
    }
}

private fun loadFromSources(): Codebase {
    progress("Processing sources: ")

    val sources = if (options.sources.isEmpty()) {
        if (options.verbose) {
            options.stdout.println("No source files specified: recursively including all sources found in the source path (${options.sourcePath.joinToString()}})")
        }
        gatherSources(options.sourcePath)
    } else {
        options.sources
    }

    progress("Reading Codebase: ")
    val codebase = parseSources(sources, "Codebase loaded from source folders")

    progress("Analyzing API: ")

    val analyzer = ApiAnalyzer(codebase)
    analyzer.mergeExternalInclusionAnnotations()
    analyzer.computeApi()

    val filterEmit = ApiPredicate(ignoreShown = true, ignoreRemoved = false)
    val apiEmit = ApiPredicate(ignoreShown = true)
    val apiReference = ApiPredicate(ignoreShown = true)

    // Copy methods from soon-to-be-hidden parents into descendant classes, when necessary. Do
    // this before merging annotations or performing checks on the API to ensure that these methods
    // can have annotations added and are checked properly.
    progress("Insert missing stubs methods: ")
    analyzer.generateInheritedStubs(apiEmit, apiReference)

    analyzer.mergeExternalQualifierAnnotations()
    options.nullabilityAnnotationsValidator?.validateAllFrom(codebase, options.validateNullabilityFromList)
    options.nullabilityAnnotationsValidator?.report()
    analyzer.handleStripping()

    val apiLintReporter = options.reporterApiLint

    if (options.checkKotlinInterop) {
        KotlinInteropChecks(apiLintReporter).check(codebase)
    }

    // General API checks for Android APIs
    AndroidApiChecks().check(codebase)

    if (options.checkApi) {
        progress("API Lint: ")
        val localTimer = Stopwatch.createStarted()
        // See if we should provide a previous codebase to provide a delta from?
        val previousApiFile = options.checkApiBaselineApiFile
        val previous =
            when {
                previousApiFile == null -> null
                previousApiFile.path.endsWith(DOT_JAR) -> loadFromJarFile(previousApiFile)
                else -> SignatureFileLoader.load(
                    file = previousApiFile,
                    kotlinStyleNulls = options.inputKotlinStyleNulls
                )
            }
        ApiLint.check(codebase, previous, apiLintReporter)
        progress("$PROGRAM_NAME ran api-lint in ${localTimer.elapsed(SECONDS)} seconds with ${apiLintReporter.getBaselineDescription()}")
    }

    // Compute default constructors (and add missing package private constructors
    // to make stubs compilable if necessary). Do this after all the checks as
    // these are not part of the API.
    if (options.stubsDir != null || options.docStubsDir != null) {
        progress("Insert missing constructors: ")
        analyzer.addConstructors(filterEmit)
    }

    progress("Performing misc API checks: ")
    analyzer.performChecks()

    return codebase
}

/**
 * Returns a codebase initialized from the given Java or Kotlin source files, with the given
 * description. The codebase will use a project environment initialized according to the current
 * [options].
 */
internal fun parseSources(
    sources: List<File>,
    description: String,
    sourcePath: List<File> = options.sourcePath,
    classpath: List<File> = options.classpath,
    javaLanguageLevel: LanguageLevel = options.javaLanguageLevel,
    manifest: File? = options.manifest,
    currentApiLevel: Int = options.currentApiLevel + if (options.currentCodeName != null) 1 else 0
): PsiBasedCodebase {
    val projectEnvironment = createProjectEnvironment()
    val project = projectEnvironment.project

    // Push language level to PSI handler
    project.getComponent(LanguageLevelProjectExtension::class.java)?.languageLevel = javaLanguageLevel

    val joined = mutableListOf<File>()
    joined.addAll(sourcePath.mapNotNull { if (it.path.isNotBlank()) it.absoluteFile else null })
    joined.addAll(classpath.map { it.absoluteFile })

    // Add in source roots implied by the source files
    val sourceRoots = mutableListOf<File>()
    if (options.allowImplicitRoot) {
        extractRoots(sources, sourceRoots)
        joined.addAll(sourceRoots)
    }

    // Create project environment with those paths
    projectEnvironment.registerPaths(joined)

    val kotlinFiles = sources.filter { it.path.endsWith(DOT_KT) }
    val trace = KotlinLintAnalyzerFacade().analyze(kotlinFiles, joined, project)

    val rootDir = sourceRoots.firstOrNull() ?: sourcePath.firstOrNull() ?: File("").canonicalFile

    val units = Extractor.createUnitsForFiles(project, sources)
    val packageDocs = gatherHiddenPackagesFromJavaDocs(sourcePath)

    val codebase = PsiBasedCodebase(rootDir, description)
    codebase.initialize(project, units, packageDocs)
    codebase.manifest = manifest
    codebase.apiLevel = currentApiLevel
    codebase.bindingContext = trace.bindingContext
    return codebase
}

fun loadFromJarFile(apiJar: File, manifest: File? = null, preFiltered: Boolean = false): Codebase {
    val projectEnvironment = createProjectEnvironment()

    progress("Processing jar file: ")

    // Create project environment with those paths
    val project = projectEnvironment.project
    projectEnvironment.registerPaths(listOf(apiJar))

    val kotlinFiles = emptyList<File>()
    val trace = KotlinLintAnalyzerFacade().analyze(kotlinFiles, listOf(apiJar), project)

    val codebase = PsiBasedCodebase(apiJar, "Codebase loaded from $apiJar")
    codebase.initialize(project, apiJar, preFiltered)
    if (manifest != null) {
        codebase.manifest = options.manifest
    }
    val apiEmit = ApiPredicate(ignoreShown = true)
    val apiReference = ApiPredicate(ignoreShown = true)
    val analyzer = ApiAnalyzer(codebase)
    analyzer.mergeExternalInclusionAnnotations()
    analyzer.computeApi()
    analyzer.mergeExternalQualifierAnnotations()
    options.nullabilityAnnotationsValidator?.validateAllFrom(codebase, options.validateNullabilityFromList)
    options.nullabilityAnnotationsValidator?.report()
    analyzer.generateInheritedStubs(apiEmit, apiReference)
    codebase.bindingContext = trace.bindingContext
    return codebase
}

private fun loadFromApiSignatureFiles(files: List<File>, kotlinStyleNulls: Boolean? = null): Codebase {
    // Make sure all the source files have .txt extensions.
    files.forEach { file ->
        if (!file.path.endsWith(DOT_TXT)) {
                throw DriverException("Inconsistent input file types: The first file is of .$DOT_TXT, but detected different extension in ${file.path}")
        }
    }
    return SignatureFileLoader.loadFiles(files, kotlinStyleNulls)
}

private fun createProjectEnvironment(): LintCoreProjectEnvironment {
    ensurePsiFileCapacity()
    val appEnv = LintCoreApplicationEnvironment.get()
    val parentDisposable = appEnv.parentDisposable

    if (!assertionsEnabled() &&
        System.getenv(ENV_VAR_METALAVA_DUMP_ARGV) == null &&
        !isUnderTest()
    ) {
        DefaultLogger.disableStderrDumping(parentDisposable)
    }

    val environment = LintCoreProjectEnvironment.create(parentDisposable, appEnv)

    // Missing service needed in metalava but not in lint: javadoc handling
    environment.project.registerService(
        com.intellij.psi.javadoc.JavadocManager::class.java,
        com.intellij.psi.impl.source.javadoc.JavadocManagerImpl::class.java
    )
    environment.registerProjectExtensionPoint(JavadocTagInfo.EP_NAME,
        com.intellij.psi.javadoc.JavadocTagInfo::class.java)
    CoreApplicationEnvironment.registerExtensionPoint(
        Extensions.getRootArea(), CustomJavadocTagProvider.EP_NAME, CustomJavadocTagProvider::class.java
    )

    return environment
}

private fun ensurePsiFileCapacity() {
    val fileSize = System.getProperty("idea.max.intellisense.filesize")
    if (fileSize == null) {
        // Ensure we can handle large compilation units like android.R
        System.setProperty("idea.max.intellisense.filesize", "100000")
    }
}

private fun extractAnnotations(codebase: Codebase, file: File) {
    val localTimer = Stopwatch.createStarted()

    options.externalAnnotations?.let { outputFile ->
        @Suppress("UNCHECKED_CAST")
        ExtractAnnotations(
            codebase,
            outputFile
        ).extractAnnotations()
        if (options.verbose) {
            progress("$PROGRAM_NAME extracted annotations into $file in ${localTimer.elapsed(SECONDS)} seconds\n")
        }
    }
}

private fun createStubFiles(stubDir: File, codebase: Codebase, docStubs: Boolean, writeStubList: Boolean) {
    // Generating stubs from a sig-file-based codebase is problematic
    assert(codebase.supportsDocumentation())

    // Temporary bug workaround for org.chromium.arc
    if (options.sourcePath.firstOrNull()?.path?.endsWith("org.chromium.arc") == true) {
        codebase.findClass("org.chromium.mojo.bindings.Callbacks")?.hidden = true
    }

    if (docStubs) {
        progress("Generating documentation stub files: ")
    } else {
        progress("Generating stub files: ")
    }

    val localTimer = Stopwatch.createStarted()
    val prevCompatibility = compatibility
    if (compatibility.compat) {
        compatibility = Compatibility(false)
        // But preserve the setting for whether we want to erase throws signatures (to ensure the API
        // stays compatible)
        compatibility.useErasureInThrows = prevCompatibility.useErasureInThrows
    }

    val stubWriter =
        StubWriter(
            codebase = codebase,
            stubsDir = stubDir,
            generateAnnotations = options.generateAnnotations,
            preFiltered = codebase.preFiltered,
            docStubs = docStubs
        )
    codebase.accept(stubWriter)

    if (docStubs) {
        // Overview docs? These are generally in the empty package.
        codebase.findPackage("")?.let { empty ->
            val overview = codebase.getPackageDocs()?.getOverviewDocumentation(empty)
            if (overview != null && overview.isNotBlank()) {
                stubWriter.writeDocOverview(empty, overview)
            }
        }
    }

    if (writeStubList) {
        // Optionally also write out a list of source files that were generated; used
        // for example to point javadoc to the stubs output to generate documentation
        val file = if (docStubs) {
            options.docStubsSourceList ?: options.stubsSourceList
        } else {
            options.stubsSourceList
        }
        file?.let {
            val root = File("").absoluteFile
            stubWriter.writeSourceList(it, root)
        }
    }

    compatibility = prevCompatibility

    progress(
        "$PROGRAM_NAME wrote ${if (docStubs) "documentation" else ""} stubs directory $stubDir in ${
        localTimer.elapsed(SECONDS)} seconds\n"
    )
}

fun createReportFile(
    codebase: Codebase,
    apiFile: File,
    description: String?,
    createVisitor: (PrintWriter) -> ApiVisitor
) {
    if (description != null) {
        progress("Writing $description file: ")
    }
    val localTimer = Stopwatch.createStarted()
    try {
        val writer = PrintWriter(Files.asCharSink(apiFile, UTF_8).openBufferedStream())
        writer.use { printWriter ->
            val apiWriter = createVisitor(printWriter)
            codebase.accept(apiWriter)
        }
    } catch (e: IOException) {
        reporter.report(Issues.IO_ERROR, apiFile, "Cannot open file for write.")
    }
    if (description != null && options.verbose) {
        progress("$PROGRAM_NAME wrote $description file $apiFile in ${localTimer.elapsed(SECONDS)} seconds\n")
    }
}

private fun skippableDirectory(file: File): Boolean = file.path.endsWith(".git") && file.name == ".git"

private fun addSourceFiles(list: MutableList<File>, file: File) {
    if (file.isDirectory) {
        if (skippableDirectory(file)) {
            return
        }
        if (java.nio.file.Files.isSymbolicLink(file.toPath())) {
            reporter.report(
                Issues.IGNORING_SYMLINK, file,
                "Ignoring symlink during source file discovery directory traversal"
            )
            return
        }
        val files = file.listFiles()
        if (files != null) {
            for (child in files) {
                addSourceFiles(list, child)
            }
        }
    } else {
        if (file.isFile && (file.path.endsWith(DOT_JAVA) || file.path.endsWith(DOT_KT))) {
            list.add(file)
        }
    }
}

fun gatherSources(sourcePath: List<File>): List<File> {
    val sources = Lists.newArrayList<File>()
    for (file in sourcePath) {
        if (file.path.isBlank()) {
            // --source-path "" means don't search source path; use "." for pwd
            continue
        }
        addSourceFiles(sources, file.absoluteFile)
    }
    return sources.sortedWith(compareBy({ it.name }))
}

private fun addHiddenPackages(
    packageToDoc: MutableMap<String, String>,
    packageToOverview: MutableMap<String, String>,
    hiddenPackages: MutableSet<String>,
    file: File,
    pkg: String
) {
    if (FileReadSandbox.isDirectory(file)) {
        if (skippableDirectory(file)) {
            return
        }
        // Ignore symbolic links during traversal
        if (java.nio.file.Files.isSymbolicLink(file.toPath())) {
            reporter.report(
                Issues.IGNORING_SYMLINK, file,
                "Ignoring symlink during package.html discovery directory traversal"
            )
            return
        }
        val files = file.listFiles()
        if (files != null) {
            for (child in files) {
                var subPkg =
                    if (FileReadSandbox.isDirectory(child))
                        if (pkg.isEmpty())
                            child.name
                        else pkg + "." + child.name
                    else pkg

                if (subPkg.endsWith("src.main.java")) {
                    // It looks like the source path was incorrectly configured; make corrections here
                    // to ensure that we map the package.html files to the real packages.
                    subPkg = ""
                }

                addHiddenPackages(packageToDoc, packageToOverview, hiddenPackages, child, subPkg)
            }
        }
    } else if (FileReadSandbox.isFile(file)) {
        var javadoc = false
        val map = when {
            file.name == "package.html" -> {
                javadoc = true; packageToDoc
            }
            file.name == "overview.html" -> {
                packageToOverview
            }
            else -> return
        }
        var contents = Files.asCharSource(file, UTF_8).read()
        if (javadoc) {
            contents = packageHtmlToJavadoc(contents)
        }

        var realPkg = pkg
        // Sanity check the package; it's computed from the directory name
        // relative to the source path, but if the real source path isn't
        // passed in (and is instead some directory containing the source path)
        // then we compute the wrong package here. Instead, look for an adjacent
        // java class and pick the package from it
        for (sibling in file.parentFile.listFiles()) {
            if (sibling.path.endsWith(DOT_JAVA)) {
                val javaPkg = ClassName(sibling.readText()).packageName
                if (javaPkg != null) {
                    realPkg = javaPkg
                    break
                }
            }
        }

        map[realPkg] = contents
        if (contents.contains("@hide")) {
            hiddenPackages.add(realPkg)
        }
    }
}

private fun gatherHiddenPackagesFromJavaDocs(sourcePath: List<File>): PackageDocs {
    val packageComments = HashMap<String, String>(100)
    val overviewHtml = HashMap<String, String>(10)
    val hiddenPackages = HashSet<String>(100)
    for (file in sourcePath) {
        if (file.path.isBlank()) {
            // Ignoring empty paths, which means "no source path search". Use "." for current directory.
            continue
        }
        addHiddenPackages(packageComments, overviewHtml, hiddenPackages, file, "")
    }

    return PackageDocs(packageComments, overviewHtml, hiddenPackages)
}

fun extractRoots(sources: List<File>, sourceRoots: MutableList<File> = mutableListOf()): List<File> {
    // Cache for each directory since computing root for a source file is
    // expensive
    val dirToRootCache = mutableMapOf<String, File>()
    for (file in sources) {
        val parent = file.parentFile ?: continue
        val found = dirToRootCache[parent.path]
        if (found != null) {
            continue
        }

        val root = findRoot(file) ?: continue
        dirToRootCache[parent.path] = root

        if (!sourceRoots.contains(root)) {
            sourceRoots.add(root)
        }
    }

    return sourceRoots
}

/**
 * If given a full path to a Java or Kotlin source file, produces the path to
 * the source root if possible.
 */
private fun findRoot(file: File): File? {
    val path = file.path
    if (path.endsWith(DOT_JAVA) || path.endsWith(DOT_KT)) {
        val pkg = findPackage(file) ?: return null
        val parent = file.parentFile ?: return null
        val endIndex = parent.path.length - pkg.length
        val before = path[endIndex - 1]
        if (before == '/' || before == '\\') {
            return File(path.substring(0, endIndex))
        } else {
            reporter.report(
                Issues.IO_ERROR, file, "$PROGRAM_NAME was unable to determine the package name. " +
                    "This usually means that a source file was where the directory does not seem to match the package " +
                    "declaration; we expected the path $path to end with /${pkg.replace('.', '/') + '/' + file.name}"
            )
        }
    }

    return null
}

/** Finds the package of the given Java/Kotlin source file, if possible */
fun findPackage(file: File): String? {
    val source = Files.asCharSource(file, UTF_8).read()
    return findPackage(source)
}

/** Finds the package of the given Java/Kotlin source code, if possible */
fun findPackage(source: String): String? {
    return ClassName(source).packageName
}

/** Whether metalava is running unit tests */
fun isUnderTest() = java.lang.Boolean.getBoolean(ENV_VAR_METALAVA_TESTS_RUNNING)

/** Whether metalava is being invoked as part of an Android platform build */
fun isBuildingAndroid() = System.getenv("ANDROID_BUILD_TOP") != null && !isUnderTest()
