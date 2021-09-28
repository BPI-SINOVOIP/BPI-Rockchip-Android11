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

package com.android.cts.userspacereboot.basic;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.PowerManager;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;

/**
 * A test app called from {@link com.android.cts.userspacereboot.host.UserspaceRebootHostTest} to
 * verify basic properties around userspace reboot.
 *
 * <p>Additionally it's used as a test app to receive {@link Intent.ACTION_BOOT_COMPLETED}
 * broadcast. Another test app {@link
 * com.android.cts.userspacereboot.bootcompleted.com.android.cts.userspacereboot.bootcompleted} will
 * query a {@link ContentProvider} exposed by this app in order to verify that broadcast was
 * received.
 *
 * <p>Such separation is required due to the fact, that when {@code adb shell am instrument} is
 * called for an app, it will always force stop that app. This means that if we start an
 * instrumentation test in the same app that listens for {@link Intent.ACTION_BOOT_COMPLETED}
 * broadcast, we might end up not receiving broadcast at all, because {@link
 * Intent.ACTION_BOOT_COMPLETED} is not delivered to stopped apps.
 */
@RunWith(JUnit4.class)
public class BasicUserspaceRebootTest {

    private static final String TAG = "UserspaceRebootTest";

    private final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();

    /**
     * Tests that {@link PowerManager#isRebootingUserspaceSupported()} returns {@code true}.
     */
    @Test
    public void testUserspaceRebootIsSupported() {
        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        assertThat(powerManager.isRebootingUserspaceSupported()).isTrue();
    }

    /**
     * Tests that {@link PowerManager#isRebootingUserspaceSupported()} returns {@code false}.
     */
    @Test
    public void testUserspaceRebootIsNotSupported() {
        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        assertThat(powerManager.isRebootingUserspaceSupported()).isFalse();
        assertThrows(UnsupportedOperationException.class,
                () -> powerManager.reboot("userspace"));
    }

    /**
     * Deletes test file in app data directory if necessary.
     */
    @Test
    public void prepareFile() throws Exception {
        Context de = mContext.createDeviceProtectedStorageContext();
        de.deleteFile(Intent.ACTION_BOOT_COMPLETED.toLowerCase());
    }

    /**
     * Receiver of {@link Intent.ACTION_LOCKED_BOOT_COMPLETED} and
     * {@link Intent.ACTION_BOOT_COMPLETED} broadcasts.
     */
    public static class BootReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG, "Received! " + intent);
            String fileName = intent.getAction().toLowerCase();
            try (PrintWriter writer = new PrintWriter(new OutputStreamWriter(
                    context.createDeviceProtectedStorageContext().openFileOutput(
                            fileName, Context.MODE_PRIVATE)))) {
                writer.println(intent.getAction());
            } catch (IOException e) {
                Log.w(TAG, "Failed to append to " + fileName, e);
            }
        }
    }

    /**
     * Returns whenever {@link Intent.ACTION_LOCKED_BOOT_COMPLETED} and
     * {@link Intent.ACTION_BOOT_COMPLETED} broadcast were received.
     */
    public static class Provider extends ContentProvider {

        @Override
        public boolean onCreate() {
            return true;
        }

        @Override
        public String getType(Uri uri) {
            return "vnd.android.cursor.item/com.android.cts.userspacereboot.basic.exists";
        }

        @Override
        public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
                String sortOrder) {
            Context de = getContext().createDeviceProtectedStorageContext();
            File locked_boot_completed = new File(
                    de.getFilesDir(), Intent.ACTION_LOCKED_BOOT_COMPLETED.toLowerCase());
            File boot_completed = new File(
                    de.getFilesDir(), Intent.ACTION_BOOT_COMPLETED.toLowerCase());
            MatrixCursor cursor = new MatrixCursor(
                    new String[]{ "locked_boot_completed", "boot_completed"});
            cursor.addRow(new Object[] {
                    locked_boot_completed.exists() ? 1 : 0, boot_completed.exists() ? 1 : 0 });
            return cursor;
        }

        @Override
        public Uri insert(Uri uri, ContentValues values) {
            throw new UnsupportedOperationException();
        }

        @Override
        public int delete(Uri uri, String selection, String[] selectionArgs) {
            throw new UnsupportedOperationException();
        }

        @Override
        public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
            throw new UnsupportedOperationException();
        }
    }
}
