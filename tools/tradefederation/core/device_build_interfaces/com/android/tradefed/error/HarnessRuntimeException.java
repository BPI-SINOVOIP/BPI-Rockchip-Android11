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
package com.android.tradefed.error;

import com.android.tradefed.result.error.ErrorIdentifier;

import java.lang.StackWalker.Option;

public class HarnessRuntimeException extends RuntimeException implements IHarnessException {

    static final long serialVersionUID = 100L;

    private String mOrigin;
    private ErrorIdentifier mErrorId;

    /**
     * Constructor for the exception.
     *
     * @param message The message associated with the exception
     * @param errorId The {@link ErrorIdentifier} categorizing the exception.
     */
    public HarnessRuntimeException(String message, ErrorIdentifier errorId) {
        super(message);
        mErrorId = errorId;
        mOrigin =
                StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE)
                        .getCallerClass()
                        .getCanonicalName();
    }

    /**
     * Constructor for the exception.
     *
     * @param message The message associated with the exception
     * @param cause The {@link IHarnessException} that caused the exception.
     */
    public HarnessRuntimeException(String message, IHarnessException cause) {
        super(message);
        mErrorId = cause.getErrorId();
        mOrigin = cause.getOrigin();
    }

    /**
     * Constructor for the exception.
     *
     * @param message The message associated with the exception
     * @param cause The cause of the exception
     * @param errorId The {@link ErrorIdentifier} categorizing the exception.
     */
    public HarnessRuntimeException(String message, Throwable cause, ErrorIdentifier errorId) {
        super(message, cause);
        mErrorId = errorId;
        mOrigin =
                StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE)
                        .getCallerClass()
                        .getCanonicalName();
    }

    @Override
    public ErrorIdentifier getErrorId() {
        return mErrorId;
    }

    @Override
    public String getOrigin() {
        return mOrigin;
    }
}
