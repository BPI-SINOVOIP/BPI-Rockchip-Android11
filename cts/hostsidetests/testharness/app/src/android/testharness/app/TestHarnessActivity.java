/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.testharness.app;

import android.app.Activity;
import android.os.Bundle;
import android.view.inputmethod.InputMethodManager;
import android.widget.TextView;

import java.io.IOException;
import java.lang.Override;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * An activity that opens up the keyboard and contains methods for testing.
 *
 * <p>This is used to verify that calling {@code adb shell cmd testharness enable} wipes all apps
 * and their private data directories.
 */
public class TestHarnessActivity extends Activity {
    public static final String DIRTY_DEVICE_FILENAME = "dirty_device.txt";
    private boolean mKeyboardVisible;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_test_harness);
    }

    @Override
    protected void onResume() {
        super.onResume();

        final TextView textView = findViewById(R.id.text_view);
        textView.postDelayed(() -> {
            getSystemService(InputMethodManager.class)
                    .toggleSoftInputFromWindow(
                            textView.getWindowToken(),
                            InputMethodManager.SHOW_FORCED,
                            0);
            mKeyboardVisible = true;
        }, 200);
    }

    boolean isKeyboardVisible() {
        return mKeyboardVisible;
    }

    void dirtyDevice() {
        try {
            Path dirtyFile = getFilesDir().toPath().resolve(DIRTY_DEVICE_FILENAME);
            Files.write(dirtyFile, "This device is dirty!".getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    boolean isDeviceClean() {
        return !Files.exists(getFilesDir().toPath().resolve(DIRTY_DEVICE_FILENAME));
    }
}
