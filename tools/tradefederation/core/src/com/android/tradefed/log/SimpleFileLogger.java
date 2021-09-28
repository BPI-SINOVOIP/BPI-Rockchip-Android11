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
package com.android.tradefed.log;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/** A {@link ILeveledLogOutput} that directs log messages to stdout and to a single log file. */
@OptionClass(alias = "simple-file")
public class SimpleFileLogger extends BaseStreamLogger<FileOutputStream> {

    @Option(name = "path", description = "File path to log to.", mandatory = true)
    private File mFile;

    @Override
    public void init() throws IOException {
        mFile.getParentFile().mkdirs();
        mOutputStream = new FileOutputStream(mFile, true);
    }

    @Override
    public InputStreamSource getLog() {
        if (mFile != null) {
            return new FileInputStreamSource(mFile);
        }
        return new ByteArrayInputStreamSource(new byte[0]);
    }

    @Override
    public SimpleFileLogger clone() {
        SimpleFileLogger logger = new SimpleFileLogger();
        OptionCopier.copyOptionsNoThrow(this, logger);
        return logger;
    }
}
