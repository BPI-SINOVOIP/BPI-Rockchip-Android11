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

package traceviewer.ui

import traceviewer.ui.tracks.MultiLineTrack
import trebuchet.model.ProcessModel
import java.awt.Dimension
import java.awt.GridBagConstraints
import java.awt.GridBagLayout
import javax.swing.JPanel

class ProcessPanel(val process: ProcessModel, renderState: RenderState) : JPanel(GridBagLayout()) {
    init {
        val constraints = GridBagConstraints()
        constraints.gridwidth = GridBagConstraints.REMAINDER
        constraints.anchor = GridBagConstraints.LINE_START
        constraints.fill = GridBagConstraints.HORIZONTAL
        add(ProcessLabel(process.name), constraints)
        process.threads.forEach {
            constraints.gridwidth = 1
            constraints.weightx = 0.0
            constraints.anchor = GridBagConstraints.PAGE_START
            add(RowLabel(it.name), constraints)
            constraints.weightx = 1.0
            constraints.gridwidth = GridBagConstraints.REMAINDER
            add(MultiLineTrack(it, renderState), constraints)
        }
        maximumSize = Dimension(Int.MAX_VALUE, preferredSize.height)
        isOpaque = true
        background = ThemeColors.RowLabelBackground
    }
}