/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.atest.toolWindow;

import com.intellij.openapi.project.Project;
import com.intellij.openapi.wm.ToolWindow;
import com.intellij.openapi.wm.ToolWindowFactory;
import com.intellij.ui.content.Content;
import com.intellij.ui.content.ContentFactory;
import org.jetbrains.annotations.NotNull;

/** Performs lazy initialization of Atest toolwindow registered in plugin.xml. */
public class AtestToolWindowFactory implements ToolWindowFactory {

    /**
     * Creates the Atest tool window content.
     *
     * @param project an object that represents an IntelliJ project.
     * @param toolWindow a child window of the IDE used to display information.
     */
    @Override
    public void createToolWindowContent(@NotNull Project project, @NotNull ToolWindow toolWindow) {
        AtestToolWindow atestToolWindow = AtestToolWindow.initAtestToolWindow(toolWindow, project);
        ContentFactory contentFactory = ContentFactory.SERVICE.getInstance();
        Content toolWindowContent =
                contentFactory.createContent(atestToolWindow.getContent(), "", true);
        toolWindow.getContentManager().addContent(toolWindowContent);
    }
}
