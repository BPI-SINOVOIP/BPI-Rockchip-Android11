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

package android.graphics.cts;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ContentResolver;
import android.content.res.Resources;
import android.net.Uri;

import androidx.test.InstrumentationRegistry;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class Utils {
    private static Resources getResources() {
        return InstrumentationRegistry.getTargetContext().getResources();
    }

    private static InputStream obtainInputStream(int resId) {
        return getResources().openRawResource(resId);
    }

    public static Uri getAsResourceUri(int resId) {
        Resources res = getResources();
        return new Uri.Builder()
                .scheme(ContentResolver.SCHEME_ANDROID_RESOURCE)
                .authority(res.getResourcePackageName(resId))
                .appendPath(res.getResourceTypeName(resId))
                .appendPath(res.getResourceEntryName(resId))
                .build();
    }

    /*
     * Create a new file and return a path to it.
     * @param resId Original file. It will be copied into the new file.
     * @param offset Number of zeroes to write to the new file before the
     *               copied file. This allows testing decodeFileDescriptor
     *               with an offset. Must be less than or equal to 1024
     */
    static String obtainPath(int resId, long offset) {
        return obtainFile(resId, offset).getPath();
    }

    /*
     * Create and return a new file.
     * @param resId Original file. It will be copied into the new file.
     * @param offset Number of zeroes to write to the new file before the
     *               copied file. This allows testing decodeFileDescriptor
     *               with an offset. Must be less than or equal to 1024
     */
    static File obtainFile(int resId, long offset) {
        assertTrue(offset >= 0);
        File dir = InstrumentationRegistry.getTargetContext().getFilesDir();
        dir.mkdirs();

        String name = getResources().getResourceEntryName(resId).toString();
        if (offset > 0) {
            name = name + "_" + String.valueOf(offset);
        }

        File file = new File(dir, name);
        if (file.exists()) {
            return file;
        }

        try {
            file.createNewFile();
        } catch (IOException e) {
            e.printStackTrace();
            // If the file does not exist it will be handled below.
        }
        if (!file.exists()) {
            fail("Failed to create new File for " + name + "!");
        }

        InputStream is = obtainInputStream(resId);

        try {
            FileOutputStream fOutput = new FileOutputStream(file);
            byte[] dataBuffer = new byte[1024];
            // Write a bunch of zeroes before the image.
            assertTrue(offset <= 1024);
            fOutput.write(dataBuffer, 0, (int) offset);
            int readLength = 0;
            while ((readLength = is.read(dataBuffer)) != -1) {
                fOutput.write(dataBuffer, 0, readLength);
            }
            is.close();
            fOutput.close();
        } catch (IOException e) {
            e.printStackTrace();
            fail("Failed to create file \"" + name + "\" with exception " + e);
        }
        return file;
    }

    /**
     * Helper for JUnit-Params tests to combine inputs.
     */
    static Object[] crossProduct(Object[] a, Object[] b) {
        final int length = a.length * b.length;
        Object[] ret = new Object[length];
        for (int i = 0; i < a.length; i++) {
            for (int j = 0; j < b.length; j++) {
                int index = i * b.length + j;
                assertNull(ret[index]);
                ret[index] = new Object[] { a[i], b[j] };
            }
        }
        return ret;
    }
}
