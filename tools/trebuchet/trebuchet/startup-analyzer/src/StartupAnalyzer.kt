/*
 * Copyright 2018 Google Inc.
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
 * TODO (chriswailes): Support JSON output
 */

/*
 * Imports
 */

import java.io.File
import trebuchet.model.Model
import trebuchet.extras.parseTrace
import trebuchet.util.time.*

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

fun measureStartup(model: Model) {
    val startupEvents = model.getStartupEvents()

    println("Found ${startupEvents.count()} startup events.")

    startupEvents.forEach { startupEvent ->
        println()
        println("App Startup summary for ${startupEvent.name} (${startupEvent.proc.id}):")
        println("\tStart offset:              ${(startupEvent.startTime - model.beginTimestamp).secondValueToMillisecondString()}")
        println("\tLaunch duration:           ${(startupEvent.endTime - startupEvent.startTime).secondValueToMillisecondString()}")

        if (startupEvent.reportFullyDrawnTime == null) {
            println("\tRFD duration:              N/A")
        } else {
            println("\tRFD duration:              ${(startupEvent.reportFullyDrawnTime!! - startupEvent.startTime).secondValueToMillisecondString()}")
        }

        println("\tServer fork time:          ${startupEvent.serverSideForkTime.secondValueToMillisecondString()}")
        println("\tTime to first slice:       ${(startupEvent.firstSliceTime - startupEvent.startTime).secondValueToMillisecondString()}")
        println("\tUndifferentiated time:     ${startupEvent.undifferentiatedTime.secondValueToMillisecondString()}")
        println()
        println("\tScheduling timings:")
        startupEvent.schedTimings.toSortedMap().forEach { schedState, timing ->
            println("\t\t${schedState.friendlyName}: ${timing.secondValueToMillisecondString()}")
        }
        println()
        println("\tTop-level slice information:")
        startupEvent.topLevelSliceInfo.toSortedMap(java.lang.String.CASE_INSENSITIVE_ORDER).forEach { sliceName, aggInfo ->
            println("\t\t$sliceName")
            println("\t\t\tEvent count:    ${aggInfo.count}")
            println("\t\t\tTotal duration: ${aggInfo.totalTime.secondValueToMillisecondString()}")
        }
        println()
        println("\tAll slice information:")
        startupEvent.allSlicesInfo.toSortedMap(java.lang.String.CASE_INSENSITIVE_ORDER).forEach { sliceName, aggInfo->
            println("\t\t$sliceName")
            println("\t\t\tEvent count:           ${aggInfo.count}")

            println("\t\t\tTotal duration:        ${aggInfo.totalTime.secondValueToMillisecondString()}")

            if (aggInfo.values.count() > 0) {
                println("\t\t\tEvent details:")
                aggInfo.values.toSortedMap(java.lang.String.CASE_INSENSITIVE_ORDER).forEach { value, (count, duration) ->
                    println("\t\t\t\t$value ${if (count > 1) "x $count " else ""}@ ${duration.secondValueToMillisecondString()}")
                }
            }
        }
        println()
    }
}

/*
 * Main Function
 */

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: StartAnalyzerKt <trace filename>")
        return
    }

    val filename = args[0]

    println("Opening `$filename`")
    val trace = parseTrace(File(filename), verbose = false)

    measureStartup(trace)
}