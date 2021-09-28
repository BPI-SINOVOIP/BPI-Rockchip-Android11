/*
 * Copyright (C) 2010 The Android Open Source Project
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
package com.android.tradefed.device;

import com.android.tradefed.error.HarnessException;
import com.android.tradefed.result.error.ErrorIdentifier;

import java.lang.StackWalker.Option;

/**
 * Thrown when a device is no longer available for testing. e.g. the adb connection to the device
 * has been lost, device has stopped responding to commands, etc
 */
public class DeviceNotAvailableException extends HarnessException {

    private static final long serialVersionUID = 2202987086655357202L;
    private String mSerial = null;

    /**
     * Creates a {@link DeviceNotAvailableException}.
     *
     * @deprecated Use {@link #DeviceNotAvailableException(String, String, ErrorIdentifier)}
     *     instead.
     */
    @Deprecated
    public DeviceNotAvailableException() {
        super(null);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceNotAvailableException}.
     *
     * @param msg a descriptive message.
     * @deprecated Use {@link #DeviceNotAvailableException(String, String, ErrorIdentifier)}
     *     instead.
     */
    @Deprecated
    public DeviceNotAvailableException(String msg) {
        this(msg, null, null, null);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceNotAvailableException}.
     *
     * @param msg a descriptive message.
     * @param serial the serial of the device concerned
     */
    public DeviceNotAvailableException(String msg, String serial) {
        this(msg, null, serial, null);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceNotAvailableException}.
     *
     * @param msg a descriptive message.
     * @param serial the serial of the device concerned
     * @param errorId the error identifier for this error.
     */
    public DeviceNotAvailableException(String msg, String serial, ErrorIdentifier errorId) {
        this(msg, null, serial, errorId);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceNotAvailableException}.
     *
     * @param msg a descriptive message.
     * @param cause the root {@link Throwable} that caused the device to become unavailable.
     * @param serial the serial of the device concerned by the exception
     */
    public DeviceNotAvailableException(String msg, Throwable cause, String serial) {
        this(msg, cause, serial, null);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceNotAvailableException}.
     *
     * @param msg a descriptive message.
     * @param cause the root {@link Throwable} that caused the device to become unavailable.
     * @param serial the serial of the device concerned by the exception
     * @param errorId the error identifier for this error.
     */
    public DeviceNotAvailableException(
            String msg, Throwable cause, String serial, ErrorIdentifier errorId) {
        super(msg, cause, errorId);
        mSerial = serial;
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Return Serial of the device associated with exception.
     */
    public String getSerial() {
        return mSerial;
    }
}
