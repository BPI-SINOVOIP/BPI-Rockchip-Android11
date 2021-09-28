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

import static org.junit.Assert.assertEquals;

import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationDescriptor.LocalTestRunner;
import com.android.tradefed.config.OptionDef;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.Abi;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link LocalRunInstructionBuilder} */
@RunWith(JUnit4.class)
public class LocalRunInstructionBuilderTest {
    private static final String OPTION_NAME = "option";
    private static final String OPTION_KEY = "key";
    private static final String OPTION_VALUE = "value";
    private static final String OPTION_NAME_ONLY = "option_only";
    private static final String OPTION_VALUE_ONLY = "value_only";
    private static final String OPTION_SOURCE = "module_name";
    private static final String CLASS_NAME = "class_name";
    private static final String METHOD_NAME = "method_name";
    private static final String METHOD_NAME_WITH_PARAMETER = "method_name[parameter_value]";
    private static final String ABI_NAME = "arm";

    /**
     * test {@link LocalRunInstructionBuilder#getInstruction(ConfigurationDescriptor,
     * LocalTestRunner, TestDescription)}
     */
    @Test
    public void testGetInstruction() {
        ConfigurationDescriptor configDescriptor = new ConfigurationDescriptor();
        configDescriptor.addRerunOption(
                new OptionDef(OPTION_NAME, OPTION_KEY, OPTION_VALUE, OPTION_SOURCE));
        configDescriptor.addRerunOption(
                new OptionDef(OPTION_NAME_ONLY, null, OPTION_VALUE_ONLY, OPTION_SOURCE));
        configDescriptor.setAbi(new Abi(ABI_NAME, "32"));
        configDescriptor.setModuleName(OPTION_SOURCE);
        String instruction =
                LocalRunInstructionBuilder.getInstruction(
                        configDescriptor, LocalTestRunner.ATEST, null);
        assertEquals(
                "Run following command to try the test in a local setup:\n"
                        + "atest module_name -- --abi arm "
                        + "--module-arg module_name:option:key:=value "
                        + "--module-arg module_name:option_only:value_only",
                instruction);
    }

    /**
     * test {@link LocalRunInstructionBuilder#getInstruction(ConfigurationDescriptor,
     * LocalTestRunner, TestDescription)} with TestId specified.
     */
    @Test
    public void testGetInstruction_withTestId() {
        ConfigurationDescriptor configDescriptor = new ConfigurationDescriptor();
        configDescriptor.setAbi(new Abi(ABI_NAME, "32"));
        configDescriptor.setModuleName(OPTION_SOURCE);
        TestDescription testId = new TestDescription(CLASS_NAME, METHOD_NAME);
        String instruction =
                LocalRunInstructionBuilder.getInstruction(
                        configDescriptor, LocalTestRunner.ATEST, testId);
        assertEquals(
                "Run following command to try the test in a local setup:\n"
                        + "atest module_name:class_name#method_name -- --abi arm",
                instruction);
    }

    /**
     * test {@link LocalRunInstructionBuilder#getInstruction(ConfigurationDescriptor,
     * LocalTestRunner, TestDescription)} with TestId specified which contains parameter for test
     * method.
     */
    @Test
    public void testGetInstruction_withTestIdAndParameter() {
        ConfigurationDescriptor configDescriptor = new ConfigurationDescriptor();
        configDescriptor.setAbi(new Abi(ABI_NAME, "32"));
        configDescriptor.setModuleName(OPTION_SOURCE);
        TestDescription testId = new TestDescription(CLASS_NAME, METHOD_NAME_WITH_PARAMETER);
        String instruction =
                LocalRunInstructionBuilder.getInstruction(
                        configDescriptor, LocalTestRunner.ATEST, testId);
        assertEquals(
                "Run following command to try the test in a local setup:\n"
                        + "atest module_name:class_name -- --abi arm",
                instruction);
    }

    /** Test when a parameterized module needs to be repro. */
    @Test
    public void testGetInstruction_withParameter() {
        ConfigurationDescriptor configDescriptor = new ConfigurationDescriptor();
        configDescriptor.setAbi(new Abi(ABI_NAME, "32"));
        configDescriptor.setModuleName(OPTION_SOURCE);
        configDescriptor.addMetadata(ConfigurationDescriptor.ACTIVE_PARAMETER_KEY, "instant");
        String instruction =
                LocalRunInstructionBuilder.getInstruction(
                        configDescriptor, LocalTestRunner.ATEST, null);
        assertEquals(
                "Run following command to try the test in a local setup:\n"
                        + "atest module_name -- --abi arm --instant",
                instruction);
    }
}
