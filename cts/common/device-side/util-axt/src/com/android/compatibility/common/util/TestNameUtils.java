/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.compatibility.common.util;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Generic helper used to set / get the the name of the test being run.
 *
 * <p>Typically used on {@code @Rule} classes.
 */
public final class TestNameUtils {

    private static String sCurrentTestName;
    private static String sCurrentTestClass;

    /**
     * Gets the name of the test current running.
     */
    @NonNull
    public static String getCurrentTestName() {
        if (sCurrentTestName != null) return sCurrentTestName;
        if (sCurrentTestClass != null) return sCurrentTestClass;
        return "(Unknown test)";
    }

    /**
     * Sets the name of the test current running
     */
    public static void setCurrentTestName(@Nullable String name) {
        sCurrentTestName = name;
    }

    /**
     * Sets the name of the test class current running
     */
    public static void setCurrentTestClass(@Nullable String testClass) {
        sCurrentTestClass = testClass;
    }

    /**
     * Checks whether a test is running, based on whether {@link #setCurrentTestName(String)} was
     * called.
     */
    public static boolean isRunningTest() {
        return sCurrentTestName != null;
    }

    private TestNameUtils() {
        throw new UnsupportedOperationException("contain static methods only");
    }
}
