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

import com.android.atest.AtestUtils;
import com.android.atest.Constants;
import com.android.atest.commandAdapter.CommandRunner;
import com.android.atest.dialog.MessageDialog;
import com.android.atest.widget.AtestFastInputController;
import com.android.atest.widget.AtestNotification;
import com.intellij.notification.Notifications;
import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.wm.ToolWindow;
import com.intellij.openapi.wm.ToolWindowManager;
import com.intellij.openapi.wm.ex.ToolWindowEx;
import com.intellij.openapi.wm.impl.ToolWindowImpl;
import org.jetbrains.annotations.NotNull;

import javax.swing.*;
import java.awt.*;

/** UI content of Atest tool window. */
public class AtestToolWindow {

    private static final int INITIAL_WIDTH = 1000;
    private static final Logger LOG = Logger.getInstance(AtestToolWindow.class);
    private static AtestToolWindow sAtestToolWindowInstance;

    private JPanel mAtestToolWindowPanel;
    private JScrollPane mScorll;
    private JLabel mAtestlabel;
    private JTextField mLunchTarget;
    private JCheckBox mRunOnHost;
    private JCheckBox mTestMapping;
    private JCheckBox mSkipBuild;
    private JButton mRunButton;
    private JComboBox mTestTarget;
    private JButton mStopButton;
    private Project mProject;

    /**
     * Initializes AtestToolWindow with ToolWindow and Project.
     *
     * @param toolWindow a child window of the IDE used to display information.
     * @param project the current intelliJ project.
     */
    private AtestToolWindow(ToolWindow toolWindow, Project project) {
        mProject = project;
        String basePath = project.getBasePath();
        setInitialWidth((ToolWindowEx) toolWindow);
        setRunButton(basePath);
        setStopButton();
        initTestTarget(basePath);
        AtestFastInputController fastInputController =
                new AtestFastInputController(mTestTarget, mRunOnHost, mTestMapping, mSkipBuild);
        fastInputController.linkCheckBoxWithTestTarget();
    }

    /**
     * Initializes AtestToolWindow instance.
     *
     * <p>Because the AtestToolWindow should be modified when the project is changed. It can't be
     * singleton. This initializer is used by the ToolWindowFactory. We use IntelliJ's mechanism to
     * make sure AtestToolWindow's instance can always follow the project.
     *
     * @param toolWindow a child window of the IDE used to display information.
     * @param project the current intelliJ project.
     * @return the AtestToolWindow instance.
     */
    @NotNull
    public static AtestToolWindow initAtestToolWindow(ToolWindow toolWindow, Project project) {
        sAtestToolWindowInstance = new AtestToolWindow(toolWindow, project);
        return sAtestToolWindowInstance;
    }

    /**
     * Gets AtestToolWindow instance by project.
     *
     * <p>When using ToolWindowManager to get atest tool window, it will initialize the
     * AtestToolWindow by {@link AtestToolWindowFactory} asynchronously. We use
     * ensureContentInitialized from ToolWindowImpl to ensure the AtestToolWindow has been
     * initialized.
     *
     * @param project the current intelliJ project.
     * @return the AtestToolWindow instance.
     */
    @NotNull
    public static AtestToolWindow getInstance(@NotNull Project project) {
        ToolWindow AtestTW =
                ToolWindowManager.getInstance(project).getToolWindow(Constants.ATEST_TOOL_WINDOW);
        if (AtestTW instanceof ToolWindowImpl) {
            ((ToolWindowImpl) AtestTW).ensureContentInitialized();
        }

        if (sAtestToolWindowInstance == null) {
            LOG.error("AtestToolWindowInstance is null when getting instance by project");
            Notifications.Bus.notify(new AtestNotification(Constants.ATEST_WINDOW_FAIL));
        }
        return sAtestToolWindowInstance;
    }

    /**
     * Initializes mTestTarget.
     *
     * @param basePath a string that represents current project's base path.
     */
    private void initTestTarget(String basePath) {
        mTestTarget.setEditable(true);
        if (AtestUtils.hasTestMapping(basePath)) {
            mTestTarget.setSelectedItem(basePath);
        }
    }

    /**
     * Sets the initial width of the tool window.
     *
     * @param toolWindowEx a toolWindow which has more methods.
     */
    private void setInitialWidth(@NotNull ToolWindowEx toolWindowEx) {
        int width = toolWindowEx.getComponent().getWidth();
        if (width < INITIAL_WIDTH) {
            toolWindowEx.stretchWidth(INITIAL_WIDTH - width);
        }
    }

    /** Initializes the run button. */
    private void setRunButton(String basePath) {
        // When command running, the run button will be set to disable, then the focus will set to
        // next object. Set run button not focusable to prevent it.
        mRunButton.setFocusable(false);
        mRunButton.addActionListener(
                e -> {
                    String lunchTarget = mLunchTarget.getText();
                    String testTarget = mTestTarget.getEditor().getItem().toString();
                    String workPath = AtestUtils.getAndroidRoot(basePath);
                    try {
                        CommandRunner runner =
                                new CommandRunner(
                                        lunchTarget,
                                        testTarget,
                                        workPath,
                                        AtestToolWindow.this,
                                        mProject);
                        runner.run();
                    } catch (IllegalArgumentException exception) {
                        String errorMessage =
                                AtestUtils.checkError(lunchTarget, testTarget, workPath);
                        MessageDialog.showMessageDialog(errorMessage);
                    }
                });
    }

    /** Initializes the stop button. */
    private void setStopButton() {
        mStopButton.addActionListener(
                e -> {
                    CommandRunner.stopProcess(mProject);
                });
    }

    /** Scrolls the output window scroll bar to the bottom. */
    public void scrollToEnd() {
        JScrollBar vertical = mScorll.getVerticalScrollBar();
        vertical.setValue(vertical.getMaximum());
    }

    /**
     * Enables (or disables) the run button.
     *
     * @param isEnable true to enable the run button, otherwise disable it.
     */
    public void setRunEnable(boolean isEnable) {
        mRunButton.setEnabled(isEnable);
    }

    /**
     * Gets the UI panel of Atest tool window.
     *
     * @return the JPanel of Atest tool window.
     */
    public JPanel getContent() {
        return mAtestToolWindowPanel;
    }

    /**
     * Sets the test target of Atest tool window.
     *
     * @param target the test target of Atest tool window.
     */
    public void setTestTarget(@NotNull String target) {
        mTestTarget.setSelectedItem(target);
    }

    /**
     * Sets the lunch target of Atest tool window.
     *
     * @param target the lunch target of Atest tool window.
     */
    public void setLunchTarget(@NotNull String target) {
        mLunchTarget.setText(target);
    }
}
