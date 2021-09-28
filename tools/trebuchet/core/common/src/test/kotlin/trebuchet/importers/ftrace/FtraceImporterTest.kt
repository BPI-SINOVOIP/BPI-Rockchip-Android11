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

package trebuchet.importers.ftrace

import org.junit.Assert.*
import org.junit.Test
import trebuchet.importers.FatalImportFeedback
import trebuchet.io.StreamingReader
import trebuchet.model.Model
import trebuchet.testutils.makeReader

class FtraceImporterTest {

    fun String.makeLoadedReader(): StreamingReader {
        val reader = this.makeReader()
        reader.loadIndex(reader.keepLoadedSize.toLong())
        return reader
    }

    @Test fun testImporterFor() {
        val line1 = "atrace-7100  ( 7100) [001] ...1  4492.047398: tracing_mark_write: trace_event_clock_sync: parent_ts=4492.069824"
        val line2 = "<idle>-0     (-----) [001] dN.4  4492.047448: sched_wakeup: comm=ksoftirqd/1 pid=15 prio=120 success=1 target_cpu=001"
        val traceData = withHeader(line1, line2)
        assertNotNull(FtraceImporter.Factory.importerFor(HEADER.makeLoadedReader(), FatalImportFeedback))
        assertNotNull(FtraceImporter.Factory.importerFor(traceData.makeLoadedReader(), FatalImportFeedback))
        assertNull(FtraceImporter.Factory.importerFor(HEADER.makeReader(), FatalImportFeedback))
        assertNull(FtraceImporter.Factory.importerFor(traceData.makeReader(), FatalImportFeedback))
        assertNull(FtraceImporter.Factory.importerFor("Hello, World!".makeLoadedReader(), FatalImportFeedback))
        assertNull(FtraceImporter.Factory.importerFor(line1.makeLoadedReader(), FatalImportFeedback))
    }

    @Test fun testImporterTimestamp() {
        val traceData = withHeader(
            "equicksearchbox-6381  ( 6381) [004] ...1  4493.734816: tracing_mark_write: trace_event_clock_sync: parent_ts=23816.083984",
            "equicksearchbox-6381  ( 6381) [004] ...1  4493.734855: tracing_mark_write: trace_event_clock_sync: realtime_ts=1491850748338")
        val importer = FtraceImporter(FatalImportFeedback)
        val modelFragment = importer.import(traceData.makeReader())
        assertNotNull(modelFragment)
        if (modelFragment == null) return // just to make Kotlin happy
        assertEquals(23816.083984, modelFragment.parentTimestamp, .001)
        assertEquals(1491850748338, modelFragment.realtimeTimestamp)
    }

    @Test fun testImportBeginEnd() {
        val traceData = withHeader(
                " equicksearchbox-6381  ( 6381) [004] ...1  4493.734816: tracing_mark_write: E",
                " equicksearchbox-6381  ( 6381) [005] ...1  4493.730786: tracing_mark_write: B|6381|Choreographer#doFrame",
                " equicksearchbox-6381  ( 6381) [005] ...1  4493.730824: tracing_mark_write: B|6381|input",
                " equicksearchbox-6381  ( 6381) [005] ...1  4493.732287: tracing_mark_write: E",
                " equicksearchbox-6381  ( 6381) [005] ...1  4493.732310: tracing_mark_write: B|6381|traversal",
                " equicksearchbox-6381  ( 6381) [005] ...1  4493.732410: tracing_mark_write: B|6381|draw",
                " equicksearchbox-6381  ( 6381) [004] ...1  4493.734816: tracing_mark_write: E",
                " equicksearchbox-6381  ( 6381) [004] ...1  4493.734828: tracing_mark_write: E",
                " equicksearchbox-6381  ( 6381) [004] ...1  4493.734855: tracing_mark_write: E")
        val importer = FtraceImporter(FatalImportFeedback)
        val modelFragment = importer.import(traceData.makeReader())
        assertNotNull(modelFragment)
        if (modelFragment == null) return // just to make Kotlin happy
        assertEquals(1, modelFragment.processes.size)
        val process = modelFragment.processes[0]
        assertEquals(6381, process.id)
        assertEquals("equicksearchbox", process.name)
        assertEquals(1, process.threads.size)
        val thread = process.threads.first()
        assertEquals(6381, thread.id)
        assertEquals("equicksearchbox", thread.name)
        val sliceGroup = thread.slicesBuilder
        assertFalse(sliceGroup.hasOpenSlices())
        assertEquals(1, sliceGroup.slices.size)
        val doFrameSlice = sliceGroup.slices[0]
        assertEquals("Choreographer#doFrame", doFrameSlice.name)
        assertEquals(2, doFrameSlice.children.size)
        assertEquals("input", doFrameSlice.children[0].name)
        assertEquals(0, doFrameSlice.children[0].children.size)
        assertEquals("traversal", doFrameSlice.children[1].name)
        assertEquals(1, doFrameSlice.children[1].children.size)
        assertEquals("draw", doFrameSlice.children[1].children[0].name)
    }

