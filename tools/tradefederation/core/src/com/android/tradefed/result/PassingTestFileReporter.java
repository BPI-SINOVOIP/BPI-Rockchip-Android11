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

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;

import java.io.File;

/** A {@link ITestInvocationListener} that saves the list of passing test cases to a test file */
public class PassingTestFileReporter extends TestResultListener implements ITestInvocationListener {
    @Option(
            name = "test-file-to-record",
            description = "path to test file to write results to",
            mandatory = true)
    private File mTestFilePath;

    private TestDescriptionsFile mResults;

    @Override
    public void invocationStarted(IInvocationContext context) {
        mResults = new TestDescriptionsFile();
    }

    @Override
    public void testResult(TestDescription test, TestResult result) {
        if (result.getStatus() == TestStatus.PASSED) {
            mResults.add(test);
        }
    }

    @Override
    public void invocationEnded(long elapsedTime) {
        mTestFilePath.getParentFile().mkdirs();
        mResults.populateTestFile(mTestFilePath);
    }
}
