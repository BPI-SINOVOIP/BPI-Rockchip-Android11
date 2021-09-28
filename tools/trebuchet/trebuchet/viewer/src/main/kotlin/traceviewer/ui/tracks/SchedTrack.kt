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
import trebuchet.model.SchedSlice
import trebuchet.model.SchedulingState
import java.awt.Color
import java.awt.FontMetrics
import java.awt.Graphics

class SchedTrack(slices: List<SchedSlice>, renderState: RenderState)
        : SliceTrack<SchedSlice>(slices, renderState, trackHeight = LayoutConstants.SchedTrackHeight) {

    override fun colorFor(slice: SchedSlice): Color {
        return when (slice.state) {
            SchedulingState.WAKING,
            SchedulingState.RUNNABLE -> Runnable
            SchedulingState.RUNNING -> Running
            SchedulingState.SLEEPING -> Sleeping
            else -> UnintrSleep
        }
    }

    override fun drawLabel(slice: SchedSlice, g: Graphics, metrics: FontMetrics, x: Int, y: Int, width: Int) {
    }

    companion object Colors {
        val Runnable = Color.decode("#03A9F4")
        val Running = Color.decode("#4CAF50")
        val Sleeping = Color.decode("#424242")
        val UnintrSleep = Color.decode("#A71C1C")
    }
}