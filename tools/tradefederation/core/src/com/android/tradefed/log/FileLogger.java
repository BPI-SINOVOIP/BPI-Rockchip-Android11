/*
 * Copyright (C) 2010 The Android Open Source Project
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
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.SnapshotInputStreamSource;
import com.android.tradefed.util.SizeLimitedOutputStream;
import com.android.tradefed.util.StreamUtil;

import com.google.common.annotations.VisibleForTesting;

import java.io.IOException;
import java.io.InputStream;

/** A {@link ILeveledLogOutput} that directs log messages to a file and to stdout. */
@OptionClass(alias = "file")
public class FileLogger extends BaseStreamLogger<SizeLimitedOutputStream> {
    private static final String TEMP_FILE_PREFIX = "tradefed_log_";
    private static final String TEMP_FILE_SUFFIX = ".txt";

    @Option(name = "max-log-size", description = "maximum allowable size of tmp log data in mB.")
    private long mMaxLogSizeMbytes = 20;

    @Override
    public void init() throws IOException {
        init(TEMP_FILE_PREFIX, TEMP_FILE_SUFFIX);
    }

    /**
     * Alternative to {@link #init()} where we can specify the file name and suffix.
     *
     * @param logPrefix the file name where to log without extension.
     * @param fileSuffix the extension of the file where to log.
     */
    protected void init(String logPrefix, String fileSuffix) {
        mOutputStream =
                new SizeLimitedOutputStream(mMaxLogSizeMbytes * 1024 * 1024, logPrefix, fileSuffix);
    }

    /**
     * Creates a new {@link FileLogger} with the same log level settings as the current object.
     * <p/>
     * Does not copy underlying log file content (ie the clone's log data will be written to a new
     * file.)
     */
    @Override
    public ILeveledLogOutput clone()  {
        FileLogger logger = new FileLogger();
        OptionCopier.copyOptionsNoThrow(this, logger);
        return logger;
    }

    /** Returns the max log size of the log in MBytes. */
    public long getMaxLogSizeMbytes() {
        return mMaxLogSizeMbytes;
    }

    @Override
    public InputStreamSource getLog() {
        if (mOutputStream != null) {
            try {
                // create a InputStream from log file
                mOutputStream.flush();
                return new SnapshotInputStreamSource("FileLogger", mOutputStream.getData());
            } catch (IOException e) {
                System.err.println("Failed to get log");
                e.printStackTrace();
            }
        }
        return new ByteArrayInputStreamSource(new byte[0]);
    }

    @Override
    public void closeLog() {
        doCloseLog();
    }

    /** Flushes stream and closes log file. */
    @VisibleForTesting
    void doCloseLog() {
        SizeLimitedOutputStream stream = mOutputStream;
        mOutputStream = null;
        StreamUtil.flushAndCloseStream(stream);
        if (stream != null) {
            stream.delete();
        }
    }

    /**
     * Dump the contents of the input stream to this log
     *
     * @param inputStream input stream to dump
     * @throws IOException if an I/O error occurs
     */
    void dumpToLog(InputStream inputStream) throws IOException {
        if (mOutputStream != null) {
            StreamUtil.copyStreams(inputStream, mOutputStream);
        }
    }
}
