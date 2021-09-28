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

package com.android.cts.appdataisolation.common;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertTrue;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.fail;
import static org.testng.Assert.expectThrows;

import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/*
 * This class is a helper class for test app file operations.
 */
public class FileUtils {

    public static final String APPA_PKG = "com.android.cts.appdataisolation.appa";
    public static final String APPB_PKG = "com.android.cts.appdataisolation.appb";
    public static final String NOT_INSTALLED_PKG = "com.android.cts.appdataisolation.not_installed_pkg";

    public static final String CE_DATA_FILE_NAME = "ce_data_file";
    public static final String DE_DATA_FILE_NAME = "de_data_file";
    public final static String EXTERNAL_DATA_FILE_NAME = "external_data_file";
    public final static String OBB_FILE_NAME = "obb_file";

    private static final String JAVA_FILE_PERMISSION_DENIED_MSG =
            "open failed: EACCES (Permission denied)";
    private static final String JAVA_FILE_NOT_FOUND_MSG =
            "open failed: ENOENT (No such file or directory)";

    public static void assertDirIsNotAccessible(String path) {
        // Trying to access a file that does not exist in that directory, it should return
        // permission denied not file not found.
        Exception exception = expectThrows(FileNotFoundException.class, () -> {
            new FileInputStream(new File(path, "FILE_DOES_NOT_EXIST"));
        });
        assertThat(exception.getMessage()).contains(JAVA_FILE_PERMISSION_DENIED_MSG);
        assertThat(exception.getMessage()).doesNotContain(JAVA_FILE_NOT_FOUND_MSG);
    }

    public static void assertDirDoesNotExist(String path) {
        // Trying to access a file/directory that does exist, but is not visible to the caller, it
        // should return file not found.
        Exception exception = expectThrows(FileNotFoundException.class, () -> {
            new FileInputStream(new File(path));
        });
        assertThat(exception.getMessage()).contains(JAVA_FILE_NOT_FOUND_MSG);
        assertThat(exception.getMessage()).doesNotContain(JAVA_FILE_PERMISSION_DENIED_MSG);

        // Try to create a directory here, and it should return permission denied not directory
        // exists.
        try {
            Os.mkdir(path, 0700);
            fail("Should not able to mkdir() on " + path);
        } catch (ErrnoException e) {
            assertEquals(e.errno, OsConstants.EACCES, "Error on path: " + path);
        }
    }

    public static void assertDirIsAccessible(String path) {
        // This can ensure directory path exists, and by trying to access a file doesn't exist,
        // if app has search permission to that directory, it should return file not found
        // and not security exception.
        assertFileDoesNotExist(path, "FILE_DOES_NOT_EXIST");
    }

    public static void assertFileIsAccessible(String path) {
        try (FileInputStream is = new FileInputStream(path)) {
            is.read();
        } catch (IOException e) {
            fail("Could not read file: " + path, e);
        }
    }

    public static void assertFileDoesNotExist(String path, String name) {
        // Make sure parent dir exists
        File directory = new File(path);
        assertTrue(directory.exists());

        Exception exception = expectThrows(FileNotFoundException.class, () -> {
            new FileInputStream(new File(path, name));
        });
        assertThat(exception.getMessage()).contains(JAVA_FILE_NOT_FOUND_MSG);
        assertThat(exception.getMessage()).doesNotContain(JAVA_FILE_PERMISSION_DENIED_MSG);
    }

    public static void assertFileExists(String path, String name) {
        File file = new File(path, name);
        assertTrue(file.exists());
    }

    public static void touchFile(String path, String name) throws IOException {
        File file = new File(path, name);
        file.createNewFile();
    }

    public static String replacePackageAWithPackageB(String path) {
        return path.replace(APPA_PKG, APPB_PKG);
    }

    public static String replacePackageAWithNotInstalledPkg(String path) {
        return path.replace(APPA_PKG, NOT_INSTALLED_PKG);
    }
}
