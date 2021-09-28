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
package com.android.atest;

import com.google.common.base.Strings;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;

public class AtestUtils {

    private static final String EMPTY_STRING_ERROR = " can't be empty.\n";

    /**
     * Checks if the directory contains test mapping file.
     *
     * @param path the directory to check.
     * @return true if the path contains test mapping file.
     */
    public static boolean hasTestMapping(String path) {
        return Files.exists(Paths.get(path, Constants.TEST_MAPPING_FILE_NAME));
    }

    /**
     * Gets the Android root path by project path.
     *
     * @param projectPath the base path of user's project.
     * @return the Android root path or null.
     */
    public static String getAndroidRoot(String projectPath) {
        File currentFolder = new File(projectPath);
        File parentFolder = currentFolder.getParentFile();
        File checkFolder = new File(currentFolder, Constants.BUILD_ENVIRONMENT);
        while (parentFolder != null) {
            if (checkFolder.exists()) {
                return currentFolder.getPath();
            } else {
                currentFolder = parentFolder;
                parentFolder = currentFolder.getParentFile();
                checkFolder = new File(currentFolder, Constants.BUILD_ENVIRONMENT);
            }
        }
        return null;
    }

    /**
     * Checks if there are any empty strings.
     *
     * @param targets strings to be checked.
     * @return true if there are any empty arguments.
     */
    public static boolean checkEmpty(String... targets) {
        for (String target : targets) {
            if (Strings.isNullOrEmpty(target)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Gets error message from the Atest arguments.
     *
     * @param lunchTarget the lunch target.
     * @param testTarget the Atest test target.
     * @param workPath the work path to run the command.
     * @return the error message shown for users.
     */
    public static String checkError(String lunchTarget, String testTarget, String workPath) {
        StringBuilder errorMessage = new StringBuilder("Please check:\n");
        if (Strings.isNullOrEmpty(testTarget)) {
            errorMessage.append("- Atest target" + EMPTY_STRING_ERROR);
        }
        if (Strings.isNullOrEmpty(lunchTarget)) {
            errorMessage.append("- lunch target" + EMPTY_STRING_ERROR);
        }
        if (Strings.isNullOrEmpty(workPath)) {
            errorMessage.append(
                    "- Atest can only execute when your project is under Android "
                            + "source directory.");
        }
        return errorMessage.toString();
    }
}
