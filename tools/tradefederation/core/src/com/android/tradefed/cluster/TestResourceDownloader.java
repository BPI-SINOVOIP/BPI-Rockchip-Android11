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
import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.util.List;

/** A class to download test resource files from file system/GCS/HTTP. */
public class TestResourceDownloader {

    private static final long DOWNLOAD_TIMEOUT_MS = 30 * 60 * 1000;
    private static final long RETRY_INTERVAL_MS = 10 * 1000;
    private static final int MAX_RETRY_COUNT = 2;

    private final File mRootDir;
    private IRunUtil mRunUtil = null;

    public TestResourceDownloader(final File rootDir) {
        mRootDir = rootDir;
    }

    public File download(TestResource resource) throws IOException {
        final URL url = new URL(resource.getUrl());
        final String protocol = url.getProtocol();
        final File dest = new File(mRootDir, resource.getName());
        final File parent = dest.getParentFile();
        if (!parent.exists()) {
            parent.mkdirs();
        }
        if ("file".equals(protocol)) {
            final File src = new File(resource.getUrl().substring(6));
            FileUtil.hardlinkFile(src, dest);
            return dest;
        }

        final List<String> cmdArgs = buildDownloadCommandArgs(url, dest);
        final CommandResult result =
                getRunUtil()
                        .runTimedCmdRetry(
                                DOWNLOAD_TIMEOUT_MS,
                                RETRY_INTERVAL_MS,
                                MAX_RETRY_COUNT + 1,
                                cmdArgs.toArray(new String[0]));
        if (!result.getStatus().equals(CommandStatus.SUCCESS)) {
            final String msg =
                    String.format(
                            "Failed to download %s: command status=%s", url, result.getStatus());
            CLog.e(msg);
            CLog.e("stdout:\n'''\n%s'''\n", result.getStdout());
            CLog.d("stderr:\n'''\n%s'''\n", result.getStderr());
            throw new RuntimeException(msg);
        }
        return dest;
    }

    /** Build a list of command line arguments to download a file. */
    private List<String> buildDownloadCommandArgs(URL url, File file) {
        final String protocol = url.getProtocol();
        if ("gs".equals(protocol)) {
            // FIXME: Check whether gsutil is available on a host.
            return ArrayUtil.list("gsutil", "cp", url.toString(), file.getAbsolutePath());
        }
        if ("http".equals(protocol) || "https".equals(protocol)) {
            // FIXME: Check whether curl is available on a host.
            // Add -L option to handle redirect.
            return ArrayUtil.list("curl", "-o", file.getAbsolutePath(), "-fL", url.toString());
        }
        throw new UnsupportedOperationException("protocol " + protocol + " is not supported");
    }

    IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = new RunUtil();
        }
        return mRunUtil;
    }
}
