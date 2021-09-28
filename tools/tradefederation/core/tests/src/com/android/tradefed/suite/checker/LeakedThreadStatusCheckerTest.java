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
package com.android.tradefed.suite.checker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.BackgroundDeviceAction;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link LeakedThreadStatusChecker} */
@RunWith(JUnit4.class)
public class LeakedThreadStatusCheckerTest {

    private LeakedThreadStatusChecker mChecker;
    private ThreadGroup mGroup;

    @Before
    public void setUp() {
        mChecker = new LeakedThreadStatusChecker();
        mGroup = new ThreadGroup("LeakedThreadStatusCheckerTest");
    }

    @Test
    public void testNoLeakedThread() throws Exception {
        TestInThread thread = new TestInThread();
        thread.start();
        thread.join();
        StatusCheckerResult result = thread.mResult;
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
        assertNull(result.getErrorMessage());
    }

    @Test
    public void testLeakedThread() throws Exception {
        LeakedTestThread leakThread =
                new LeakedTestThread("LeakedTestThread#LeakedThreadStatusCheckerTest");
        leakThread.start();
        TestInThread thread = new TestInThread();
        thread.start();
        thread.join();
        leakThread.mIsCancelled = true;
        try {
            StatusCheckerResult result = thread.mResult;
            assertEquals(CheckStatus.FAILED, result.getStatus());
            assertTrue(result.getErrorMessage().contains("We have 2 threads instead of 1."));
        } finally {
            leakThread.join();
        }
    }

    /** Test that background action threads are ignored. */
    @Test
    public void testLeakedThread_background() throws Exception {
        LeakedTestThread leakThread =
                new LeakedTestThread(BackgroundDeviceAction.BACKGROUND_DEVICE_ACTION + "-logcat");
        leakThread.start();
        TestInThread thread = new TestInThread();
        thread.start();
        thread.join();
        leakThread.mIsCancelled = true;
        try {
            StatusCheckerResult result = thread.mResult;
            assertEquals(CheckStatus.SUCCESS, result.getStatus());
            assertNull(result.getErrorMessage());
        } finally {
            leakThread.join();
        }
    }

    /** Run the check in a thread in its own group to avoid interference from other tests or IDE. */
    private class TestInThread extends Thread {

        public StatusCheckerResult mResult = null;

        public TestInThread() {
            super(mGroup, "TestInThread#LeakedThreadStatusCheckerTest");
        }

        @Override
        public void run() {
            try {
                mResult = mChecker.postExecutionCheck(null);
            } catch (DeviceNotAvailableException e) {
                // Ignore
            }
        }
    }

    /** A thread that runs until cancelled. */
    private class LeakedTestThread extends Thread {

        public boolean mIsCancelled = false;

        public LeakedTestThread(String name) {
            super(mGroup, name);
        }

        @Override
        public void run() {
            while (!mIsCancelled) {
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    // Ignore
                }
            }
        }
    }
}