    @Test fun testImportBeginEndNoTgids() {
        val traceData = withHeader(
                " equicksearchbox-6381  (-----) [004] ...1  4493.734828: tracing_mark_write: E",
                " equicksearchbox-6381  (-----) [005] ...1  4493.730786: tracing_mark_write: B|6381|Choreographer#doFrame",
                " equicksearchbox-6381  (-----) [005] ...1  4493.730824: tracing_mark_write: B|6381|input",
                " equicksearchbox-6381  (-----) [005] ...1  4493.732287: tracing_mark_write: E",
                " equicksearchbox-6381  (-----) [005] ...1  4493.732310: tracing_mark_write: B|6381|traversal",
                " equicksearchbox-6381  (-----) [005] ...1  4493.732410: tracing_mark_write: B|6381|draw",
                " equicksearchbox-6381  (-----) [004] ...1  4493.734816: tracing_mark_write: E",
                " equicksearchbox-6381  (-----) [004] ...1  4493.734828: tracing_mark_write: E",
                " equicksearchbox-6381  (-----) [004] ...1  4493.734855: tracing_mark_write: E")
        val importer = FtraceImporter(FatalImportFeedback)
        val modelFragment = importer.import(traceData.makeReader())
        assertNotNull(modelFragment)
        if (modelFragment == null) return // just to make Kotlin happy
        assertEquals(1, modelFragment.processes.size)
        val process = modelFragment.processes[0]
        assertEquals(6381, process.id)
        assertEquals("equicksearchbox", process.name)
        assertEquals(1, process.threads.size)
        val thread = process.threads.first()
        assertEquals(6381, thread.id)
        assertEquals("equicksearchbox", thread.name)
        val sliceGroup = thread.slicesBuilder
        assertFalse(sliceGroup.hasOpenSlices())
        assertEquals(1, sliceGroup.slices.size)
        val doFrameSlice = sliceGroup.slices[0]
        assertEquals("Choreographer#doFrame", doFrameSlice.name)
        assertEquals(2, doFrameSlice.children.size)
        assertEquals("input", doFrameSlice.children[0].name)
        assertEquals(0, doFrameSlice.children[0].children.size)
        assertEquals("traversal", doFrameSlice.children[1].name)
        assertEquals(1, doFrameSlice.children[1].children.size)
        assertEquals("draw", doFrameSlice.children[1].children[0].name)
    }

    @Test fun testCounters() {
        val model = parse(
                "          <...>-4932  (-----) [000] ...1  4493.660106: tracing_mark_write: C|3691|iq|1",
                "InputDispatcher-4931  ( 3691) [000] ...1  4493.660790: tracing_mark_write: C|3691|iq|0")
        assertEquals(1, model.processes.size)
        val p = model.processes[3691]!!
        assertEquals(3691, p.id)
        assertEquals(3, p.threads.size)
        assertTrue(p.threads.any { it.id == 3691 })
        assertTrue(p.threads.any { it.id == 4931 })
        assertTrue(p.threads.any { it.id == 4932 })
        assertEquals(1, p.counters.size)
        val c = p.counters[0]
        assertEquals(2, c.events.size)
        assertEquals(4493.660106, c.events[0].timestamp, .1)
        assertEquals(1, c.events[0].count)
        assertEquals(4493.660790, c.events[1].timestamp, .1)
        assertEquals(0, c.events[1].count)

    }

    fun parse(vararg lines: String): Model {
        val traceData = withHeader(*lines)
        val importer = FtraceImporter(FatalImportFeedback)
        val modelFragment = importer.import(traceData.makeReader())
        assertNotNull(modelFragment)
        return Model(modelFragment!!)
    }

    fun withHeader(vararg lines: String): String {
        return lines.joinToString("\n", HEADER)
    }

    val HEADER = """TRACE:
# tracer: nop
#
# entries-in-buffer/entries-written: 69580/69580   #P:8
#
#                                      _-----=> irqs-off
#                                     / _----=> need-resched
#                                    | / _---=> hardirq/softirq
#                                    || / _--=> preempt-depth
#                                    ||| /     delay
#           TASK-PID    TGID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |        |      |   ||||       |         |
"""
}