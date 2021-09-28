/*
 * Copyright 2020 Google Inc.
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

fun Double.durationString(): String {
    return "%.3f ms".format(this * 1000)
}

fun measureServiceStartup(
    model: Model,
    userId: Int = 0,
    count: Int = 0,
    output: PrintStream = System.out) {

    var stages = arrayOf("Start", "Switch", "Unlock")

    for (item in stages) {
        model.slices().filter {
            it.name.contains("ssm." + item + "User-" + userId) && !it.name.contains("ssm.on")
        }.forEach {
            output.println(item + "User-" + userId + " duration: " + it.duration.durationString())
        }

        model.slices().filter {
            it.name.contains("ssm.on" + item + "User-" + userId)
        }.sortedByDescending {
            it.duration
        }.take(count).forEach {
            output.println(it.name.removePrefix("ssm.on" + item + "User-" + userId + " ") + " " + it.duration.durationString())
        }
    }
}

/*
 * Main Function
 */

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: UserSwitchAnalyzerKt <trace_filename> [-u user_Id] [-o output_filename] [-c service_count]")
        return
    }

    val input = args[0]

    println("Opening ${input}")
    val trace = parseTrace(File(input), verbose = true)

    var output = System.out
    var userId = -1;
    var serviceCount = 5;

    // Parse optional arguments
    var nextArg = 1
    while (nextArg < args.size) {
        var arg = args[nextArg++]
        var value = args[nextArg++]
        when (arg) {
            "-u" -> userId = value.toInt()
            "-c" -> serviceCount = value.toInt()
            "-o" -> {
                output = PrintStream(File(value).outputStream())
            }
            else -> println("invalid option: ${arg}")
        }
    }

    if (userId == -1) {
        println("user Id not provided")
        return
    }

    measureServiceStartup(trace, userId, serviceCount, output)
}
