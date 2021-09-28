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

package android.content.pm.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.platform.test.annotations.AppModeFull;
import android.service.dataloader.DataLoaderService;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.Reader;
import java.util.Arrays;
import java.util.Optional;
import java.util.stream.Collectors;
import java.util.stream.Stream;

@RunWith(AndroidJUnit4.class)
@AppModeFull
public class PackageManagerShellCommandIncrementalTest {
    private static final String TEST_APP_PACKAGE = "com.example.helloworld";

    private static final String TEST_APK_PATH = "/data/local/tmp/cts/content/";
    private static final String TEST_APK = "HelloWorld5.apk";
    private static final String TEST_APK_SPLIT = "HelloWorld5_hdpi-v4.apk";

    private static UiAutomation getUiAutomation() {
        return InstrumentationRegistry.getInstrumentation().getUiAutomation();
    }

    private static String executeShellCommand(String command) throws IOException {
        final ParcelFileDescriptor stdout = getUiAutomation().executeShellCommand(command);
        try (InputStream inputStream = new ParcelFileDescriptor.AutoCloseInputStream(stdout)) {
            return readFullStream(inputStream);
        }
    }

    private static String executeShellCommand(String command, File[] inputs)
            throws IOException {
        return executeShellCommand(command, inputs, Stream.of(inputs).mapToLong(
                File::length).toArray());
    }

    private static String executeShellCommand(String command, File[] inputs, long[] expected)
            throws IOException {
        assertEquals(inputs.length, expected.length);
        final ParcelFileDescriptor[] pfds =
                InstrumentationRegistry.getInstrumentation().getUiAutomation()
                        .executeShellCommandRw(command);
        ParcelFileDescriptor stdout = pfds[0];
        ParcelFileDescriptor stdin = pfds[1];
        try (FileOutputStream outputStream = new ParcelFileDescriptor.AutoCloseOutputStream(
                stdin)) {
            for (int i = 0; i < inputs.length; i++) {
                try (FileInputStream inputStream = new FileInputStream(inputs[i])) {
                    writeFullStream(inputStream, outputStream, expected[i]);
                }
            }
        }
        try (InputStream inputStream = new ParcelFileDescriptor.AutoCloseInputStream(stdout)) {
            return readFullStream(inputStream);
        }
    }

    private static String readFullStream(InputStream inputStream, long expected)
            throws IOException {
        ByteArrayOutputStream result = new ByteArrayOutputStream();
        writeFullStream(inputStream, result, expected);
        return result.toString("UTF-8");
    }

    private static String readFullStream(InputStream inputStream) throws IOException {
        return readFullStream(inputStream, -1);
    }

    private static void writeFullStream(InputStream inputStream, OutputStream outputStream,
            long expected)
            throws IOException {
        byte[] buffer = new byte[1024];
        long total = 0;
        int length;
        while ((length = inputStream.read(buffer)) != -1 && (expected < 0 || total < expected)) {
            outputStream.write(buffer, 0, length);
            total += length;
        }
        if (expected > 0) {
            assertEquals(expected, total);
        }
    }

    private static String waitForSubstring(InputStream inputStream, String expected)
            throws IOException {
        try (Reader reader = new InputStreamReader(inputStream);
             BufferedReader lines = new BufferedReader(reader)) {
            return lines.lines().filter(line -> line.contains(expected)).findFirst().orElse("");
        }
    }

    @Before
    public void onBefore() throws Exception {
        checkIncrementalDeliveryFeature();
        uninstallPackageSilently(TEST_APP_PACKAGE);
        assertFalse(isAppInstalled(TEST_APP_PACKAGE));
    }

    @After
    public void onAfter() throws Exception {
        uninstallPackageSilently(TEST_APP_PACKAGE);
        assertFalse(isAppInstalled(TEST_APP_PACKAGE));
        assertEquals(null, getSplits(TEST_APP_PACKAGE));
    }

