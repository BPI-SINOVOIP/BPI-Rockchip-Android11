/*
 * Copyright (C) 2016 The Android Open Source Project
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

import com.google.common.io.Closeables;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Reader;
import java.nio.charset.StandardCharsets;


public class StreamUtil {

    // 16K buffer size
    private static final int BUFFER_SIZE = 16 * 1024;
    /**
     * Copies contents of origStream to destStream.
     * <p/>
     * Recommended to provide a buffered stream for input and output
     *
     * @param inStream the {@link InputStream}
     * @param outStream the {@link OutputStream}
     * @throws IOException
     */
    public static void copyStreams(InputStream inStream, OutputStream outStream)
            throws IOException {
        byte[] buf = new byte[BUFFER_SIZE];
        int size = -1;
        while ((size = inStream.read(buf)) != -1) {
            outStream.write(buf, 0, size);
        }
    }

    /**
     * Reads {@code inputStream} converting it into a string. Does NOT close it.
     *
     * @throws IOException
     */
    public static String readInputStream(InputStream inputStream) throws IOException {
        ByteArrayOutputStream result = new ByteArrayOutputStream();
        byte[] buffer = new byte[1024];
        int length;
        while ((length = inputStream.read(buffer)) != -1) {
            result.write(buffer, 0, length);
        }
        return result.toString(StandardCharsets.UTF_8.name());
    }

    public static void drainAndClose(Reader reader) {
        try {
            while (reader.read() >= 0) {}
        } catch (IOException ignored) {}
        Closeables.closeQuietly(reader);
    }
}
