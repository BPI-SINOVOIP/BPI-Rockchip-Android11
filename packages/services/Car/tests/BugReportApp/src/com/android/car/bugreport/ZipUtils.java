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
package com.android.car.bugreport;

import android.util.Log;

import com.google.common.io.ByteStreams;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

/** Zip utility functions. */
final class ZipUtils {
    private static final String TAG = ZipUtils.class.getSimpleName();

    /** Extracts the contents of a zip file to the zip output stream. */
    static void extractZippedFileToZipStream(File file, ZipOutputStream zipStream) {
        if (!file.exists()) {
            Log.w(TAG, "File " + file + " not found");
            return;
        }
        if (file.length() == 0) {
            // If there were issues with reading from dumpstate socket, the dumpstate zip
            // file still might be available in
            // /data/user_de/0/com.android.shell/files/bugreports/.
            Log.w(TAG, "Zip file " + file.getName() + " is empty, skipping.");
            return;
        }
        try (ZipFile zipFile = new ZipFile(file)) {
            Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                try (InputStream stream = zipFile.getInputStream(entry)) {
                    writeInputStreamToZipStream(entry.getName(), stream, zipStream);
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "Failed to add " + file + " to zip", e);
        }
    }

    /** Adds a file to the zip output stream. */
    static void addFileToZipStream(File file, ZipOutputStream zipStream) {
        if (!file.exists()) {
            Log.w(TAG, "File " + file + " not found");
            return;
        }
        if (file.length() == 0) {
            Log.w(TAG, "File " + file.getName() + " is empty, skipping.");
            return;
        }
        try (FileInputStream audioInput = new FileInputStream(file)) {
            writeInputStreamToZipStream(file.getName(), audioInput, zipStream);
        } catch (IOException e) {
            Log.w(TAG, "Failed to add " + file + "to the final zip");
        }
    }

    /** Writes a new file from input stream to the zip stream. */
    static void writeInputStreamToZipStream(
            String filename, InputStream input, ZipOutputStream zipStream) throws IOException {
        ZipEntry entry = new ZipEntry(filename);
        zipStream.putNextEntry(entry);
        ByteStreams.copy(input, zipStream);
        zipStream.closeEntry();
    }

    private ZipUtils() {}
}
