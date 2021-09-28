/*
 * Copyright (C) 2016 The Android Open Source Project
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

import com.android.tradefed.result.error.ErrorIdentifier;

import java.lang.StackWalker.Option;

/**
 * Thrown when a device is no longer reachable via it's transport type, e.g. if the device is no
 * longer visible via USB, or TCP/IP connection
 */
public class DeviceDisconnectedException extends DeviceNotAvailableException {

    private static final long serialVersionUID = 1L;

    /**
     * Creates a {@link DeviceUnresponsiveException}.
     *
     * @param msg a descriptive message.
     * @param serial the serial of the device concerned.
     */
    public DeviceDisconnectedException(String msg, String serial) {
        this(msg, null, serial, null);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceUnresponsiveException}.
     *
     * @param msg a descriptive message.
     * @param serial the serial of the device concerned.
     * @param errorId the error identifier for this error.
     */
    public DeviceDisconnectedException(String msg, String serial, ErrorIdentifier errorId) {
        this(msg, null, serial, errorId);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceUnresponsiveException}.
     *
     * @param msg a descriptive message.
     * @param cause the root {@link Throwable} that caused the device to become unavailable.
     * @param serial the serial of the device concerned.
     */
    public DeviceDisconnectedException(String msg, Throwable cause, String serial) {
        this(msg, cause, serial, null);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Creates a {@link DeviceUnresponsiveException}.
     *
     * @param msg a descriptive message.
     * @param cause the root {@link Throwable} that caused the device to become unavailable.
     * @param serial the serial of the device concerned.
     * @param errorId the error identifier for this error.
     */
    public DeviceDisconnectedException(
            String msg, Throwable cause, String serial, ErrorIdentifier errorId) {
        super(msg, cause, serial, errorId);
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }
}
