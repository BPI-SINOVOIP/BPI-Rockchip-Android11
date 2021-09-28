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
import trebuchet.model.SchedulingState
import trebuchet.util.MatchResult
import trebuchet.util.PreviewReader

private fun PreviewReader.readSchedulingState(): SchedulingState {
    val byte = readByte()
    return when (byte) {
        'S'.toByte() -> SchedulingState.SLEEPING
        'R'.toByte() -> SchedulingState.RUNNABLE
        'D'.toByte() -> {
            if (peek() == '|'.toByte()) {
                skip()
                return when (readByte()) {
                    'K'.toByte() -> SchedulingState.UNINTR_SLEEP_WAKE_KILL
                    'W'.toByte() -> SchedulingState.UNINTR_SLEEP_WAKING
                    else -> SchedulingState.UNINTR_SLEEP
                }
            }
            SchedulingState.UNINTR_SLEEP
        }
        'T'.toByte() -> SchedulingState.STOPPED
        't'.toByte() -> SchedulingState.DEBUG
        'Z'.toByte() -> SchedulingState.ZOMBIE
        'X'.toByte() -> SchedulingState.EXIT_DEAD
        'x'.toByte() -> SchedulingState.TASK_DEAD
        'K'.toByte() -> SchedulingState.WAKE_KILL
        'W'.toByte() -> SchedulingState.WAKING
        else -> SchedulingState.UNKNOWN
    }
}

private fun MatchResult.schedState(group: Int) = read(group) { readSchedulingState() }

class SchedSwitchEvent(val prevComm: String, val prevPid: Int, val prevPrio: Int,
                       val prevState: SchedulingState, val nextComm: String, val nextPid: Int,
                       val nextPrio: Int) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        val prevThread = state.threadFor(prevPid, task = prevComm)
        val nextThread = state.threadFor(nextPid, task = nextComm)
        val cpu = state.cpuFor(event.cpu)

        prevThread.schedulingStateBuilder.switchState(prevState, event.timestamp)
        nextThread.schedulingStateBuilder.switchState(SchedulingState.RUNNING, event.timestamp)
        cpu.schedulingProcessBuilder.switchProcess(nextThread.process, nextThread, event.timestamp)
    }
}

class SchedWakeupEvent(val comm: String, val pid: Int, val prio: Int, val targetCpu: Int)
    : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        val thread = state.threadFor(pid, task = comm)
        thread.schedulingStateBuilder.switchState(SchedulingState.WAKING, event.timestamp)
    }
}

object SchedEvent {
    private const val SchedSwitchRE = "prev_comm=(.*) prev_pid=(\\d+) prev_prio=(\\d+) prev_state=([^\\s]+) ==> next_comm=(.*) next_pid=(\\d+) next_prio=(\\d+)"
    private const val SchedWakeupRE = """comm=(.+) pid=(\d+) prio=(\d+)(?: success=\d+)? target_cpu=(\d+)"""

    val register: EventRegistryEntry = { sharedState ->
        sharedState.onParseDetailsWithMatch("sched_switch", SchedSwitchRE) {
            SchedSwitchEvent(string(1), int(2), int(3), schedState(4),
                    string(5), int(6), int(7))
        }
        sharedState.onParseDetailsWithMatch(arrayOf("sched_waking", "sched_wake"), SchedWakeupRE) {
            SchedWakeupEvent(string(1), int(2), int(3), int(4))
        }
    }
}
