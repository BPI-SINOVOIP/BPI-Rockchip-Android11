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

package trebuchet.io

import kotlin.sequences.iterator

private fun findNewlineInWindow(window: StreamingReader.Window, startIndex: Long): Long {
    for (i in startIndex..window.globalEndIndex) {
        if (window[i] == '\n'.toByte()) return i
    }
    return -1
}

/**
 * Iterates over all the lines in the stream, skipping any empty line. Lines do not contain
 * the line-end marker(s). Handles both LF & CRLF endings.
 */
fun StreamingReader.iterLines() = iterator {
    val stream = this@iterLines

    var lineStartIndex = stream.startIndex
    while (true) {
        var index = lineStartIndex
        var foundAt = -1L
        while (true) {
            if (index > stream.endIndex) {
                if (!stream.loadIndex(index)) break
            }
            val window = stream.windowFor(index)
            foundAt = findNewlineInWindow(window, index)
            if (foundAt != -1L) break
            index = window.globalEndIndex + 1
        }

        // Reached EOF with no data, return
        if (lineStartIndex > stream.endIndex) break

        // Reached EOF, consume remaining as a line
        if (foundAt == -1L) foundAt = stream.endIndex + 1

        val nextStart = foundAt + 1

        // Handle CLRF
        if (foundAt > 0 && stream[foundAt - 1] == '\r'.toByte()) foundAt -= 1

        val lineEndIndexInclusive = foundAt - 1
        if (lineStartIndex >= stream.startIndex && (lineEndIndexInclusive - lineStartIndex) >= 0) {
            val window = stream.windowFor(lineStartIndex)
            if (window === stream.windowFor(lineEndIndexInclusive)) {
                // slice endIndex is exclusive
                yield(window.slice.slice((lineStartIndex - window.globalStartIndex).toInt(),
                        (lineEndIndexInclusive - window.globalStartIndex + 1).toInt()))
            } else {
                val tmpBuffer = ByteArray((lineEndIndexInclusive - lineStartIndex + 1).toInt())
                stream.copyTo(tmpBuffer, lineStartIndex, lineEndIndexInclusive)
                yield(tmpBuffer.asSlice())
            }
        }
        lineStartIndex = nextStart
    }
}
