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

import com.android.atest.AtestUtils;
import com.android.atest.Constants;
import com.android.atest.toolWindow.AtestToolWindow;
import com.android.atest.widget.AtestNotification;
import com.intellij.build.BuildTextConsoleView;
import com.intellij.execution.ExecutionException;
import com.intellij.execution.ExecutionManager;
import com.intellij.execution.configurations.PtyCommandLine;
import com.intellij.execution.executors.DefaultRunExecutor;
import com.intellij.execution.process.KillableColoredProcessHandler;
import com.intellij.execution.process.ProcessListener;
import com.intellij.execution.ui.RunContentDescriptor;
import com.intellij.notification.Notifications;
import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.project.Project;
import org.jetbrains.annotations.NotNull;

import javax.swing.*;
import java.awt.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;

/** A manager to handle command. */
public class CommandRunner {

    private static final Logger LOG = Logger.getInstance(CommandRunner.class);
    private static final String ATEST_COMMAND_PREFIX =
            "export USER_FROM_TOOL=\"IntelliJ_Atest_plugin\" && source build/envsetup.sh && lunch ";
    private static KillableColoredProcessHandler sProcessHandler;
    private PtyCommandLine mCommand;
    private ProcessListener mProcessListener;
    private Project mProject;

    /**
     * Initializes CommandRunner by the command.
     *
     * @param cmds the command to run.
     * @param workPath the work path to run the command.
     */
    public CommandRunner(ArrayList<String> cmds, String workPath) {
        mCommand = new PtyCommandLine(cmds);
        mCommand.setCharset(StandardCharsets.UTF_8);
        mCommand.setWorkDirectory(workPath);
    }

    /**
     * Initializes CommandRunner by Atest lunch target and test target.
     *
     * @param lunchTarget the Atest lunch target.
     * @param testTarget the Atest test target.
     * @param workPath the work path to run the command.
     * @param toolWindow an AtestToolWindow to display the output.
     * @param project the current intelliJ project.
     */
    public CommandRunner(
            String lunchTarget,
            String testTarget,
            String workPath,
            @NotNull AtestToolWindow toolWindow,
            @NotNull Project project)
            throws IllegalArgumentException {
        if (AtestUtils.checkEmpty(lunchTarget, testTarget, workPath)) {
            throw new IllegalArgumentException();
        }
        mProject = project;
        StringBuffer commandBuffer = new StringBuffer(ATEST_COMMAND_PREFIX);
        String atestCommand =
                commandBuffer
                        .append(lunchTarget)
                        .append(" && atest ")
                        .append(testTarget)
                        .toString();
        LOG.info("Atest command: " + atestCommand + ", work path: " + workPath);

        String[] commandArray = {"/bin/bash", "-c", atestCommand};
        ArrayList<String> cmds = new ArrayList<>(Arrays.asList(commandArray));
        mCommand = new PtyCommandLine(cmds);
        mCommand.setCharset(StandardCharsets.UTF_8);
        mCommand.setWorkDirectory(workPath);
        mProcessListener = new AtestProcessListener(toolWindow);
    }

    /**
     * Sets the process listener.
     *
     * @param processListener a processListener handle the output.
     */
    public void setProcessListener(ProcessListener processListener) {
        mProcessListener = processListener;
    }

    /**
     * Executes the command in processHandler.
     *
     * <p>Execute this method when caller is ready to access linux command.
     */
    public void run() {
        try {
            stopProcess(mProject);
            sProcessHandler = new KillableColoredProcessHandler(mCommand);
            if (mProcessListener != null) {
                sProcessHandler.addProcessListener(mProcessListener);
            }
            launchConsole();
            sProcessHandler.startNotify();
        } catch (ExecutionException e) {
            Notifications.Bus.notify(new AtestNotification("Command execution failed."));
            LOG.error("Command executes fail: " + mCommand.getCommandLineString());
        }
    }

    /** Launch the console view of current process. */
    private void launchConsole() {
        BuildTextConsoleView consoleView = new BuildTextConsoleView(mProject);
        JPanel panel = new JPanel(new BorderLayout());
        consoleView.attachToProcess(sProcessHandler);
        panel.add(consoleView.getComponent(), BorderLayout.CENTER);
        panel.updateUI();
        RunContentDescriptor contentDescriptor =
                new RunContentDescriptor(consoleView, sProcessHandler, panel, Constants.ATEST_NAME);
        ExecutionManager.getInstance(mProject)
                .getContentManager()
                .showRunContent(DefaultRunExecutor.getRunExecutorInstance(), contentDescriptor);
    }

    /**
     * Stops the current process.
     *
     * @param project the current intelliJ project.
     */
    public static void stopProcess(Project project) {
        if (sProcessHandler != null) {
            RunContentDescriptor contentDescriptor =
                    ExecutionManager.getInstance(project)
                            .getContentManager()
                            .findContentDescriptor(
                                    DefaultRunExecutor.getRunExecutorInstance(), sProcessHandler);
            ExecutionManager.getInstance(project)
                    .getContentManager()
                    .hideRunContent(DefaultRunExecutor.getRunExecutorInstance(), contentDescriptor);
            sProcessHandler.killProcess();
            sProcessHandler = null;
        }
    }
}
