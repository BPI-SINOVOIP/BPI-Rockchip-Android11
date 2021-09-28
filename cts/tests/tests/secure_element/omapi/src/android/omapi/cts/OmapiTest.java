/*
 * Copyright (C) 2018 The Android Open Source Project
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

/* Contributed by Orange */

package android.omapi.cts;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.*;
import static org.junit.Assume.assumeTrue;

import android.content.pm.PackageManager;
import android.os.Build;
import android.os.SystemProperties;
import android.se.omapi.Channel;
import android.se.omapi.Reader;
import android.se.omapi.SEService;
import android.se.omapi.SEService.OnConnectedListener;
import android.se.omapi.Session;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.PropertyUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.NoSuchElementException;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeoutException;

public class OmapiTest {

    private final static String UICC_READER_PREFIX = "SIM";
    private final static String ESE_READER_PREFIX = "eSE";
    private final static String SD_READER_PREFIX = "SD";
    private final static byte[] SELECTABLE_AID =
            new byte[]{(byte) 0xA0, 0x00, 0x00, 0x04, 0x76, 0x41, 0x6E, 0x64,
                    0x72, 0x6F, 0x69, 0x64, 0x43, 0x54, 0x53, 0x31};
    private final static byte[] LONG_SELECT_RESPONSE_AID =
            new byte[]{(byte) 0xA0, 0x00, 0x00, 0x04, 0x76, 0x41, 0x6E, 0x64,
                    0x72, 0x6F, 0x69, 0x64, 0x43, 0x54, 0x53, 0x32};
    private final static byte[] NON_SELECTABLE_AID =
            new byte[]{(byte) 0xA0, 0x00, 0x00, 0x04, 0x76, 0x41, 0x6E, 0x64,
                    0x72, 0x6F, 0x69, 0x64, 0x43, 0x54, 0x53, (byte) 0xFF};

    /* MANAGE open/close and SELECT AID */
    private final static byte[][] ILLEGAL_COMMANDS_TRANSMIT = new byte[][]{{0x00, 0x70, 0x00, 0x00},
            {0x00, 0x70, (byte) 0x80, 0x00},
            {0x00, (byte) 0xA4, 0x04, 0x04, 0x10, 0x4A, 0x53,
                    0x52, 0x31, 0x37, 0x37, 0x54, 0x65, 0x73,
                    0x74, 0x65, 0x72, 0x20, 0x31, 0x2E, 0x30}
    };

    /* OMAPI APDU Test case 1 and 3 */
    private final static byte[][] NO_DATA_APDU = new byte[][]{{0x00, 0x06, 0x00, 0x00},
            {(byte) 0x80, 0x06, 0x00, 0x00},
            {(byte) 0xA0, 0x06, 0x00, 0x00},
            {(byte) 0x94, 0x06, 0x00, 0x00},
            {0x00, 0x0A, 0x00, 0x00, 0x01, (byte) 0xAA},
            {(byte) 0x80, 0x0A, 0x00, 0x00, 0x01, (byte) 0xAA},
            {(byte) 0xA0, 0x0A, 0x00, 0x00, 0x01, (byte) 0xAA},
            {(byte) 0x94, 0x0A, 0x00, 0x00, 0x01, (byte) 0xAA}
    };
    /* OMAPI APDU Test case 2 and 4 */
    private final static byte[][] DATA_APDU = new byte[][]{{0x00, 0x08, 0x00, 0x00, 0x00},
            {(byte) 0x80, 0x08, 0x00, 0x00, 0x00},
            {(byte) 0xA0, 0x08, 0x00, 0x00, 0x00},
            {(byte) 0x94, 0x08, 0x00, 0x00, 0x00},
            {0x00, (byte) 0x0C, 0x00, 0x00, 0x01, (byte) 0xAA, 0x00},
            {(byte) 0x80, (byte) 0x0C, 0x00, 0x00, 0x01, (byte) 0xAA, 0x00},
            {(byte) 0xA0, (byte) 0x0C, 0x00, 0x00, 0x01, (byte) 0xAA, 0x00},
            {(byte) 0x94, (byte) 0x0C, 0x00, 0x00, 0x01, (byte) 0xAA, 0x00}
    };

