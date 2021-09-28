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

package com.android.tradefed.util;

import static org.junit.Assert.assertArrayEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Arrays;
import java.util.Random;

/** Unit tests for {@link ShellOutputReceiverStream} */
@RunWith(JUnit4.class)
public final class ShellOutputReceiverStreamTest {
    private static final int TEST_DATA_SIZE = 1024;

    private FakeShellOutputReceiver mReceiver;
    private byte[] mRandomTestData;
    private int mRandomTestByte;

    @Before
    public void setUp() {
        mReceiver = new FakeShellOutputReceiver();
        mRandomTestData = new byte[TEST_DATA_SIZE];
        final Random rnd = new Random();
        rnd.nextBytes(mRandomTestData);
        mRandomTestByte = rnd.nextInt(256);
    }

    @Test
    public void testNullReceiver() throws Exception {
        // Verify no NPE if null receiver provided.
        try (ShellOutputReceiverStream stream = new ShellOutputReceiverStream(null)) {
            stream.write(mRandomTestByte);
            stream.write(mRandomTestData);
            stream.write(mRandomTestData, 0, mRandomTestData.length);
        }
    }

    @Test
    public void testUpperBitsIgnoredInSingleByteWrite() throws Exception {
        final int testByte = 0x123456;
        final int expectedByte = 0x56;
        try (ShellOutputReceiverStream stream = new ShellOutputReceiverStream(mReceiver)) {
            stream.write(testByte);
        }
        final byte[] expected = new byte[] {expectedByte};
        assertArrayEquals(expected, mReceiver.getReceivedOutput());
    }

    @Test
    public void testWriteSingleBytes() throws Exception {
        try (ShellOutputReceiverStream stream = new ShellOutputReceiverStream(mReceiver)) {
            stream.write(mRandomTestByte);
            stream.write(mRandomTestByte);
        }
        final byte[] expected = new byte[] {(byte) mRandomTestByte, (byte) mRandomTestByte};
        assertArrayEquals(expected, mReceiver.getReceivedOutput());
    }

    @Test
    public void testWriteFullByteArray1() throws Exception {
        try (ShellOutputReceiverStream stream = new ShellOutputReceiverStream(mReceiver)) {
            stream.write(mRandomTestData);
        }
        assertArrayEquals(mRandomTestData, mReceiver.getReceivedOutput());
    }

    @Test
    public void testWriteFullByteArray2() throws Exception {
        try (ShellOutputReceiverStream stream = new ShellOutputReceiverStream(mReceiver)) {
            stream.write(mRandomTestData, 0, mRandomTestData.length);
        }
        assertArrayEquals(mRandomTestData, mReceiver.getReceivedOutput());
    }

    @Test
    public void testWritePartialByteArray() throws Exception {
        final int len = mRandomTestData.length / 2;
        try (ShellOutputReceiverStream stream = new ShellOutputReceiverStream(mReceiver)) {
            stream.write(mRandomTestData, 0, len);
        }
        final byte[] expected = Arrays.copyOfRange(mRandomTestData, 0, len);
        assertArrayEquals(expected, mReceiver.getReceivedOutput());
    }
}
