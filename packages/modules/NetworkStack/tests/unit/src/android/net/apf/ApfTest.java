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

package android.net.apf;

import static android.system.OsConstants.AF_UNIX;
import static android.system.OsConstants.ARPHRD_ETHER;
import static android.system.OsConstants.ETH_P_ARP;
import static android.system.OsConstants.ETH_P_IP;
import static android.system.OsConstants.ETH_P_IPV6;
import static android.system.OsConstants.IPPROTO_ICMPV6;
import static android.system.OsConstants.IPPROTO_TCP;
import static android.system.OsConstants.IPPROTO_UDP;
import static android.system.OsConstants.SOCK_STREAM;

import static com.android.server.util.NetworkStackConstants.ICMPV6_ECHO_REQUEST_TYPE;
import static com.android.server.util.NetworkStackConstants.IPV6_ADDR_LEN;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.net.InetAddresses;
import android.net.IpPrefix;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.NattKeepalivePacketDataParcelable;
import android.net.TcpKeepalivePacketDataParcelable;
import android.net.apf.ApfFilter.ApfConfiguration;
import android.net.apf.ApfGenerator.IllegalInstructionException;
import android.net.apf.ApfGenerator.Register;
import android.net.ip.IIpClientCallbacks;
import android.net.ip.IpClient.IpClientCallbacksWrapper;
import android.net.metrics.IpConnectivityLog;
import android.net.metrics.RaEvent;
import android.net.util.InterfaceParams;
import android.net.util.SharedLog;
import android.os.ConditionVariable;
import android.os.Parcelable;
import android.os.SystemClock;
import android.system.ErrnoException;
import android.system.Os;
import android.text.format.DateUtils;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.internal.util.HexDump;
import com.android.net.module.util.Inet4AddressUtils;
import com.android.networkstack.apishim.NetworkInformationShimImpl;
import com.android.server.networkstack.tests.R;
import com.android.server.util.NetworkStackConstants;

import libcore.io.IoUtils;
import libcore.io.Streams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.Random;

/**
 * Tests for APF program generator and interpreter.
 *
 * Build, install and run with:
 *  runtest frameworks-net -c android.net.apf.ApfTest
 */
@RunWith(AndroidJUnit4.class)
@SmallTest
public class ApfTest {
    private static final int TIMEOUT_MS = 500;
    private static final int MIN_APF_VERSION = 2;

