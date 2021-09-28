/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.traceur;

import android.os.Build;
import android.os.AsyncTask;
import android.os.FileUtils;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import java.util.Collection;
import java.util.TreeMap;

/**
 * Utility functions for tracing.
 * Will call atrace or perfetto depending on the setting.
 */
public class TraceUtils {

    static final String TAG = "Traceur";

    public static final String TRACE_DIRECTORY = "/data/local/traces/";

    // To change Traceur to use atrace to collect traces,
    // change mTraceEngine to point to AtraceUtils().
    private static TraceEngine mTraceEngine = new PerfettoUtils();

    private static final Runtime RUNTIME = Runtime.getRuntime();

    public interface TraceEngine {
        public String getName();
        public String getOutputExtension();
        public boolean traceStart(Collection<String> tags, int bufferSizeKb, boolean apps,
            boolean longTrace, int maxLongTraceSizeMb, int maxLongTraceDurationMinutes);
        public void traceStop();
        public boolean traceDump(File outFile);
        public boolean isTracingOn();
    }

    public static String currentTraceEngine() {
        return mTraceEngine.getName();
    }

    public static boolean traceStart(Collection<String> tags, int bufferSizeKb, boolean apps,
            boolean longTrace, int maxLongTraceSizeMb, int maxLongTraceDurationMinutes) {
        return mTraceEngine.traceStart(tags, bufferSizeKb, apps,
            longTrace, maxLongTraceSizeMb, maxLongTraceDurationMinutes);
    }

    public static void traceStop() {
        mTraceEngine.traceStop();
    }

    public static boolean traceDump(File outFile) {
        return mTraceEngine.traceDump(outFile);
    }

    public static boolean isTracingOn() {
        return mTraceEngine.isTracingOn();
    }

    public static TreeMap<String, String> listCategories() {
        return AtraceUtils.atraceListCategories();
    }

    public static void clearSavedTraces() {
        String cmd = "rm -f " + TRACE_DIRECTORY + "trace-*.*trace";

        Log.v(TAG, "Clearing trace directory: " + cmd);
        try {
            Process rm = exec(cmd);

            if (rm.waitFor() != 0) {
                Log.e(TAG, "clearSavedTraces failed with: " + rm.exitValue());
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static Process exec(String cmd) throws IOException {
        return exec(cmd, null);
    }

    public static Process exec(String cmd, String tmpdir) throws IOException {
        return exec(cmd, tmpdir, true);
    }

    public static Process exec(String cmd, String tmpdir, boolean logOutput) throws IOException {
        String[] cmdarray = {"sh", "-c", cmd};
        String[] envp = {"TMPDIR=" + tmpdir};
        envp = tmpdir == null ? null : envp;

        Log.v(TAG, "exec: " + Arrays.toString(envp) + " " + Arrays.toString(cmdarray));

        Process process = RUNTIME.exec(cmdarray, envp);
        new Logger("traceService:stderr", process.getErrorStream());
        if (logOutput) {
            new Logger("traceService:stdout", process.getInputStream());
        }

        return process;
    }

    public static String getOutputFilename() {
        String format = "yyyy-MM-dd-HH-mm-ss";
        String now = new SimpleDateFormat(format, Locale.US).format(new Date());
        return String.format("trace-%s-%s-%s.%s", Build.BOARD, Build.ID, now,
            mTraceEngine.getOutputExtension());
    }

    public static File getOutputFile(String filename) {
        return new File(TraceUtils.TRACE_DIRECTORY, filename);
    }

    protected static void cleanupOlderFiles(final int minCount, final long minAge) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                try {
                    FileUtils.deleteOlderFiles(new File(TRACE_DIRECTORY), minCount, minAge);
                } catch (RuntimeException e) {
                    Log.e(TAG, "Failed to delete older traces", e);
                }
                return null;
            }
        }.execute();
    }

    /**
     * Streams data from an InputStream to an OutputStream
     */
    static class Streamer {
        private boolean mDone;

        Streamer(final String tag, final InputStream in, final OutputStream out) {
            new Thread(tag) {
                @Override
                public void run() {
                    int read;
                    byte[] buf = new byte[2 << 10];
                    try {
                        while ((read = in.read(buf)) != -1) {
                            out.write(buf, 0, read);
                        }
                    } catch (IOException e) {
                        Log.e(TAG, "Error while streaming " + tag);
                    } finally {
                        try {
                            out.close();
                        } catch (IOException e) {
                            // Welp.
                        }
                        synchronized (Streamer.this) {
                            mDone = true;
                            Streamer.this.notify();
                        }
                    }
                }
            }.start();
        }

        synchronized boolean isDone() {
            return mDone;
        }

        synchronized void waitForDone() {
            while (!isDone()) {
                try {
                    wait();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        }
    }

    /**
     * Streams data from an InputStream to an OutputStream
     */
    private static class Logger {

        Logger(final String tag, final InputStream in) {
            new Thread(tag) {
                @Override
                public void run() {
                    int read;
                    String line;
                    BufferedReader r = new BufferedReader(new InputStreamReader(in));
                    try {
                        while ((line = r.readLine()) != null) {
                            Log.e(TAG, tag + ": " + line);
                        }
                    } catch (IOException e) {
                        Log.e(TAG, "Error while streaming " + tag);
                    } finally {
                        try {
                            r.close();
                        } catch (IOException e) {
                            // Welp.
                        }
                    }
                }
            }.start();
        }
    }
}
