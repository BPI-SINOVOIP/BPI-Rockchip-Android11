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
import trebuchet.importers.ftrace.FtraceLine
import trebuchet.io.DataSlice
import trebuchet.model.InvalidId
import trebuchet.util.BufferReader
import trebuchet.util.MatchResult
import trebuchet.util.StringCache
import java.util.regex.Matcher
import java.util.regex.Pattern

const val FtraceLineRE = """^ *(.{1,16})-(\d+) +(?:\( *(\d+)?-*\) )?\[(\d+)] (?:[dX.]...)? *([\d.]*): *([^:]*): *(.*) *$"""

interface FtraceEventDetails {
    fun import(event: FtraceEvent, state: FtraceImporterState)
}

val NoDetails = object : FtraceEventDetails {
    override fun import(event: FtraceEvent, state: FtraceImporterState) {}
}

typealias EventDetailsParser = (state: EventParserState, slice: DataSlice) -> FtraceEventDetails?

class SharedEventParserState {
    private val patterns = mutableListOf<Pattern>()
    private val detailHandlers = hashMapOf<String, EventDetailsParser>()
    private var frozen = false

    init {
        EventRegistry.forEach { it(this) }
    }

    fun addPattern(newPattern: String) = addPattern(Pattern.compile(newPattern))

    fun addPattern(newPattern: Pattern): Int {
        if (frozen) throw IllegalStateException("State is frozen")
        val index = patterns.size
        patterns.add(newPattern)
        return index
    }

    fun createParser(): EventParserState {
        frozen = true
        return EventParserState(patterns, detailHandlers)
    }

    fun onParseDetails(funcName: String, parser: EventDetailsParser) {
        if (frozen) throw IllegalStateException("State is frozen")
        detailHandlers[funcName] = parser
    }

    inline fun onParseDetailsWithMatch(funcName: String, regex: String,
                                       crossinline result: MatchResult.() -> FtraceEventDetails?) {
        val patternMatcher = addPattern(regex)
        onParseDetails(funcName) { state, details ->
            state.ifMatches(patternMatcher, details) {
                return@onParseDetails this.result()
            }
            null
        }
    }

    inline fun onParseDetailsWithMatch(funcNames: Array<String>, regex: String,
                                       crossinline result: MatchResult.() -> FtraceEventDetails?) {
        val patternMatcher = addPattern(regex)
        funcNames.forEach { funcName ->
            onParseDetails(funcName) { state, details ->
                state.ifMatches(patternMatcher, details) {
                    return@onParseDetails this.result()
                }
                null
            }
        }
    }
}

val GlobalSharedParserState = SharedEventParserState()

fun createEventParser() = GlobalSharedParserState.createParser()

class EventParserState constructor(patterns: List<Pattern>,
                                   private val detailHandlers: Map<String, EventDetailsParser>) {
    val matchers = Array<Matcher>(patterns.size) { patterns[it].matcher("") }
    val stringCache = StringCache()
    val reader = BufferReader()

    inline fun ifMatches(matcher: Int, slice: DataSlice, result: MatchResult.() -> Unit): Boolean {
        reader.reset(slice, stringCache)
        return reader.tryMatch(matchers[matcher], result)
    }

    fun detailsForText(funcName: CharSequence, detailsSlice: DataSlice): FtraceEventDetails {
        return detailHandlers[funcName]?.invoke(this, detailsSlice) ?: NoDetails
    }

    inline fun <T> readDetails(detailsSlice: DataSlice, init: BufferReader.() -> T): T {
        return reader.read(detailsSlice, stringCache, init)
    }
}