/*
 * Copyright (C) 2012 The Android Open Source Project
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
package com.android.tradefed.util;

import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.InputStreamSource;

import junit.framework.TestCase;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;

/** Unit tests for the {@link com.android.tradefed.util.StreamUtil} utility class */
public class StreamUtilTest extends TestCase {

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#getByteArrayListFromSource} works as
     * expected.
     */
    public void testGetByteArrayListFromSource() throws Exception {
        final String contents = "this is a string";
        final byte[] contentBytes = contents.getBytes();
        try (final InputStreamSource source = new ByteArrayInputStreamSource(contentBytes)) {
            final InputStream stream = source.createInputStream();
            final ByteArrayList output = StreamUtil.getByteArrayListFromStream(stream);
            final byte[] outputBytes = output.getContents();

            assertEquals(contentBytes.length, outputBytes.length);
            for (int i = 0; i < contentBytes.length; ++i) {
                assertEquals(contentBytes[i], outputBytes[i]);
            }
        }
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#getByteArrayListFromStream} works as
     * expected.
     */
    public void testGetByteArrayListFromStream() throws Exception {
        final String contents = "this is a string";
        final byte[] contentBytes = contents.getBytes();
        final ByteArrayList output = StreamUtil.getByteArrayListFromStream(
                new ByteArrayInputStream(contentBytes));
        final byte[] outputBytes = output.getContents();

        assertEquals(contentBytes.length, outputBytes.length);
        for (int i = 0; i < contentBytes.length; ++i) {
            assertEquals(contentBytes[i], outputBytes[i]);
        }
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#getStringFromSource} works as
     * expected.
     */
    public void testGetStringFromSource() throws Exception {
        final String contents = "this is a string";
        try (InputStreamSource source = new ByteArrayInputStreamSource(contents.getBytes())) {
            final InputStream stream = source.createInputStream();
            final String output = StreamUtil.getStringFromStream(stream);
            assertEquals(contents, output);
        }
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#getBufferedReaderFromStreamSrc} works
     * as expected.
     */
    public void testGetBufferedReaderFromInputStream() throws Exception {
        final String contents = "this is a string";
        BufferedReader output = null;
        try (InputStreamSource source = new ByteArrayInputStreamSource(contents.getBytes())) {
            output = StreamUtil.getBufferedReaderFromStreamSrc(source);
            assertEquals(contents, output.readLine());
        } finally {
            if (null != output) {
                output.close();
            }
        }
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#countLinesFromSource} works as
     * expected.
     */
    public void testCountLinesFromSource() throws Exception {
        final String contents = "foo\nbar\n\foo\n";
        final InputStreamSource source = new ByteArrayInputStreamSource(contents.getBytes());
        assertEquals(3, StreamUtil.countLinesFromSource(source));
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#getStringFromStream(InputStream)}
     * works as expected.
     */
    public void testGetStringFromStream() throws Exception {
        final String contents = "this is a string";
        final String output = StreamUtil.getStringFromStream(
                new ByteArrayInputStream(contents.getBytes()));
        assertEquals(contents, output);
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#getStringFromStream(InputStream,
     * long)} works as expected.
     */
    public void testGetStringFromStream_withLength() throws Exception {
        final String contents = "this is a string";
        final String output =
                StreamUtil.getStringFromStream(new ByteArrayInputStream(contents.getBytes()), 5);
        assertEquals("this ", output);
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#calculateCrc32(InputStream)} works as
     * expected.
     *
     * @throws IOException
     */
    public void testCalculateCrc32() throws IOException {
        final String source = getLargeText();
        final long crc32 = 3023941728L;
        ByteArrayInputStream inputSource = new ByteArrayInputStream(source.getBytes());
        long actualCrc32 = StreamUtil.calculateCrc32(inputSource);
        assertEquals(crc32, actualCrc32);
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#calculateMd5(InputStream)} works as
     * expected.
     *
     * @throws IOException
     */
    public void testCalculateMd5() throws IOException {
        final String source = "testtesttesttesttest";
        final String md5 = "f317f682fafe0309c6a423af0b4efa59";
        ByteArrayInputStream inputSource = new ByteArrayInputStream(source.getBytes());
        String actualMd5 = StreamUtil.calculateMd5(inputSource);
        assertEquals(md5, actualMd5);
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#calculateBase64Md5(InputStream)}
     * works as expected.
     *
     * @throws IOException
     */
    public void testCalculateBase64Md5() throws IOException {
        final String source = "testtesttesttesttest";
        final String base64Md5 = "8xf2gvr+AwnGpCOvC076WQ==";
        ByteArrayInputStream inputSource = new ByteArrayInputStream(source.getBytes());
        String actualBase64Md5 = StreamUtil.calculateBase64Md5(inputSource);
        assertEquals(base64Md5, actualBase64Md5);
    }

    public void testCopyStreams() throws Exception {
        String text = getLargeText();
        ByteArrayInputStream bais = new ByteArrayInputStream(text.getBytes());
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        StreamUtil.copyStreams(bais, baos);
        bais.close();
        baos.close();
        assertEquals(text, baos.toString());
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#copyStreams(InputStream,
     * OutputStream, long, long)} can copy partial content.
     */
    public void testCopyStreams_partialSuccess() throws Exception {
        String text = getLargeText();
        StringBuilder builder = new StringBuilder(33 * 1024);
        // Create a string longer than StreamUtil.BUF_SIZE
        while (builder.length() < 32 * 1024) {
            builder.append(text);
        }
        ByteArrayInputStream bais = new ByteArrayInputStream(builder.toString().getBytes());
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        // Skip the first 1kB, and read longer than StreamUtil.BUF_SIZE
        StreamUtil.copyStreams(bais, baos, 1024, 20 * 1024);
        bais.close();
        baos.close();
        assertEquals(builder.toString().substring(1024, 21 * 1024), baos.toString());
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#copyStreams(InputStream,
     * OutputStream, long, long)} cannot copy partial content if requested size is larger than
     * what's available.
     */
    public void testCopyStreams_partialFail() throws Exception {
        ByteArrayInputStream bais = null;
        try (ByteArrayOutputStream baos = new ByteArrayOutputStream()) {
            String text = getLargeText();
            bais = new ByteArrayInputStream(text.getBytes());
            // Skip the first 1kB, and read longer than the size of text
            StreamUtil.copyStreams(bais, baos, 10, text.length() + 1024);
            fail("IOException should be thrown when reading too much data.");
        } catch (IOException e) {
            // Ignore expected error.
        } finally {
            StreamUtil.close(bais);
        }
    }

    public void testCopyStreamToWriter() throws Exception {
        String text = getLargeText();
        ByteArrayInputStream bais = new ByteArrayInputStream(text.getBytes());
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Writer writer = new OutputStreamWriter(baos);
        StreamUtil.copyStreamToWriter(bais, writer);
        bais.close();
        writer.close();
        baos.close();
        assertEquals(text, baos.toString());
    }

    /**
     * Verify that {@link com.android.tradefed.util.StreamUtil#copyFileToStream(File, OutputStream)}
     * works as expected.
     *
     * @throws IOException
     */
    public void testCopyFileToStream() throws IOException {
        String text = getLargeText();
        File file = File.createTempFile("testCopyFileToStream", ".txt");
        try {
            FileUtil.writeToFile(text, file);
            try (ByteArrayOutputStream outStream = new ByteArrayOutputStream()) {
                StreamUtil.copyFileToStream(file, outStream);
                assertEquals(text, outStream.toString());
            }
        } finally {
            file.delete();
        }
    }

    /**
     * Returns a large chunk of text that's at least 16K in size
     */
    private String getLargeText() {
        String text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                + "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
                + "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
                + "aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in "
                + "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                + "occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit "
                + "anim id est laborum."; // 446 bytes
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 40; i ++) {
            sb.append(text);
        }
        return sb.toString();
    }
}