    /* Case 2 APDU command expects the P2 received in the SELECT command as 1-byte outgoing data */
    private static final byte[] CHECK_SELECT_P2_APDU = new byte[] {
            0x00, (byte) 0xF4, 0x00, 0x00, 0x00
    };

    /* OMAPI APDU Test case 1 and 3 */
    private final static byte[][] SW_62xx_NO_DATA_APDU =
            new byte[][]{{0x00, (byte) 0xF3, 0x00, 0x06},
                    {0x00, (byte) 0xF3, 0x00, 0x0A, 0x01, (byte) 0xAA}
            };
    /* OMAPI APDU Test case 2 and 4 */
    private final static byte[] SW_62xx_DATA_APDU = new byte[]{0x00, (byte) 0xF3, 0x00, 0x08, 0x00};
    private final static byte[] SW_62xx_VALIDATE_DATA_APDU =
            new byte[]{0x00, (byte) 0xF3, 0x00, 0x0C, 0x01, (byte) 0xAA, 0x00};
    private final static byte[][] SW_62xx =
            new byte[][]{{0x62, 0x00}, {0x62, (byte) 0x81}, {0x62, (byte) 0x82},
                    {0x62, (byte) 0x83},
                    {0x62, (byte) 0x85}, {0x62, (byte) 0xF1}, {0x62, (byte) 0xF2},
                    {0x63, (byte) 0xF1},
                    {0x63, (byte) 0xF2}, {0x63, (byte) 0xC2}, {0x62, 0x02}, {0x62, (byte) 0x80},
                    {0x62, (byte) 0x84}, {0x62, (byte) 0x86}, {0x63, 0x00}, {0x63, (byte) 0x81}
            };
    private final static byte[][] SEGMENTED_RESP_APDU = new byte[][]{
            //Get response Case2 61FF+61XX with answer length (P1P2) of 0x0800, 2048 bytes
            {0x00, (byte) 0xC2, 0x08, 0x00, 0x00},
            //Get response Case4 61FF+61XX with answer length (P1P2) of 0x0800, 2048 bytes
            {0x00, (byte) 0xC4, 0x08, 0x00, 0x02, 0x12, 0x34, 0x00},
            //Get response Case2 6100+61XX with answer length (P1P2) of 0x0800, 2048 bytes
            {0x00, (byte) 0xC6, 0x08, 0x00, 0x00},
            //Get response Case4 6100+61XX with answer length (P1P2) of 0x0800, 2048 bytes
            {0x00, (byte) 0xC8, 0x08, 0x00, 0x02, 0x12, 0x34, 0x00},
            //Test device buffer capacity 7FFF data
            {0x00, (byte) 0xC2, (byte) 0x7F, (byte) 0xFF, 0x00},
            //Get response 6CFF+61XX with answer length (P1P2) of 0x0800, 2048 bytes
            {0x00, (byte) 0xCF, 0x08, 0x00, 0x00},
            //Get response with another CLA  with answer length (P1P2) of 0x0800, 2048 bytes
            {(byte) 0x94, (byte) 0xC2, 0x08, 0x00, 0x00}
    };
    private final long SERVICE_CONNECTION_TIME_OUT = 3000;
    private SEService seService;
    private Object serviceMutex = new Object();
    private Timer connectionTimer;
    private ServiceConnectionTimerTask mTimerTask = new ServiceConnectionTimerTask();
    private boolean connected = false;
    private final OnConnectedListener mListener = new OnConnectedListener() {
                @Override
                public void onConnected() {
                    synchronized (serviceMutex) {
                        connected = true;
                        serviceMutex.notify();
                    }
                }
            };

    class SynchronousExecutor implements Executor {
        public void execute(Runnable r) {
            r.run();
        }
    }

    private boolean supportsHardware() {
        final PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        boolean lowRamDevice = SystemProperties.getBoolean("ro.config.low_ram", false);
        return !lowRamDevice || pm.hasSystemFeature("android.hardware.type.watch")
                || hasSecureElementPackage(pm);
    }

