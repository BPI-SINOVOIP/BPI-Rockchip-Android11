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
package com.android.atest.run;

import com.android.atest.AtestUtils;
import com.android.atest.Constants;
import com.android.atest.commandAdapter.CommandRunner;
import com.android.atest.dialog.MessageDialog;
import com.android.atest.toolWindow.AtestToolWindow;
import com.android.atest.widget.AtestNotification;
import com.google.common.base.Strings;
import com.intellij.execution.ExecutionException;
import com.intellij.execution.Executor;
import com.intellij.execution.configurations.*;
import com.intellij.execution.runners.ExecutionEnvironment;
import com.intellij.notification.Notifications;
import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.options.SettingsEditor;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.util.InvalidDataException;
import com.intellij.openapi.util.text.StringUtil;
import com.intellij.openapi.wm.ToolWindow;
import com.intellij.openapi.wm.ToolWindowManager;
import org.jdom.Element;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

/** Runs configurations which can be managed by a user and displayed in the UI. */
public class AtestRunConfiguration extends LocatableConfigurationBase {

    private static final Logger LOG = Logger.getInstance(AtestRunConfiguration.class);
    public static final String TEST_TARGET_KEY = "testTarget";
    public static final String LUNCH_TARGET_KEY = "lunchTarget";
    private String mTestTarget = "";
    private String mLunchTarget = "";

    protected AtestRunConfiguration(Project project, ConfigurationFactory factory, String name) {
        super(project, factory, name);
    }

    /**
     * Reads the run configuration settings from the file system.
     *
     * @param element an Element object to read.
     * @throws InvalidDataException if the data is invalid.
     */
    @Override
    public void readExternal(@NotNull Element element) throws InvalidDataException {
        Element child = element.getChild(TEST_TARGET_KEY);
        if (hasValue(child)) {
            mTestTarget = child.getTextTrim();
        }
        child = element.getChild(LUNCH_TARGET_KEY);
        if (hasValue(child)) {
            mLunchTarget = child.getTextTrim();
        }
    }

    /**
     * Checks the element has value or not.
     *
     * @param element an Element object to check.
     * @return true if the element has value.
     */
    private boolean hasValue(Element element) {
        return element != null && !Strings.isNullOrEmpty(element.getTextTrim());
    }

    /**
     * Stores the run configuration settings at file system.
     *
     * @param element an Element object to write.
     */
    @Override
    public void writeExternal(@NotNull Element element) {
        setElementChild(element, TEST_TARGET_KEY, mTestTarget);
        setElementChild(element, LUNCH_TARGET_KEY, mLunchTarget);
        super.writeExternal(element);
    }

    /**
     * Sets the child element for an element.
     *
     * @param element an Element object to set.
     * @param key the key of the child Element object.
     * @param value the value of the child Element object.
     */
    private void setElementChild(@NotNull Element element, String key, String value) {
        Element child = new Element(key);
        child.setText(value);
        element.addContent(child);
    }

    /**
     * Returns the UI control for editing the run configuration settings. If additional control over
     * validation is required, the object returned from this method may also implement {@link
     * com.intellij.execution.impl.CheckableRunConfigurationEditor}. The returned object can also
     * implement {@link com.intellij.openapi.options.SettingsEditorGroup} if the settings it
     * provides need to be displayed in multiple tabs.
     *
     * @return Atest settings editor component.
     */
    @NotNull
    @Override
    public SettingsEditor<? extends RunConfiguration> getConfigurationEditor() {
        return new AtestSettingsEditor();
    }

    /**
     * Checks whether the run configuration settings are valid. Note that this check may be invoked
     * on every change (i.e. after each character typed in an input field).
     *
     * @throws RuntimeConfigurationException if the configuration settings contain a non-fatal
     *     problem which the user should be warned about but the execution should still be allowed.
     */
    @Override
    public void checkConfiguration() throws RuntimeConfigurationException {
        if (StringUtil.isEmptyOrSpaces(mTestTarget)) {
            throw new RuntimeConfigurationWarning("Test target not defined");
        }
        if (StringUtil.isEmptyOrSpaces(mLunchTarget)) {
            throw new RuntimeConfigurationWarning("Lunch target not defined");
        }
    }

    /**
     * Prepares for executing a specific instance of the run configuration.
     *
     * <p>Since Atest plugin uses tool window to output the message. It always returns null to
     * handle the process inside Atest plugin.
     *
     * @param executor the execution mode selected by the user (run, debug, profile etc.)
     * @param executionEnvironment the environment object containing additional settings for
     *     executing the configuration.
     * @throws ExecutionException if exception happens when executing.
     * @return the RunProfileState describing the process which is about to be started, or null if
     *     it won't start the process.
     */
    @Nullable
    @Override
    public RunProfileState getState(
            @NotNull Executor executor, @NotNull ExecutionEnvironment executionEnvironment)
            throws ExecutionException {
        AtestToolWindow atestToolWindow =
                AtestToolWindow.getInstance(executionEnvironment.getProject());

        if (!showAtestTW(executionEnvironment.getProject())) {
            return null;
        }
        String workPath =
                AtestUtils.getAndroidRoot(executionEnvironment.getProject().getBasePath());
        atestToolWindow.setLunchTarget(mLunchTarget);
        atestToolWindow.setTestTarget(mTestTarget);
        try {
            CommandRunner runner =
                    new CommandRunner(
                            mLunchTarget,
                            mTestTarget,
                            workPath,
                            atestToolWindow,
                            executionEnvironment.getProject());
            runner.run();
        } catch (IllegalArgumentException exception) {
            String errorMessage = AtestUtils.checkError(mLunchTarget, mTestTarget, workPath);
            MessageDialog.showMessageDialog(errorMessage);
        }
        return null;
    }

    /**
     * Shows the Atest tool window.
     *
     * @param project the project in which the run configuration will be used.
     * @return true if show Atest tool window successful.
     */
    private boolean showAtestTW(@NotNull Project project) {
        boolean result = false;
        ToolWindow atestTWController =
                ToolWindowManager.getInstance(project).getToolWindow(Constants.ATEST_TOOL_WINDOW);
        if (atestTWController != null) {
            atestTWController.show(null);
            result = true;
        } else {
            Notifications.Bus.notify(new AtestNotification(Constants.ATEST_WINDOW_FAIL));
            LOG.error("Can't get Atest tool window.");
        }
        return result;
    }

    public String getTestTarget() {
        return mTestTarget;
    }

    public void setTestTarget(@NotNull String testTarget) {
        mTestTarget = testTarget;
    }

    public String getLaunchTarget() {
        return mLunchTarget;
    }

    public void setLaunchTarget(@NotNull String launchTarget) {
        mLunchTarget = launchTarget;
    }
}
