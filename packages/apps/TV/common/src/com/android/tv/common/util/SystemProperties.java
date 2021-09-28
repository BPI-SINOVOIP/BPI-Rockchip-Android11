/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.common.util;

import com.android.tv.common.BooleanSystemProperty;

/** A convenience class for getting TV related system properties. */
public final class SystemProperties {

    /** Allow third party inputs. */
    public static final BooleanSystemProperty ALLOW_THIRD_PARTY_INPUTS =
            new BooleanSystemProperty("ro.tv_allow_third_party_inputs", true);

    static {
        updateSystemProperties();
    }

    private SystemProperties() {}

    /** Update the TV related system properties. */
    public static void updateSystemProperties() {
        BooleanSystemProperty.resetAll();
    }
}
