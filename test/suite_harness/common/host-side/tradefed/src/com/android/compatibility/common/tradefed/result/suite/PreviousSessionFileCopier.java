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
package com.android.compatibility.common.tradefed.result.suite;

import com.android.annotations.VisibleForTesting;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.util.ChecksumReporter;
import com.android.compatibility.common.util.ResultHandler;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Recursively copy all the files from a previous session into the current one if they don't exists
 * already.
 */
public class PreviousSessionFileCopier implements ITestInvocationListener {

    private static final List<String> NOT_RETRY_FILES =
            Arrays.asList(
                    ChecksumReporter.NAME,
                    ChecksumReporter.PREV_NAME,
                    ResultHandler.FAILURE_REPORT_NAME,
                    CertificationSuiteResultReporter.HTLM_REPORT_NAME,
                    CertificationSuiteResultReporter.FAILURE_REPORT_NAME,
                    CertificationSuiteResultReporter.SUMMARY_FILE,
                    CertificationChecksumHelper.NAME,
                    "diffs",
                    "proto");

    private CompatibilityBuildHelper mBuildHelper;
    private File mPreviousSessionDir = null;

    /** Sets the previous session directory to copy from. */
    public void setPreviousSessionDir(File previousSessionDir) {
        mPreviousSessionDir = previousSessionDir;
    }

    @Override
    public void invocationStarted(IInvocationContext context) {
        if (mBuildHelper == null) {
            mBuildHelper = createCompatibilityHelper(context.getBuildInfos().get(0));
        }
    }

    @Override
    public void invocationEnded(long elapsedTime) {
        if (mPreviousSessionDir == null) {
            CLog.e("Could not copy previous sesson files.");
            return;
        }
        File resultDir = getResultDirectory();
        copyRetryFiles(mPreviousSessionDir, resultDir);
    }

    @VisibleForTesting
    protected CompatibilityBuildHelper createCompatibilityHelper(IBuildInfo info) {
        return new CompatibilityBuildHelper(info);
    }

    /**
     * Recursively copy any other files found in the previous session's result directory to the new
     * result directory, so long as they don't already exist. For example, a "screenshots" directory
     * generated in a previous session by a passing test will not be generated on retry unless
     * copied from the old result directory.
     *
     * @param oldDir
     * @param newDir
     */
    private void copyRetryFiles(File oldDir, File newDir) {
        File[] oldChildren = oldDir.listFiles();
        for (File oldChild : oldChildren) {
            if (NOT_RETRY_FILES.contains(oldChild.getName())) {
                continue; // do not copy this file/directory or its children
            }
            File newChild = new File(newDir, oldChild.getName());
            if (!newChild.exists()) {
                // If this old file or directory doesn't exist in new dir, simply copy it
                try {
                    CLog.d("Copying %s to new session.", oldChild.getName());
                    if (oldChild.isDirectory()) {
                        FileUtil.recursiveCopy(oldChild, newChild);
                    } else {
                        FileUtil.copyFile(oldChild, newChild);
                    }
                } catch (IOException e) {
                    CLog.w("Failed to copy file \"%s\" from previous session", oldChild.getName());
                }
            } else if (oldChild.isDirectory() && newChild.isDirectory()) {
                // If both children exist as directories, make sure the children of the old child
                // directory exist in the new child directory.
                copyRetryFiles(oldChild, newChild);
            }
        }
    }

    private File getResultDirectory() {
        File resultDir = null;
        try {
            resultDir = mBuildHelper.getResultDir();
            if (resultDir != null) {
                resultDir.mkdirs();
            }
        } catch (FileNotFoundException e) {
            throw new RuntimeException(e);
        }
        if (resultDir == null) {
            throw new RuntimeException("Result Directory was not created");
        }
        if (!resultDir.exists()) {
            throw new RuntimeException(
                    "Result Directory was not created: " + resultDir.getAbsolutePath());
        }
        CLog.d("Results Directory: %s", resultDir.getAbsolutePath());
        return resultDir;
    }
}
