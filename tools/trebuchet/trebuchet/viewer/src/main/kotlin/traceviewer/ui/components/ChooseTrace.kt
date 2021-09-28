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

package traceviewer.ui.components

import java.awt.FileDialog
import java.awt.Frame
import java.awt.Point
import java.io.File
import javax.swing.JLabel
import javax.swing.JPanel

class ChooseTrace(val listener: (File) -> Unit) : JPanel() {
    companion object {
        fun selectFile(location: Point, fileSelected: (File) -> Unit) {
            val fd = FileDialog(null as Frame?)
            fd.setLocation(location.x + 100, location.y + 50)
            fd.isVisible = true
            val filename = fd.file
            if (filename != null) {
                val file = File(fd.directory, filename)
                if (file.exists()) {
                    fileSelected(file)
                }
            }
        }
    }

    init {
        add(JLabel("No file selected"))
    }
}