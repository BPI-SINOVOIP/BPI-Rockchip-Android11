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

package com.android.tools.metalava

import com.android.ide.common.process.CachedProcessOutputHandler
import com.android.ide.common.process.DefaultProcessExecutor
import com.android.ide.common.process.ProcessInfoBuilder
import com.android.utils.LineCollector
import com.android.utils.StdLogger
import java.io.File

// Copied from lint's test suite: TestUtils.diff in tools/base with
// some changes to allow running native diff.

/** Returns the universal diff for the two given files, or null if not supported or working */
fun getNativeDiff(before: File, after: File): String? {
    if (options.noNativeDiff) {
        return null
    }

    // A lot faster for big files:
    val native = File("/usr/bin/diff")
    if (native.isFile) {
        try {
            val builder = ProcessInfoBuilder()
            builder.setExecutable(native)
            builder.addArgs("-u", before.path, after.path)
            val processOutputHandler = CachedProcessOutputHandler()
            DefaultProcessExecutor(StdLogger(StdLogger.Level.ERROR))
                .execute(builder.createProcess(), processOutputHandler)
                .rethrowFailure()

            val output = processOutputHandler.processOutput
            val lineCollector = LineCollector()
            output.processStandardOutputLines(lineCollector)

            return lineCollector.result.joinToString(separator = "\n")
        } catch (ignore: Throwable) {
        }
    }

    return null
}

fun getDiff(before: String, after: String, windowSize: Int): String {
    return getDiff(
        before.split("\n".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray(),
        after.split("\n".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray(),
        windowSize
    )
}

fun getDiff(
    before: Array<String>,
    after: Array<String>,
    windowSize: Int
): String {
    // Based on the LCS section in http://introcs.cs.princeton.edu/java/96optimization/
    val sb = StringBuilder()
    val n = before.size
    val m = after.size

    // Compute longest common subsequence of x[i..m] and y[j..n] bottom up
    val lcs = Array(n + 1) { IntArray(m + 1) }
    for (i in n - 1 downTo 0) {
        for (j in m - 1 downTo 0) {
            if (before[i] == after[j]) {
                lcs[i][j] = lcs[i + 1][j + 1] + 1
            } else {
                lcs[i][j] = Math.max(lcs[i + 1][j], lcs[i][j + 1])
            }
        }
    }

    var i = 0
    var j = 0
    while (i < n && j < m) {
        if (before[i] == after[j]) {
            i++
            j++
        } else {
            sb.append("@@ -")
            sb.append(Integer.toString(i + 1))
            sb.append(" +")
            sb.append(Integer.toString(j + 1))
            sb.append('\n')

            if (windowSize > 0) {
                for (context in Math.max(0, i - windowSize) until i) {
                    sb.append("  ")
                    sb.append(before[context])
                    sb.append("\n")
                }
            }

            while (i < n && j < m && before[i] != after[j]) {
                if (lcs[i + 1][j] >= lcs[i][j + 1]) {
                    sb.append('-')
                    if (!before[i].trim { it <= ' ' }.isEmpty()) {
                        sb.append(' ')
                    }
                    sb.append(before[i])
                    sb.append('\n')
                    i++
                } else {
                    sb.append('+')
                    if (!after[j].trim { it <= ' ' }.isEmpty()) {
                        sb.append(' ')
                    }
                    sb.append(after[j])
                    sb.append('\n')
                    j++
                }
            }

            if (windowSize > 0) {
                for (context in i until Math.min(n, i + windowSize)) {
                    sb.append("  ")
                    sb.append(before[context])
                    sb.append("\n")
                }
            }
        }
    }

    if (i < n || j < m) {
        assert(i == n || j == m)
        sb.append("@@ -")
        sb.append(Integer.toString(i + 1))
        sb.append(" +")
        sb.append(Integer.toString(j + 1))
        sb.append('\n')
        while (i < n) {
            sb.append('-')
            if (!before[i].trim { it <= ' ' }.isEmpty()) {
                sb.append(' ')
            }
            sb.append(before[i])
            sb.append('\n')
            i++
        }
        while (j < m) {
            sb.append('+')
            if (!after[j].trim { it <= ' ' }.isEmpty()) {
                sb.append(' ')
            }
            sb.append(after[j])
            sb.append('\n')
            j++
        }
    }

    return sb.toString()
}
