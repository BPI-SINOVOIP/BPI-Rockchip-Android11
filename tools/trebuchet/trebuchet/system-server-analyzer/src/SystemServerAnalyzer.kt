/*
 * Copyright 2019 Google Inc.
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

/*
 * Notes
 *
 * TODO (felipeal): generate .csv file
 * TODO (felipeal): automatically generate atrace / restart system_server
 * TODO (felipeal): add mre command-line options, like threshold value
 */

/*
 * Imports
 */

import java.io.File
import trebuchet.model.Model
import trebuchet.extras.parseTrace
import trebuchet.model.base.Slice
import trebuchet.model.base.SliceGroup
import trebuchet.queries.slices.slices
import java.io.PrintStream

/*
 * Constants
 */

/*
 * Class Definition
 */

/*
 * Class Extensions
 */

/*
 * Helper Functions
 */

inline fun <reified T> Slice?.ifGroup(func: (SliceGroup) -> T): T? {
    if (this is SliceGroup) {
        return func(this)
    }
    return null
}

fun Double.durationString(): String {
    return "%.3f ms".format(this * 1000)
}

fun printLine(output: PrintStream, name: String, duration: Double, csvFormat: Boolean = false) {
    if (csvFormat) {
        output.println("${name};${duration * 1000}")
    } else {
        output.println("${name}: ${duration.durationString()}")
    }
}

fun printLine(output: PrintStream, model: SliceGroup, csvFormat: Boolean = false) {
    printLine(output, model.name, model.duration, csvFormat)
}

fun measureServiceStartup(model: Model, thresholdMs: Int = 0, output: PrintStream = System.out, csvFormat: Boolean = false, otherName: String = "Other") {
    model.slices().firstOrNull {
        it.name == "StartServices" && it is SliceGroup
    }.ifGroup {
        printLine(output, "StartServices", it.duration, csvFormat);
        val childDurations = it.children.sumByDouble { service ->
            if (service.duration * 1000 >= thresholdMs) {
                printLine(output, service, csvFormat)
                service.duration
            } else {
                0.0
            }
        }
        printLine(output, "Other", it.duration - childDurations, csvFormat);
    }
}

/*
 * Main Function
 */

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: SystemServerAnalyzerKt <trace_filename> [-t threshold_ms] [-o output_filename]")
        return
    }

    val input = args[0]

    println("Opening ${input}")
    val trace = parseTrace(File(input), verbose = true)

    var csvFormat = false
    var output = System.out
    var thresholdMs = 5;

    // Parse optional arguments
    var nextArg = 1
    while (nextArg < args.size) {
        var arg = args[nextArg++]
        var value = args[nextArg++]
        when (arg) {
            "-t" -> thresholdMs = value.toInt()
            "-o" -> {
                output = PrintStream(File(value).outputStream())
                csvFormat = true
                println("Writing CSV output to ${value}")
            }
            else -> println("invalid option: ${arg}")
        }
    }

    measureServiceStartup(trace, thresholdMs, output, csvFormat)
}