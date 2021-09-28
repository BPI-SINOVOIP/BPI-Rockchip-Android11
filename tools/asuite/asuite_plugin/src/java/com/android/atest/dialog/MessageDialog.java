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
package com.android.atest.dialog;

import com.android.atest.Constants;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.ui.DialogWrapper;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import javax.swing.*;

/** The UI of a modal dialog box which shows the summary after Atest execution finishes. */
public class MessageDialog extends DialogWrapper {

    private JPanel mDialogPanel;
    private JTextPane mTestResult;
    private JScrollPane mScrollPane;

    /**
     * Creates modal {@code DialogWrapper} that can be parent for other windows. The current active
     * window will be the dialog's parent.
     *
     * @param project parent window for the dialog will be calculated based on the focused window
     *     for the specified {@code project}. This parameter can be {@code null}. In this case
     *     parent window will be suggested based on the current focused window.
     * @throws IllegalStateException if the dialog is invoked not on the event dispatch thread.
     * @see DialogWrapper#DialogWrapper(Project, boolean)
     */
    public MessageDialog(@Nullable Project project) {
        super(project);
        init();
        setTitle(Constants.ATEST_NAME);
    }

    /**
     * Sets the message in the dialog.
     *
     * @param message the message to be shown.
     */
    public void setMessage(String message) {
        mTestResult.setText(message);
    }

    /**
     * Overrides createActions to only show OK button.
     *
     * @return dialog actions.
     */
    @NotNull
    @Override
    protected Action[] createActions() {
        return new Action[] {getOKAction()};
    }

    /**
     * Creates panel with dialog options. Options panel is located at the center of the dialog's
     * content pane. The implementation can return {@code null} value. In this case there will be no
     * options panel.
     *
     * @return center panel.
     */
    @Nullable
    @Override
    protected JComponent createCenterPanel() {
        return mDialogPanel;
    }

    /**
     * Shows a dialog with the message.
     *
     * @param message the message to be shown.
     */
    public static void showMessageDialog(String message) {
        MessageDialog dialog = new MessageDialog(null);
        dialog.setMessage(message);
        dialog.show();
    }
}
