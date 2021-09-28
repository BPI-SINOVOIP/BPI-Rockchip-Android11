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

package com.android.cts.userspacereboot.bootcompleted;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.UserManager;
import android.util.Log;

import com.android.compatibility.common.util.TestUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.OutputStreamWriter;
import java.time.Duration;
import java.util.Scanner;

/**
 * A test app called from {@link com.android.cts.userspacereboot.host.UserspaceRebootHostTest} to
 * verify CE storage related properties of userspace reboot.
 */
@RunWith(JUnit4.class)
public class BootCompletedUserspaceRebootTest {

    private static final String TAG = "UserspaceRebootTest";

    private static final String FILE_NAME = "secret.txt";
    private static final String SECRET_MESSAGE = "wow, much secret";

    private static final Duration LOCKED_BOOT_TIMEOUT = Duration.ofMinutes(3);
    private static final Duration BOOT_TIMEOUT = Duration.ofMinutes(6);

    private final Context mCeContext = getInstrumentation().getContext();
    private final Context mDeContext = mCeContext.createDeviceProtectedStorageContext();

    /**
     * Writes to a file in CE storage of {@link BootCompletedUserspaceRebootTest}.
     *
     * <p>Reading content of this file is used by other test cases in this class to verify that CE
     * storage is unlocked after userspace reboot.
     */
    @Test
    public void prepareFile() throws Exception {
        try (OutputStreamWriter writer = new OutputStreamWriter(
                mCeContext.openFileOutput(FILE_NAME, Context.MODE_PRIVATE))) {
            writer.write(SECRET_MESSAGE);
        }
    }

    /**
     * Tests that CE storage is unlocked by reading content of a file in CE storage.
     */
    @Test
    public void testVerifyCeStorageUnlocked() throws Exception {
        UserManager um = getInstrumentation().getContext().getSystemService(UserManager.class);
        assertThat(um.isUserUnlocked()).isTrue();
        try (Scanner scanner = new Scanner(mCeContext.openFileInput(FILE_NAME))) {
            final String content = scanner.nextLine();
            assertThat(content).isEqualTo(SECRET_MESSAGE);
        }
    }

    /**
     * Tests that {@link Intent.ACTION_LOCKED_BOOT_COMPLETED} broadcast was sent.
     */
    @Test
    public void testVerifyReceivedLockedBootCompletedBroadcast() throws Exception {
        waitForBroadcast(Intent.ACTION_LOCKED_BOOT_COMPLETED, LOCKED_BOOT_TIMEOUT);
    }

    /**
     * Tests that {@link Intent.ACTION_BOOT_COMPLETED} broadcast was sent.
     */
    @Test
    public void testVerifyReceivedBootCompletedBroadcast() throws Exception {
        waitForBroadcast(Intent.ACTION_BOOT_COMPLETED, BOOT_TIMEOUT);
    }

    private void waitForBroadcast(String intent, Duration timeout) throws Exception {
        TestUtils.waitUntil(
                "Didn't receive broadcast " + intent + " in " + timeout,
                (int) timeout.getSeconds(),
                () -> queryBroadcast(intent));
    }

    private boolean queryBroadcast(String intent) {
        Uri uri = Uri.parse("content://com.android.cts.userspacereboot.basic/files/"
                + intent.toLowerCase());
        Cursor cursor = mDeContext.getContentResolver().query(uri, null, null, null, null);
        if (cursor == null) {
            return false;
        }
        if (!cursor.moveToFirst()) {
            Log.w(TAG, "Broadcast: " + intent + " cursor is empty");
            return false;
        }
        String column = intent.equals(Intent.ACTION_LOCKED_BOOT_COMPLETED)
                ? "locked_boot_completed"
                : "boot_completed";
        int index = cursor.getColumnIndex(column);
        return cursor.getInt(index) == 1;
    }
}
