/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.atest.widget;

import org.jetbrains.annotations.NotNull;

import javax.swing.*;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;
import javax.swing.text.JTextComponent;

/** A controller for Atest target with checkboxes. */
public class AtestFastInputController {

    public static final String RUN_ON_HOST_POST = " --host";
    public static final String TEST_MAPPING_POST = " --test-mapping";
    public static final String SKIP_BUILD_POST = " -it";
    private JComboBox mTestTarget;
    private JCheckBox mRunOnHost;
    private JCheckBox mTestMapping;
    private JCheckBox mSkipBuild;

    /**
     * Initializes the UI components.
     *
     * @param testTarget a JComboBox used to input test command.
     * @param runOnHost a JCheckBox used to turn on/off run on host.
     * @param testMapping a JCheckBox used to turn on/off test mapping.
     * @param skipBuild a JCheckBox used to turn on/off skip build.
     */
    public AtestFastInputController(
            @NotNull JComboBox testTarget,
            @NotNull JCheckBox runOnHost,
            @NotNull JCheckBox testMapping,
            @NotNull JCheckBox skipBuild) {
        mTestTarget = testTarget;
        mRunOnHost = runOnHost;
        mTestMapping = testMapping;
        mSkipBuild = skipBuild;
    }
    /**
     * Links all checkbox to test target.
     *
     * <p>e.g. if users select run on host check box, the test target appends --host. if users type
     * "--host" in test target, the run on host check box is selected.
     */
    public void linkCheckBoxWithTestTarget() {
        mRunOnHost.addActionListener(e -> setCheckbox(mRunOnHost, RUN_ON_HOST_POST));
        mTestMapping.addActionListener(e -> setCheckbox(mTestMapping, TEST_MAPPING_POST));
        mSkipBuild.addActionListener(e -> setCheckbox(mSkipBuild, SKIP_BUILD_POST));
        final JTextComponent tc = (JTextComponent) mTestTarget.getEditor().getEditorComponent();
        tc.getDocument()
                .addDocumentListener(
                        new DocumentListener() {

                            /**
                             * Gives notification that there was an insert into the document. The
                             * range given by the DocumentEvent bounds the freshly inserted region.
                             *
                             * @param e the document event
                             */
                            @Override
                            public void insertUpdate(DocumentEvent e) {
                                checkAllTestTarget();
                            }

                            /**
                             * Gives notification that a portion of the document has been removed.
                             * The range is given in terms of what the view last saw (that is,
                             * before updating sticky positions).
                             *
                             * @param e the document event
                             */
                            @Override
                            public void removeUpdate(DocumentEvent e) {
                                checkAllTestTarget();
                            }

                            /**
                             * Gives notification that an attribute or set of attributes changed.
                             *
                             * @param e the document event
                             */
                            @Override
                            public void changedUpdate(DocumentEvent e) {
                                checkAllTestTarget();
                            }
                        });
    }

    /** Checks test target if it contains key words of all checkboxes. */
    private void checkAllTestTarget() {
        String testTarget = mTestTarget.getEditor().getItem().toString();
        checkTestTarget(testTarget, mRunOnHost, RUN_ON_HOST_POST);
        checkTestTarget(testTarget, mTestMapping, TEST_MAPPING_POST);
        checkTestTarget(testTarget, mSkipBuild, SKIP_BUILD_POST);
    }

    /**
     * Sets the checkbox behavior.
     *
     * <p>if users select check box, the test target appends the corresponding postfix.
     *
     * @param checkbox the JCheckBox to set.
     * @param postfix the corresponding postfix of the JCheckBox.
     */
    private void setCheckbox(JCheckBox checkbox, String postfix) {
        String testTarget = mTestTarget.getEditor().getItem().toString();
        if (checkbox.isSelected()) {
            testTarget = testTarget.concat(postfix);
        } else {
            testTarget = testTarget.replace(postfix, "");
        }
        mTestTarget.setSelectedItem(testTarget);
    }

    /**
     * Checks test target if it contains key words of checkbox.
     *
     * <p>if users type the keyword in test target, the corresponding checkbox is selected.
     *
     * @param testTarget the string in test target.
     * @param checkbox the JCheckBox to set.
     * @param postfix the corresponding postfix of the JCheckBox.
     */
    private void checkTestTarget(String testTarget, JCheckBox checkbox, String postfix) {
        if (testTarget.contains(postfix)) {
            checkbox.setSelected(true);
        } else {
            checkbox.setSelected(false);
        }
    }
}