    @Mock IpConnectivityLog mLog;
    @Mock Context mContext;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        // Load up native shared library containing APF interpreter exposed via JNI.
        System.loadLibrary("networkstacktestsjni");
    }

    private static final String TAG = "ApfTest";
    // Expected return codes from APF interpreter.
    private static final int PASS = 1;
    private static final int DROP = 0;
    // Interpreter will just accept packets without link layer headers, so pad fake packet to at
    // least the minimum packet size.
    private static final int MIN_PKT_SIZE = 15;

    private static final ApfCapabilities MOCK_APF_CAPABILITIES =
            new ApfCapabilities(2, 4096, ARPHRD_ETHER);

    private static final boolean DROP_MULTICAST = true;
    private static final boolean ALLOW_MULTICAST = false;

    private static final boolean DROP_802_3_FRAMES = true;
    private static final boolean ALLOW_802_3_FRAMES = false;

    private static final int MIN_RDNSS_LIFETIME_SEC = 0;

    // Constants for opcode encoding
    private static final byte LI_OP   = (byte)(13 << 3);
    private static final byte LDDW_OP = (byte)(22 << 3);
    private static final byte STDW_OP = (byte)(23 << 3);
    private static final byte SIZE0   = (byte)(0 << 1);
    private static final byte SIZE8   = (byte)(1 << 1);
    private static final byte SIZE16  = (byte)(2 << 1);
    private static final byte SIZE32  = (byte)(3 << 1);
    private static final byte R1 = 1;

    private static ApfConfiguration getDefaultConfig() {
        ApfFilter.ApfConfiguration config = new ApfConfiguration();
        config.apfCapabilities = MOCK_APF_CAPABILITIES;
        config.multicastFilter = ALLOW_MULTICAST;
        config.ieee802_3Filter = ALLOW_802_3_FRAMES;
        config.ethTypeBlackList = new int[0];
        config.minRdnssLifetimeSec = MIN_RDNSS_LIFETIME_SEC;
        config.minRdnssLifetimeSec = 67;
        return config;
    }

    private static String label(int code) {
        switch (code) {
            case PASS: return "PASS";
            case DROP: return "DROP";
            default:   return "UNKNOWN";
        }
    }

    private static void assertReturnCodesEqual(int expected, int got) {
        assertEquals(label(expected), label(got));
    }

    private void assertVerdict(int expected, byte[] program, byte[] packet, int filterAge) {
        assertReturnCodesEqual(expected, apfSimulate(program, packet, null, filterAge));
    }

    private void assertVerdict(int expected, byte[] program, byte[] packet) {
        assertReturnCodesEqual(expected, apfSimulate(program, packet, null, 0));
    }

    private void assertPass(byte[] program, byte[] packet, int filterAge) {
        assertVerdict(PASS, program, packet, filterAge);
    }

    private void assertPass(byte[] program, byte[] packet) {
        assertVerdict(PASS, program, packet);
    }

    private void assertDrop(byte[] program, byte[] packet, int filterAge) {
        assertVerdict(DROP, program, packet, filterAge);
    }

    private void assertDrop(byte[] program, byte[] packet) {
        assertVerdict(DROP, program, packet);
    }

    private void assertProgramEquals(byte[] expected, byte[] program) throws AssertionError {
        // assertArrayEquals() would only print one byte, making debugging difficult.
        if (!Arrays.equals(expected, program)) {
            throw new AssertionError(
                    "\nexpected: " + HexDump.toHexString(expected) +
                    "\nactual:   " + HexDump.toHexString(program));
        }
    }

    private void assertDataMemoryContents(
            int expected, byte[] program, byte[] packet, byte[] data, byte[] expected_data)
            throws IllegalInstructionException, Exception {
        assertReturnCodesEqual(expected, apfSimulate(program, packet, data, 0 /* filterAge */));

        // assertArrayEquals() would only print one byte, making debugging difficult.
        if (!Arrays.equals(expected_data, data)) {
            throw new Exception(
                    "\nprogram:     " + HexDump.toHexString(program) +
                    "\ndata memory: " + HexDump.toHexString(data) +
                    "\nexpected:    " + HexDump.toHexString(expected_data));
        }
    }

    private void assertVerdict(int expected, ApfGenerator gen, byte[] packet, int filterAge)
            throws IllegalInstructionException {
        assertReturnCodesEqual(expected, apfSimulate(gen.generate(), packet, null,
              filterAge));
    }

    private void assertPass(ApfGenerator gen, byte[] packet, int filterAge)
            throws IllegalInstructionException {
        assertVerdict(PASS, gen, packet, filterAge);
    }

    private void assertDrop(ApfGenerator gen, byte[] packet, int filterAge)
            throws IllegalInstructionException {
        assertVerdict(DROP, gen, packet, filterAge);
    }

    private void assertPass(ApfGenerator gen)
            throws IllegalInstructionException {
        assertVerdict(PASS, gen, new byte[MIN_PKT_SIZE], 0);
    }

    private void assertDrop(ApfGenerator gen)
            throws IllegalInstructionException {
        assertVerdict(DROP, gen, new byte[MIN_PKT_SIZE], 0);
    }

    /**
     * Test each instruction by generating a program containing the instruction,
     * generating bytecode for that program and running it through the
     * interpreter to verify it functions correctly.
     */
    @Test
    public void testApfInstructions() throws IllegalInstructionException {
        // Empty program should pass because having the program counter reach the
        // location immediately after the program indicates the packet should be
        // passed to the AP.
        ApfGenerator gen = new ApfGenerator(MIN_APF_VERSION);
        assertPass(gen);

        // Test jumping to pass label.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJump(gen.PASS_LABEL);
        byte[] program = gen.generate();
        assertEquals(1, program.length);
        assertEquals((14 << 3) | (0 << 1) | 0, program[0]);
        assertPass(program, new byte[MIN_PKT_SIZE], 0);

        // Test jumping to drop label.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJump(gen.DROP_LABEL);
        program = gen.generate();
        assertEquals(2, program.length);
        assertEquals((14 << 3) | (1 << 1) | 0, program[0]);
        assertEquals(1, program[1]);
        assertDrop(program, new byte[15], 15);

        // Test jumping if equal to 0.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0Equals(0, gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if not equal to 0.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0NotEquals(0, gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfR0NotEquals(0, gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if registers equal.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0EqualsR1(gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if registers not equal.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0NotEqualsR1(gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfR0NotEqualsR1(gen.DROP_LABEL);
        assertDrop(gen);

        // Test load immediate.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test add.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addAdd(1234567890);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test subtract.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addAdd(-1234567890);
        gen.addJumpIfR0Equals(-1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test or.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addOr(1234567890);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test and.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addAnd(123456789);
        gen.addJumpIfR0Equals(1234567890 & 123456789, gen.DROP_LABEL);
        assertDrop(gen);

        // Test left shift.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addLeftShift(1);
        gen.addJumpIfR0Equals(1234567890 << 1, gen.DROP_LABEL);
        assertDrop(gen);

        // Test right shift.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addRightShift(1);
        gen.addJumpIfR0Equals(1234567890 >> 1, gen.DROP_LABEL);
        assertDrop(gen);

        // Test multiply.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 123456789);
        gen.addMul(2);
        gen.addJumpIfR0Equals(123456789 * 2, gen.DROP_LABEL);
        assertDrop(gen);

        // Test divide.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addDiv(2);
        gen.addJumpIfR0Equals(1234567890 / 2, gen.DROP_LABEL);
        assertDrop(gen);

        // Test divide by zero.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addDiv(0);
        gen.addJump(gen.DROP_LABEL);
        assertPass(gen);

        // Test add.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1234567890);
        gen.addAddR1();
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test subtract.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, -1234567890);
        gen.addAddR1();
        gen.addJumpIfR0Equals(-1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test or.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1234567890);
        gen.addOrR1();
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test and.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addLoadImmediate(Register.R1, 123456789);
        gen.addAndR1();
        gen.addJumpIfR0Equals(1234567890 & 123456789, gen.DROP_LABEL);
        assertDrop(gen);

        // Test left shift.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addLoadImmediate(Register.R1, 1);
        gen.addLeftShiftR1();
        gen.addJumpIfR0Equals(1234567890 << 1, gen.DROP_LABEL);
        assertDrop(gen);

        // Test right shift.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addLoadImmediate(Register.R1, -1);
        gen.addLeftShiftR1();
        gen.addJumpIfR0Equals(1234567890 >> 1, gen.DROP_LABEL);
        assertDrop(gen);

        // Test multiply.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 123456789);
        gen.addLoadImmediate(Register.R1, 2);
        gen.addMulR1();
        gen.addJumpIfR0Equals(123456789 * 2, gen.DROP_LABEL);
        assertDrop(gen);

        // Test divide.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addLoadImmediate(Register.R1, 2);
        gen.addDivR1();
        gen.addJumpIfR0Equals(1234567890 / 2, gen.DROP_LABEL);
        assertDrop(gen);

        // Test divide by zero.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addDivR1();
        gen.addJump(gen.DROP_LABEL);
        assertPass(gen);

        // Test byte load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoad8(Register.R0, 1);
        gen.addJumpIfR0Equals(45, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{123,45,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test out of bounds load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoad8(Register.R0, 16);
        gen.addJumpIfR0Equals(0, gen.DROP_LABEL);
        assertPass(gen, new byte[]{123,45,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test half-word load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoad16(Register.R0, 1);
        gen.addJumpIfR0Equals((45 << 8) | 67, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{123,45,67,0,0,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test word load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoad32(Register.R0, 1);
        gen.addJumpIfR0Equals((45 << 24) | (67 << 16) | (89 << 8) | 12, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{123,45,67,89,12,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test byte indexed load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1);
        gen.addLoad8Indexed(Register.R0, 0);
        gen.addJumpIfR0Equals(45, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{123,45,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test out of bounds indexed load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 8);
        gen.addLoad8Indexed(Register.R0, 8);
        gen.addJumpIfR0Equals(0, gen.DROP_LABEL);
        assertPass(gen, new byte[]{123,45,0,0,0,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test half-word indexed load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1);
        gen.addLoad16Indexed(Register.R0, 0);
        gen.addJumpIfR0Equals((45 << 8) | 67, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{123,45,67,0,0,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test word indexed load.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1);
        gen.addLoad32Indexed(Register.R0, 0);
        gen.addJumpIfR0Equals((45 << 24) | (67 << 16) | (89 << 8) | 12, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{123,45,67,89,12,0,0,0,0,0,0,0,0,0,0}, 0);

        // Test jumping if greater than.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0GreaterThan(0, gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfR0GreaterThan(0, gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if less than.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0LessThan(0, gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0LessThan(1, gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if any bits set.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0AnyBitsSet(3, gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfR0AnyBitsSet(3, gen.DROP_LABEL);
        assertDrop(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 3);
        gen.addJumpIfR0AnyBitsSet(3, gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if register greater than.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0GreaterThanR1(gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 2);
        gen.addLoadImmediate(Register.R1, 1);
        gen.addJumpIfR0GreaterThanR1(gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if register less than.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfR0LessThanR1(gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1);
        gen.addJumpIfR0LessThanR1(gen.DROP_LABEL);
        assertDrop(gen);

        // Test jumping if any bits set in register.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 3);
        gen.addJumpIfR0AnyBitsSetR1(gen.DROP_LABEL);
        assertPass(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 3);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfR0AnyBitsSetR1(gen.DROP_LABEL);
        assertDrop(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 3);
        gen.addLoadImmediate(Register.R0, 3);
        gen.addJumpIfR0AnyBitsSetR1(gen.DROP_LABEL);
        assertDrop(gen);

        // Test load from memory.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadFromMemory(Register.R0, 0);
        gen.addJumpIfR0Equals(0, gen.DROP_LABEL);
        assertDrop(gen);

        // Test store to memory.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1234567890);
        gen.addStoreToMemory(Register.R1, 12);
        gen.addLoadFromMemory(Register.R0, 12);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test filter age pre-filled memory.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadFromMemory(Register.R0, gen.FILTER_AGE_MEMORY_SLOT);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen, new byte[MIN_PKT_SIZE], 1234567890);

        // Test packet size pre-filled memory.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadFromMemory(Register.R0, gen.PACKET_SIZE_MEMORY_SLOT);
        gen.addJumpIfR0Equals(MIN_PKT_SIZE, gen.DROP_LABEL);
        assertDrop(gen);

        // Test IPv4 header size pre-filled memory.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadFromMemory(Register.R0, gen.IPV4_HEADER_SIZE_MEMORY_SLOT);
        gen.addJumpIfR0Equals(20, gen.DROP_LABEL);
        assertDrop(gen, new byte[]{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x45}, 0);

        // Test not.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addNot(Register.R0);
        gen.addJumpIfR0Equals(~1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test negate.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addNeg(Register.R0);
        gen.addJumpIfR0Equals(-1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test move.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1234567890);
        gen.addMove(Register.R0);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addMove(Register.R1);
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);

        // Test swap.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R1, 1234567890);
        gen.addSwap();
        gen.addJumpIfR0Equals(1234567890, gen.DROP_LABEL);
        assertDrop(gen);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1234567890);
        gen.addSwap();
        gen.addJumpIfR0Equals(0, gen.DROP_LABEL);
        assertDrop(gen);

        // Test jump if bytes not equal.
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfBytesNotEqual(Register.R0, new byte[]{123}, gen.DROP_LABEL);
        program = gen.generate();
        assertEquals(6, program.length);
        assertEquals((13 << 3) | (1 << 1) | 0, program[0]);
        assertEquals(1, program[1]);
        assertEquals(((20 << 3) | (1 << 1) | 0) - 256, program[2]);
        assertEquals(1, program[3]);
        assertEquals(1, program[4]);
        assertEquals(123, program[5]);
        assertDrop(program, new byte[MIN_PKT_SIZE], 0);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfBytesNotEqual(Register.R0, new byte[]{123}, gen.DROP_LABEL);
        byte[] packet123 = {0,123,0,0,0,0,0,0,0,0,0,0,0,0,0};
        assertPass(gen, packet123, 0);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addJumpIfBytesNotEqual(Register.R0, new byte[]{123}, gen.DROP_LABEL);
        assertDrop(gen, packet123, 0);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfBytesNotEqual(Register.R0, new byte[]{1,2,30,4,5}, gen.DROP_LABEL);
        byte[] packet12345 = {0,1,2,3,4,5,0,0,0,0,0,0,0,0,0};
        assertDrop(gen, packet12345, 0);
        gen = new ApfGenerator(MIN_APF_VERSION);
        gen.addLoadImmediate(Register.R0, 1);
        gen.addJumpIfBytesNotEqual(Register.R0, new byte[]{1,2,3,4,5}, gen.DROP_LABEL);
        assertPass(gen, packet12345, 0);
    }

    @Test(expected = ApfGenerator.IllegalInstructionException.class)
    public void testApfGeneratorWantsV2OrGreater() throws Exception {
        // The minimum supported APF version is 2.
        new ApfGenerator(1);
    }

    @Test
    public void testApfDataOpcodesWantApfV3() throws IllegalInstructionException, Exception {
        ApfGenerator gen = new ApfGenerator(MIN_APF_VERSION);
        try {
            gen.addStoreData(Register.R0, 0);
            fail();
        } catch (IllegalInstructionException expected) {
            /* pass */
        }
        try {
            gen.addLoadData(Register.R0, 0);
            fail();
        } catch (IllegalInstructionException expected) {
            /* pass */
        }
    }

    /**
     * Test that the generator emits immediates using the shortest possible encoding.
     */
    @Test
    public void testImmediateEncoding() throws IllegalInstructionException {
        ApfGenerator gen;

        // 0-byte immediate: li R0, 0
        gen = new ApfGenerator(4);
        gen.addLoadImmediate(Register.R0, 0);
        assertProgramEquals(new byte[]{LI_OP | SIZE0}, gen.generate());

        // 1-byte immediate: li R0, 42
        gen = new ApfGenerator(4);
        gen.addLoadImmediate(Register.R0, 42);
        assertProgramEquals(new byte[]{LI_OP | SIZE8, 42}, gen.generate());

        // 2-byte immediate: li R1, 0x1234
        gen = new ApfGenerator(4);
        gen.addLoadImmediate(Register.R1, 0x1234);
        assertProgramEquals(new byte[]{LI_OP | SIZE16 | R1, 0x12, 0x34}, gen.generate());

        // 4-byte immediate: li R0, 0x12345678
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 0x12345678);
        assertProgramEquals(
                new byte[]{LI_OP | SIZE32, 0x12, 0x34, 0x56, 0x78},
                gen.generate());
    }

    /**
     * Test that the generator emits negative immediates using the shortest possible encoding.
     */
    @Test
    public void testNegativeImmediateEncoding() throws IllegalInstructionException {
        ApfGenerator gen;

        // 1-byte negative immediate: li R0, -42
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, -42);
        assertProgramEquals(new byte[]{LI_OP | SIZE8, -42}, gen.generate());

        // 2-byte negative immediate: li R1, -0x1122
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R1, -0x1122);
        assertProgramEquals(new byte[]{LI_OP | SIZE16 | R1, (byte)0xEE, (byte)0xDE},
                gen.generate());

        // 4-byte negative immediate: li R0, -0x11223344
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, -0x11223344);
        assertProgramEquals(
                new byte[]{LI_OP | SIZE32, (byte)0xEE, (byte)0xDD, (byte)0xCC, (byte)0xBC},
                gen.generate());
    }

    /**
     * Test that the generator correctly emits positive and negative immediates for LDDW/STDW.
     */
    @Test
    public void testLoadStoreDataEncoding() throws IllegalInstructionException {
        ApfGenerator gen;

        // Load data with no offset: lddw R0, [0 + r1]
        gen = new ApfGenerator(3);
        gen.addLoadData(Register.R0, 0);
        assertProgramEquals(new byte[]{LDDW_OP | SIZE0}, gen.generate());

        // Store data with 8bit negative offset: lddw r0, [-42 + r1]
        gen = new ApfGenerator(3);
        gen.addStoreData(Register.R0, -42);
        assertProgramEquals(new byte[]{STDW_OP | SIZE8, -42}, gen.generate());

        // Store data to R1 with 16bit negative offset: stdw r1, [-0x1122 + r0]
        gen = new ApfGenerator(3);
        gen.addStoreData(Register.R1, -0x1122);
        assertProgramEquals(new byte[]{STDW_OP | SIZE16 | R1, (byte)0xEE, (byte)0xDE},
                gen.generate());

        // Load data to R1 with 32bit negative offset: lddw r1, [0xDEADBEEF + r0]
        gen = new ApfGenerator(3);
        gen.addLoadData(Register.R1, 0xDEADBEEF);
        assertProgramEquals(
                new byte[]{LDDW_OP | SIZE32 | R1, (byte)0xDE, (byte)0xAD, (byte)0xBE, (byte)0xEF},
                gen.generate());
    }

    /**
     * Test that the interpreter correctly executes STDW with a negative 8bit offset
     */
    @Test
    public void testApfDataWrite() throws IllegalInstructionException, Exception {
        byte[] packet = new byte[MIN_PKT_SIZE];
        byte[] data = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        byte[] expected_data = data.clone();

        // No memory access instructions: should leave the data segment untouched.
        ApfGenerator gen = new ApfGenerator(3);
        assertDataMemoryContents(PASS, gen.generate(), packet, data, expected_data);

        // Expect value 0x87654321 to be stored starting from address -11 from the end of the
        // data buffer, in big-endian order.
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 0x87654321);
        gen.addLoadImmediate(Register.R1, -5);
        gen.addStoreData(Register.R0, -6);  // -5 + -6 = -11 (offset +5 with data_len=16)
        expected_data[5] = (byte)0x87;
        expected_data[6] = (byte)0x65;
        expected_data[7] = (byte)0x43;
        expected_data[8] = (byte)0x21;
        assertDataMemoryContents(PASS, gen.generate(), packet, data, expected_data);
    }

    /**
     * Test that the interpreter correctly executes LDDW with a negative 16bit offset
     */
    @Test
    public void testApfDataRead() throws IllegalInstructionException, Exception {
        // Program that DROPs if address 10 (-6) contains 0x87654321.
        ApfGenerator gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R1, 1000);
        gen.addLoadData(Register.R0, -1006);  // 1000 + -1006 = -6 (offset +10 with data_len=16)
        gen.addJumpIfR0Equals(0x87654321, gen.DROP_LABEL);
        byte[] program = gen.generate();
        byte[] packet = new byte[MIN_PKT_SIZE];

        // Content is incorrect (last byte does not match) -> PASS
        byte[] data = new byte[16];
        data[10] = (byte)0x87;
        data[11] = (byte)0x65;
        data[12] = (byte)0x43;
        data[13] = (byte)0x00;  // != 0x21
        byte[] expected_data = data.clone();
        assertDataMemoryContents(PASS, program, packet, data, expected_data);

        // Fix the last byte -> conditional jump taken -> DROP
        data[13] = (byte)0x21;
        expected_data = data;
        assertDataMemoryContents(DROP, program, packet, data, expected_data);
    }

    /**
     * Test that the interpreter correctly executes LDDW followed by a STDW.
     * To cover a few more edge cases, LDDW has a 0bit offset, while STDW has a positive 8bit
     * offset.
     */
    @Test
    public void testApfDataReadModifyWrite() throws IllegalInstructionException, Exception {
        ApfGenerator gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R1, -22);
        gen.addLoadData(Register.R0, 0);  // Load from address 32 -22 + 0 = 10
        gen.addAdd(0x78453412);  // 87654321 + 78453412 = FFAA7733
        gen.addStoreData(Register.R0, 4);  // Write back to address 32 -22 + 4 = 14

        byte[] packet = new byte[MIN_PKT_SIZE];
        byte[] data = new byte[32];
        data[10] = (byte)0x87;
        data[11] = (byte)0x65;
        data[12] = (byte)0x43;
        data[13] = (byte)0x21;
        byte[] expected_data = data.clone();
        expected_data[14] = (byte)0xFF;
        expected_data[15] = (byte)0xAA;
        expected_data[16] = (byte)0x77;
        expected_data[17] = (byte)0x33;
        assertDataMemoryContents(PASS, gen.generate(), packet, data, expected_data);
    }

    @Test
    public void testApfDataBoundChecking() throws IllegalInstructionException, Exception {
        byte[] packet = new byte[MIN_PKT_SIZE];
        byte[] data = new byte[32];
        byte[] expected_data = data;

        // Program that DROPs unconditionally. This is our the baseline.
        ApfGenerator gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 3);
        gen.addLoadData(Register.R1, 7);
        gen.addJump(gen.DROP_LABEL);
        assertDataMemoryContents(DROP, gen.generate(), packet, data, expected_data);

        // Same program as before, but this time we're trying to load past the end of the data.
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 20);
        gen.addLoadData(Register.R1, 15);  // 20 + 15 > 32
        gen.addJump(gen.DROP_LABEL);  // Not reached.
        assertDataMemoryContents(PASS, gen.generate(), packet, data, expected_data);

        // Subtracting an immediate should work...
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 20);
        gen.addLoadData(Register.R1, -4);
        gen.addJump(gen.DROP_LABEL);
        assertDataMemoryContents(DROP, gen.generate(), packet, data, expected_data);

        // ...and underflowing simply wraps around to the end of the buffer...
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 20);
        gen.addLoadData(Register.R1, -30);
        gen.addJump(gen.DROP_LABEL);
        assertDataMemoryContents(DROP, gen.generate(), packet, data, expected_data);

        // ...but doesn't allow accesses before the start of the buffer
        gen = new ApfGenerator(3);
        gen.addLoadImmediate(Register.R0, 20);
        gen.addLoadData(Register.R1, -1000);
        gen.addJump(gen.DROP_LABEL);  // Not reached.
        assertDataMemoryContents(PASS, gen.generate(), packet, data, expected_data);
    }

    /**
     * Generate some BPF programs, translate them to APF, then run APF and BPF programs
     * over packet traces and verify both programs filter out the same packets.
     */
    @Test
    public void testApfAgainstBpf() throws Exception {
        String[] tcpdump_filters = new String[]{ "udp", "tcp", "icmp", "icmp6", "udp port 53",
                "arp", "dst 239.255.255.250", "arp or tcp or udp port 53", "net 192.168.1.0/24",
                "arp or icmp6 or portrange 53-54", "portrange 53-54 or portrange 100-50000",
                "tcp[tcpflags] & (tcp-ack|tcp-fin) != 0 and (ip[2:2] > 57 or icmp)" };
        String pcap_filename = stageFile(R.raw.apf);
        for (String tcpdump_filter : tcpdump_filters) {
            byte[] apf_program = Bpf2Apf.convert(compileToBpf(tcpdump_filter));
            assertTrue("Failed to match for filter: " + tcpdump_filter,
                    compareBpfApf(tcpdump_filter, pcap_filename, apf_program));
        }
    }

    /**
     * Generate APF program, run pcap file though APF filter, then check all the packets in the file
     * should be dropped.
     */
    @Test
    public void testApfFilterPcapFile() throws Exception {
        final byte[] MOCK_PCAP_IPV4_ADDR = {(byte) 172, 16, 7, (byte) 151};
        String pcapFilename = stageFile(R.raw.apfPcap);
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        LinkAddress link = new LinkAddress(InetAddress.getByAddress(MOCK_PCAP_IPV4_ADDR), 16);
        LinkProperties lp = new LinkProperties();
        lp.addLinkAddress(link);

        ApfConfiguration config = getDefaultConfig();
        ApfCapabilities MOCK_APF_PCAP_CAPABILITIES = new ApfCapabilities(4, 1700, ARPHRD_ETHER);
        config.apfCapabilities = MOCK_APF_PCAP_CAPABILITIES;
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        apfFilter.setLinkProperties(lp);
        byte[] program = ipClientCallback.getApfProgram();
        byte[] data = new byte[ApfFilter.Counter.totalSize()];
        final boolean result;

        result = dropsAllPackets(program, data, pcapFilename);
        Log.i(TAG, "testApfFilterPcapFile(): Data counters: " + HexDump.toHexString(data, false));

        assertTrue("Failed to drop all packets by filter. \nAPF counters:" +
            HexDump.toHexString(data, false), result);
    }

    private class MockIpClientCallback extends IpClientCallbacksWrapper {
        private final ConditionVariable mGotApfProgram = new ConditionVariable();
        private byte[] mLastApfProgram;

        MockIpClientCallback() {
            super(mock(IIpClientCallbacks.class), mock(SharedLog.class),
                    NetworkInformationShimImpl.newInstance());
        }

        @Override
        public void installPacketFilter(byte[] filter) {
            mLastApfProgram = filter;
            mGotApfProgram.open();
        }

        public void resetApfProgramWait() {
            mGotApfProgram.close();
        }

        public byte[] getApfProgram() {
            assertTrue(mGotApfProgram.block(TIMEOUT_MS));
            return mLastApfProgram;
        }

        public void assertNoProgramUpdate() {
            assertFalse(mGotApfProgram.block(TIMEOUT_MS));
        }
    }

    private static class TestApfFilter extends ApfFilter {
        public static final byte[] MOCK_MAC_ADDR = {1,2,3,4,5,6};

        private FileDescriptor mWriteSocket;
        private final long mFixedTimeMs = SystemClock.elapsedRealtime();

        public TestApfFilter(Context context, ApfConfiguration config,
                IpClientCallbacksWrapper ipClientCallback, IpConnectivityLog log) throws Exception {
            super(context, config, InterfaceParams.getByName("lo"), ipClientCallback, log);
        }

        // Pretend an RA packet has been received and show it to ApfFilter.
        public void pretendPacketReceived(byte[] packet) throws IOException, ErrnoException {
            // ApfFilter's ReceiveThread will be waiting to read this.
            Os.write(mWriteSocket, packet, 0, packet.length);
        }

        @Override
        protected long currentTimeSeconds() {
            return mFixedTimeMs / DateUtils.SECOND_IN_MILLIS;
        }

        @Override
        void maybeStartFilter() {
            mHardwareAddress = MOCK_MAC_ADDR;
            installNewProgramLocked();

            // Create two sockets, "readSocket" and "mWriteSocket" and connect them together.
            FileDescriptor readSocket = new FileDescriptor();
            mWriteSocket = new FileDescriptor();
            try {
                Os.socketpair(AF_UNIX, SOCK_STREAM, 0, mWriteSocket, readSocket);
            } catch (ErrnoException e) {
                fail();
                return;
            }
            // Now pass readSocket to ReceiveThread as if it was setup to read raw RAs.
            // This allows us to pretend RA packets have been recieved via pretendPacketReceived().
            mReceiveThread = new ReceiveThread(readSocket);
            mReceiveThread.start();
        }

        @Override
        public void shutdown() {
            super.shutdown();
            IoUtils.closeQuietly(mWriteSocket);
        }
    }

    private static final int ETH_HEADER_LEN               = 14;
    private static final int ETH_DEST_ADDR_OFFSET         = 0;
    private static final int ETH_ETHERTYPE_OFFSET         = 12;
    private static final byte[] ETH_BROADCAST_MAC_ADDRESS =
            {(byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff, (byte) 0xff };

    private static final int IP_HEADER_OFFSET = ETH_HEADER_LEN;

    private static final int IPV4_HEADER_LEN          = 20;
    private static final int IPV4_TOTAL_LENGTH_OFFSET = IP_HEADER_OFFSET + 2;
    private static final int IPV4_PROTOCOL_OFFSET     = IP_HEADER_OFFSET + 9;
    private static final int IPV4_SRC_ADDR_OFFSET     = IP_HEADER_OFFSET + 12;
    private static final int IPV4_DEST_ADDR_OFFSET    = IP_HEADER_OFFSET + 16;

    private static final int IPV4_TCP_HEADER_LEN           = 20;
    private static final int IPV4_TCP_HEADER_OFFSET        = IP_HEADER_OFFSET + IPV4_HEADER_LEN;
    private static final int IPV4_TCP_SRC_PORT_OFFSET      = IPV4_TCP_HEADER_OFFSET + 0;
    private static final int IPV4_TCP_DEST_PORT_OFFSET     = IPV4_TCP_HEADER_OFFSET + 2;
    private static final int IPV4_TCP_SEQ_NUM_OFFSET       = IPV4_TCP_HEADER_OFFSET + 4;
    private static final int IPV4_TCP_ACK_NUM_OFFSET       = IPV4_TCP_HEADER_OFFSET + 8;
    private static final int IPV4_TCP_HEADER_LENGTH_OFFSET = IPV4_TCP_HEADER_OFFSET + 12;
    private static final int IPV4_TCP_HEADER_FLAG_OFFSET   = IPV4_TCP_HEADER_OFFSET + 13;

    private static final int IPV4_UDP_HEADER_OFFSET    = IP_HEADER_OFFSET + IPV4_HEADER_LEN;
    private static final int IPV4_UDP_SRC_PORT_OFFSET  = IPV4_UDP_HEADER_OFFSET + 0;
    private static final int IPV4_UDP_DEST_PORT_OFFSET = IPV4_UDP_HEADER_OFFSET + 2;
    private static final int IPV4_UDP_LENGTH_OFFSET    = IPV4_UDP_HEADER_OFFSET + 4;
    private static final int IPV4_UDP_PAYLOAD_OFFSET   = IPV4_UDP_HEADER_OFFSET + 8;
    private static final byte[] IPV4_BROADCAST_ADDRESS =
            {(byte) 255, (byte) 255, (byte) 255, (byte) 255};

    private static final int IPV6_HEADER_LEN             = 40;
    private static final int IPV6_PAYLOAD_LENGTH_OFFSET  = IP_HEADER_OFFSET + 4;
    private static final int IPV6_NEXT_HEADER_OFFSET     = IP_HEADER_OFFSET + 6;
    private static final int IPV6_SRC_ADDR_OFFSET        = IP_HEADER_OFFSET + 8;
    private static final int IPV6_DEST_ADDR_OFFSET       = IP_HEADER_OFFSET + 24;
    private static final int IPV6_TCP_HEADER_OFFSET      = IP_HEADER_OFFSET + IPV6_HEADER_LEN;
    private static final int IPV6_TCP_SRC_PORT_OFFSET    = IPV6_TCP_HEADER_OFFSET + 0;
    private static final int IPV6_TCP_DEST_PORT_OFFSET   = IPV6_TCP_HEADER_OFFSET + 2;
    private static final int IPV6_TCP_SEQ_NUM_OFFSET     = IPV6_TCP_HEADER_OFFSET + 4;
    private static final int IPV6_TCP_ACK_NUM_OFFSET     = IPV6_TCP_HEADER_OFFSET + 8;
    // The IPv6 all nodes address ff02::1
    private static final byte[] IPV6_ALL_NODES_ADDRESS   =
            { (byte) 0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    private static final byte[] IPV6_ALL_ROUTERS_ADDRESS =
            { (byte) 0xff, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 };

    private static final int ICMP6_TYPE_OFFSET           = IP_HEADER_OFFSET + IPV6_HEADER_LEN;
    private static final int ICMP6_ROUTER_SOLICITATION   = 133;
    private static final int ICMP6_ROUTER_ADVERTISEMENT  = 134;
    private static final int ICMP6_NEIGHBOR_SOLICITATION = 135;
    private static final int ICMP6_NEIGHBOR_ANNOUNCEMENT = 136;

    private static final int ICMP6_RA_HEADER_LEN = 16;
    private static final int ICMP6_RA_CHECKSUM_OFFSET =
            IP_HEADER_OFFSET + IPV6_HEADER_LEN + 2;
    private static final int ICMP6_RA_ROUTER_LIFETIME_OFFSET =
            IP_HEADER_OFFSET + IPV6_HEADER_LEN + 6;
    private static final int ICMP6_RA_REACHABLE_TIME_OFFSET =
            IP_HEADER_OFFSET + IPV6_HEADER_LEN + 8;
    private static final int ICMP6_RA_RETRANSMISSION_TIMER_OFFSET =
            IP_HEADER_OFFSET + IPV6_HEADER_LEN + 12;
    private static final int ICMP6_RA_OPTION_OFFSET =
            IP_HEADER_OFFSET + IPV6_HEADER_LEN + ICMP6_RA_HEADER_LEN;

    private static final int ICMP6_PREFIX_OPTION_TYPE                      = 3;
    private static final int ICMP6_PREFIX_OPTION_LEN                       = 32;
    private static final int ICMP6_PREFIX_OPTION_VALID_LIFETIME_OFFSET     = 4;
    private static final int ICMP6_PREFIX_OPTION_PREFERRED_LIFETIME_OFFSET = 8;

    // From RFC6106: Recursive DNS Server option
    private static final int ICMP6_RDNSS_OPTION_TYPE = 25;
    // From RFC6106: DNS Search List option
    private static final int ICMP6_DNSSL_OPTION_TYPE = 31;

    // From RFC4191: Route Information option
    private static final int ICMP6_ROUTE_INFO_OPTION_TYPE = 24;
    // Above three options all have the same format:
    private static final int ICMP6_4_BYTE_OPTION_LEN      = 8;
    private static final int ICMP6_4_BYTE_LIFETIME_OFFSET = 4;
    private static final int ICMP6_4_BYTE_LIFETIME_LEN    = 4;

    private static final int UDP_HEADER_LEN              = 8;
    private static final int UDP_DESTINATION_PORT_OFFSET = ETH_HEADER_LEN + 22;

    private static final int DHCP_CLIENT_PORT       = 68;
    private static final int DHCP_CLIENT_MAC_OFFSET = ETH_HEADER_LEN + UDP_HEADER_LEN + 48;

    private static final int ARP_HEADER_OFFSET          = ETH_HEADER_LEN;
    private static final byte[] ARP_IPV4_REQUEST_HEADER = {
            0, 1, // Hardware type: Ethernet (1)
            8, 0, // Protocol type: IP (0x0800)
            6,    // Hardware size: 6
            4,    // Protocol size: 4
            0, 1  // Opcode: request (1)
    };
    private static final byte[] ARP_IPV4_REPLY_HEADER = {
            0, 1, // Hardware type: Ethernet (1)
            8, 0, // Protocol type: IP (0x0800)
            6,    // Hardware size: 6
            4,    // Protocol size: 4
            0, 2  // Opcode: reply (2)
    };
    private static final int ARP_SOURCE_IP_ADDRESS_OFFSET = ARP_HEADER_OFFSET + 14;
    private static final int ARP_TARGET_IP_ADDRESS_OFFSET = ARP_HEADER_OFFSET + 24;

    private static final byte[] MOCK_IPV4_ADDR           = {10, 0, 0, 1};
    private static final byte[] MOCK_BROADCAST_IPV4_ADDR = {10, 0, 31, (byte) 255}; // prefix = 19
    private static final byte[] MOCK_MULTICAST_IPV4_ADDR = {(byte) 224, 0, 0, 1};
    private static final byte[] ANOTHER_IPV4_ADDR        = {10, 0, 0, 2};
    private static final byte[] IPV4_SOURCE_ADDR         = {10, 0, 0, 3};
    private static final byte[] ANOTHER_IPV4_SOURCE_ADDR = {(byte) 192, 0, 2, 1};
    private static final byte[] BUG_PROBE_SOURCE_ADDR1   = {0, 0, 1, 2};
    private static final byte[] BUG_PROBE_SOURCE_ADDR2   = {3, 4, 0, 0};
    private static final byte[] IPV4_ANY_HOST_ADDR       = {0, 0, 0, 0};

    // Helper to initialize a default apfFilter.
    private ApfFilter setupApfFilter(
            IpClientCallbacksWrapper ipClientCallback, ApfConfiguration config) throws Exception {
        LinkAddress link = new LinkAddress(InetAddress.getByAddress(MOCK_IPV4_ADDR), 19);
        LinkProperties lp = new LinkProperties();
        lp.addLinkAddress(link);
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        apfFilter.setLinkProperties(lp);
        return apfFilter;
    }

    private static void setIpv4VersionFields(ByteBuffer packet) {
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short) ETH_P_IP);
        packet.put(IP_HEADER_OFFSET, (byte) 0x45);
    }

    private static void setIpv6VersionFields(ByteBuffer packet) {
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short) ETH_P_IPV6);
        packet.put(IP_HEADER_OFFSET, (byte) 0x60);
    }

    private static ByteBuffer makeIpv4Packet(int proto) {
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        setIpv4VersionFields(packet);
        packet.put(IPV4_PROTOCOL_OFFSET, (byte) proto);
        return packet;
    }

    private static ByteBuffer makeIpv6Packet(int nextHeader) {
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        setIpv6VersionFields(packet);
        packet.put(IPV6_NEXT_HEADER_OFFSET, (byte) nextHeader);
        return packet;
    }

    @Test
    public void testApfFilterIPv4() throws Exception {
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        LinkAddress link = new LinkAddress(InetAddress.getByAddress(MOCK_IPV4_ADDR), 19);
        LinkProperties lp = new LinkProperties();
        lp.addLinkAddress(link);

        ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        apfFilter.setLinkProperties(lp);

        byte[] program = ipClientCallback.getApfProgram();

        // Verify empty packet of 100 zero bytes is passed
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        assertPass(program, packet.array());

        // Verify unicast IPv4 packet is passed
        put(packet, ETH_DEST_ADDR_OFFSET, TestApfFilter.MOCK_MAC_ADDR);
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_IP);
        put(packet, IPV4_DEST_ADDR_OFFSET, MOCK_IPV4_ADDR);
        assertPass(program, packet.array());

        // Verify L2 unicast to IPv4 broadcast addresses is dropped (b/30231088)
        put(packet, IPV4_DEST_ADDR_OFFSET, IPV4_BROADCAST_ADDRESS);
        assertDrop(program, packet.array());
        put(packet, IPV4_DEST_ADDR_OFFSET, MOCK_BROADCAST_IPV4_ADDR);
        assertDrop(program, packet.array());

        // Verify multicast/broadcast IPv4, not DHCP to us, is dropped
        put(packet, ETH_DEST_ADDR_OFFSET, ETH_BROADCAST_MAC_ADDRESS);
        assertDrop(program, packet.array());
        packet.put(IP_HEADER_OFFSET, (byte) 0x45);
        assertDrop(program, packet.array());
        packet.put(IPV4_PROTOCOL_OFFSET, (byte)IPPROTO_UDP);
        assertDrop(program, packet.array());
        packet.putShort(UDP_DESTINATION_PORT_OFFSET, (short)DHCP_CLIENT_PORT);
        assertDrop(program, packet.array());
        put(packet, IPV4_DEST_ADDR_OFFSET, MOCK_MULTICAST_IPV4_ADDR);
        assertDrop(program, packet.array());
        put(packet, IPV4_DEST_ADDR_OFFSET, MOCK_BROADCAST_IPV4_ADDR);
        assertDrop(program, packet.array());
        put(packet, IPV4_DEST_ADDR_OFFSET, IPV4_BROADCAST_ADDRESS);
        assertDrop(program, packet.array());

        // Verify broadcast IPv4 DHCP to us is passed
        put(packet, DHCP_CLIENT_MAC_OFFSET, TestApfFilter.MOCK_MAC_ADDR);
        assertPass(program, packet.array());

        // Verify unicast IPv4 DHCP to us is passed
        put(packet, ETH_DEST_ADDR_OFFSET, TestApfFilter.MOCK_MAC_ADDR);
        assertPass(program, packet.array());

        apfFilter.shutdown();
    }

    @Test
    public void testApfFilterIPv6() throws Exception {
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        byte[] program = ipClientCallback.getApfProgram();

        // Verify empty IPv6 packet is passed
        ByteBuffer packet = makeIpv6Packet(IPPROTO_UDP);
        assertPass(program, packet.array());

        // Verify empty ICMPv6 packet is passed
        packet.put(IPV6_NEXT_HEADER_OFFSET, (byte)IPPROTO_ICMPV6);
        assertPass(program, packet.array());

        // Verify empty ICMPv6 NA packet is passed
        packet.put(ICMP6_TYPE_OFFSET, (byte)ICMP6_NEIGHBOR_ANNOUNCEMENT);
        assertPass(program, packet.array());

        // Verify ICMPv6 NA to ff02::1 is dropped
        put(packet, IPV6_DEST_ADDR_OFFSET, IPV6_ALL_NODES_ADDRESS);
        assertDrop(program, packet.array());

        // Verify ICMPv6 RS to any is dropped
        packet.put(ICMP6_TYPE_OFFSET, (byte)ICMP6_ROUTER_SOLICITATION);
        assertDrop(program, packet.array());
        put(packet, IPV6_DEST_ADDR_OFFSET, IPV6_ALL_ROUTERS_ADDRESS);
        assertDrop(program, packet.array());

        apfFilter.shutdown();
    }

    @Test
    public void testApfFilterMulticast() throws Exception {
        final byte[] unicastIpv4Addr   = {(byte)192,0,2,63};
        final byte[] broadcastIpv4Addr = {(byte)192,0,2,(byte)255};
        final byte[] multicastIpv4Addr = {(byte)224,0,0,1};
        final byte[] multicastIpv6Addr = {(byte)0xff,2,0,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xfb};

        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        LinkAddress link = new LinkAddress(InetAddress.getByAddress(unicastIpv4Addr), 24);
        LinkProperties lp = new LinkProperties();
        lp.addLinkAddress(link);

        ApfConfiguration config = getDefaultConfig();
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        apfFilter.setLinkProperties(lp);

        byte[] program = ipClientCallback.getApfProgram();

        // Construct IPv4 and IPv6 multicast packets.
        ByteBuffer mcastv4packet = makeIpv4Packet(IPPROTO_UDP);
        put(mcastv4packet, IPV4_DEST_ADDR_OFFSET, multicastIpv4Addr);

        ByteBuffer mcastv6packet = makeIpv6Packet(IPPROTO_UDP);
        put(mcastv6packet, IPV6_DEST_ADDR_OFFSET, multicastIpv6Addr);

        // Construct IPv4 broadcast packet.
        ByteBuffer bcastv4packet1 = makeIpv4Packet(IPPROTO_UDP);
        bcastv4packet1.put(ETH_BROADCAST_MAC_ADDRESS);
        bcastv4packet1.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_IP);
        put(bcastv4packet1, IPV4_DEST_ADDR_OFFSET, multicastIpv4Addr);

        ByteBuffer bcastv4packet2 = makeIpv4Packet(IPPROTO_UDP);
        bcastv4packet2.put(ETH_BROADCAST_MAC_ADDRESS);
        bcastv4packet2.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_IP);
        put(bcastv4packet2, IPV4_DEST_ADDR_OFFSET, IPV4_BROADCAST_ADDRESS);

        // Construct IPv4 broadcast with L2 unicast address packet (b/30231088).
        ByteBuffer bcastv4unicastl2packet = makeIpv4Packet(IPPROTO_UDP);
        bcastv4unicastl2packet.put(TestApfFilter.MOCK_MAC_ADDR);
        bcastv4unicastl2packet.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_IP);
        put(bcastv4unicastl2packet, IPV4_DEST_ADDR_OFFSET, broadcastIpv4Addr);

        // Verify initially disabled multicast filter is off
        assertPass(program, mcastv4packet.array());
        assertPass(program, mcastv6packet.array());
        assertPass(program, bcastv4packet1.array());
        assertPass(program, bcastv4packet2.array());
        assertPass(program, bcastv4unicastl2packet.array());

        // Turn on multicast filter and verify it works
        ipClientCallback.resetApfProgramWait();
        apfFilter.setMulticastFilter(true);
        program = ipClientCallback.getApfProgram();
        assertDrop(program, mcastv4packet.array());
        assertDrop(program, mcastv6packet.array());
        assertDrop(program, bcastv4packet1.array());
        assertDrop(program, bcastv4packet2.array());
        assertDrop(program, bcastv4unicastl2packet.array());

        // Turn off multicast filter and verify it's off
        ipClientCallback.resetApfProgramWait();
        apfFilter.setMulticastFilter(false);
        program = ipClientCallback.getApfProgram();
        assertPass(program, mcastv4packet.array());
        assertPass(program, mcastv6packet.array());
        assertPass(program, bcastv4packet1.array());
        assertPass(program, bcastv4packet2.array());
        assertPass(program, bcastv4unicastl2packet.array());

        // Verify it can be initialized to on
        ipClientCallback.resetApfProgramWait();
        apfFilter.shutdown();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        apfFilter.setLinkProperties(lp);
        program = ipClientCallback.getApfProgram();
        assertDrop(program, mcastv4packet.array());
        assertDrop(program, mcastv6packet.array());
        assertDrop(program, bcastv4packet1.array());
        assertDrop(program, bcastv4unicastl2packet.array());

        // Verify that ICMPv6 multicast is not dropped.
        mcastv6packet.put(IPV6_NEXT_HEADER_OFFSET, (byte)IPPROTO_ICMPV6);
        assertPass(program, mcastv6packet.array());

        apfFilter.shutdown();
    }

    @Test
    public void testApfFilterMulticastPingWhileDozing() throws Exception {
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        ApfFilter apfFilter = setupApfFilter(ipClientCallback, getDefaultConfig());

        // Construct a multicast ICMPv6 ECHO request.
        final byte[] multicastIpv6Addr = {(byte)0xff,2,0,0,0,0,0,0,0,0,0,0,0,0,0,(byte)0xfb};
        ByteBuffer packet = makeIpv6Packet(IPPROTO_ICMPV6);
        packet.put(ICMP6_TYPE_OFFSET, (byte)ICMPV6_ECHO_REQUEST_TYPE);
        put(packet, IPV6_DEST_ADDR_OFFSET, multicastIpv6Addr);

        // Normally, we let multicast pings alone...
        assertPass(ipClientCallback.getApfProgram(), packet.array());

        // ...and even while dozing...
        apfFilter.setDozeMode(true);
        assertPass(ipClientCallback.getApfProgram(), packet.array());

        // ...but when the multicast filter is also enabled, drop the multicast pings to save power.
        apfFilter.setMulticastFilter(true);
        assertDrop(ipClientCallback.getApfProgram(), packet.array());

        // However, we should still let through all other ICMPv6 types.
        ByteBuffer raPacket = ByteBuffer.wrap(packet.array().clone());
        setIpv6VersionFields(packet);
        packet.put(IPV6_NEXT_HEADER_OFFSET, (byte) IPPROTO_ICMPV6);
        raPacket.put(ICMP6_TYPE_OFFSET, (byte) NetworkStackConstants.ICMPV6_ROUTER_ADVERTISEMENT);
        assertPass(ipClientCallback.getApfProgram(), raPacket.array());

        // Now wake up from doze mode to ensure that we no longer drop the packets.
        // (The multicast filter is still enabled at this point).
        apfFilter.setDozeMode(false);
        assertPass(ipClientCallback.getApfProgram(), packet.array());

        apfFilter.shutdown();
    }

    @Test
    public void testApfFilter802_3() throws Exception {
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        ApfFilter apfFilter = setupApfFilter(ipClientCallback, config);
        byte[] program = ipClientCallback.getApfProgram();

        // Verify empty packet of 100 zero bytes is passed
        // Note that eth-type = 0 makes it an IEEE802.3 frame
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        assertPass(program, packet.array());

        // Verify empty packet with IPv4 is passed
        setIpv4VersionFields(packet);
        assertPass(program, packet.array());

        // Verify empty IPv6 packet is passed
        setIpv6VersionFields(packet);
        assertPass(program, packet.array());

        // Now turn on the filter
        ipClientCallback.resetApfProgramWait();
        apfFilter.shutdown();
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        apfFilter = setupApfFilter(ipClientCallback, config);
        program = ipClientCallback.getApfProgram();

        // Verify that IEEE802.3 frame is dropped
        // In this case ethtype is used for payload length
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short)(100 - 14));
        assertDrop(program, packet.array());

        // Verify that IPv4 (as example of Ethernet II) frame will pass
        setIpv4VersionFields(packet);
        assertPass(program, packet.array());

        // Verify that IPv6 (as example of Ethernet II) frame will pass
        setIpv6VersionFields(packet);
        assertPass(program, packet.array());

        apfFilter.shutdown();
    }

    @Test
    public void testApfFilterEthTypeBL() throws Exception {
        final int[] emptyBlackList = {};
        final int[] ipv4BlackList = {ETH_P_IP};
        final int[] ipv4Ipv6BlackList = {ETH_P_IP, ETH_P_IPV6};

        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        ApfFilter apfFilter = setupApfFilter(ipClientCallback, config);
        byte[] program = ipClientCallback.getApfProgram();

        // Verify empty packet of 100 zero bytes is passed
        // Note that eth-type = 0 makes it an IEEE802.3 frame
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        assertPass(program, packet.array());

        // Verify empty packet with IPv4 is passed
        setIpv4VersionFields(packet);
        assertPass(program, packet.array());

        // Verify empty IPv6 packet is passed
        setIpv6VersionFields(packet);
        assertPass(program, packet.array());

        // Now add IPv4 to the black list
        ipClientCallback.resetApfProgramWait();
        apfFilter.shutdown();
        config.ethTypeBlackList = ipv4BlackList;
        apfFilter = setupApfFilter(ipClientCallback, config);
        program = ipClientCallback.getApfProgram();

        // Verify that IPv4 frame will be dropped
        setIpv4VersionFields(packet);
        assertDrop(program, packet.array());

        // Verify that IPv6 frame will pass
        setIpv6VersionFields(packet);
        assertPass(program, packet.array());

        // Now let us have both IPv4 and IPv6 in the black list
        ipClientCallback.resetApfProgramWait();
        apfFilter.shutdown();
        config.ethTypeBlackList = ipv4Ipv6BlackList;
        apfFilter = setupApfFilter(ipClientCallback, config);
        program = ipClientCallback.getApfProgram();

        // Verify that IPv4 frame will be dropped
        setIpv4VersionFields(packet);
        assertDrop(program, packet.array());

        // Verify that IPv6 frame will be dropped
        setIpv6VersionFields(packet);
        assertDrop(program, packet.array());

        apfFilter.shutdown();
    }

    private byte[] getProgram(MockIpClientCallback cb, ApfFilter filter, LinkProperties lp) {
        cb.resetApfProgramWait();
        filter.setLinkProperties(lp);
        return cb.getApfProgram();
    }

    private void verifyArpFilter(byte[] program, int filterResult) {
        // Verify ARP request packet
        assertPass(program, arpRequestBroadcast(MOCK_IPV4_ADDR));
        assertVerdict(filterResult, program, arpRequestBroadcast(ANOTHER_IPV4_ADDR));
        assertDrop(program, arpRequestBroadcast(IPV4_ANY_HOST_ADDR));

        // Verify ARP reply packets from different source ip
        assertDrop(program, arpReply(IPV4_ANY_HOST_ADDR, IPV4_ANY_HOST_ADDR));
        assertPass(program, arpReply(ANOTHER_IPV4_SOURCE_ADDR, IPV4_ANY_HOST_ADDR));
        assertPass(program, arpReply(BUG_PROBE_SOURCE_ADDR1, IPV4_ANY_HOST_ADDR));
        assertPass(program, arpReply(BUG_PROBE_SOURCE_ADDR2, IPV4_ANY_HOST_ADDR));

        // Verify unicast ARP reply packet is always accepted.
        assertPass(program, arpReply(IPV4_SOURCE_ADDR, MOCK_IPV4_ADDR));
        assertPass(program, arpReply(IPV4_SOURCE_ADDR, ANOTHER_IPV4_ADDR));
        assertPass(program, arpReply(IPV4_SOURCE_ADDR, IPV4_ANY_HOST_ADDR));

        // Verify GARP reply packets are always filtered
        assertDrop(program, garpReply());
    }

    @Test
    public void testApfFilterArp() throws Exception {
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);

        // Verify initially ARP request filter is off, and GARP filter is on.
        verifyArpFilter(ipClientCallback.getApfProgram(), PASS);

        // Inform ApfFilter of our address and verify ARP filtering is on
        LinkAddress linkAddress = new LinkAddress(InetAddress.getByAddress(MOCK_IPV4_ADDR), 24);
        LinkProperties lp = new LinkProperties();
        assertTrue(lp.addLinkAddress(linkAddress));
        verifyArpFilter(getProgram(ipClientCallback, apfFilter, lp), DROP);

        // Inform ApfFilter of loss of IP and verify ARP filtering is off
        verifyArpFilter(getProgram(ipClientCallback, apfFilter, new LinkProperties()), PASS);

        apfFilter.shutdown();
    }

    private static byte[] arpReply(byte[] sip, byte[] tip) {
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_ARP);
        put(packet, ARP_HEADER_OFFSET, ARP_IPV4_REPLY_HEADER);
        put(packet, ARP_SOURCE_IP_ADDRESS_OFFSET, sip);
        put(packet, ARP_TARGET_IP_ADDRESS_OFFSET, tip);
        return packet.array();
    }

    private static byte[] arpRequestBroadcast(byte[] tip) {
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_ARP);
        put(packet, ETH_DEST_ADDR_OFFSET, ETH_BROADCAST_MAC_ADDRESS);
        put(packet, ARP_HEADER_OFFSET, ARP_IPV4_REQUEST_HEADER);
        put(packet, ARP_TARGET_IP_ADDRESS_OFFSET, tip);
        return packet.array();
    }

    private static byte[] garpReply() {
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short)ETH_P_ARP);
        put(packet, ETH_DEST_ADDR_OFFSET, ETH_BROADCAST_MAC_ADDRESS);
        put(packet, ARP_HEADER_OFFSET, ARP_IPV4_REPLY_HEADER);
        put(packet, ARP_TARGET_IP_ADDRESS_OFFSET, IPV4_ANY_HOST_ADDR);
        return packet.array();
    }

    private static final byte[] IPV4_KEEPALIVE_SRC_ADDR = {10, 0, 0, 5};
    private static final byte[] IPV4_KEEPALIVE_DST_ADDR = {10, 0, 0, 6};
    private static final byte[] IPV4_ANOTHER_ADDR = {10, 0 , 0, 7};
    private static final byte[] IPV6_KEEPALIVE_SRC_ADDR =
            {(byte) 0x24, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (byte) 0xfa, (byte) 0xf1};
    private static final byte[] IPV6_KEEPALIVE_DST_ADDR =
            {(byte) 0x24, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (byte) 0xfa, (byte) 0xf2};
    private static final byte[] IPV6_ANOTHER_ADDR =
            {(byte) 0x24, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (byte) 0xfa, (byte) 0xf5};

    @Test
    public void testApfFilterKeepaliveAck() throws Exception {
        final MockIpClientCallback cb = new MockIpClientCallback();
        final ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        final TestApfFilter apfFilter = new TestApfFilter(mContext, config, cb, mLog);
        byte[] program;
        final int srcPort = 12345;
        final int dstPort = 54321;
        final int seqNum = 2123456789;
        final int ackNum = 1234567890;
        final int anotherSrcPort = 23456;
        final int anotherDstPort = 65432;
        final int anotherSeqNum = 2123456780;
        final int anotherAckNum = 1123456789;
        final int slot1 = 1;
        final int slot2 = 2;
        final int window = 14480;
        final int windowScale = 4;

        // src: 10.0.0.5, port: 12345
        // dst: 10.0.0.6, port: 54321
        InetAddress srcAddr = InetAddress.getByAddress(IPV4_KEEPALIVE_SRC_ADDR);
        InetAddress dstAddr = InetAddress.getByAddress(IPV4_KEEPALIVE_DST_ADDR);

        final TcpKeepalivePacketDataParcelable parcel = new TcpKeepalivePacketDataParcelable();
        parcel.srcAddress = srcAddr.getAddress();
        parcel.srcPort = srcPort;
        parcel.dstAddress = dstAddr.getAddress();
        parcel.dstPort = dstPort;
        parcel.seq = seqNum;
        parcel.ack = ackNum;

        apfFilter.addTcpKeepalivePacketFilter(slot1, parcel);
        program = cb.getApfProgram();

        // Verify IPv4 keepalive ack packet is dropped
        // src: 10.0.0.6, port: 54321
        // dst: 10.0.0.5, port: 12345
        assertDrop(program,
                ipv4TcpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, ackNum, seqNum + 1, 0 /* dataLength */));
        // Verify IPv4 non-keepalive ack packet from the same source address is passed
        assertPass(program,
                ipv4TcpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, ackNum + 100, seqNum, 0 /* dataLength */));
        assertPass(program,
                ipv4TcpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, ackNum, seqNum + 1, 10 /* dataLength */));
        // Verify IPv4 packet from another address is passed
        assertPass(program,
                ipv4TcpPacket(IPV4_ANOTHER_ADDR, IPV4_KEEPALIVE_SRC_ADDR, anotherSrcPort,
                        anotherDstPort, anotherSeqNum, anotherAckNum, 0 /* dataLength */));

        // Remove IPv4 keepalive filter
        apfFilter.removeKeepalivePacketFilter(slot1);

        try {
            // src: 2404:0:0:0:0:0:faf1, port: 12345
            // dst: 2404:0:0:0:0:0:faf2, port: 54321
            srcAddr = InetAddress.getByAddress(IPV6_KEEPALIVE_SRC_ADDR);
            dstAddr = InetAddress.getByAddress(IPV6_KEEPALIVE_DST_ADDR);

            final TcpKeepalivePacketDataParcelable ipv6Parcel =
                    new TcpKeepalivePacketDataParcelable();
            ipv6Parcel.srcAddress = srcAddr.getAddress();
            ipv6Parcel.srcPort = srcPort;
            ipv6Parcel.dstAddress = dstAddr.getAddress();
            ipv6Parcel.dstPort = dstPort;
            ipv6Parcel.seq = seqNum;
            ipv6Parcel.ack = ackNum;

            apfFilter.addTcpKeepalivePacketFilter(slot1, ipv6Parcel);
            program = cb.getApfProgram();

            // Verify IPv6 keepalive ack packet is dropped
            // src: 2404:0:0:0:0:0:faf2, port: 54321
            // dst: 2404:0:0:0:0:0:faf1, port: 12345
            assertDrop(program,
                    ipv6TcpPacket(IPV6_KEEPALIVE_DST_ADDR, IPV6_KEEPALIVE_SRC_ADDR,
                            dstPort, srcPort, ackNum, seqNum + 1));
            // Verify IPv6 non-keepalive ack packet from the same source address is passed
            assertPass(program,
                    ipv6TcpPacket(IPV6_KEEPALIVE_DST_ADDR, IPV6_KEEPALIVE_SRC_ADDR,
                            dstPort, srcPort, ackNum + 100, seqNum));
            // Verify IPv6 packet from another address is passed
            assertPass(program,
                    ipv6TcpPacket(IPV6_ANOTHER_ADDR, IPV6_KEEPALIVE_SRC_ADDR, anotherSrcPort,
                            anotherDstPort, anotherSeqNum, anotherAckNum));

            // Remove IPv6 keepalive filter
            apfFilter.removeKeepalivePacketFilter(slot1);

            // Verify multiple filters
            apfFilter.addTcpKeepalivePacketFilter(slot1, parcel);
            apfFilter.addTcpKeepalivePacketFilter(slot2, ipv6Parcel);
            program = cb.getApfProgram();

            // Verify IPv4 keepalive ack packet is dropped
            // src: 10.0.0.6, port: 54321
            // dst: 10.0.0.5, port: 12345
            assertDrop(program,
                    ipv4TcpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                            dstPort, srcPort, ackNum, seqNum + 1, 0 /* dataLength */));
            // Verify IPv4 non-keepalive ack packet from the same source address is passed
            assertPass(program,
                    ipv4TcpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                            dstPort, srcPort, ackNum + 100, seqNum, 0 /* dataLength */));
            // Verify IPv4 packet from another address is passed
            assertPass(program,
                    ipv4TcpPacket(IPV4_ANOTHER_ADDR, IPV4_KEEPALIVE_SRC_ADDR, anotherSrcPort,
                            anotherDstPort, anotherSeqNum, anotherAckNum, 0 /* dataLength */));

            // Verify IPv6 keepalive ack packet is dropped
            // src: 2404:0:0:0:0:0:faf2, port: 54321
            // dst: 2404:0:0:0:0:0:faf1, port: 12345
            assertDrop(program,
                    ipv6TcpPacket(IPV6_KEEPALIVE_DST_ADDR, IPV6_KEEPALIVE_SRC_ADDR,
                            dstPort, srcPort, ackNum, seqNum + 1));
            // Verify IPv6 non-keepalive ack packet from the same source address is passed
            assertPass(program,
                    ipv6TcpPacket(IPV6_KEEPALIVE_DST_ADDR, IPV6_KEEPALIVE_SRC_ADDR,
                            dstPort, srcPort, ackNum + 100, seqNum));
            // Verify IPv6 packet from another address is passed
            assertPass(program,
                    ipv6TcpPacket(IPV6_ANOTHER_ADDR, IPV6_KEEPALIVE_SRC_ADDR, anotherSrcPort,
                            anotherDstPort, anotherSeqNum, anotherAckNum));

            // Remove keepalive filters
            apfFilter.removeKeepalivePacketFilter(slot1);
            apfFilter.removeKeepalivePacketFilter(slot2);
        } catch (UnsupportedOperationException e) {
            // TODO: support V6 packets
        }

        program = cb.getApfProgram();

        // Verify IPv4, IPv6 packets are passed
        assertPass(program,
                ipv4TcpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, ackNum, seqNum + 1, 0 /* dataLength */));
        assertPass(program,
                ipv6TcpPacket(IPV6_KEEPALIVE_DST_ADDR, IPV6_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, ackNum, seqNum + 1));
        assertPass(program,
                ipv4TcpPacket(IPV4_ANOTHER_ADDR, IPV4_KEEPALIVE_SRC_ADDR, srcPort,
                        dstPort, anotherSeqNum, anotherAckNum, 0 /* dataLength */));
        assertPass(program,
                ipv6TcpPacket(IPV6_ANOTHER_ADDR, IPV6_KEEPALIVE_SRC_ADDR, srcPort,
                        dstPort, anotherSeqNum, anotherAckNum));

        apfFilter.shutdown();
    }

    private static byte[] ipv4TcpPacket(byte[] sip, byte[] dip, int sport,
            int dport, int seq, int ack, int dataLength) {
        final int totalLength = dataLength + IPV4_HEADER_LEN + IPV4_TCP_HEADER_LEN;

        ByteBuffer packet = ByteBuffer.wrap(new byte[totalLength + ETH_HEADER_LEN]);

        // Ethertype and IPv4 header
        setIpv4VersionFields(packet);
        packet.putShort(IPV4_TOTAL_LENGTH_OFFSET, (short) totalLength);
        packet.put(IPV4_PROTOCOL_OFFSET, (byte) IPPROTO_TCP);
        put(packet, IPV4_SRC_ADDR_OFFSET, sip);
        put(packet, IPV4_DEST_ADDR_OFFSET, dip);
        packet.putShort(IPV4_TCP_SRC_PORT_OFFSET, (short) sport);
        packet.putShort(IPV4_TCP_DEST_PORT_OFFSET, (short) dport);
        packet.putInt(IPV4_TCP_SEQ_NUM_OFFSET, seq);
        packet.putInt(IPV4_TCP_ACK_NUM_OFFSET, ack);

        // TCP header length 5(20 bytes), reserved 3 bits, NS=0
        packet.put(IPV4_TCP_HEADER_LENGTH_OFFSET, (byte) 0x50);
        // TCP flags: ACK set
        packet.put(IPV4_TCP_HEADER_FLAG_OFFSET, (byte) 0x10);
        return packet.array();
    }

    private static byte[] ipv6TcpPacket(byte[] sip, byte[] tip, int sport,
            int dport, int seq, int ack) {
        ByteBuffer packet = ByteBuffer.wrap(new byte[100]);
        setIpv6VersionFields(packet);
        packet.put(IPV6_NEXT_HEADER_OFFSET, (byte) IPPROTO_TCP);
        put(packet, IPV6_SRC_ADDR_OFFSET, sip);
        put(packet, IPV6_DEST_ADDR_OFFSET, tip);
        packet.putShort(IPV6_TCP_SRC_PORT_OFFSET, (short) sport);
        packet.putShort(IPV6_TCP_DEST_PORT_OFFSET, (short) dport);
        packet.putInt(IPV6_TCP_SEQ_NUM_OFFSET, seq);
        packet.putInt(IPV6_TCP_ACK_NUM_OFFSET, ack);
        return packet.array();
    }

    @Test
    public void testApfFilterNattKeepalivePacket() throws Exception {
        final MockIpClientCallback cb = new MockIpClientCallback();
        final ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        final TestApfFilter apfFilter = new TestApfFilter(mContext, config, cb, mLog);
        byte[] program;
        final int srcPort = 1024;
        final int dstPort = 4500;
        final int slot1 = 1;
        // NAT-T keepalive
        final byte[] kaPayload = {(byte) 0xff};
        final byte[] nonKaPayload = {(byte) 0xfe};

        // src: 10.0.0.5, port: 1024
        // dst: 10.0.0.6, port: 4500
        InetAddress srcAddr = InetAddress.getByAddress(IPV4_KEEPALIVE_SRC_ADDR);
        InetAddress dstAddr = InetAddress.getByAddress(IPV4_KEEPALIVE_DST_ADDR);

        final NattKeepalivePacketDataParcelable parcel = new NattKeepalivePacketDataParcelable();
        parcel.srcAddress = srcAddr.getAddress();
        parcel.srcPort = srcPort;
        parcel.dstAddress = dstAddr.getAddress();
        parcel.dstPort = dstPort;

        apfFilter.addNattKeepalivePacketFilter(slot1, parcel);
        program = cb.getApfProgram();

        // Verify IPv4 keepalive packet is dropped
        // src: 10.0.0.6, port: 4500
        // dst: 10.0.0.5, port: 1024
        byte[] pkt = ipv4UdpPacket(IPV4_KEEPALIVE_DST_ADDR,
                    IPV4_KEEPALIVE_SRC_ADDR, dstPort, srcPort, 1 /* dataLength */);
        System.arraycopy(kaPayload, 0, pkt, IPV4_UDP_PAYLOAD_OFFSET, kaPayload.length);
        assertDrop(program, pkt);

        // Verify a packet with payload length 1 byte but it is not 0xff will pass the filter.
        System.arraycopy(nonKaPayload, 0, pkt, IPV4_UDP_PAYLOAD_OFFSET, nonKaPayload.length);
        assertPass(program, pkt);

        // Verify IPv4 non-keepalive response packet from the same source address is passed
        assertPass(program,
                ipv4UdpPacket(IPV4_KEEPALIVE_DST_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, 10 /* dataLength */));

        // Verify IPv4 non-keepalive response packet from other source address is passed
        assertPass(program,
                ipv4UdpPacket(IPV4_ANOTHER_ADDR, IPV4_KEEPALIVE_SRC_ADDR,
                        dstPort, srcPort, 10 /* dataLength */));

        apfFilter.removeKeepalivePacketFilter(slot1);
        apfFilter.shutdown();
    }

    private static byte[] ipv4UdpPacket(byte[] sip, byte[] dip, int sport,
            int dport, int dataLength) {
        final int totalLength = dataLength + IPV4_HEADER_LEN + UDP_HEADER_LEN;
        final int udpLength = UDP_HEADER_LEN + dataLength;
        ByteBuffer packet = ByteBuffer.wrap(new byte[totalLength + ETH_HEADER_LEN]);

        // Ethertype and IPv4 header
        setIpv4VersionFields(packet);
        packet.putShort(IPV4_TOTAL_LENGTH_OFFSET, (short) totalLength);
        packet.put(IPV4_PROTOCOL_OFFSET, (byte) IPPROTO_UDP);
        put(packet, IPV4_SRC_ADDR_OFFSET, sip);
        put(packet, IPV4_DEST_ADDR_OFFSET, dip);
        packet.putShort(IPV4_UDP_SRC_PORT_OFFSET, (short) sport);
        packet.putShort(IPV4_UDP_DEST_PORT_OFFSET, (short) dport);
        packet.putShort(IPV4_UDP_LENGTH_OFFSET, (short) udpLength);

        return packet.array();
    }

    private void addRdnssOption(ByteBuffer packet, int lifetime, String... servers)
            throws Exception {
        int optionLength = 1 + 2 * servers.length;   // In 8-byte units
        packet.put((byte) ICMP6_RDNSS_OPTION_TYPE);  // Type
        packet.put((byte) optionLength);             // Length
        packet.putShort((short) 0);                  // Reserved
        packet.putInt(lifetime);                     // Lifetime
        for (String server : servers) {
            packet.put(InetAddress.getByName(server).getAddress());
        }
    }

    private void addRioOption(ByteBuffer packet, int lifetime, String prefixString)
            throws Exception {
        IpPrefix prefix = new IpPrefix(prefixString);

        int optionLength;
        if (prefix.getPrefixLength() == 0) {
            optionLength = 1;
        } else if (prefix.getPrefixLength() <= 64) {
            optionLength = 2;
        } else {
            optionLength = 3;
        }

        packet.put((byte) ICMP6_ROUTE_INFO_OPTION_TYPE);  // Type
        packet.put((byte) optionLength);                  // Length in 8-byte units
        packet.put((byte) prefix.getPrefixLength());      // Prefix length
        packet.put((byte) 0b00011000);                    // Pref = high
        packet.putInt(lifetime);                          // Lifetime

        byte[] prefixBytes = prefix.getRawAddress();
        packet.put(prefixBytes, 0, (optionLength - 1) * 8);
    }

    private void addPioOption(ByteBuffer packet, int valid, int preferred, String prefixString) {
        IpPrefix prefix = new IpPrefix(prefixString);
        packet.put((byte) ICMP6_PREFIX_OPTION_TYPE);  // Type
        packet.put((byte) 4);                         // Length in 8-byte units
        packet.put((byte) prefix.getPrefixLength());  // Prefix length
        packet.put((byte) 0b11000000);                // L = 1, A = 1
        packet.putInt(valid);
        packet.putInt(preferred);
        packet.putInt(0);                             // Reserved
        packet.put(prefix.getRawAddress());
    }

    private byte[] buildLargeRa() throws Exception {
        InetAddress src = InetAddress.getByName("fe80::1234:abcd");

        ByteBuffer packet = ByteBuffer.wrap(new byte[1514]);
        packet.putShort(ETH_ETHERTYPE_OFFSET, (short) ETH_P_IPV6);
        packet.position(ETH_HEADER_LEN);

        packet.putInt(0x60012345);                                  // Version, tclass, flowlabel
        packet.putShort((short) 0);                                 // Payload length; updated later
        packet.put((byte) IPPROTO_ICMPV6);                          // Next header
        packet.put((byte) 0xff);                                    // Hop limit
        packet.put(src.getAddress());                               // Source address
        packet.put(IPV6_ALL_NODES_ADDRESS);                         // Destination address

        packet.put((byte) ICMP6_ROUTER_ADVERTISEMENT);              // Type
        packet.put((byte) 0);                                       // Code (0)
        packet.putShort((short) 0);                                 // Checksum (ignored)
        packet.put((byte) 64);                                      // Hop limit
        packet.put((byte) 0);                                       // M/O, reserved
        packet.putShort((short) 1800);                              // Router lifetime
        packet.putInt(30_000);                                      // Reachable time
        packet.putInt(1000);                                        // Retrans timer

        addRioOption(packet, 1200, "64:ff9b::/96");
        addRdnssOption(packet, 7200, "2001:db8:1::1", "2001:db8:1::2");
        addRioOption(packet, 2100, "2000::/3");
        addRioOption(packet, 2400, "::/0");
        addPioOption(packet, 600, 300, "2001:db8:a::/64");
        addRioOption(packet, 1500, "2001:db8:c:d::/64");
        addPioOption(packet, 86400, 43200, "fd95:d1e:12::/64");

        int length = packet.position();
        packet.putShort(IPV6_PAYLOAD_LENGTH_OFFSET, (short) length);

        // Don't pass the Ra constructor a packet that is longer than the actual RA.
        // This relies on the fact that all the relative writes to the byte buffer are at the end.
        byte[] packetArray = new byte[length];
        packet.rewind();
        packet.get(packetArray);
        return packetArray;
    }

    @Test
    public void testRaToString() throws Exception {
        MockIpClientCallback cb = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, cb, mLog);

        byte[] packet = buildLargeRa();
        ApfFilter.Ra ra = apfFilter.new Ra(packet, packet.length);
        String expected = "RA fe80::1234:abcd -> ff02::1 1800s "
                + "2001:db8:a::/64 600s/300s fd95:d1e:12::/64 86400s/43200s "
                + "DNS 7200s 2001:db8:1::1 2001:db8:1::2 "
                + "RIO 1200s 64:ff9b::/96 RIO 2100s 2000::/3 "
                + "RIO 2400s ::/0 RIO 1500s 2001:db8:c:d::/64 ";
        assertEquals(expected, ra.toString());
    }

    // Verify that the last program pushed to the IpClient.Callback properly filters the
    // given packet for the given lifetime.
    private void verifyRaLifetime(byte[] program, ByteBuffer packet, int lifetime) {
        final int FRACTION_OF_LIFETIME = 6;
        final int ageLimit = lifetime / FRACTION_OF_LIFETIME;

        // Verify new program should drop RA for 1/6th its lifetime and pass afterwards.
        assertDrop(program, packet.array());
        assertDrop(program, packet.array(), ageLimit);
        assertPass(program, packet.array(), ageLimit + 1);
        assertPass(program, packet.array(), lifetime);
        // Verify RA checksum is ignored
        final short originalChecksum = packet.getShort(ICMP6_RA_CHECKSUM_OFFSET);
        packet.putShort(ICMP6_RA_CHECKSUM_OFFSET, (short)12345);
        assertDrop(program, packet.array());
        packet.putShort(ICMP6_RA_CHECKSUM_OFFSET, (short)-12345);
        assertDrop(program, packet.array());
        packet.putShort(ICMP6_RA_CHECKSUM_OFFSET, originalChecksum);

        // Verify other changes to RA (e.g., a change in the source address) make it not match.
        final int offset = IPV6_SRC_ADDR_OFFSET + 5;
        final byte originalByte = packet.get(offset);
        packet.put(offset, (byte) (~originalByte));
        assertPass(program, packet.array());
        packet.put(offset, originalByte);
        assertDrop(program, packet.array());
    }

    // Test that when ApfFilter is shown the given packet, it generates a program to filter it
    // for the given lifetime.
    private void verifyRaLifetime(TestApfFilter apfFilter, MockIpClientCallback ipClientCallback,
            ByteBuffer packet, int lifetime) throws IOException, ErrnoException {
        // Verify new program generated if ApfFilter witnesses RA
        ipClientCallback.resetApfProgramWait();
        apfFilter.pretendPacketReceived(packet.array());
        byte[] program = ipClientCallback.getApfProgram();
        verifyRaLifetime(program, packet, lifetime);
    }

    private void verifyRaEvent(RaEvent expected) {
        ArgumentCaptor<IpConnectivityLog.Event> captor =
                ArgumentCaptor.forClass(IpConnectivityLog.Event.class);
        verify(mLog, atLeastOnce()).log(captor.capture());
        RaEvent got = lastRaEvent(captor.getAllValues());
        if (!raEventEquals(expected, got)) {
            assertEquals(expected, got);  // fail for printing an assertion error message.
        }
    }

    private RaEvent lastRaEvent(List<IpConnectivityLog.Event> events) {
        RaEvent got = null;
        for (Parcelable ev : events) {
            if (ev instanceof RaEvent) {
                got = (RaEvent) ev;
            }
        }
        return got;
    }

    private boolean raEventEquals(RaEvent ev1, RaEvent ev2) {
        return (ev1 != null) && (ev2 != null)
                && (ev1.routerLifetime == ev2.routerLifetime)
                && (ev1.prefixValidLifetime == ev2.prefixValidLifetime)
                && (ev1.prefixPreferredLifetime == ev2.prefixPreferredLifetime)
                && (ev1.routeInfoLifetime == ev2.routeInfoLifetime)
                && (ev1.rdnssLifetime == ev2.rdnssLifetime)
                && (ev1.dnsslLifetime == ev2.dnsslLifetime);
    }

    private void assertInvalidRa(TestApfFilter apfFilter, MockIpClientCallback ipClientCallback,
            ByteBuffer packet) throws IOException, ErrnoException {
        ipClientCallback.resetApfProgramWait();
        apfFilter.pretendPacketReceived(packet.array());
        ipClientCallback.assertNoProgramUpdate();
    }

    private ByteBuffer makeBaseRaPacket() {
        ByteBuffer basePacket = ByteBuffer.wrap(new byte[ICMP6_RA_OPTION_OFFSET]);
        final int ROUTER_LIFETIME = 1000;
        final int VERSION_TRAFFIC_CLASS_FLOW_LABEL_OFFSET = ETH_HEADER_LEN;
        // IPv6, traffic class = 0, flow label = 0x12345
        final int VERSION_TRAFFIC_CLASS_FLOW_LABEL = 0x60012345;

        basePacket.putShort(ETH_ETHERTYPE_OFFSET, (short) ETH_P_IPV6);
        basePacket.putInt(VERSION_TRAFFIC_CLASS_FLOW_LABEL_OFFSET,
                VERSION_TRAFFIC_CLASS_FLOW_LABEL);
        basePacket.put(IPV6_NEXT_HEADER_OFFSET, (byte) IPPROTO_ICMPV6);
        basePacket.put(ICMP6_TYPE_OFFSET, (byte) ICMP6_ROUTER_ADVERTISEMENT);
        basePacket.putShort(ICMP6_RA_ROUTER_LIFETIME_OFFSET, (short) ROUTER_LIFETIME);
        basePacket.position(IPV6_DEST_ADDR_OFFSET);
        basePacket.put(IPV6_ALL_NODES_ADDRESS);

        return basePacket;
    }

    @Test
    public void testApfFilterRa() throws Exception {
        MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        byte[] program = ipClientCallback.getApfProgram();

        final int ROUTER_LIFETIME = 1000;
        final int PREFIX_VALID_LIFETIME = 200;
        final int PREFIX_PREFERRED_LIFETIME = 100;
        final int RDNSS_LIFETIME  = 300;
        final int ROUTE_LIFETIME  = 400;
        // Note that lifetime of 2000 will be ignored in favor of shorter route lifetime of 1000.
        final int DNSSL_LIFETIME  = 2000;
        final int VERSION_TRAFFIC_CLASS_FLOW_LABEL_OFFSET = ETH_HEADER_LEN;
        // IPv6, traffic class = 0, flow label = 0x12345
        final int VERSION_TRAFFIC_CLASS_FLOW_LABEL = 0x60012345;

        // Verify RA is passed the first time
        ByteBuffer basePacket = makeBaseRaPacket();
        assertPass(program, basePacket.array());

        verifyRaLifetime(apfFilter, ipClientCallback, basePacket, ROUTER_LIFETIME);
        verifyRaEvent(new RaEvent(ROUTER_LIFETIME, -1, -1, -1, -1, -1));

        ByteBuffer newFlowLabelPacket = ByteBuffer.wrap(new byte[ICMP6_RA_OPTION_OFFSET]);
        basePacket.clear();
        newFlowLabelPacket.put(basePacket);
        // Check that changes are ignored in every byte of the flow label.
        newFlowLabelPacket.putInt(VERSION_TRAFFIC_CLASS_FLOW_LABEL_OFFSET,
                VERSION_TRAFFIC_CLASS_FLOW_LABEL + 0x11111);

        // Ensure zero-length options cause the packet to be silently skipped.
        // Do this before we test other packets. http://b/29586253
        ByteBuffer zeroLengthOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_OPTION_LEN]);
        basePacket.clear();
        zeroLengthOptionPacket.put(basePacket);
        zeroLengthOptionPacket.put((byte)ICMP6_PREFIX_OPTION_TYPE);
        zeroLengthOptionPacket.put((byte)0);
        assertInvalidRa(apfFilter, ipClientCallback, zeroLengthOptionPacket);

        // Generate several RAs with different options and lifetimes, and verify when
        // ApfFilter is shown these packets, it generates programs to filter them for the
        // appropriate lifetime.
        ByteBuffer prefixOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_PREFIX_OPTION_LEN]);
        basePacket.clear();
        prefixOptionPacket.put(basePacket);
        addPioOption(prefixOptionPacket, PREFIX_VALID_LIFETIME, PREFIX_PREFERRED_LIFETIME,
                "2001:db8::/64");
        verifyRaLifetime(
                apfFilter, ipClientCallback, prefixOptionPacket, PREFIX_PREFERRED_LIFETIME);
        verifyRaEvent(new RaEvent(
                ROUTER_LIFETIME, PREFIX_VALID_LIFETIME, PREFIX_PREFERRED_LIFETIME, -1, -1, -1));

        ByteBuffer rdnssOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_OPTION_LEN + 2 * IPV6_ADDR_LEN]);
        basePacket.clear();
        rdnssOptionPacket.put(basePacket);
        addRdnssOption(rdnssOptionPacket, RDNSS_LIFETIME,
                "2001:4860:4860::8888", "2001:4860:4860::8844");
        verifyRaLifetime(apfFilter, ipClientCallback, rdnssOptionPacket, RDNSS_LIFETIME);
        verifyRaEvent(new RaEvent(ROUTER_LIFETIME, -1, -1, -1, RDNSS_LIFETIME, -1));

        final int lowLifetime = 60;
        ByteBuffer lowLifetimeRdnssOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_OPTION_LEN + IPV6_ADDR_LEN]);
        basePacket.clear();
        lowLifetimeRdnssOptionPacket.put(basePacket);
        addRdnssOption(lowLifetimeRdnssOptionPacket, lowLifetime, "2620:fe::9");
        verifyRaLifetime(apfFilter, ipClientCallback, lowLifetimeRdnssOptionPacket,
                ROUTER_LIFETIME);
        verifyRaEvent(new RaEvent(ROUTER_LIFETIME, -1, -1, -1, lowLifetime, -1));

        ByteBuffer routeInfoOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_OPTION_LEN + IPV6_ADDR_LEN]);
        basePacket.clear();
        routeInfoOptionPacket.put(basePacket);
        addRioOption(routeInfoOptionPacket, ROUTE_LIFETIME, "64:ff9b::/96");
        verifyRaLifetime(apfFilter, ipClientCallback, routeInfoOptionPacket, ROUTE_LIFETIME);
        verifyRaEvent(new RaEvent(ROUTER_LIFETIME, -1, -1, ROUTE_LIFETIME, -1, -1));

        // Check that RIOs differing only in the first 4 bytes are different.
        ByteBuffer similarRouteInfoOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_OPTION_LEN + IPV6_ADDR_LEN]);
        basePacket.clear();
        similarRouteInfoOptionPacket.put(basePacket);
        addRioOption(similarRouteInfoOptionPacket, ROUTE_LIFETIME, "64:ff9b::/64");
        // Packet should be passed because it is different.
        program = ipClientCallback.getApfProgram();
        assertPass(program, similarRouteInfoOptionPacket.array());

        ByteBuffer dnsslOptionPacket = ByteBuffer.wrap(
                new byte[ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_OPTION_LEN]);
        basePacket.clear();
        dnsslOptionPacket.put(basePacket);
        dnsslOptionPacket.put((byte)ICMP6_DNSSL_OPTION_TYPE);
        dnsslOptionPacket.put((byte)(ICMP6_4_BYTE_OPTION_LEN / 8));
        dnsslOptionPacket.putInt(
                ICMP6_RA_OPTION_OFFSET + ICMP6_4_BYTE_LIFETIME_OFFSET, DNSSL_LIFETIME);
        verifyRaLifetime(apfFilter, ipClientCallback, dnsslOptionPacket, ROUTER_LIFETIME);
        verifyRaEvent(new RaEvent(ROUTER_LIFETIME, -1, -1, -1, -1, DNSSL_LIFETIME));

        ByteBuffer largeRaPacket = ByteBuffer.wrap(buildLargeRa());
        verifyRaLifetime(apfFilter, ipClientCallback, largeRaPacket, 300);
        verifyRaEvent(new RaEvent(1800, 600, 300, 1200, 7200, -1));

        // Verify that current program filters all the RAs (note: ApfFilter.MAX_RAS == 10).
        program = ipClientCallback.getApfProgram();
        verifyRaLifetime(program, basePacket, ROUTER_LIFETIME);
        verifyRaLifetime(program, newFlowLabelPacket, ROUTER_LIFETIME);
        verifyRaLifetime(program, prefixOptionPacket, PREFIX_PREFERRED_LIFETIME);
        verifyRaLifetime(program, rdnssOptionPacket, RDNSS_LIFETIME);
        verifyRaLifetime(program, lowLifetimeRdnssOptionPacket, ROUTER_LIFETIME);
        verifyRaLifetime(program, routeInfoOptionPacket, ROUTE_LIFETIME);
        verifyRaLifetime(program, dnsslOptionPacket, ROUTER_LIFETIME);
        verifyRaLifetime(program, largeRaPacket, 300);

        apfFilter.shutdown();
    }

    @Test
    public void testRaWithDifferentReachableTimeAndRetransTimer() throws Exception {
        final MockIpClientCallback ipClientCallback = new MockIpClientCallback();
        final ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        final TestApfFilter apfFilter = new TestApfFilter(mContext, config, ipClientCallback, mLog);
        byte[] program = ipClientCallback.getApfProgram();
        final int RA_REACHABLE_TIME = 1800;
        final int RA_RETRANSMISSION_TIMER = 1234;

        // Create an Ra packet without options
        // Reachable time = 1800, retransmission timer = 1234
        ByteBuffer raPacket = makeBaseRaPacket();
        raPacket.position(ICMP6_RA_REACHABLE_TIME_OFFSET);
        raPacket.putInt(RA_REACHABLE_TIME);
        raPacket.putInt(RA_RETRANSMISSION_TIMER);
        // First RA passes filter
        assertPass(program, raPacket.array());

        // Assume apf is shown the given RA, it generates program to filter it.
        ipClientCallback.resetApfProgramWait();
        apfFilter.pretendPacketReceived(raPacket.array());
        program = ipClientCallback.getApfProgram();
        assertDrop(program, raPacket.array());

        // A packet with different reachable time should be passed.
        // Reachable time = 2300, retransmission timer = 1234
        raPacket.clear();
        raPacket.putInt(ICMP6_RA_REACHABLE_TIME_OFFSET, RA_REACHABLE_TIME + 500);
        assertPass(program, raPacket.array());

        // A packet with different retransmission timer should be passed.
        // Reachable time = 1800, retransmission timer = 2234
        raPacket.clear();
        raPacket.putInt(ICMP6_RA_REACHABLE_TIME_OFFSET, RA_REACHABLE_TIME);
        raPacket.putInt(ICMP6_RA_RETRANSMISSION_TIMER_OFFSET, RA_RETRANSMISSION_TIMER + 1000);
        assertPass(program, raPacket.array());
    }

    /**
     * Stage a file for testing, i.e. make it native accessible. Given a resource ID,
     * copy that resource into the app's data directory and return the path to it.
     */
    private String stageFile(int rawId) throws Exception {
        File file = new File(InstrumentationRegistry.getContext().getFilesDir(), "staged_file");
        new File(file.getParent()).mkdirs();
        InputStream in = null;
        OutputStream out = null;
        try {
            in = InstrumentationRegistry.getContext().getResources().openRawResource(rawId);
            out = new FileOutputStream(file);
            Streams.copy(in, out);
        } finally {
            if (in != null) in.close();
            if (out != null) out.close();
        }
        return file.getAbsolutePath();
    }

    private static void put(ByteBuffer buffer, int position, byte[] bytes) {
        final int original = buffer.position();
        buffer.position(position);
        buffer.put(bytes);
        buffer.position(original);
    }

    @Test
    public void testRaParsing() throws Exception {
        final int maxRandomPacketSize = 512;
        final Random r = new Random();
        MockIpClientCallback cb = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, cb, mLog);
        for (int i = 0; i < 1000; i++) {
            byte[] packet = new byte[r.nextInt(maxRandomPacketSize + 1)];
            r.nextBytes(packet);
            try {
                apfFilter.new Ra(packet, packet.length);
            } catch (ApfFilter.InvalidRaException e) {
            } catch (Exception e) {
                throw new Exception("bad packet: " + HexDump.toHexString(packet), e);
            }
        }
    }

    @Test
    public void testRaProcessing() throws Exception {
        final int maxRandomPacketSize = 512;
        final Random r = new Random();
        MockIpClientCallback cb = new MockIpClientCallback();
        ApfConfiguration config = getDefaultConfig();
        config.multicastFilter = DROP_MULTICAST;
        config.ieee802_3Filter = DROP_802_3_FRAMES;
        TestApfFilter apfFilter = new TestApfFilter(mContext, config, cb, mLog);
        for (int i = 0; i < 1000; i++) {
            byte[] packet = new byte[r.nextInt(maxRandomPacketSize + 1)];
            r.nextBytes(packet);
            try {
                apfFilter.processRa(packet, packet.length);
            } catch (Exception e) {
                throw new Exception("bad packet: " + HexDump.toHexString(packet), e);
            }
        }
    }

    /**
     * Call the APF interpreter to run {@code program} on {@code packet} with persistent memory
     * segment {@data} pretending the filter was installed {@code filter_age} seconds ago.
     */
    private native static int apfSimulate(byte[] program, byte[] packet, byte[] data,
        int filter_age);

    /**
     * Compile a tcpdump human-readable filter (e.g. "icmp" or "tcp port 54") into a BPF
     * prorgam and return a human-readable dump of the BPF program identical to "tcpdump -d".
     */
    private native static String compileToBpf(String filter);

    /**
     * Open packet capture file {@code pcap_filename} and filter the packets using tcpdump
     * human-readable filter (e.g. "icmp" or "tcp port 54") compiled to a BPF program and
     * at the same time using APF program {@code apf_program}.  Return {@code true} if
     * both APF and BPF programs filter out exactly the same packets.
     */
    private native static boolean compareBpfApf(String filter, String pcap_filename,
            byte[] apf_program);


    /**
     * Open packet capture file {@code pcapFilename} and run it through APF filter. Then
     * checks whether all the packets are dropped and populates data[] {@code data} with
     * the APF counters.
     */
    private native static boolean dropsAllPackets(byte[] program, byte[] data, String pcapFilename);

    @Test
    public void testBroadcastAddress() throws Exception {
        assertEqualsIp("255.255.255.255", ApfFilter.ipv4BroadcastAddress(IPV4_ANY_HOST_ADDR, 0));
        assertEqualsIp("0.0.0.0", ApfFilter.ipv4BroadcastAddress(IPV4_ANY_HOST_ADDR, 32));
        assertEqualsIp("0.0.3.255", ApfFilter.ipv4BroadcastAddress(IPV4_ANY_HOST_ADDR, 22));
        assertEqualsIp("0.255.255.255", ApfFilter.ipv4BroadcastAddress(IPV4_ANY_HOST_ADDR, 8));

        assertEqualsIp("255.255.255.255", ApfFilter.ipv4BroadcastAddress(MOCK_IPV4_ADDR, 0));
        assertEqualsIp("10.0.0.1", ApfFilter.ipv4BroadcastAddress(MOCK_IPV4_ADDR, 32));
        assertEqualsIp("10.0.0.255", ApfFilter.ipv4BroadcastAddress(MOCK_IPV4_ADDR, 24));
        assertEqualsIp("10.0.255.255", ApfFilter.ipv4BroadcastAddress(MOCK_IPV4_ADDR, 16));
    }

    public void assertEqualsIp(String expected, int got) throws Exception {
        int want = Inet4AddressUtils.inet4AddressToIntHTH(
                (Inet4Address) InetAddresses.parseNumericAddress(expected));
        assertEquals(want, got);
    }
}
