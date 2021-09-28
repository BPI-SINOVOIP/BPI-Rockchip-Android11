/*
 * Copyright 2019 Google Inc.
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

package trebuchet.analyzer.startup

import getStartupEvents
import org.junit.Test
import trebuchet.io.BufferProducer
import trebuchet.io.DataSlice
import trebuchet.io.asSlice
import trebuchet.model.Model
import trebuchet.task.ImportTask
import trebuchet.util.PrintlnImportFeedback

fun parseString(source: String): Model {
    val task = ImportTask(PrintlnImportFeedback())
    return task.import(object : BufferProducer {
        var hasRead = false
        override fun next(): DataSlice? {
            if (hasRead) return null
            hasRead = true
            return source.asSlice()
        }
    })
}

class StartupCommonTest {
    @Test
    fun testEmptyThreadStart() {
        // This is a reduced trace based on one seen in the wild that used to
        // crash because it creates a thread with no slices assigned to it.
        val test = """
TRACE:
# tracer: nop
#
# entries-in-buffer/entries-written: 289801/289801   #P:4
#
#                                      _-----=> irqs-off
#                                     / _----=> need-resched
#                                    | / _---=> hardirq/softirq
#                                    || / _--=> preempt-depth
#                                    ||| /     delay
#           TASK-PID    TGID   CPU#  ||||    TIMESTAMP  FUNCTION
#              | |        |      |   ||||       |         |
   Binder:980_13-3951  (  980) [002] ...1  1628.269667: tracing_mark_write: E|980
   Binder:980_13-3951  (  980) [002] ...2  1628.270068: tracing_mark_write: S|980|launching: com.example.eholk.myfirstapp|0
           <...>-1104  (-----) [000] ...1  1628.292040: tracing_mark_write: B|980|Start proc: com.example.eholk.myfirstapp
           <...>-1103  (-----) [002] d..4  1628.300748: sched_waking: comm=system_server pid=980 prio=118 target_cpu=001
    RenderThread-9817  ( 9793) [003] d..3  1628.457017: sched_waking: comm=holk.myfirstapp pid=9793 prio=110 target_cpu=002
    RenderThread-9817  ( 9793) [003] ...1  1628.457191: tracing_mark_write: B|9793|Thread birth
        """

        val trace = parseString(test)
        trace.getStartupEvents()
    }
}
