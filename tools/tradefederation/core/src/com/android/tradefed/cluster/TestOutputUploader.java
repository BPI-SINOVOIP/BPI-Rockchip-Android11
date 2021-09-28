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
package com.android.tradefed.cluster;

import com.android.loganalysis.util.ArrayUtil;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.List;

/** A class to upload test output files to GCS/HTTP. */
public class TestOutputUploader {

    static final long UPLOAD_TIMEOUT_MS = 30 * 60 * 1000;
    static final long RETRY_INTERVAL_MS = 10 * 1000;
    static final int MAX_RETRY_COUNT = 2;
    static final String FILE_PROTOCOL = "file";
    static final String GCS_PROTOCOL = "gs";
    static final String HTTP_PROTOCOL = "http";
    static final String HTTPS_PROTOCOL = "https";

    private String mUploadUrl = null;
    private String mProtocol = null;
    private IRunUtil mRunUtil = null;

    public void setUploadUrl(final String url) throws MalformedURLException {
        mUploadUrl = url;
        URL urlObj = new URL(url);
        mProtocol = urlObj.getProtocol();
    }

    public String uploadFile(File file, final String destinationPath) throws IOException {
        String uploadUrl = mUploadUrl;
        if (!mUploadUrl.endsWith("/")) {
            uploadUrl += "/";
        }
        if (destinationPath != null) {
            uploadUrl += destinationPath;
        }
        CLog.i("Uploading %s to %s", file.getAbsolutePath(), uploadUrl);
        if (FILE_PROTOCOL.equals(mProtocol)) {
            final File dir = new File(new URL(uploadUrl).getPath());
            if (!dir.exists()) {
                dir.mkdirs();
            }
            final File destFile = new File(dir, file.getName());
            FileUtil.copyFile(file, destFile);
        } else {
            final List<String> cmdArgs = buildUploadCommandArgs(file, uploadUrl);
            final CommandResult result =
                    getRunUtil()
                            .runTimedCmdRetry(
                                    UPLOAD_TIMEOUT_MS,
                                    RETRY_INTERVAL_MS,
                                    MAX_RETRY_COUNT,
                                    cmdArgs.toArray(new String[0]));
            if (!result.getStatus().equals(CommandStatus.SUCCESS)) {
                final String msg =
                        String.format(
                                "failed to upload %s: command status=%s",
                                file.getAbsolutePath(), result.getStatus());
                CLog.e(msg);
                CLog.e("stdout:\n'''\n%s'''\n", result.getStdout());
                CLog.d("stderr:\n'''\n%s'''\n", result.getStderr());
                throw new RuntimeException(msg);
            }
        }
        String baseUrl = uploadUrl;
        if (!baseUrl.endsWith("/")) {
            baseUrl += "/";
        }
        return baseUrl + file.getName();
    }

    private List<String> buildUploadCommandArgs(File file, String uploadUrl) {
        if (mUploadUrl == null) {
            throw new IllegalStateException("upload url is not set");
        }
        switch (mProtocol) {
            case GCS_PROTOCOL:
                return ArrayUtil.list("gsutil", "cp", file.getAbsolutePath(), uploadUrl);
            case HTTP_PROTOCOL:
            case HTTPS_PROTOCOL:
                // Add -L option to handle redirect.
                return ArrayUtil.list(
                        "curl",
                        "-X",
                        "POST",
                        "-F file=@" + file.getAbsolutePath(),
                        "-fL",
                        uploadUrl);
        }
        throw new IllegalArgumentException(
                String.format("Protocol '%s' is not supported", mProtocol));
    }

    @VisibleForTesting
    IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = RunUtil.getDefault();
        }
        return mRunUtil;
    }
}
