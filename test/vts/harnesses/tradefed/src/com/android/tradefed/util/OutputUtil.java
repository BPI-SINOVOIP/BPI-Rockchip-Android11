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

import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.LogDataType;
import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Set;

/**
 * Utility class to add output file to TradeFed log directory.
 */
public class OutputUtil {
    // Test logger object from test invocation
    ITestLogger mListener;
    private String mTestModuleName = null;
    private String mAbiName = null;

    public OutputUtil(ITestLogger listener) {
        mListener = listener;
    }

    /**
     * Collect all VTS python runner log output files as a single zip file
     * @param logDirectory
     */
    public void ZipVtsRunnerOutputDir(File logDirectory) {
        try {
            Set<String> latest = FileUtil.findFiles(logDirectory, "latest");
            if (latest.isEmpty()) {
                CLog.e("Empty python log directory: %s", logDirectory);
                return;
            }

            File tmpZip = ZipUtil.createZip(
                    Arrays.asList(new File(latest.iterator().next()).listFiles()));
            String outputFileName = "module_" + mTestModuleName + "_output_files_" + mAbiName;
            try (FileInputStreamSource inputSource = new FileInputStreamSource(tmpZip, true)) {
                mListener.testLog(outputFileName, LogDataType.ZIP, inputSource);
            }
        } catch (IOException e) {
            CLog.e("Error processing python module output directory: %s", logDirectory);
            CLog.e(e);
        }
    }

    /**
     * @param testModuleName
     */
    public void setTestModuleName(String testModuleName) {
        mTestModuleName = testModuleName;
    }

    /**
     * @param abiName
     */
    public void setAbiName(String abiName) {
        mAbiName = abiName;
    }
}