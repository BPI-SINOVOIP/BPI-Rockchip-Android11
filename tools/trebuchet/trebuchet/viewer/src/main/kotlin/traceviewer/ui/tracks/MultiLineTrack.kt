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

package traceviewer.ui.tracks

import traceviewer.ui.ThemeColors
import traceviewer.ui.RenderState
import trebuchet.model.ThreadModel
import trebuchet.model.base.SliceGroup
import javax.swing.BorderFactory
import javax.swing.BoxLayout
import javax.swing.JPanel

class MultiLineTrack(thread: ThreadModel, renderState: RenderState) : JPanel() {
    init {
        isOpaque = true
        layout = BoxLayout(this, BoxLayout.PAGE_AXIS)
        border = BorderFactory.createEmptyBorder(4, 0, 0, 0)
        background = ThemeColors.SliceTrackBackground
        if (thread.schedSlices.isNotEmpty()) {
            add(SchedTrack(thread.schedSlices, renderState))
        }
        val rows = RowCreater(thread.slices).rows
        rows.forEach {
            add(SliceTrack(it, renderState))
        }
    }

    class RowCreater constructor(slices: List<SliceGroup>) {
        val rows = mutableListOf<MutableList<SliceGroup>>()
        var index = -1

        init {
            push()
            slices.forEach { addSlice(it) }
        }

        val row: MutableList<SliceGroup> get() = rows[index]

        fun push() {
            index++
            while (index >= rows.size) rows.add(mutableListOf())
        }

        fun pop() {
            index--
        }

        fun addSlice(slice: SliceGroup) {
            row.add(slice)
            if (slice.children.isNotEmpty()) {
                push()
                slice.children.forEach { addSlice(it) }
                pop()
            }
        }
    }
}