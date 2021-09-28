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

import trebuchet.model.Model
import java.awt.Dimension
import java.awt.Point
import java.awt.Rectangle
import java.awt.event.*
import javax.swing.*


class TimelineView(val model: Model, val renderState: RenderState) : JScrollPane(), MouseListener,
        MouseMotionListener {
    
    private val lastMousePosition = Point()

    init {
        val box = object : JPanel(), Scrollable {

            override fun getScrollableTracksViewportWidth(): Boolean {
                return true
            }

            override fun getScrollableTracksViewportHeight(): Boolean {
                return false
            }

            override fun getScrollableBlockIncrement(visibleRect: Rectangle, orientation: Int,
                                                     direction: Int): Int {
                return visibleRect.height - LayoutConstants.TrackHeight
            }

            override fun getScrollableUnitIncrement(visibleRect: Rectangle, orientation: Int,
                                                    direction: Int): Int {
                return LayoutConstants.TrackHeight
            }

            override fun getPreferredScrollableViewportSize(): Dimension {
                return preferredSize
            }

            init {
                layout = BoxLayout(this, BoxLayout.PAGE_AXIS)
            }
        }
        model.processes.values.filter { it.hasContent }.forEach {
            box.add(ProcessPanel(it, renderState))
        }
        setViewportView(box)
        addMouseListener(this)
        addMouseMotionListener(this)
        lastMousePosition.setLocation(width / 2, 0)
        renderState.listeners.add({ repaint() })
        // TODO: Add workaround for OSX 'defaults write -g ApplePressAndHoldEnabled -bool false'
        addAction('w', "zoomIn") {
            renderState.zoomBy(1.5, zoomFocalPointX)
        }
        addAction('s', "zoomOut") {
            renderState.zoomBy(1 / 1.5, zoomFocalPointX)
        }
        addAction('a', "panLeft") {
            renderState.pan((width * -.3).toInt())
        }
        addAction('d', "panRight") {
            renderState.pan((width * .3).toInt())
        }
    }

    fun addAction(key: Char, name: String, listener: (ActionEvent) -> Unit) {
        inputMap.put(KeyStroke.getKeyStroke(key), name)
        actionMap.put(name, object : AbstractAction(name) {
            override fun actionPerformed(event: ActionEvent) {
                listener.invoke(event)
            }
        })
    }

    override fun isFocusable() = true

    private val zoomFocalPointX: Int get() {
        return maxOf(0, minOf(this.width, lastMousePosition.x - LayoutConstants.RowLabelWidth))
    }

    override fun mouseReleased(e: MouseEvent) {
        
    }

    override fun mouseEntered(e: MouseEvent) {
        
    }

    override fun mouseClicked(e: MouseEvent) {
        
    }

    override fun mouseExited(e: MouseEvent) {
        
    }

    override fun mousePressed(e: MouseEvent) {
        
    }

    override fun mouseMoved(e: MouseEvent) {
        lastMousePosition.location = e.point
    }

    override fun mouseDragged(e: MouseEvent) {
        
    }
}