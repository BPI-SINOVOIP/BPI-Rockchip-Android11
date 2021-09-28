/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package trebuchet.extras

import trebuchet.model.Model
import trebuchet.task.ImportTask
import trebuchet.util.PrintlnImportFeedback
import java.io.File

fun Double.format(digits: Int) = String.format("%.${digits}f", this)

fun reportImportProgress(read: Long, total: Long) {
    if (total > 0) {
        val progress = read.toDouble() / total.toDouble() * 100.0
        print("\rProgress: ${progress.format(2)}%")
        if (progress >= 100) { print("\n") }
    }
}

fun parseTrace(file: File, verbose : Boolean = true): Model {
    val before = System.nanoTime()
    val task = ImportTask(PrintlnImportFeedback())
    val model = task.import(InputStreamAdapter(file, if (verbose) ::reportImportProgress else null))
    val after = System.nanoTime()

    if (verbose) {
        val duration = (after - before) / 1000000
        println("Parsing ${file.name} took ${duration}ms")
    }

    return model
}

fun findSampleData(): String {
    var dir = File(".").absoluteFile
    while (!File(dir, "sample_data").exists()) {
        dir = dir.parentFile
        if (dir == null) {
            throw IllegalStateException("Can't find sample_data")
        }
    }
    return File(dir, "sample_data").absolutePath
}

fun openSample(name: String): Model {
    return parseTrace(File(findSampleData(), name))
}