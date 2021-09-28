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

import trebuchet.importers.ImportFeedback
import trebuchet.importers.Importer
import trebuchet.importers.ImporterFactory
import trebuchet.importers.ftrace.events.CpuBufferStarted
import trebuchet.importers.ftrace.events.FtraceEvent
import trebuchet.importers.ftrace.events.createEventParser
import trebuchet.io.GenericByteBuffer
import trebuchet.io.StreamingReader
import trebuchet.io.iterLines
import trebuchet.model.fragments.ModelFragment
import trebuchet.util.contains
import trebuchet.util.par_map

class FtraceImporter(val feedback: ImportFeedback) : Importer {
    var score: Long = 0
    var state = FtraceImporterState()

    override fun import(stream: StreamingReader): ModelFragment? {
        par_map(stream.iterLines(), ::createEventParser) { parserState, it ->
            try {
                FtraceEvent.tryParseText(parserState, it)
            } catch (t: Throwable) {
                println("line '$it' failed")
                t.printStackTrace()
                null
            }
        }.forEach {
            if (it == null) {
                score--
                if (score < -20) {
                    feedback.reportImportWarning("Too many errors, giving up")
                    return null
                }
            } else {
                score++
                if (it === CpuBufferStarted) {
                    state = FtraceImporterState()
                } else {
                    state.importEvent(it)
                }
            }
        }
        return state.finish()
    }

    object Factory : ImporterFactory {
        override fun importerFor(buffer: GenericByteBuffer, feedback: ImportFeedback): Importer? {
            if (buffer.contains("# tracer: nop\n", 1000)
                    || buffer.contains("       trace-cmd", 1000)) {
                return FtraceImporter(feedback)
            }
            return null
        }
    }
}