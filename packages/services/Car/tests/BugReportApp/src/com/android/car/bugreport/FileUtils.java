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
package com.android.car.bugreport;

import android.content.Context;

import com.google.common.base.Preconditions;

import java.io.File;

/**
 * File utilities.
 *
 * Thread safety and file operations: All file operations should happen on the same worker
 * thread for thread safety. This is provided by running both bugreport service and file upload
 * service on a asynctask. Asynctasks are by default executed on the same worker thread in serial.
 *
 * There is one exception to the rule above:
 * Voice recorder works on main thread, however this is not a thread safety problem because:
 * i. voice recorder always works before starting to collect rest of the bugreport
 * ii. a bug report cannot be moved to upload (pending) directory before it is completely
 * collected.
 */
public class FileUtils {
    private static final String PREFIX = "bugreport-";
    /** A directory under the system user; contains bugreport zip files and audio files. */
    private static final String PENDING_DIR = "bug_reports_pending";
    // Temporary directory under the current user, used for zipping files.
    private static final String TEMP_DIR = "bug_reports_temp";

    private static final String FS = "@";

    static File getPendingDir(Context context) {
        Preconditions.checkArgument(context.getUser().isSystem(),
                "Must be called from the system user.");
        File dir = new File(context.getDataDir(), PENDING_DIR);
        dir.mkdirs();
        return dir;
    }

    /**
     * Creates and returns file directory for storing bug report files before they are zipped into
     * a single file.
     */
    static File createTempDir(Context context, String timestamp) {
        File dir = getTempDir(context, timestamp);
        dir.mkdirs();
        return dir;
    }

    /**
     * Returns path to the directory for storing bug report files before they are zipped into a
     * single file.
     */
    static File getTempDir(Context context, String timestamp) {
        Preconditions.checkArgument(!context.getUser().isSystem(),
                "Must be called from the current user.");
        return new File(context.getDataDir(), TEMP_DIR + "/" + timestamp);
    }

    /**
     * Constructs a bugreport zip file name.
     *
     * <p>Add lookup code to the filename to allow matching audio file and bugreport file in USB.
     */
    static String getZipFileName(MetaBugReport bug) {
        String lookupCode = extractLookupCode(bug);
        return PREFIX + bug.getUserName() + FS + bug.getTimestamp() + "-" + lookupCode + ".zip";
    }

    /**
     * Constructs a audio message file name.
     *
     * <p>Add lookup code to the filename to allow matching audio file and bugreport file in USB.
     *
     * @param timestamp - current timestamp, when audio was created.
     * @param bug       - a bug report.
     */
    static String getAudioFileName(String timestamp, MetaBugReport bug) {
        String lookupCode = extractLookupCode(bug);
        return PREFIX + bug.getUserName() + FS + timestamp + "-" + lookupCode + "-message.3gp";
    }

    private static String extractLookupCode(MetaBugReport bug) {
        Preconditions.checkArgument(bug.getTitle().startsWith("["),
                "Invalid bugreport title, doesn't contain lookup code. ");
        return bug.getTitle().substring(1, BugReportActivity.LOOKUP_STRING_LENGTH + 1);
    }

    /**
     * Returns a {@link File} object pointing to a path in a temp directory under current users
     * {@link Context#getDataDir}.
     *
     * @param context       - an application context.
     * @param timestamp     - generates file for this timestamp
     * @param suffix        - a filename suffix.
     * @return A file.
     */
    static File getFileWithSuffix(Context context, String timestamp, String suffix) {
        return new File(createTempDir(context, timestamp), timestamp + suffix);
    }

    /**
     * Returns a {@link File} object pointing to a path in a temp directory under current users
     * {@link Context#getDataDir}.
     *
     * @param context     - an application context.
     * @param timestamp   - generates file for this timestamp.
     * @param name        - a filename
     * @return A file.
     */
    static File getFile(Context context, String timestamp, String name) {
        return new File(getTempDir(context, timestamp), name);
    }

    /**
     * Deletes a directory and its contents recursively
     *
     * @param directory to delete
     */
    static void deleteDirectory(File directory) {
        File[] files = directory.listFiles();
        if (files != null) {
            for (File file : files) {
                if (!file.isDirectory()) {
                    file.delete();
                } else {
                    deleteDirectory(file);
                }
            }
        }
        directory.delete();
    }
}
