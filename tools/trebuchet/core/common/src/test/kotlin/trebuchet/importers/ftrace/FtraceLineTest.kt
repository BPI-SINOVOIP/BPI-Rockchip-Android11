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
import trebuchet.io.asSlice
import trebuchet.model.InvalidId
import trebuchet.util.StringCache

class FtraceLineTest {

    /** Tests a 3.2+ ftrace line with tgid and no missing data */
    @Test fun testFtraceLineParser1() {
        FtraceLine.Parser(StringCache()).parseLine(
                ("atrace-7100  ( 7100) [001] d..3  4492.047432: sched_switch: prev_comm=atrace prev_pid=7100" +
                " prev_prio=120 prev_state=S ==> next_comm=swapper/1 next_pid=0 next_prio=120").asSlice()) {
            line ->
            assertEquals("atrace", line.task)
            assertEquals(7100, line.pid)
            assertEquals(7100, line.tgid)
            assertEquals(1, line.cpu)
            assertEquals(4492.047432, line.timestamp, .1)
            assertEquals("sched_switch", line.function.toString())
            assertEquals("prev_comm=atrace prev_pid=7100 prev_prio=120 prev_state=S ==> next_comm=swapper/1" +
                    " next_pid=0 next_prio=120", line.functionDetailsReader.stringTo { end() })
        }
    }

    /** Tests a 3.2+ ftrace line with tgid option but missing tgid & task name */
    @Test fun testFtraceLineParser2() {
        FtraceLine.Parser(StringCache()).parseLine(("           <...>-2692  (-----) [000] d..4  4492.062904: " +
                "sched_blocked_reason: pid=2694 iowait=0 caller=qpnp_vadc_conv_seq_request+0x64/0x794").asSlice()) {
            line ->
            assertNull(line.task)
            assertEquals(2692, line.pid)
            assertEquals(InvalidId, line.tgid)
            assertEquals(0, line.cpu)
            assertEquals(4492.062904, line.timestamp, .1)
            assertEquals("sched_blocked_reason", line.function.toString())
            assertEquals("pid=2694 iowait=0 caller=qpnp_vadc_conv_seq_request+0x64/0x794",
                    line.functionDetailsReader.stringTo { end() })
        }
    }

    /** Tests a 3.2+ ftrace line without tgid option */
    @Test fun testFtraceLineParser3() {
        FtraceLine.Parser(StringCache()).parseLine(
                ("atrace-7100  [001] d..3  4492.047432: sched_switch: prev_comm=atrace prev_pid=7100 " +
                "prev_prio=120 prev_state=S ==> next_comm=swapper/1 next_pid=0 next_prio=120").asSlice()) {
            line ->
            assertEquals("atrace", line.task)
            assertEquals(7100, line.pid)
            assertEquals(InvalidId, line.tgid)
            assertEquals(1, line.cpu)
            assertEquals(4492.047432, line.timestamp, .1)
            assertEquals("sched_switch", line.function.toString())
            assertEquals("prev_comm=atrace prev_pid=7100 prev_prio=120 prev_state=S ==> next_comm=swapper/1" +
                    " next_pid=0 next_prio=120", line.functionDetailsReader.stringTo { end() })
        }
    }

    /** Tests a pre-3.2 ftrace line */
    @Test fun testFtraceLineParser4() {
        FtraceLine.Parser(StringCache()).parseLine(
                "Binder-Thread #-647   [001]   260.464294: sched_switch: prev_pid=0".asSlice()) {
            line ->
            assertEquals("Binder-Thread #", line.task)
            assertEquals(647, line.pid)
            assertEquals(InvalidId, line.tgid)
            assertEquals(1, line.cpu)
            assertEquals(260.464294, line.timestamp, .1)
            assertEquals("sched_switch", line.function.toString())
            assertEquals("prev_pid=0", line.functionDetailsReader.stringTo { end() })
        }
    }

    @Test fun testFtraceLineParse5() {
        FtraceLine.Parser(StringCache()).parseLine(("NearbyMessages-5675 ( 2303) [001] dn.4 23815.466030: "
                + "sched_wakeup: comm=Binder:928_1 pid=945 prio=120 success=1 target_cpu=001").asSlice()) {
            line ->
            assertEquals("NearbyMessages", line.task)
            assertEquals(5675, line.pid)
            assertEquals(2303, line.tgid)
            assertEquals(1, line.cpu)
            assertEquals(23815.466030, line.timestamp, .1)
            assertEquals("sched_wakeup", line.function.toString())
            assertEquals("comm=Binder:928_1 pid=945 prio=120 success=1 target_cpu=001",
                    line.functionDetailsReader.stringTo { end() })
        }
    }

    @Test fun testFtraceLineParse6() {
        FtraceLine.Parser(StringCache()).parseLine(("NearbyMessages-56751 (12303) [001] dn.4 23815.466030: "
                + "sched_wakeup: comm=Binder:928_1 pid=945 prio=120 success=1 target_cpu=001").asSlice()) {
            line ->
            assertEquals("NearbyMessages", line.task)
            assertEquals(56751, line.pid)
            assertEquals(12303, line.tgid)
            assertEquals(1, line.cpu)
            assertEquals(23815.466030, line.timestamp, .1)
            assertEquals("sched_wakeup", line.function.toString())
            assertEquals("comm=Binder:928_1 pid=945 prio=120 success=1 target_cpu=001",
                    line.functionDetailsReader.stringTo { end() })
        }
    }

    @Test fun testFtraceLineParse7() {
        FtraceLine.Parser(StringCache()).parseLine(("<9729>-9729  (-----) [003] ...1 23815.466141: "
                + "tracing_mark_write: trace_event_clock_sync: parent_ts=23816.083984").asSlice()) {
            line ->
            assertEquals("<9729>", line.task)
            assertEquals(9729, line.pid)
            assertEquals(InvalidId, line.tgid)
            assertEquals(3, line.cpu)
            assertEquals(23815.466141, line.timestamp, .1)
            assertEquals("tracing_mark_write", line.function.toString())
            assertEquals("trace_event_clock_sync: parent_ts=23816.083984",
                    line.functionDetailsReader.stringTo { end() })
        }
    }
}