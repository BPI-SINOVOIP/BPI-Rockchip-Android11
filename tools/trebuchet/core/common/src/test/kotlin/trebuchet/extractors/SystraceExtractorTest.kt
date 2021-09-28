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

package trebuchet.extractors

import org.junit.Test
import org.junit.Assert.*
import trebuchet.io.BufferProducer
import trebuchet.io.DataSlice
import trebuchet.io.StreamingReader
import trebuchet.io.asSlice


class SystraceExtractorTest {
    val HTML_HEADER = "<!DOCTYPE html>\n<html>\n<head>"
    val TRACE_BEGIN = """<script class="trace-data" type="application/text">"""
    val TRACE_END = "</script>"

    @Test
    fun testExtractMultiWindowSingleOutput() {
        val buffers = listOf(HTML_HEADER, TRACE_BEGIN, "one", TRACE_END)
        val result = extract(buffers)
        assertEquals(1, result.size)
        assertEquals("one", result[0].toString())
    }

    @Test
    fun testExtractMultiWindowMultiOutput() {
        val buffers = listOf(HTML_HEADER, TRACE_BEGIN, "first", "second", "third", TRACE_END)
        val result = extract(buffers)
        assertEquals(3, result.size)
        assertEquals("first", result[0].toString())
        assertEquals("second", result[1].toString())
        assertEquals("third", result[2].toString())
    }

    @Test
    fun testExtractExtremeSplitting() {
        val buffers = listOf(HTML_HEADER, TRACE_BEGIN, "12345", TRACE_END)
                .joinToString(separator = "").asIterable().map { it.toString() }
        val result = extract(buffers)
//        assertEquals(5, result.size)
        assertEquals("1", result[0].toString())
        assertEquals("2", result[1].toString())
        assertEquals("3", result[2].toString())
        assertEquals("4", result[3].toString())
        assertEquals("5", result[4].toString())
    }

    @Test
    fun testExtractSingleBuffer() {
        val buffer = listOf(HTML_HEADER, TRACE_BEGIN, "one big buffer", TRACE_END)
                .joinToString(separator = "")
        val result = extract(listOf(buffer))
        assertEquals(1, result.size)
        assertEquals("one big buffer", result[0].toString())
    }

    fun extract(list: Iterable<String>): List<DataSlice> {
        val extractor = SystraceExtractor()
        var output = mutableListOf<DataSlice>()
        extractor.extract(makeStream(list.iterator())) { outputStream ->
            while (true) {
                val next = outputStream.next()
                if (next == null) {
                    outputStream.close()
                    break
                }
                output.add(next)
            }
        }
        return output
    }

    fun makeStream(list: Iterator<String>): StreamingReader {
        return StreamingReader(object : BufferProducer {
            override fun next(): DataSlice? {
                if (list.hasNext()) {
                    return list.next().asSlice()
                }
                return null
            }
        })
    }
}