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

package android.appsecurity.cts;

/**
 * Utilities to deal with exceptions
 */
public class ExceptionUtils {
    private ExceptionUtils() {}

    /**
     * Gets the root {@link Throwable#getCause() cause} of {@code t}
     */
    public static Throwable getRootCause(Throwable t) {
        while (t.getCause() != null) t = t.getCause();
        return t;
    }

    /**
     * Appends {@code cause} at the end of the causal chain of {@code t}
     *
     * @return {@code t} for convenience
     */
    public static <E extends Throwable> E appendCause(E t, Throwable cause) {
        if (cause != null) {
            getRootCause(t).initCause(cause);
        }
        return t;
    }
}
