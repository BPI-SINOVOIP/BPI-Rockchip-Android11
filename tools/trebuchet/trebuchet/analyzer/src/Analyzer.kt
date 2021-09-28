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

import trebuchet.extras.openSample
import trebuchet.model.Model
import trebuchet.queries.slices.*
import trebuchet.queries.ThreadQueries
import trebuchet.util.par_map

fun timeMergeShot(model: Model) {
    println("Clock sync parent=${model.parentTimestamp}, realtime=${model.realtimeTimestamp}")
    val slices = model.slices().filter { it.name.startsWith("MergeShot") }
    slices.forEach { println("${it.name} took ${it.duration}") }
    val totalDuration = slices.map { it.duration }.reduce { a, b -> a+b }
    println("Total Duration: $totalDuration")
}


fun measureStartup(model: Model) {
    val uiThread = ThreadQueries.firstOrNull(model) {
        it.slices.any {
            it.name == "PreFork" || it.name == "activityStart"
        }
    } ?: return
    val process = uiThread.process
    val rtThread = process.threads.first { it.name == "RenderThread" }
    val start = uiThread.slices.first {
        it.name == "PreFork" || it.name == "activityStart"
    }
    val end = rtThread.slices.first {
        it.name == "DrawFrame"
    }
    val startupDuration = end.endTime - start.startTime
    println("Process ${process.name} took $startupDuration to start")
}

fun measureRotator(model: Model) {
    val latchBuffers = model.selectAll {
        it.name == "latchBuffer"
    }
    var largestDuration = 0.0
    var latchStart = 0.0
    var retireStart = 0.0
    latchBuffers.forEachIndexed { index,  slice ->
        val cutoff = if (index < latchBuffers.size - 1) latchBuffers[index + 1].startTime else Double.MAX_VALUE
        val retire = model.selectAll {
            it.name == "sde_rotator_retire_handler"
                    && it.startTime > slice.endTime
                    && it.endTime < cutoff
        }.firstOrNull()
        if (retire != null) {
            val duration = retire.endTime - slice.startTime
            if (duration > largestDuration) {
                largestDuration = duration
                latchStart = slice.startTime
                retireStart = retire.startTime
            }
        }
    }
    println("Largest duration %.2fms, occured at latchStart=%f, retireStart=%f".format(
            largestDuration.toMilliseconds(), latchStart, retireStart))
}

private fun Double.toMilliseconds() = this / 1000.0

fun main(args: Array<String>) {
    timeMergeShot(openSample("hdr-0608-4-trace.html"))
    //timeMergeShot(openSample("huge_trace.txt"))
}
