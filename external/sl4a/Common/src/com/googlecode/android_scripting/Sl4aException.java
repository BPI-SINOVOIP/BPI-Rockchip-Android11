/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.googlecode.android_scripting;

/**
 * The base class for all errors to be sent over RPC.
 *
 * For user defined errors, convention dictates that error code values
 * should start with 10000.
 */
public class Sl4aException extends Exception {

    private int mErrorCode;
    private String mErrorMessage;

    Sl4aException(int errorCode, String errorMessage) {
        mErrorCode = errorCode;
        mErrorMessage = errorMessage;
    }

    /**
     * @return the error code associated with this exception
     */
    public int getErrorCode() {
        return mErrorCode;
    }

    /**
     * @return the error message associated with this exception
     */
    public String getErrorMessage() {
        return mErrorMessage;
    }
}
