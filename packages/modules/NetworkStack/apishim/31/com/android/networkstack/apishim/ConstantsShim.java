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

package com.android.networkstack.apishim;

import androidx.annotation.VisibleForTesting;

/**
 * Utility class for defining and importing constants from the Android platform.
 */
public class ConstantsShim extends com.android.networkstack.apishim.api30.ConstantsShim {
    /**
     * Constant that callers can use to determine what version of the shim they are using.
     * Must be the same as the version of the shims.
     * This should only be used by test code. Production code that uses the shims should be using
     * the shimmed objects and methods themselves.
     */
    @VisibleForTesting
    public static final int VERSION = 31;

    // When removing this shim, the version in NetworkMonitorUtils should be removed too.
    // TODO: add TRANSPORT_TEST to system API in API 31 (it is only a test API as of R)
    public static final int TRANSPORT_TEST = 7;
}
