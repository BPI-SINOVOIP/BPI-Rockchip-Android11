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

package com.android.internal.net;

import static org.mockito.Matchers.anyObject;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;
import com.android.internal.net.utils.Log;

import java.nio.ByteBuffer;

/** TestUtils provides utility methods to facilitate IKE unit tests */
public class TestUtils {
    public static byte[] hexStringToByteArray(String hexString) {
        int len = hexString.length();
        if (len % 2 != 0) {
            throw new IllegalArgumentException("Invalid Hex String");
        }
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] =
                    (byte)
                            ((Character.digit(hexString.charAt(i), 16) << 4)
                                    + Character.digit(hexString.charAt(i + 1), 16));
        }
        return data;
    }

    public static int hexStringToInt(String hexString) {
        if (hexString.length() > 8) {
            throw new IllegalArgumentException("Invalid hex string length for integer type");
        }

        for (int i = hexString.length(); i < 8; i++) {
            hexString = "0" + hexString;
        }

        return ByteBuffer.wrap(hexStringToByteArray(hexString)).getInt();
    }

    public static String stringToHexString(String s) {
        // two hex characters for each char in s
        StringBuilder sb = new StringBuilder(s.length() * 2);
        char[] chars = s.toCharArray();
        for (char c : chars) {
            sb.append(Integer.toHexString(c));
        }
        return sb.toString();
    }

    public static Log makeSpyLogThrowExceptionForWtf(String tag) {
        Log spyLog = spy(new Log(tag, true /*logSensitive*/));

        doAnswer(
                (invocation) -> {
                    throw new IllegalStateException((String) invocation.getArguments()[1]);
                })
                .when(spyLog)
                .wtf(anyString(), anyString());

        doAnswer(
                (invocation) -> {
                    throw (Throwable) invocation.getArguments()[2];
                })
                .when(spyLog)
                .wtf(anyString(), anyString(), anyObject());

        return spyLog;
    }

    public static Log makeSpyLogDoLogErrorForWtf(String tag) {
        Log spyLog = spy(new Log(tag, true /*logSensitive*/));

        doAnswer(
                (invocation) -> {
                    spyLog.e(
                            "Mock logging WTF: " + invocation.getArguments()[0],
                            (String) invocation.getArguments()[1]);
                    return null;
                })
                .when(spyLog)
                .wtf(anyString(), anyString());

        doAnswer(
                (invocation) -> {
                    spyLog.e(
                            "Mock logging WTF: " + invocation.getArguments()[0],
                            (String) invocation.getArguments()[1],
                            (Throwable) invocation.getArguments()[2]);
                    return null;
                })
                .when(spyLog)
                .wtf(anyString(), anyString(), anyObject());

        return spyLog;
    }

    public static RandomnessFactory createMockRandomFactory() {
        RandomnessFactory rFactory = mock(RandomnessFactory.class);
        doReturn(null).when(rFactory).getRandom();
        return rFactory;
    }
}
