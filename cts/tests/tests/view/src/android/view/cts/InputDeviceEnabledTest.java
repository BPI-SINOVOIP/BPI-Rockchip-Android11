/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.view.cts;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.hardware.input.InputManager;
import android.view.InputDevice;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link android.view.InputDevice} enable/disable functionality.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class InputDeviceEnabledTest {
    private InputManager mInputManager;

    @Before
    public void setup() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mInputManager = instrumentation.getTargetContext().getSystemService(InputManager.class);
        assertNotNull(mInputManager);
    }

    @Test
    public void testIsEnabled() {
        final int[] inputDeviceIds = mInputManager.getInputDeviceIds();
        assertNotNull(inputDeviceIds);
        for (int inputDeviceId : inputDeviceIds) {
            final InputDevice inputDevice = mInputManager.getInputDevice(inputDeviceId);
            // this function call should not produce an error
            inputDevice.isEnabled(); // output is not checked
        }
    }

    @Test
    public void testEnableDisableException() {
        // Check that InputDevice.enable() and InputDevice.disable() throw SecurityException
        final String failMsgNotThrown = "Expected SecurityException not thrown";

        final int[] inputDeviceIds = mInputManager.getInputDeviceIds();
        for (int inputDeviceId : inputDeviceIds) {
            final InputDevice inputDevice = mInputManager.getInputDevice(inputDeviceId);

            try {
                inputDevice.enable();
                fail(failMsgNotThrown);
            } catch (SecurityException e) {
                // Expect to get this exception
            }

            try {
                inputDevice.disable();
                fail(failMsgNotThrown);
            } catch (SecurityException e) {
                // Expect to get this exception
            }
        }
    }
}
