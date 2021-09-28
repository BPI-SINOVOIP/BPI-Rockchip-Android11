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

/** Error Identifiers from Trade Federation infra, and dependent infra (like Build infra). */
public enum InfraErrorIdentifier implements ErrorIdentifier {

    // ********************************************************************************************
    // Infra: 10_001 ~ 20_000
    // ********************************************************************************************
    // 10_001 - 10_500: General errors
    ARTIFACT_NOT_FOUND(10_001, FailureStatus.INFRA_FAILURE),
    FAIL_TO_CREATE_FILE(10_002, FailureStatus.INFRA_FAILURE),
    INVOCATION_CANCELLED(10_003, FailureStatus.CANCELLED),

    // 10_501 - 11_000: Build, Artifacts download related errors
    ARTIFACT_REMOTE_PATH_NULL(10_501, FailureStatus.INFRA_FAILURE),
    ARTIFACT_UNSUPPORTED_PATH(10_502, FailureStatus.INFRA_FAILURE),
    ARTIFACT_DOWNLOAD_ERROR(10_503, FailureStatus.INFRA_FAILURE),

    // 11_001 - 11_500: environment issues: For example: lab wifi
    WIFI_FAILED_CONNECT(11_001, FailureStatus.UNSET), // TODO: switch to dependency_issue

    // 12_000 - 12_100: Test issues detected by infra
    EXPECTED_TESTS_MISMATCH(12_000, FailureStatus.TEST_FAILURE),

    UNDETERMINED(20_000, FailureStatus.UNSET);

    private final long code;
    private final FailureStatus status;

    InfraErrorIdentifier(int code, FailureStatus status) {
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
