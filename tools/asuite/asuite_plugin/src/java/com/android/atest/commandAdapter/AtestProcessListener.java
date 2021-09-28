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
package com.android.atest.commandAdapter;

import com.android.atest.toolWindow.AtestToolWindow;
import com.intellij.execution.process.ProcessEvent;
import com.intellij.execution.process.ProcessListener;
import com.intellij.execution.process.ProcessOutputTypes;
import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.util.Key;
import org.jetbrains.annotations.NotNull;

/** Atest process listener. */
public class AtestProcessListener implements ProcessListener {

    private static final Logger LOG = Logger.getInstance(AtestProcessListener.class);
    private final AtestToolWindow mToolWindow;
    private StringBuffer mOutputBuffer = new StringBuffer();

    public AtestProcessListener(@NotNull AtestToolWindow toolWindow) {
        mToolWindow = toolWindow;
    }

    /**
     * Callback when the process starts notifying.
     *
     * @param event a ProcessEvent object.
     */
    @Override
    public void startNotified(@NotNull ProcessEvent event) {
        mToolWindow.setRunEnable(false);
    }

    /**
     * Callback when the process terminates.
     *
     * @param event a ProcessEvent object.
     */
    @Override
    public void processTerminated(@NotNull ProcessEvent event) {
        mToolWindow.setRunEnable(true);
    }

    /**
     * Callback when the process is going to terminate.
     *
     * @param event a ProcessEvent object.
     * @param willBeDestroyed true if process will be destroyed.
     */
    @Override
    public void processWillTerminate(@NotNull ProcessEvent event, boolean willBeDestroyed) {}

    /**
     * Callback when the process outputs messages.
     *
     * @param event a ProcessEvent object.
     * @param outputType output type defined in ProcessOutputTypes.
     */
    @Override
    public void onTextAvailable(@NotNull ProcessEvent event, @NotNull Key outputType) {
        if (outputType == ProcessOutputTypes.STDERR) {
            LOG.warn(event.getText());
        }
    }
}