    private boolean hasSecureElementPackage(PackageManager pm) {
        try {
            pm.getPackageInfo("com.android.se", 0 /* flags*/);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private boolean supportUICCReaders() {
        final PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_SE_OMAPI_UICC);
    }

    private boolean supportESEReaders() {
        final PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_SE_OMAPI_ESE);
    }

    private boolean supportSDReaders() {
        final PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_SE_OMAPI_SD);
    }

    private boolean supportOMAPIReaders() {
        final PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        return (pm.hasSystemFeature(PackageManager.FEATURE_SE_OMAPI_UICC)
            || pm.hasSystemFeature(PackageManager.FEATURE_SE_OMAPI_ESE)
            || pm.hasSystemFeature(PackageManager.FEATURE_SE_OMAPI_SD));
    }


    private void assertGreaterOrEqual(long greater, long lesser) {
        assertTrue("" + greater + " expected to be greater than or equal to " + lesser,
                greater >= lesser);
    }

    @Before
    public void setUp() throws Exception {
        assumeTrue(PropertyUtil.getFirstApiLevel() > Build.VERSION_CODES.O_MR1);
        assumeTrue(supportsHardware());
        seService = new SEService(InstrumentationRegistry.getContext(), new SynchronousExecutor(), mListener);
        connectionTimer = new Timer();
        connectionTimer.schedule(mTimerTask, SERVICE_CONNECTION_TIME_OUT);
    }

    @After
    public void tearDown() throws Exception {
        if (seService != null && seService.isConnected()) {
            seService.shutdown();
            connected = false;
        }
    }

    private void waitForConnection() throws TimeoutException {
        synchronized (serviceMutex) {
            if (!connected) {
                try {
                    serviceMutex.wait();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
            if (!connected) {
                throw new TimeoutException(
                        "Service could not be connected after " + SERVICE_CONNECTION_TIME_OUT
                                + " ms");
            }
            if (connectionTimer != null) {
                connectionTimer.cancel();
            }
        }
    }

    /** Tests getReaders API */
    @Test
    public void testGetReaders() {
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();
            ArrayList<Reader> uiccReaders = new ArrayList<Reader>();
            ArrayList<Reader> eseReaders = new ArrayList<Reader>();
            ArrayList<Reader> sdReaders = new ArrayList<Reader>();

            for (Reader reader : readers) {
                assertTrue(reader.isSecureElementPresent());
                String name = reader.getName();
                if (!(name.startsWith(UICC_READER_PREFIX) || name.startsWith(ESE_READER_PREFIX)
                        || name.startsWith(SD_READER_PREFIX))) {
                    fail("Incorrect Reader name");
                }
                assertNotNull("getseService returned null", reader.getSEService());

                if (reader.getName().startsWith(UICC_READER_PREFIX)) {
                    uiccReaders.add(reader);
                }
                if (reader.getName().startsWith(ESE_READER_PREFIX)) {
                    eseReaders.add(reader);
                }
                if (reader.getName().startsWith(SD_READER_PREFIX)) {
                    sdReaders.add(reader);
                }
            }

            if (supportUICCReaders()) {
                assertGreaterOrEqual(uiccReaders.size(), 1);
                // Test API getUiccReader(int slotNumber)
                // The result should be the same as getReaders() with UICC reader prefix
                for (int i = 1; i <= uiccReaders.size(); i++) {
                    try {
                        Reader uiccReader = seService.getUiccReader(i);
                        if (!uiccReaders.contains(uiccReader))
                            fail("Incorrect reader object - getUiccReader(" + i + ")");
                    } catch (IllegalArgumentException e) {
                        fail("Fail to get Reader object by calling getUiccReader(" + i + ")");
                    }
                }
            } else {
                assertTrue(uiccReaders.size() == 0);
            }

            if (supportESEReaders()) {
                assertGreaterOrEqual(eseReaders.size(), 1);
            } else {
                assertTrue(eseReaders.size() == 0);
            }

            if (supportSDReaders()) {
                assertGreaterOrEqual(eseReaders.size(), 1);
            } else {
                assertTrue(sdReaders.size() == 0);
            }
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

    /** Tests getATR API */
    @Test
    public void testATR() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();
            ArrayList<Reader> uiccReaders = new ArrayList<Reader>();
            if (readers != null && readers.length > 0) {
                for (int i = 0; i < readers.length; i++) {
                    if (readers[i].getName().startsWith(UICC_READER_PREFIX)) {
                        uiccReaders.add(readers[i]);
                    }
                }

                for (Reader reader : uiccReaders) {
                    Session session = null;
                    try {
                        session = reader.openSession();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                    assertNotNull("Could not open session", session);
                    byte[] atr = session.getATR();
                    session.close();
                    assertNotNull("ATR is Null", atr);
                }
            }
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

    /** Tests OpenBasicChannel API when aid is null */
    @Test
    public void testOpenBasicChannelNullAid() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                Session session = null;
                Channel channel = null;
                try {
                    session = reader.openSession();
                    assertNotNull("Could not open session", session);
                    channel = session.openBasicChannel(null, (byte) 0x00);
                } finally {
                    if (channel != null) channel.close();
                    if (session != null) session.close();
                }
                if (reader.getName().startsWith(UICC_READER_PREFIX)) {
                    assertNull("Basic channel on UICC can be opened", channel);
                } else {
                    assertNotNull("Basic Channel cannot be opened", channel);
                }
            }
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

    /** Tests OpenBasicChannel API when aid is provided */
    @Test
    public void testOpenBasicChannelNonNullAid() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                Session session = null;
                Channel channel = null;
                try {
                    session = reader.openSession();
                    assertNotNull("Could not open session", session);
                    channel = session.openBasicChannel(SELECTABLE_AID, (byte) 0x00);
                } finally {
                    if (channel != null) channel.close();
                    if (session != null) session.close();
                }
                if (reader.getName().startsWith(UICC_READER_PREFIX)) {
                    assertNull("Basic channel on UICC can be opened", channel);
                } else {
                    assertNotNull("Basic Channel cannot be opened", channel);
                }
            }
        } catch (Exception e) {
            fail("Unexpected Exception " + e);
        }
    }

    /** Tests Select API */
    @Test
    public void testSelectableAid() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();
            for (Reader reader : readers) {
                testSelectableAid(reader, SELECTABLE_AID);
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    @Test
    public void testLongSelectResponse() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();
            for (Reader reader : readers) {
                byte[] selectResponse = testSelectableAid(reader, LONG_SELECT_RESPONSE_AID);
                assertTrue("Select Response is not complete", verifyBerTlvData(selectResponse));
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    private byte[] testSelectableAid(Reader reader, byte[] aid) throws IOException {
        byte[] selectResponse = null;
        Session session = null;
        Channel channel = null;
        try {
            assertTrue(reader.isSecureElementPresent());
            session = reader.openSession();
            assertNotNull("Null Session", session);
            channel = session.openLogicalChannel(aid, (byte) 0x00);
            assertNotNull("Null Channel", channel);
            selectResponse = channel.getSelectResponse();
        } finally {
            if (channel != null) channel.close();
            if (session != null) session.close();
        }
        assertNotNull("Null Select Response", selectResponse);
        assertGreaterOrEqual(selectResponse.length, 2);
        assertThat(selectResponse[selectResponse.length - 1] & 0xFF, is(0x00));
        assertThat(selectResponse[selectResponse.length - 2] & 0xFF, is(0x90));
        return selectResponse;
    }

    /** Tests if NoSuchElementException in Select */
    @Test
    public void testWrongAid() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();
            for (Reader reader : readers) {
                testNonSelectableAid(reader, NON_SELECTABLE_AID);
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    private void testNonSelectableAid(Reader reader, byte[] aid) throws IOException {
        Session session = null;
        Channel channel = null;
        try {
            assertTrue(reader.isSecureElementPresent());
            session = reader.openSession();
            assertNotNull("null session", session);
            channel = session.openLogicalChannel(aid, (byte) 0x00);
            fail("Exception expected for this test");
        } catch (NoSuchElementException e) {
            // Catch the expected exception here.
        } finally {
            if (channel != null) channel.close();
            if (session != null) session.close();
        }
    }

    /** Tests if Security Exception in Transmit */
    @Test
    public void testSecurityExceptionInTransmit() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                Session session = null;
                Channel channel = null;
                try {
                    assertTrue(reader.isSecureElementPresent());
                    session = reader.openSession();
                    assertNotNull("null session", session);
                    channel = session.openLogicalChannel(SELECTABLE_AID, (byte) 0x00);
                    assertNotNull("Null Channel", channel);
                    byte[] selectResponse = channel.getSelectResponse();
                    assertNotNull("Null Select Response", selectResponse);
                    assertGreaterOrEqual(selectResponse.length, 2);
                    assertThat(selectResponse[selectResponse.length - 1] & 0xFF, is(0x00));
                    assertThat(selectResponse[selectResponse.length - 2] & 0xFF, is(0x90));
                    for (byte[] cmd : ILLEGAL_COMMANDS_TRANSMIT) {
                        try {
                            byte[] response = channel.transmit(cmd);
                            fail("Exception expected for this test");
                        } catch (SecurityException e) {
                            // Catch the expected exception here.
                        }
                    }
                } finally {
                    if (channel != null) channel.close();
                    if (session != null) session.close();
                }
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    private byte[] internalTransmitApdu(Reader reader, byte[] apdu) throws IOException {
        byte[] transmitResponse = null;
        Session session = null;
        Channel channel = null;
        try {
            assertTrue(reader.isSecureElementPresent());
            session = reader.openSession();
            assertNotNull("null session", session);
            channel = session.openLogicalChannel(SELECTABLE_AID, (byte) 0x00);
            assertNotNull("Null Channel", channel);
            byte[] selectResponse = channel.getSelectResponse();
            assertNotNull("Null Select Response", selectResponse);
            assertGreaterOrEqual(selectResponse.length, 2);
            transmitResponse = channel.transmit(apdu);
        } finally {
            if (channel != null) channel.close();
            if (session != null) session.close();
        }
        return transmitResponse;
    }

    private byte[] internalTransmitApduWithoutP2(Reader reader, byte[] apdu) throws IOException {
        byte[] transmitResponse = null;
        Session session = null;
        Channel channel = null;
        try {
            assertTrue(reader.isSecureElementPresent());
            session = reader.openSession();
            assertNotNull("null session", session);
            channel = session.openLogicalChannel(SELECTABLE_AID);
            assertNotNull("Null Channel", channel);
            byte[] selectResponse = channel.getSelectResponse();
            assertNotNull("Null Select Response", selectResponse);
            assertGreaterOrEqual(selectResponse.length, 2);
            transmitResponse = channel.transmit(apdu);
        } finally {
            if (channel != null) channel.close();
            if (session != null) session.close();
        }
        return transmitResponse;
    }

    /**
     * Tests Transmit API for all readers.
     *
     * Checks the return status and verifies the size of the
     * response.
     */
    @Test
    public void testTransmitApdu() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                for (byte[] apdu : NO_DATA_APDU) {
                    byte[] response = internalTransmitApdu(reader, apdu);
                    assertThat(response.length, is(2));
                    assertThat(response[response.length - 1] & 0xFF, is(0x00));
                    assertThat(response[response.length - 2] & 0xFF, is(0x90));
                }

                for (byte[] apdu : DATA_APDU) {
                    byte[] response = internalTransmitApdu(reader, apdu);
                    /* 256 byte data and 2 bytes of status word */
                    assertThat(response.length, is(258));
                    assertThat(response[response.length - 1] & 0xFF, is(0x00));
                    assertThat(response[response.length - 2] & 0xFF, is(0x90));
                }
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    /**
     * Tests if underlying implementations returns the correct Status Word
     *
     * TO verify that :
     * - the device does not modify the APDU sent to the Secure Element
     * - the warning code is properly received by the application layer as SW answer
     * - the verify that the application layer can fetch the additionnal data (when present)
     */
    @Test
    public void testStatusWordTransmit() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                for (byte[] apdu : SW_62xx_NO_DATA_APDU) {
                    for (byte i = 0x00; i < SW_62xx.length; i++) {
                        apdu[2] = (byte)(i+1);
                        byte[] response = internalTransmitApdu(reader, apdu);
                        byte[] SW = SW_62xx[i];
                        assertThat(response[response.length - 1], is(SW[1]));
                        assertThat(response[response.length - 2], is(SW[0]));
                    }
                }

                for (byte i = 0x00; i < SW_62xx.length; i++) {
                    byte[] apdu = SW_62xx_DATA_APDU;
                    apdu[2] = (byte)(i+1);
                    byte[] response = internalTransmitApdu(reader, apdu);
                    byte[] SW = SW_62xx[i];
                    assertGreaterOrEqual(response.length, 3);
                    assertThat(response[response.length - 1], is(SW[1]));
                    assertThat(response[response.length - 2], is(SW[0]));
                }

                for (byte i = 0x00; i < SW_62xx.length; i++) {
                    byte[] apdu = SW_62xx_VALIDATE_DATA_APDU;
                    apdu[2] = (byte)(i+1);
                    byte[] response = internalTransmitApdu(reader, apdu);
                    assertGreaterOrEqual(response.length, apdu.length + 2);
                    byte[] responseSubstring = Arrays.copyOfRange(response, 0, apdu.length);
                    // We should not care about which channel number is actually assigned.
                    responseSubstring[0] = apdu[0];
                    assertTrue(Arrays.equals(responseSubstring, apdu));
                    byte[] SW = SW_62xx[i];
                    assertThat(response[response.length - 1], is(SW[1]));
                    assertThat(response[response.length - 2], is(SW[0]));
                }
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    /** Test if the responses are segmented by the underlying implementation */
    @Test
    public void testSegmentedResponseTransmit() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                for (byte[] apdu : SEGMENTED_RESP_APDU) {
                    byte[] response = internalTransmitApdu(reader, apdu);
                    byte[] b = { 0x00, 0x00, apdu[2], apdu[3] };
                    ByteBuffer wrapped = ByteBuffer.wrap(b);
                    int expectedLength = wrapped.getInt();
                    assertThat(response.length, is(expectedLength + 2));
                    assertThat(response[response.length - 1] & 0xFF, is(0x00));
                    assertThat(response[response.length - 2] & 0xFF, is(0x90));
                    assertThat(response[response.length - 3] & 0xFF, is(0xFF));
                }
            }
        } catch (Exception e) {
            fail("unexpected exception " + e);
        }
    }

    /**
     * Tests the P2 value of the select command.
     *
     * Verifies that the default P2 value (0x00) is not modified by the underlying implementation.
     */
    @Test
    public void testP2Value() {
        assumeTrue(supportOMAPIReaders());
        try {
            waitForConnection();
            Reader[] readers = seService.getReaders();

            for (Reader reader : readers) {
                byte[] response = internalTransmitApduWithoutP2(reader, CHECK_SELECT_P2_APDU);
                assertGreaterOrEqual(response.length, 3);
                assertThat(response[response.length - 1] & 0xFF, is(0x00));
                assertThat(response[response.length - 2] & 0xFF, is(0x90));
                assertThat(response[response.length - 3] & 0xFF, is(0x00));
            }
        } catch (Exception e) {
          fail("unexpected exception " + e);
        }
    }

    /**
     * Verifies TLV data
     * @param tlv
     * @return true if the data is tlv formatted, false otherwise
     */
    private static boolean verifyBerTlvData(byte[] tlv){
        if (tlv == null || tlv.length == 0) {
            throw new RuntimeException("Invalid tlv, null");
        }
        int i = 0;
        if ((tlv[i++] & 0x1F) == 0x1F) {
            // extra byte for TAG field
            i++;
        }

        int len = tlv[i++] & 0xFF;
        if (len > 127) {
            // more than 1 byte for length
            int bytesLength = len-128;
            len = 0;
            for(int j = bytesLength; j > 0; j--) {
                len += (len << 8) + (tlv[i++] & 0xFF);
            }
        }
        // Additional 2 bytes for the SW
        return (tlv.length == (i+len+2));
    }

    class ServiceConnectionTimerTask extends TimerTask {
        @Override
        public void run() {
            synchronized (serviceMutex) {
                serviceMutex.notifyAll();
            }
        }
    }
}
