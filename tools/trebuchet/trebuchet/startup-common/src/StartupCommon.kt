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
 * TODO (chriswailes): Add an argument parser
 * TODO (chriswailes): Move slice name parsing into slice class (if approved by jreck)
 */

/*
 * Imports
 */

import trebuchet.model.Model
import trebuchet.model.ProcessModel
import trebuchet.model.SchedulingState
import trebuchet.model.base.Slice
import trebuchet.queries.slices.*
import trebuchet.util.slices.*
import trebuchet.util.time.*

/*
 * Constants
 */

// Duration (in milliseconds) used for startup period when none of the
// appropriate events could be found.
const val DEFAULT_START_DURATION = 5000

const val PROC_NAME_SYSTEM_SERVER = "system_server"

/*
 * Class Definition
 */

data class StartupEvent(val proc : ProcessModel,
                        val name : String,
                        val startTime : Double,
                        val endTime : Double,
                        val serverSideForkTime : Double,
                        val reportFullyDrawnTime : Double?,
                        val firstSliceTime : Double,
                        val undifferentiatedTime : Double,
                        val schedTimings : Map<SchedulingState, Double>,
                        val allSlicesInfo : AggregateSliceInfoMap,
                        val topLevelSliceInfo : AggregateSliceInfoMap,
                        val undifferentiatedSliceInfo : AggregateSliceInfoMap,
                        val nonNestedSliceInfo : AggregateSliceInfoMap)

class AggregateSliceInfo {
    var count : Int = 0
    var totalTime : Double = 0.0

    val values : MutableMap<String, Pair<Int, Double>> = mutableMapOf()
}

typealias MutableAggregateSliceInfoMap = MutableMap<String, AggregateSliceInfo>
typealias AggregateSliceInfoMap = Map<String, AggregateSliceInfo>

class MissingProcessInfoException(pid : Int) : Exception("Missing process info for PID $pid")

class MissingProcessException : Exception {
    constructor(name : String) {
        Exception("Unable to find process: $name")
    }

    constructor(name : String, lowerBound : Double, upperBound : Double) {
        Exception("Unable to find process: $name" +
                  " (Bounds: [${lowerBound.secondValueToMillisecondString()}," +
                  " ${upperBound.secondValueToMillisecondString()}]")
    }
}

/*
 * Class Extensions
 */

fun ProcessModel.fuzzyNameMatch(queryName : String) : Boolean {
    if (queryName.endsWith(this.name)) {
        return true
    } else {
        for (thread in this.threads) {
            if (queryName.endsWith(thread.name)) {
                return true
            }
        }
    }

    return false
}

fun Model.findProcess(queryName: String,
                      lowerBound : Double = this.beginTimestamp,
                      upperBound : Double = this.endTimestamp): ProcessModel {

    for (process in this.processes.values) {
        if (process.fuzzyNameMatch(queryName)) {
            val firstSliceStart =
                process.
                    threads.
                    map { it.slices }.
                    filter { it.isNotEmpty() }.
                    map { it.first().startTime }.
                    min() ?: throw MissingProcessInfoException(process.id)

            if (firstSliceStart in lowerBound..upperBound) {
                return process
            }
        }
    }

    if (lowerBound == this.beginTimestamp && upperBound == this.endTimestamp) {
        throw MissingProcessException(queryName)
    } else {
        throw MissingProcessException(queryName, lowerBound, upperBound)
    }
}

