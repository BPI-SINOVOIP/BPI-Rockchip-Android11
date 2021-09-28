/*
 * Copyright (C) 2020 The Android Open Source Project
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

import com.intellij.util.execution.ParametersListUtil
import java.io.File
import java.io.IOException
import java.io.PrintWriter
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import java.util.regex.Pattern
import kotlin.random.Random

/**
 * Proprocess command line arguments.
 * 1. Prepend/append {@code ENV_VAR_METALAVA_PREPEND_ARGS} and {@code ENV_VAR_METALAVA_PREPEND_ARGS}
 * 2. Reflect --verbose to {@link options#verbose}.
 */
internal fun preprocessArgv(args: Array<String>): Array<String> {
    val modifiedArgs =
        if (args.isEmpty()) {
            arrayOf("--help")
        } else if (!isUnderTest()) {
            val prepend = envVarToArgs(ENV_VAR_METALAVA_PREPEND_ARGS)
            val append = envVarToArgs(ENV_VAR_METALAVA_APPEND_ARGS)
            if (prepend.isEmpty() && append.isEmpty()) {
                args
            } else {
                val index = args.indexOf(ARG_GENERATE_DOCUMENTATION)
                val newArgs =
                    if (index != -1) {
                        args.sliceArray(0 until index) + prepend +
                            args.sliceArray(index until args.size) + append
                    } else {
                        prepend + args + append
                    }
                newArgs
            }
        } else {
            args
        }

    // We want to enable verbose log as soon as possible, so we cheat here and try to detect
    // --verbose and --quiet.
    // (Note this logic could generate results different from what Options.kt would generate,
    // for example when "--verbose" is used as a flag value. But that's not a practical problem....)
    modifiedArgs.forEach { arg ->
        when (arg) {
            ARG_QUIET -> {
                options.quiet = true; options.verbose = false
            }

            ARG_VERBOSE -> {
                options.verbose = true; options.quiet = false
            }
        }
    }

    return modifiedArgs
}

/**
 * Given an environment variable name pointing to a shell argument string,
 * returns the parsed argument strings (or empty array if not set)
 */
private fun envVarToArgs(varName: String): Array<String> {
    val value = System.getenv(varName) ?: return emptyArray()
    return ParametersListUtil.parse(value).toTypedArray()
}

/**
 * If the {@link ENV_VAR_METALAVA_DUMP_ARGV} environmental variable is set, dump the passed
 * arguments.
 *
 * If the variable is set to"full", also dump the content of the file
 * specified with "@".
 *
 * If the variable is set to "script", it'll generate a "rerun" script instead.
 */
internal fun maybeDumpArgv(
    out: PrintWriter,
    originalArgs: Array<String>,
    modifiedArgs: Array<String>
) {
    val dumpOption = System.getenv(ENV_VAR_METALAVA_DUMP_ARGV)
    if (dumpOption == null || isUnderTest()) {
        return
    }

    // Generate a rerun script, if needed, with the original args.
    if ("script".equals(dumpOption)) {
        generateRerunScript(out, originalArgs)
    }

    val fullDump = "full".equals(dumpOption) // Dump rsp file contents too?

    dumpArgv(out, "Original args", originalArgs, fullDump)
    dumpArgv(out, "Modified args", modifiedArgs, fullDump)
}

private fun dumpArgv(
    out: PrintWriter,
    description: String,
    args: Array<String>,
    fullDump: Boolean
) {
    out.println("== $description ==")
    out.println("  pwd: ${File("").absolutePath}")
    val sep = Pattern.compile("""\s+""")
    var i = 0
    args.forEach { arg ->
        out.println("  argv[${i++}]: $arg")

        // Optionally dump the content of an "@" file.
        if (fullDump && arg.startsWith("@")) {
            val file = arg.substring(1)
            out.println("    ==FILE CONTENT==")
            try {
                File(file).bufferedReader().forEachLine { line ->
                    line.split(sep).forEach { item ->
                        out.println("    | $item")
                    }
                }
            } catch (e: IOException) {
                out.println("  " + e.message)
            }
            out.println("    ================")
        }
    }

    out.flush()
}

/** Generate a rerun script file name minus the extension. */
private fun createRerunScriptBaseFilename(): String {
    val timestamp = LocalDateTime.now().format(
        DateTimeFormatter.ofPattern("yyyy-MM-dd_HH-mm-ss.SSS"))

    val uniqueInt = Random.nextInt(0, Int.MAX_VALUE)
    val dir = System.getenv("METALAVA_TEMP") ?: System.getenv("TMP") ?: System.getenv("TEMP") ?: "/tmp"
    val file = "$PROGRAM_NAME-rerun-${timestamp}_$uniqueInt" // no extension

    return dir + File.separator + file
}

/**
 * Generate a rerun script, if specified by the command line arguments.
 */
private fun generateRerunScript(stdout: PrintWriter, args: Array<String>) {
    val scriptBaseName = createRerunScriptBaseFilename()

    // Java runtime executable.
    val java = System.getProperty("java.home") + "/bin/java"
    // Metalava's jar path.
    val jar = ApiLint::class.java.protectionDomain?.codeSource?.location?.toURI()?.path
    // JVM options.
    val jvmOptions = ManagementWrapper.getVmArguments()
    if (jvmOptions == null || jar == null) {
        stdout.println("$PROGRAM_NAME unable to get my jar file path.")
        return
    }

    val script = File(scriptBaseName + ".sh")
    var optFileIndex = 0
    script.printWriter().use { out ->
        out.println("""
            |#!/bin/sh
            |#
            |# Auto-generated $PROGRAM_NAME rerun script
            |#
            |
            |# Exit on failure
            |set -e
            |
            |cd ${shellEscape(File("").absolutePath)}
            |
            |export $ENV_VAR_METALAVA_DUMP_ARGV=1
            |
            |# Overwrite JVM options with ${"$"}METALAVA_JVM_OPTS, if available
            |jvm_opts=(${"$"}METALAVA_JVM_OPTS)
            |
            |if [ ${"$"}{#jvm_opts[@]} -eq 0 ] ; then
            """.trimMargin())

        jvmOptions.forEach {
            out.println("""    jvm_opts+=(${shellEscape(it)})""")
        }

        out.println("""
            |fi
            |
            |${"$"}METALAVA_RUN_PREFIX $java "${"$"}{jvm_opts[@]}" \
            """.trimMargin())
        out.println("""    -jar $jar \""")

        // Write the actual metalava options
        args.forEach {
            if (!it.startsWith("@")) {
                out.print(if (it.startsWith("-")) "    " else "        ")
                out.print(shellEscape(it))
                out.println(" \\")
            } else {
                val optFile = "${scriptBaseName}_arg_${++optFileIndex}.txt"
                File(it.substring(1)).copyTo(File(optFile), true)
                out.println("""    @$optFile \""")
            }
        }
    }

    // Show the filename.
    stdout.println("Generated rerun script: $script")
    stdout.flush()
}

/** Characters that need escaping in shell scripts. */
private val SHELL_UNSAFE_CHARS by lazy { Pattern.compile("""[^\-a-zA-Z0-9_/.,:=@+]""") }

/** Escape a string as a single word for shell scripts. */
private fun shellEscape(s: String): String {
    if (!SHELL_UNSAFE_CHARS.matcher(s).find()) {
        return s
    }
    // Just wrap a string in ' ... ', except of it contains a ', it needs to be changed to
    // '\''.
    return "'" + s.replace("""'""", """'\''""") + "'"
}