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

import trebuchet.importers.ftrace.events.FtraceEvent
import trebuchet.importers.ftrace.events.FtraceEventDetails
import trebuchet.importers.ftrace.events.createEventParser
import trebuchet.io.asSlice
import kotlin.test.fail

fun parseEvent(line: String): FtraceEvent {
    return FtraceEvent.tryParseText(createEventParser(),
            line.asSlice()) ?: fail("Failed to parseEvent event '${line}'")
}

inline fun <reified T : FtraceEventDetails> detailsFor(details: String): T {
    val event = parseEvent("atrace-7100  ( 7100) [001] ...1  4492.047398: $details")
    return event.details as? T
            ?: fail("Details expected ${T::class.simpleName}, got ${event.details::class.simpleName}")
}