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

package android.cts.statsd.atom;

import java.nio.charset.StandardCharsets;
import java.util.Formatter;

/**
 * Print utility for byte[].
 */
public class BufferDebug {
    private static final int HALF_WIDTH = 8;

    /**
     * Number of bytes represented per row in hex output.
     */
    public static final int WIDTH = HALF_WIDTH * 2;

    /**
     * Return a string suitable for debugging.
     * - If the byte is printable as an ascii string, return that, in quotation marks,
     *   with a newline at the end.
     * - Otherwise, return the hexdump -C style output.
     *
     * @param buf the buffer
     * @param max print up to _max_ bytes, or the length of the string. If max is 0,
     *      print the whole contents of buf.
     */
    public static String debugString(byte[] buf, int max) {
        if (buf == null) {
            return "(null)";
        }
        if (buf.length == 0) {
            return "(length 0)";
        }

        int len = max;
        if (len <= 0 || len > buf.length) {
            max = len = buf.length;
        }

        if (isPrintable(buf, len)) {
            return "\"" + new String(buf, 0, len, StandardCharsets.UTF_8) + "\"\n";
        } else {
            return toHex(buf, len, max);
        }
    }

    private static String toHex(byte[] buf, int len, int max) {
        final StringBuilder str = new StringBuilder();

        // All but the last row
        int rows = len / WIDTH;
        for (int row = 0; row < rows; row++) {
            writeRow(str, buf, row * WIDTH, WIDTH, max);
        }

        // Last row
        if (len % WIDTH != 0) {
            writeRow(str, buf, rows * WIDTH, max - (rows * WIDTH), max);
        }

        // Final len
        str.append(String.format("%10d 0x%08x  ", buf.length, buf.length));
        if (buf.length != max) {
            str.append(String.format("truncated to %d 0x%08x", max, max));
        }
        str.append('\n');

        return str.toString();
    }

    private static void writeRow(StringBuilder str, byte[] buf, int start, int len, int max) {
        final Formatter f = new Formatter(str);

        // Start index
        f.format("%10d 0x%08x  ", start, start);

        // One past the last char we will print
        int end = start + len;
        // Number of missing caracters due to this being the last line.
        int padding = 0;
        if (start + WIDTH > max) {
            padding = WIDTH - (end % WIDTH);
            end = max;
        }

        // Hex
        for (int i = start; i < end; i++) {
            f.format("%02x ", buf[i]);
            if (i == start + HALF_WIDTH - 1) {
                str.append(" ");
            }
        }
        for (int i = 0; i < padding; i++) {
            str.append("   ");
        }
        if (padding >= HALF_WIDTH) {
            str.append(" ");
        }

        str.append("  ");
        for (int i = start; i < end; i++) {
            byte b = buf[i];
            if (isPrintable(b)) {
                str.append((char)b);
            } else {
                str.append('.');
            }
            if (i == start + HALF_WIDTH - 1) {
                str.append("  ");
            }
        }

        str.append('\n');
    }

    private static boolean isPrintable(byte[] buf, int len) {
        for (int i=0; i<len; i++) {
            if (!isPrintable(buf[i])) {
                return false;
            }
        }
        return true;
    }

    private static boolean isPrintable(byte c) {
        return c >= 0x20 && c <= 0x7e;
    }
}
