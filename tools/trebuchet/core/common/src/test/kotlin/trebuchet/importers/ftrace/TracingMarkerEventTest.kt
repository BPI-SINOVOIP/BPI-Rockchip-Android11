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

package trebuchet.importers.ftrace

import trebuchet.importers.ftrace.events.*
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertSame

class TracingMarkerEventTest {
    @Test fun testBegin() {
        val begin = detailsFor<BeginSliceEvent>("tracing_mark_write: B|5000|hello world")
        assertEquals(5000, begin.tgid)
        assertEquals("hello world", begin.title)
    }

    @Test fun testEnd() {
        detailsFor<EndSliceEvent>("tracing_mark_write: E")
    }

    @Test fun testCounter() {
        val counter = detailsFor<CounterSliceEvent>("tracing_mark_write: C|42|myCounter|9001")
        assertEquals(42, counter.tgid)
        assertEquals("myCounter", counter.name)
        assertEquals(9001, counter.value)
    }

    @Test fun testParentTS() {
        val parentTS = detailsFor<ParentTSEvent>(
                "tracing_mark_write: trace_event_clock_sync: parent_ts=4492.069824")
        assertEquals(4492.069824, parentTS.parentTimestamp)
    }

    @Test fun testRealtimeTS() {
        val realtimeTS = detailsFor<RealtimeTSEvent>(
                "tracing_mark_write: trace_event_clock_sync: realtime_ts=1491850748338")
        assertEquals(1491850748338, realtimeTS.realtimeTimestamp)
    }

    @Test fun testUnknown() {
        val details = detailsFor<FtraceEventDetails>("tracing_mark_write: this is unknown")
        assertSame(NoDetails, details)
    }

    @Test fun testStartAsync() {
        val start = detailsFor<StartAsyncSliceEvent>("tracing_mark_write: S|1150|launching: com.google.android.calculator|0")
        assertEquals(1150, start.tgid)
        assertEquals("launching: com.google.android.calculator", start.name)
        assertEquals(0, start.cookie)
    }

    @Test fun testFinishAsync() {
        val finish = detailsFor<FinishAsyncSliceEvent>("tracing_mark_write: F|1150|launching: com.google.android.calculator|0")
        assertEquals(1150, finish.tgid)
        assertEquals("launching: com.google.android.calculator", finish.name)
        assertEquals(0, finish.cookie)
    }
}