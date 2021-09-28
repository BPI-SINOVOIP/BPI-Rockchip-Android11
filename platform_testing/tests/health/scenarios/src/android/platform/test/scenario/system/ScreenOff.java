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

package android.platform.test.scenario.system;

import android.os.RemoteException;
import android.os.SystemClock;
import android.platform.test.option.BooleanOption;
import android.platform.test.option.LongOption;
import android.platform.test.scenario.annotation.Scenario;
import android.support.test.uiautomator.UiDevice;
import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Shuts the screen off and waits for a specified amount of time. */
@Scenario
@RunWith(JUnit4.class)
public class ScreenOff {
    @Rule public LongOption mDurationMs = new LongOption("screenOffDurationMs").setDefault(1000L);

    @Rule
    public BooleanOption mTurnScreenBackOn =
            new BooleanOption("screenOffTurnScreenBackOnAfterTest").setDefault(false);

    private UiDevice mDevice;

    @Before
    public void setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    @Test
    public void testScreenOff() throws RemoteException {
        mDevice.sleep();
        SystemClock.sleep(mDurationMs.get());
    }

    @After
    public void tearDown() throws RemoteException {
        if (mTurnScreenBackOn.get()) {
            // Wake up the display. wakeUp() is not used here as when the duration is short, the
            // device might register a double power button press and launch camera.
            mDevice.pressMenu();
            mDevice.waitForIdle();
            // Unlock the screen.
            mDevice.pressMenu();
            mDevice.waitForIdle();
        }
    }
}
