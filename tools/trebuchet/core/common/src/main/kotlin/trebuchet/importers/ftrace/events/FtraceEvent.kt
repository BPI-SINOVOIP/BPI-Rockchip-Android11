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

package trebuchet.importers.ftrace.events

import trebuchet.importers.ftrace.FtraceImporterState
import trebuchet.io.DataSlice
import trebuchet.model.InvalidId

val CpuBufferStarted = FtraceEvent(null, InvalidId, InvalidId, -1, -1.0,
        "CPU BUFFER STARTED", NoDetails)

class FtraceEvent(val task: String?,
                  val pid: Int,
                  val tgid: Int,
                  val cpu: Int,
                  val timestamp: Double,
                  val function: String,
                  val details: FtraceEventDetails) {

    fun import(state: FtraceImporterState) {
        details.import(this, state)
    }

    companion object {
        private var ftraceLineMatcher: Int = -1
        private var cpuBufferStarted: Int = -1

        val register: EventRegistryEntry = { sharedState ->
            ftraceLineMatcher = sharedState.addPattern(FtraceLineRE)
            cpuBufferStarted = sharedState.addPattern("##### CPU \\d+ buffer started ####")
        }

        fun tryParseText(state: EventParserState, slice: DataSlice): FtraceEvent? {
            state.ifMatches(ftraceLineMatcher, slice) {
                val task = string(1)
                val function = string(6)
                return FtraceEvent(
                        task = if (task == "<...>") null else task,
                        pid = int(2),
                        tgid = intOr(3, InvalidId),
                        cpu = int(4),
                        timestamp = double(5),
                        function = function,
                        details = state.detailsForText(function, slice(7))
                )
            }
            state.ifMatches(cpuBufferStarted, slice) {
                return CpuBufferStarted
            }
            return null
        }

    }
}