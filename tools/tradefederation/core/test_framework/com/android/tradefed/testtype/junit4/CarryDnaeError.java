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
package com.android.tradefed.testtype.junit4;

import com.android.tradefed.device.DeviceNotAvailableException;

/**
 * Internal {@link RuntimeException} to carry {@link DeviceNotAvailableException} through the JUnit4
 * framework.
 */
public class CarryDnaeError extends RuntimeException {
    static final long serialVersionUID = 4980196508277280342L;

    private DeviceNotAvailableException mException;

    public CarryDnaeError(DeviceNotAvailableException e) {
        super(e);
        mException = e;
    }

    /** Returns the {@link DeviceNotAvailableException} carried by this wrapper. */
    public DeviceNotAvailableException getDeviceNotAvailableException() {
        return mException;
    }
}
