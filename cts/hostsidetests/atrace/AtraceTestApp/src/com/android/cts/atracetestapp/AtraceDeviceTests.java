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

package com.android.cts.atracetestapp;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.os.Trace;
import android.system.Os;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class AtraceDeviceTests {

    @BeforeClass
    public static void reportPidTid() {
        Bundle status = new Bundle();
        status.putLong("AtraceDeviceTests_pid", Os.getpid());
        status.putLong("AtraceDeviceTests_tid", Os.gettid());
        InstrumentationRegistry.getInstrumentation().addResults(status);
    }

    @Rule
    public ActivityTestRule<AtraceTestAppActivity> mActivity =
            new ActivityTestRule<>(AtraceTestAppActivity.class, true, false);

    @Test
    public void assertTracingOn() {
        assertTrue(Trace.isEnabled());
        assertTrue(AtraceNdkMethods.isEnabled());
    }

    @Test
    public void assertTracingOff() {
        assertFalse(Trace.isEnabled());
        assertFalse(AtraceNdkMethods.isEnabled());
    }

    @Test
    public void beginEndSection() {
        assertTrue(Trace.isEnabled());
        assertTrue(AtraceNdkMethods.isEnabled());
        Trace.beginSection("AtraceDeviceTest::beginEndSection");
        Trace.endSection();
        AtraceNdkMethods.beginEndSection();
    }

    @Test
    public void asyncBeginEndSection() {
        assertTrue(Trace.isEnabled());
        assertTrue(AtraceNdkMethods.isEnabled());
        Trace.beginAsyncSection("AtraceDeviceTest::asyncBeginEndSection", 42);
        Trace.endAsyncSection("AtraceDeviceTest::asyncBeginEndSection", 42);
        AtraceNdkMethods.asyncBeginEndSection();
    }

    @Test
    public void counter() {
        assertTrue(Trace.isEnabled());
        assertTrue(AtraceNdkMethods.isEnabled());
        Trace.setCounter("AtraceDeviceTest::counter", 10);
        Trace.setCounter("AtraceDeviceTest::counter", 20);
        Trace.setCounter("AtraceDeviceTest::counter", 30);
        Trace.setCounter("AtraceDeviceTest::counter", 9223372000000005807L);
        AtraceNdkMethods.counter();
    }

    @Test
    public void launchActivity() {
        AtraceTestAppActivity activity = mActivity.launchActivity(null);
        activity.waitForDraw();
        activity.finish();
    }

    static {
        System.loadLibrary("ctstrace_jni");
    }
}
