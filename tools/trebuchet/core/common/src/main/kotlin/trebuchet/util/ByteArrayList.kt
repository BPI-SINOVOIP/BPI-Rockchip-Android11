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

package trebuchet.util

import trebuchet.io.DataSlice
import trebuchet.io.asSlice

class ByteArrayList constructor(initialCapacity: Int = 10) {

    var size: Int = 0
        private set

    var capacity: Int
        get() = array.size
        set(value) {
            if (value > capacity) {
                array = array.copyOf(maxOf(value, (capacity * 1.5).toInt()))
            }
        }

    private var array: ByteArray = ByteArray(initialCapacity)

    operator fun get(index: Int): Byte {
        return array[index]
    }

    fun put(b: Byte) {
        capacity = size + 1
        array[size] = b
        size++
    }

    fun put(buf: ByteArray, offset: Int, length: Int) {
        capacity = size + length
        System.arraycopy(buf, offset, array, size, length)
        size += length
    }

    fun put(slice: DataSlice) {
        put(slice.buffer, slice.startIndex, slice.length)
    }

    fun reset(initialCapacity: Int = 10): DataSlice {
        val slice = array.asSlice(size)
        array = ByteArray(initialCapacity)
        size = 0
        return slice
    }
}