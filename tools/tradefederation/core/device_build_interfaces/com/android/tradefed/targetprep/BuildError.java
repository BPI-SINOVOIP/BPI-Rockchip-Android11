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
package com.android.tradefed.targetprep;

import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.error.HarnessException;
import com.android.tradefed.result.error.ErrorIdentifier;

import java.lang.StackWalker.Option;

/** Thrown if the provided build fails to run. */
public class BuildError extends HarnessException {

    private static final long serialVersionUID = 2202987086655357201L;
    private String mDeviceSerial = null;

    /**
     * Constructs a new (@link BuildError} with a detailed error message.
     *
     * @param reason an error message giving more details on the build error
     * @param descriptor the descriptor of the device concerned
     * @deprecated use {@link #BuildError(String, DeviceDescriptor, ErrorIdentifier)} instead.
     */
    @Deprecated
    public BuildError(String reason, DeviceDescriptor descriptor) {
        super(reason + " " + descriptor, null);
        if (descriptor != null) {
            mDeviceSerial = descriptor.getSerial();
        }
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /**
     * Constructs a new (@link BuildError} with a detailed error message.
     *
     * @param reason an error message giving more details on the build error
     * @param descriptor the descriptor of the device concerned
     * @param errorId the error identifier for this error.
     */
    public BuildError(String reason, DeviceDescriptor descriptor, ErrorIdentifier errorId) {
        super(reason + " " + descriptor, errorId);
        if (descriptor != null) {
            mDeviceSerial = descriptor.getSerial();
        }
        setCallerClass(StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass());
    }

    /** Return the serial of the device impacted by the BuildError. */
    public String getDeviceSerial() {
        return mDeviceSerial;
    }
}
