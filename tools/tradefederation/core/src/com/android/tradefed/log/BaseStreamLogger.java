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

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.util.StreamUtil;

import java.io.IOException;
import java.io.OutputStream;

/** A {@link ILeveledLogOutput} that directs log messages to an output stream and to stdout. */
public abstract class BaseStreamLogger<OS extends OutputStream> extends BaseLeveledLogOutput {

    @Option(name = "log-level", description = "the minimum log level to log.")
    private LogLevel mLogLevel = LogLevel.DEBUG;

    @Option(
        name = "log-level-display",
        shortName = 'l',
        description = "the minimum log level to display on stdout.",
        importance = Importance.ALWAYS
    )
    private LogLevel mLogLevelDisplay = LogLevel.ERROR;

    // output stream to print logs to, exposed to subclasses
    protected OS mOutputStream;

    @Override
    public LogLevel getLogLevel() {
        return mLogLevel;
    }

    @Override
    public void setLogLevel(LogLevel logLevel) {
        mLogLevel = logLevel;
    }

    /** Sets the minimum {@link LogLevel} to display on stdout. */
    public void setLogLevelDisplay(LogLevel logLevel) {
        mLogLevelDisplay = logLevel;
    }

    /** @return current minimum {@link LogLevel} to display on stdout. */
    LogLevel getLogLevelDisplay() {
        return mLogLevelDisplay;
    }

    @Override
    public void closeLog() {
        StreamUtil.flushAndCloseStream(mOutputStream);
        mOutputStream = null;
    }

    @Override
    public void printAndPromptLog(LogLevel logLevel, String tag, String message) {
        internalPrintLog(logLevel, tag, message, true /* force print to stdout */);
    }

    @Override
    public void printLog(LogLevel logLevel, String tag, String message) {
        internalPrintLog(logLevel, tag, message, false /* don't force stdout */);
    }

    /**
     * A version of printLog(...) which can be forced to print to stdout, even if the log level
     * isn't above the urgency threshold.
     */
    private void internalPrintLog(
            LogLevel logLevel, String tag, String message, boolean forceStdout) {
        String outMessage = LogUtil.getLogFormatString(logLevel, tag, message);
        if (shouldDisplay(forceStdout, mLogLevelDisplay, logLevel, tag)) {
            System.out.print(outMessage);
        }
        if (shouldWrite(tag, logLevel, mLogLevel)) {
            try {
                writeToLog(outMessage);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    // Determines whether a message should be written to the output stream.
    private boolean shouldWrite(String tag, LogLevel messageLogLevel, LogLevel invocationLogLevel) {
        LogLevel forcedLevel = getForcedVerbosityMap().get(tag);
        if (forcedLevel == null || !shouldForceVerbosity()) {
            return true;
        }
        // Use the highest level of our forced and invocation to decide if we should log the
        // particular tag.
        int minWriteLevel = Math.max(forcedLevel.getPriority(), invocationLogLevel.getPriority());
        return messageLogLevel.getPriority() >= minWriteLevel;
    }

    /**
     * Writes a message to the output stream.
     *
     * @param message the entry to write to log
     * @throws IOException if an I/O error occurs
     */
    protected void writeToLog(String message) throws IOException {
        if (mOutputStream != null) {
            mOutputStream.write(message.getBytes());
        }
    }
}
