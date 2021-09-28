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

package com.android.compatibility.common.util;

import android.app.Instrumentation;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.ArraySet;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * A utility class interact with "am monitor"
 */
public final class AmMonitor {
    private static final String TAG = "AmMonitor";
    public static final String WAIT_FOR_EARLY_ANR =
            "Waiting after early ANR...  available commands:";
    public static final String WAIT_FOR_ANR =
            "Waiting after ANR...  available commands:";
    public static final String WAIT_FOR_CRASHED =
            "Waiting after crash...  available commands:";
    public static final String MONITOR_READY =
            "Monitoring activity manager...  available commands:";

    /**
     * Command for the {@link #sendCommand}: continue the process
     */
    public static final String CMD_CONTINUE = "c";

    /**
     * Command for the {@link #sendCommand}: kill the process
     */
    public static final String CMD_KILL = "k";

    /**
     * Command for the {@link #sendCommand}: quit the monitor
     */
    public static final String CMD_QUIT = "q";

    private final Instrumentation mInstrumentation;
    private final ParcelFileDescriptor mReadFd;
    private final FileInputStream mReadStream;
    private final BufferedReader mReadReader;
    private final ParcelFileDescriptor mWriteFd;
    private final FileOutputStream mWriteStream;
    private final PrintWriter mWritePrinter;
    private final Thread mReaderThread;

    private final ArraySet<String> mNotExpected = new ArraySet<>();
    private final ArrayList<String> mPendingLines = new ArrayList<>();

    /**
     * Construct an instance of this class.
     */
    public AmMonitor(final Instrumentation instrumentation, final String[] notExpected) {
        mInstrumentation = instrumentation;
        ParcelFileDescriptor[] pfds = instrumentation.getUiAutomation()
                .executeShellCommandRw("am monitor");
        mReadFd = pfds[0];
        mReadStream = new ParcelFileDescriptor.AutoCloseInputStream(mReadFd);
        mReadReader = new BufferedReader(new InputStreamReader(mReadStream));
        mWriteFd = pfds[1];
        mWriteStream = new ParcelFileDescriptor.AutoCloseOutputStream(mWriteFd);
        mWritePrinter = new PrintWriter(new BufferedOutputStream(mWriteStream));
        if (notExpected != null) {
            mNotExpected.addAll(Arrays.asList(notExpected));
        }
        mReaderThread = new ReaderThread();
        mReaderThread.start();
        waitFor(MONITOR_READY, 3600000L);
    }

    /**
     * Wait for the given output.
     *
     * @return true if it was successful, false if it got a timeout.
     */
    public boolean waitFor(final String expected, final long timeout) {
        final long waitUntil = SystemClock.uptimeMillis() + timeout;
        synchronized (mPendingLines) {
            while (true) {
                while (mPendingLines.size() == 0) {
                    final long now = SystemClock.uptimeMillis();
                    if (now >= waitUntil) {
                        String msg = "Timed out waiting for next line: expected=" + expected;
                        Log.d(TAG, msg);
                        throw new IllegalStateException(msg);
                    }
                    try {
                        mPendingLines.wait(waitUntil - now);
                    } catch (InterruptedException e) {
                    }
                }
                final String line = mPendingLines.remove(0);
                if (TextUtils.equals(line, expected)) {
                    return true;
                } else if (TextUtils.equals(line, WAIT_FOR_EARLY_ANR)
                        || TextUtils.equals(line, WAIT_FOR_ANR)
                        || TextUtils.equals(line, WAIT_FOR_CRASHED)) {
                    // If we are getting any of the unexpected state,
                    // for example, get a crash while waiting for an ANR,
                    // it could be from another unrelated process, kill it directly.
                    sendCommand(CMD_KILL);
                }
            }
        }
    }

    /**
     * Finish the monitor and close the streams.
     */
    public void finish() {
        sendCommand(CMD_QUIT);
        try {
            mWriteStream.close();
        } catch (IOException e) {
        }
        try {
            mReadStream.close();
        } catch (IOException e) {
        }
    }

    /**
     * Send the command to the interactive command.
     *
     * @param cmd could be {@link #CMD_KILL}, {@link #CMD_QUIT} or {@link #CMD_CONTINUE}.
     */
    public void sendCommand(final String cmd) {
        synchronized (mPendingLines) {
            mWritePrinter.println(cmd);
            mWritePrinter.flush();
        }
    }

    private final class ReaderThread extends Thread {
        @Override
        public void run() {
            try {
                String line;
                while ((line = mReadReader.readLine()) != null) {
                    Log.i(TAG, "debug: " + line);
                    if (mNotExpected.contains(line)) {
                        // If we are getting any of the unexpected state,
                        // for example, get a crash while waiting for an ANR,
                        // it could be from another unrelated process, kill it directly.
                        sendCommand(CMD_KILL);
                        continue;
                    }
                    synchronized (mPendingLines) {
                        mPendingLines.add(line);
                        mPendingLines.notifyAll();
                    }
                }
            } catch (IOException e) {
                Log.w(TAG, "Failed reading", e);
            }
        }
    }
}
