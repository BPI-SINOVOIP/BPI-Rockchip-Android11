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

package android.platform.test.rule;

import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.runner.Description;

import java.io.IOException;

/**
 * This rule sets the Android Autofill provider to none, and prevents autofill dialogs from popping
 * up during the test. .
 */
public class DisableAutofillRule extends TestWatcher {
    private String originalAutoFillService = "null";

    @Override
    protected void starting(Description description) {
        try {
            UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
            String result = uiDevice.executeShellCommand("settings get secure autofill_service");
            if (!TextUtils.isEmpty(result) && !result.equals(originalAutoFillService)) {
                originalAutoFillService = result;
            }
            uiDevice.executeShellCommand("settings put secure autofill_service null");
        } catch (IOException e) {
            throw new RuntimeException("Failed to set autofill service to none");
        }
    }

    @Override
    protected void finished(Description description) {
        try {
            UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                    .executeShellCommand(
                            "settings put secure autofill_service " + originalAutoFillService);
        } catch (IOException e) {
            throw new RuntimeException("Failed to set autofill service back to original");
        }
    }
}
