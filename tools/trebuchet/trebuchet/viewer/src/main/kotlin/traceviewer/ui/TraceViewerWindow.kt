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

import traceviewer.ui.builders.MenuBar
import traceviewer.ui.components.ChooseTrace
import trebuchet.io.BufferProducer
import trebuchet.io.DataSlice
import trebuchet.io.asSlice
import trebuchet.model.Model
import trebuchet.task.ImportTask
import trebuchet.util.PrintlnImportFeedback
import java.awt.Dimension
import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import javax.swing.*


class TraceViewerWindow {

    private var _model: Model? = null
    val frame = JFrame("TraceViewer")

    init {
        frame.defaultCloseOperation = JFrame.EXIT_ON_CLOSE
        frame.size = Dimension(1400, 1000)
        frame.jMenuBar = createMenuBar()
        frame.contentPane = ChooseTrace { file -> import(file) }
        frame.isLocationByPlatform = true
        frame.isVisible = true
    }

    constructor(file: File) {
        import(file)
    }

    private fun createMenuBar(): JMenuBar {
        return MenuBar {
            menu("File") {
                item("Open") {
                    action { ChooseTrace.selectFile(frame.location) { import(it) } }
                }
            }
        }
    }

    fun import(file: File) {
        object : SwingWorker<Model, Void>() {
            override fun doInBackground(): Model {
                val progress = ProgressMonitorInputStream(frame, "Importing ${file.name}",
                        FileInputStream(file))
                progress.progressMonitor.millisToDecideToPopup = 0
                progress.progressMonitor.millisToPopup = 0
                val stream = BufferedInputStream(progress)

                val task = ImportTask(PrintlnImportFeedback())
                val model = task.import(object : BufferProducer {
                    override fun next(): DataSlice? {
                        val buffer = ByteArray(2 * 1024 * 1024)
                        val read = stream.read(buffer)
                        if (read == -1) return null
                        return buffer.asSlice(read)
                    }

                    override fun close() {
                        stream.close()
                    }
                })
                println("Import finished")
                return model
            }

            override fun done() {
                println("Done called")
                model = get()
            }
        }.execute()
    }

    var model: Model?
        get() = _model
        set(model) {
            _model = model
            if (model != null) {
                val boost = model.duration * .15
                val zoom = RenderState(model.beginTimestamp - boost,
                        model.endTimestamp + boost,
                        frame.size.width - LayoutConstants.RowLabelWidth)
                val timelineView = TimelineView(model, zoom)
                frame.contentPane = timelineView
                timelineView.requestFocus()
                frame.revalidate()
                frame.repaint()
            }
        }
}