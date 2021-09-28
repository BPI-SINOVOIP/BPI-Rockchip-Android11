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

package com.android.tools.metalava

import com.android.SdkConstants.ATTR_VALUE
import com.android.tools.metalava.Severity.ERROR
import com.android.tools.metalava.Severity.HIDDEN
import com.android.tools.metalava.Severity.INFO
import com.android.tools.metalava.Severity.INHERIT
import com.android.tools.metalava.Severity.LINT
import com.android.tools.metalava.Severity.WARNING
import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.AnnotationArrayAttributeValue
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.configuration
import com.android.tools.metalava.model.psi.PsiItem
import com.android.tools.metalava.model.text.TextItem
import com.google.common.annotations.VisibleForTesting
import com.intellij.openapi.util.TextRange
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.psi.PsiCompiledElement
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiModifierListOwner
import com.intellij.psi.impl.light.LightElement
import java.io.File
import java.io.PrintWriter

/**
 * "Global" [Reporter] used by most operations.
 * Certain operations, such as api-lint and compatibility check, may use a custom [Reporter]
 */
lateinit var reporter: Reporter

enum class Severity(private val displayName: String) {
    INHERIT("inherit"),

    HIDDEN("hidden"),

    /**
     * Information level are for issues that are informational only; may or
     * may not be a problem.
     */
    INFO("info"),

    /**
     * Lint level means that we encountered inconsistent or broken documentation.
     * These should be resolved, but don't impact API compatibility.
     */
    LINT("lint"),

    /**
     * Warning level means that we encountered some incompatible or inconsistent
     * API change. These must be resolved to preserve API compatibility.
     */
    WARNING("warning"),

    /**
     * Error level means that we encountered severe trouble and were unable to
     * output the requested documentation.
     */
    ERROR("error");

    override fun toString(): String = displayName
}

