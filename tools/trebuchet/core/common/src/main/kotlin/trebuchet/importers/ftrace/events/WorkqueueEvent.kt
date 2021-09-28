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

class WorkqueueExecuteStartEvent(val struct: String, val function: String) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.threadFor(event).slicesBuilder.beginSlice {
            it.name = function
            it.startTime = event.timestamp
        }
    }
}

class WorkqueueExecuteEndEvent(val struct: String) : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {
        state.threadFor(event).slicesBuilder.endSlice {
            it.endTime = event.timestamp
        }
    }
}

object WorkqueueEvent {
    private const val workqueueExecuteStartRE = """work struct (\w+): function (\S+)"""
    private const val workqueueExecuteEndRE = """work struct (\w+)"""

    val register: EventRegistryEntry = { sharedState ->
        sharedState.onParseDetailsWithMatch("workqueue_execute_start", workqueueExecuteStartRE) {
            WorkqueueExecuteStartEvent(string(1), string(2))
        }
        sharedState.onParseDetailsWithMatch("workqueue_execute_end", workqueueExecuteEndRE) {
            WorkqueueExecuteEndEvent(string(1))
        }
    }
}