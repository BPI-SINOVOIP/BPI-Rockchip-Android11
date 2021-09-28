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
package android.net.ipsec.ike.exceptions;

import android.annotation.SystemApi;

/**
 * IkeInternalException encapsulates all local implementation or resource related exceptions.
 *
 * <p>Causes may include exceptions such as {@link IpSecManager.SpiUnavailableException} when the
 * requested SPI resources failed to be allocated.
 *
 * @hide
 */
@SystemApi
public final class IkeInternalException extends IkeException {
    /**
     * Constructs a new exception with the specified cause.
     *
     * @param cause the cause (which is saved for later retrieval by the {@link #getCause()}
     *     method).
     * @hide
     */
    public IkeInternalException(Throwable cause) {
        super(cause);
    }

    /**
     * Constructs a new exception with the specified cause.
     *
     * @param message the descriptive message.
     * @param cause the cause (which is saved for later retrieval by the {@link #getCause()}
     *     method).
     * @hide
     */
    public IkeInternalException(String message, Throwable cause) {
        super(message, cause);
    }
}
