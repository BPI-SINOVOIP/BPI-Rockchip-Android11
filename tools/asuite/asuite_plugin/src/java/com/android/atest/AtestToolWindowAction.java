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
package com.android.atest;

import com.android.atest.widget.AtestNotification;
import com.intellij.notification.Notifications;
import com.intellij.openapi.actionSystem.AnActionEvent;
import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.wm.ToolWindow;
import com.intellij.openapi.wm.ToolWindowManager;
import org.jetbrains.annotations.NotNull;

/** An Action to launch Atest tool window. */
public class AtestToolWindowAction extends com.intellij.openapi.actionSystem.AnAction {

    private static final Logger LOG = Logger.getInstance(AtestToolWindowAction.class);

    /**
     * Launches the atest tool window.
     *
     * @param event carries information on the invocation place.
     */
    @Override
    public void actionPerformed(@NotNull AnActionEvent event) {
        ToolWindow AtestTW =
                ToolWindowManager.getInstance(event.getProject())
                        .getToolWindow(Constants.ATEST_TOOL_WINDOW);
        if (AtestTW == null) {
            Notifications.Bus.notify(new AtestNotification(Constants.ATEST_WINDOW_FAIL));
            LOG.error("Can't get Atest tool window.");
            return;
        }

        if (AtestTW.isVisible()) {
            AtestTW.hide(null);
        } else {
            AtestTW.show(null);
        }
    }
}
