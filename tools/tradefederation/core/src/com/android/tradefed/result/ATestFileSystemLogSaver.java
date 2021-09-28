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
package com.android.tradefed.result;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.IOException;

/** This LogSaver class is used by ATest to save logs in a specific path. */
@OptionClass(alias = "atest-file-system-log-saver")
public class ATestFileSystemLogSaver extends FileSystemLogSaver {

    @Option(name = "atest-log-file-path", description = "root file system path to store log files.")
    private File mAtestRootReportDir = new File(System.getProperty("java.io.tmpdir"));

    /**
     * {@inheritDoc}
     *
     * @return The directory created in the path that option atest-log-file-path specify.
     */
    @Override
    protected File generateLogReportDir(IBuildInfo buildInfo, File reportDir) throws IOException {
        return FileUtil.createTempDir("invocation_", mAtestRootReportDir);
    }
}
