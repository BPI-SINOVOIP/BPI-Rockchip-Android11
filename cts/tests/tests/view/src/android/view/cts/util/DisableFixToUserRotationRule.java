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

package android.view.cts.util;

import android.app.UiAutomation;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;

public class DisableFixToUserRotationRule implements TestRule {
    private static final String TAG = "DisableFixToUserRotationRule";
    private static final String COMMAND = "cmd window set-fix-to-user-rotation ";

    private final UiAutomation mUiAutomation;

    public DisableFixToUserRotationRule() {
        mUiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                executeShellCommandAndPrint(COMMAND + "disabled");
                try {
                    base.evaluate();
                } finally {
                    executeShellCommandAndPrint(COMMAND + "default");
                }
            }
        };
    }

    private void executeShellCommandAndPrint(String cmd) {
        ParcelFileDescriptor pfd = mUiAutomation.executeShellCommand(cmd);
        StringBuilder builder = new StringBuilder();
        char[] buffer = new char[256];
        int charRead;
        try (Reader reader =
                     new InputStreamReader(new ParcelFileDescriptor.AutoCloseInputStream(pfd))) {
            while ((charRead = reader.read(buffer)) > 0) {
                builder.append(buffer, 0, charRead);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        Log.i(TAG, "Command: " + cmd + " Output: " + builder);
    }

}
