/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.tools.metalava.doclava1

class SourcePositionInfo(
    private val file: String,
    private val line: Int
) : Comparable<SourcePositionInfo> {
    override fun toString(): String {
        return "$file:$line"
    }

    override fun compareTo(other: SourcePositionInfo): Int {
        val r = file.compareTo(other.file)
        return if (r != 0) r else line - other.line
    }

    companion object {
        val UNKNOWN = SourcePositionInfo("(unknown)", 0)

        /**
         * Given this position and str which occurs at that position, as well as str an index into str,
         * find the SourcePositionInfo.
         *
         * @throws StringIndexOutOfBoundsException if index &gt; str.length()
         */
        fun add(
            that: SourcePositionInfo?,
            str: String,
            index: Int
        ): SourcePositionInfo? {
            if (that == null) {
                return null
            }
            var line = that.line
            var prev = 0.toChar()
            for (i in 0 until index) {
                val c = str[i]
                if (c == '\r' || c == '\n' && prev != '\r') {
                    line++
                }
                prev = c
            }
            return SourcePositionInfo(that.file, line)
        }
    }
}