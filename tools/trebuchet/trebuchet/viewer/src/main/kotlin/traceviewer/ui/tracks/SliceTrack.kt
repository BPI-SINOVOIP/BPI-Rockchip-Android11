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

import traceviewer.ui.LayoutConstants
import traceviewer.ui.RenderState
import trebuchet.model.base.Slice
import java.awt.Color
import java.awt.Dimension
import java.awt.FontMetrics
import java.awt.Graphics
import javax.swing.JComponent

open class SliceTrack<in T : Slice>(private val slices: List<T>, private val renderState: RenderState,
                                    trackHeight: Int = LayoutConstants.TrackHeight) : JComponent() {

    init {
        preferredSize = Dimension(0, trackHeight)
        size = preferredSize
    }

    override fun paintComponent(g: Graphics?) {
        if (g == null) return

        val scale = renderState.scale
        val panX = renderState.panX
        val y = 0
        val height = height
        var x: Int
        var width: Int

        val metrics = g.fontMetrics
        var ty = metrics.ascent
        ty += (height - ty) / 2

        slices.forEach {
            x = ((it.startTime - panX) * scale).toInt()
            val scaledWidth = (it.endTime - it.startTime) * scale
            width = maxOf(scaledWidth.toInt(), 1)
            if (x + width > 0 && x < this.width) {
                var color = colorFor(it)
                if (scaledWidth < 1) {
                    color = Color(color.red, color.green, color.blue,
                            maxOf((255 * scaledWidth).toInt(), 50))
                }
                g.color = color
                g.fillRect(x, y, width, height)

                if (height >= metrics.height) {
                    drawLabel(it, g, metrics, x, ty, width)
                }
            }
        }
    }

    open fun drawLabel(slice: T, g: Graphics, metrics: FontMetrics, x: Int, y: Int, width: Int) {
        var strLimit = 0
        var strWidth = 0
        while (strLimit < slice.name.length && strWidth <= width) {
            strWidth += metrics.charWidth(slice.name[strLimit])
            strLimit++
        }
        if (strWidth > width) strLimit--
        if (strLimit > 2) {
            g.color = textColor
            g.drawString(slice.name.substring(0, strLimit), x, y)
        }
    }

    open fun colorFor(slice: T): Color {
        return colors[Math.abs(slice.name.hashCode()) % colors.size]
    }

    private fun sliceAt(timestamp: Double) = slices.binarySearch {
        when {
            it.startTime > timestamp -> 1
            it.endTime < timestamp -> -1
            else -> 0
        }
    }.let { if (it < 0) null else slices[it] }

    companion object Style {
        private val colors = listOf(
                Color(0x0D47A1), Color(0x1565C0), Color(0x1976D2),
                Color(0x1A237E), Color(0x283593), Color(0x303F9F),
                Color(0x01579B), Color(0x006064), Color(0x004D40),
                Color(0x1B5E20), Color(0x37474F), Color(0x263238)
            )

        private val textColor = Color(0xff, 0xff, 0xff, 0x7f)
    }
}