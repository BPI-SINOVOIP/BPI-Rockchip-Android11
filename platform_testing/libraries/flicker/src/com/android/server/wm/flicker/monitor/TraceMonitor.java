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

package com.android.server.wm.flicker.monitor;

import static com.android.compatibility.common.util.SystemUtil.runShellCommand;

import android.os.RemoteException;

import androidx.annotation.VisibleForTesting;

import java.nio.file.Path;
import java.util.Locale;

/**
 * Base class for monitors containing common logic to read the trace as a byte array and save the
 * trace to another location.
 */
public abstract class TraceMonitor implements ITransitionMonitor {
    public static final String TAG = "FLICKER";
    private static final String TRACE_DIR = "/data/misc/wmtrace/";

    private Path mOutputDir;
    public String mTraceFileName;

    public abstract boolean isEnabled() throws RemoteException;

    public TraceMonitor(Path outputDir, String traceFileName) {
        mOutputDir = outputDir;
        mTraceFileName = traceFileName;
    }

    /**
     * Saves trace file to the external storage directory suffixing the name with the testtag and
     * iteration.
     *
     * <p>Moves the trace file from the default location via a shell command since the test app does
     * not have security privileges to access /data/misc/wmtrace.
     *
     * @param testTag suffix added to trace name used to identify trace
     * @return Path to saved trace file
     */
    @Override
    public Path save(String testTag) {
        mOutputDir.toFile().mkdirs();
        Path traceFileCopy = getOutputTraceFilePath(testTag);

        // Move the trace file to the output directory
        // Note: Due to b/141386109, certain devices do not allow moving the files between
        //       directories with different encryption policies, so manually copy and then
        //       remove the original file
        String copyCommand =
                String.format(
                        Locale.getDefault(),
                        "cp %s%s %s",
                        TRACE_DIR,
                        mTraceFileName,
                        traceFileCopy.toString());
        runShellCommand(copyCommand);
        String removeCommand =
                String.format(
                        Locale.getDefault(),
                        "rm %s%s",
                        TRACE_DIR,
                        mTraceFileName);
        runShellCommand(removeCommand);
        return traceFileCopy;
    }

    @VisibleForTesting
    public Path getOutputTraceFilePath(String testTag) {
        return mOutputDir.resolve(mTraceFileName + "_" + testTag);
    }
}
