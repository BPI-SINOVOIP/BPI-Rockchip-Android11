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

package android.os.cts;

import android.os.AsyncTask;
import android.os.FileUtils;
import android.system.Os;
import android.system.OsConstants;
import android.test.AndroidTestCase;

import org.junit.Assert;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.RandomAccessFile;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

public class FileUtilsTest extends AndroidTestCase {
    private File mExpected;
    private File mActual;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mExpected = File.createTempFile("cts", "expected");
        mActual = File.createTempFile("cts", "actual");

        try (RandomAccessFile raf = new RandomAccessFile(mExpected, "rw")) {
            raf.writeUTF("Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        mExpected.delete();
        mActual.delete();
    }

    public void testCopyByteStream() throws Exception {
        final byte[] data = new byte[] { 42, 0, 1, 2, 3 };

        try (ByteArrayInputStream in = new ByteArrayInputStream(data);
                ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            FileUtils.copy(in, out);
            Assert.assertArrayEquals(out.toByteArray(), data);
        }
    }

    public void testCopyFileStream() throws Exception {
        try (FileInputStream in = new FileInputStream(mExpected);
                FileOutputStream out = new FileOutputStream(mActual)) {
            FileUtils.copy(in, out);
        }
        assertEquals(mExpected, mActual);
    }

    public void testCopyFileStreamListener() throws Exception {
        final Executor executor = AsyncTask.THREAD_POOL_EXECUTOR;
        final CompletableFuture<Long> event = new CompletableFuture<>();
        try (FileInputStream in = new FileInputStream(mExpected);
                FileOutputStream out = new FileOutputStream(mActual)) {
            FileUtils.copy(in, out, null, executor, (progress) -> {
                event.complete(progress);
            });
        }
        assertEquals(mExpected, mActual);
        assertEquals(mExpected.length(), (long) event.get(1, TimeUnit.SECONDS));
    }

    public void testCopyFileDescriptor() throws Exception {
        try (FileInputStream in = new FileInputStream(mExpected);
                FileOutputStream out = new FileOutputStream(mActual)) {
            FileUtils.copy(in.getFD(), out.getFD());
        }
        assertEquals(mExpected, mActual);
    }

    public void testCopyFileDescriptorListener() throws Exception {
        final Executor executor = AsyncTask.THREAD_POOL_EXECUTOR;
        final CompletableFuture<Long> event = new CompletableFuture<>();
        try (FileInputStream in = new FileInputStream(mExpected);
                FileOutputStream out = new FileOutputStream(mActual)) {
            FileUtils.copy(in.getFD(), out.getFD(), null, executor, (progress) -> {
                event.complete(progress);
            });
        }
        assertEquals(mExpected, mActual);
        assertEquals(mExpected.length(), (long) event.get(1, TimeUnit.SECONDS));
    }

    public void testClose() throws Exception {
        final FileInputStream fis = new FileInputStream("/dev/null");
        FileUtils.closeQuietly(fis);

        final FileDescriptor fd = Os.open("/dev/null", 0, OsConstants.O_RDONLY);
        FileUtils.closeQuietly(fd);
    }

    private static void assertEquals(File expected, File actual) throws Exception {
        final byte[] expectedRaw = new byte[(int) expected.length()];
        try (RandomAccessFile file = new RandomAccessFile(expected, "r")) {
            file.readFully(expectedRaw);
        }

        final byte[] actualRaw = new byte[(int) actual.length()];
        try (RandomAccessFile file = new RandomAccessFile(actual, "r")) {
            file.readFully(actualRaw);
        }

        Assert.assertArrayEquals(expectedRaw, actualRaw);
    }
}
