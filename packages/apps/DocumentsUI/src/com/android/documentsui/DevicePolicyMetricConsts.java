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

package com.android.documentsui;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * All constants are based on the enums in
 * frameworks/base/core/proto/android/stats/devicepolicy/device_policy_enums.proto.
 */
public class DevicePolicyMetricConsts {

    public static final int ATOM_DEVICE_POLICY_EVENT = 103;

    @IntDef(flag = true, value = {
            ATOM_DEVICE_POLICY_EVENT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Atom {
    }

    // Codes representing DocsUI event id.
    public static final int EVENT_ID_DOCSUI_EMPTY_STATE_NO_PERMISSION = 173;
    public static final int EVENT_ID_DOCSUI_EMPTY_STATE_QUIET_MODE = 174;
    public static final int EVENT_ID_DOCSUI_LAUNCH_OTHER_APP = 175;
    public static final int EVENT_ID_DOCSUI_PICK_RESULT = 176;

    @IntDef(flag = true, value = {
            EVENT_ID_DOCSUI_EMPTY_STATE_NO_PERMISSION,
            EVENT_ID_DOCSUI_EMPTY_STATE_QUIET_MODE,
            EVENT_ID_DOCSUI_LAUNCH_OTHER_APP,
            EVENT_ID_DOCSUI_PICK_RESULT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EventId {
    }
}
