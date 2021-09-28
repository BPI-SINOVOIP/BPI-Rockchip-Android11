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

data class DataSlice(var buffer: ByteArray,
                     var startIndex: Int,
                     var endIndex: Int) {

    companion object {
        val EmptyBuffer = ByteArray(0)
    }

    constructor() : this(EmptyBuffer, 0, 0)
    constructor(buffer: ByteArray) : this(buffer, 0, buffer.size)

    @Suppress("NOTHING_TO_INLINE")
    inline operator fun get(i: Int): Byte = buffer[startIndex + i]

    @Suppress("NOTHING_TO_INLINE")
    inline fun slice(startIndex: Int, endIndex: Int = this.endIndex,
                     dest: DataSlice = DataSlice()): DataSlice {
        dest.set(buffer, this.startIndex + startIndex, this.startIndex + endIndex)
        return dest
    }

    inline val length: Int get() = endIndex - startIndex

    fun set(buffer: ByteArray, startIndex: Int, endIndex: Int) {
        this.buffer = buffer
        this.startIndex = startIndex
        this.endIndex = endIndex
        _hasCachedHashCode = false
    }

    override fun equals(other: Any?): Boolean {
        if (other === this) return true
        if (other !is DataSlice) return false
        if (length != other.length) return false
        if (_hasCachedHashCode && other._hasCachedHashCode
                && hashCode() != other.hashCode()) return false
        var myIndex = startIndex
        var otherIndex = other.startIndex
        while (myIndex < endIndex) {
            if (buffer[myIndex] != other.buffer[otherIndex]) {
                return false
            }
            myIndex++
            otherIndex++
        }
        return true
    }

    private var _cachedHashCode: Int = 1
    private var _hasCachedHashCode: Boolean = false
    override fun hashCode(): Int {
        if (_hasCachedHashCode) return _cachedHashCode
        var hash = 1
        for (i in startIndex..endIndex-1) {
            hash = 31 * hash + buffer[i].toInt()
        }
        _cachedHashCode = hash
        _hasCachedHashCode = true
        return hash
    }

    override fun toString() = String(buffer, startIndex, length)

    fun compact(): DataSlice {
        if (length - buffer.size < 50) {
            return this
        }
        return DataSlice(buffer.copyOfRange(startIndex, endIndex))
    }
}

fun ByteArray.asSlice(length: Int = this.size) = DataSlice(this, 0, length)
fun String.asSlice() = DataSlice(this.toByteArray())

fun DataSlice.asText(): CharSequence {
    val data = this
    return object : CharSequence {
        override val length: Int get() = data.length

        override fun get(index: Int): Char {
            return data[index].toChar()
        }

        override fun subSequence(startIndex: Int, endIndex: Int): CharSequence {
            return data.slice(startIndex, endIndex).asText()
        }
    }
}