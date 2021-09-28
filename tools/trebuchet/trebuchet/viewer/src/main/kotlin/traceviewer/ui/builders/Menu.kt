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

package traceviewer.ui.builders

import java.awt.event.ActionEvent
import javax.swing.JMenu
import javax.swing.JMenuBar
import javax.swing.JMenuItem

class MenuBar private constructor(val base: JMenuBar) {
    companion object {
        operator fun invoke(init: MenuBar.() -> Unit): JMenuBar {
            val base = JMenuBar()
            MenuBar(base).init()
            return base
        }
    }

    fun menu(name: String, init: Menu.() -> Unit) {
        base.add(Menu(name, init))
    }
}

class Menu private constructor(val base: JMenu) {
    companion object {
        operator fun invoke(name: String, init: Menu.() -> Unit): JMenu {
            val base = JMenu(name)
            Menu(base).init()
            return base
        }
    }

    fun item(name: String, init: MenuItem.() -> Unit) {
        base.add(MenuItem(name, init))
    }
}

class MenuItem private constructor(val base: JMenuItem) {
    companion object {
        operator fun invoke(name: String, init: MenuItem.() -> Unit): JMenuItem {
            val base = JMenuItem(name)
            MenuItem(base).init()
            return base
        }
    }

    fun action(listener: (ActionEvent) -> Unit) {
        base.addActionListener(listener)
    }
}