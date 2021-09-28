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

package android.hardware.input.cts.tests;

import android.hardware.cts.R;
import android.os.SystemClock;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class NintendoSwitchProTest extends InputTestCase {
    public NintendoSwitchProTest() {
        super(R.raw.nintendo_switchpro_register);
    }

    @Before
    public void setUp() {
        super.setUp();
        /**
         * During probe, hid-nintendo sends commands to the joystick and waits for some of those
         * commands to execute. Somewhere in the middle of the commands, the driver will register
         * an input device, which is the notification received by InputTestCase.
         * If a command is still being waited on while we start writing
         * events to uhid, all incoming events are dropped, because probe() still hasn't finished.
         * To ensure that hid-nintendo probe is done, add a delay here.
         */
        SystemClock.sleep(1000);
    }

    @Test
    public void testAllKeys() {
        testInputEvents(R.raw.nintendo_switchpro_keyeventtests);
    }

    @Test
    public void testAllMotions() {
        testInputEvents(R.raw.nintendo_switchpro_motioneventtests);
    }
}