    private void checkIncrementalDeliveryFeature() throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        Assume.assumeTrue(context.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_INCREMENTAL_DELIVERY));
    }

    @Test
    public void testInstallWithIdSig() throws Exception {
        installPackage(TEST_APK);
        assertTrue(isAppInstalled(TEST_APP_PACKAGE));
    }

    @Test
    public void testInstallWithIdSigAndSplit() throws Exception {
        File apkfile = new File(createApkPath(TEST_APK));
        File splitfile = new File(createApkPath(TEST_APK_SPLIT));
        File[] files = new File[]{apkfile, splitfile};
        String param = Arrays.stream(files).map(
                file -> file.getName() + ":" + file.length()).collect(Collectors.joining(" "));
        assertEquals("Success\n", executeShellCommand(
                String.format("pm install-incremental -t -g -S %s %s",
                        (apkfile.length() + splitfile.length()), param),
                files));
        assertTrue(isAppInstalled(TEST_APP_PACKAGE));
        assertEquals("base, config.hdpi", getSplits(TEST_APP_PACKAGE));
    }

    @Test
    public void testInstallWithIdSigInvalidLength() throws Exception {
        File file = new File(createApkPath(TEST_APK));
        assertTrue(
                executeShellCommand("pm install-incremental -t -g -S " + (file.length() - 1),
                        new File[]{file}).contains(
                        "Failure"));
        assertFalse(isAppInstalled(TEST_APP_PACKAGE));
    }

    @Test
    public void testInstallWithIdSigStreamIncompleteData() throws Exception {
        File file = new File(createApkPath(TEST_APK));
        long length = file.length();
        // Streaming happens in blocks of 1024 bytes, new length will not stream the last block.
        long newLength = length - (length % 1024 == 0 ? 1024 : length % 1024);
        assertTrue(
                executeShellCommand("pm install-incremental -t -g -S " + length,
                        new File[]{file}, new long[]{newLength}).contains(
                        "Failure"));
        assertFalse(isAppInstalled(TEST_APP_PACKAGE));
    }

    @Test
    public void testInstallWithIdSigStreamIncompleteDataForSplit() throws Exception {
        File apkfile = new File(createApkPath(TEST_APK));
        File splitfile = new File(createApkPath(TEST_APK_SPLIT));
        long splitLength = splitfile.length();
        // Don't fully stream the split.
        long newSplitLength = splitLength - (splitLength % 1024 == 0 ? 1024 : splitLength % 1024);
        File[] files = new File[]{apkfile, splitfile};
        String param = Arrays.stream(files).map(
                file -> file.getName() + ":" + file.length()).collect(Collectors.joining(" "));
        assertTrue(executeShellCommand(
                String.format("pm install-incremental -t -g -S %s %s",
                        (apkfile.length() + splitfile.length()), param),
                files, new long[]{apkfile.length(), newSplitLength}).contains(
                "Failure"));
        assertFalse(isAppInstalled(TEST_APP_PACKAGE));
    }

    static class TestDataLoaderService extends DataLoaderService {
    }

    @Test
    public void testDataLoaderServiceDefaultImplementation() {
        DataLoaderService service = new TestDataLoaderService();
        assertEquals(null, service.onCreateDataLoader(null));
        IBinder binder = service.onBind(null);
        assertNotEquals(null, binder);
        assertEquals(binder, service.onBind(new Intent()));
    }

    @LargeTest
    @Test
    public void testInstallSysTrace() throws Exception {
        // Async atrace dump uses less resources but requires periodic pulls.
        // Overall timeout of 30secs in 100ms intervals should be enough.
        final int atraceDumpIterations = 300;
        final int atraceDumpDelayMs = 100;

        final String expected = "|page_read:";
        final ByteArrayOutputStream result = new ByteArrayOutputStream();
        final Thread readFromProcess = new Thread(() -> {
            try {
                executeShellCommand("atrace --async_start -b 1024 -c adb");
                try {
                    for (int i = 0; i < atraceDumpIterations; ++i) {
                        final ParcelFileDescriptor stdout = getUiAutomation().executeShellCommand(
                                "atrace --async_dump");
                        try (InputStream inputStream =
                                     new ParcelFileDescriptor.AutoCloseInputStream(
                                stdout)) {
                            final String found = waitForSubstring(inputStream, expected);
                            if (!TextUtils.isEmpty(found)) {
                                result.write(found.getBytes());
                                return;
                            }
                            Thread.currentThread().sleep(atraceDumpDelayMs);
                        } catch (InterruptedException ignored) {
                        }
                    }
                } finally {
                    executeShellCommand("atrace --async_stop");
                }
            } catch (IOException ignored) {
            }
        });
        readFromProcess.start();

        for (int i = 0; i < 3; ++i) {
            installPackage(TEST_APK);
            assertTrue(isAppInstalled(TEST_APP_PACKAGE));
            uninstallPackageSilently(TEST_APP_PACKAGE);
        }

        readFromProcess.join();
        assertNotEquals(0, result.size());
    }

    private boolean isAppInstalled(String packageName) throws IOException {
        final String commandResult = executeShellCommand("pm list packages");
        final int prefixLength = "package:".length();
        return Arrays.stream(commandResult.split("\\r?\\n"))
                .anyMatch(line -> line.substring(prefixLength).equals(packageName));
    }

    private String getSplits(String packageName) throws IOException {
        final String commandResult = executeShellCommand("pm dump " + packageName);
        final String prefix = "    splits=[";
        final int prefixLength = prefix.length();
        Optional<String> maybeSplits = Arrays.stream(commandResult.split("\\r?\\n"))
                .filter(line -> line.startsWith(prefix)).findFirst();
        if (!maybeSplits.isPresent()) {
            return null;
        }
        String splits = maybeSplits.get();
        return splits.substring(prefixLength, splits.length() - 1);
    }

    private static String createApkPath(String baseName) {
        return TEST_APK_PATH + baseName;
    }

    private void installPackage(String baseName) throws IOException {
        File file = new File(createApkPath(baseName));
        assertEquals("Success\n",
                executeShellCommand("pm install-incremental -t -g " + file.getPath()));
    }

    private String uninstallPackageSilently(String packageName) throws IOException {
        return executeShellCommand("pm uninstall " + packageName);
    }
}