fun Model.getStartupEvents() : List<StartupEvent> {
    val systemServerProc = this.findProcess(PROC_NAME_SYSTEM_SERVER)

    val startupEvents = mutableListOf<StartupEvent>()

    systemServerProc.asyncSlices.forEach { systemServerSlice ->
        if (systemServerSlice.name.startsWith(SLICE_NAME_APP_LAUNCH)) {

            val newProcName    = systemServerSlice.name.split(':', limit = 2)[1].trim()
            val newProc        = this.findProcess(newProcName, systemServerSlice.startTime, systemServerSlice.endTime)
            val startProcSlice = systemServerProc.findFirstSlice(SLICE_NAME_PROC_START, newProcName, systemServerSlice.startTime, systemServerSlice.endTime)
            val rfdSlice       = systemServerProc.findFirstSliceOrNull(SLICE_NAME_REPORT_FULLY_DRAWN, newProcName, systemServerSlice.startTime)
            val firstSliceTime = newProc.threads.map { it.slices.firstOrNull()?.startTime ?: Double.POSITIVE_INFINITY }.min()!!

            val schedSliceInfo : MutableMap<SchedulingState, Double> = mutableMapOf()
            newProc.threads.first().schedSlices.forEach schedLoop@ { schedSlice ->
                val origVal = schedSliceInfo.getOrDefault(schedSlice.state, 0.0)

                when {
                    schedSlice.startTime >= systemServerSlice.endTime -> return@schedLoop
                    schedSlice.endTime <= systemServerSlice.endTime   -> schedSliceInfo[schedSlice.state] = origVal + schedSlice.duration
                    else                                              -> {
                        schedSliceInfo[schedSlice.state] = origVal + (systemServerSlice.endTime - schedSlice.startTime)
                        return@schedLoop
                    }
                }
            }

            var undifferentiatedTime = 0.0

            val allSlicesInfo : MutableAggregateSliceInfoMap = mutableMapOf()
            val topLevelSliceInfo : MutableAggregateSliceInfoMap = mutableMapOf()
            val undifferentiatedSliceInfo : MutableAggregateSliceInfoMap = mutableMapOf()
            val nonNestedSliceInfo : MutableAggregateSliceInfoMap = mutableMapOf()

            newProc.threads.first().traverseSlices(object : SliceTraverser {
                // Our depth down an individual tree in the slice forest.
                var treeDepth = -1
                val sliceDepths: MutableMap<String, Int> = mutableMapOf()

                var lastTopLevelSlice : Slice? = null

                override fun beginSlice(slice : Slice) : TraverseAction {
                    val sliceContents = parseSliceName(slice.name)

                    ++this.treeDepth

                    val sliceDepth = this.sliceDepths.getOrDefault(sliceContents.name, -1)
                    this.sliceDepths[sliceContents.name] = sliceDepth + 1

                    if (slice.startTime < systemServerSlice.endTime) {
                        // This slice starts during the startup period.  If it
                        // ends within the startup period we will record info
                        // from this slice.  Either way we will visit its
                        // children.

                        if (this.treeDepth == 0 && this.lastTopLevelSlice != null) {
                            undifferentiatedTime += (slice.startTime - this.lastTopLevelSlice!!.endTime)
                        }

                        if (slice.endTime <= systemServerSlice.endTime) {
                            // This slice belongs in our collection.

                            // All Slice Timings
                            aggregateSliceInfo(allSlicesInfo, sliceContents, slice.duration)

                            // Undifferentiated Timings
                            aggregateSliceInfo(undifferentiatedSliceInfo, sliceContents, slice.durationSelf)

                            // Top-level timings
                            if (this.treeDepth == 0) {
                                aggregateSliceInfo(topLevelSliceInfo, sliceContents, slice.duration)
                            }

                            // Non-nested timings
                            if (sliceDepths[sliceContents.name] == 0) {
                                aggregateSliceInfo(nonNestedSliceInfo, sliceContents, slice.duration)
                            }
                        }

                        return TraverseAction.VISIT_CHILDREN

                    } else {
                        // All contents of this slice occur after the startup
                        // period has ended. We don't need to record anything
                        // or traverse any children.
                        return TraverseAction.DONE
                    }
                }

                override fun endSlice(slice : Slice) {
                    if (this.treeDepth == 0) {
                        lastTopLevelSlice = slice
                    }

                    val sliceInfo = parseSliceName(slice.name)
                    this.sliceDepths[sliceInfo.name] = this.sliceDepths[sliceInfo.name]!! - 1

                    --this.treeDepth
                }
            })

            startupEvents.add(
                StartupEvent(newProc,
                             newProcName,
                             systemServerSlice.startTime,
                             systemServerSlice.endTime,
                             startProcSlice.duration,
                             rfdSlice?.startTime,
                             firstSliceTime,
                             undifferentiatedTime,
                             schedSliceInfo,
                             allSlicesInfo,
                             topLevelSliceInfo,
                             undifferentiatedSliceInfo,
                             nonNestedSliceInfo))
        }
    }

    return startupEvents
}

fun aggregateSliceInfo(infoMap : MutableAggregateSliceInfoMap, sliceContents : SliceContents, duration : Double) {
    val aggInfo = infoMap.getOrPut(sliceContents.name, ::AggregateSliceInfo)
    ++aggInfo.count
    aggInfo.totalTime += duration
    if (sliceContents.value != null) {
        val (uniqueValueCount, uniqueValueDuration) = aggInfo.values.getOrDefault(sliceContents.value as String, Pair(0, 0.0))
        aggInfo.values[sliceContents.value as String] = Pair(uniqueValueCount + 1, uniqueValueDuration + duration)
    }
}