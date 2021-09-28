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

package com.android.internal.telephony;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.content.Intent;
import android.os.Message;
import android.provider.Telephony;
import android.test.suitebuilder.annotation.SmallTest;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

@RunWith(AndroidTestingRunner.class)
@TestableLooper.RunWithLooper
public class SmsStorageMonitorTest extends TelephonyTest {

    private SmsStorageMonitor mSmsStorageMonitor;

    @Before
    public void setUp() throws Exception {
        super.setUp(getClass().getSimpleName());
        mSmsStorageMonitor = new SmsStorageMonitor(mPhone);
        processAllMessages();
    }

    @After
    public void tearDown() throws Exception {
        mSmsStorageMonitor = null;
        super.tearDown();
    }

    @Test @SmallTest
    public void testEventIccFull() {
        // Notify icc sms full
        mSimulatedCommands.notifyIccSmsFull();
        processAllMessages();

        // SIM_FULL_ACTION intent should be broadcast
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContextFixture.getTestDouble()).sendBroadcast(intentArgumentCaptor.capture());
        assertEquals(Telephony.Sms.Intents.SIM_FULL_ACTION,
                intentArgumentCaptor.getValue().getAction());
    }

    @Test @SmallTest
    public void testSmsMemoryStatus() {
        // Notify radio on
        mSimulatedCommands.notifyRadioOn();
        processAllMessages();

        verify(mSimulatedCommandsVerifier, never()).reportSmsMemoryStatus(anyBoolean(),
                any(Message.class));

        // Send DEVICE_STORAGE_FULL
        mContextFixture.getTestDouble().sendBroadcast(
                new Intent(Intent.ACTION_DEVICE_STORAGE_FULL));
        processAllMessages();

        verify(mSimulatedCommandsVerifier).reportSmsMemoryStatus(eq(false), any(Message.class));
        assertFalse(mSmsStorageMonitor.isStorageAvailable());

        mSimulatedCommands.notifyRadioOn();
        processAllMessages();

        verify(mSimulatedCommandsVerifier).reportSmsMemoryStatus(eq(false), any(Message.class));

        // Send DEVICE_STORAGE_NOT_FULL
        mContextFixture.getTestDouble().sendBroadcast(
                new Intent(Intent.ACTION_DEVICE_STORAGE_NOT_FULL));
        processAllMessages();

        verify(mSimulatedCommandsVerifier).reportSmsMemoryStatus(eq(true), any(Message.class));
        assertTrue(mSmsStorageMonitor.isStorageAvailable());

        mSimulatedCommands.notifyRadioOn();
        processAllMessages();

        verify(mSimulatedCommandsVerifier).reportSmsMemoryStatus(eq(true), any(Message.class));
    }
}