class Reporter(
    /** [Baseline] file associated with this [Reporter]. If null, the global baseline is used. */
    // See the comment on [getBaseline] for why it's nullable.
    private val customBaseline: Baseline?,

    /**
     * An error message associated with this [Reporter], which should be shown to the user
     * when metalava finishes with errors.
     */
    private val errorMessage: String?
) {
    var errorCount = 0
        private set
    var warningCount = 0
        private set
    val totalCount get() = errorCount + warningCount

    private var hasErrors = false

    // Note we can't set [options.baseline] as the default for [customBaseline], because
    // options.baseline will be initialized after the global [Reporter] is instantiated.
    fun getBaseline(): Baseline? = customBaseline ?: options.baseline

    fun report(id: Issues.Issue, element: PsiElement?, message: String): Boolean {
        val severity = configuration.getSeverity(id)

        if (severity == HIDDEN) {
            return false
        }

        val baseline = getBaseline()
        if (element != null && baseline != null && baseline.mark(element, message, id)) {
            return false
        }

        return report(severity, elementToLocation(element), message, id)
    }

    fun report(id: Issues.Issue, file: File?, message: String): Boolean {
        val severity = configuration.getSeverity(id)

        if (severity == HIDDEN) {
            return false
        }

        val baseline = getBaseline()
        if (file != null && baseline != null && baseline.mark(file, message, id)) {
            return false
        }

        return report(severity, file?.path, message, id)
    }

    fun report(id: Issues.Issue, item: Item?, message: String, psi: PsiElement? = null): Boolean {
        val severity = configuration.getSeverity(id)
        if (severity == HIDDEN) {
            return false
        }

        fun dispatch(
            which: (severity: Severity, location: String?, message: String, id: Issues.Issue) -> Boolean
        ) = when {
            psi != null -> which(severity, elementToLocation(psi), message, id)
            item is PsiItem -> which(severity, elementToLocation(item.psi()), message, id)
            item is TextItem ->
                which(severity, (item as? TextItem)?.position.toString(), message, id)
            else -> which(severity, null as String?, message, id)
        }

        // Optionally write to the --report-even-if-suppressed file.
        dispatch(this::reportEvenIfSuppressed)

        if (isSuppressed(id, item, message)) {
            return false
        }

        // If we are only emitting some packages (--stub-packages), don't report
        // issues from other packages
        if (item != null) {
            val packageFilter = options.stubPackages
            if (packageFilter != null) {
                val pkg = item.containingPackage(false)
                if (pkg != null && !packageFilter.matches(pkg)) {
                    return false
                }
            }
        }

        val baseline = getBaseline()
        if (item != null && baseline != null && baseline.mark(item, message, id)) {
            return false
        } else if (psi != null && baseline != null && baseline.mark(psi, message, id)) {
            return false
        }

        return dispatch(this::doReport)
    }

    fun isSuppressed(id: Issues.Issue, item: Item? = null, message: String? = null): Boolean {
        val severity = configuration.getSeverity(id)
        if (severity == HIDDEN) {
            return true
        }

        item ?: return false

        if (severity == LINT || severity == WARNING || severity == ERROR) {
            val annotation = item.modifiers.findAnnotation("android.annotation.SuppressLint")
            if (annotation != null) {
                val attribute = annotation.findAttribute(ATTR_VALUE)
                if (attribute != null) {
                    val id1 = "Doclava${id.code}"
                    val id2 = id.name
                    val value = attribute.value
                    if (value is AnnotationArrayAttributeValue) {
                        // Example: @SuppressLint({"DocLava1", "DocLava2"})
                        for (innerValue in value.values) {
                            val string = innerValue.value()?.toString() ?: continue
                            if (suppressMatches(string, id1, message) || suppressMatches(string, id2, message)) {
                                return true
                            }
                        }
                    } else {
                        // Example: @SuppressLint("DocLava1")
                        val string = value.value()?.toString()
                        if (string != null && (
                                suppressMatches(string, id1, message) || suppressMatches(string, id2, message))
                        ) {
                            return true
                        }
                    }
                }
            }
        }

        return false
    }

    private fun suppressMatches(value: String, id: String?, message: String?): Boolean {
        id ?: return false

        if (value == id) {
            return true
        }

        if (message != null && value.startsWith(id) && value.endsWith(message) &&
            (value == "$id:$message" || value == "$id: $message")
        ) {
            return true
        }

        return false
    }

    private fun getTextRange(element: PsiElement): TextRange? {
        var range: TextRange? = null

        if (element is PsiCompiledElement) {
            if (element is LightElement) {
                range = (element as PsiElement).textRange
            }
            if (range == null || TextRange.EMPTY_RANGE == range) {
                return null
            }
        } else {
            range = element.textRange
        }

        return range
    }

    fun elementToLocation(element: PsiElement?, includeDocs: Boolean = true): String? {
        element ?: return null
        val psiFile = element.containingFile ?: return null
        val virtualFile = psiFile.virtualFile ?: return null
        val file = VfsUtilCore.virtualToIoFile(virtualFile)

        val path = (rootFolder?.toPath()?.relativize(file.toPath()) ?: file.toPath()).toString()

        // Skip doc comments for classes, methods and fields; we usually want to point right to
        // the class/method/field definition
        val rangeElement = if (!includeDocs && element is PsiModifierListOwner) {
            element.modifierList ?: element
        } else
            element

        val range = getTextRange(rangeElement)
        val lineNumber = if (range == null) {
            // No source offsets, use invalid line number
            -1
        } else {
            getLineNumber(psiFile.text, range.startOffset) + 1
        }
        return if (lineNumber > 0) "$path:$lineNumber" else path
    }

    /** Returns the 0-based line number of character position <offset> in <text> */
    private fun getLineNumber(text: String, offset: Int): Int {
        var line = 0
        var curr = 0
        val target = Math.min(offset, text.length)
        while (curr < target) {
            if (text[curr++] == '\n') {
                line++
            }
        }
        return line
    }

    /** Alias to allow method reference in [report.dispatch] */
    private fun doReport(severity: Severity, location: String?, message: String, id: Issues.Issue?) =
        report(severity, location, message, id)

    fun report(
        severity: Severity,
        location: String?,
        message: String,
        id: Issues.Issue? = null,
        color: Boolean = options.color
    ): Boolean {
        if (severity == HIDDEN) {
            return false
        }

        val effectiveSeverity =
            if (severity == LINT && options.lintsAreErrors)
                ERROR
            else if (severity == WARNING && options.warningsAreErrors) {
                ERROR
            } else {
                severity
            }

        if (effectiveSeverity == ERROR) {
            hasErrors = true
            errorCount++
        } else if (severity == WARNING) {
            warningCount++
        }

        reportPrinter(
            format(effectiveSeverity, location, message, id, color, options.omitLocations),
            effectiveSeverity
        )
        return true
    }

    private fun format(
        severity: Severity,
        location: String?,
        message: String,
        id: Issues.Issue?,
        color: Boolean,
        omitLocations: Boolean
    ): String {
        val sb = StringBuilder(100)

        if (color && !isUnderTest()) {
            sb.append(terminalAttributes(bold = true))
            if (!omitLocations) {
                location?.let {
                    sb.append(it).append(": ")
                }
            }
            when (severity) {
                LINT -> sb.append(terminalAttributes(foreground = TerminalColor.CYAN)).append("lint: ")
                INFO -> sb.append(terminalAttributes(foreground = TerminalColor.CYAN)).append("info: ")
                WARNING -> sb.append(terminalAttributes(foreground = TerminalColor.YELLOW)).append("warning: ")
                ERROR -> sb.append(terminalAttributes(foreground = TerminalColor.RED)).append("error: ")
                INHERIT, HIDDEN -> {
                }
            }
            sb.append(resetTerminal())
            sb.append(message)
            id?.let {
                sb.append(" [").append(it.name).append("]")
            }
        } else {
            if (!omitLocations) {
                location?.let { sb.append(it).append(": ") }
            }
            if (compatibility.oldErrorOutputFormat) {
                // according to doclava1 there are some people or tools parsing old format
                when (severity) {
                    LINT -> sb.append("lint ")
                    INFO -> sb.append("info ")
                    WARNING -> sb.append("warning ")
                    ERROR -> sb.append("error ")
                    INHERIT, HIDDEN -> {
                    }
                }
                id?.let { sb.append(it.name).append(": ") }
                sb.append(message)
            } else {
                when (severity) {
                    LINT -> sb.append("lint: ")
                    INFO -> sb.append("info: ")
                    WARNING -> sb.append("warning: ")
                    ERROR -> sb.append("error: ")
                    INHERIT, HIDDEN -> {
                    }
                }
                sb.append(message)
                id?.let {
                    sb.append(" [")
                    sb.append(it.name)
                    if (compatibility.includeExitCode) {
                        sb.append(":")
                        sb.append(it.code)
                    }
                    sb.append("]")
                    if (it.rule != null) {
                        sb.append(" [Rule ").append(it.rule)
                        val link = it.category.ruleLink
                        if (link != null) {
                            sb.append(" in ").append(link)
                        }
                        sb.append("]")
                    }
                }
            }
        }
        return sb.toString()
    }

    private fun reportEvenIfSuppressed(
        severity: Severity,
        location: String?,
        message: String,
        id: Issues.Issue
    ): Boolean {
        options.reportEvenIfSuppressedWriter?.println(
            format(
                severity,
                location,
                message,
                id,
                color = false,
                omitLocations = false
            ))
        return true
    }

    fun hasErrors(): Boolean = hasErrors

    /** Write the error message set to this [Reporter], if any errors have been detected. */
    fun writeErrorMessage(writer: PrintWriter) {
        if (hasErrors()) {
            errorMessage ?. let { writer.write(it) }
        }
    }

    fun getBaselineDescription(): String {
        val file = getBaseline()?.file
        return if (file != null) {
            "baseline ${file.path}"
        } else {
            "no baseline"
        }
    }

    companion object {
        /** root folder, which needs to be changed for unit tests. */
        @VisibleForTesting
        internal var rootFolder: File? = File("").absoluteFile

        /** Injection point for unit tests. */
        internal var reportPrinter: (String, Severity) -> Unit = { message, severity ->
            val output = if (severity == ERROR) {
                options.stderr
            } else {
                options.stdout
            }
            output.println()
            output.print(message.trim())
            output.flush()
        }
    }
}
