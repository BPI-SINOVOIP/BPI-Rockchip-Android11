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

package android.hdmicec.cts;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CecMessage {

    private static final int HEXADECIMAL_RADIX = 16;

    /** Gets the hexadecimal ASCII character values of a string. */
    public static String getHexAsciiString(String string) {
        String asciiString = "";
        byte[] ascii = string.trim().getBytes();

        for (byte b : ascii) {
            asciiString.concat(Integer.toHexString(b));
        }

        return asciiString;
    }

    public static String formatParams(String rawParams) {
        StringBuilder params = new StringBuilder("");
        int position = 0;
        int endPosition = 2;

        do {
            params.append(":" + rawParams.substring(position, endPosition));
            position = endPosition;
            endPosition += 2;
        } while (endPosition <= rawParams.length());
        return params.toString();
    }

    public static String formatParams(long rawParam) {
        StringBuilder params = new StringBuilder("");

        do {
            params.insert(0, ":" + String.format("%02x", rawParam % 256));
            rawParam >>= 8;
        } while (rawParam > 0);

        return params.toString();
    }

    /**
     * Formats the rawParam into CEC message parameters. The parameters will be at least
     * minimumNibbles long.
     */
    public static String formatParams(long rawParam, int minimumNibbles) {
        StringBuilder params = new StringBuilder("");

        do {
            params.insert(0, ":" + String.format("%02x", rawParam % 256));
            rawParam >>= 8;
            minimumNibbles -= 2;
        } while (rawParam > 0 || minimumNibbles > 0);

        return params.toString();
    }

    public static int hexStringToInt(String message) {
        return Integer.parseInt(message, HEXADECIMAL_RADIX);
    }

    public static String getAsciiString(String message) {
        String params = getNibbles(message).substring(4);
        StringBuilder builder = new StringBuilder();

        for (int i = 2; i <= params.length(); i += 2) {
            builder.append((char) hexStringToInt(params.substring(i - 2, i)));
        }

        return builder.toString();
    }

    public static String getParamsAsString(String message) {
        return getNibbles(message).substring(4);
    }

    /** Gets the params from a CEC message. */
    public static int getParams(String message) {
        return hexStringToInt(getNibbles(message).substring(4));
    }

    /** Gets the first 'numNibbles' number of param nibbles from a CEC message. */
    public static int getParams(String message, int numNibbles) {
        int paramStart = 4;
        int end = numNibbles + paramStart;
        return hexStringToInt(getNibbles(message).substring(paramStart, end));
    }

    /**
     * From the params of a CEC message, gets the nibbles from position start to position end.
     * The start and end are relative to the beginning of the params. For example, in the following
     * message - 4F:82:10:00:04, getParamsFromMessage(message, 0, 4) will return 0x1000 and
     * getParamsFromMessage(message, 4, 6) will return 0x04.
     */
    public static int getParams(String message, int start, int end) {
        return hexStringToInt(getNibbles(message).substring(4).substring(start, end));
    }

    /**
     * Gets the source logical address from a CEC message.
     */
    public static LogicalAddress getSource(String message) {
        String param = getNibbles(message).substring(0, 1);
        return LogicalAddress.getLogicalAddress(hexStringToInt(param));
    }

    /** Gets the destination logical address from a CEC message. */
    public static LogicalAddress getDestination(String message) {
        String param = getNibbles(message).substring(1, 2);
        return LogicalAddress.getLogicalAddress(hexStringToInt(param));
    }

    /** Gets the operand from a CEC message. */
    public static CecOperand getOperand(String message) {
        String param = getNibbles(message).substring(2, 4);
        return CecOperand.getOperand(hexStringToInt(param));
    }

    /**
     * Converts ascii characters to hexadecimal numbers that can be appended to a CEC message as
     * params. For example, "spa" will be converted to ":73:70:61"
     */
    public static String convertStringToHexParams(String rawParams) {
        StringBuilder params = new StringBuilder("");
        for (int i = 0; i < rawParams.length(); i++) {
            params.append(String.format(":%02x", (int) rawParams.charAt(i)));
        }
        return params.toString();
    }

    private static String getNibbles(String message) {
        final String tag1 = "group1";
        final String tag2 = "group2";
        String paramsPattern = "(?:.*[>>|<<].*?)" +
                "(?<" + tag1 + ">[\\p{XDigit}{2}:]+)" +
                "(?<" + tag2 + ">\\p{XDigit}{2})" +
                "(?:.*?)";
        String nibbles = "";

        Pattern p = Pattern.compile(paramsPattern);
        Matcher m = p.matcher(message);
        if (m.matches()) {
            nibbles = m.group(tag1).replace(":", "") + m.group(tag2);
        }
        return nibbles;
    }
}
