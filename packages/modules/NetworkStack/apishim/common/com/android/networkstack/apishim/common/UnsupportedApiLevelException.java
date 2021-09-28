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
package com.android.networkstack.apishim.common;

import androidx.annotation.Nullable;

/**
 * Signals that the calling API is not supported with device release or development API level
 */
public class UnsupportedApiLevelException extends Exception {
    /**
     * Constructs an {@code UnsupportedApiLevelException} with {@code null} as its error detail
     * message.
     */
    public UnsupportedApiLevelException() {
        super();
    }

    /**
     * Constructs an {@code UnsupportedApiLevelException} with the specified detail message.
     *
     * @param message The detail message (which is saved for later retrieval by the
     *                {@link #getMessage()} method)
     */
    public UnsupportedApiLevelException(String message) {
        super(message);
    }

    /**
     * Constructs an {@code UnsupportedApiLevelException} with the specified detail message
     * and cause.
     *
     * @param message The detail message (which is saved for later retrieval by the
     *                {@link #getMessage()} method)
     *
     * @param cause The cause (which is saved for later retrieval by the {@link #getCause()}
     *              method).  (A null value is permitted, and indicates that the cause is
     *              nonexistent or unknown.)
     */
    public UnsupportedApiLevelException(String message, @Nullable Throwable cause) {
        super(message, cause);
    }

    /**
     * Constructs an {@code UnsupportedApiLevelException} with the specified cause and a
     * detail message of {@code (cause==null ? null : cause.toString())} (which typically contains
     * the class and detail message of {@code cause}).
     *
     * @param cause The cause (which is saved for later retrieval by the {@link #getCause()}
     *              method).  (A null value is permitted, and indicates that the cause is
     *              nonexistent or unknown.)
     */
    public UnsupportedApiLevelException(@Nullable Throwable cause) {
        super(cause);
    }
}
