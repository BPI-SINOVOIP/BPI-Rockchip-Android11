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

package android.carrierapi.cts;

import javax.annotation.Nonnull;

/**
 * Utility class for converting between hex Strings and bitwise representations.
 */
public class IccUtils {

    // A table mapping from a number to a hex character for fast encoding hex strings.
    private static final char[] HEX_CHARS = {
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    @Nonnull
    public static String bytesToHexString(byte[] bytes) {
        StringBuilder ret = new StringBuilder(2 * bytes.length);
        for (int i = 0 ; i < bytes.length ; i++) {
            int b;
            b = 0x0f & (bytes[i] >> 4);
            ret.append(HEX_CHARS[b]);
            b = 0x0f & bytes[i];
            ret.append(HEX_CHARS[b]);
        }
        return ret.toString();
    }

    /**
     * Converts a hex String to a byte array.
     *
     * @param s A string of hexadecimal characters, must be an even number of
     *          chars long
     *
     * @return byte array representation
     *
     * @throws RuntimeException on invalid format
     */
    public static byte[] hexStringToBytes(String s) {
        byte[] ret;

        if (s == null) return null;

        int sz = s.length();

        ret = new byte[sz/2];

        for (int i=0 ; i <sz ; i+=2) {
            ret[i/2] = (byte) ((hexCharToInt(s.charAt(i)) << 4) | hexCharToInt(s.charAt(i+1)));
        }

        return ret;
    }

    /**
     * Converts a hex char to its integer value
     *
     * @param c A single hexadecimal character. Must be in one of these ranges:
     *          - '0' to '9', or
     *          - 'a' to 'f', or
     *          - 'A' to 'F'
     *
     * @return the integer representation of {@code c}
     *
     * @throws RuntimeException on invalid character
     */
    public static int hexCharToInt(char c) {
        if (c >= '0' && c <= '9') return (c - '0');
        if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return (c - 'a' + 10);

        throw new RuntimeException ("invalid hex char '" + c + "'");
    }
}
