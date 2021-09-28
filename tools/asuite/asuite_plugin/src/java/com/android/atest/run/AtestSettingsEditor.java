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

import com.android.atest.Constants;
import com.android.atest.widget.AtestFastInputController;
import com.google.common.base.Strings;
import com.intellij.openapi.options.ConfigurationException;
import com.intellij.openapi.options.SettingsEditor;
import org.jetbrains.annotations.NotNull;

import javax.swing.*;

/** The UI of Atest settings editor. */
public class AtestSettingsEditor extends SettingsEditor<AtestRunConfiguration> {

    private JComboBox mTestTarget;
    private JTextField mLunchTarget;
    private JPanel mSettingPanel;
    private JCheckBox mRunOnHost;
    private JCheckBox mSkipBuild;
    private JCheckBox mTestMapping;

    public AtestSettingsEditor() {
        mTestTarget.setEditable(true);
    }

    /**
     * It is called when discarding all non-confirmed user changes made via that UI.
     *
     * @param atestRunConfiguration An object of AtestRunConfiguration.
     */
    @Override
    protected void resetEditorFrom(@NotNull AtestRunConfiguration atestRunConfiguration) {
        String lunchTargetValue = atestRunConfiguration.getLaunchTarget();
        if (Strings.isNullOrEmpty(lunchTargetValue)) {
            lunchTargetValue = Constants.DEFAULT_LUNCH_TARGET;
        }
        mLunchTarget.setText(lunchTargetValue);
        mTestTarget.setSelectedItem(atestRunConfiguration.getTestTarget());
    }

    /**
     * It is called when confirming the changes.
     *
     * @param atestRunConfiguration An object of AtestRunConfiguration.
     * @throws ConfigurationException if the configuration settings have problem.
     */
    @Override
    protected void applyEditorTo(@NotNull AtestRunConfiguration atestRunConfiguration)
            throws ConfigurationException {
        atestRunConfiguration.setLaunchTarget(mLunchTarget.getText());
        atestRunConfiguration.setTestTarget(mTestTarget.getSelectedItem().toString());
    }

    /**
     * Creates the editor panel.
     *
     * @return the setting panel from AtestSettingsEditor.form.
     */
    @NotNull
    @Override
    protected JComponent createEditor() {
        AtestFastInputController atestFastInputController =
                new AtestFastInputController(mTestTarget, mRunOnHost, mTestMapping, mSkipBuild);
        atestFastInputController.linkCheckBoxWithTestTarget();
        return mSettingPanel;
    }
}
