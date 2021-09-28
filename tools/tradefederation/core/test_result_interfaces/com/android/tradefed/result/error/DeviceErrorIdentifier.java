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
package com.android.tradefed.result.error;

import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;

/** Error Identifiers from Device errors and device reported errors. */
public enum DeviceErrorIdentifier implements ErrorIdentifier {

    // ********************************************************************************************
    // Device Errors: 30_001 ~ 40_000
    // ********************************************************************************************
    APK_INSTALLATION_FAILED(30_001, FailureStatus.UNSET),

    AAPT_PARSER_FAILED(30_050, FailureStatus.UNSET),

    SHELL_COMMAND_ERROR(30_100, FailureStatus.UNSET),
    DEVICE_UNEXPECTED_RESPONSE(30_101, FailureStatus.UNSET),

    INSTRUMENATION_CRASH(30_200, FailureStatus.UNSET),

    FAILED_TO_LAUNCH_GCE(30_500, FailureStatus.LOST_SYSTEM_UNDER_TEST),
    FAILED_TO_CONNECT_TO_GCE(30_501, FailureStatus.LOST_SYSTEM_UNDER_TEST),
    ERROR_AFTER_FLASHING(30_502, FailureStatus.LOST_SYSTEM_UNDER_TEST),

    DEVICE_UNAVAILABLE(30_750, FailureStatus.LOST_SYSTEM_UNDER_TEST),
    DEVICE_UNRESPONSIVE(30_751, FailureStatus.LOST_SYSTEM_UNDER_TEST);

    private final long code;
    private final FailureStatus status;

    DeviceErrorIdentifier(int code, FailureStatus status) {
        this.code = code;
        this.status = status;
    }

    @Override
    public long code() {
        return code;
    }

    @Override
    public FailureStatus status() {
        return status;
    }
}
