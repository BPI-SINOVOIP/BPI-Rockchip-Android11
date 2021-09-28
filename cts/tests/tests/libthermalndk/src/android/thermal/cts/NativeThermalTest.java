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

package android.thermal.cts;

import android.content.Context;
import android.os.PowerManager;
import android.support.test.uiautomator.UiDevice;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import com.google.common.base.Strings;

import org.junit.After;
import org.junit.Before;
import org.junit.runner.RunWith;
import org.junit.Test;

import java.io.IOException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import static org.junit.Assert.fail;

/**
 * Tests native thermal API for get current thermal status, register and unregister
 * thermal status listeners.
 */
@RunWith(AndroidJUnit4.class)
public class NativeThermalTest {
    private UiDevice mUiDevice;
    private Executor mExec = Executors.newSingleThreadExecutor();

    private native String nativeTestGetCurrentThermalStatus(int level);
    private native String nativeTestRegisterThermalStatusListener();
    private native String nativeTestThermalStatusRegisterNullListener();
    private native String nativeTestThermalStatusListenerDoubleRegistration();

    @Before
    public void setUp() throws Exception {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    @After
    public void tearDown() throws Exception {
        mUiDevice.executeShellCommand("cmd thermalservice reset");
    }

    /**
     * Helper function to set override status
     */
    public void setOverrideStatus (int level)  throws Exception {
        mUiDevice.executeShellCommand("cmd thermalservice override-status " + level);
    }

    /**
     * Confirm that we can get thermal status.
     *
     * @throws Exception
     */
    @Test
    public void testGetCurrentThermalStatus() throws Exception {
        for (int level = PowerManager.THERMAL_STATUS_NONE;
                level < PowerManager.THERMAL_STATUS_SHUTDOWN; level++) {
            final String failureMessage = nativeTestGetCurrentThermalStatus(level);
            if (!Strings.isNullOrEmpty(failureMessage)) {
                fail(failureMessage);
            }
        }
    }

    /**
     * Confirm that we can register thermal status listener and get callback.
     *
     * @throws Exception
     */
    @Test
    public void testRegisterThermalStatusListener() throws Exception {
        final String failureMessage = nativeTestRegisterThermalStatusListener();
        if (!Strings.isNullOrEmpty(failureMessage)) {
            fail(failureMessage);
        }
    }

    /**
     * Confirm that register null thermal status listener fails with error.
     *
     * @throws Exception
     */
    @Test
    public void testThermalStatusRegisterNullListener() throws Exception {
        final String failureMessage = nativeTestThermalStatusRegisterNullListener();
        if (!Strings.isNullOrEmpty(failureMessage)) {
            fail(failureMessage);
        }
    }

    /**
     * Confirm that double register and unregister same listener fails with error.
     *
     * @throws Exception
     */
    @Test
    public void testThermalStatusListenerDoubleRegistration() throws Exception {
        final String failureMessage = nativeTestThermalStatusListenerDoubleRegistration();
        if (!Strings.isNullOrEmpty(failureMessage)) {
            fail(failureMessage);
        }
    }

    static {
        System.loadLibrary("ctsthermal_jni");
    }
}
