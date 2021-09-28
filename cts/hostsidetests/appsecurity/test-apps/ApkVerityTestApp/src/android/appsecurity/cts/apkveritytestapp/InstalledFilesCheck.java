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

package android.appsecurity.cts.apkveritytestapp;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;

import org.junit.Test;

import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;

public class InstalledFilesCheck {
    private static final String TAG = InstalledFilesCheck.class.getSimpleName();
    private static final String PACKAGE_NAME = InstalledFilesCheck.class.getPackage().getName();

    static {
        System.loadLibrary("CtsApkVerityTestAppJni");
    }

    private static native boolean hasFsverityNative(@NonNull String path);

    @Test
    public void testFilesHaveFsverity() {
        for (String path : getInterestedFiles()) {
            assertTrue("Expect file installed: " + path, new File(path).exists());
            assertTrue("Expect to have fs-verity: " + path, hasFsverityNative(path));
        }
    }

    private ArrayList<String> getInterestedFiles() {
        Context context = InstrumentationRegistry.getContext();
        Path dirname = Paths.get(context.getApplicationInfo().sourceDir).getParent();
        ArrayList<String> output = new ArrayList<>();

        android.os.Bundle testArgs = InstrumentationRegistry.getArguments();
        assertNotEquals(testArgs, null);
        int number = Integer.valueOf(testArgs.getString("Number"));
        assertTrue(number > 0);

        for (int i = 0; i < number; ++i) {
            String basename = testArgs.getString("File" + Integer.toString(i));
            String filename = dirname.resolve(basename).toString();
            Log.d(TAG, "Adding interested file " + filename);
            output.add(filename);
        }
        return output;
    }
}
