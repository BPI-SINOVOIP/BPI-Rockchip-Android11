/*
 * Copyright (C) 2014 The Android Open Source Project
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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.ActivityManager;
import android.app.ActivityManager.MemoryInfo;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.os.StatFs;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Predicate;

public class SystemUtil {
    private static final String TAG = "CtsSystemUtil";
    private static final long TIMEOUT_MILLIS = 10000;

    public static long getFreeDiskSize(Context context) {
        final StatFs statFs = new StatFs(context.getFilesDir().getAbsolutePath());
        return (long)statFs.getAvailableBlocks() * statFs.getBlockSize();
    }

    public static long getFreeMemory(Context context) {
        final MemoryInfo info = new MemoryInfo();
        ((ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE)).getMemoryInfo(info);
        return info.availMem;
    }

    public static long getTotalMemory(Context context) {
        final MemoryInfo info = new MemoryInfo();
        ((ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE)).getMemoryInfo(info);
        return info.totalMem;
    }

    /**
     * Executes a shell command using shell user identity, and return the standard output in string
     * <p>Note: calling this function requires API level 21 or above
     * @param instrumentation {@link Instrumentation} instance, obtained from a test running in
     * instrumentation framework
     * @param cmd the command to run
     * @return the standard output of the command
     * @throws Exception
     */
    public static String runShellCommand(Instrumentation instrumentation, String cmd)
            throws IOException {
        return runShellCommand(instrumentation.getUiAutomation(), cmd);
    }

    /**
     * Executes a shell command using shell user identity, and return the standard output in string
     * <p>Note: calling this function requires API level 21 or above
     * @param automation {@link UiAutomation} instance, obtained from a test running in
     * instrumentation framework
     * @param cmd the command to run
     * @return the standard output of the command
     * @throws Exception
     */
    public static String runShellCommand(UiAutomation automation, String cmd)
            throws IOException {
        return new String(runShellCommandByteOutput(automation, cmd));
    }

    /**
     * Executes a shell command using shell user identity, and return the standard output as a byte
     * array
     * <p>Note: calling this function requires API level 21 or above
     *
     * @param automation {@link UiAutomation} instance, obtained from a test running in
     *                   instrumentation framework
     * @param cmd        the command to run
     * @return the standard output of the command as a byte array
     */
    static byte[] runShellCommandByteOutput(UiAutomation automation, String cmd)
            throws IOException {
        Log.v(TAG, "Running command: " + cmd);
        if (cmd.startsWith("pm grant ") || cmd.startsWith("pm revoke ")) {
            throw new UnsupportedOperationException("Use UiAutomation.grantRuntimePermission() "
                    + "or revokeRuntimePermission() directly, which are more robust.");
        }
        ParcelFileDescriptor pfd = automation.executeShellCommand(cmd);
        try (FileInputStream fis = new ParcelFileDescriptor.AutoCloseInputStream(pfd)) {
            return FileUtils.readInputStreamFully(fis);
        }
    }

    /**
     * Simpler version of {@link #runShellCommand(Instrumentation, String)}.
     */
    public static String runShellCommand(String cmd) {
        try {
            return runShellCommand(InstrumentationRegistry.getInstrumentation(), cmd);
        } catch (IOException e) {
            fail("Failed reading command output: " + e);
            return "";
        }
    }

    /**
     * Same as {@link #runShellCommand(String)}, with optionally
     * check the result using {@code resultChecker}.
     */
    public static String runShellCommand(String cmd, Predicate<String> resultChecker) {
        final String result = runShellCommand(cmd);
        if (resultChecker != null) {
            assertTrue("Assertion failed. Command was: " + cmd + "\n"
                    + "Output was:\n" + result,
                    resultChecker.test(result));
        }
        return result;
    }

    /**
     * Same as {@link #runShellCommand(String)}, but fails if the output is not empty.
     */
    public static String runShellCommandForNoOutput(String cmd) {
        final String result = runShellCommand(cmd);
        assertTrue("Command failed. Command was: " + cmd + "\n"
                + "Didn't expect any output, but the output was:\n" + result,
                result.length() == 0);
        return result;
    }

    /**
     * Runs a command and print the result on logcat.
     */
    public static void runCommandAndPrintOnLogcat(String logtag, String cmd) {
        Log.i(logtag, "Executing: " + cmd);
        final String output = runShellCommand(cmd);
        for (String line : output.split("\\n", -1)) {
            Log.i(logtag, line);
        }
    }

    /**
     * Runs a command and return the section matching the patterns.
     *
     * @see TextUtils#extractSection
     */
    public static String runCommandAndExtractSection(String cmd,
            String extractionStartRegex, boolean startInclusive,
            String extractionEndRegex, boolean endInclusive) {
        return TextUtils.extractSection(runShellCommand(cmd), extractionStartRegex, startInclusive,
                extractionEndRegex, endInclusive);
    }

    /**
     * Runs a {@link ThrowingSupplier} adopting Shell's permissions, and returning the result.
     */
    public static <T> T runWithShellPermissionIdentity(@NonNull ThrowingSupplier<T> supplier) {
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        AtomicReference<T> result = new AtomicReference<>();
        runWithShellPermissionIdentity(automan, () -> result.set(supplier.get()));
        return result.get();
    }

    /**
     * Runs a {@link ThrowingSupplier} adopting a subset of Shell's permissions,
     * and returning the result.
     */
    public static <T> T runWithShellPermissionIdentity(@NonNull ThrowingSupplier<T> supplier,
            String... permissions) {
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        AtomicReference<T> result = new AtomicReference<>();
        runWithShellPermissionIdentity(automan, () -> result.set(supplier.get()), permissions);
        return result.get();
    }

    /**
     * Runs a {@link ThrowingRunnable} adopting Shell's permissions.
     */
    public static void runWithShellPermissionIdentity(@NonNull ThrowingRunnable runnable) {
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        runWithShellPermissionIdentity(automan, runnable);
    }

    /**
     * Runs a {@link ThrowingRunnable} adopting a subset of Shell's permissions.
     */
    public static void runWithShellPermissionIdentity(@NonNull ThrowingRunnable runnable,
            String... permissions) {
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        runWithShellPermissionIdentity(automan, runnable, permissions);
    }

    /**
     * Runs a {@link ThrowingRunnable} adopting Shell's permissions, where you can specify the
     * uiAutomation used.
     */
    public static void runWithShellPermissionIdentity(
            @NonNull UiAutomation automan, @NonNull ThrowingRunnable runnable) {
        runWithShellPermissionIdentity(automan, runnable, null /* permissions */);
    }

    /**
     * Runs a {@link ThrowingRunnable} adopting Shell's permissions, where you can specify the
     * uiAutomation used.
     * @param automan UIAutomation to use.
     * @param runnable The code to run with Shell's identity.
     * @param permissions A subset of Shell's permissions. Passing {@code null} will use all
     *                    available permissions.
     */
    public static void runWithShellPermissionIdentity(@NonNull UiAutomation automan,
            @NonNull ThrowingRunnable runnable, String... permissions) {
        automan.adoptShellPermissionIdentity(permissions);
        try {
            runnable.run();
        } catch (Exception e) {
            throw new RuntimeException("Caught exception", e);
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    /**
     * Calls a {@link Callable} adopting Shell's permissions.
     */
    public static <T> T callWithShellPermissionIdentity(@NonNull Callable<T> callable)
            throws Exception {
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        automan.adoptShellPermissionIdentity();
        try {
            return callable.call();
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    /**
     * Calls a {@link Callable} adopting Shell's permissions.
     *
     * @param callable The code to call with Shell's identity.
     * @param permissions A subset of Shell's permissions. Passing {@code null} will use all
     *                    available permissions.     */
    public static <T> T callWithShellPermissionIdentity(@NonNull Callable<T> callable,
            String... permissions) throws Exception {
        final UiAutomation automan = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        automan.adoptShellPermissionIdentity(permissions);
        try {
            return callable.call();
        } finally {
            automan.dropShellPermissionIdentity();
        }
    }

    /**
     * Make sure that a {@link Runnable} eventually finishes without throwing a {@link
     * Exception}.
     *
     * @param r The {@link Runnable} to run.
     */
    public static void eventually(@NonNull ThrowingRunnable r) {
        eventually(r, TIMEOUT_MILLIS);
    }

    /**
     * Make sure that a {@link Runnable} eventually finishes without throwing a {@link
     * Exception}.
     *
     * @param r The {@link Runnable} to run.
     * @param r The number of milliseconds to wait for r to not throw
     */
    public static void eventually(@NonNull ThrowingRunnable r, long timeoutMillis) {
        long start = System.currentTimeMillis();

        while (true) {
            try {
                r.run();
                return;
            } catch (Throwable e) {
                if (System.currentTimeMillis() - start < timeoutMillis) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException ignored) {
                        throw new RuntimeException(e);
                    }
                } else {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    /**
     * Make sure that a {@link Callable} eventually finishes without throwing a {@link
     * Exception}.
     *
     * @param c The {@link Callable} to run.
     *
     * @return The return value of {@code c}
     */
    public static <T> T getEventually(@NonNull Callable<T> c) throws Exception {
        return getEventually(c, TIMEOUT_MILLIS);
    }

    /**
     * Make sure that a {@link Callable} eventually finishes without throwing a {@link
     * Exception}.
     *
     * @param c The {@link Callable} to run.
     * @param timeoutMillis The number of milliseconds to wait for r to not throw
     *
     * @return The return value of {@code c}
     */
    public static <T> T getEventually(@NonNull Callable<T> c, long timeoutMillis) throws Exception {
        long start = System.currentTimeMillis();

        while (true) {
            try {
                return c.call();
            } catch (Throwable e) {
                if (System.currentTimeMillis() - start < timeoutMillis) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException ignored) {
                        throw new RuntimeException(e);
                    }
                } else {
                    throw e;
                }
            }
        }
    }
}
