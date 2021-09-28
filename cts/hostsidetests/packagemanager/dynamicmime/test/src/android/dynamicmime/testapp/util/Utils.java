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

package android.dynamicmime.testapp.util;

import static org.junit.Assert.assertTrue;

import android.os.ParcelFileDescriptor;

import androidx.test.platform.app.InstrumentationRegistry;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

public class Utils {
    public static void installApk(String pathToApk) {
        executeShellCommand("pm install -t " + pathToApk);
    }

    public static void updateApp(String pathToApk) {
        executeShellCommand("pm install -t -r  " + pathToApk);
    }

    public static void uninstallApp(String appPackage) {
        executeShellCommand("pm uninstall " + appPackage);
    }

    private static void executeShellCommand(String command) {
        ParcelFileDescriptor pfd = InstrumentationRegistry
                .getInstrumentation()
                .getUiAutomation()
                .executeShellCommand(command);
        InputStream is = new FileInputStream(pfd.getFileDescriptor());

        try {
            readFully(is);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static void readFully(InputStream in) throws IOException {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            byte[] buffer = new byte[1024];
            int count;
            while ((count = in.read(buffer)) != -1) {
                bytes.write(buffer, 0, count);
            }

            String result = new String(bytes.toByteArray());
            assertTrue(result, result.isEmpty() || result.contains("Success"));
        } finally {
            in.close();
        }
    }
}
