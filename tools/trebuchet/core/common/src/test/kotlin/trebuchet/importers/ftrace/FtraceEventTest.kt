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
import trebuchet.io.asSlice
import trebuchet.model.InvalidId
import trebuchet.model.SchedulingState
import kotlin.test.*

class FtraceEventTest {

    /** Tests a 3.2+ ftrace line with tgid and no missing data */
    @Test fun testParseLineComplete() {
        val event = parseEvent("atrace-7100  ( 7100) [001] ...1  4492.047398: tracing_mark_write: " +
                "trace_event_clock_sync: parent_ts=4492.069824")
        assertEquals("atrace", event.task)
        assertEquals(7100, event.pid)
        assertEquals(7100, event.tgid)
        assertEquals(1, event.cpu)
        assertEquals(4492.047398, event.timestamp)
        assertEquals("tracing_mark_write", event.function)
        assertTrue { event.details is ParentTSEvent }
        assertEquals(4492.069824, (event.details as ParentTSEvent).parentTimestamp)
    }

    /** Tests a 3.2+ ftrace line with tgid option but missing tgid & task name */
    @Test fun testParseLineUnknownTgid() {
        val event = parseEvent("<...>-2692  (-----) [000] d..4  4492.062904: sched_blocked_reason: pid=2694 " +
                "iowait=0 caller=qpnp_vadc_conv_seq_request+0x64/0x794")
        assertNull(event.task)
        assertEquals(2692, event.pid)
        assertEquals(InvalidId, event.tgid)
        assertEquals(0, event.cpu)
        assertEquals(4492.062904, event.timestamp)
        assertEquals("sched_blocked_reason", event.function)
    }

    /** Tests a 3.2+ ftrace line without tgid option */
    @Test fun testParseLineNoTgid() {
        val event = parseEvent("atrace-7100  [001] d..3  4492.047432: sched_switch: prev_comm=atrace prev_pid=7100 " +
                "prev_prio=120 prev_state=S ==> next_comm=swapper/1 next_pid=0 next_prio=120")
        assertEquals("atrace", event.task)
        assertEquals(7100, event.pid)
        assertEquals(InvalidId, event.tgid)
        assertEquals(1, event.cpu)
        assertEquals(4492.047432, event.timestamp)
        assertEquals("sched_switch", event.function)
        val details = event.details as? SchedSwitchEvent ?: fail("${event.details} not instance of SchedSwitchEvent")
        assertEquals("atrace", details.prevComm)
        assertEquals(7100, details.prevPid)
        assertEquals(120, details.prevPrio)
        assertEquals(SchedulingState.SLEEPING, details.prevState)
        assertEquals("swapper/1", details.nextComm)
        assertEquals(0, details.nextPid)
        assertEquals(120, details.nextPrio)
    }

    /** Tests a pre-3.2 ftrace line */
    @Test fun testParseLineLegacy() {
        val event = parseEvent("Binder-Thread #-647   [001]   260.464294: sched_switch: prev_pid=0")
        assertEquals("Binder-Thread #", event.task)
        assertEquals(647, event.pid)
        assertEquals(InvalidId, event.tgid)
        assertEquals(1, event.cpu)
        assertEquals(260.464294, event.timestamp)
        assertEquals("sched_switch", event.function)
        assertSame(NoDetails, event.details)
    }
}