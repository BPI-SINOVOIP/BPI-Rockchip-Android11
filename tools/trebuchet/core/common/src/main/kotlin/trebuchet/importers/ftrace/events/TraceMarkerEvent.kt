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
import trebuchet.util.BufferReader

class BeginSliceEvent(val tgid: Int, val title: String) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.threadFor(event.pid, tgid, event.task).slicesBuilder.beginSlice {
            it.startTime = event.timestamp
            it.name = title
        }
    }
}

class EndSliceEvent : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.threadFor(event.pid, event.tgid, event.task).slicesBuilder.endSlice {
            it.endTime = event.timestamp
        }
    }
}

class CounterSliceEvent(val tgid: Int, val name: String, val value: Long) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.threadFor(event.pid, tgid, event.task).process
                .addCounterSample(name, event.timestamp, value)
    }

}

class StartAsyncSliceEvent(val tgid: Int, val name: String, val cookie: Int) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.processFor(tgid, event.task).asyncSlicesBuilder
            .openAsyncSlice(event.pid, name, cookie, event.timestamp)
    }
}

class FinishAsyncSliceEvent(val tgid: Int, val name: String, val cookie: Int) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.processFor(tgid, event.task).asyncSlicesBuilder
            .closeAsyncSlice(event.pid, name, cookie, event.timestamp)
    }
}

private fun BufferReader.readBeginSlice(): BeginSliceEvent {
    // Begin format: B|<tgid>|<title>
    skipCount(2)
    val tgid = readInt()
    skip()
    val name = stringTo { end() }
    return BeginSliceEvent(tgid, name)
}

private fun BufferReader.readCounter(): CounterSliceEvent {
    // Counter format: C|<tgid>|<name>|<value>
    skipCount(2)
    val tgid = readInt()
    skip()
    val name = stringTo { skipUntil { it == '|'.toByte() } }
    skip()
    val value = readLong()
    return CounterSliceEvent(tgid, name, value)
}

private fun BufferReader.readOpenAsyncSlice(): StartAsyncSliceEvent {
    // Async start format: S|<tgid>|<title>|<cookie>
    skipCount(2)
    val tgid = readInt()
    skip()
    val name = stringTo { skipUntil {  it == '|'.toByte() } }
    skip()
    val cookie = readInt()
    return StartAsyncSliceEvent(tgid, name, cookie)
}

private fun BufferReader.readCloseAsyncSlice(): FinishAsyncSliceEvent {
    // Async start format: F|<tgid>|<title>|<cookie>
    skipCount(2)
    val tgid = readInt()
    skip()
    val name = stringTo { skipUntil {  it == '|'.toByte() } }
    skip()
    val cookie = readInt()
    return FinishAsyncSliceEvent(tgid, name, cookie)
}

class ParentTSEvent(val parentTimestamp: Double) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.modelFragment.parentTimestamp = parentTimestamp
    }
}

class RealtimeTSEvent(val realtimeTimestamp: Long) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.modelFragment.realtimeTimestamp = realtimeTimestamp
    }
}

object TraceMarkerEvent {
    const val Begin = 'B'.toByte()
    const val End = 'E'.toByte()
    const val Counter = 'C'.toByte()
    const val Start = 'S'.toByte()
    const val Finish = 'F'.toByte()

    val register: EventRegistryEntry = { sharedState ->

        val parentTsMatcher = sharedState.addPattern(
                "trace_event_clock_sync: parent_ts=(.*)")
        val realtimeTsMatcher = sharedState.addPattern(
                "trace_event_clock_sync: realtime_ts=(.*)")

        fun BufferReader.readClockSyncMarker(state: EventParserState): FtraceEventDetails? {
            // First check if the line we are importing is the parent timestamp line.
            tryMatch(state.matchers[parentTsMatcher]) {
                return ParentTSEvent(double(1))
            }
            // Test if the line we are testing has the realtime timestamp.
            tryMatch(state.matchers[realtimeTsMatcher]) {
                return RealtimeTSEvent(long(1))
            }
            return null
        }

        sharedState.onParseDetails("tracing_mark_write") { state, details ->
            state.readDetails(details) {
                when (peek()) {
                    Begin -> readBeginSlice()
                    End -> EndSliceEvent()
                    Counter -> readCounter()
                    Start -> readOpenAsyncSlice()
                    Finish -> readCloseAsyncSlice()
                    else -> readClockSyncMarker(state)
                }
            }
        }
    }
}