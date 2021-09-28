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

package android.appsecurity.cts;

import static org.junit.Assert.fail;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.ITestInformationReceiver;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * {@link TestRule} class that disables screen saver and doze settings before each test method
 * running and restoring to initial values after test method finished.
 */
public class NormalizeScreenStateRule implements TestRule {
    private final ITestInformationReceiver mTestInformationReceiver;

    public NormalizeScreenStateRule(ITestInformationReceiver testInformationReceiver) {
        mTestInformationReceiver = testInformationReceiver;
    }

    /** Doze items copied from ActivityManagerTestBase since the keys are hidden. */
    private static final String[] DOZE_SETTINGS = {
            "doze_enabled",
            "doze_always_on",
            "doze_pulse_on_pick_up",
            "doze_pulse_on_long_press",
            "doze_pulse_on_double_tap",
            "doze_wake_screen_gesture",
            "doze_wake_display_gesture",
            "doze_tap_gesture",
            "screensaver_enabled",
    };

    private ITestDevice getDevice() {
        return mTestInformationReceiver.getTestInformation().getDevice();
    }

    private String getSecureSetting(String key) {
        try {
            CommandResult res = getDevice().executeShellV2Command("settings get secure " + key);
            if (res.getStatus() != CommandStatus.SUCCESS) {
                fail("Could not set setting " + key + ": " + res.getStdout());
            }
            return res.getStdout().trim();
        } catch (DeviceNotAvailableException e) {
            fail("Could not connect to device: " + e.getMessage());
            return null;
        }
    }

    private void putSecureSetting(String key, String value) {
        try {
            CommandResult res = getDevice().executeShellV2Command(
                    "settings put secure " + key + " " + value);
            if (res.getStatus() != CommandStatus.SUCCESS) {
                fail("Could not set setting " + key + ": " + res.getStdout());
            }
        } catch (DeviceNotAvailableException e) {
            fail("Could not connect to device: " + e.getMessage());
        }
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                final Map<String, String> initialValues = new HashMap<>();
                Arrays.stream(DOZE_SETTINGS).forEach(
                        k -> initialValues.put(k, getSecureSetting(k)));
                try {
                    Arrays.stream(DOZE_SETTINGS).forEach(k -> putSecureSetting(k, "0"));
                    base.evaluate();
                } finally {
                    Arrays.stream(DOZE_SETTINGS).forEach(
                            k -> putSecureSetting(k, initialValues.get(k)));
                }
            }
        };
    }
}
