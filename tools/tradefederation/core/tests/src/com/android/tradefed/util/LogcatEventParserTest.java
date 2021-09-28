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

package com.android.tradefed.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.IShellOutputReceiver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.TestDeviceState;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link LogcatEventParser}. */
@RunWith(JUnit4.class)
public class LogcatEventParserTest {

    private static final String[] LOGS_UPDATE_COMPLETE = {
        "11-11 00:00:00.001  123 321 I update_engine: foo bar baz",
        "11-11 00:00:00.001  123 321 I update_engine: foo bar baz",
        "11-11 00:00:00.001  123 321 I update_engine: foo bar baz",
        "11-11 00:00:00.001  123 321 I update_engine: foo bar baz",
        "11-11 00:00:00.001  123 321 I update_engine: Update successfully applied",
    };

    private static final long EVENT_TIMEOUT_MS = 5 * 1000L;

    private LogcatEventParser mParser;
    private ITestDevice mTestDevice;
    private IDevice mNativeDevice;
    private Capture<IShellOutputReceiver> mShellOutputReceiverCapture;

    @Before
    public void setUp() {
        mTestDevice = EasyMock.createNiceMock(ITestDevice.class);

        mParser = new LogcatEventParser(mTestDevice);
        mParser.registerEventTrigger(
                "update_engine", "Unexpected event case 1", LogcatEventType.INFRA_TIMEOUT);
        mParser.registerEventTrigger(
                "update_engine_client", "Unexpected event case 2", LogcatEventType.INFRA_TIMEOUT);
    }

    @After
    public void tearDown() {
        mParser.close();
    }

    /**
     * Test that a known event parses to the expected {@link LogcatEventType} when there is an exact
     * match.
     */
    @Test
    public void testParseEventTypeExactMatch() {
        mParser.registerEventTrigger(
                "update_verifier",
                "Leaving update_verifier.",
                LogcatEventType.UPDATE_VERIFIER_COMPLETE);
        String mappedLogLine =
                "11-11 00:00:00.001  123 321 I update_verifier: Leaving update_verifier.";
        mParser.parseEvents(new String[] {mappedLogLine});
        assertEquals(
                LogcatEventType.UPDATE_VERIFIER_COMPLETE, mParser.pollForEvent().getEventType());
    }

    /**
     * Test that a known event parses to the expected {@link LogcatEventType} when the registered
     * message is a substring of the log message.
     */
    @Test
    public void testParseEventTypePartialMatch() {
        mParser.registerEventTrigger(
                "update_engine_client",
                "onPayloadApplicationComplete(ErrorCode",
                LogcatEventType.ERROR);
        String log =
                "01-09 17:06:50.799  8688  8688 I update_engine_client: "
                        + "onPayloadApplicationComplete(ErrorCode::kUserCanceled (48))";
        mParser.parseEvents(new String[] {log});
        assertEquals(LogcatEventType.ERROR, mParser.pollForEvent().getEventType());
    }

    /** Test that unknown events parse to null. */
    @Test
    public void testParseEventTypeUnknown() {
        String unmappedLogLine = "11-11 00:00:00.001  123 321 I update_engine: foo bar baz";
        mParser.parseEvents(new String[] {unmappedLogLine});
        assertNull(mParser.pollForEvent());
        unmappedLogLine = "11-11 00:00:00.001  123 321 I foobar_engine: Update succeeded";
        mParser.parseEvents(new String[] {unmappedLogLine});
        assertNull(mParser.pollForEvent());
        unmappedLogLine = "11-11 00:00:00.001  123 321 I foobar_engine: foo bar baz";
        mParser.parseEvents(new String[] {unmappedLogLine});
        assertNull(mParser.pollForEvent());
    }

    /** Test that events registered first are matched first */
    @Test
    public void testParseEventTypeMatchOrder() {
        mParser.registerEventTrigger(
                "update_engine",
                "finished with ErrorCode::kSuccess",
                LogcatEventType.PATCH_COMPLETE);
        mParser.registerEventTrigger(
                "update_engine", "finished with ErrorCode", LogcatEventType.ERROR);
        String notError =
                "11-11 00:00:00.001  123 321 I update_engine: finished with ErrorCode::kSuccess";
        mParser.parseEvents(new String[] {notError});
        assertEquals(LogcatEventType.PATCH_COMPLETE, mParser.pollForEvent().getEventType());
    }

    /** Test that waitForEvent returns. */
    @Test
    public void testWaitForEvent() throws InterruptedException {
        mParser.registerEventTrigger(
                "update_engine", "Update successfully applied", LogcatEventType.UPDATE_COMPLETE);
        mParser.parseEvents(LOGS_UPDATE_COMPLETE);
        LogcatEventParser.LogcatEvent result = mParser.waitForEvent(EVENT_TIMEOUT_MS);
        assertEquals(LogcatEventType.UPDATE_COMPLETE, result.getEventType());
    }

    /** Test that waitForEvent honors the timeout. */
    @Test
    public void testWaitForEventTimeout() throws InterruptedException {
        String unmappedLogLine = "11-11 00:00:00.001  123 321 I update_engine: foo bar baz";
        mParser.parseEvents(new String[] {unmappedLogLine});
        LogcatEventParser.LogcatEvent result = mParser.waitForEvent(0);
        assertNull(result);
    }

    /** Test end to end that waitForEvent returns. */
    @Test
    public void testEndToEnd() throws InterruptedException {
        mParser.registerEventTrigger(
                "update_engine", "Update successfully applied", LogcatEventType.UPDATE_COMPLETE);

        EasyMock.expect(mTestDevice.getDeviceState()).andReturn(TestDeviceState.ONLINE);
        mNativeDevice = EasyMock.createNiceMock(IDevice.class);
        EasyMock.expect(mTestDevice.getIDevice()).andStubReturn(mNativeDevice);
        mShellOutputReceiverCapture = new Capture<>();
        try {
            mNativeDevice.executeShellCommand(
                    EasyMock.isA(String.class),
                    EasyMock.capture(mShellOutputReceiverCapture),
                    EasyMock.anyInt(),
                    EasyMock.isA(TimeUnit.class));
            EasyMock.expectLastCall();
        } catch (Exception e) {
            fail(e.toString());
        }

        EasyMock.replay(mTestDevice, mNativeDevice);
        mParser.start();
        // Allow the BackgroundDeviceAction to start
        new RunUtil().sleep(1000);
        EasyMock.verify(mTestDevice, mNativeDevice);

        IShellOutputReceiver receiver = mShellOutputReceiverCapture.getValue();
        String output = String.join("\n", LOGS_UPDATE_COMPLETE) + "\n";
        receiver.addOutput(output.getBytes(), 0, output.getBytes().length);
        receiver.flush();

        LogcatEventParser.LogcatEvent result = mParser.waitForEvent(EVENT_TIMEOUT_MS);
        assertEquals(LogcatEventType.UPDATE_COMPLETE, result.getEventType());
    }
}
