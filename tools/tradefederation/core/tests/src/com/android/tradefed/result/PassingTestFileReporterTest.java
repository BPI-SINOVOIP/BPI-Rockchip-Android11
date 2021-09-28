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
package com.android.tradefed.result;

import static com.android.tradefed.util.FileUtil.readStringFromFile;

import com.android.ddmlib.testrunner.TestResult;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.FileUtil;

import com.google.common.truth.Truth;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;

/** Unit tests for {@link PassingTestFileReporter} */
@RunWith(JUnit4.class)
public class PassingTestFileReporterTest {
    private PassingTestFileReporter mResultReporter;
    private TestDescription mTest;
    private File mFile;

    @Before
    public void setUp() throws IOException, ConfigurationException {
        mFile = File.createTempFile("testfile", "txt");
        mResultReporter = new PassingTestFileReporter();
        OptionSetter setter = new OptionSetter(mResultReporter);
        setter.setOptionValue("test-file-to-record", mFile.getAbsolutePath());
        mTest = new TestDescription("FooTest", "testFoo");
        mResultReporter.invocationStarted(null);
    }

    @After
    public void tearDown() {
        FileUtil.deleteFile(mFile);
    }

    @Test
    public void testPassed() throws IOException {
        mResultReporter.testResult(mTest, createTestResult(TestResult.TestStatus.PASSED));
        mResultReporter.invocationEnded(0);

        String output = readStringFromFile(mFile);
        Truth.assertThat(output).contains("FooTest#testFoo");
    }

    @Test
    public void testFailed() throws IOException {
        mResultReporter.testResult(mTest, createTestResult(TestResult.TestStatus.FAILURE));
        mResultReporter.invocationEnded(0);

        String output = readStringFromFile(mFile);
        Truth.assertThat(output.length()).isEqualTo(0);
    }

    private com.android.tradefed.result.TestResult createTestResult(TestResult.TestStatus status) {
        com.android.tradefed.result.TestResult result =
                new com.android.tradefed.result.TestResult();
        result.setStatus(status);
        return result;
    }
}
