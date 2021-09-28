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

package trebuchet.extractors

import trebuchet.importers.ImportFeedback
import trebuchet.io.*
import trebuchet.util.indexOf
import java.util.zip.DataFormatException
import java.util.zip.Inflater
import kotlin.sequences.iterator

private const val TRACE = "TRACE:"

private fun findStart(buffer: GenericByteBuffer): Long {
    var start = buffer.indexOf(TRACE, 100)
    if (start == -1L) {
        start = 0L
    } else {
        start += TRACE.length
    }
    while (start < buffer.length &&
            (buffer[start] == '\n'.toByte() || buffer[start] == '\r'.toByte())) {
        start++
    }
    return start
}

private class DeflateProducer(stream: StreamingReader, val feedback: ImportFeedback)
        : BufferProducer {

    private val source = stream.source
    private val inflater = Inflater()
    private var closed = false

    private val sourceIterator = iterator {
        stream.loadIndex(stream.startIndex + 1024)
        val offset = findStart(stream)
        val buffIter = stream.iter(offset)
        var avgCompressFactor = 5.0
        while (buffIter.hasNext()) {
            val nextBuffer = buffIter.next()
            inflater.setInput(nextBuffer.buffer, nextBuffer.startIndex, nextBuffer.length)
            do {
                val remaining = inflater.remaining
                val estSize = (remaining * avgCompressFactor * 1.2).toInt()
                val array = ByteArray(estSize)
                val len = inflater.inflate(array)
                if (inflater.needsDictionary()) {
                    feedback.reportImportException(IllegalStateException(
                            "inflater needs dictionary, which isn't supported"))
                    return@iterator
                }
                val compressFactor = len.toDouble() / (remaining - inflater.remaining)
                avgCompressFactor = (avgCompressFactor * 9 + compressFactor) / 10
                yield(array.asSlice(len))
                if (closed) return@iterator
            } while (!inflater.needsInput())
            inflater.end()
        }
    }

    override fun next(): DataSlice? {
        return if (sourceIterator.hasNext()) sourceIterator.next() else null
    }

    override fun close() {
        closed = true
        source.close()
        inflater.end()
    }
}

class ZlibExtractor(val feedback: ImportFeedback) : Extractor {

    override fun extract(stream: StreamingReader, processSubStream: (BufferProducer) -> Unit) {
        processSubStream(DeflateProducer(stream, feedback))
    }

    object Factory : ExtractorFactory {
        private const val SIZE_TO_CHECK = 200

        override fun extractorFor(buffer: GenericByteBuffer, feedback: ImportFeedback): Extractor? {
            val start = findStart(buffer)
            val toRead = minOf((buffer.length - start).toInt(), SIZE_TO_CHECK)
            // deflate must contain at least a 2 byte header + 4 byte checksum
            // So if there's less than 6 bytes this either isn't deflate or
            // there's not enough data to try an inflate anyway
            if (toRead <= 6) {
                return null
            }
            val inflate = Inflater()
            try {
                val tmpBuffer = ByteArray(toRead) { buffer[start + it] }
                inflate.setInput(tmpBuffer)
                val result = ByteArray(1024)
                val inflated = inflate.inflate(result)
                inflate.end()
                if (inflated > 0) {
                    return ZlibExtractor(feedback)
                }
            } catch (ex: DataFormatException) {
                // Must not be deflate format
            } finally {
                inflate.end()
            }
            return null
        }
    }
}