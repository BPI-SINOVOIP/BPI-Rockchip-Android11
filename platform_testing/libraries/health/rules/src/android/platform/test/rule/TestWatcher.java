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
package android.platform.test.rule;

import android.content.Context;
import android.os.Bundle;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.test.InstrumentationRegistry;

import java.io.IOException;

/**
 * A base {@link org.junit.rules.TestWatcher} with common support for platform testing.
 */
public class TestWatcher extends org.junit.rules.TestWatcher {
    private static final String LOG_TAG = TestWatcher.class.getSimpleName();

    private UiDevice mDevice;

    /**
     * Returns the active {@link UiDevice} to interact with.
     *
     * <p>Override this for unit testing device calls.
     */
    protected UiDevice getUiDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }
        return mDevice;
    }

    /**
     * Runs a shell command, {@code cmd}, and returns the output.
     *
     * <p>Override this for unit testing shell commands.
     */
    protected String executeShellCommand(String cmd) {
        try {
            Log.v(LOG_TAG, String.format("Executing command from %s: %s", this.getClass(), cmd));
            return getUiDevice().executeShellCommand(cmd);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Returns the {@link Bundle} containing registered arguments.
     *
     * <p>Override this for unit testing device calls.
     */
    protected Bundle getArguments() {
        return InstrumentationRegistry.getArguments();
    }

    /** Returns the {@link Context} for this application. */
    protected Context getContext() {
        return InstrumentationRegistry.getContext();
    }
}
