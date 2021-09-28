/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tradefed.util;

import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationDescriptor.LocalTestRunner;
import com.android.tradefed.config.OptionDef;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.TestDescription;

import java.util.List;

/** Utility to compile the instruction to run test locally. */
public class LocalRunInstructionBuilder {

    // TODO(dshi): The method is added for backwards compatibility. Remove this method after lab is
    // updated.
    /**
     * Compile the instruction to run test locally.
     *
     * @param configDescriptor {@link ConfigurationDescriptor} to create instruction for.
     * @param runner {@link LocalTestRunner} to be used to build instruction.
     * @return {@link String} of the instruction.
     */
    public static String getInstruction(
            ConfigurationDescriptor configDescriptor, LocalTestRunner runner) {
        return getInstruction(configDescriptor, runner, null);
    }

    /**
     * Compile the instruction to run test locally.
     *
     * @param configDescriptor {@link ConfigurationDescriptor} to create instruction for.
     * @param runner {@link LocalTestRunner} to be used to build instruction.
     * @param testId {@link TestDescription} of the test to run. It can be null when building local
     *     run instruction for the whole module.
     * @return {@link String} of the instruction.
     */
    public static String getInstruction(
            ConfigurationDescriptor configDescriptor,
            LocalTestRunner runner,
            TestDescription testId) {
        if (runner == null) {
            return null;
        }
        switch (runner) {
            case ATEST:
                return getAtestInstruction(configDescriptor, testId);
            default:
                CLog.v("Runner %s is not supported yet, no instruction will be built.", runner);
                return null;
        }
    }

    /**
     * Compile the instruction to run test locally using atest.
     *
     * @param configDescriptor {@link ConfigurationDescriptor} to create instruction for.
     * @param testId {@link TestDescription} of the test to run.
     * @return {@link String} of the instruction.
     */
    private static String getAtestInstruction(
            ConfigurationDescriptor configDescriptor, TestDescription testId) {
        StringBuilder instruction = new StringBuilder();
        instruction.append("Run following command to try the test in a local setup:\n");
        instruction.append(getCommand(configDescriptor, testId, LocalTestRunner.ATEST));
        return instruction.toString();
    }

    /**
     * Return a command to run a test locally.
     *
     * @param configDescriptor {@link ConfiguratonDescriptor} configuration for the test run.
     * @param testId {@link TestDescription} to specify which test to run.
     * @param LocalTestRunner {@link LocalTestRunner} to use for running the test.
     * @return {@link String} command to run the test locally.
     */
    public static String getCommand(
            ConfigurationDescriptor configDescriptor,
            TestDescription testId,
            LocalTestRunner runner) {
        if (runner != LocalTestRunner.ATEST) {
            return "";
        }
        StringBuilder instruction = new StringBuilder();
        StringBuilder testName = new StringBuilder(configDescriptor.getModuleName());
        boolean testMethodAdded = false;
        if (testId != null) {
            testName.append(String.format(":%s", testId.getClassName()));
            // Atest doesn't support test parameter, so ignore the method filter if parameter is
            // set.
            if (testId.getTestName().equals(testId.getTestNameWithoutParams())) {
                testName.append(String.format("#%s", testId.getTestName()));
                testMethodAdded = true;
            }
        }

        instruction.append(String.format("atest %s --", testName));
        if (configDescriptor.getAbi() != null) {
            instruction.append(String.format(" --abi %s", configDescriptor.getAbi().getName()));
        }
        if (!testMethodAdded) {
            for (OptionDef optionDef : configDescriptor.getRerunOptions()) {
                StringBuilder option =
                        new StringBuilder(
                                String.format(
                                        "--module-arg %s:%s:",
                                        configDescriptor.getModuleName(), optionDef.name));
                if (optionDef.key == null) {
                    option.append(String.format("%s", optionDef.value));
                } else {
                    option.append(String.format("%s:=%s", optionDef.key, optionDef.value));
                }
                instruction.append(" " + option);
            }
        }
        // Ensure repro is aligned with parameterized modules.
        List<String> paramMetadata =
                configDescriptor.getMetaData(ConfigurationDescriptor.ACTIVE_PARAMETER_KEY);
        if (paramMetadata != null
                && paramMetadata.size() > 0
                && "instant".equals(paramMetadata.get(0))) {
            instruction.append(" --instant");
        }
        return instruction.toString();
    }
}
