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

package com.android.tradefed.testtype;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.MultiMap;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** This is a tradefed test to test tradefed can resolve dynamic file correctly. */
public class DynamicFileStubTest implements IRemoteTest {

    @Option(
        name = "dynamic-file",
        description = "To test tradefed can resolve dynamic file for file option."
    )
    private File mFile;

    @Option(
        name = "dynamic-file-list",
        description = "To test tradefed can resolve dynamic files in list option."
    )
    private List<File> mFileList = new ArrayList<File>();

    @Option(
        name = "dynamic-file-map",
        description = "To test tradefed can resolve dynamic files in map option."
    )
    private Map<File, File> mFileMap = new HashMap<>();

    @Option(
        name = "dynamic-file-multimap",
        description = "To test tradefed can resolve dynamic files in multimap option."
    )
    private MultiMap<File, File> mFileMultiMap = new MultiMap<>();

    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        long startTime = System.currentTimeMillis();
        listener.testRunStarted(this.getClass().getName(), 1);
        checkDynamicFile(listener);
        checkDynamicFileList(listener);
        checkDynamicFileMap(listener);
        checkDynamicFileMultiMap(listener);
        listener.testRunEnded(
                System.currentTimeMillis() - startTime, new LinkedHashMap<String, Metric>());
    }

    private void checkDynamicFile(ITestInvocationListener listener) {
        TestDescription testId = new TestDescription(this.getClass().getName(), "DynamicFile");
        listener.testStarted(testId);
        if (mFile != null) {
            if (!mFile.exists()) {
                listener.testFailed(testId, String.format("%s doesn't exist.", mFile.getPath()));
            }
        }
        listener.testEnded(testId, new LinkedHashMap<String, Metric>());
    }

    private void checkDynamicFileList(ITestInvocationListener listener) {
        TestDescription testId = new TestDescription(this.getClass().getName(), "DynamicFileList");
        listener.testStarted(testId);
        List<String> nonExistFiles = new ArrayList<String>();
        if (mFileList != null && !mFileList.isEmpty()) {
            for (File file : mFileList) {
                if (!file.exists()) {
                    nonExistFiles.add(file.getPath());
                }
            }
            if (!nonExistFiles.isEmpty()) {
                listener.testFailed(testId, String.format("%s doesn't exist.", nonExistFiles));
            }
        }
        listener.testEnded(testId, new LinkedHashMap<String, Metric>());
    }

    private void checkDynamicFileMap(ITestInvocationListener listener) {
        TestDescription testId = new TestDescription(this.getClass().getName(), "DynamicFileMap");
        listener.testStarted(testId);
        List<String> nonExistFiles = new ArrayList<String>();
        if (mFileMap != null && !mFileMap.isEmpty()) {
            for (Map.Entry<File, File> entry : mFileMap.entrySet()) {
                if (!entry.getKey().exists()) {
                    nonExistFiles.add(entry.getKey().getPath());
                }
                if (!entry.getValue().exists()) {
                    nonExistFiles.add(entry.getValue().getPath());
                }
            }
            if (!nonExistFiles.isEmpty()) {
                listener.testFailed(testId, String.format("%s doesn't exist.", nonExistFiles));
            }
        }
        listener.testEnded(testId, new LinkedHashMap<String, Metric>());
    }

    private void checkDynamicFileMultiMap(ITestInvocationListener listener) {
        TestDescription testId =
                new TestDescription(this.getClass().getName(), "DynamicFileMultiMap");
        listener.testStarted(testId);
        List<String> nonExistFiles = new ArrayList<String>();
        if (mFileMultiMap != null && !mFileMultiMap.isEmpty()) {
            for (File key : mFileMultiMap.keySet()) {
                if (!key.exists()) {
                    nonExistFiles.add(key.getPath());
                }
                for (File value : mFileMultiMap.get(key)) {
                    if (!value.exists()) {
                        nonExistFiles.add(value.getPath());
                    }
                }
            }
            if (!nonExistFiles.isEmpty()) {
                listener.testFailed(testId, String.format("%s doesn't exist.", nonExistFiles));
            }
        }
        listener.testEnded(testId, new LinkedHashMap<String, Metric>());
    }
}
