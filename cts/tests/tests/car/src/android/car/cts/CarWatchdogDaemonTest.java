/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.car.cts;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import android.os.UserHandle;
import android.system.Os;
import android.system.StructStatVfs;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.RequiredFeatureRule;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.lang.Math;
import java.nio.file.Files;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class CarWatchdogDaemonTest {
    @ClassRule
    public static final RequiredFeatureRule sRequiredFeatureRule = new RequiredFeatureRule(
            PackageManager.FEATURE_AUTOMOTIVE);

    private static final String TAG = CarWatchdogDaemonTest.class.getSimpleName();

    private static final String CAR_WATCHDOG_SERVICE_NAME
            = "android.automotive.watchdog.ICarWatchdog/default";

    private static final int MAX_WRITE_BYTES = 100 * 1000;
    private static final int CAPTURE_WAIT_MS = 10 * 1000;

    private static final String VALUE_PERCENT_REGEX_PAIR = ",\\s(\\d+),\\s\\d+\\.\\d+%";
    private static final Pattern TOP_N_WRITES_LINE_PATTERN = Pattern.compile("(\\d+),\\s(\\S*)" +
            VALUE_PERCENT_REGEX_PAIR + VALUE_PERCENT_REGEX_PAIR + VALUE_PERCENT_REGEX_PAIR +
            VALUE_PERCENT_REGEX_PAIR);

    private File testDir;

    @Before
    public void setUp() throws IOException {
        File dataDir = getContext().getDataDir();
        testDir = Files.createTempDirectory(dataDir.toPath(),
                "CarWatchdogDaemon").toFile();
    }

    @After
    public void tearDown() {
        testDir.delete();
    }

    @Test
    public void testRecordsIoPerformanceData() throws Exception {
        String packageName = getContext().getPackageName();
        runShellCommand("dumpsys " + CAR_WATCHDOG_SERVICE_NAME
                + " --start_io --interval 5 --max_duration 120 --filter_packages " + packageName);
        long writtenBytes = writeToDisk(testDir);
        assertWithMessage("Failed to write data to dir '" + testDir.getAbsolutePath() + "'").that(
                writtenBytes).isGreaterThan(0L);
        // Sleep twice the collection interval to capture the entire write.
        Thread.sleep(CAPTURE_WAIT_MS);
        String contents = runShellCommand("dumpsys " + CAR_WATCHDOG_SERVICE_NAME + " --stop_io");
        Log.i(TAG, "stop results:" + contents);
        assertWithMessage("Failed to custom collect I/O performance data").that(
                contents).isNotEmpty();
        long recordedBytes = parseDump(contents, UserHandle.getUserId(Process.myUid()),
                packageName);
        assertThat(recordedBytes).isAtLeast(writtenBytes);
    }

    private static Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }

    private static void writeToFos(FileOutputStream fos, long maxSize) throws IOException {
        while (maxSize != 0) {
            int writeSize = (int) Math.min(Integer.MAX_VALUE,
                    Math.min(Runtime.getRuntime().freeMemory(), maxSize));
            Log.i(TAG, "writeSize:" + writeSize);
            try {
                fos.write(new byte[writeSize]);
            } catch (InterruptedIOException e) {
                Thread.currentThread().interrupt();
                continue;
            }
            maxSize -= writeSize;
        }
    }

    private static long writeToDisk(File dir) throws Exception {
        if (!dir.exists()) {
            throw new FileNotFoundException(
                    "directory '" + dir.getAbsolutePath() + "' doesn't exist");
        }
        StructStatVfs stat;
        stat = Os.statvfs(dir.getAbsolutePath());
        // Write enough data so the I/O performance data collector can capture the write in
        // the top N writes.
        long limit = (long) (stat.f_bfree * stat.f_frsize * ((double) 2 / 3));
        long size = Math.min(MAX_WRITE_BYTES, limit);
        File uniqueFile = new File(dir, Long.toString(System.nanoTime()));
        FileOutputStream fos = new FileOutputStream(uniqueFile);
        Log.d(TAG, "Attempting to write " + size + " bytes");
        writeToFos(fos, size);
        fos.getFD().sync();
        return size;
    }

    /**
     * Parse the custom I/O performance data dump generated by the carwatchdog daemon.
     *
     * Format of the dump:
     *
     * ProcStat collector failed to access the file /proc/stat
     * ... <Skipping unrelated text> ...
     *
     * Top N Writes:
     * -------------
     * Android User ID, Package Name, Foreground Bytes, Foreground Bytes %, Foreground Fsync, ...
     * 10, android.car.cts, 0, 0.00%, 0, 0.00%, 348516352, 100.00%, 1, 33.33%
     * 0, root, 389120, 84.82%, 2, 22.22%, 0, 0.00%, 0, 0.00%
     * 10, shared:android.uid.bluetooth, 36864, 8.04%, 2, 22.22%, 4096, 0.00%, 2, 66.67%
     * 0, system, 32768, 7.14%, 5, 55.56%, 0, 0.00%, 0, 0.00%
     * 0, shared:com.google.uid.shared, 0, 0.00%, 0, 0.00%, 0, 0.00%, 0, 0.00%
     *
     * ... <Repeats on multiple collections> ...
     *
     * @param content     Content of the dump.
     * @param userId      UserId of the current process.
     * @param packageName Package name of the current process.
     * @return Total written bytes recorded for the current userId and package name.
     */
    private static long parseDump(String content, int userId, String packageName) throws Exception {
        long writtenBytes = 0;
        Section curSection = Section.NONE;
        String errorLines = "";
        for (String line : content.split("\\r?\\n")) {
            if (line.isEmpty() || line.startsWith("-") || line.startsWith("=")) {
                if (curSection == Section.WRITTEN_BYTES_DATA_SECTION) {
                    // Marks the end of data section.
                    curSection = Section.NONE;
                }
                continue;
            }
            // A collector fails to access its data source when there are SELinux policy violations.
            if (line.contains("collector failed to access")) {
                errorLines += "\n" + line;
            }
            if (line.matches("Top N Writes:")) {
                curSection = Section.WRITTEN_BYTES_HEADER_SECTION;
                continue;
            }
            if (curSection == Section.WRITTEN_BYTES_HEADER_SECTION) {
                // Skip the header line.
                curSection = Section.WRITTEN_BYTES_DATA_SECTION;
                continue;
            }
            if (curSection != Section.WRITTEN_BYTES_DATA_SECTION) {
                continue;
            }
            Matcher m = TOP_N_WRITES_LINE_PATTERN.matcher(line);
            if (!m.matches() || m.groupCount() != 6) {
                throw new IllegalStateException(
                        "'Top N Writes' data line '" + line + "' doesn't match regex '"
                                + TOP_N_WRITES_LINE_PATTERN.toString() + "'");
            }
            if (Integer.valueOf(m.group(1), 10) != userId ||
                    !String.valueOf(m.group(2)).equals(packageName)) {
                continue;
            }
            writtenBytes += Integer.valueOf(m.group(3), 10);
            writtenBytes += Integer.valueOf(m.group(5), 10);
            curSection = Section.NONE;
        }
        if (!errorLines.isEmpty()) {
            throw new IllegalStateException(
                    "One or more collectors failed to access their data source. Errors:" +
                            errorLines);
        }
        return writtenBytes;
    }

    private enum Section {
        WRITTEN_BYTES_HEADER_SECTION,
        WRITTEN_BYTES_DATA_SECTION,
        NONE,
    }
}
